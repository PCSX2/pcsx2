// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE FPU (COP1) Instruction Codegen — NEON-based
// Transfer ops (MFC1/MTC1/CFC1/CTC1): native with NEON allocation.
// Branch ops (BC1F/BC1T): native, read fprc[31] directly.
// Arithmetic ops: native NEON with PS2 clamping and guard-bit ADD/SUB emulation
//   (fast single-precision path here; the accuracy-mode DOUBLE path is in
//   iFPUd-arm64.cpp, selected when CHECK_FPU_FULL). All ops including DIV/SQRT/
//   RSQRT are native; nothing here defers to the interpreter.

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
// FCR31 block residency (GE-12)
//------------------------------------------------------------------
// The leak/4248 guest-class-0xc design: the C.cond/BC1/CFC1/CTC1/DIV/SQRT
// family accesses fprc[31] through the GPR allocator (ARM64TYPE_FPRC — the
// load/writeback plumbing existed in iCore but had no allocation site).
// C.cond becomes Fcmp+Cset+Bfi on the resident reg, BC1 a Tbnz on it (zero
// loads after a preceding compare), and the DIV/SQRT flag RMWs lose their
// Ldr/Str round-trips. Every iFlushCall seam writes the slot back (FPRC
// lives in a caller-saved-only pool — see _allocArm64GPR), and recCall's
// FLUSH_INTERPRETER fully evicts it (e.g. the recRSQRT_S fallback, whose
// interp body RMWs fprc in C), so fprc[31] memory is canonical wherever C
// code or another block can look.
//
// Returns -1 under CHECK_FPU_FULL: the DOUBLE:: bodies (iFPUd-arm64.cpp)
// RMW fprc[31] memory raw, and these entry points are shared between modes —
// mixing a resident copy with raw-memory RMWs inside one block would desync.
// Under FULL every site below keeps today's raw-memory shape (GE-20 owns
// FULL-mode depth).
static int fpuTryAllocFCR31(int mode)
{
	if (CHECK_FPU_FULL)
		return -1;
	return _allocArm64GPR(ARM64TYPE_FPRC, 31, mode);
}

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
		// FCR31: mask out always-zero bits, set always-one bits (x86 recCFC1
		// shape; the interpreter returns the raw word — JIT-side fixed-bit
		// emulation is an x86-parity divergence by design, pinned by
		// EeRecFpu.CompareThenCfc1SeesFreshConditionBit).
		const int fl = fpuTryAllocFCR31(MODE_READ);
		if (fl >= 0)
			armAsm->And(RWSCRATCH, armWRegister(fl), 0x0083c078);
		else
		{
			armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
			armAsm->And(RWSCRATCH, RWSCRATCH, 0x0083c078);
		}
		armAsm->Orr(RWSCRATCH, RWSCRATCH, 0x01000001);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	else
	{
		// FCR0: read-only revision register
		armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[0]);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	// Deposit last, after any FCR31 slot allocation above: the dest home is
	// resolved at the store so an allocator-resident rt slot can't be evicted
	// between resolve and use.
	_eeStoreGPRDestReg(_Rt_, RXSCRATCH);
}

//------------------------------------------------------------------
// CTC1 — fprc[fs] = rt (write FPU control register)
//------------------------------------------------------------------
void recCTC1()
{
	if (_Fs_ != 31) return;

	const int fl = fpuTryAllocFCR31(MODE_WRITE);
	if (fl >= 0)
	{
		// Full-register write: no read-in, just materialize rt into the slot.
		// _eeMoveGPRtoR reads const/pin/allocator-resident rt directly (the
		// old shape forced rt to memory with _deleteEEreg and reloaded it).
		_eeMoveGPRtoR(armWRegister(fl), _Rt_);
	}
	else
	{
		a64::Register rt = RWSCRATCH;
		if (GPR_IS_CONST1(_Rt_))
			armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].UL[0]);
		else
		{
			_deleteEEreg(_Rt_, 1);
			rt = _eeGetGPRSourceReg(RWSCRATCH, _Rt_);
		}
		armStoreEERegPtr(rt, &fpuRegs.fprc[_Fs_]);
	}
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
	// Deposit last, after the FPR-slot probe above (see recCFC1).
	_eeStoreGPRDestReg(_Rt_, RXSCRATCH);
}

