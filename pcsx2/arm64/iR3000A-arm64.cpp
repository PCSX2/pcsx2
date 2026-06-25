// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "arm64/iR3000A-arm64.h"
#include "arm64/AsmHelpers.h"
#include "Host.h"
#include "R3000A.h"
#include "arm64/BaseblockEx-arm64.h"
#include "R5900OpcodeTables.h"
#include "IopBios.h"
#include "IopHw.h"
#include "DebugTools/SymbolGuardian.h"
#include "Common.h"
#include "VMManager.h"
#include "Config.h"

#include "common/Assertions.h"
#include "common/AlignedMalloc.h"
#include "common/Console.h"
#include "common/FastJmp.h"
#include "common/HeapArray.h"
#include "common/Perf.h"

namespace a64 = vixl::aarch64;

using namespace R3000A;

extern void psxBREAK();

u32 g_psxMaxRecMem = 0;

uptr psxRecLUT[0x10000];
u32 psxhwLUT[0x10000];

static __fi u32 HWADDR(u32 mem) { return psxhwLUT[mem >> 16] + mem; }

static BASEBLOCK* recRAM = nullptr;
static BASEBLOCK* recROM = nullptr;
static BASEBLOCK* recROM1 = nullptr;
static BASEBLOCK* recROM2 = nullptr;
static Arm64BaseBlocks recBlocks;
static u8* recPtr = nullptr;
static u8* recPtrEnd = nullptr;
u32 psxpc;
int psxbranch;
u32 g_iopCyclePenalty;

static EEINST* s_pInstCache = nullptr;
static u32 s_nInstCacheSize = 0;

static BASEBLOCK* s_pCurBlock = nullptr;
static BASEBLOCKEX* s_pCurBlockEx = nullptr;

static u32 s_nEndBlock = 0;
static u32 s_branchTo;
static bool s_nBlockFF;

static u32 s_saveConstRegs[32];
static u32 s_saveHasConstReg = 0, s_saveFlushedConstReg = 0;
static EEINST* s_psaveInstInfo = nullptr;

u32 s_psxBlockCycles = 0;
static u32 s_savenBlockCycles = 0;
static bool s_recompilingDelaySlot = false;

static void iPsxBranchTest(u32 newpc, u32 cpuBranch);
void psxRecompileNextInstruction(bool delayslot, bool swapped_delayslot);

extern void (*rpsxBSC[64])();
void rpsxpropBSC(EEINST* prev, EEINST* pinst);

static void iopClearRecLUT(BASEBLOCK* base, int count);
static void iopRecError(int err);

#define PSX_GETBLOCK(x) PC_GETBLOCK_(x, psxRecLUT)

#define PSXREC_CLEARM(mem) \
	(((mem) < g_psxMaxRecMem && (psxRecLUT[(mem) >> 16] + (mem))) ? \
			psxRecClearMem(mem) : \
			4)

// LUT page management — same as x86 (architecture-independent)
static DynamicHeapArray<BASEBLOCK, 4096> recLutReserve;
static DynamicHeapArray<BASEBLOCK, 4096> recLutUnmapped;
static size_t recLutEntries = 0;
static bool extraRam = false;

// Constant pool for the IOP recompiler
static ArmConstantPool s_iopConstantPool;

// =====================================================================================================
//  Dynamically Compiled Dispatchers - R3000A ARM64
// =====================================================================================================

static void iopRecRecompile(u32 startpc);

static const void* iopDispatcherEvent = nullptr;
static const void* iopDispatcherReg = nullptr;
static const void* iopJITCompile = nullptr;
static const void* iopEnterRecompiledCode = nullptr;
static const void* iopExitRecompiledCode = nullptr;
static const void* iopUnmappedRecLUTPage = nullptr;
static void recEventTest()
{
	_cpuEventTest_Shared();
}

// ARM64 dispatcher: Load PC → two-level LUT lookup → jump to block
//
// psxRecLUT[pc >> 16] gives a base pointer to the BASEBLOCK array for that 64K page.
// Each BASEBLOCK is 8 bytes (one uptr function pointer).
// Index within page: (pc & 0xFFFF) >> 2 (since instructions are 4-byte aligned).
// So: base + ((pc & 0xFFFF) >> 2) * 8 = base + (pc & 0xFFFF) * 2
//
// ARM64 codegen:
//   ldr w0, [addr of psxRegs.pc]     // load PC
//   lsr w1, w0, #16                   // page index
//   adr x2, psxRecLUT
//   ldr x2, [x2, x1, lsl #3]         // page base = psxRecLUT[pc >> 16]
//   and w0, w0, #0xFFFF               // low 16 bits
//   add x2, x2, w0, uxtw #1          // base + (pc & 0xFFFF) * 2
//   ldr x2, [x2]                      // block fnptr
//   br  x2                            // jump to compiled code

static const void* _DynGen_DispatcherReg()
{
	u8* retval = armGetCurrentCodePointer();

	// Load psxRegs.pc
	armAsm->Ldr(a64::w0, armPsxRegMem(&psxRegs.pc));

	// Two-level LUT lookup
	armAsm->Lsr(a64::w1, a64::w0, 16);
	armMoveAddressToReg(RSCRATCHADDR, psxRecLUT);
	armAsm->Ldr(RSCRATCHADDR, a64::MemOperand(RSCRATCHADDR, a64::x1, a64::LSL, 3));

	// Index with full PC: base + pc * sizeof(BASEBLOCK)/4 = base + pc * 2
	// recLUT_SetPage adjusts base to account for upper bits, so use full pc.
	armAsm->Add(RSCRATCHADDR, RSCRATCHADDR, a64::Operand(a64::x0, a64::LSL, 1));

	// Load block function pointer and jump
	armAsm->Ldr(RSCRATCHADDR, a64::MemOperand(RSCRATCHADDR));
	armAsm->Br(RSCRATCHADDR);

	return retval;
}

// Called when a block hasn't been compiled yet — compile it, then dispatch
static const void* _DynGen_JITCompile()
{
	u8* retval = armGetCurrentCodePointer();

	// Call iopRecRecompile(psxRegs.pc)
	armAsm->Ldr(RWARG1, armPsxRegMem(&psxRegs.pc));
	armEmitCall((void*)iopRecRecompile);

	// Now dispatch to the newly compiled block
	armEmitJmp(iopDispatcherReg);

	return retval;
}

