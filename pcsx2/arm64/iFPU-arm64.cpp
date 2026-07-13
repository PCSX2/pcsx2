// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE FPU (COP1) Instruction Codegen — NEON-based
// Transfer ops (MFC1/MTC1/CFC1/CTC1): native with NEON allocation.
// Branch ops (BC1F/BC1T): native, read fprc[31] directly.
// Arithmetic ops: interpreter fallback (PS2 float clamping needed).

#include "arm64/iR5900-arm64.h"

#include <cfloat>

namespace a64 = vixl::aarch64;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {
namespace COP1 {

namespace Interp = R5900::Interpreter::OpcodeImpl::COP1;

#ifdef FORCE_INTERP_FPU
REC_FUNC(CFC1);
REC_FUNC(CTC1);
REC_FUNC(MFC1);
REC_FUNC(MTC1);
REC_SYS(BC1F);
REC_SYS(BC1T);
REC_SYS(BC1FL);
REC_SYS(BC1TL);
#else

#define _Ft_ _Rt_
#define _Fs_ _Rd_
#define _Fd_ _Sa_

#define FPUflagC  0x00800000
#define FPUflagI  0x00020000
#define FPUflagD  0x00010000
#define FPUflagSI 0x00000040
#define FPUflagSD 0x00000020

//------------------------------------------------------------------
// CFC1 — rt = fprc[fs] (read FPU control register)
//------------------------------------------------------------------
void recCFC1()
{
	if (!_Rt_) return;

	_deleteEEreg(_Rt_, 0);
	GPR_DEL_CONST(_Rt_);

	if (_Fs_ >= 16)
	{
		// FCR31: mask out always-zero bits, set always-one bits
		armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
		armAsm->And(RWSCRATCH, RWSCRATCH, 0x0083c078);
		armAsm->Orr(RWSCRATCH, RWSCRATCH, 0x01000001);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	else
	{
		// FCR0: read-only revision register
		armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[0]);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rt_].UD[0]);
}

//------------------------------------------------------------------
// CTC1 — fprc[fs] = rt (write FPU control register)
//------------------------------------------------------------------
void recCTC1()
{
	if (_Fs_ != 31) return;

	if (GPR_IS_CONST1(_Rt_))
		armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].UL[0]);
	else
	{
		_deleteEEreg(_Rt_, 1);
		armLoadEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rt_].UL[0]);
	}
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[_Fs_]);
}

//------------------------------------------------------------------
// MFC1 — rt = sign_extend(fpr[fs]) (move 32-bit float to GPR)
//------------------------------------------------------------------
void recMFC1()
{
	if (!_Rt_) return;

	_deleteEEreg(_Rt_, 0);
	GPR_DEL_CONST(_Rt_);

	// FPR-side allocator coherence: fpr[fs] may be live in NEON (e.g. a
	// preceding ADD_S wrote it, possibly MODE_WRITE-only). If it is already
	// resident, read it straight from the host reg instead of flushing it to
	// memory and reloading (store→load-forward stall on A53).
	// MFC1 doesn't modify fpr[fs], so leave the allocator slot intact. Only
	// the not-resident case falls back to the memory load.
	const int fsreg = _checkNEONreg(NEONTYPE_FPREG, _Fs_, MODE_READ);
	if (fsreg >= 0)
	{
		armAsm->Fmov(RWSCRATCH, armSRegister(fsreg));
	}
	else
	{
		armLoadEERegPtr(RWSCRATCH, &fpuRegs.fpr[_Fs_].UL);
	}
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rt_].UD[0]);
}

//------------------------------------------------------------------
// MTC1 — fpr[fs] = rt[31:0] (move GPR lower 32 bits to FPR)
//------------------------------------------------------------------
void recMTC1()
{
	if (GPR_IS_CONST1(_Rt_))
		armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].UL[0]);
	else
	{
		_deleteEEreg(_Rt_, 1);
		armLoadEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rt_].UL[0]);
	}

	// If fpr[fs] is already resident in NEON, write the new bits straight into
	// the host reg and mark it dirty (MODE_WRITE), keeping it hot for a
	// following FPU op; the block epilogue flushes the S-reg to fpr[fs].f.
	// MTC1 overwrites fpr[fs] wholesale, so any prior MODE_WRITE-only value
	// in the slot is dead and correctly discarded by overwriting lane 0.
	// GE-11: when fs is NOT resident but the backprop analysis says it is
	// used later in the block, ALLOCATE the destination slot (write-only, no
	// memory load) — this is the previously-dead _allocIfUsedFPUtoNEON, the
	// x86 recMTC1 model. Kills the Str+Ldr idiom between MTC1 and the
	// following CVT.S/arith. Unused-dest falls back to the memory store.
	int fsreg = _checkNEONreg(NEONTYPE_FPREG, _Fs_, MODE_WRITE);
	if (fsreg < 0)
		fsreg = _allocIfUsedFPUtoNEON(_Fs_, MODE_WRITE);
	if (fsreg >= 0)
	{
		armAsm->Fmov(armSRegister(fsreg), RWSCRATCH);
	}
	else
	{
		armStoreEERegPtr(RWSCRATCH, &fpuRegs.fpr[_Fs_].UL);
	}
}