//------------------------------------------------------------------
// MTC1 — fpr[fs] = rt[31:0] (move GPR lower 32 bits to FPR)
//------------------------------------------------------------------
void recMTC1()
{
	a64::Register rt = RWSCRATCH;
	if (GPR_IS_CONST1(_Rt_))
		armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].UL[0]);
	else
	{
		_deleteEEreg(_Rt_, 1);
		rt = _eeGetGPRSourceReg(RWSCRATCH, _Rt_);
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
		armAsm->Fmov(armSRegister(fsreg), rt);
	}
	else
	{
		armStoreEERegPtr(rt, &fpuRegs.fpr[_Fs_].UL);
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

	// GE-12: _eeFlushAllDirty writes back but KEEPS residency, so when an
	// earlier op in the block touched FCR31 (C.cond, DIV, CTC1, ...) the
	// common compare→branch chain pays zero loads here; not-resident
	// allocates with one Ldr, same cost as the old raw load.
	a64::Register flagReg = RWSCRATCH;
	const int fl = fpuTryAllocFCR31(MODE_READ);
	if (fl >= 0)
		flagReg = armWRegister(fl);
	else
		armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);

	// FPUflagC (0x00800000) is a single fixed bit (23), so the Tst+B.cond pair
	// collapses to one test-bit-and-branch (Tbnz/Tbz). The forward branch
	// skips the delay slot on the not-taken edge.
	static_assert(FPUflagC == (1u << 23), "FPUflagC must be a single bit for Tbz/Tbnz");
	s_pBC1Label = new a64::Label();
	if (branchOnTrue)
		armAsm->Tbz(flagReg, 23, s_pBC1Label);   // BC1T: skip taken if C clear
	else
		armAsm->Tbnz(flagReg, 23, s_pBC1Label);  // BC1F: skip taken if C set
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