// Entry point called from C code — sets up stack frame, enters dispatcher loop
static const void* _DynGen_EnterRecompiledCode()
{
	u8* retval = armGetCurrentCodePointer();

	// Save callee-saved registers and set up stack frame
	armBeginStackFrame(false);

	// Pin &psxRegs into RPSXSTATE for the duration of IOP JIT execution. Like
	// EE's RSTATE, this turns "armMoveAddressToReg(scratch, &psxRegs.X); ldr"
	// into a single ldr [RPSXSTATE, #off]. Callee-saved across all C calls.
	armMoveAddressToReg(RPSXSTATE, &psxRegs);

	// Jump into the dispatcher
	armEmitJmp(iopDispatcherReg);

	// Exit point — restore callee-saved registers and return
	iopExitRecompiledCode = armGetCurrentCodePointer();
	armEndStackFrame(false);
	armAsm->Ret();

	return retval;
}

// Error handler for jumps to unmapped memory
static const void* _DynGen_UnmappedRecLUTPage()
{
	u8* retval = armGetCurrentCodePointer();

	armAsm->Mov(RWARG1, 0);
	armEmitCall((void*)iopRecError);
	armEmitJmp(iopExitRecompiledCode);

	return retval;
}

// Generate all dispatcher stubs during reset
static void _DynGen_Dispatchers()
{
	const u8* start = armGetCurrentCodePointer();

	// Event test: call recEventTest, then fall through to dispatcher
	iopDispatcherEvent = armGetCurrentCodePointer();
	armEmitCall((void*)recEventTest);

	iopDispatcherReg = _DynGen_DispatcherReg();
	iopJITCompile = _DynGen_JITCompile();
	iopEnterRecompiledCode = _DynGen_EnterRecompiledCode();
	iopUnmappedRecLUTPage = _DynGen_UnmappedRecLUTPage();

	// Block linker needs iopJITCompile so it can route stale / not-yet-
	// compiled link sites through the dispatcher path. Mirrors EE rec
	// at iR5900-arm64.cpp:674 and x86 IOP rec at iR3000A.cpp:257.
	recBlocks.SetJITCompile(iopJITCompile);

	Perf::any.Register(start, static_cast<u32>(armGetCurrentCodePointer() - start), "IOP Dispatcher");
}

static void iopRecError(int err)
{
	switch (err)
	{
		case 0:
			Host::ReportErrorAsync("R3000A Exception",
				fmt::format("Unrecognized opcode (PC: 0x{:08x})", psxRegs.pc));
			break;

		case 1:
			Host::ReportErrorAsync("R3000A Exception",
				fmt::format("Jump to unaligned address (PC: 0x{:08x})", psxRegs.pc));
			break;
	}

	VMManager::SetPaused(true);
	Cpu->ExitExecution();
}

// =====================================================================================================
//  Register allocation and code generation helpers
// =====================================================================================================

void _psxFlushConstReg(int reg)
{
	if (PSX_IS_CONST1(reg) && !(g_psxFlushedConstReg & (1 << reg)))
	{
		armAsm->Mov(RWSCRATCH, g_psxConstRegs[reg]);
		armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.GPR.r[reg]));
		g_psxFlushedConstReg |= (1 << reg);
	}
}

void _psxFlushConstRegs()
{
	for (int i = 1; i < 32; ++i)
	{
		if (g_psxHasConstReg & (1 << i))
		{
			if (!(g_psxFlushedConstReg & (1 << i)))
			{
				armAsm->Mov(RWSCRATCH, g_psxConstRegs[i]);
				armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.GPR.r[i]));
				g_psxFlushedConstReg |= 1 << i;
			}

			if (g_psxHasConstReg == g_psxFlushedConstReg)
				break;
		}
	}
}

void _psxDeleteReg(int reg, int flush)
{
	if (!reg)
		return;
	if (flush && PSX_IS_CONST1(reg))
		_psxFlushConstReg(reg);

	PSX_DEL_CONST(reg);
	_deletePSXtoArm64GPR(reg, flush ? DELETE_REG_FREE : DELETE_REG_FREE_NO_WRITEBACK);
}

void _psxMoveGPRtoR(const a64::Register& to, int fromgpr)
{
	if (PSX_IS_CONST1(fromgpr))
	{
		armAsm->Mov(to.IsX() ? to : a64::Register(to.GetCode(), a64::kWRegSize), g_psxConstRegs[fromgpr]);
	}
	else
	{
		const int reg = EEINST_USEDTEST(fromgpr)
			? _allocArm64GPR(ARM64TYPE_PSX, fromgpr, MODE_READ)
			: _checkArm64GPR(ARM64TYPE_PSX, fromgpr, MODE_READ);
		if (reg >= 0)
			armAsm->Mov(to.IsX() ? to : a64::Register(to.GetCode(), a64::kWRegSize), armWRegister(reg));
		else
			armLoadPsxRegPtr(to.IsX() ? to : a64::Register(to.GetCode(), a64::kWRegSize), &psxRegs.GPR.r[fromgpr]);
	}
}

void _psxFlushCall(int flushtype)
{
	// Free caller-saved registers (and optionally others per flushtype)
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (!arm64gprs[i].inuse)
			continue;

		if (!armIsCalleeSavedRegister(i) ||
			((flushtype & FLUSH_FREE_NONTEMP_X86) && arm64gprs[i].type != ARM64TYPE_TEMP) ||
			((flushtype & FLUSH_FREE_TEMP_X86) && arm64gprs[i].type == ARM64TYPE_TEMP))
		{
			_freeArm64GPR(i);
		}
	}

	if (flushtype & FLUSH_ALL_X86)
		_flushArm64GPRregs();

	if (flushtype & FLUSH_CONSTANT_REGS)
		_psxFlushConstRegs();

	if (flushtype & FLUSH_PC)
	{
		armAsm->Mov(RWSCRATCH, psxpc);
		armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.pc));
	}
}

void _psxFlushAllDirty()
{
	for (u32 i = 0; i < 32; ++i)
	{
		if (PSX_IS_CONST1(i))
			_psxFlushConstReg(i);
	}

	_flushArm64GPRregs();
}

void _psxOnWriteReg(int reg)
{
	PSX_DEL_CONST(reg);
}

void psxSaveBranchState()
{
	s_savenBlockCycles = s_psxBlockCycles;
	memcpy(s_saveConstRegs, g_psxConstRegs, sizeof(g_psxConstRegs));
	s_saveHasConstReg = g_psxHasConstReg;
	s_saveFlushedConstReg = g_psxFlushedConstReg;
	s_psaveInstInfo = g_pCurInstInfo;
	memcpy(s_saveArm64GPRregs, arm64gprs, sizeof(arm64gprs));
}

