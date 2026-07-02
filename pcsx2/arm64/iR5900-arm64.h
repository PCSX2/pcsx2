// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Config.h"
#include "R5900.h"
#include "R5900OpcodeTables.h"
#include "VU.h"
#include "arm64/iCore-arm64.h"

// Per-category interpreter fallback toggles.
// Comment out a define to enable native ARM64 codegen for that category.
// #define FORCE_INTERP_BRANCH 1
// #define FORCE_INTERP_JUMP 1
// #define FORCE_INTERP_MOVE 1
// #define FORCE_INTERP_SHIFT 1
// #define FORCE_INTERP_ALU 1
// #define FORCE_INTERP_ARITIMM 1
// #define FORCE_INTERP_MULTDIV 1
// #define FORCE_INTERP_MEMORY 1
// #define FORCE_INTERP_COP0 1
// #define FORCE_INTERP_FPU 1
// #define FORCE_INTERP_COP2 1

// Reserved ARM64 registers for the recompiler
// x19: Fastmem base pointer (callee-saved)
#define RFASTMEMBASE vixl::aarch64::x19
// x20: Pointer to cpuRegs struct (callee-saved). Loaded once at JIT entry by
// EnterRecompiledCode and never modified for the duration of JIT execution.
// Use armCpuRegMem() to construct cpuRegs-relative MemOperands cheaply.
#define RSTATE vixl::aarch64::x20
// x24: Pinned &VU0 (callee-saved). Loaded once at JIT entry by
// EnterRecompiledCode; used to address VU0.{VF,VI,ACC,q,statusflag,...}
// fields via single [RVU0, #imm12] load/store in iCOP2-arm64.cpp,
// instead of the 3-mov+ldr abs-addr materialization sequence.
// Survives armEmitCall (callee-saved per AAPCS) and the mVU dispatcher's
// outer Stp/Ldp pair, so cross-EE/mVU dispatches preserve it.
#define RVU0 vixl::aarch64::x24
// x25: Pinned EE cycle DELTA (callee-saved):
//   RECCYCLE = (s64)(cpuRegs.cycle - cpuRegs.nextEventCycle)
// Negative ⇒ the next event is still in the future. Block cycle accounting
// adds into the delta exactly as it used to add into the absolute counter,
// so every block-tail event check is a flag-setting add (or a bare Cmp
// against zero) + B.ge — no nextEventCycle load and no 64-bit compare per
// exit. cpuRegs.cycle in MEMORY stays ABSOLUTE and canonical for all C
// code; armFlushCycleDelta/armReloadCycleDelta convert at the JIT↔C seams.
// nextEventCycle is only written by C code (cpuSetNextEventDelta & co.),
// which can only run inside those seams, so a fresh reload after every
// cycle/event-touching call keeps the delta exact. (The one exception is
// recSafeExitExecution's cross-thread `nextEventCycle = 0` exit poke: the
// in-register delta doesn't see it, so the exit lands at the previously
// scheduled event — worst case ~one hblank later — instead of the very
// next block tail. recEventTest still observes eeRecExitRequested.)
#define RECCYCLE vixl::aarch64::x25
// x22/x23: Write-through pinned read-cache for the two hottest guest GPRs:
//   x22 = cpuRegs.GPR.r[29].UD[0]  ($sp)
//   x23 = cpuRegs.GPR.r[31].UD[0]  ($ra)
// MEMORY STAYS CANONICAL. Every guest-visible write still stores to
// cpuRegs.GPR; the pin mirror is refreshed at the same emission point
// (armStoreEERegPtr write-through, armStoreEEGPRQuad for 128-bit stores).
// Reads that would load UD[0] from memory use the pin register instead
// (armLoadEERegPtr substitution / armEEPinForGPR in the scalar templates).
// Because the pin never holds a value memory doesn't, there is no new
// C-call or block-exit contract: C code reads and writes memory exactly as
// before, and the pins are re-read from memory (armReloadEEGPRPins) after
// the C calls that can write guest GPRs — interpreter fallbacks
// (recCall/recBranchCall), recEventTest (savestate load), recRecompile
// (ELF entry hooks), and eeloadHook/eeloadHook2. The upper 64 bits of the
// 128-bit guest reg are NOT mirrored; only UD[0] accesses match.
// Both host regs are callee-saved, carved out of the dynamic allocator
// pool (ALLOCATABLE_MASK in iCore-arm64.cpp), and are not used by any
// emission context reachable from inside an EE block: COP2 macro-mode flag
// code uses the s_cop2DenormStatusFlag memory scratch (not gprF2/F3), and
// the mVU micro dispatcher saves/restores x19-x28 around VU execution.
#define REEPIN_SP vixl::aarch64::x22
#define REEPIN_RA vixl::aarch64::x23

