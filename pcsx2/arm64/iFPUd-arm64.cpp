// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE FPU (COP1) — "Full" / DOUBLE-precision codegen.
//
// This is the arm64 port of pcsx2/x86/iFPUd.cpp: the PS2-accurate FPU that
// widens each single to IEEE double, performs the op in double, then narrows
// back to a PS2 single with the hardware's overflow/underflow/clamp semantics.
// It is selected only when CHECK_FPU_FULL (EmuConfig.Cpu.Recompiler.fpuFullMode,
// the GameDB `eeClampMode:3` path — FFX, Max Payne, Dark Cloud 2, Klonoa 2 …).
// Default config runs the single-precision fast path in iFPU-arm64.cpp.
//
// The algorithm is translated from the x86 semantics; the codegen follows the
// iFPU-arm64.cpp idioms (scalar Fcvt, GPR bit-twiddle via Fmov, the
// armLoadEERegPtr fprc[31]/ACCflag accessors). The shared interpreter
// (FPU.cpp fpuDouble) has no double path, so this codegen has no interpreter
// counterpart.

#include "arm64/iR5900-arm64.h"

#include <cfloat>

namespace a64 = vixl::aarch64;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {
namespace COP1 {
namespace DOUBLE {

#define _Ft_ _Rt_
#define _Fs_ _Rd_
#define _Fd_ _Sa_

#define FPUflagO  0x00008000
#define FPUflagU  0x00004000
#define FPUflagSO 0x00000010
#define FPUflagSU 0x00000008
#define FPUflagI  0x00020000
#define FPUflagD  0x00010000
#define FPUflagSI 0x00000040
#define FPUflagSD 0x00000020

// ---- PS2 single -> IEEE double --------------------------------------------
//
// A PS2 single with exponent field 0xff is a *normal* large number (1.m * 2^128),
// but IEEE reads exp 0xff as Inf/NaN — so a plain cvtss2sd would corrupt it.
// For those (and only those) lower the exponent by one in the single domain,
// widen exactly, then raise the exponent by one in the double domain. Mirrors
// x86 ToDouble (xPSUB.D one_exp / xCVTSS2SD / xPADD.Q dbl_one_exp).
//
// Operates in place on temp NEON reg `idx`: reads the S lane, writes the D lane.
static void ToDouble(int idx)
{
	const a64::VRegister s = armSRegister(idx);
	const a64::VRegister d = armDRegister(idx);

	a64::Label simple, done;
	armAsm->Fmov(RWSCRATCH, s);
	armAsm->And(RWARG1, RWSCRATCH, 0x7f800000);
	armAsm->Cmp(RWARG1, 0x7f800000);
	armAsm->B(&simple, a64::ne);

	// Complex: exp field == 0xff (Inf/NaN to IEEE, finite to PS2).
	armAsm->Sub(RWSCRATCH, RWSCRATCH, 0x00800000);   // lower exponent by one (single)
	armAsm->Fmov(s, RWSCRATCH);
	armAsm->Fcvt(d, s);                              // cvtss2sd (now finite)
	armAsm->Fmov(RXSCRATCH, d);
	armAsm->Mov(RXARG1, static_cast<u64>(1) << 52);  // dbl_one_exp
	armAsm->Add(RXSCRATCH, RXSCRATCH, RXARG1);       // raise exponent by one (double)
	armAsm->Fmov(d, RXSCRATCH);
	armAsm->B(&done);

	armAsm->Bind(&simple);
	armAsm->Fcvt(d, s);

	armAsm->Bind(&done);
}

// ---- IEEE double -> PS2 single (full overflow/underflow/flag handling) -----
//
// Port of x86 ToPS2FPU_Full. `idx` holds the double result (D lane); `absidx`
// is a scratch NEON reg. On return the PS2 single is in `idx`'s S lane.
// Comparisons are done on the integer bit pattern of |x| — valid because every
// operand here is a finite double, so unsigned-integer order == magnitude order
// (sidesteps NaN/unordered, which never reach this point for ADD/SUB/MUL).
static void ToPS2FPU_Full(int idx, bool flags, int /*absidx*/, bool acc, bool addsub)
{
	const a64::VRegister s = armSRegister(idx);
	const a64::VRegister d = armDRegister(idx);

	if (flags)
	{
		armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
		armAsm->Bic(RWSCRATCH, RWSCRATCH, FPUflagO | FPUflagU);
		armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
		if (acc)
		{
			armLoadEERegPtr(RWSCRATCH, &fpuRegs.ACCflag);
			armAsm->Bic(RWSCRATCH, RWSCRATCH, 1);
			armStoreEERegPtr(RWSCRATCH, &fpuRegs.ACCflag);
		}
	}

	// abs = |reg| (integer, low 63 bits)
	armAsm->Fmov(RXSCRATCH, d);
	armAsm->And(RXARG1, RXSCRATCH, 0x7fffffffffffffffULL);

	a64::Label toComplex, toUnderflow, toOverflow, end;

	armAsm->Mov(RXARG2, static_cast<u64>(1151) << 52);   // dbl_cvt_overflow (2^128)
	armAsm->Cmp(RXARG1, RXARG2);
	armAsm->B(&toComplex, a64::hs);

	armAsm->Mov(RXARG2, static_cast<u64>(897) << 52);    // dbl_underflow (2^-126)
	armAsm->Cmp(RXARG1, RXARG2);
	armAsm->B(&toUnderflow, a64::lo);

	// In-range: plain narrow.
	armAsm->Fcvt(s, d);
	armAsm->B(&end);

	armAsm->Bind(&toComplex);
	armAsm->Mov(RXARG2, static_cast<u64>(1152) << 52);   // dbl_ps2_overflow (2^129)
	armAsm->Cmp(RXARG1, RXARG2);
	armAsm->B(&toOverflow, a64::hs);

	// Large but PS2-representable (exp-0xff range): lower double exp, narrow,
	// raise single exp — the inverse of ToDouble's complex path.
	armAsm->Mov(RXARG2, static_cast<u64>(1) << 52);
	armAsm->Sub(RXSCRATCH, RXSCRATCH, RXARG2);
	armAsm->Fmov(d, RXSCRATCH);
	armAsm->Fcvt(s, d);
	armAsm->Fmov(RWSCRATCH, s);
	armAsm->Add(RWSCRATCH, RWSCRATCH, 0x00800000);
	armAsm->Fmov(s, RWSCRATCH);
	armAsm->B(&end);

	armAsm->Bind(&toOverflow);
	// Beyond PS2 range: narrow then clamp to +/-max (keep sign, set all other bits).
	armAsm->Fcvt(s, d);
	armAsm->Fmov(RWSCRATCH, s);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, 0x7fffffff);
	armAsm->Fmov(s, RWSCRATCH);
	if (flags)
	{
		armLoadEERegPtr(RWARG1, &fpuRegs.fprc[31]);
		armAsm->Orr(RWARG1, RWARG1, FPUflagO | FPUflagSO);
		armStoreEERegPtr(RWARG1, &fpuRegs.fprc[31]);
		if (acc)
		{
			armLoadEERegPtr(RWARG1, &fpuRegs.ACCflag);
			armAsm->Orr(RWARG1, RWARG1, 1);
			armStoreEERegPtr(RWARG1, &fpuRegs.ACCflag);
		}
	}
	armAsm->B(&end);

