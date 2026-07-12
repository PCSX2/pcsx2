// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Config.h"
#include "R3000A.h"
#include "R5900.h"
#include "Vif.h"
#include "VU.h"
#include "arm64/iR5900-arm64.h"
#include "arm64/iR3000A-arm64.h"

#include "common/Assertions.h"
#include "common/Console.h"

namespace a64 = vixl::aarch64;

//#define RALOG(...) fprintf(stderr, __VA_ARGS__)
#define RALOG(...)

////////////////////////////////////////////////////////////////////////////////
// IOP constant propagation externs
// These are defined in the IOP recompiler, but the register allocator needs
// them to handle PSX register allocation correctly.

extern u32 g_psxConstRegs[32];
extern u32 g_psxHasConstReg, g_psxFlushedConstReg;

#define PSX_IS_CONST1(reg) ((reg) < 32 && (g_psxHasConstReg & (1 << (reg))))
#define PSX_DEL_CONST(reg) \
	{ \
		if ((reg) < 32) \
			g_psxHasConstReg &= ~(1 << (reg)); \
	}

////////////////////////////////////////////////////////////////////////////////
// Shared state

EEINST* g_pCurInstInfo = nullptr;

u16 g_arm64AllocCounter = 0;
u16 g_neonAllocCounter = 0;

// EE constant propagation state
alignas(16) GPR_reg64 g_cpuConstRegs[32] = {};
u32 g_cpuHasConstReg = 0, g_cpuFlushedConstReg = 0;

////////////////////////////////////////////////////////////////////////////////
// ARM64 GPR Register Allocator

_arm64gprregs arm64gprs[NUM_ARM_GPR_REGS], s_saveArm64GPRregs[NUM_ARM_GPR_REGS];
static uint g_arm64checknext = 0;

_arm64neonregs arm64neon[NUM_ARM_NEON_REGS], s_saveArm64NEONregs[NUM_ARM_NEON_REGS];

// ARM64 register allocation policy (EE-SRA 3 Arm D tier-2 re-home):
// x0-x3:   argument/return registers (caller-saved, allocatable)
// x4-x7:   caller-saved temporaries (allocatable; vacated by the Arm D
//          tier-2 re-home — S3's 0f16948ae removed every hardcoded w4-w7
//          scratch use, so nothing conflicts with allocator residency)
// x8-x10:  scratch (RXSCRATCH + load/store addr/value) — NOT allocatable
// x11:     REEPIN_AT — NOT allocatable for EE (tier-2 pinned mirror of
//          GPR.r[1].UD[0]; caller-saved but preserve_most-spared — see the
//          preservation contract in iR5900-arm64.h). IOP-allocatable.
// x12/x13: REEPIN_K0/REEPIN_S0 — NOT allocatable for EE (tier-2 pinned
//          mirrors of GPR.r[26]/[16].UD[0]; caller-saved but
//          preserve_most-spared). IOP-allocatable.
// x14/x15: caller-saved temporaries (allocatable; shared with the mVU
//          macro-mode VI pool, which is compatible because macro ops emit
//          on a flushed allocator)
// x16:     VIXL intra-procedure scratch — NOT allocatable
// x17:     RSCRATCHADDR — NOT allocatable
// x18:     platform reserved — NOT allocatable
// x19:     RFASTMEMBASE — NOT allocatable (reserved for fastmem base)
// x20:     RSTATE — NOT allocatable (reserved for cpuRegs pointer)
// x21:     REEPIN_A1 — NOT allocatable (pinned mirror of GPR.r[5].UD[0],
//          $a1; callee-saved). Doubles as RPSXSTATE (psxRegs base) inside
//          the IOP dispatcher's armBeginStackFrame — see iR5900-arm64.h.
// x22:     REEPIN_SP — NOT allocatable (pinned mirror of GPR.r[29].UD[0], $sp)
// x23:     REEPIN_RA — NOT allocatable (pinned mirror of GPR.r[31].UD[0], $ra)
// x24:     RVU0 — NOT allocatable (reserved for &VU0 pointer in EE COP2 JIT)
// x25:     RECCYCLE — NOT allocatable (pinned cycle delta: cycle - nextEventCycle)
// x26/x27: REEPIN_V1/REEPIN_A0 — NOT allocatable for EE (pinned mirrors of
//          GPR.r[3]/GPR.r[4].UD[0], $v1/$a0; callee-saved). IOP-allocatable
//          (see IOP_ALLOCATABLE_MASK below).
// x28:     callee-saved (allocatable) — the ONLY callee-saved pool member
//          for EE, so total EE MODE_CALLEESAVED demand must stay ≤1 and the
//          demanders (vtlb unaligned handlers) must issue that alloc before
//          any same-instruction alloc can hold x28 `needed`.
// x29:     REEPIN_V0 — NOT allocatable (pinned mirror of GPR.r[2].UD[0], $v0;
//          doubles as the AAPCS frame pointer outside JIT execution)
// x30:     link register — NOT allocatable