//------------------------------------------------------------------
// BC1F / BC1T — branch on FPU condition flag
//------------------------------------------------------------------

// FPU branch setup: flush state and test fprc[31] condition flag.
// Emits conditional forward branch (skip label), matching EE branch pattern.
// bne=false: BC1F (skip if C set), bne=true: BC1T (skip if C clear)
static a64::Label* s_pBC1Label = nullptr;

static void recSetBranchBC1(bool branchOnTrue)
{
	_eeFlushAllDirty();

	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);

	// FPUflagC (0x00800000) is a single fixed bit (23), so the Tst+B.cond pair
	// collapses to one test-bit-and-branch (Tbnz/Tbz). The forward branch
	// skips the delay slot on the not-taken edge.
	static_assert(FPUflagC == (1u << 23), "FPUflagC must be a single bit for Tbz/Tbnz");
	s_pBC1Label = new a64::Label();
	if (branchOnTrue)
		armAsm->Tbz(RWSCRATCH, 23, s_pBC1Label);   // BC1T: skip taken if C clear
	else
		armAsm->Tbnz(RWSCRATCH, 23, s_pBC1Label);  // BC1F: skip taken if C set
}

static void recBindBC1Label()
{
	armAsm->Bind(s_pBC1Label);
	delete s_pBC1Label;
	s_pBC1Label = nullptr;
}

void recBC1F()
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	const bool swap = TrySwapDelaySlot(0, 0, 0, true);
	recSetBranchBC1(false);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(branchTo);

	recBindBC1Label();

	if (!swap)
	{
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(pc);
}

void recBC1T()
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	const bool swap = TrySwapDelaySlot(0, 0, 0, true);
	recSetBranchBC1(true);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(branchTo);

	recBindBC1Label();

	if (!swap)
	{
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(pc);
}

void recBC1FL()
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	recSetBranchBC1(false);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	recBindBC1Label();
	LoadBranchState();
	SetBranchImm(pc);
}

void recBC1TL()
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	recSetBranchBC1(true);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	recBindBC1Label();
	LoadBranchState();
	SetBranchImm(pc);
}

#undef _Ft_
#undef _Fs_
#undef _Fd_

#endif // !FORCE_INTERP_FPU

//------------------------------------------------------------------
// FPU Arithmetic — lightweight interpreter call
// FPU ops only touch fpuRegs memory, not cpuRegs.GPR. EE GPRs are
// in callee-saved NEON registers (q8-q15) that survive C calls.
// Only flush PC/code for exception handling — skip NEON flush.
//------------------------------------------------------------------

static void recFPUCall(void (*func)())
{
	// Flush PC and code (needed if FPU op triggers an exception)
	armAsm->Mov(RWSCRATCH, pc);
	armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.pc));

	armAsm->Mov(RWSCRATCH, cpuRegs.code);
	armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.code));

	// FPU allocator coherence: the interpreter reads fpuRegs.fpr[] and
	// fpuRegs.ACC directly, and those values can live in NEON slots
	// (MODE_WRITE-only) until block-end flush — so writeback every
	// FPREG/FPACC slot before the call. EE GPRs in callee-saved q8-q15
	// survive (FPU interpreter doesn't touch cpuRegs.GPR), so iFlushCall's
	// full eviction is not needed here.
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse &&
			(arm64neon[i].type == NEONTYPE_FPREG || arm64neon[i].type == NEONTYPE_FPACC))
		{
			_freeNEONreg(i);
		}
	}

	armFlushEEClobberedPins(); // lazy-dirty seam: pairs with the reload below
	armEmitCall((void*)func);
	// FPU interpreter fallbacks touch fpuRegs only, never cpuRegs.GPR; restore
	// the caller-saved pins the C call clobbered — the block continues.
	armReloadEEClobberedPins();
}


#define _Ft_ _Rt_
#define _Fs_ _Rd_
#define _Fd_ _Sa_