	armAsm->Bind(&toUnderflow);
	a64::Label uDone;
	if (flags)
	{
		// Set U|SU unless the result is exactly +/-0.
		armAsm->Fmov(RXSCRATCH, d);
		armAsm->And(RXARG1, RXSCRATCH, 0x7fffffffffffffffULL);
		a64::Label isZero;
		armAsm->Cbz(RXARG1, &isZero);
		armLoadEERegPtr(RWARG2, &fpuRegs.fprc[31]);
		armAsm->Orr(RWARG2, RWARG2, FPUflagU | FPUflagSU);
		armStoreEERegPtr(RWARG2, &fpuRegs.fprc[31]);
		if (addsub)
		{
			// ADD/SUB leave the (post-normalization) mantissa bits in place;
			// reconstruct a PS2 denormal single: bits[22:0] = dbl_mant[51:29],
			// bit31 = sign, exp = 0. (x86 PSLL.Q 12 / PSRL.Q 41 / sign<<31 / POR.)
			armAsm->Fmov(RXSCRATCH, d);
			armAsm->Lsl(RXARG1, RXSCRATCH, 12);
			armAsm->Lsr(RXARG1, RXARG1, 41);
			armAsm->Lsr(RXARG2, RXSCRATCH, 63);
			armAsm->Lsl(RXARG2, RXARG2, 31);
			armAsm->Orr(RWSCRATCH, RWARG1, RWARG2);
			armAsm->Fmov(s, RWSCRATCH);
			armAsm->B(&uDone);
		}
		armAsm->Bind(&isZero);
	}
	// Flush to +/-0 (keep sign).
	armAsm->Fcvt(s, d);
	armAsm->Fmov(RWSCRATCH, s);
	armAsm->And(RWSCRATCH, RWSCRATCH, 0x80000000);
	armAsm->Fmov(s, RWSCRATCH);