// PS2 add/sub guard-bit emulation for the single-precision fast path.
//
// A compliant IEEE FPU keeps "guard" bits to the right of the mantissa during
// add/sub; the EE FPU does not. On a subtraction (or mixed-sign add) that
// left-shifts the mantissa, the bits that would have lived in those guard
// positions must read as zero on hardware. This masks the low mantissa bits of
// the smaller-exponent operand by the exponent difference, then does the single
// op. It is the arm64 fast-path port of x86 FPU_ADD_SUB (iFPU.cpp:402). Both
// JITs gate this masking on the same off-by-default CHECK_FPU_GUARDED option
// (x86 FPU_ADD/FPU_SUB, iFPU.cpp) — see the early-out below. It reproduces the
// masking already present in the DOUBLE path's FPU_ADD_SUB (iFPUd-arm64.cpp:200);
// the CHECK_FPU_FULL (double) config dispatches to that path instead and never
// reaches here (Full mode guards unconditionally on both arches).
//
// When |expd - expt| <= 1 the mask clears zero bits, so that (common) case skips
// straight to the plain op. Only |diff| >= 2 masks the smaller-exponent operand;
// the guest fpr slots (EEREC_S/T/ACC) are never mutated, and s/t may alias dst.
// result = issub ? (s - t) : (s + t), written to dst.
//
// The masking is emitted one of two equivalent ways, chosen at build time by
// FPU_GUARD_MASK_STUB (iR5900-arm64.h): inlined here, or via a bl to the shared
// g_fpuGuardMaskStub. Both produce identical results.
//
// ⚠️ GPR scratch contract: this sequence touches only w0/w1 (RWARG1/2 — the
// raw-clobber habit the ARM64TYPE_FPRC pool exclusion in _allocArm64GPR is
// built around), w8 (RWSCRATCH), the non-allocatable x9/x10 value scratches
// and x16/x17 (vixl/addr scratch, via any temp-alloc eviction). None are in
// an int-allocator pool. It must NEVER touch x2-x7/x14/x15: a resident FCR31
// (GE-12, dirty from a C.cond/DIV/CTC1 earlier in the block) lives in that
// pool and survives across ops — clobbering it flips later BC1x/CFC1/flag
// writebacks. This is
// exactly the SotC geometry/movement bug (w2/w3 were used here); pinned by
// EeRecFpu.CompareSurvivesInterposedGuardedAdd{Cfc1,Bc1}.
static void fpuEmitGuardedAddSub(const a64::VRegister& dst,
	const a64::VRegister& s, const a64::VRegister& t, bool issub)
{
	// Guard-bit emulation is opt-in via the off-by-default fpuGuardedAddSub
	// Recompiler option (per-game GameDB clampModes.guardedAddSub, or global
	// INI). Off = a plain single op, matching AetherSX2 / PCSX2 v1.0 and the
	// x86 FPU_ADD/FPU_SUB guard-off branch (iFPU.cpp). This is the
	// default path — the majority of titles never need guard-bit accuracy and
	// skip the ~13-insn common-path cost. Returns before the NEON-temp alloc and
	// GPR-scratch use below so nothing is booked on the fast path. (Full clamp
	// mode is unaffected: it runs the DOUBLE path, which masks guard bits itself
	// — iFPUd-arm64.cpp.)
	if (!CHECK_FPU_GUARDED)
	{
		if (issub)
			armAsm->Fsub(dst, s, t);
		else
			armAsm->Fadd(dst, s, t);
		return;
	}

	// Alloc the NEON temp FIRST, before any raw GPR scratch below goes live.
	// The alloc can emit a victim eviction whose address materialization uses
	// scratch (today only x16/x17 via armMoveAddressToReg); keeping w9/w10
	// dead across it is the defensive invariant, so a future eviction path
	// that reaches for a value scratch can't corrupt our diff/mask. (EEREC_S/
	// T/D/ACC are `needed` for the whole op and RSSCRATCH/RSSCRATCH2 are
	// outside the pool, so the temp can never alias s/t/dst.)
	const int tmp = _allocTempNEONreg();
	const a64::VRegister vtmp = armSRegister(tmp);

	const a64::Register rdiff = a64::w9;  // non-allocatable scratch (iCore mask)
	const a64::Register rmask = a64::w10; // non-allocatable scratch (iCore mask)

	armAsm->Fmov(RWARG1, s);                 // s bits (non-destructive read)
	armAsm->Fmov(RWARG2, t);                 // t bits
	armAsm->Ubfx(rdiff, RWARG1, 23, 8);      // expd
	armAsm->Ubfx(RWSCRATCH, RWARG2, 23, 8);  // expt
	armAsm->Sub(rdiff, rdiff, RWSCRATCH);    // diff = expd - expt (signed)

#if FPU_GUARD_MASK_STUB
	a64::Label slow, done;
	armAsm->Cmp(rdiff, 1);
	armAsm->B(&slow, a64::gt);                // diff >= 2
	armAsm->Cmn(rdiff, 1);
	armAsm->B(&slow, a64::lt);                // diff <= -2
	if (issub)                                // -1 <= diff <= 1: no masking
		armAsm->Fsub(dst, s, t);
	else
		armAsm->Fadd(dst, s, t);
	armAsm->B(&done);

	armAsm->Bind(&slow);
	// RWARG1/RWARG2 still hold s/t bits; the stub masks them in place
	// (in: w0=A, w1=B; out: w0=maskedA, w1=maskedB; clobbers w0/w1/w8/w9/
	// w10/w16/w17 + x30 — nothing in the int-allocator pools).
	armEmitCall(g_fpuGuardMaskStub);
	armAsm->Fmov(dst, RWARG1);
	armAsm->Fmov(vtmp, RWARG2);
	if (issub)
		armAsm->Fsub(dst, dst, vtmp);
	else
		armAsm->Fadd(dst, dst, vtmp);
	armAsm->Bind(&done);
#else
	a64::Label maskT, maskS, plain, done;

	armAsm->Cmp(rdiff, 1);
	armAsm->B(&maskT, a64::gt);               // diff >= 2  -> t smaller, mask t
	armAsm->Cmn(rdiff, 1);
	armAsm->B(&maskS, a64::lt);               // diff <= -2 -> s smaller, mask s
	armAsm->B(&plain);                        // -1 <= diff <= 1 -> mask nothing

	// diff >= 2: mask t's low (diff-1) bits; diff >= 25 keeps only t's sign.
	armAsm->Bind(&maskT);
	{
		a64::Label big, apply;
		armAsm->Cmp(rdiff, 25);
		armAsm->B(&big, a64::ge);
		armAsm->Sub(RWSCRATCH, rdiff, 1);
		armAsm->Mov(rmask, 0xffffffff);
		armAsm->Lsl(rmask, rmask, RWSCRATCH);
		armAsm->And(RWARG2, RWARG2, rmask);
		armAsm->B(&apply);
		armAsm->Bind(&big);
		armAsm->And(RWARG2, RWARG2, 0x80000000);
		armAsm->Bind(&apply);
		armAsm->Fmov(vtmp, RWARG2);
	}
	if (issub)
		armAsm->Fsub(dst, s, vtmp);
	else
		armAsm->Fadd(dst, s, vtmp);
	armAsm->B(&done);

	// diff <= -2: mask s's low (-diff-1) bits; diff <= -25 keeps only s's sign.
	armAsm->Bind(&maskS);
	{
		a64::Label big, apply;
		armAsm->Cmn(rdiff, 25);
		armAsm->B(&big, a64::le);
		armAsm->Neg(RWSCRATCH, rdiff);
		armAsm->Sub(RWSCRATCH, RWSCRATCH, 1);
		armAsm->Mov(rmask, 0xffffffff);
		armAsm->Lsl(rmask, rmask, RWSCRATCH);
		armAsm->And(RWARG1, RWARG1, rmask);
		armAsm->B(&apply);
		armAsm->Bind(&big);
		armAsm->And(RWARG1, RWARG1, 0x80000000);
		armAsm->Bind(&apply);
		armAsm->Fmov(vtmp, RWARG1);
	}
	if (issub)
		armAsm->Fsub(dst, vtmp, t);
	else
		armAsm->Fadd(dst, vtmp, t);
	armAsm->B(&done);

	armAsm->Bind(&plain);
	if (issub)
		armAsm->Fsub(dst, s, t);
	else
		armAsm->Fadd(dst, s, t);

	armAsm->Bind(&done);
#endif
	_freeNEONreg(tmp);
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

// "Full" / DOUBLE-precision emitters (iFPUd-arm64.cpp), selected per-op when
// CHECK_FPU_FULL (GameDB eeClampMode:3). Default config uses the fast paths.
// MOV.S is a raw bit-copy in BOTH modes (x86 DOUBLE::recMOV_S_xmm == the fast
// body), so it has no DOUBLE selection.
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
// GE-20: non-arith DOUBLE bodies.
void recABS_S_xmm(int info);
void recNEG_S_xmm(int info);
void recMAX_S_xmm(int info);
void recMIN_S_xmm(int info);
void recC_EQ_xmm(int info);
void recC_LT_xmm(int info);
void recC_LE_xmm(int info);
void recDIV_S_xmm(int info);
void recSQRT_S_xmm(int info);
void recRSQRT_S_xmm(int info);
} // namespace DOUBLE

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
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recABS_S_xmm : recABS_S_xmm, Interp::ABS_S,
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
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recNEG_S_xmm : recNEG_S_xmm, Interp::NEG_S,
		XMMINFO_WRITED | XMMINFO_READS);
}