// PS2 FPU max representable float (no infinity) — exactly ±FLT_MAX
// under IEEE-754 single precision: 0x7F7FFFFF / 0xFF7FFFFF.
// Load-bearing for the EE/mVU dispatcher prologues that park these bit
// patterns in s8/s9 — keep this assert if the type of FLT_MAX ever changes.
static_assert(FLT_MAX == 3.40282346638528859811704183484516925e+38f,
	"FLT_MAX must be the IEEE-754 0x7F7FFFFF bit pattern (PS2 FPU clamp upper bound).");

// Clamp a result in `fpr` to PS2 float range (no inf/nan).
//
// Branchless Fminnm/Fmaxnm: the Number variants are NaN-eating (matching
// x86 MINSS/MAXSS), so NaN routes through Fminnm to +max and Fmaxnm
// passes it through. Both ±Inf get clamped to ±max.
//
// The ±FLT_MAX bounds live in callee-saved s8/s9 — loaded once at JIT
// session entry in `_DynGen_EnterRecompiledCode` and held across every
// armEmitCall via AAPCS64. v8/v9 are excluded from the NEON allocator
// pool so no codegen path can clobber them. 2 host insns per clamp.
//
// NaN sign is not preserved (matches x86 fpuFloat / ClampValues; the
// PS2 FPU has no NaN concept so this is design-correct).
static void fpuClampResult(const a64::VRegister& fpr)
{
	armAsm->Fminnm(fpr, fpr, a64::s8);
	armAsm->Fmaxnm(fpr, fpr, a64::s9);
}

// One-sided positive clamp for results that are statically non-negative
// (ABS.S). Fabs clears the sign bit, so the value is always >= 0 (NaN ->
// +0x7FFFFFFF), which makes the lower Fmaxnm(-FLT_MAX) of fpuClampResult dead.
// Like x86 recABS_S_xmm, only the positive clamp is needed (the ABS result is
// never negative), so only one Fminnm vs 0x7f7fffff is emitted.
// Saves one NEON insn per ABS.S.
static void fpuClampResultPositive(const a64::VRegister& fpr)
{
	armAsm->Fminnm(fpr, fpr, a64::s8);
}

// Sign-preserving operand clamp for FPU comparisons (C.cond.S).
//
// Mirrors the x86 JIT's fpuFloat3 (PMIN.SD vs 0x7f7fffff then PMIN.UD vs
// 0xff7fffff): +NaN->+fMax, -NaN->-fMax, +Inf->+fMax, -Inf->-fMax. The PS2
// FPU has no Inf/NaN concept, so both compare operands must be clamped first;
// a raw Fcmp on an unclamped NaN would go unordered (all-false) where the PS2
// wants an ordered compare against ±FLT_MAX.
//
// Integer SMIN/UMIN preserve the sign bit — unlike fpuClampResult's
// Fminnm/Fmaxnm, which fold every NaN to +fMax and would mis-order -NaN.
// s8/s9 already hold the 0x7f7fffff / 0xff7fffff bit patterns. Only lane 0
// (the scalar S reg) is consumed by the following Fcmp, so the upper V4S
// lanes are don't-care. (Denormal flush-to-zero is intentionally omitted to
// match fpuFloat3; that is a pre-existing shared JIT-vs-interp behavior.)
static void fpuClampCompareOperand(const a64::VRegister& s)
{
	const a64::VRegister v(s.GetCode(), a64::kQRegSize);
	armAsm->Smin(v.V4S(), v.V4S(), a64::VRegister(8, a64::kQRegSize).V4S());
	armAsm->Umin(v.V4S(), v.V4S(), a64::VRegister(9, a64::kQRegSize).V4S());
}

// Source-operand clamp for the FPU arithmetic family, gated on
// CHECK_FPU_EXTRA_OVERFLOW (per-game GameDB clampMode>=2). When enabled, the
// PS2 FPU recs clamp each fpr source to ±fMax *before* the op — matching the
// interpreter (which routes every operand through fpuDouble) and x86
// recCommutativeOp/recMADDtemp (fpuFloat2 under the same gate). This catches
// Inf*0 -> NaN / (+Inf)+(-Inf) -> NaN poison where an fpr was filled with raw
// Inf/NaN bits via MOV.S/LWC1/MTC1; without it the op produces a NaN that
// the result clamp folds to +fMax, diverging from the interpreter's
// clamp-then-compute (e.g. fMax*0 = 0).
//
// Copies into `scratch` rather than mutating the allocator-resident source
// (vs x86's in-place fpuFloat2) so a later read of the same fpr in this block
// still sees the unclamped value. Sign-preserving (fpuClampCompareOperand),
// so -Inf -> -fMax. Only fpr-sourced operands (S/T) need this; ACC is written
// only by always-clamping acc-ops and can never be poisoned. In the default
// config (flag off) this emits nothing and returns the source reg.
static a64::VRegister fpuClampInput(const a64::VRegister& src, const a64::VRegister& scratch)
{
	if (!CHECK_FPU_EXTRA_OVERFLOW)
		return src;
	armAsm->Fmov(scratch, src);
	fpuClampCompareOperand(scratch);
	return scratch;
}