void psxLoadBranchState()
{
	s_psxBlockCycles = s_savenBlockCycles;
	memcpy(g_psxConstRegs, s_saveConstRegs, sizeof(g_psxConstRegs));
	g_psxHasConstReg = s_saveHasConstReg;
	g_psxFlushedConstReg = s_saveFlushedConstReg;
	g_pCurInstInfo = s_psaveInstInfo;
	memcpy(arm64gprs, s_saveArm64GPRregs, sizeof(arm64gprs));
}

// =====================================================================================================
//  Constant Propagation Code Templates
// =====================================================================================================

// rd = rs op rt — dispatch based on which operands are constant
// rd = rs op rt — matching x86 ordering: const check before _addNeeded
void psxRecompileCodeConst0(R3000AFNPTR constcode, R3000AFNPTR_INFO constscode, R3000AFNPTR_INFO consttcode, R3000AFNPTR_INFO noconstcode, int xmminfo)
{
	if (!_Rd_)
		return;

	if (PSX_IS_CONST2(_Rs_, _Rt_))
	{
		_psxDeleteReg(_Rd_, 0);
		PSX_SET_CONST(_Rd_);
		constcode();
		return;
	}

	int info = 0;
	if (PSX_IS_CONST1(_Rs_))
		info |= PROCESS_CONSTS;
	if (PSX_IS_CONST1(_Rt_))
		info |= PROCESS_CONSTT;

	_addNeededPSXtoArm64GPR(_Rs_);
	_addNeededPSXtoArm64GPR(_Rt_);
	_addNeededPSXtoArm64GPR(_Rd_);

	if (xmminfo & XMMINFO_READS)
	{
		if (!(info & PROCESS_CONSTS))
		{
			int reg = _allocArm64GPR(ARM64TYPE_PSX, _Rs_, MODE_READ);
			if (reg >= 0)
				info |= PROCESS_EE_SET_S(reg);
		}
	}
	if (xmminfo & XMMINFO_READT)
	{
		if (!(info & PROCESS_CONSTT))
		{
			int reg = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_READ);
			if (reg >= 0)
				info |= PROCESS_EE_SET_T(reg);
		}
	}
	if (xmminfo & XMMINFO_WRITED)
	{
		int reg = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE);
		if (reg >= 0)
			info |= PROCESS_EE_SET_D(reg);
	}

	PSX_DEL_CONST(_Rd_);

	if (info & PROCESS_CONSTS)
		constscode(info);
	else if (info & PROCESS_CONSTT)
		consttcode(info);
	else
		noconstcode(info);

	_clearNeededArm64GPRregs();
}

// IOP v1.0 IRX-import HLE backdoor — mirrors x86 psxRecompileIrxImport
// (pcsx2/x86/iR3000A.cpp). The IOP loader leaves module-import trampolines as
// an `ADDIU $0,$0,<index>` whose opcode top half is 0x2400. If the libname at
// the import table for this PC resolves a registered HLE handler, call it
// directly. A non-zero return means the handler advanced psxRegs.pc, so we must
// re-dispatch there instead of tearing the recompiled stint down (commit
// 89c7eb3a2). Restores HostFS (ioman/iomanx) redirection + IOP module symbol
// recovery under the JIT (DT-01). The devbuild-only trace/irxImportDebug path
// is intentionally omitted (diagnostic logging, no guest-visible effect).
void psxRecompileIrxImport()
{
	const u32 import_table = irxImportTableAddr(psxpc - 4);
	if (!import_table)
		return;

	const std::string libname = iopMemReadString(import_table + 12, 8);
	const irxHLE hle = irxImportHLE(libname, static_cast<u16>(psxRegs.code & 0xffff));
	if (!hle)
		return;

	armAsm->Mov(RWSCRATCH, psxRegs.code);
	armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.code));
	armAsm->Mov(RWSCRATCH, psxpc);
	armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.pc));

	_psxFlushCall(FLUSH_NODESTROY);
	armEmitCall(reinterpret_cast<const void*>(hle));

	// HLE handled it (returned non-zero) → it set psxRegs.pc; re-dispatch.
	armEmitCbnz(a64::w0, iopDispatcherReg);
}

// rt = rs op imm16
// Matches x86 pattern: const check before _addNeeded, PSX_DEL_CONST before noconstcode
void psxRecompileCodeConst1(R3000AFNPTR constcode, R3000AFNPTR_INFO noconstcode, int xmminfo)
{
	if (!_Rt_)
		return;

	// x86 checks const BEFORE _addNeeded
	if (PSX_IS_CONST1(_Rs_))
	{
		_psxDeleteReg(_Rt_, 0);
		PSX_SET_CONST(_Rt_);
		constcode();
		return;
	}

	_addNeededPSXtoArm64GPR(_Rs_);
	_addNeededPSXtoArm64GPR(_Rt_);

	int info = 0;

	if (xmminfo & XMMINFO_READS)
	{
		int reg = _allocArm64GPR(ARM64TYPE_PSX, _Rs_, MODE_READ);
		if (reg >= 0)
			info |= PROCESS_EE_SET_S(reg);
	}
	if (xmminfo & XMMINFO_WRITET)
	{
		int reg = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_WRITE);
		if (reg >= 0)
			info |= PROCESS_EE_SET_T(reg);
	}

	// x86 deletes const BEFORE noconstcode
	PSX_DEL_CONST(_Rt_);
	noconstcode(info);

	_clearNeededArm64GPRregs();
}

// rd = rt op sa
void psxRecompileCodeConst2(R3000AFNPTR constcode, R3000AFNPTR_INFO noconstcode, int xmminfo)
{
	if (!_Rd_)
		return;

	_addNeededPSXtoArm64GPR(_Rt_);
	_addNeededPSXtoArm64GPR(_Rd_);

	int info = 0;

	if (PSX_IS_CONST1(_Rt_))
	{
		_psxDeleteReg(_Rd_, 0);
		PSX_SET_CONST(_Rd_);
		constcode();
		_clearNeededArm64GPRregs();
		return;
	}

	if (xmminfo & XMMINFO_READT)
	{
		int reg = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_READ);
		if (reg >= 0)
			info |= PROCESS_EE_SET_T(reg);
	}
	if (xmminfo & XMMINFO_WRITED)
	{
		int reg = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE);
		if (reg >= 0)
			info |= PROCESS_EE_SET_D(reg);
	}

	noconstcode(info);

	_clearNeededArm64GPRregs();
	PSX_DEL_CONST(_Rd_);
}