	armAsm->Bind(&uDone);
	armAsm->Bind(&end);
}

// ---- PS2 add/sub guard-bit emulation --------------------------------------
//
// The EE FPU has no guard bits to the right of the mantissa; subtraction (and
// add of mixed signs) can shift the mantissa left and expose what would have
// been guard bits. This masks the low mantissa bits of the smaller operand by
// the exponent difference so they read as zero. Port of x86 FPU_ADD_SUB; both
// operands (single, in temp NEON regs `idxd`/`idxt`) are mutated in place.
static void FPU_ADD_SUB(int idxd, int idxt)
{
	const a64::VRegister sd = armSRegister(idxd);
	const a64::VRegister st = armSRegister(idxt);

	armAsm->Fmov(RWARG1, sd);  // d bits
	armAsm->Fmov(RWARG2, st);  // t bits
	// GE-M2: the exponent-diff and mask temps use the reserved load/store scratch
	// x9/x10, not the RWARG3/RWARG4 (w2/w3) pool hosts they replaced — w2/w3 are
	// EE-allocatable, so under the residency flip they can hold a live guest GPR,
	// and this hand-emitted path never flushes the allocator. This span has no
	// load/store or C-call, so x9/x10 are free scratch here. (x86 uses GPR temps
	// too: pcsx2/x86/iFPU.cpp FPU_ADD_SUB; only the register choice is our
	// scratch-discipline constraint.)
	armAsm->Ubfx(a64::w9, RWARG1, 23, 8);    // expd
	armAsm->Ubfx(RWSCRATCH, RWARG2, 23, 8); // expt
	armAsm->Sub(a64::w9, a64::w9, RWSCRATCH); // diff = expd - expt (signed)

	a64::Label caseD25, casePos, caseEq, caseDn25, done;
	armAsm->Cmp(a64::w9, 25);
	armAsm->B(&caseD25, a64::ge);
	armAsm->Cmp(a64::w9, 0);
	armAsm->B(&casePos, a64::gt);
	armAsm->B(&caseEq, a64::eq);
	armAsm->Cmn(a64::w9, 25);                 // cmp diff, -25
	armAsm->B(&caseDn25, a64::le);

	// diff in -24..-1 (expd < expt): mask tempd's low (-diff-1) bits.
	armAsm->Neg(RWSCRATCH, a64::w9);
	armAsm->Sub(RWSCRATCH, RWSCRATCH, 1);
	armAsm->Mov(a64::w10, 0xffffffff);
	armAsm->Lsl(a64::w10, a64::w10, RWSCRATCH);
	armAsm->And(RWARG1, RWARG1, a64::w10);
	armAsm->Fmov(sd, RWARG1);
	armAsm->B(&done);

	armAsm->Bind(&caseD25);
	// diff >= 25 (expt much smaller): tempt keeps only its sign.
	armAsm->And(RWARG2, RWARG2, 0x80000000);
	armAsm->Fmov(st, RWARG2);
	armAsm->B(&done);

	armAsm->Bind(&casePos);
	// diff in 1..24 (expt smaller): mask tempt's low (diff-1) bits.
	armAsm->Sub(RWSCRATCH, a64::w9, 1);
	armAsm->Mov(a64::w10, 0xffffffff);
	armAsm->Lsl(a64::w10, a64::w10, RWSCRATCH);
	armAsm->And(RWARG2, RWARG2, a64::w10);
	armAsm->Fmov(st, RWARG2);
	armAsm->B(&done);

	armAsm->Bind(&caseDn25);
	// diff <= -25 (expd much smaller): tempd keeps only its sign.
	armAsm->And(RWARG1, RWARG1, 0x80000000);
	armAsm->Fmov(sd, RWARG1);

	armAsm->Bind(&caseEq);  // diff == 0: nothing
	armAsm->Bind(&done);
}

// ---- Op cores --------------------------------------------------------------

// Copy an allocator-resident FP source (EEREC_S/EEREC_T) into a fresh temp so
// ToDouble can mutate it without corrupting the guest fpr slot.
static int copySrc(int eerec)
{
	const int idx = _allocTempNEONreg();
	armAsm->Fmov(armSRegister(idx), armSRegister(eerec));
	return idx;
}

// ADD/SUB/ADDA/SUBA: FPU_ADD_SUB guard mask -> widen -> op in double -> narrow.
static void recFPUOp(int info, int eeRecDst, int op /*0=add,1=sub*/, bool acc)
{
	const int sreg = copySrc(EEREC_S);
	const int treg = copySrc(EEREC_T);

	FPU_ADD_SUB(sreg, treg);
	ToDouble(sreg);
	ToDouble(treg);

	if (op == 0)
		armAsm->Fadd(armDRegister(sreg), armDRegister(sreg), armDRegister(treg));
	else
		armAsm->Fsub(armDRegister(sreg), armDRegister(sreg), armDRegister(treg));

	ToPS2FPU_Full(sreg, true, treg, acc, true);
	armAsm->Fmov(armSRegister(eeRecDst), armSRegister(sreg));

	_freeNEONreg(sreg);
	_freeNEONreg(treg);
}

// MUL/MULA: widen -> multiply in double -> narrow. (FPUMULHACK — the Tales of
// Destiny gamefix — is intentionally not folded in here; default off.)
static void recMULop(int info, int eeRecDst, bool acc)
{
	const int sreg = copySrc(EEREC_S);
	const int treg = copySrc(EEREC_T);

	ToDouble(sreg);
	ToDouble(treg);
	armAsm->Fmul(armDRegister(sreg), armDRegister(sreg), armDRegister(treg));

	ToPS2FPU_Full(sreg, true, treg, acc, false);
	armAsm->Fmov(armSRegister(eeRecDst), armSRegister(sreg));

	_freeNEONreg(sreg);
	_freeNEONreg(treg);
}

// MADD/MSUB/MADDA/MSUBA: (Fd or ACC) = ACC +/- Fs*Ft, with two PS2-accurate
// roundings (the multiply, then the accumulate) and overflow propagation from
// BOTH the product and the prior ACC. Port of x86 recMaddsub.
//
// The control flow mirrors x86: do the full-mode multiply (which may raise O),
// guard-mask ACC against the product, then branch on whether the product
// overflowed (FPUflagO) or the incoming ACC was already saturated (ACCflag&1).
// If either did, the accumulate is dominated by a 2^128-class term and the
// result is just +/-max with the dominant sign — skip the double add entirely.
// Only when both are finite is the accumulation performed in double.
static void recMaddsub(int info, int eeRecDst, int op /*0=add,1=sub*/, bool acc)
{
	const int sreg = copySrc(EEREC_S);
	const int treg = copySrc(EEREC_T);

	// --- multiply stage: sreg = ToPS2FPU(ToDouble(s) * ToDouble(t)). Sets O on
	//     product overflow; acc=false so it never touches ACCflag here. ---
	ToDouble(sreg);
	ToDouble(treg);
	armAsm->Fmul(armDRegister(sreg), armDRegister(sreg), armDRegister(treg));
	ToPS2FPU_Full(sreg, true, treg, false, false);

	// --- reload ACC (allocator-resident) into treg, then guard-mask it against
	//     the single-precision product. ---
	armAsm->Fmov(armSRegister(treg), armSRegister(EEREC_ACC));
	FPU_ADD_SUB(treg, sreg);

	a64::Label mulovf, accovf, operation, skipall;

	// product overflowed? -> mulovf
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Tst(RWSCRATCH, FPUflagO);
	armAsm->B(&mulovf, a64::ne);
	ToDouble(sreg);

	// prior ACC saturated? -> accovf
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.ACCflag);
	armAsm->Tst(RWSCRATCH, 1);
	armAsm->B(&accovf, a64::ne);
	ToDouble(treg);
	armAsm->B(&operation);