// Build a MemOperand addressing a cpuRegs field via RSTATE.
// Replaces the 3-instruction `armMoveAddressToReg(RSCRATCHADDR, &cpuRegs.X);
// Ldr/Str ..., [RSCRATCHADDR]` pattern with a single Ldr/Str using a
// signed/unsigned-immediate offset on RSTATE. ARM64 LDR with imm12 covers
// offsets up to 32760 bytes (64-bit) / 16380 bytes (32-bit) — easily larger
// than cpuRegs / fpuRegs combined, so a single instruction suffices for
// every reachable field.
static __fi vixl::aarch64::MemOperand armCpuRegMem(const void* field)
{
	const ptrdiff_t off = reinterpret_cast<const u8*>(field) - reinterpret_cast<const u8*>(&cpuRegs);
	return vixl::aarch64::MemOperand(RSTATE, static_cast<int64_t>(off));
}

// Publish the ABSOLUTE cycle to cpuRegs.cycle before a C call that reads it:
// abs = delta + nextEventCycle. RECCYCLE itself is preserved (still the
// delta); `scratch` is clobbered. Pair with armReloadCycleDelta after any
// call that can advance cpuRegs.cycle or reschedule nextEventCycle.
static __fi void armFlushCycleDelta(const vixl::aarch64::Register& scratch = RXSCRATCH)
{
	armAsm->Ldr(scratch, armCpuRegMem(&cpuRegs.nextEventCycle));
	armAsm->Add(scratch, RECCYCLE, scratch);
	armAsm->Str(scratch, armCpuRegMem(&cpuRegs.cycle));
}

// Re-derive the delta from (possibly modified) cpuRegs.cycle/nextEventCycle
// after a C call. Both fields are re-read, so any rescheduling the callee
// did (cpuSetNextEventDelta, IntCHackCheck cycle bumps, vu0 catch-up, ...)
// is captured exactly. `scratch` is clobbered.
static __fi void armReloadCycleDelta(const vixl::aarch64::Register& scratch = RXSCRATCH)
{
	armAsm->Ldr(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));
	armAsm->Ldr(scratch, armCpuRegMem(&cpuRegs.nextEventCycle));
	armAsm->Sub(RECCYCLE, RECCYCLE, scratch);
}

// Pinned-base load/store helpers: when the target is anywhere inside
// _cpuRegistersPack (cpuRegs + fpuRegs), reach it via [RSTATE, #off] in one
// instruction; otherwise fall back to the generic 4-inst armLoadPtr/StorePtr.
static __fi bool armIsCpuRegPtr(const void* field)
{
	const u8* base = reinterpret_cast<const u8*>(&_cpuRegistersPack);
	const u8* p    = reinterpret_cast<const u8*>(field);
	return p >= base && p < base + sizeof(cpuRegistersPack);
}
// Pin lookup by guest GPR index. Returns the pinned host register mirroring
// GPR.r[gpr].UD[0], or nullptr when gpr is not pinned.
static __fi const vixl::aarch64::Register* armEEPinForGPR(int gpr)
{
	if (gpr == 29)
		return &REEPIN_SP;
	if (gpr == 31)
		return &REEPIN_RA;
	return nullptr;
}