//------------------------------------------------------------------
// FPU Comparisons — set/clear fprc[31] condition bit
//------------------------------------------------------------------

void recC_F()
{
	// Always false — clear condition bit
	const int fl = fpuTryAllocFCR31(MODE_READ | MODE_WRITE);
	if (fl >= 0)
	{
		armAsm->And(armWRegister(fl), armWRegister(fl), ~u32(FPUflagC));
	}
	else
	{
		armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
		armAsm->And(RWSCRATCH, RWSCRATCH, ~u32(FPUflagC));
		armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	}
}

// Shared C.EQ/C.LT/C.LE body (GE-12). Operand reads probe the NEON allocator
// read-only — a MODE_WRITE-only slot is authoritative, same rule as recMFC1 —
// with the clamped compare copies always going to scratch (the old shape
// flush-and-FREED both operands and reloaded them from memory). The flag
// write is a single Bfi of the Cset bit into the resident FCR31, replacing
// the Ldr + Mov/Bic/Orr(LSL 23) + Str chain; the Cset result parks in
// RWSCRATCH (w8), not w0 — pool regs can hold live allocator values.
static void recCompareFPRs(a64::Condition cond)
{
	// Allocate FIRST: the alloc can emit an eviction store, which must not
	// land between the Fcmp and the Cset (NZCV) nor split the scratch loads.
	const int fl = fpuTryAllocFCR31(MODE_READ | MODE_WRITE);

	const int fsreg = _checkNEONreg(NEONTYPE_FPREG, _Fs_, 0);
	if (fsreg >= 0)
		armAsm->Fmov(RSSCRATCH, armSRegister(fsreg));
	else
		armLoadEERegPtr(RSSCRATCH, &fpuRegs.fpr[_Fs_].f);
	const int ftreg = (_Ft_ == _Fs_) ? fsreg : _checkNEONreg(NEONTYPE_FPREG, _Ft_, 0);
	if (ftreg >= 0)
		armAsm->Fmov(RSSCRATCH2, armSRegister(ftreg));
	else
		armLoadEERegPtr(RSSCRATCH2, &fpuRegs.fpr[_Ft_].f);
	fpuClampCompareOperand(RSSCRATCH);
	fpuClampCompareOperand(RSSCRATCH2);
	armAsm->Fcmp(RSSCRATCH, RSSCRATCH2);
	armAsm->Cset(RWSCRATCH, cond);

	static_assert(FPUflagC == (1u << 23), "Bfi below inserts the C bit at bit 23");
	if (fl >= 0)
	{
		armAsm->Bfi(armWRegister(fl), RWSCRATCH, 23, 1);
	}
	else
	{
		armLoadEERegPtr(RWARG1, &fpuRegs.fprc[31]);
		armAsm->Bfi(RWARG1, RWSCRATCH, 23, 1);
		armStoreEERegPtr(RWARG1, &fpuRegs.fprc[31]);
	}
}