// [lo,hi] = rt op rs
void psxRecompileCodeConst3(R3000AFNPTR constcode, R3000AFNPTR_INFO constscode, R3000AFNPTR_INFO consttcode, R3000AFNPTR_INFO noconstcode, int LOHI)
{
	_addNeededPSXtoArm64GPR(_Rs_);
	_addNeededPSXtoArm64GPR(_Rt_);

	int info = 0;

	if (PSX_IS_CONST2(_Rs_, _Rt_))
	{
		constcode();
		_clearNeededArm64GPRregs();
		return;
	}

	if (PSX_IS_CONST1(_Rs_))
		info |= PROCESS_CONSTS;
	if (PSX_IS_CONST1(_Rt_))
		info |= PROCESS_CONSTT;

	if (!(info & PROCESS_CONSTS))
	{
		int reg = _allocArm64GPR(ARM64TYPE_PSX, _Rs_, MODE_READ);
		if (reg >= 0)
			info |= PROCESS_EE_SET_S(reg);
	}
	if (!(info & PROCESS_CONSTT))
	{
		int reg = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_READ);
		if (reg >= 0)
			info |= PROCESS_EE_SET_T(reg);
	}

	if (info & PROCESS_CONSTS)
		constscode(info);
	else if (info & PROCESS_CONSTT)
		consttcode(info);
	else
		noconstcode(info);

	_clearNeededArm64GPRregs();
}

// =====================================================================================================
//  Branch handling
// =====================================================================================================

void psxSetBranchReg()
{
	psxbranch = 1;

	// Flush all register allocations first, then load branch target from pcWriteback.
	// This matches the EE SetBranchReg pattern — ensures delay slot results are
	// written back before the branch target overwrites w0.
	_psxFlushCall(FLUSH_EVERYTHING);

	// Load branch target from pcWriteback
	armLoadPsxRegPtr(a64::w0, &psxRegs.pcWriteback);

	// Store to psxRegs.pc
	armAsm->Str(a64::w0, armPsxRegMem(&psxRegs.pc));

	// Check alignment
	a64::Label unaligned;
	armAsm->Tst(a64::w0, 3);
	armAsm->B(&unaligned, a64::ne);

	iPsxBranchTest(0xffffffff, 1);

	armEmitJmp(iopDispatcherReg);

	armAsm->Bind(&unaligned);
	armAsm->Mov(RWARG1, 1);
	armEmitCall((void*)iopRecError);
	armEmitJmp(iopExitRecompiledCode);
}

void psxSetBranchImm(u32 imm)
{
	psxbranch = 1;
	pxAssert(imm);

	armAsm->Mov(RWSCRATCH, imm);
	armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.pc));
	_psxFlushCall(FLUSH_EVERYTHING);
	iPsxBranchTest(imm, imm <= psxpc);

	// Block linking: emit a single B as the patch site. Initially routed
	// through iopJITCompile via recBlocks.Link(); once the target block is
	// compiled, recBlocks.New() rewrites this B's imm26 to branch to the
	// target's fnptr directly, bypassing the dispatcher. Mirrors EE rec
	// at iR5900-arm64.cpp:1064.
	{
		a64::SingleEmissionCheckScope guard(armAsm);
		u8* patch_site = armGetCurrentCodePointer();
		armAsm->b(int64_t{0}); // placeholder; recBlocks.Link will overwrite
		recBlocks.Link(HWADDR(imm), patch_site);
	}
}

static __fi u32 psxScaleBlockCycles()
{
	return s_psxBlockCycles;
}

static void iPsxAddEECycles(u32 blockCycles)
{
	// Subtract cycles * 8 from iopCycleEE
	armAsm->Ldr(RWSCRATCH, armPsxRegMem(&psxRegs.iopCycleEE));

	if (blockCycles != 0xFFFFFFFF)
	{
		if (blockCycles * 8 < 4096)
			armAsm->Sub(RWSCRATCH, RWSCRATCH, blockCycles * 8);
		else
		{
			armAsm->Mov(a64::w1, blockCycles * 8);
			armAsm->Sub(RWSCRATCH, RWSCRATCH, a64::w1);
		}
	}
	else
	{
		// blockCycles in w0 (from wait loop optimization)
		armAsm->Sub(RWSCRATCH, RWSCRATCH, a64::w0);
	}

	armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.iopCycleEE));
}