// Pin lookup by target pointer: matches any byte within the LOWER 64 bits of
// a pinned guest GPR slot. *offset_in_dword receives the byte offset (0..7)
// of `field` within UD[0]. UD[1]/UL[2]/UL[3] accesses do not match (the
// upper half is not mirrored).
static __fi const vixl::aarch64::Register* armEEPinForPtr(const void* field, int* offset_in_dword)
{
	const u8* p = reinterpret_cast<const u8*>(field);
	const ptrdiff_t off_sp = p - reinterpret_cast<const u8*>(&cpuRegs.GPR.r[29]);
	if (off_sp >= 0 && off_sp < 8)
	{
		*offset_in_dword = static_cast<int>(off_sp);
		return &REEPIN_SP;
	}
	const ptrdiff_t off_ra = p - reinterpret_cast<const u8*>(&cpuRegs.GPR.r[31]);
	if (off_ra >= 0 && off_ra < 8)
	{
		*offset_in_dword = static_cast<int>(off_ra);
		return &REEPIN_RA;
	}
	return nullptr;
}

// Re-read the pin mirrors from canonical memory. Needed after any C call
// that can write guest GPRs, and at every JIT entry (see the REEPIN_* doc).
static __fi void armReloadEEGPRPins()
{
	armAsm->Ldr(REEPIN_SP, armCpuRegMem(&cpuRegs.GPR.r[29].UD[0]));
	armAsm->Ldr(REEPIN_RA, armCpuRegMem(&cpuRegs.GPR.r[31].UD[0]));
}

static __fi void armLoadEERegPtr(const vixl::aarch64::CPURegister& reg, const void* field)
{
	// Pinned guest GPR: serve the read from the mirror register. The mirror
	// always equals memory, so this is exactly the load it replaces.
	int off;
	if (const vixl::aarch64::Register* pin = armEEPinForPtr(field, &off); pin && reg.IsRegister())
	{
		const vixl::aarch64::Register dst(reg);
		if (off == 0 && reg.Is64Bits())
		{
			armAsm->Mov(dst, *pin);
			return;
		}
		if (off == 0 && reg.Is32Bits())
		{
			armAsm->Mov(dst, pin->W());
			return;
		}
		if (off == 4 && reg.Is32Bits())
		{
			armAsm->Lsr(dst.X(), *pin, 32);
			return;
		}
		// Unusual shapes fall through to the (identical) canonical memory load.
	}
	if (armIsCpuRegPtr(field))
		armAsm->Ldr(reg, armCpuRegMem(field));
	else
		armLoadPtr(reg, field);
}
static __fi void armStoreEERegPtr(const vixl::aarch64::CPURegister& reg, const void* field)
{
	if (armIsCpuRegPtr(field))
		armAsm->Str(reg, armCpuRegMem(field));
	else
		armStorePtr(reg, field);

	// Write-through: keep the pin mirror equal to the memory just written.
	int off;
	if (const vixl::aarch64::Register* pin = armEEPinForPtr(field, &off))
	{
		if (reg.IsRegister())
		{
			const vixl::aarch64::Register src(reg);
			if (off == 0 && reg.Is64Bits())
			{
				if (!src.Is(*pin))
					armAsm->Mov(*pin, src);
				return;
			}
			if (off == 0 && reg.Is32Bits())
			{
				armAsm->Bfi(*pin, src.X(), 0, 32);
				return;
			}
			if (off == 4 && reg.Is32Bits())
			{
				armAsm->Bfi(*pin, src.X(), 32, 32);
				return;
			}
		}
		// Odd store shape (vector reg / sub-word): reload the mirror from the
		// just-written canonical memory.
		armAsm->Ldr(*pin, armCpuRegMem(pin == &REEPIN_SP ? static_cast<const void*>(&cpuRegs.GPR.r[29].UD[0]) : static_cast<const void*>(&cpuRegs.GPR.r[31].UD[0])));
	}
}