// GE-20: FULL mode compares as PS2-widened doubles with no operand clamping
// (DOUBLE::recC_*_xmm); the fast path clamps and compares as singles.
void recC_EQ()
{
	if (CHECK_FPU_FULL)
	{
		eeFPURecompileCode(DOUBLE::recC_EQ_xmm, Interp::C_EQ, XMMINFO_READS | XMMINFO_READT);
		return;
	}
	recCompareFPRs(a64::eq);
}
void recC_LT()
{
	if (CHECK_FPU_FULL)
	{
		eeFPURecompileCode(DOUBLE::recC_LT_xmm, Interp::C_LT, XMMINFO_READS | XMMINFO_READT);
		return;
	}
	recCompareFPRs(a64::lt);
}
void recC_LE()
{
	if (CHECK_FPU_FULL)
	{
		eeFPURecompileCode(DOUBLE::recC_LE_xmm, Interp::C_LE, XMMINFO_READS | XMMINFO_READT);
		return;
	}
	recCompareFPRs(a64::le);
}

//------------------------------------------------------------------
// FPU Arithmetic — native with PS2 clamping (no inf/nan)
//------------------------------------------------------------------

static void recADD_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	fpuEmitGuardedAddSub(armSRegister(EEREC_D), s, t, false);
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
	fpuEmitGuardedAddSub(armSRegister(EEREC_D), s, t, true);
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

// Emit: mov x9, #bitmask; msr FPCR, x9 — set FPCR to a compile-time-known
// bitmask. The PS2 FPU divides/sqrts in round-to-nearest while ADD/MUL round
// toward zero, so DIV must briefly swap FPCR to FPUDivFPCR and restore FPUFPCR
// after (mirrors x86 recDIV_S_xmm's xLDMXCSR pair). GE-13: the swap gate
// itself is baked per-compile (a CPU-config change resets the recompilers), so
// the VALUE is equally bake-safe — the old adrp+Ldr+Msr paid an address setup
// plus a dependent load on both sides of every DIV/SQRT.
static void emitLoadFPCR(u64 bitmask)
{
	armAsm->Mov(a64::x9, bitmask);
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
		emitLoadFPCR(EmuConfig.Cpu.FPUDivFPCR.bitmask);

	// GE-13: no operand copies — every read of the raw (pre-clamp) fs/ft bits
	// on the zero-divisor paths happens BEFORE the single EEREC_D write, so D
	// aliasing S or T is safe; the normal path routes through fpuClampInput
	// (scratch copies only when CHECK_FPU_EXTRA_OVERFLOW clamping is on, like
	// the rest of the arithmetic family). The old shape paid two temp allocs
	// plus two Fmovs unconditionally.
	const a64::VRegister fs = armSRegister(EEREC_S);
	const a64::VRegister ft = armSRegister(EEREC_T);

	// GE-12: the three flag RMWs below (clear I|D, then I|SI or D|SD on the
	// zero-divisor paths) hit the resident FCR31 — no Ldr/Str round-trips.
	// The alloc emits any eviction store HERE, before the Fcmp chain (NZCV)
	// and the branch arms (allocator calls inside a runtime-conditional emit
	// region are forbidden — the Twinsanity class). GE-20 gave DIV a DOUBLE::
	// variant, so this body no longer runs under FULL mode; the fl<0 fallback
	// stays as defensive coverage.
	const int fl = fpuTryAllocFCR31(MODE_READ | MODE_WRITE);

	// Clear I|D.
	if (fl >= 0)
		armAsm->Bic(armWRegister(fl), armWRegister(fl), FPUflagI | FPUflagD);
	else
	{
		armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
		armAsm->Bic(RWSCRATCH, RWSCRATCH, FPUflagI | FPUflagD);
		armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	}

	a64::Label normal, setMax, xDiv, end;

	armAsm->Fcmp(ft, 0.0);
	armAsm->B(&normal, a64::ne);   // divisor != 0 → normal divide (unordered too)

	// Divisor is zero: distinguish 0/0 (I|SI) from x/0 (D|SD).
	armAsm->Fcmp(fs, 0.0);
	armAsm->B(&xDiv, a64::ne);
	if (fl >= 0)
		armAsm->Orr(armWRegister(fl), armWRegister(fl), FPUflagI | FPUflagSI);
	else
	{
		armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
		armAsm->Orr(RWSCRATCH, RWSCRATCH, FPUflagI | FPUflagSI);
		armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	}
	armAsm->B(&setMax);
	armAsm->Bind(&xDiv);
	if (fl >= 0)
		armAsm->Orr(armWRegister(fl), armWRegister(fl), FPUflagD | FPUflagSD);
	else
	{
		armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
		armAsm->Orr(RWSCRATCH, RWSCRATCH, FPUflagD | FPUflagSD);
		armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	}

	armAsm->Bind(&setMax);
	// result = sign(Fs ^ Ft) | 0x7f7fffff — raw sign reads precede the D write.
	armAsm->Fmov(RWARG1, fs);
	armAsm->Fmov(RWARG2, ft);
	armAsm->Eor(RWARG1, RWARG1, RWARG2);
	armAsm->And(RWARG1, RWARG1, 0x80000000);
	armAsm->Orr(RWARG1, RWARG1, 0x7f7fffff);
	armAsm->Fmov(armSRegister(EEREC_D), RWARG1);
	armAsm->B(&end);

	armAsm->Bind(&normal);
	{
		const a64::VRegister s = fpuClampInput(fs, RSSCRATCH);
		const a64::VRegister t = fpuClampInput(ft, RSSCRATCH2);
		armAsm->Fdiv(armSRegister(EEREC_D), s, t);
		fpuClampResult(armSRegister(EEREC_D));
	}

	armAsm->Bind(&end);

	if (swapFpcr)
		emitLoadFPCR(EmuConfig.Cpu.FPUFPCR.bitmask);
}