// FpuMulHack (Tales of Destiny Remake gamefix, EmuConfig.Gamefixes.FpuMulHack).
// x86 routes every FPU multiply (MUL/MULA/MADD/MSUB) through FPU_MUL, which —
// when the gamefix is on — patches the single specific product 0.25 * (π/2)
// (0x3e800000 * 0x40490fdb) to 0x3f490fda so the game stops hanging in one
// late-game room. Emit `dst = (hit) ? 0x3f490fda : s*t`; callers clamp/accumulate
// dst as they normally would (the magic value is an ordinary small float, so a
// following fpuClampResult is a no-op). In the default config (gamefix off) this
// is a bare Fmul — zero added cost.
static void emitFpuMul(const a64::VRegister& dst, const a64::VRegister& s, const a64::VRegister& t)
{
	if (!CHECK_FPUMULHACK)
	{
		armAsm->Fmul(dst, s, t);
		return;
	}

	a64::Label noHack, done;
	armAsm->Fmov(RWARG1, s);
	armAsm->Fmov(RWARG2, t);
	armAsm->Mov(RWSCRATCH, 0x3e800000);
	armAsm->Cmp(RWARG1, RWSCRATCH);
	armAsm->B(&noHack, a64::ne);
	armAsm->Mov(RWSCRATCH, 0x40490fdb);
	armAsm->Cmp(RWARG2, RWSCRATCH);
	armAsm->B(&noHack, a64::ne);
	armAsm->Mov(RWSCRATCH, 0x3f490fda);
	armAsm->Fmov(dst, RWSCRATCH);
	armAsm->B(&done);
	armAsm->Bind(&noHack);
	armAsm->Fmul(dst, s, t);
	armAsm->Bind(&done);
}

//------------------------------------------------------------------
// Simple FPU ops — no clamping needed
//------------------------------------------------------------------

static void recMOV_S_xmm(int info)
{
	// MOV.S is a raw bit-copy (PS2 FPR[fd] = FPR[fs]); no clamp/NaN logic.
	// Skip the emit entirely when fd and fs alias the same host reg (guest
	// fs==fd): the allocator hands back EEREC_D==EEREC_S and the Fmov would be
	// an identity self-move.
	if (EEREC_D != EEREC_S)
		armAsm->Fmov(armSRegister(EEREC_D), armSRegister(EEREC_S));
}

void recMOV_S()
{
	eeFPURecompileCode(recMOV_S_xmm, Interp::MOV_S,
		XMMINFO_WRITED | XMMINFO_READS);
}

static void recABS_S_xmm(int info)
{
	armAsm->Fabs(armSRegister(EEREC_D), armSRegister(EEREC_S));
	// ABS output is always non-negative -> one-sided positive clamp.
	fpuClampResultPositive(armSRegister(EEREC_D));
}

void recABS_S()
{
	eeFPURecompileCode(recABS_S_xmm, Interp::ABS_S,
		XMMINFO_WRITED | XMMINFO_READS);
}

static void recNEG_S_xmm(int info)
{
	armAsm->Fneg(armSRegister(EEREC_D), armSRegister(EEREC_S));
	// Sign-preserving clamp: NEG.S of a poisoned +NaN/-Inf must keep the sign
	// (-> -fMax), but fpuClampResult (Fminnm/Fmaxnm) folds every NaN to +fMax.
	// Mirrors x86's switch from ClampValues to fpuFloat3 (upstream 4ffbe0bbf).
	fpuClampCompareOperand(armSRegister(EEREC_D));
}

void recNEG_S()
{
	eeFPURecompileCode(recNEG_S_xmm, Interp::NEG_S,
		XMMINFO_WRITED | XMMINFO_READS);
}

//------------------------------------------------------------------
// FPU Comparisons — set/clear fprc[31] condition bit
//------------------------------------------------------------------

void recC_F()
{
	// Always false — clear condition bit
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Mov(RWARG1, FPUflagC); armAsm->Bic(RWSCRATCH, RWSCRATCH, RWARG1);
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
}