// Bitmask of allocatable aarch64 GPRs for EE-side codegen (EE/VU-macro/
// temps). Bit `n` set ↔ x_n is in the pool. Cleared bits as documented
// above:
//   bit 8       — x8  : RXSCRATCH/RWSCRATCH (value scratch)
//   bits 9-10   — x9/x10 : load/store address + value scratch
//   bits 11-13  — x11/x12/x13 : REEPIN_AT/REEPIN_K0/REEPIN_S0 (tier-2
//                 pinned mirrors)
//   bits 16-18  — x16 (vixl), x17 (RSCRATCHADDR), x18 (platform reserved)
//   bit 19      — x19 : RFASTMEMBASE
//   bit 20      — x20 : RSTATE (cpuRegs base pointer)
//   bit 21      — x21 : REEPIN_A1 (pinned $a1 mirror / IOP RPSXSTATE)
//   bits 22-23  — x22/x23 : REEPIN_SP/REEPIN_RA (pinned $sp/$ra mirrors)
//   bit 24      — x24 : RVU0 (pinned &VU0 for iCOP2)
//   bit 25      — x25 : RECCYCLE (pinned cycle delta)
//   bits 26-27  — x26/x27 : REEPIN_V1/REEPIN_A0 (pinned $v1/$a0 mirrors)
//   bits 29-30  — x29 (REEPIN_V0, pinned $v0 mirror / FP), x30 (LR) — never
//                 allocatable
// Inner allocator loop runs 31× per cache miss and was nine sequential
// `if (armreg == N) return false` branches per probe; collapse to one
// LSR + AND + cbz against this mask.
static constexpr uint32_t EE_ALLOCATABLE_MASK = ~((1u << 8)
	| (1u << 9) | (1u << 10)
	| (7u << 11)
	| (7u << 16)
	| (1u << 19) | (1u << 20) | (1u << 21)
	| (3u << 22)
	| (1u << 24) | (1u << 25)
	| (3u << 26)
	| (3u << 29));

// IOP-side pool (ARM64TYPE_PSX / ARM64TYPE_PSX_PCWRITEBACK allocations):
// re-admits the EE pin homes x11-x13 and x26/x27. IOP blocks execute under
// EnterRecompiledCode's armBeginStackFrame (x19-x28 saved) so the
// callee-saved pins are restored before EE JIT code resumes; the
// caller-saved x11-x13 are legal because IOP execution is reachable from a
// live EE session only through C seams, and every EE C seam that can run
// IOP reloads its caller-saved pins afterwards (the preserve_most
// emit-nothing seams cannot run IOP: the vtlb dispatchers' preserve_most
// contract restores x9-x15 regardless of what they call internally).
// Shared TEMP allocations always use the EE mask (restrictive = safe for
// both CPUs).
static constexpr uint32_t IOP_ALLOCATABLE_MASK =
	EE_ALLOCATABLE_MASK | (7u << 11) | (1u << 26) | (1u << 27);

bool _isAllocatableArm64GPR(int armreg)
{
	// EE-mask semantics: callers outside the allocator use this as "may EE
	// codegen ever see a dynamic value here"; the IOP-only extras are
	// handled inside _getFreeArm64GPR via the pool parameter.
	return ((EE_ALLOCATABLE_MASK >> armreg) & 1u) != 0u;
}

void _initArm64GPRregs()
{
	std::memset(arm64gprs, 0, sizeof(arm64gprs));
	g_arm64AllocCounter = 0;
	g_arm64checknext = 0;
}

bool _hasArm64GPR(int type, int reg, int required_mode)
{
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (arm64gprs[i].inuse && arm64gprs[i].type == type && arm64gprs[i].reg == reg)
			return ((arm64gprs[i].mode & required_mode) == required_mode);
	}
	return false;
}

int _getFreeArm64GPR(int mode, u32 pool)
{
	int tempi = -1;
	u32 bestcount = 0x10000;

	// First pass: find a completely free register
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		const int reg = (g_arm64checknext + i) % NUM_ARM_GPR_REGS;
		if (arm64gprs[reg].inuse || !((pool >> reg) & 1u))
			continue;

		if ((mode & MODE_CALLEESAVED) && !armIsCalleeSavedRegister(reg))
			continue;

		if ((mode & MODE_COP2) && mVUIsReservedCOP2(reg))
			continue;

		g_arm64checknext = (reg + 1) % NUM_ARM_GPR_REGS;
		return reg;
	}

	// Second pass: evict by LRU, prefer temps first
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (!((pool >> i) & 1u))
			continue;
		if ((mode & MODE_CALLEESAVED) && !armIsCalleeSavedRegister(i))
			continue;
		if ((mode & MODE_COP2) && mVUIsReservedCOP2(i))
			continue;

		pxAssert(arm64gprs[i].inuse);
		if (arm64gprs[i].needed)
			continue;

		if (arm64gprs[i].type == ARM64TYPE_TEMP)
		{
			_freeArm64GPR(i);
			return i;
		}

		if (arm64gprs[i].counter < bestcount)
		{
			tempi = i;
			bestcount = arm64gprs[i].counter;
		}
	}

	if (tempi != -1)
	{
		_freeArm64GPR(tempi);
		return tempi;
	}

	pxFailRel("ARM64 GPR register allocation error");
	return -1;
}

void _writebackArm64GPR(int armreg)
{
	switch (arm64gprs[armreg].type)
	{
		case ARM64TYPE_GPR:
			RALOG("Writing back ARM64 GPR %d for guest reg %d\n", armreg, arm64gprs[armreg].reg);
			armStoreEERegPtr(armXRegister(armreg), &cpuRegs.GPR.r[arm64gprs[armreg].reg].UD[0]);
			break;

		case ARM64TYPE_FPRC:
			RALOG("Writing back ARM64 GPR %d for guest FPCR %d\n", armreg, arm64gprs[armreg].reg);
			armStoreEERegPtr(armWRegister(armreg), &fpuRegs.fprc[arm64gprs[armreg].reg]);
			break;

		case ARM64TYPE_VIREG:
			RALOG("Writing back ARM64 GPR %d for guest VI %d\n", armreg, arm64gprs[armreg].reg);
			armAsm->Strh(armWRegister(armreg), armVU0Mem(&VU0.VI[arm64gprs[armreg].reg].UL));
			break;

		case ARM64TYPE_PCWRITEBACK:
			RALOG("Writing back PC writeback from ARM64 GPR %d\n", armreg);
			armAsm->Str(armWRegister(armreg), armCpuRegMem(&cpuRegs.pcWriteback));
			break;

		case ARM64TYPE_PSX:
			RALOG("Writing back ARM64 GPR %d for guest PSX reg %d\n", armreg, arm64gprs[armreg].reg);
			armAsm->Str(armWRegister(armreg), armPsxRegMem(&psxRegs.GPR.r[arm64gprs[armreg].reg]));
			break;

		case ARM64TYPE_PSX_PCWRITEBACK:
			RALOG("Writing back PSX PC writeback from ARM64 GPR %d\n", armreg);
			armAsm->Str(armWRegister(armreg), armPsxRegMem(&psxRegs.pcWriteback));
			break;

		default:
			break;
	}
}