static void iPsxBranchTest(u32 newpc, u32 cpuBranch)
{
	u32 blockCycles = psxScaleBlockCycles();

	if (EmuConfig.Speedhacks.WaitLoop && s_nBlockFF && newpc == s_branchTo)
	{
		// WaitLoop fast-forward: tight busy-wait loop detected.
		// Advance cycle counter to consume the remaining IOP timeslice,
		// clamped to iopNextEventCycle. Matches x86 iR3000A.cpp:1179.
		// new_cycle = old_cycle + (iopCycleEE + 7) / 8
		// new_cycle = min(new_cycle, iopNextEventCycle)
		armAsm->Ldr(a64::x2, armPsxRegMem(&psxRegs.cycle));   // x2 = old cycle
		armAsm->Mov(a64::x4, a64::x2);                        // x4 = old cycle (saved)

		// iopCycleEE is the ONLY signed quantity in this block — it can go
		// negative, so the timeslice divide uses Asr (arithmetic shift). The
		// cycle/iopNextEventCycle clamp below is unsigned (Csel hi); don't
		// swap predicates between the two.
		armAsm->Ldr(RWSCRATCH, armPsxRegMem(&psxRegs.iopCycleEE)); // w8 = iopCycleEE (s32)
		armAsm->Add(RWSCRATCH, RWSCRATCH, 7);
		armAsm->Asr(RWSCRATCH, RWSCRATCH, 3);                  // w8 = (iopCycleEE + 7) >> 3 (signed)
		armAsm->Add(a64::x2, a64::x2, a64::x8);               // x2 = cycle + advance

		// Clamp to iopNextEventCycle
		armAsm->Ldr(a64::x3, armPsxRegMem(&psxRegs.iopNextEventCycle));   // x3 = iopNextEventCycle
		armAsm->Cmp(a64::x2, a64::x3);
		// Both cycle counters are u32; use unsigned predicate so the clamp
		// stays correct after either operand crosses 2^31. x86 CMOVA / JL
		// (the upstream reference) are likewise unsigned.
		armAsm->Csel(a64::x2, a64::x3, a64::x2, a64::hi);     // x2 = min(x2, x3) unsigned

		// Store new cycle
		armAsm->Str(a64::x2, armPsxRegMem(&psxRegs.cycle));

		// consumed = (new_cycle - old_cycle) << 3, in w0 for iPsxAddEECycles
		armAsm->Sub(a64::x0, a64::x2, a64::x4);
		armAsm->Lsl(a64::w0, a64::w0, 3);

		// Subtract consumed cycles from iopCycleEE
		iPsxAddEECycles(0xFFFFFFFF); // uses w0 as the cycle count

		armAsm->Cmp(RWSCRATCH, 0);
		armEmitCondBranch(a64::le, iopExitRecompiledCode);

		// Call event test
		armEmitCall((void*)iopEventTest);

		if (newpc != 0xffffffff)
		{
			armAsm->Ldr(RWSCRATCH, armPsxRegMem(&psxRegs.pc));
			armAsm->Cmp(RWSCRATCH, newpc);
			armEmitCondBranch(a64::ne, iopDispatcherReg);
		}
	}
	else
	{
		// Normal path: add block cycles and check events
		armAsm->Ldr(a64::x2, armPsxRegMem(&psxRegs.cycle));
		if (blockCycles < 4096)
			armAsm->Add(a64::x2, a64::x2, blockCycles);
		else
		{
			armAsm->Mov(a64::x3, static_cast<u64>(blockCycles));
			armAsm->Add(a64::x2, a64::x2, a64::x3);
		}
		armAsm->Str(a64::x2, armPsxRegMem(&psxRegs.cycle));

		// Subtract from iopCycleEE — exit if <= 0
		iPsxAddEECycles(blockCycles);
		armAsm->Cmp(RWSCRATCH, 0);
		armEmitCondBranch(a64::le, iopExitRecompiledCode);

		// Check if event is pending: cycle >= iopNextEventCycle
		armAsm->Ldr(a64::x3, armPsxRegMem(&psxRegs.iopNextEventCycle));
		armAsm->Cmp(a64::x2, a64::x3);
		a64::Label noEvent;
		// Unsigned predicate — see clamp note above. Both operands are u32;
		// signed lt would mis-fire after either crosses 2^31.
		armAsm->B(&noEvent, a64::lo);

		// Event pending — call event test
		armEmitCall((void*)iopEventTest);

		if (newpc != 0xffffffff)
		{
			armAsm->Ldr(RWSCRATCH, armPsxRegMem(&psxRegs.pc));
			armAsm->Cmp(RWSCRATCH, newpc);
			armEmitCondBranch(a64::ne, iopDispatcherReg);
		}

		armAsm->Bind(&noEvent);
	}
}

// =====================================================================================================
//  Delay slot swap optimization
// =====================================================================================================

// Hoist the branch's delay-slot instruction ahead of the branch test when it
// provably doesn't read/write any of the branch's source/dest registers.
// Skips the psxSaveBranchState/recompile/psxLoadBranchState/recompile-again
// ceremony that the branch handler otherwise pays. Mirrors x86 iR3000A.cpp:439
// opcode-for-opcode.
bool psxTrySwapDelaySlot(u32 rs, u32 rt, u32 rd)
{
	if (s_recompilingDelaySlot)
		return false;

	const u32 opcode_encoded = iopMemRead32(psxpc);
	if (opcode_encoded == 0)
	{
		psxRecompileNextInstruction(true, true);
		return true;
	}

	const u32 opcode_rs = ((opcode_encoded >> 21) & 0x1F);
	const u32 opcode_rt = ((opcode_encoded >> 16) & 0x1F);
	const u32 opcode_rd = ((opcode_encoded >> 11) & 0x1F);

	switch (opcode_encoded >> 26)
	{
		case 8: // ADDI
		case 9: // ADDIU
		case 10: // SLTI
		case 11: // SLTIU
		case 12: // ANDI
		case 13: // ORI
		case 14: // XORI
		case 15: // LUI
		case 32: // LB
		case 33: // LH
		case 34: // LWL
		case 35: // LW
		case 36: // LBU
		case 37: // LHU
		case 38: // LWR
		case 39: // LWU
		case 40: // SB
		case 41: // SH
		case 42: // SWL
		case 43: // SW
		case 46: // SWR
		{
			if ((rs != 0 && rs == opcode_rt) || (rt != 0 && rt == opcode_rt) || (rd != 0 && (rd == opcode_rs || rd == opcode_rt)))
				return false;
		}
		break;

		case 50: // LWC2
		case 58: // SWC2
			break;

		case 0: // SPECIAL
		{
			switch (opcode_encoded & 0x3F)
			{
				case 0:  // SLL
				case 2:  // SRL
				case 3:  // SRA
				case 4:  // SLLV
				case 6:  // SRLV
				case 7:  // SRAV
				case 32: // ADD
				case 33: // ADDU
				case 34: // SUB
				case 35: // SUBU
				case 36: // AND
				case 37: // OR
				case 38: // XOR
				case 39: // NOR
				case 42: // SLT
				case 43: // SLTU
				{
					if ((rs != 0 && rs == opcode_rd) || (rt != 0 && rt == opcode_rd) || (rd != 0 && (rd == opcode_rs || rd == opcode_rt)))
						return false;
				}
				break;

				case 15: // SYNC
				case 24: // MULT
				case 25: // MULTU
				case 26: // DIV
				case 27: // DIVU
					break;

				default:
					return false;
			}
		}
		break;

		case 16: // COP0
		case 17: // COP1
		case 18: // COP2
		case 19: // COP3
		{
			switch ((opcode_encoded >> 21) & 0x1F)
			{
				case 0: // MFC
				case 2: // CFC
				{
					if ((rs != 0 && rs == opcode_rt) || (rt != 0 && rt == opcode_rt) || (rd != 0 && rd == opcode_rt))
						return false;
				}
				break;

				case 4: // MTC
				case 6: // CTC
					break;

				default:
					// GTE (COP2) ops are safe.
					if ((opcode_encoded >> 26) != 18)
						return false;
					break;
			}
		}
		break;

		default:
			return false;
	}

	psxRecompileNextInstruction(true, true);
	return true;
}

// =====================================================================================================
//  Instruction recompilation
// =====================================================================================================