	armAsm->Bind(&mulovf);
	// Product is a saturated single; for SUB negate its sign, then it becomes
	// the (single) accumulate result. Falls through into accovf.
	if (op == 1)
	{
		armAsm->Fmov(RWSCRATCH, armSRegister(sreg));
		armAsm->Eor(RWSCRATCH, RWSCRATCH, 0x80000000);
		armAsm->Fmov(armSRegister(sreg), RWSCRATCH);
	}
	armAsm->Fmov(armSRegister(treg), armSRegister(sreg));

	armAsm->Bind(&accovf);
	// SetMaxValue(treg): keep sign, set all lower bits -> +/-PS2 max.
	armAsm->Fmov(RWSCRATCH, armSRegister(treg));
	armAsm->Orr(RWSCRATCH, RWSCRATCH, 0x7fffffff);
	armAsm->Fmov(armSRegister(treg), RWSCRATCH);
	// Clear O|U then raise O|SO (and ACCflag for the *A variants).
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Bic(RWSCRATCH, RWSCRATCH, FPUflagO | FPUflagU);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, FPUflagO | FPUflagSO);
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	if (acc)
	{
		armLoadEERegPtr(RWSCRATCH, &fpuRegs.ACCflag);
		armAsm->Orr(RWSCRATCH, RWSCRATCH, 1);
		armStoreEERegPtr(RWSCRATCH, &fpuRegs.ACCflag);
	}
	armAsm->B(&skipall);