void _freeArm64GPR(int armreg)
{
	pxAssert(armreg >= 0 && armreg < NUM_ARM_GPR_REGS);
	if (!arm64gprs[armreg].inuse)
		return;

	if (arm64gprs[armreg].mode & MODE_WRITE)
		_writebackArm64GPR(armreg);

	arm64gprs[armreg].inuse = 0;
	arm64gprs[armreg].mode = 0;
}

void _freeArm64GPRWithoutWriteback(int armreg)
{
	pxAssert(armreg >= 0 && armreg < NUM_ARM_GPR_REGS);
	arm64gprs[armreg].inuse = 0;
	arm64gprs[armreg].mode = 0;
}

void _freeArm64GPRregs()
{
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (arm64gprs[i].inuse)
			_freeArm64GPR(i);
	}
}

void _flushArm64GPRregs()
{
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (arm64gprs[i].inuse && (arm64gprs[i].mode & MODE_WRITE))
		{
			_writebackArm64GPR(i);
			arm64gprs[i].mode &= ~MODE_WRITE;
			arm64gprs[i].mode |= MODE_READ;
		}
	}
}

int _checkArm64GPR(int type, int reg, int mode)
{
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (arm64gprs[i].inuse && arm64gprs[i].type == type && arm64gprs[i].reg == reg)
		{
			arm64gprs[i].mode |= mode;
			arm64gprs[i].counter = g_arm64AllocCounter++;
			arm64gprs[i].needed = 1;
			return i;
		}
	}
	return -1;
}

int _allocArm64GPR(int type, int reg, int mode)
{
	if (type == ARM64TYPE_GPR || type == ARM64TYPE_PSX)
		pxAssertMsg(reg >= 0 && reg < 34, "Register index out of bounds.");

	int hostNEONreg = (type == ARM64TYPE_GPR) ? _checkNEONreg(NEONTYPE_GPRREG, reg, 0) : -1;

	// Check if already allocated
	if (type != ARM64TYPE_TEMP)
	{
		for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
		{
			if (!arm64gprs[i].inuse || arm64gprs[i].type != type || arm64gprs[i].reg != reg)
				continue;

			if (type == ARM64TYPE_VIREG && reg < 0)
				continue;

			if (type == ARM64TYPE_GPR && (mode & MODE_WRITE))
			{
				if (GPR_IS_CONST1(reg))
					GPR_DEL_CONST(reg);
				if (hostNEONreg >= 0)
				{
					pxAssert(!(arm64neon[hostNEONreg].mode & MODE_WRITE));
					_freeNEONreg(hostNEONreg);
				}
			}
			else if (type == ARM64TYPE_PSX && (mode & MODE_WRITE))
			{
				if (PSX_IS_CONST1(reg))
					PSX_DEL_CONST(reg);
			}

			arm64gprs[i].counter = g_arm64AllocCounter++;
			arm64gprs[i].mode |= mode & ~MODE_CALLEESAVED;
			arm64gprs[i].needed = true;
			return i;
		}
	}

	// Need to allocate a new register. PSX-typed values may use the wider
	// IOP pool (x26/x27 ride under armBeginStackFrame); everything else —
	// EE guest state, VI mirrors, and shared TEMPs — stays inside the EE
	// mask so it can never land on an EE pin host.
	const u32 pool = (type == ARM64TYPE_PSX || type == ARM64TYPE_PSX_PCWRITEBACK)
	                     ? IOP_ALLOCATABLE_MASK
	                     : EE_ALLOCATABLE_MASK;
	const int regnum = _getFreeArm64GPR(mode, pool);
	arm64gprs[regnum].type = type;
	arm64gprs[regnum].reg = reg;
	arm64gprs[regnum].mode = mode & ~MODE_CALLEESAVED;
	arm64gprs[regnum].counter = g_arm64AllocCounter++;
	arm64gprs[regnum].needed = true;
	arm64gprs[regnum].inuse = true;

	if (mode & MODE_READ)
	{
		switch (type)
		{
			case ARM64TYPE_GPR:
			{
				if (reg == 0)
				{
					// r0 is always zero
					armAsm->Mov(armWRegister(regnum), 0);
				}
				else if (hostNEONreg >= 0)
				{
					// Value is in a NEON register, extract lower 64 bits
					RALOG("Copying guest reg %d from NEON %d to GPR %d\n", reg, hostNEONreg, regnum);
					armAsm->Mov(armXRegister(regnum), armQRegister(hostNEONreg).V2D(), 0);

					if (arm64neon[hostNEONreg].mode & MODE_WRITE)
					{
						_freeNEONreg(hostNEONreg);
					}
				}
				else if (GPR_IS_CONST1(reg))
				{
					RALOG("Loading constant %lld for guest reg %d to GPR %d\n",
						(long long)g_cpuConstRegs[reg].SD[0], reg, regnum);
					armAsm->Mov(armXRegister(regnum), g_cpuConstRegs[reg].SD[0]);
					g_cpuFlushedConstReg |= (1u << reg);
					arm64gprs[regnum].mode |= MODE_WRITE;
				}
				else
				{
					RALOG("Loading guest reg %d to GPR %d\n", reg, regnum);
					armLoadEERegPtr(armXRegister(regnum), &cpuRegs.GPR.r[reg].UD[0]);
				}
			}
			break;

			case ARM64TYPE_FPRC:
				RALOG("Loading guest FPCR %d to GPR %d\n", reg, regnum);
				armLoadEERegPtr(armWRegister(regnum), &fpuRegs.fprc[reg]);
				break;

			case ARM64TYPE_PSX:
			{
				if (reg == 0)
				{
					armAsm->Mov(armWRegister(regnum), 0);
				}
				else if (PSX_IS_CONST1(reg))
				{
					armAsm->Mov(armWRegister(regnum), g_psxConstRegs[reg]);
					g_psxFlushedConstReg |= (1u << reg);
					arm64gprs[regnum].mode |= MODE_WRITE;
				}
				else
				{
					armLoadPsxRegPtr(armWRegister(regnum), &psxRegs.GPR.r[reg]);
				}
			}
			break;

			case ARM64TYPE_VIREG:
			{
				RALOG("Loading guest VI reg %d to GPR %d\n", reg, regnum);
				armAsm->Ldrh(armWRegister(regnum), armVU0Mem(&VU0.VI[reg].US[0]));
			}
			break;

			default:
				break;
		}
	}

	if (type == ARM64TYPE_GPR && (mode & MODE_WRITE))
	{
		if (reg < 32 && GPR_IS_CONST1(reg))
			GPR_DEL_CONST(reg);
		if (hostNEONreg >= 0)
		{
			// We're about to write this guest reg into the scalar GPR, so the
			// cached NEON copy is superseded — discard it WITHOUT writeback
			// (mirrors _allocGPRtoNEONreg and x86 _allocGPRtoXMMreg). Writing
			// it back would store a stale value the new GPR's flush overwrites.
			_freeNEONregWithoutWriteback(hostNEONreg);
		}
	}
	else if (type == ARM64TYPE_PSX && (mode & MODE_WRITE))
	{
		if (reg < 32 && PSX_IS_CONST1(reg))
			PSX_DEL_CONST(reg);
	}

	return regnum;
}