void recDIV_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recDIV_S_xmm : recDIV_S_xmm, Interp::DIV_S,
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
		emitLoadFPCR(EmuConfig.Cpu.FPUDivFPCR.bitmask);

	// PS2 SQRT.S flag handling (interp SQRT_S, FPU.cpp; CHECK_FPU_EXTRA_FLAGS
	// is always on): clear I|D unconditionally, then set I|SI when Ft is a
	// negative *non-zero* value (exp field nonzero AND sign bit set). The
	// ±0 / denormal-as-zero case (exp field == 0) sets no flag. Read the Ft
	// bits before Fabs clobbers EEREC_D, which may alias EEREC_T.
	// GE-12: flag RMW on the resident FCR31; alloc first (eviction stores
	// must precede the RWARG1 clobber and the branch arms). GE-20 gave SQRT
	// a DOUBLE:: variant, so this body no longer runs under FULL mode; the
	// fl<0 fallback stays as defensive coverage.
	const int fl = fpuTryAllocFCR31(MODE_READ | MODE_WRITE);
	armAsm->Fmov(RWARG1, ft);
	const a64::Register flagReg = (fl >= 0) ? armWRegister(fl) : RWSCRATCH;
	if (fl < 0)
		armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Bic(flagReg, flagReg, FPUflagI | FPUflagD);
	a64::Label skipFlag;
	armAsm->Tst(RWARG1, 0x7F800000);                 // exp field
	armAsm->B(&skipFlag, a64::eq);                    // ±0/denorm → no flag
	armAsm->Tbz(RWARG1, 31, &skipFlag);              // positive → no flag
	armAsm->Orr(flagReg, flagReg, FPUflagI | FPUflagSI);
	armAsm->Bind(&skipFlag);
	if (fl < 0)
		armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);

	// PS2 takes sqrt of |ft| → Fabs first.
	armAsm->Fabs(armSRegister(EEREC_D), ft);
	armAsm->Fsqrt(armSRegister(EEREC_D), armSRegister(EEREC_D));
	fpuClampResult(armSRegister(EEREC_D));

	if (swapFpcr)
		emitLoadFPCR(EmuConfig.Cpu.FPUFPCR.bitmask);
}