	armAsm->Bind(&operation);
	// Both finite: accumulate in double, narrow with flags.
	if (op == 1)
		armAsm->Fsub(armDRegister(treg), armDRegister(treg), armDRegister(sreg));
	else
		armAsm->Fadd(armDRegister(treg), armDRegister(treg), armDRegister(sreg));
	ToPS2FPU_Full(treg, true, sreg, acc, true);

	armAsm->Bind(&skipall);
	armAsm->Fmov(armSRegister(eeRecDst), armSRegister(treg));

	_freeNEONreg(sreg);
	_freeNEONreg(treg);
}

// ---- Per-opcode DOUBLE emitters (called by the CHECK_FPU_FULL branch in
//      iFPU-arm64.cpp via eeFPURecompileCode) -------------------------------

void recADD_S_xmm(int info)  { recFPUOp(info, EEREC_D,   0, false); }
void recSUB_S_xmm(int info)  { recFPUOp(info, EEREC_D,   1, false); }
void recADDA_S_xmm(int info) { recFPUOp(info, EEREC_ACC, 0, true);  }
void recSUBA_S_xmm(int info) { recFPUOp(info, EEREC_ACC, 1, true);  }
void recMUL_S_xmm(int info)  { recMULop(info, EEREC_D,   false); }
void recMULA_S_xmm(int info) { recMULop(info, EEREC_ACC, true);  }
void recMADD_S_xmm(int info)  { recMaddsub(info, EEREC_D,   0, false); }
void recMSUB_S_xmm(int info)  { recMaddsub(info, EEREC_D,   1, false); }
void recMADDA_S_xmm(int info) { recMaddsub(info, EEREC_ACC, 0, true);  }
void recMSUBA_S_xmm(int info) { recMaddsub(info, EEREC_ACC, 1, true);  }

// ---- GE-20: the non-arith DOUBLE bodies (x86 iFPUd.cpp ports) --------------

// x86 CLEAR_OU_FLAGS. Memory RMW is coherent with the GE-12 FCR31 residency
// because fpuTryAllocFCR31 refuses to allocate under CHECK_FPU_FULL — in FULL
// mode fprc[31] memory is the only home.
static void ClearOUFlags()
{
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Bic(RWSCRATCH, RWSCRATCH, FPUflagO | FPUflagU);
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
}

// ABS/NEG: raw sign-bit ops — NO clamp (a pseudo-inf stays a pseudo-inf) —
// plus the O/U clear. ARM FABS/FNEG are non-arithmetic bit operations (no
// exceptions, NaN patterns pass through with only the sign changed), so they
// match x86's AND/XOR-with-mask exactly.
void recABS_S_xmm(int info)
{
	ClearOUFlags();
	armAsm->Fabs(armSRegister(EEREC_D), armSRegister(EEREC_S));
}