// delayslot = true when called from a branch handler to compile its delay
// slot. swapped_delayslot = true when called via psxTrySwapDelaySlot —
// the delay slot is being hoisted ahead of the branch test, so we must
// snapshot psxRegs.code + g_pCurInstInfo so the enclosing branch handler's
// codegen continues against its own instruction, and suppress the trailing
// _clearNeededArm64GPRregs because the branch hasn't emitted its flush yet.
void psxRecompileNextInstruction(bool delayslot, bool swapped_delayslot)
{
	// IOP recompiler breakpoint support is not implemented on this target.

	const u32 old_code = psxRegs.code;
	EEINST* const old_inst_info = g_pCurInstInfo;
	s_recompilingDelaySlot = delayslot;

	// Read the instruction
	psxRegs.code = iopMemRead32(psxpc);
	s_psxBlockCycles++;
	psxpc += 4;
	g_pCurInstInfo++;

	g_iopCyclePenalty = 0;

	// Dispatch to the appropriate recompiler function
	rpsxBSC[psxRegs.code >> 26]();

	s_psxBlockCycles += g_iopCyclePenalty;

	if (!swapped_delayslot)
	{
		_clearNeededArm64GPRregs();
	}
	else
	{
		psxRegs.code = old_code;
		g_pCurInstInfo = old_inst_info;
	}

	s_recompilingDelaySlot = false;
}

// SYSCALL and BREAK
void rpsxSYSCALL()
{
	armAsm->Mov(RWSCRATCH, psxRegs.code);
	armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.code));

	armAsm->Mov(RWSCRATCH, psxpc - 4);
	armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.pc));

	_psxFlushCall(FLUSH_NODESTROY);

	armAsm->Mov(RWARG1, 0x20);
	armAsm->Mov(RWARG2, psxbranch == 1 ? 1 : 0);
	armEmitCall((void*)psxException);

	// Check if PC changed
	armAsm->Ldr(RWSCRATCH, armPsxRegMem(&psxRegs.pc));
	armAsm->Cmp(RWSCRATCH, psxpc - 4);
	a64::Label noChange;
	armAsm->B(&noChange, a64::eq);

	// PC changed — update cycles and re-dispatch
	armAsm->Ldr(a64::x0, armPsxRegMem(&psxRegs.cycle));
	if (psxScaleBlockCycles() < 4096)
		armAsm->Add(a64::x0, a64::x0, psxScaleBlockCycles());
	else
	{
		armAsm->Mov(a64::x1, static_cast<u64>(psxScaleBlockCycles()));
		armAsm->Add(a64::x0, a64::x0, a64::x1);
	}
	armAsm->Str(a64::x0, armPsxRegMem(&psxRegs.cycle));
	iPsxAddEECycles(psxScaleBlockCycles());
	armEmitJmp(iopDispatcherReg);

	armAsm->Bind(&noChange);
}

void rpsxBREAK()
{
	armAsm->Mov(RWSCRATCH, psxRegs.code);
	armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.code));

	armAsm->Mov(RWSCRATCH, psxpc - 4);
	armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.pc));

	_psxFlushCall(FLUSH_NODESTROY);

	armAsm->Mov(RWARG1, 0x24);
	armAsm->Mov(RWARG2, psxbranch == 1 ? 1 : 0);
	armEmitCall((void*)psxException);

	armAsm->Ldr(RWSCRATCH, armPsxRegMem(&psxRegs.pc));
	armAsm->Cmp(RWSCRATCH, psxpc - 4);
	a64::Label noChange;
	armAsm->B(&noChange, a64::eq);

	armAsm->Ldr(a64::x0, armPsxRegMem(&psxRegs.cycle));
	if (psxScaleBlockCycles() < 4096)
		armAsm->Add(a64::x0, a64::x0, psxScaleBlockCycles());
	else
	{
		armAsm->Mov(a64::x1, static_cast<u64>(psxScaleBlockCycles()));
		armAsm->Add(a64::x0, a64::x0, a64::x1);
	}
	armAsm->Str(a64::x0, armPsxRegMem(&psxRegs.cycle));
	iPsxAddEECycles(psxScaleBlockCycles());
	armEmitJmp(iopDispatcherReg);

	armAsm->Bind(&noChange);
}

// =====================================================================================================
//  Memory management and block clearing
// =====================================================================================================

// recLUT_SetPage is defined in BaseblockEx.h (architecture-independent)

static __fi u32 psxRecClearMem(u32 pc)
{
	// Look up the containing block via recBlocks instead of testing the
	// per-instruction LUT fnptr. The LUT only patches block-head slots away
	// from iopJITCompile; mid-block words still hold the trampoline, so a
	// fnptr-based early-exit would leak mid-block writes through to stale
	// compiled code.
	int blockidx = recBlocks.Index(HWADDR(pc));
	if (blockidx == -1)
		return 4;

	u32 lowerextent = pc, upperextent = pc + 4;

	while (BASEBLOCKEX* pexblock = recBlocks[blockidx - 1])
	{
		if (pexblock->startpc + pexblock->size * 4 <= HWADDR(lowerextent))
			break;

		lowerextent = std::min(lowerextent, pexblock->startpc);
		blockidx--;
	}

	while (BASEBLOCKEX* pexblock = recBlocks[blockidx])
	{
		if (pexblock->startpc >= HWADDR(upperextent))
			break;

		lowerextent = std::min(lowerextent, pexblock->startpc);
		upperextent = std::max(upperextent, pexblock->startpc + pexblock->size * 4);
		recBlocks.Remove(blockidx, blockidx);
	}

	// Clear all blocks in the range
	for (u32 addr = lowerextent; addr < upperextent; addr += 4)
	{
		BASEBLOCK* p = PSX_GETBLOCK(addr);
		p->SetFnptr((uptr)iopJITCompile);
	}

	return upperextent - pc;
}

static void recClearIOP(u32 Addr, u32 Size)
{
	u32 end = Addr + Size * 4;
	for (u32 i = Addr; i < end; i += PSXREC_CLEARM(i))
		;
}

static void iopClearRecLUT(BASEBLOCK* base, int count)
{
	for (int i = 0; i < count / 4; i++)
		base[i].SetFnptr((uptr)iopJITCompile);
}

// =====================================================================================================
//  Reserve / Reset / Shutdown / Execute
// =====================================================================================================

static void recReserveRAM()
{
	recLutEntries =
		((Ps2MemSize::ExposedIopRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) / 4);

	if (recLutReserve.size() != recLutEntries)
		recLutReserve.resize(recLutEntries);

	recLutUnmapped.resize(_64kb / 4);

	BASEBLOCK* curpos = recLutReserve.data();
	recRAM = curpos;
	curpos += (Ps2MemSize::ExposedIopRam / 4);
	recROM = curpos;
	curpos += (Ps2MemSize::Rom / 4);
	recROM1 = curpos;
	curpos += (Ps2MemSize::Rom1 / 4);
	recROM2 = curpos;
	curpos += (Ps2MemSize::Rom2 / 4);
}