void recSQRT_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recSQRT_S_xmm : recSQRT_S_xmm, Interp::SQRT_S,
		XMMINFO_WRITED | XMMINFO_READT);
}

// Native RSQRT.S: Fd = Fs / sqrt(|Ft|) implemented in the shape of
// recDIV_S_xmm/recSQRT_S_xmm and matching interp RSQRT_S (FPU.cpp) and
// x86 recRSQRThelper1 (CHECK_FPU_EXTRA_FLAGS always on):
//   - Ft exponent field == 0 (zero, including denormals-as-zero): result is
//     sign(Ft) | 0x7f7fffff (+/-fMax), and set D|SD;
//   - Ft negative (exp nonzero): set I|SI, then divide by sqrt(|Ft|);
//   - Ft positive nonzero: divide by sqrt(Ft).
// I|D are cleared first (sticky SI|SD survive). Like DIV.S/SQRT.S the PS2
// rounds RSQRT to nearest regardless of the configured FCR31 rounding mode, so
// swap host FPCR to the nearest-rounding FPUDivFPCR around the sqrt+div and
// restore FPUFPCR after
static void recRSQRT_S_xmm(int info)
{
	const bool swapFpcr = EmuConfig.Cpu.FPUFPCR.bitmask != EmuConfig.Cpu.FPUDivFPCR.bitmask;
	if (swapFpcr)
		emitLoadFPCR(EmuConfig.Cpu.FPUDivFPCR.bitmask);

	// Copy operands into temps: EEREC_D may alias EEREC_S/EEREC_T, and the
	// zero-divisor path needs the raw Ft sign bit after EEREC_D is written.
	const int dreg = _allocTempNEONreg();   // dividend Fs
	const int treg = _allocTempNEONreg();   // divisor, made |Ft| for the sqrt
	armAsm->Fmov(armSRegister(dreg), armSRegister(EEREC_S));
	armAsm->Fmov(armSRegister(treg), armSRegister(EEREC_T));

	// Raw Ft bits drive the zero/negative branch and the +/-fMax result sign.
	armAsm->Fmov(RWARG1, armSRegister(EEREC_T));

	// Clear I|D (sticky SI|SD are left intact).
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Bic(RWSCRATCH, RWSCRATCH, FPUflagI | FPUflagD);
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);

	a64::Label notZero, doDiv, end;

	// Ft is treated as zero when its exponent field is 0 (denormals included).
	armAsm->Tst(RWARG1, 0x7F800000);
	armAsm->B(&notZero, a64::ne);

	// Zero divisor: set D|SD; result = sign(Ft) | 0x7f7fffff.
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, FPUflagD | FPUflagSD);
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->And(RWARG1, RWARG1, 0x80000000);
	armAsm->Orr(RWARG1, RWARG1, 0x7f7fffff);
	armAsm->Fmov(armSRegister(EEREC_D), RWARG1);
	armAsm->B(&end);

	armAsm->Bind(&notZero);
	// Negative divisor (exp nonzero, sign set): set I|SI. sqrt still takes |Ft|.
	armAsm->Tbz(RWARG1, 31, &doDiv);
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, FPUflagI | FPUflagSI);
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);

	armAsm->Bind(&doDiv);
	armAsm->Fabs(armSRegister(treg), armSRegister(treg)); // |Ft| (no-op if positive)
	if (CHECK_FPU_EXTRA_OVERFLOW)
	{
		fpuClampCompareOperand(armSRegister(dreg));
		fpuClampCompareOperand(armSRegister(treg));
	}
	armAsm->Fsqrt(armSRegister(treg), armSRegister(treg));
	armAsm->Fdiv(armSRegister(EEREC_D), armSRegister(dreg), armSRegister(treg));
	fpuClampResult(armSRegister(EEREC_D));

	armAsm->Bind(&end);

	_freeNEONreg(dreg);
	_freeNEONreg(treg);

	if (swapFpcr)
		emitLoadFPCR(EmuConfig.Cpu.FPUFPCR.bitmask);
}