void recNEG_S_xmm(int info)
{
	ClearOUFlags();
	armAsm->Fneg(armSRegister(EEREC_D), armSRegister(EEREC_S));
}

// MAX/MIN: PS2 semantics on ALL values (incl. denormals — no FTZ, no clamp).
// Port of x86 recMINMAX's integer-ordering trick: for each operand build the
// 64-bit double pattern {lo32 = raw float bits, hi32 = sign | 0x40000000} and
// compare as doubles. The fixed 0x400-exponent upper word makes IEEE-double
// ordering equal PS2 total (sign, magnitude) ordering over the raw bits, and
// no constructed input can be NaN/Inf (the double exponent field is constant),
// so Fmin/Fmax's NaN propagation can never trigger. Result = lower 32 bits of
// the selected pattern.
static void recMINMAX(int info, bool ismin)
{
	// Temps FIRST: the alloc's eviction stores must not land between the GPR
	// pattern builds and their consuming Fmovs (alloc-before-emit rule).
	const int sreg = _allocTempNEONreg();
	const int treg = _allocTempNEONreg();

	ClearOUFlags();

	armAsm->Fmov(RWSCRATCH, armSRegister(EEREC_S)); // x8 = zext(s bits)
	armAsm->And(RWARG1, RWSCRATCH, 0x80000000);
	armAsm->Orr(RWARG1, RWARG1, 0x40000000);
	armAsm->Orr(RXSCRATCH, RXSCRATCH, a64::Operand(RXARG1, a64::LSL, 32));

	armAsm->Fmov(RWARG2, armSRegister(EEREC_T)); // x1 = zext(t bits)
	// GE-M2: exp/sign pattern temp in reserved scratch x9 (was RWARG3/w2, an
	// EE-allocatable pool host — see FPU_ADD_SUB). No load/store or C-call spans it.
	armAsm->And(a64::w9, RWARG2, 0x80000000);
	armAsm->Orr(a64::w9, a64::w9, 0x40000000);
	armAsm->Orr(RXARG2, RXARG2, a64::Operand(a64::x9, a64::LSL, 32));

	armAsm->Fmov(armDRegister(sreg), RXSCRATCH);
	armAsm->Fmov(armDRegister(treg), RXARG2);
	if (ismin)
		armAsm->Fmin(armDRegister(sreg), armDRegister(sreg), armDRegister(treg));
	else
		armAsm->Fmax(armDRegister(sreg), armDRegister(sreg), armDRegister(treg));
	armAsm->Fmov(RXSCRATCH, armDRegister(sreg));
	armAsm->Fmov(armSRegister(EEREC_D), RWSCRATCH); // lower 32 = winner's raw bits
	_freeNEONreg(sreg);
	_freeNEONreg(treg);
}

void recMAX_S_xmm(int info) { recMINMAX(info, false); }
void recMIN_S_xmm(int info) { recMINMAX(info, true); }

// C.cond: widen both operands with ToDouble and compare as doubles — a PS2
// pseudo-inf compares as the finite 2^128-scale number it is, with no operand
// clamping (x86 recCMP + recC_*_xmm). ToDouble never yields NaN, so the
// compare is always ordered and the lt/le/eq condition reads are exact.
static void recCMP(int info)
{
	const int sreg = copySrc(EEREC_S);
	const int treg = copySrc(EEREC_T);
	ToDouble(sreg);
	ToDouble(treg);
	armAsm->Fcmp(armDRegister(sreg), armDRegister(treg));
	_freeNEONreg(sreg);
	_freeNEONreg(treg);
}

static void recCcond(int info, a64::Condition cond)
{
	recCMP(info);
	// NZCV is live from the Fcmp: _freeNEONreg emits at most plain stores and
	// the fprc load below is a plain Ldr — neither touches the flags.
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Cset(RWARG1, cond);
	armAsm->Bfi(RWSCRATCH, RWARG1, 23, 1); // FPUflagC = bit 23
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
}

void recC_EQ_xmm(int info) { recCcond(info, a64::eq); }
void recC_LT_xmm(int info) { recCcond(info, a64::lt); }
void recC_LE_xmm(int info) { recCcond(info, a64::le); }