static void recReserve()
{
	Console.WriteLn(Color_Green, "IOP: ARM64 Recompiler reserved.");
	recPtr = SysMemory::GetIOPRec();
	recPtrEnd = SysMemory::GetIOPRecEnd() - _64kb;

	recReserveRAM();

	pxAssertRel(!s_pInstCache, "InstCache not allocated");
	s_nInstCacheSize = 128;
	s_pInstCache = (EEINST*)malloc(sizeof(EEINST) * s_nInstCacheSize);
	if (!s_pInstCache)
		pxFailRel("Failed to allocate R3000A InstCache array.");

	// Initialize constant pool
	// Reserve some space at the end of the IOP rec region for the constant pool
	const u32 poolSize = 65536;
	u8* poolBase = SysMemory::GetIOPRecEnd() - poolSize;
	s_iopConstantPool.Init(poolBase, poolSize);
}

void recResetIOP()
{
	Console.WriteLn(Color_Green, "iR3000A-ARM64 Recompiler reset.");

	if (CHECK_EXTRAMEM != extraRam)
	{
		recReserveRAM();
		extraRam = !extraRam;
	}

	armSetAsmPtr(SysMemory::GetIOPRec(), SysMemory::GetIOPRecEnd() - SysMemory::GetIOPRec(), &s_iopConstantPool);
	armStartBlock();
	_DynGen_Dispatchers();
	recPtr = armEndBlock();

	iopClearRecLUT(reinterpret_cast<BASEBLOCK*>(recLutReserve.data()),
		Ps2MemSize::ExposedIopRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2);

	BASEBLOCK* unmapped = recLutUnmapped.data();

	for (int i = 0; i < 0x10000; i++)
		recLUT_SetPage(psxRecLUT, psxhwLUT, unmapped, i, 0, 0);

	for (int i = 0; i < _64kb / 4; i++)
		unmapped[i].SetFnptr((uptr)iopUnmappedRecLUTPage);

	// Map IOP RAM (with mirrors)
	for (int i = 0; i < 0x80; i++)
	{
		u32 mask = (Ps2MemSize::ExposedIopRam / _64kb) - 1;
		recLUT_SetPage(psxRecLUT, psxhwLUT, recRAM, 0x0000, i, i & mask);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recRAM, 0x8000, i, i & mask);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recRAM, 0xa000, i, i & mask);
	}

	// Map ROM
	for (int i = 0x1fc0; i < 0x2000; i++)
	{
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM, 0x0000, i, i - 0x1fc0);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM, 0x8000, i, i - 0x1fc0);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM, 0xa000, i, i - 0x1fc0);
	}

	for (int i = 0x1e00; i < 0x1e40; i++)
	{
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM1, 0x0000, i, i - 0x1e00);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM1, 0x8000, i, i - 0x1e00);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM1, 0xa000, i, i - 0x1e00);
	}

	for (int i = 0x1e40; i < 0x1e48; i++)
	{
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM2, 0x0000, i, i - 0x1e40);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM2, 0x8000, i, i - 0x1e40);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM2, 0xa000, i, i - 0x1e40);
	}

	if (s_pInstCache)
		memset(s_pInstCache, 0, sizeof(EEINST) * s_nInstCacheSize);

	recBlocks.Reset();
	g_psxMaxRecMem = 0;

	psxbranch = 0;
}

static void recShutdown()
{
	s_iopConstantPool.Destroy();
	recLutReserve.deallocate();

	safe_free(s_pInstCache);
	s_nInstCacheSize = 0;

	recPtr = nullptr;
	recPtrEnd = nullptr;
}

static __noinline s32 recExecuteBlock(s32 eeCycles)
{
	psxRegs.iopBreak = 0;
	psxRegs.iopCycleEE = eeCycles;

	((void (*)())iopEnterRecompiledCode)();

	return psxRegs.iopBreak + psxRegs.iopCycleEE;
}

// =====================================================================================================
//  Main Recompilation Loop
// =====================================================================================================