void recC_EQ()
{
	_deleteFPtoNEONreg(_Fs_, DELETE_REG_FLUSH_AND_FREE);
	_deleteFPtoNEONreg(_Ft_, DELETE_REG_FLUSH_AND_FREE);
	armLoadEERegPtr(RSSCRATCH, &fpuRegs.fpr[_Fs_].f);
	armLoadEERegPtr(RSSCRATCH2, &fpuRegs.fpr[_Ft_].f);
	fpuClampCompareOperand(RSSCRATCH);
	fpuClampCompareOperand(RSSCRATCH2);
	armAsm->Fcmp(RSSCRATCH, RSSCRATCH2);
	armAsm->Cset(a64::w0, a64::eq); // fully defines w0 — no pre-zero needed (GE-03)
	// Set or clear FPUflagC based on result (w0 holds cset result, don't clobber)
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Mov(RWARG2, FPUflagC); armAsm->Bic(RWSCRATCH, RWSCRATCH, RWARG2);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, a64::Operand(a64::w0, a64::LSL, 23));
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
}

void recC_LT()
{
	_deleteFPtoNEONreg(_Fs_, DELETE_REG_FLUSH_AND_FREE);
	_deleteFPtoNEONreg(_Ft_, DELETE_REG_FLUSH_AND_FREE);
	armLoadEERegPtr(RSSCRATCH, &fpuRegs.fpr[_Fs_].f);
	armLoadEERegPtr(RSSCRATCH2, &fpuRegs.fpr[_Ft_].f);
	fpuClampCompareOperand(RSSCRATCH);
	fpuClampCompareOperand(RSSCRATCH2);
	armAsm->Fcmp(RSSCRATCH, RSSCRATCH2);
	armAsm->Cset(a64::w0, a64::lt); // fully defines w0 — no pre-zero needed (GE-03)
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Mov(RWARG2, FPUflagC); armAsm->Bic(RWSCRATCH, RWSCRATCH, RWARG2);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, a64::Operand(a64::w0, a64::LSL, 23));
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
}

void recC_LE()
{
	_deleteFPtoNEONreg(_Fs_, DELETE_REG_FLUSH_AND_FREE);
	_deleteFPtoNEONreg(_Ft_, DELETE_REG_FLUSH_AND_FREE);
	armLoadEERegPtr(RSSCRATCH, &fpuRegs.fpr[_Fs_].f);
	armLoadEERegPtr(RSSCRATCH2, &fpuRegs.fpr[_Ft_].f);
	fpuClampCompareOperand(RSSCRATCH);
	fpuClampCompareOperand(RSSCRATCH2);
	armAsm->Fcmp(RSSCRATCH, RSSCRATCH2);
	armAsm->Cset(a64::w0, a64::le); // fully defines w0 — no pre-zero needed (GE-03)
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Mov(RWARG2, FPUflagC); armAsm->Bic(RWSCRATCH, RWSCRATCH, RWARG2);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, a64::Operand(a64::w0, a64::LSL, 23));
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
}

//------------------------------------------------------------------
// FPU Arithmetic — native with PS2 clamping (no inf/nan)
//------------------------------------------------------------------

// "Full" / DOUBLE-precision emitters (iFPUd-arm64.cpp), selected per-op when
// CHECK_FPU_FULL (GameDB eeClampMode:3). Default config uses the fast paths below.
namespace DOUBLE {
void recADD_S_xmm(int info);
void recSUB_S_xmm(int info);
void recADDA_S_xmm(int info);
void recSUBA_S_xmm(int info);
void recMUL_S_xmm(int info);
void recMULA_S_xmm(int info);
void recMADD_S_xmm(int info);
void recMSUB_S_xmm(int info);
void recMADDA_S_xmm(int info);
void recMSUBA_S_xmm(int info);
} // namespace DOUBLE

static void recADD_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	armAsm->Fadd(armSRegister(EEREC_D), s, t);
	fpuClampResult(armSRegister(EEREC_D));
}

void recADD_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recADD_S_xmm : recADD_S_xmm, Interp::ADD_S,
		XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
}

static void recSUB_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	armAsm->Fsub(armSRegister(EEREC_D), s, t);
	fpuClampResult(armSRegister(EEREC_D));
}

void recSUB_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recSUB_S_xmm : recSUB_S_xmm, Interp::SUB_S,
		XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
}

static void recMUL_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	emitFpuMul(armSRegister(EEREC_D), s, t);
	fpuClampResult(armSRegister(EEREC_D));
}

void recMUL_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recMUL_S_xmm : recMUL_S_xmm, Interp::MUL_S,
		XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
}

// Emit: ldr x9, [addr]; msr FPCR, x9 — load a 64-bit FPCR bitmask from `addr`.
// The PS2 FPU divides/sqrts in round-to-nearest while ADD/MUL round toward zero,
// so DIV must briefly swap FPCR to FPUDivFPCR and restore FPUFPCR after (mirrors
// x86 recDIV_S_xmm's xLDMXCSR pair). Uses x8/x9 as scratch.
static void emitLoadFPCR(const void* addr)
{
	armMoveAddressToReg(a64::x8, const_cast<void*>(addr));
	armAsm->Ldr(a64::x9, a64::MemOperand(a64::x8));
	armAsm->Msr(a64::FPCR, a64::x9);
}