// 128-bit guest-GPR store (MMI/NEON writeback, LQ, QMFC2): store the full
// quad, then refresh the pin mirror from lane 0 when gpr is pinned.
static __fi void armStoreEEGPRQuad(const vixl::aarch64::VRegister& q, int gpr)
{
	armAsm->Str(q, armCpuRegMem(&cpuRegs.GPR.r[gpr].UQ));
	if (const vixl::aarch64::Register* pin = armEEPinForGPR(gpr))
		armAsm->Mov(*pin, q.V2D(), 0);
}

// Build a MemOperand addressing a VU0 field via RVU0. VURegs is < 2 KB, so
// every reachable field fits in imm12 for byte/halfword/word/doubleword/quad
// ldr/str. Mirrors armCpuRegMem for VU0 — used by iCOP2-arm64.cpp.
static __fi vixl::aarch64::MemOperand armVU0Mem(const void* field)
{
	const ptrdiff_t off = reinterpret_cast<const u8*>(field) - reinterpret_cast<const u8*>(&VU0);
	return vixl::aarch64::MemOperand(RVU0, static_cast<int64_t>(off));
}

// Emit LD1R from a VU0 field, broadcasting to all lanes. ARM64 LD1R does
// NOT support [base, #imm] addressing — only [base] or post-index. vixl's
// LoadStoreStructAddrModeField silently drops the offset (and the assert
// is gated on VIXL_DEBUG, so Devel builds ship the wrong encoding instead
// of trapping). Materialize the address with a single ADD imm12 instead of
// 3-mov: VURegs fields fit within 4 KB of &VU0, so one ADD suffices.
// Total cost: ADD + LD1R = 2 insns, vs the original 4-insn 3-mov + LD1R.
static __fi void armLd1rVU0(const vixl::aarch64::VRegister& vt, const void* field)
{
	const ptrdiff_t off = reinterpret_cast<const u8*>(field) - reinterpret_cast<const u8*>(&VU0);
	armAsm->Add(RSCRATCHADDR, RVU0, static_cast<int64_t>(off));
	armAsm->Ld1r(vt, vixl::aarch64::MemOperand(RSCRATCHADDR));
}

extern u32 maxrecmem;
extern u32 pc;             // recompiler pc
extern int g_branch;       // set for branch
extern u32 target;         // branch target
extern u32 s_nBlockCycles; // cycles of current block recompiling
extern bool s_nBlockInterlocked; // Current block has VU0 interlocking

//////////////////////////////////////////////////////////////////////////////////////////
// Interpreter fallback macros

#define REC_FUNC(f) \
	void rec##f() \
	{ \
		/* Delete destination register's const/alloc state before interpreter call. \
		 * The interpreter writes directly to cpuRegs, making any cached const or \
		 * allocated register stale. SPECIAL ops (Rd), loads/COP (Rt). */ \
		const u32 _op = cpuRegs.code >> 26; \
		const int _dest = (_op == 0 || _op == 0x1C) ? _Rd_ : _Rt_; \
		if (_dest > 0) \
			_deleteEEreg(_dest, 1); \
		recCall(Interp::f); \
	}

#define REC_FUNC_DEL(f, delreg) \
	void rec##f() \
	{ \
		if ((delreg) > 0) \
			_deleteEEreg(delreg, 1); \
		recCall(Interp::f); \
	}

#define REC_SYS(f) \
	void rec##f() \
	{ \
		recBranchCall(Interp::f); \
	}

#define REC_SYS_DEL(f, delreg) \
	void rec##f() \
	{ \
		if ((delreg) > 0) \
			_deleteEEreg(delreg, 1); \
		recBranchCall(Interp::f); \
	}

extern bool g_recompilingDelaySlot;

// LDL/LDR (and later SDL/SDR) pair fusion. An unaligned 64-bit access is emitted
// by the game as an LDL/LDR pair on the same Rt/Rs whose offsets differ by 7;
// together they are exactly one (un)aligned 64-bit access at the lower address,
// which ARM64 performs in a single op. The leading half emits that fused op and
// sets g_eeUnalignedFused; the trailing half consumes the flag and emits nothing.
// Cleared at block start (the gate guarantees the partner is consumed in-block,
// so the flag never legitimately survives a block; the clear only sweeps residue
// from an aborted compile). g_eeUnalignedFuseCount tallies fusions (tests/diag).
extern bool g_eeUnalignedFused;
extern u32 g_eeUnalignedFuseCount;