// ---- DIV / SQRT / RSQRT ----------------------------------------------------

// GE-13's immediate-FPCR idiom (local copy of iFPU-arm64.cpp emitLoadFPCR —
// the value is bake-safe: a CPU-config change resets the recompilers).
static void emitLoadFPCRImm(u64 bitmask)
{
	armAsm->Mov(a64::x9, bitmask);
	armAsm->Msr(a64::FPCR, a64::x9);
}

// Plain memory RMWs on fprc[31] (FULL mode ⇒ never GPR-resident, see
// ClearOUFlags). No allocator calls — safe inside conditional emit arms.
static void SetFprcOr(u32 bits)
{
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, bits);
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
}

static void ClearIDFlags()
{
	armLoadEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
	armAsm->Bic(RWSCRATCH, RWSCRATCH, FPUflagI | FPUflagD);
	armStoreEERegPtr(RWSCRATCH, &fpuRegs.fprc[31]);
}

// x86 SetMaxValue: keep the sign bit, force all magnitude bits -> ±PS2 max.
static void SetMaxValueS(int idx)
{
	armAsm->Fmov(RWSCRATCH, armSRegister(idx));
	armAsm->And(RWSCRATCH, RWSCRATCH, 0x80000000);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, 0x7f7fffff);
	armAsm->Fmov(armSRegister(idx), RWSCRATCH);
}

// x86 recDIVhelper1 (FPU_FLAGS_ID == 1 unconditionally): divide-by-zero
// flag/result shape in the single domain, otherwise divide in double.
// sreg/treg are temp copies (mutated); the result lands in sreg's S lane.
// The Fcmp-with-zero runs under the EE FPCR whose FZ bit flushes denormal
// inputs — same divisor-is-zero net as x86's DAZ'd CMPEQ.SS. The double
// quotient of two in-range PS2 values is always finite (max magnitude
// ~2^255), so ToPS2FPU_Full's finite-only contract holds.
static void recDIVhelper1(int sreg, int treg)
{
	ClearIDFlags();

	a64::Label normal, xOverZero, setDone, done;
	armAsm->Fcmp(armSRegister(treg), 0.0);
	armAsm->B(&normal, a64::ne);

	// Divisor is ±0: pick the flag pair, then result = sign(fs^ft) | +max.
	armAsm->Fcmp(armSRegister(sreg), 0.0);
	armAsm->B(&xOverZero, a64::ne);
	SetFprcOr(FPUflagI | FPUflagSI); // 0/0
	armAsm->B(&setDone);
	armAsm->Bind(&xOverZero);
	SetFprcOr(FPUflagD | FPUflagSD); // x/0
	armAsm->Bind(&setDone);

	armAsm->Fmov(RWSCRATCH, armSRegister(sreg));
	armAsm->Fmov(RWARG1, armSRegister(treg));
	armAsm->Eor(RWSCRATCH, RWSCRATCH, RWARG1);
	armAsm->And(RWSCRATCH, RWSCRATCH, 0x80000000);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, 0x7f7fffff);
	armAsm->Fmov(armSRegister(sreg), RWSCRATCH);
	armAsm->B(&done);

	armAsm->Bind(&normal);
	ToDouble(sreg);
	ToDouble(treg);
	armAsm->Fdiv(armDRegister(sreg), armDRegister(sreg), armDRegister(treg));
	ToPS2FPU_Full(sreg, false, treg, false, false);

	armAsm->Bind(&done);
}

void recDIV_S_xmm(int info)
{
	// PS2 DIV rounds to nearest (x86 swaps MXCSR to FPUDivFPCR around the op).
	const bool swapFpcr = EmuConfig.Cpu.FPUFPCR.bitmask != EmuConfig.Cpu.FPUDivFPCR.bitmask;
	if (swapFpcr)
		emitLoadFPCRImm(EmuConfig.Cpu.FPUDivFPCR.bitmask);

	const int sreg = copySrc(EEREC_S);
	const int treg = copySrc(EEREC_T);
	recDIVhelper1(sreg, treg);
	armAsm->Fmov(armSRegister(EEREC_D), armSRegister(sreg));
	_freeNEONreg(sreg);
	_freeNEONreg(treg);

	if (swapFpcr)
		emitLoadFPCRImm(EmuConfig.Cpu.FPUFPCR.bitmask);
}