// Native DIV.S — port of x86 recDIVhelper1 (CHECK_FPU_EXTRA_FLAGS is always on)
// + the recDIV_S_xmm FPCR round-mode swap. Matches interp DIV_S /
// checkDivideByZero:
//   - divisor == 0 (exp field 0; FZ in FPCR flushes denormals so the float
//     compare catches them too): result = sign(Fs^Ft) | 0x7f7fffff (±fMax),
//     and set I|SI for 0/0, D|SD for x/0;
//   - otherwise native Fdiv (round-to-nearest) then ±fMax result clamp.
// I|D are cleared first to match the divide-by-zero result-shape and sticky
// flag semantics of the interpreter.
static void recDIV_S_xmm(int info)
{
	const bool swapFpcr = EmuConfig.Cpu.FPUFPCR.bitmask != EmuConfig.Cpu.FPUDivFPCR.bitmask;
	if (swapFpcr)
		emitLoadFPCR(&EmuConfig.Cpu.FPUDivFPCR.bitmask);

	// Copy both operands into temps: EEREC_D may alias EEREC_S/EEREC_T, and the
	// div-by-zero path needs the raw (pre-clamp) dividend/divisor sign bits.
	const int dreg = _allocTempNEONreg();
	const int treg = _allocTempNEONreg();
	armAsm->Fmov(armSRegister(dreg), armSRegister(EEREC_S));
	armAsm->Fmov(armSRegister(treg), armSRegister(EEREC_T));

	// Clear I|D.
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Bic(RWSCRATCH, RWSCRATCH, FPUflagI | FPUflagD);
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);

	a64::Label normal, setMax, xDiv, end;

	armAsm->Fcmp(armSRegister(treg), 0.0);
	armAsm->B(&normal, a64::ne);   // divisor != 0 → normal divide (unordered too)

	// Divisor is zero: distinguish 0/0 (I|SI) from x/0 (D|SD).
	armAsm->Fcmp(armSRegister(dreg), 0.0);
	armAsm->B(&xDiv, a64::ne);
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, FPUflagI | FPUflagSI);
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->B(&setMax);
	armAsm->Bind(&xDiv);
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, FPUflagD | FPUflagSD);
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);

	armAsm->Bind(&setMax);
	// result = sign(Fs ^ Ft) | 0x7f7fffff
	armAsm->Fmov(RWARG1, armSRegister(dreg));
	armAsm->Fmov(RWARG2, armSRegister(treg));
	armAsm->Eor(RWARG1, RWARG1, RWARG2);
	armAsm->And(RWARG1, RWARG1, 0x80000000);
	armAsm->Orr(RWARG1, RWARG1, 0x7f7fffff);
	armAsm->Fmov(armSRegister(EEREC_D), RWARG1);
	armAsm->B(&end);

	armAsm->Bind(&normal);
	if (CHECK_FPU_EXTRA_OVERFLOW)
	{
		fpuClampCompareOperand(armSRegister(dreg));
		fpuClampCompareOperand(armSRegister(treg));
	}
	armAsm->Fdiv(armSRegister(EEREC_D), armSRegister(dreg), armSRegister(treg));
	fpuClampResult(armSRegister(EEREC_D));

	armAsm->Bind(&end);

	_freeNEONreg(dreg);
	_freeNEONreg(treg);

	if (swapFpcr)
		emitLoadFPCR(&EmuConfig.Cpu.FPUFPCR.bitmask);
}