// Exclusive end PC of the block currently being recompiled. Used by the LDL/LDR
// fusion to confirm the peeked partner instruction is in the same block.
u32 recCurrentBlockEndPC();

// Used for generating backpatch thunks for fastmem
u8* recBeginThunk();
u8* recEndThunk();

// Branch processing
bool TrySwapDelaySlot(u32 rs, u32 rt, u32 rd, bool allow_loadstore);
void SaveBranchState();
void LoadBranchState();

void recompileNextInstruction(bool delayslot, bool swapped_delay_slot);
void SetBranchReg();
void SetBranchImm(u32 imm);

void iFlushCall(int flushtype);
void recBranchCall(void (*func)());
void recCall(void (*func)());
// Emit the post-interpreter-call TLB-miss exception dispatch (defined in
// iR5900-arm64.cpp). DispatcherReg/s_recTlbMissOccurred are file-local there,
// so cross-TU interpreter-call sites (recVTLB-arm64.cpp) route through this.
void recEmitInterpTlbMissCheck();
u32 scaleblockcycles_clear();

// COP2 / VU0 sync emit helper (defined in iCOP2-arm64.cpp).
// interlock=true mirrors x86 COP2_Interlock (CFC2/CTC2/QMFC2/QMTC2 path);
// interlock=false mirrors mVUSyncVU0 / mVUFinishVU0 gating used by LQC2/SQC2
// and the COP2 macro-arithmetic setup. finishFunc is the secondary helper
// to invoke after vu0Sync (typically _vu0FinishMicro or _vu0WaitMicro);
// pass nullptr for "sync only". Emits zero instructions when EEINST analysis
// flags say no sync is needed.
void cop2EmitConditionalSync(bool interlock, void (*finishFunc)());

// COP2 macro-mode microVU0 state setup/teardown (defined in microVU-arm64.cpp).
// Mirrors x86 setupMacroOp/endMacroOp's regAlloc reset, microVU0.cop2 = 1,
// prog.IRinfo.curPC/info[0] init, code = cpuRegs.code, and flag scaffolding.
// Required before invoking any mVU emitter (mVU_LQI/SQI/MFIR/...) from a
// COP2 macro-mode dispatch wrapper. eeinstInfo is g_pCurInstInfo->info (or 0
// when EEINST analysis isn't live for this site).
void mVUmacroSetupCOP2State(int mode, u32 eeinstInfo);
void mVUmacroEndCOP2State();

// COP2 macro-mode setup/teardown wrapper (defined in iCOP2-arm64.cpp).
// Calls cop2EmitConditionalSync, emits status-flag denormalize/normalize when
// mode & 0x10, then runs mVUmacroSetup/EndCOP2State to ready microVU0 for the
// mVU emitter pass. REC_COP2_mVU0_ARM64-style wrappers in iR5900Misc-arm64.cpp
// bracket calls to mVUmacroEmit_<op> with these.
void setupMacroOp_arm64(int mode);
void endMacroOp_arm64(int mode);

// COP2 macro-mode emit adapters (defined in microVU-arm64.cpp). Each runs the
// pass1+pass2 dispatch x86 uses in REC_COP2_mVU0 (microVU_Macro.inl:127-133).
// mode is the same mode bits passed to setupMacroOp_arm64; only bit 0x04
// (requires analysis pass) is observed by the adapter.
void mVUmacroEmit_LQI(int mode);
void mVUmacroEmit_SQI(int mode);
void mVUmacroEmit_LQD(int mode);
void mVUmacroEmit_SQD(int mode);
void mVUmacroEmit_MTIR(int mode);
void mVUmacroEmit_MFIR(int mode);
void mVUmacroEmit_ILWR(int mode);
void mVUmacroEmit_ISWR(int mode);
void mVUmacroEmit_RNEXT(int mode);
void mVUmacroEmit_RGET(int mode);
void mVUmacroEmit_RINIT(int mode);
void mVUmacroEmit_RXOR(int mode);