void _addNeededArm64GPR(int type, int reg)
{
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (arm64gprs[i].inuse && arm64gprs[i].type == type && arm64gprs[i].reg == reg)
			arm64gprs[i].needed = 1;
	}
}

void _clearNeededArm64GPRregs()
{
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (arm64gprs[i].needed && arm64gprs[i].type == ARM64TYPE_TEMP)
			_freeArm64GPR(i);
		arm64gprs[i].needed = 0;
	}
}

void _flushConstReg(int reg)
{
	if (GPR_IS_CONST1(reg) && !(g_cpuFlushedConstReg & (1 << reg)))
	{
		// Materialize the constant into its destination directly. When reg is
		// pinned, that destination IS the pin mirror: armStoreEERegPtr then
		// recognizes the pin as its own store source and emits only the
		// canonical STR (write-through) or nothing (lazy-dirty), collapsing
		// the old Mov-scratch / STR / Mov-pin triad to Mov-pin / STR. Unpinned
		// regs keep routing through RXSCRATCH.
		const vixl::aarch64::Register* pin = armEEPinForGPR(reg);
		const vixl::aarch64::Register& dst = pin ? *pin : RXSCRATCH;
		armAsm->Mov(dst, static_cast<s64>(g_cpuConstRegs[reg].SD[0]));
		armStoreEERegPtr(dst, &cpuRegs.GPR.r[reg].UD[0]);
		g_cpuFlushedConstReg |= (1 << reg);
		if (reg == 0)
			DevCon.Warning("Flushing r0!");
	}
}

void _flushConstRegs(bool delete_const)
{
	for (u32 i = 0; i < 32; i++)
	{
		if (!GPR_IS_CONST1(i) || g_cpuFlushedConstReg & (1u << i))
			continue;

		// Const-into-pin (see _flushConstReg): materialize straight into the
		// pin mirror when i is pinned, else through RXSCRATCH.
		const vixl::aarch64::Register* pin = armEEPinForGPR(static_cast<int>(i));
		const vixl::aarch64::Register& dst = pin ? *pin : RXSCRATCH;
		armAsm->Mov(dst, static_cast<u64>(g_cpuConstRegs[i].UD[0]));
		armStoreEERegPtr(dst, &cpuRegs.GPR.r[i].UD[0]);
		g_cpuFlushedConstReg |= 1u << i;
	}

	if (delete_const)
	{
		// Clear ALL const state, including already-flushed registers.
		// After an interpreter call, the interpreter may have modified any
		// register — stale const flags would cause subsequent native code
		// to use outdated values from g_cpuConstRegs instead of memory.
		g_cpuHasConstReg = 1; // keep r0 (always zero)
		g_cpuFlushedConstReg = 1;
	}
}

void _validateRegs()
{
#ifdef PCSX2_DEVBUILD
	for (s8 guestreg = 0; guestreg < 32; guestreg++)
	{
		u32 gprreg = 0, gprmode = 0;
		u32 neonreg = 0, neonmode = 0;
		for (int hostreg = 0; hostreg < NUM_ARM_GPR_REGS; hostreg++)
		{
			if (arm64gprs[hostreg].inuse && arm64gprs[hostreg].type == ARM64TYPE_GPR && arm64gprs[hostreg].reg == guestreg)
			{
				pxAssertMsg(gprreg == 0 && gprmode == 0, "register not already allocated in GPR");
				gprreg = hostreg;
				gprmode = arm64gprs[hostreg].mode;
			}
		}
		for (int hostreg = 0; hostreg < NUM_ARM_NEON_REGS; hostreg++)
		{
			if (arm64neon[hostreg].inuse && arm64neon[hostreg].type == NEONTYPE_GPRREG && arm64neon[hostreg].reg == guestreg)
			{
				pxAssertMsg(neonreg == 0 && neonmode == 0, "register not already allocated in NEON");
				neonreg = hostreg;
				neonmode = arm64neon[hostreg].mode;
			}
		}

		if ((gprmode | neonmode) & MODE_WRITE)
			pxAssertMsg((gprmode & MODE_WRITE) != (neonmode & MODE_WRITE), "only one of GPR/NEON is in write state");
	}
#endif
}