void recDIV_S()
{
	eeFPURecompileCode(recDIV_S_xmm, Interp::DIV_S,
		XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
}

static void recSQRT_S_xmm(int info)
{
	const a64::VRegister ft = armSRegister(EEREC_T);

	// PS2 SQRT.S rounds to nearest regardless of the configured FCR31 rounding
	// mode (same hardware quirk as DIV.S — see recDIV_S_xmm + the emitLoadFPCR
	// comment). The EE rec runs under host FPCR = FPUFPCR (ChopZero by default),
	// so swap to the nearest-rounding FPUDivFPCR around the Fsqrt and restore
	// FPUFPCR after. Mirrors x86 recSQRT_S_xmm (iFPU.cpp:1745-1782).
	const bool swapFpcr = EmuConfig.Cpu.FPUFPCR.bitmask != EmuConfig.Cpu.FPUDivFPCR.bitmask;
	if (swapFpcr)
		emitLoadFPCR(&EmuConfig.Cpu.FPUDivFPCR.bitmask);

	// PS2 SQRT.S flag handling (interp SQRT_S, FPU.cpp; CHECK_FPU_EXTRA_FLAGS
	// is always on): clear I|D unconditionally, then set I|SI when Ft is a
	// negative *non-zero* value (exp field nonzero AND sign bit set). The
	// ±0 / denormal-as-zero case (exp field == 0) sets no flag. Read the Ft
	// bits before Fabs clobbers EEREC_D, which may alias EEREC_T.
	armAsm->Fmov(RWARG1, ft);
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Bic(RWSCRATCH, RWSCRATCH, FPUflagI | FPUflagD);
	a64::Label skipFlag;
	armAsm->Tst(RWARG1, 0x7F800000);                 // exp field
	armAsm->B(&skipFlag, a64::eq);                    // ±0/denorm → no flag
	armAsm->Tbz(RWARG1, 31, &skipFlag);              // positive → no flag
	armAsm->Orr(RWSCRATCH, RWSCRATCH, FPUflagI | FPUflagSI);
	armAsm->Bind(&skipFlag);
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);

	// PS2 takes sqrt of |ft| → Fabs first.
	armAsm->Fabs(armSRegister(EEREC_D), ft);
	armAsm->Fsqrt(armSRegister(EEREC_D), armSRegister(EEREC_D));
	fpuClampResult(armSRegister(EEREC_D));

	if (swapFpcr)
		emitLoadFPCR(&EmuConfig.Cpu.FPUFPCR.bitmask);
}

void recSQRT_S()
{
	eeFPURecompileCode(recSQRT_S_xmm, Interp::SQRT_S,
		XMMINFO_WRITED | XMMINFO_READT);
}

void recRSQRT_S()
{
	// Defer to the interpreter: interp RSQRT_S (FPU.cpp) sets D|SD when Ft
	// (divisor) is zero and I|SI when Ft is negative, and its Ft==0 branch
	// returns ±posFmax keyed off the Ft sign (not Fs) — neither the sticky
	// flags nor that result shape are reproducible by a raw Fdiv. RSQRT is
	// rare, so the interpreter call is the lowest-risk match and keeps emitted
	// code small. Same shape as recDIV_S.
	recFPUCall(Interp::RSQRT_S);
}

// PS2 FPU has no NaN concept — match x86 MAXSS/MINSS NaN-eating semantics
// with Fmaxnm/Fminnm (Fmax/Fmin IEEE-propagate NaN, same trap as mVUclamp1).
// No clamp needed: MAX/MIN cannot widen finite inputs.
static void recMAX_S_xmm(int info)
{
	armAsm->Fmaxnm(armSRegister(EEREC_D), armSRegister(EEREC_S), armSRegister(EEREC_T));
}

void recMAX_S()
{
	eeFPURecompileCode(recMAX_S_xmm, Interp::MAX_S,
		XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
}

static void recMIN_S_xmm(int info)
{
	armAsm->Fminnm(armSRegister(EEREC_D), armSRegister(EEREC_S), armSRegister(EEREC_T));
}

void recMIN_S()
{
	eeFPURecompileCode(recMIN_S_xmm, Interp::MIN_S,
		XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
}

//------------------------------------------------------------------
// FPU Accumulator ops — ACC = fs OP ft, then fd = ACC OP fs2
//------------------------------------------------------------------

static void recADDA_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	armAsm->Fadd(armSRegister(EEREC_ACC), s, t);
	fpuClampResult(armSRegister(EEREC_ACC));
}

void recADDA_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recADDA_S_xmm : recADDA_S_xmm, Interp::ADDA_S,
		XMMINFO_WRITEACC | XMMINFO_READS | XMMINFO_READT);
}

static void recSUBA_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	armAsm->Fsub(armSRegister(EEREC_ACC), s, t);
	fpuClampResult(armSRegister(EEREC_ACC));
}

void recSUBA_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recSUBA_S_xmm : recSUBA_S_xmm, Interp::SUBA_S,
		XMMINFO_WRITEACC | XMMINFO_READS | XMMINFO_READT);
}

static void recMULA_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	emitFpuMul(armSRegister(EEREC_ACC), s, t);
	fpuClampResult(armSRegister(EEREC_ACC));
}

void recMULA_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recMULA_S_xmm : recMULA_S_xmm, Interp::MULA_S,
		XMMINFO_WRITEACC | XMMINFO_READS | XMMINFO_READT);
}