void recRSQRT_S()
{
	// GE-20: FULL mode gets the x86 DOUBLE body (widen -> sqrt+div in double,
	// zero-divisor max keyed off the DIVIDEND's sign per x86).
	if (CHECK_FPU_FULL)
	{
		eeFPURecompileCode(DOUBLE::recRSQRT_S_xmm, Interp::RSQRT_S,
			XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
		return;
	}
	eeFPURecompileCode(recRSQRT_S_xmm, Interp::RSQRT_S,
		XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
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
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recMAX_S_xmm : recMAX_S_xmm, Interp::MAX_S,
		XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
}

static void recMIN_S_xmm(int info)
{
	armAsm->Fminnm(armSRegister(EEREC_D), armSRegister(EEREC_S), armSRegister(EEREC_T));
}

void recMIN_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recMIN_S_xmm : recMIN_S_xmm, Interp::MIN_S,
		XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
}

//------------------------------------------------------------------
// FPU Accumulator ops — ACC = fs OP ft, then fd = ACC OP fs2
//------------------------------------------------------------------

static void recADDA_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	fpuEmitGuardedAddSub(armSRegister(EEREC_ACC), s, t, false);
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
	fpuEmitGuardedAddSub(armSRegister(EEREC_ACC), s, t, true);
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
	// Intermediate-product clamp gated on CHECK_FPU_EXTRA_OVERFLOW — x86-JIT
	// parity (GE-19; x86 recMADDtemp applies fpuFloat to the product under
	// the same gate). The INTERPRETER always clamps this product (fpuDouble
	// temp, FPU.cpp:271-277), so on the product-overflow + opposite-sign-ACC
	// corner default-mode JIT = ±fMax while interp = 0 — divergence BY
	// DESIGN, shared with x86; games are tuned against the x86 JIT. Pinned
	// by EeRecFpu.MaddSProductOverflowDefaultModeMatchesX86Jit.
	// (x86 also pre-clamps ACC here under the gate; our ACC writers all end
	// in an unconditional fpuClampResult, so a resident ACC can never hold
	// Inf/NaN and that leg is dead on arm64.)
	if (CHECK_FPU_EXTRA_OVERFLOW)
		fpuClampResult(RSSCRATCH);
	fpuEmitGuardedAddSub(armSRegister(EEREC_D), armSRegister(EEREC_ACC), RSSCRATCH, false);
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
	// Extra-gated product clamp — x86-JIT parity, see recMADD_S_xmm (GE-19).
	if (CHECK_FPU_EXTRA_OVERFLOW)
		fpuClampResult(RSSCRATCH);
	fpuEmitGuardedAddSub(armSRegister(EEREC_D), armSRegister(EEREC_ACC), RSSCRATCH, true);
	fpuClampResult(armSRegister(EEREC_D));
}

void recMSUB_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recMSUB_S_xmm : recMSUB_S_xmm, Interp::MSUB_S,
		XMMINFO_WRITED | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);
}

// ACC = ACC + fs * ft. In the default clamp mode the raw fs*ft product is
// added unclamped — interp MADDA_S (FPU.cpp) has no fpuDouble temp for the
// product, and x86 recMADDtemp doesn't clamp it either; both agree that an
// overflowing product overflows the accumulate (→ ±fMax) rather than
// cancelling an opposite-signed ACC. Under CHECK_FPU_EXTRA_OVERFLOW the x86
// JIT clamps the A-form product too (same recMADDtemp serves MADD and MADDA)
// — mirror it (GE-19; x86-JIT parity is the bar, and there interp diverges
// the other way by never clamping the A-form product).
static void recMADDA_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	emitFpuMul(RSSCRATCH, s, t);
	if (CHECK_FPU_EXTRA_OVERFLOW)
		fpuClampResult(RSSCRATCH);
	fpuEmitGuardedAddSub(armSRegister(EEREC_ACC), armSRegister(EEREC_ACC), RSSCRATCH, false);
	fpuClampResult(armSRegister(EEREC_ACC));
}

void recMADDA_S()
{
	eeFPURecompileCode(CHECK_FPU_FULL ? DOUBLE::recMADDA_S_xmm : recMADDA_S_xmm, Interp::MADDA_S,
		XMMINFO_WRITEACC | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);
}

// ACC = ACC - fs * ft. Same as MADDA_S: raw product in the default mode,
// extra-gated product clamp for x86-JIT parity (GE-19).
static void recMSUBA_S_xmm(int info)
{
	const a64::VRegister s = fpuClampInput(armSRegister(EEREC_S), RSSCRATCH);
	const a64::VRegister t = fpuClampInput(armSRegister(EEREC_T), RSSCRATCH2);
	emitFpuMul(RSSCRATCH, s, t);
	if (CHECK_FPU_EXTRA_OVERFLOW)
		fpuClampResult(RSSCRATCH);
	fpuEmitGuardedAddSub(armSRegister(EEREC_ACC), armSRegister(EEREC_ACC), RSSCRATCH, true);
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