// Type-specific convenience wrappers over _addNeededArm64GPR.
void _addNeededGPRtoArm64GPR(int gprreg) { _addNeededArm64GPR(ARM64TYPE_GPR, gprreg); }
void _addNeededPSXtoArm64GPR(int gprreg) { _addNeededArm64GPR(ARM64TYPE_PSX, gprreg); }

void _deleteGPRtoArm64GPR(int reg, int flush)
{
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (arm64gprs[i].inuse && arm64gprs[i].type == ARM64TYPE_GPR && arm64gprs[i].reg == reg)
		{
			switch (flush)
			{
				case DELETE_REG_FREE: _freeArm64GPR(i); break;
				case DELETE_REG_FLUSH:
					if (arm64gprs[i].mode & MODE_WRITE)
					{
						_writebackArm64GPR(i);
						// Drop MODE_WRITE (keep MODE_READ) so a later
						// _freeArm64GPR won't store the same value again.
						arm64gprs[i].mode = (arm64gprs[i].mode & ~MODE_WRITE) | MODE_READ;
					}
					break;
				case DELETE_REG_FLUSH_AND_FREE: _freeArm64GPR(i); break;
				case DELETE_REG_FREE_NO_WRITEBACK: _freeArm64GPRWithoutWriteback(i); break;
			}
			return;
		}
	}
}

void _deletePSXtoArm64GPR(int reg, int flush)
{
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (arm64gprs[i].inuse && arm64gprs[i].type == ARM64TYPE_PSX && arm64gprs[i].reg == reg)
		{
			switch (flush)
			{
				case DELETE_REG_FREE: _freeArm64GPR(i); break;
				case DELETE_REG_FLUSH:
					if (arm64gprs[i].mode & MODE_WRITE)
					{
						_writebackArm64GPR(i);
						// Drop MODE_WRITE (keep MODE_READ) so a later
						// _freeArm64GPR won't store the same value again.
						arm64gprs[i].mode = (arm64gprs[i].mode & ~MODE_WRITE) | MODE_READ;
					}
					break;
				case DELETE_REG_FLUSH_AND_FREE: _freeArm64GPR(i); break;
				case DELETE_REG_FREE_NO_WRITEBACK: _freeArm64GPRWithoutWriteback(i); break;
			}
			return;
		}
	}
}

int _allocIfUsedGPRtoArm64(int gprreg, int mode)
{
	return EEINST_USEDTEST(gprreg) ? _allocArm64GPR(ARM64TYPE_GPR, gprreg, mode) : -1;
}

int _allocIfUsedVItoArm64(int vireg, int mode)
{
	return EEINST_VIUSEDTEST(vireg) ? _allocArm64GPR(ARM64TYPE_VIREG, vireg, mode) : -1;
}

////////////////////////////////////////////////////////////////////////////////
// ARM64 NEON Register Allocator

void _initArm64NEONregs()
{
	std::memset(arm64neon, 0, sizeof(arm64neon));
	g_neonAllocCounter = 0;
}

// Reserved NEON scalars for PS2 FPU clamp constants (held across the JIT
// session). s8 = +FLT_MAX, s9 = -FLT_MAX. Loaded in the EE dispatcher and
// mVU dispatcher prologues; used by fpuClampResult and iCOP2 scalar
// VDIV/VSQRT/VRSQRT. Lower 64 bits are callee-saved per AAPCS64, so the
// values survive every armEmitCall path without compile-time tracking.
// v8/v9 are skipped by every _getFreeArm64NEON search loop below — no
// allocator codepath can land on them.
static constexpr u32 NEON_RESERVED_FPU_MAX = 8;
static constexpr u32 NEON_RESERVED_FPU_MIN = 9;

// Callee-saved NEON range available to the allocator: q10-q15
// (indices 8/9 reserved above). EE GPR values allocated here survive FPU
// interpreter calls without flushing.
static constexpr u32 NEON_CALLEE_SAVED_START = 10;
static constexpr u32 NEON_CALLEE_SAVED_END = 16; // exclusive

int _getFreeArm64NEON(u32 minreg, u32 maxreg)
{
	int tempi = -1;
	u32 bestcount = 0x10000;

	// Check for free registers
	for (u32 i = minreg; i < maxreg; i++)
	{
		if (i == NEON_RESERVED_FPU_MAX || i == NEON_RESERVED_FPU_MIN)
			continue;
		if (!arm64neon[i].inuse)
			return i;
	}

	// Check for dead regs
	tempi = -1;
	bestcount = 0xffff;
	for (u32 i = minreg; i < maxreg; i++)
	{
		if (i == NEON_RESERVED_FPU_MAX || i == NEON_RESERVED_FPU_MIN)
			continue;
		pxAssert(arm64neon[i].inuse);
		if (arm64neon[i].needed)
			continue;

		pxAssert(arm64neon[i].type != NEONTYPE_TEMP);

		if (arm64neon[i].counter < bestcount)
		{
			switch (arm64neon[i].type)
			{
				case NEONTYPE_GPRREG:
					if (EEINST_USEDTEST(arm64neon[i].reg))
						continue;
					break;
				case NEONTYPE_FPREG:
					if (FPUINST_USEDTEST(arm64neon[i].reg))
						continue;
					break;
				case NEONTYPE_VFREG:
					if (EEINST_VFUSEDTEST(arm64neon[i].reg))
						continue;
					break;
			}

			tempi = i;
			bestcount = arm64neon[i].counter;
		}
	}

	if (tempi != -1)
	{
		_freeNEONreg(tempi);
		return tempi;
	}

	// Last resort: take the LRU register
	bestcount = 0xffff;
	for (u32 i = minreg; i < maxreg; i++)
	{
		if (i == NEON_RESERVED_FPU_MAX || i == NEON_RESERVED_FPU_MIN)
			continue;
		pxAssert(arm64neon[i].inuse);
		if (arm64neon[i].needed)
			continue;

		if (arm64neon[i].counter < bestcount)
		{
			tempi = i;
			bestcount = arm64neon[i].counter;
		}
	}

	if (tempi != -1)
	{
		_freeNEONreg(tempi);
		return tempi;
	}

	pxFailRel("ARM64 NEON register allocation error");
	return -1;
}