void recSQRT_S_xmm(int info)
{
	// Round-to-nearest for the double Fsqrt + the ToPS2FPU narrowing, like
	// x86's roundmode_nearest swap (FPUDivFPCR is the nearest-mode FPCR).
	const bool swapFpcr = EmuConfig.Cpu.FPUFPCR.bitmask != EmuConfig.Cpu.FPUDivFPCR.bitmask;
	if (swapFpcr)
		emitLoadFPCRImm(EmuConfig.Cpu.FPUDivFPCR.bitmask);

	const int treg = copySrc(EEREC_T); // SQRT.S reads FT

	ClearIDFlags();
	// x86 DOUBLE tests the raw SIGN BIT (unlike the fast body's exp-field
	// gate): sqrt(-0) sets I|SI too, then |t| makes the operand positive.
	// x86-JIT is the FULL-mode oracle for this corner.
	armAsm->Fmov(RWARG1, armSRegister(treg));
	a64::Label tPositive;
	armAsm->Tbz(RWARG1, 31, &tPositive);
	SetFprcOr(FPUflagI | FPUflagSI);
	armAsm->Fabs(armSRegister(treg), armSRegister(treg));
	armAsm->Bind(&tPositive);

	ToDouble(treg);
	armAsm->Fsqrt(armDRegister(treg), armDRegister(treg));
	ToPS2FPU_Full(treg, false, treg, false, false);
	armAsm->Fmov(armSRegister(EEREC_D), armSRegister(treg));
	_freeNEONreg(treg);

	if (swapFpcr)
		emitLoadFPCRImm(EmuConfig.Cpu.FPUFPCR.bitmask);
}

// x86 recRSQRThelper1: negative-divisor I|SI + |t|, zero-divisor flag pair
// with SetMaxValue keyed off the DIVIDEND's sign, else fs / sqrt(ft) in
// double. (The interp keys the zero-divisor sign off the DIVISOR — x86-JIT
// wins that disagreement under FULL.)
void recRSQRT_S_xmm(int info)
{
	const bool swapFpcr = EmuConfig.Cpu.FPUFPCR.bitmask != EmuConfig.Cpu.FPUDivFPCR.bitmask;
	if (swapFpcr)
		emitLoadFPCRImm(EmuConfig.Cpu.FPUDivFPCR.bitmask);

	const int sreg = copySrc(EEREC_S);
	const int treg = copySrc(EEREC_T);

	ClearIDFlags();

	armAsm->Fmov(RWARG1, armSRegister(treg));
	a64::Label tPositive;
	armAsm->Tbz(RWARG1, 31, &tPositive);
	SetFprcOr(FPUflagI | FPUflagSI);
	armAsm->Fabs(armSRegister(treg), armSRegister(treg));
	armAsm->Bind(&tPositive);

	a64::Label normal, zeroOverZero, setDone, done;
	armAsm->Fcmp(armSRegister(treg), 0.0);
	armAsm->B(&normal, a64::ne);

	armAsm->Fcmp(armSRegister(sreg), 0.0);
	armAsm->B(&zeroOverZero, a64::eq);
	SetFprcOr(FPUflagD | FPUflagSD); // x/0
	armAsm->B(&setDone);
	armAsm->Bind(&zeroOverZero);
	SetFprcOr(FPUflagI | FPUflagSI); // 0/0
	armAsm->Bind(&setDone);
	SetMaxValueS(sreg);
	armAsm->B(&done);

	armAsm->Bind(&normal);
	ToDouble(treg);
	ToDouble(sreg);
	armAsm->Fsqrt(armDRegister(treg), armDRegister(treg));
	armAsm->Fdiv(armDRegister(sreg), armDRegister(sreg), armDRegister(treg));
	ToPS2FPU_Full(sreg, false, treg, false, false);

	armAsm->Bind(&done);
	armAsm->Fmov(armSRegister(EEREC_D), armSRegister(sreg));
	_freeNEONreg(sreg);
	_freeNEONreg(treg);

	if (swapFpcr)
		emitLoadFPCRImm(EmuConfig.Cpu.FPUFPCR.bitmask);
}

#undef _Ft_
#undef _Fs_
#undef _Fd_

} // namespace DOUBLE
} // namespace COP1
} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