// fd = ACC + fs * ft. PS2 ISA mandates two separate roundings (mul then
// add), so don't fuse into FMA. RSSCRATCH (s30) is the non-pool scratch
// for the intermediate product — leaves EEREC_S/T allocator-resident.
static void recMADD_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	emitFpuMul(RSSCRATCH, s, t);
	fpuClampResult(RSSCRATCH);
	armAsm->Fadd(armSRegister(EEREC_D), armSRegister(EEREC_ACC), RSSCRATCH);
	fpuClampResult(armSRegister(EEREC_D));
}

void recMADD_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recMADD_S_xmm : recMADD_S_xmm, Interp::MADD_S,
		XMMINFO_WRITED | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);
}

// fd = ACC - fs * ft
static void recMSUB_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	emitFpuMul(RSSCRATCH, s, t);
	fpuClampResult(RSSCRATCH);
	armAsm->Fsub(armSRegister(EEREC_D), armSRegister(EEREC_ACC), RSSCRATCH);
	fpuClampResult(armSRegister(EEREC_D));
}

void recMSUB_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recMSUB_S_xmm : recMSUB_S_xmm, Interp::MSUB_S,
		XMMINFO_WRITED | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);
}

// ACC = ACC + fs * ft. Unlike MADD_S, interp MADDA_S (FPU.cpp) adds the raw
// fs*ft product without routing it through fpuDouble — only the final ACC is
// overflow-clamped. So do NOT clamp the intermediate product here, else an
// overflowing product clamped to +-fMax cancels an opposite-signed ACC instead
// of overflowing the accumulate.
static void recMADDA_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	emitFpuMul(RSSCRATCH, s, t);
	armAsm->Fadd(armSRegister(EEREC_ACC), armSRegister(EEREC_ACC), RSSCRATCH);
	fpuClampResult(armSRegister(EEREC_ACC));
}

void recMADDA_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recMADDA_S_xmm : recMADDA_S_xmm, Interp::MADDA_S,
		XMMINFO_WRITEACC | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);
}

// ACC = ACC - fs * ft. Same as MADDA_S: interp MSUBA_S does not clamp the
// intermediate product, only the final ACC.
static void recMSUBA_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	emitFpuMul(RSSCRATCH, s, t);
	armAsm->Fsub(armSRegister(EEREC_ACC), armSRegister(EEREC_ACC), RSSCRATCH);
	fpuClampResult(armSRegister(EEREC_ACC));
}

void recMSUBA_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recMSUBA_S_xmm : recMSUBA_S_xmm, Interp::MSUBA_S,
		XMMINFO_WRITEACC | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);
}

// CVT.S: fd = (float)int_bits_of(fpr[fs])
// Single NEON-scalar SCVTF Sd,Sn — the int32 bits are already in the V file;
// the old Fmov-to-GPR bounce cost an extra insn + cross-file hazard (GE-02).
static void recCVT_S_xmm(int info)
{
	armAsm->Scvtf(armSRegister(EEREC_D), armSRegister(EEREC_S));
}

void recCVT_S()
{
	eeFPURecompileCode(recCVT_S_xmm, Interp::CVT_S,
		XMMINFO_WRITED | XMMINFO_READS);
}

// CVT.W: fd_bits = (int32_t)fpr[fs] truncating toward zero.
// PS2 clamps overflow to INT32_MAX/MIN — ARM64 Fcvtzs saturates by default,
// matching interp for the finite-overflow and ±Inf cases. The one divergence
// is NaN: ARM Fcvtzs yields 0, but the PS2 (interp CVT_W, FPU.cpp) saturates
// NaN by sign — positive NaN → 0x7fffffff, negative NaN → 0x80000000. Fix up
// the NaN case only (cold branch over the source-sign select).
static void recCVT_W_xmm(int info)
{
	const a64::VRegister fs = armSRegister(EEREC_S);
	armAsm->Fcvtzs(RWSCRATCH, fs);
	a64::Label done;
	armAsm->Fcmp(fs, fs);                         // NaN → unordered (V set)
	armAsm->B(&done, a64::vc);                    // ordered → keep Fcvtzs result
	armAsm->Fmov(RWARG1, fs);                     // NaN: broadcast source sign
	armAsm->Asr(RWARG1, RWARG1, 31);             // 0 if +, 0xFFFFFFFF if -
	armAsm->Eor(RWSCRATCH, RWARG1, 0x7fffffff);  // + → 0x7fffffff, − → 0x80000000
	armAsm->Bind(&done);
	armAsm->Fmov(armSRegister(EEREC_D), RWSCRATCH);
}

void recCVT_W()
{
	eeFPURecompileCode(recCVT_W_xmm, Interp::CVT_W,
		XMMINFO_WRITED | XMMINFO_READS);
}

#undef _Ft_
#undef _Fs_
#undef _Fd_

} // namespace COP1
} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