// Overload for backward compatibility (full range)
int _getFreeArm64NEON(u32 maxreg)
{
	return _getFreeArm64NEON(0, maxreg);
}

int _allocTempNEONreg()
{
	const int neonreg = _getFreeArm64NEON();
	arm64neon[neonreg].inuse = 1;
	arm64neon[neonreg].type = NEONTYPE_TEMP;
	arm64neon[neonreg].needed = 1;
	arm64neon[neonreg].counter = g_neonAllocCounter++;
	return neonreg;
}

int _checkNEONreg(int type, int reg, int mode)
{
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse && (arm64neon[i].type == (type & 0xff)) && (arm64neon[i].reg == reg))
		{
			if (type == NEONTYPE_GPRREG && (mode & MODE_WRITE))
				return _allocGPRtoNEONreg(reg, mode);

			arm64neon[i].mode |= mode;
			arm64neon[i].counter = g_neonAllocCounter++;
			arm64neon[i].needed = 1;
			return i;
		}
	}
	return -1;
}

bool _hasNEONreg(int type, int reg, int required_mode)
{
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse && arm64neon[i].type == type && arm64neon[i].reg == reg)
			return ((arm64neon[i].mode & required_mode) == required_mode);
	}
	return false;
}

int _allocFPtoNEONreg(int fpreg, int mode)
{
	// Check if already allocated
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (!arm64neon[i].inuse || arm64neon[i].type != NEONTYPE_FPREG || arm64neon[i].reg != fpreg)
			continue;

		// Slot already holds the live value (MODE_READ → loaded from memory,
		// MODE_WRITE → freshly written; both are authoritative over memory).
		// Reloading here would clobber a MODE_WRITE-only live value with
		// stale memory, breaking chained ops where the next read consumes
		// the previous write. Mirrors _allocGPRtoNEONreg's reuse path.
		arm64neon[i].counter = g_neonAllocCounter++;
		arm64neon[i].needed = 1;
		arm64neon[i].mode |= mode;
		return i;
	}

	// New allocation
	const int neonreg = _getFreeArm64NEON();
	arm64neon[neonreg].inuse = 1;
	arm64neon[neonreg].type = NEONTYPE_FPREG;
	arm64neon[neonreg].reg = fpreg;
	arm64neon[neonreg].mode = mode;
	arm64neon[neonreg].needed = 1;
	arm64neon[neonreg].counter = g_neonAllocCounter++;

	if (mode & MODE_READ)
	{
		armLoadEERegPtr(armSRegister(neonreg), &fpuRegs.fpr[fpreg].f);
	}

	return neonreg;
}

int _allocGPRtoNEONreg(int gprreg, int mode)
{
	const int hostGPRreg = _checkArm64GPR(ARM64TYPE_GPR, gprreg, MODE_READ);

	// Check if already in NEON
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (!arm64neon[i].inuse || arm64neon[i].type != NEONTYPE_GPRREG || arm64neon[i].reg != gprreg)
			continue;

		if (mode & MODE_WRITE && hostGPRreg >= 0)
		{
			// Dual-dirty (NEON MODE_WRITE + arm64gpr MODE_WRITE for the same
			// guest reg) means a scalar op left a pending lower-64 write.
			// Flush it before freeing so the value isn't lost. This case is
			// legitimate, not an error: eeRecompileCodeXMM can reuse a
			// MMI-written slot for a subsequent MMI Rd while the scalar GPR
			// allocator still holds an unrelated MODE_WRITE entry for the
			// same guest reg.
			if (arm64gprs[hostGPRreg].mode & MODE_WRITE)
				_writebackArm64GPR(hostGPRreg);
			_freeArm64GPRWithoutWriteback(hostGPRreg);
		}

		if (mode & MODE_WRITE && GPR_IS_CONST1(gprreg))
			GPR_DEL_CONST(gprreg);

		arm64neon[i].counter = g_neonAllocCounter++;
		arm64neon[i].needed = true;
		arm64neon[i].mode |= mode;
		return i;
	}

	// Allocate EE GPRs to callee-saved NEON range so they survive C
	// function calls (FPU interpreter, etc.) without flushing.
	const int neonreg = _getFreeArm64NEON(NEON_CALLEE_SAVED_START, NEON_CALLEE_SAVED_END);
	arm64neon[neonreg].inuse = 1;
	arm64neon[neonreg].type = NEONTYPE_GPRREG;
	arm64neon[neonreg].reg = gprreg;
	arm64neon[neonreg].mode = mode;
	arm64neon[neonreg].needed = 1;
	arm64neon[neonreg].counter = g_neonAllocCounter++;

	if (mode & MODE_READ)
	{
		if (gprreg == 0)
		{
			armAsm->Movi(armQRegister(neonreg).V2D(), 0);
		}
		else if (GPR_IS_CONST1(gprreg))
		{
			// Load full 128 bits from memory, replace lower 64 with constant
			armLoadEERegPtr(armQRegister(neonreg), &cpuRegs.GPR.r[gprreg].UQ);
			armAsm->Mov(RXSCRATCH, static_cast<s64>(g_cpuConstRegs[gprreg].SD[0]));
			armAsm->Ins(armQRegister(neonreg).V2D(), 0, RXSCRATCH);
			arm64neon[neonreg].mode |= MODE_WRITE;
			g_cpuFlushedConstReg |= (1u << gprreg);

			if (hostGPRreg >= 0)
				_freeArm64GPRWithoutWriteback(hostGPRreg);
		}
		else if (hostGPRreg >= 0)
		{
			// Load full 128, replace lower if dirty
			armLoadEERegPtr(armQRegister(neonreg), &cpuRegs.GPR.r[gprreg].UQ);
			if (arm64gprs[hostGPRreg].mode & MODE_WRITE)
			{
				armAsm->Ins(armQRegister(neonreg).V2D(), 0, armXRegister(hostGPRreg));
				_freeArm64GPRWithoutWriteback(hostGPRreg);
				arm64neon[neonreg].mode |= MODE_WRITE;
			}
		}
		else
		{
			armLoadEERegPtr(armQRegister(neonreg), &cpuRegs.GPR.r[gprreg].UQ);
			// Lazy-dirty: a dirty pin makes the memory lower half stale.
			armMergeEEPinIntoQuad(armQRegister(neonreg), gprreg);
		}
	}

	if (mode & MODE_WRITE && gprreg < 32 && GPR_IS_CONST1(gprreg))
		GPR_DEL_CONST(gprreg);
	if (mode & MODE_WRITE && hostGPRreg >= 0)
		_freeArm64GPRWithoutWriteback(hostGPRreg);

	return neonreg;
}