static void iopRecRecompile(const u32 startpc)
{
	u32 i;
	u32 link_next_block = 0;

	// BIOS hacks
	if (startpc == 0x890)
	{
		DevCon.WriteLn(Color_Gray, "R3000 Debugger: Branch to 0x890 (SYSMEM). Clearing modules.");
		R3000SymbolGuardian.ClearIrxModules();
	}

	if (startpc == 0x1630 && EmuConfig.CurrentIRX.length() > 3)
	{
		if (iopMemRead32(0x20018) == 0x1F)
			iopMemWrite32(0x20094, 0xbffc0000);
	}

	if (startpc == 0xbfc4a000)
		psxRegs.GPR.n.a0 = Ps2MemSize::ExposedIopRam >> 20;

	pxAssert(startpc);

	// Reset code buffer if full
	if (recPtr >= recPtrEnd)
		recResetIOP();

	armSetAsmPtr(recPtr, recPtrEnd - recPtr + _64kb, &s_iopConstantPool);
	armStartBlock();

	s_pCurBlock = PSX_GETBLOCK(startpc);
	pxAssert(s_pCurBlock->GetFnptr() == (uptr)iopJITCompile);

	// armStartBlock() aligned armAsmPtr to 16 bytes, so the actual block
	// code starts at armGetCurrentCodePointer(), not at recPtr. Block
	// linking branches to BASEBLOCKEX::fnptr, so it must be the aligned
	// address — using recPtr instead lands the branch on padding bytes
	// and triggers SIGILL. Same fix as EE rec at iR5900-arm64.cpp:1751.
	const uptr block_fnptr = (uptr)armGetCurrentCodePointer();

	s_pCurBlockEx = recBlocks.Get(HWADDR(startpc));
	if (!s_pCurBlockEx || s_pCurBlockEx->startpc != HWADDR(startpc))
		s_pCurBlockEx = recBlocks.New(HWADDR(startpc), block_fnptr);

	psxbranch = 0;

	s_pCurBlock->SetFnptr(block_fnptr);
	s_psxBlockCycles = 0;
	// Reset recomp state
	psxpc = startpc;
	g_psxHasConstReg = g_psxFlushedConstReg = 1;

	_initArm64GPRregs();

	// BIOS call interception
	if ((psxHu32(HW_ICFG) & 8) && (HWADDR(startpc) == 0xa0 || HWADDR(startpc) == 0xb0 || HWADDR(startpc) == 0xc0))
	{
		armEmitCall((void*)psxBiosCall);
		// If psxBiosCall returns non-zero, skip to dispatcher
		armAsm->Tst(RWRET, RWRET);
		armEmitCondBranch(a64::ne, iopDispatcherReg);
	}

	// Scan for block boundary
	i = startpc;
	s_nEndBlock = 0xffffffff;
	s_branchTo = -1;

	while (1)
	{
		BASEBLOCK* pblock = PSX_GETBLOCK(i);
		if (i != startpc && pblock->GetFnptr() != (uptr)iopJITCompile)
		{
			link_next_block = 1;
			s_nEndBlock = i;
			break;
		}

		psxRegs.code = iopMemRead32(i);

		switch (psxRegs.code >> 26)
		{
			case 0: // special
				if (_Funct_ == 8 || _Funct_ == 9) // JR, JALR
				{
					s_nEndBlock = i + 8;
					goto StartRecomp;
				}
				break;

			case 1: // regimm
				if (_Rt_ == 0 || _Rt_ == 1 || _Rt_ == 16 || _Rt_ == 17)
				{
					s_branchTo = _Imm_ * 4 + i + 4;
					if (s_branchTo > startpc && s_branchTo < i)
						s_nEndBlock = s_branchTo;
					else
						s_nEndBlock = i + 8;
					goto StartRecomp;
				}
				break;

			case 2: // J
			case 3: // JAL
				s_branchTo = (_InstrucTarget_ << 2) | ((i + 4) & 0xf0000000);
				s_nEndBlock = i + 8;
				goto StartRecomp;

			case 4: case 5: case 6: case 7: // BEQ, BNE, BLEZ, BGTZ
				s_branchTo = _Imm_ * 4 + i + 4;
				if (s_branchTo > startpc && s_branchTo < i)
					s_nEndBlock = s_branchTo;
				else
					s_nEndBlock = i + 8;
				goto StartRecomp;
		}

		i += 4;
	}

StartRecomp:

	// Detect infinite loops (NOP loops)
	s_nBlockFF = false;
	if (s_branchTo == startpc)
	{
		s_nBlockFF = true;
		for (i = startpc; i < s_nEndBlock; i += 4)
		{
			if (i != s_nEndBlock - 8)
			{
				if (iopMemRead32(i) != 0)
				{
					s_nBlockFF = false;
					break;
				}
			}
		}
	}

	// Instruction liveness analysis (backward pass)
	{
		EEINST* pcur;

		if (s_nInstCacheSize < (s_nEndBlock - startpc) / 4 + 1)
		{
			free(s_pInstCache);
			s_nInstCacheSize = (s_nEndBlock - startpc) / 4 + 10;
			s_pInstCache = (EEINST*)malloc(sizeof(EEINST) * s_nInstCacheSize);
			pxAssert(s_pInstCache != NULL);
		}

		pcur = s_pInstCache + (s_nEndBlock - startpc) / 4;
		_recClearInst(pcur);
		pcur->info = 0;

		for (i = s_nEndBlock; i > startpc; i -= 4)
		{
			psxRegs.code = iopMemRead32(i - 4);
			pcur[-1] = pcur[0];
			rpsxpropBSC(pcur - 1, pcur);
			pcur--;
		}
	}

	// Compile instructions (forward pass)
	g_pCurInstInfo = s_pInstCache;
	while (!psxbranch && psxpc < s_nEndBlock)
	{
		psxRecompileNextInstruction(false, false);
	}

	pxAssert((psxpc - startpc) >> 2 <= 0xffff);
	s_pCurBlockEx->size = (psxpc - startpc) >> 2;

	if (!(psxpc & 0x10000000))
		g_psxMaxRecMem = std::max((psxpc & ~0xa0000000), g_psxMaxRecMem);

	if (psxbranch == 2)
	{
		_psxFlushCall(FLUSH_EVERYTHING);
		iPsxBranchTest(0xffffffff, 1);
		armEmitJmp(iopDispatcherReg);
	}
	else
	{
		if (psxbranch)
			pxAssert(!link_next_block);
		else
		{
			// Non-branch block end: add cycles.
			//
			// The flush MUST precede the cycle accounting below: the accounting
			// uses x0/x1 as scratch, but the register allocator can still hold
			// an unflushed write-back in x0 from the block's last instruction
			// (e.g. SRA writing rd via _allocArm64GPR MODE_WRITE). Without this
			// hoist, the deferred flush at the link-next-block site stores the
			// cycle counter's low bits into the dirty guest register's psxRegs
			// slot.
			_psxFlushCall(FLUSH_EVERYTHING);

			armAsm->Ldr(a64::x0, armPsxRegMem(&psxRegs.cycle));
			u32 scaledCycles = psxScaleBlockCycles();
			if (scaledCycles < 4096)
				armAsm->Add(a64::x0, a64::x0, scaledCycles);
			else
			{
				armAsm->Mov(a64::x1, static_cast<u64>(scaledCycles));
				armAsm->Add(a64::x0, a64::x0, a64::x1);
			}
			armAsm->Str(a64::x0, armPsxRegMem(&psxRegs.cycle));
			iPsxAddEECycles(psxScaleBlockCycles());
		}

		if (link_next_block || !psxbranch)
		{
			pxAssert(psxpc == s_nEndBlock);
			_psxFlushCall(FLUSH_EVERYTHING);

			armAsm->Mov(RWSCRATCH, psxpc);
			armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.pc));

			// Block linking — see psxSetBranchImm.
			{
				a64::SingleEmissionCheckScope guard(armAsm);
				u8* patch_site = armGetCurrentCodePointer();
				armAsm->b(int64_t{0}); // placeholder; recBlocks.Link will overwrite
				recBlocks.Link(HWADDR(psxpc), patch_site);
			}
			psxbranch = 3;
		}
	}

	pxAssert(armGetCurrentCodePointer() < SysMemory::GetIOPRecEnd());

	s_pCurBlockEx->x86size = static_cast<u32>(armGetCurrentCodePointer() - recPtr);

	Perf::iop.RegisterPC((void*)s_pCurBlockEx->fnptr, s_pCurBlockEx->x86size, s_pCurBlockEx->startpc);

	recPtr = armEndBlock();

	pxAssert((g_psxHasConstReg & g_psxFlushedConstReg) == g_psxHasConstReg);

	s_pCurBlock = NULL;
	s_pCurBlockEx = NULL;
}

// =====================================================================================================
//  R3000Acpu struct — the public interface
// =====================================================================================================

R3000Acpu psxRec = {
	recReserve,
	recResetIOP,
	recExecuteBlock,
	recClearIOP,
	recShutdown,
};