namespace R5900
{
	namespace Dynarec
	{
		extern void recDoBranchImm(u32 branchTo, u32* jmpSkip, bool isLikely = false, bool swappedDelaySlot = false);
	}
}

////////////////////////////////////////////////////////////////////
// Constant Propagation

#define GPR_IS_CONST1(reg) (EE_CONST_PROP && (reg) < 32 && (g_cpuHasConstReg & (1 << (reg))))
#define GPR_IS_CONST2(reg1, reg2) (EE_CONST_PROP && (g_cpuHasConstReg & (1 << (reg1))) && (g_cpuHasConstReg & (1 << (reg2))))
#define GPR_IS_DIRTY_CONST(reg) (EE_CONST_PROP && (reg) < 32 && (g_cpuHasConstReg & (1 << (reg))) && (!(g_cpuFlushedConstReg & (1 << (reg)))))
#define GPR_SET_CONST(reg) \
	{ \
		if ((reg) < 32) \
		{ \
			g_cpuHasConstReg |= (1 << (reg)); \
			g_cpuFlushedConstReg &= ~(1 << (reg)); \
		} \
	}

#define GPR_DEL_CONST(reg) \
	{ \
		if ((reg) < 32) \
			g_cpuHasConstReg &= ~(1 << (reg)); \
	}

alignas(16) extern GPR_reg64 g_cpuConstRegs[32];
extern u32 g_cpuHasConstReg, g_cpuFlushedConstReg;

// Move guest GPR value to an ARM64 register
void _eeMoveGPRtoR(const vixl::aarch64::Register& to, int fromgpr, bool allow_preload = true);

void _eeFlushAllDirty();
void _eeOnWriteReg(int reg, int signext);

// Totally deletes from const, NEON, and GPR entries
// if flush is 1, also flushes to memory
void _deleteEEreg(int reg, int flush);
void _deleteEEreg128(int reg);

void _flushEEreg(int reg, bool clear = false);

//////////////////////////////////////
// Templates for code recompilation //
//////////////////////////////////////

typedef void (*R5900FNPTR)();
typedef void (*R5900FNPTR_INFO)(int info);

// Memory-based templates — no register allocation, all operands via cpuRegs memory.
void eeRecompileCodeRC0_MEM(R5900FNPTR constcode, R5900FNPTR_INFO constscode, R5900FNPTR_INFO consttcode, R5900FNPTR_INFO noconstcode, int xmminfo);
void eeRecompileCodeRC1_MEM(R5900FNPTR constcode, R5900FNPTR_INFO noconstcode, int xmminfo);
void eeRecompileCodeRC2_MEM(R5900FNPTR constcode, R5900FNPTR_INFO noconstcode, int xmminfo);

#define EERECOMPILE_CODERC0_MEM(fn, xmminfo) \
	void rec##fn(void) \
	{ \
		eeRecompileCodeRC0_MEM(rec##fn##_const, rec##fn##_consts, rec##fn##_constt, rec##fn##_, (xmminfo)); \
	}

#define EERECOMPILE_CODEX_MEM(codename, fn, xmminfo) \
	void rec##fn(void) \
	{ \
		codename(rec##fn##_const, rec##fn##_, (xmminfo)); \
	}

#define FPURECOMPILE_CONSTCODE(fn, xmminfo) \
	void rec##fn(void) \
	{ \
		if (CHECK_FPU_FULL) \
			eeFPURecompileCode(DOUBLE::rec##fn##_xmm, R5900::Interpreter::OpcodeImpl::COP1::fn, xmminfo); \
		else \
			eeFPURecompileCode(rec##fn##_xmm, R5900::Interpreter::OpcodeImpl::COP1::fn, xmminfo); \
	}

int eeRecompileCodeXMM(int xmminfo);
void eeFPURecompileCode(R5900FNPTR_INFO xmmcode, R5900FNPTR fpucode, int xmminfo);