int _allocFPACCtoNEONreg(int mode)
{
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (!arm64neon[i].inuse || arm64neon[i].type != NEONTYPE_FPACC)
			continue;

		// Same invariant as _allocFPtoNEONreg: the slot already holds the
		// authoritative value (loaded or freshly written). Reloading would
		// clobber a MODE_WRITE-only ACC with stale memory, so a later read of
		// ACC must consume the value emitted earlier in the same block rather
		// than the pre-block memory image.
		arm64neon[i].counter = g_neonAllocCounter++;
		arm64neon[i].needed = 1;
		arm64neon[i].mode |= mode;
		return i;
	}

	const int neonreg = _getFreeArm64NEON();
	arm64neon[neonreg].inuse = 1;
	arm64neon[neonreg].type = NEONTYPE_FPACC;
	arm64neon[neonreg].reg = 0;
	arm64neon[neonreg].mode = mode;
	arm64neon[neonreg].needed = 1;
	arm64neon[neonreg].counter = g_neonAllocCounter++;

	if (mode & MODE_READ)
	{
		armLoadEERegPtr(armSRegister(neonreg), &fpuRegs.ACC.f);
	}

	return neonreg;
}

int _allocVFtoNEONreg(int vfreg, int mode)
{
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (!arm64neon[i].inuse || arm64neon[i].type != NEONTYPE_VFREG || arm64neon[i].reg != vfreg)
			continue;

		if (!(arm64neon[i].mode & MODE_READ) && (mode & MODE_READ))
		{
			armLoadPtr(armQRegister(i), &VU0.VF[vfreg]);
			arm64neon[i].mode |= MODE_READ;
		}

		arm64neon[i].counter = g_neonAllocCounter++;
		arm64neon[i].needed = 1;
		arm64neon[i].mode |= mode;
		return i;
	}

	const int neonreg = _getFreeArm64NEON();
	arm64neon[neonreg].inuse = 1;
	arm64neon[neonreg].type = NEONTYPE_VFREG;
	arm64neon[neonreg].reg = vfreg;
	arm64neon[neonreg].mode = mode;
	arm64neon[neonreg].needed = 1;
	arm64neon[neonreg].counter = g_neonAllocCounter++;

	if (mode & MODE_READ)
		armLoadPtr(armQRegister(neonreg), &VU0.VF[vfreg]);

	return neonreg;
}

void _writebackNEONreg(int neonreg)
{
	switch (arm64neon[neonreg].type)
	{
		case NEONTYPE_GPRREG:
		{
			// EE GPRs are 128-bit. Store the full Q register so MMI ops (which
			// write all 128 bits via eeRecompileCodeXMM) preserve their upper
			// 64-bit lanes through the writeback. _allocGPRtoNEONreg always
			// loads 128 bits on MODE_READ, so writeback symmetry is required.
			const int reg = arm64neon[neonreg].reg;
			if (reg == NEONGPR_LO)
				armStorePtr(armQRegister(neonreg), &cpuRegs.LO.UQ);
			else if (reg == NEONGPR_HI)
				armStorePtr(armQRegister(neonreg), &cpuRegs.HI.UQ);
			else
				armStoreEEGPRQuad(armQRegister(neonreg), reg);
		}
		break;

		case NEONTYPE_FPREG:
		{
			armStoreEERegPtr(armSRegister(neonreg), &fpuRegs.fpr[arm64neon[neonreg].reg].f);
		}
		break;

		case NEONTYPE_FPACC:
		{
			armStoreEERegPtr(armSRegister(neonreg), &fpuRegs.ACC.f);
		}
		break;

		case NEONTYPE_VFREG:
			armStorePtr(armQRegister(neonreg), &VU0.VF[arm64neon[neonreg].reg]);
			break;

		default:
			break;
	}
}

void _freeNEONreg(int neonreg)
{
	pxAssert(neonreg >= 0 && neonreg < NUM_ARM_NEON_REGS);
	if (!arm64neon[neonreg].inuse)
		return;

	if (arm64neon[neonreg].mode & MODE_WRITE)
		_writebackNEONreg(neonreg);

	arm64neon[neonreg].inuse = 0;
	arm64neon[neonreg].mode = 0;
}

void _freeNEONregWithoutWriteback(int neonreg)
{
	pxAssert(neonreg >= 0 && neonreg < NUM_ARM_NEON_REGS);
	arm64neon[neonreg].inuse = 0;
	arm64neon[neonreg].mode = 0;
}

void _freeNEONregs()
{
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse)
			_freeNEONreg(i);
	}
}

void _flushNEONreg(int neonreg)
{
	if (arm64neon[neonreg].inuse && (arm64neon[neonreg].mode & MODE_WRITE))
	{
		_writebackNEONreg(neonreg);
		arm64neon[neonreg].mode &= ~MODE_WRITE;
		arm64neon[neonreg].mode |= MODE_READ;
	}
}

void _flushNEONregs()
{
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
		_flushNEONreg(i);
}

void _addNeededFPtoNEONreg(int fpreg)
{
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse && arm64neon[i].type == NEONTYPE_FPREG && arm64neon[i].reg == fpreg)
			arm64neon[i].needed = 1;
	}
}

void _addNeededFPACCtoNEONreg()
{
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse && arm64neon[i].type == NEONTYPE_FPACC)
			arm64neon[i].needed = 1;
	}
}

void _addNeededGPRtoNEONreg(int gprreg)
{
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse && arm64neon[i].type == NEONTYPE_GPRREG && arm64neon[i].reg == gprreg)
			arm64neon[i].needed = 1;
	}
}

void _clearNeededNEONregs()
{
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].needed && arm64neon[i].type == NEONTYPE_TEMP)
			_freeNEONreg(i);
		arm64neon[i].needed = 0;
	}
}

void _deleteGPRtoNEONreg(int reg, int flush)
{
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse && arm64neon[i].type == NEONTYPE_GPRREG && arm64neon[i].reg == reg)
		{
			switch (flush)
			{
				case DELETE_REG_FREE: _freeNEONreg(i); break;
				case DELETE_REG_FLUSH:
					if (arm64neon[i].mode & MODE_WRITE)
					{
						_writebackNEONreg(i);
						// Drop MODE_WRITE (keep MODE_READ) so a later
						// _freeNEONreg won't store the same value again.
						arm64neon[i].mode = (arm64neon[i].mode & ~MODE_WRITE) | MODE_READ;
					}
					break;
				case DELETE_REG_FLUSH_AND_FREE: _freeNEONreg(i); break;
				case DELETE_REG_FREE_NO_WRITEBACK: _freeNEONregWithoutWriteback(i); break;
			}
			return;
		}
	}
}

void _deleteFPtoNEONreg(int reg, int flush)
{
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse && arm64neon[i].type == NEONTYPE_FPREG && arm64neon[i].reg == reg)
		{
			switch (flush)
			{
				case DELETE_REG_FREE: _freeNEONreg(i); break;
				case DELETE_REG_FLUSH:
					if (arm64neon[i].mode & MODE_WRITE)
					{
						_writebackNEONreg(i);
						// Drop MODE_WRITE (keep MODE_READ) so a later
						// _freeNEONreg won't store the same value again.
						arm64neon[i].mode = (arm64neon[i].mode & ~MODE_WRITE) | MODE_READ;
					}
					break;
				case DELETE_REG_FLUSH_AND_FREE: _freeNEONreg(i); break;
				case DELETE_REG_FREE_NO_WRITEBACK: _freeNEONregWithoutWriteback(i); break;
			}
			return;
		}
	}
}

void _reallocateNEONreg(int neonreg, int newtype, int newreg, int newmode, bool writeback)
{
	if (arm64neon[neonreg].inuse && writeback)
		_writebackNEONreg(neonreg);

	arm64neon[neonreg].inuse = 1;
	arm64neon[neonreg].type = newtype;
	arm64neon[neonreg].reg = newreg;
	arm64neon[neonreg].mode = newmode;
	arm64neon[neonreg].needed = 1;
	arm64neon[neonreg].counter = g_neonAllocCounter++;
}

int _allocIfUsedGPRtoNEON(int gprreg, int mode)
{
	return EEINST_XMMUSEDTEST(gprreg) ? _allocGPRtoNEONreg(gprreg, mode) : -1;
}

int _allocIfUsedFPUtoNEON(int fpureg, int mode)
{
	return FPUINST_USEDTEST(fpureg) ? _allocFPtoNEONreg(fpureg, mode) : -1;
}

void _flushCOP2regs()
{
	// Flush any VU registers cached in host regs
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse && arm64neon[i].type == NEONTYPE_VFREG)
			_freeNEONreg(i);
	}
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (arm64gprs[i].inuse && arm64gprs[i].type == ARM64TYPE_VIREG)
			_freeArm64GPR(i);
	}
}

// Stubs for COP2 reserved register management
void mVUFreeCOP2GPR(int hostreg)
{
}

bool mVUIsReservedCOP2(int hostreg)
{
	return false;
}

void mVUFreeCOP2NEONreg(int hostreg)
{
}

////////////////////////////////////////////////////////////////////////////////
// Architecture-independent utility functions

void _recClearInst(EEINST* pinst)
{
	std::memset(pinst, 0, sizeof(EEINST));
	std::memset(pinst->regs, EEINST_LIVE, sizeof(pinst->regs));
	std::memset(pinst->fpuregs, EEINST_LIVE, sizeof(pinst->fpuregs));
	std::memset(pinst->vfregs, EEINST_LIVE, sizeof(pinst->vfregs));
	std::memset(pinst->viregs, EEINST_LIVE, sizeof(pinst->viregs));
}

u32 _recIsRegReadOrWritten(EEINST* pinst, int size, u8 xmmtype, u8 reg)
{
	u32 inst = 1;
	while (size-- > 0)
	{
		for (u32 i = 0; i < std::size(pinst->writeType); ++i)
		{
			if ((pinst->writeType[i] == xmmtype) && (pinst->writeReg[i] == reg))
				return inst;
		}
		for (u32 i = 0; i < std::size(pinst->readType); ++i)
		{
			if ((pinst->readType[i] == xmmtype) && (pinst->readReg[i] == reg))
				return inst;
		}
		++inst;
		pinst++;
	}
	return 0;
}

void _recFillRegister(EEINST& pinst, int type, int reg, int write)
{
	if (write)
	{
		for (u32 i = 0; i < std::size(pinst.writeType); ++i)
		{
			if (pinst.writeType[i] == NEONTYPE_TEMP)
			{
				pinst.writeType[i] = type;
				pinst.writeReg[i] = reg;
				return;
			}
		}
		pxAssume(false);
	}
	else
	{
		for (u32 i = 0; i < std::size(pinst.readType); ++i)
		{
			if (pinst.readType[i] == NEONTYPE_TEMP)
			{
				pinst.readType[i] = type;
				pinst.readReg[i] = reg;
				return;
			}
		}
		pxAssume(false);
	}
}
