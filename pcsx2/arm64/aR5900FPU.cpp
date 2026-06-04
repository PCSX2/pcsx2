// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) recompiler — FPU (COP1) exact-semantics opcode generators
// (Phase 5.2a).
//
// This file implements the COP1 instructions that are pure bit/integer movement:
// register transfers (MFC1/MTC1/CFC1/CTC1), the bit-twiddling "arithmetic" ops
// (MOV_S/ABS_S/NEG_S), and the FPU load/store ops (LWC1/SWC1). None of these
// involve the EE FPU's non-IEEE float rounding/clamping, so the ARM64 codegen is
// bit-exact against the interpreter (the ground truth — see pcsx2/FPU.cpp).
//
// The remaining float arithmetic (MADD/MSUB, MAX/MIN, CVT, the C.*.S compares,
// and the BC1* branches) needs the EE's denormal-flush /
// infinity-clamp / overflow-underflow behaviour (fpuDouble + checkOverflow/
// checkUnderflow). Those remain interpreter fallbacks until a later increment.

#include "aR5900.h"

#include "R5900.h"

#include "common/Assertions.h"

#include <cstddef>

namespace a64 = vixl::aarch64;

// FCR31 (fprc[31]) flag bits — see pcsx2/FPU.cpp.
static constexpr u32 FPUflagO = 0x00008000; // overflow (cause)
static constexpr u32 FPUflagU = 0x00004000; // underflow (cause)
static constexpr u32 FPUflagI = 0x00020000; // invalid operation (cause)
static constexpr u32 FPUflagD = 0x00010000; // divide by zero (cause)
static constexpr u32 FPUflagSO = 0x00000010; // overflow (sticky)
static constexpr u32 FPUflagSU = 0x00000008; // underflow (sticky)
static constexpr u32 FPUflagSI = 0x00000040; // invalid operation (sticky)
static constexpr u32 FPUflagSD = 0x00000020; // divide by zero (sticky)

// IEEE-754 single-precision sentinel bit patterns used by the EE clamp logic.
static constexpr u32 kPosInfinity = 0x7f800000; // exponent all-ones, mantissa zero
static constexpr u32 kPosFmax = 0x7f7fffff;     // largest finite magnitude
static constexpr u32 kSignBit = 0x80000000;
static constexpr u32 kExpMask = 0x7f800000;
static constexpr u32 kMantMask = 0x007fffff;

// Clear the given FCR31 cause flags (read-modify-write fprc[31]). Used by the
// MOV-family ops that the interpreter documents as clearing O|U every execution.
static void emitClearFCR31Flags(u32 flags)
{
	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
	armAsm->And(RSCRATCHADDR.W(), RSCRATCHADDR.W(), ~flags);
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
}

// ------------------------------------------------------------------------
// MFC1: move FPR -> GPR. The interpreter sign-extends the 32-bit FPR into the
// low 64-bit doubleword of the GPR (GPR[rt].SD[0] = (s32)fpr[fs]); the upper
// doubleword is left untouched, matching the EE's scalar-write semantics.
void armEmitMFC1(u32 rt, u32 fs)
{
	if (rt == 0)
		return;

	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
	armAsm->Sxtw(RSCRATCHADDR, RSCRATCHADDR.W());
	armAsm->Str(RSCRATCHADDR, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

// ------------------------------------------------------------------------
// MTC1: move GPR -> FPR (low word only). GPR[0] reads as zero straight from the
// register file, so rt==0 needs no special case.
void armEmitMTC1(u32 fs, u32 rt)
{
	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
}

// ------------------------------------------------------------------------
// CFC1: move FPU control register -> GPR. `fs` is a compile-time constant, so the
// interpreter's three-way select collapses to a single emitted path. fprc[31] is
// sign-extended; the other defined values (0x2E00 for fs==0, else 0) are constants.
void armEmitCFC1(u32 rt, u32 fs)
{
	if (rt == 0)
		return;

	if (fs == 31)
	{
		armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
		armAsm->Sxtw(RSCRATCHADDR, RSCRATCHADDR.W());
		armAsm->Str(RSCRATCHADDR, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	}
	else if (fs == 0)
	{
		armAsm->Mov(RSCRATCHADDR, 0x2E00);
		armAsm->Str(RSCRATCHADDR, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	}
	else
	{
		armAsm->Str(a64::xzr, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	}
}

// ------------------------------------------------------------------------
// CTC1: move GPR -> FPU control register. The interpreter only honours writes to
// fprc[31]; writes to any other control register are ignored, so for a compile-time
// fs != 31 this generator emits nothing.
void armEmitCTC1(u32 fs, u32 rt)
{
	if (fs != 31)
		return;

	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
}

// ------------------------------------------------------------------------
// MOV_S: fpr[fd] = fpr[fs] (pure 32-bit copy, no flags touched).
void armEmitMOV_S(u32 fd, u32 fs)
{
	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
}

// ------------------------------------------------------------------------
// ABS_S: fpr[fd] = fpr[fs] with the sign bit cleared; clears the O|U cause flags.
void armEmitABS_S(u32 fd, u32 fs)
{
	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
	armAsm->And(RSCRATCHADDR.W(), RSCRATCHADDR.W(), 0x7fffffff);
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
	emitClearFCR31Flags(FPUflagO | FPUflagU);
}

// ------------------------------------------------------------------------
// NEG_S: fpr[fd] = fpr[fs] with the sign bit flipped; clears the O|U cause flags.
void armEmitNEG_S(u32 fd, u32 fs)
{
	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
	armAsm->Eor(RSCRATCHADDR.W(), RSCRATCHADDR.W(), 0x80000000);
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
	emitClearFCR31Flags(FPUflagO | FPUflagU);
}

// ------------------------------------------------------------------------
// LWC1: fpr[ft].UL = mem32[GPR[rs] + imm]. Routed through the same slow-path vtlb
// helper as the GPR loads; the 32-bit result is written to the FPR's low word.
// (FPR0 is a real register, so there is no rt==0 discard as there is for GPRs.)
void armEmitLWC1(u32 ft, u32 rs, s32 imm)
{
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armEmitVtlbRead(32, /*sign*/ false, RXRET, RWARG1);
	armAsm->Str(RWRET, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
}

// ------------------------------------------------------------------------
// SWC1: mem32[GPR[rs] + imm] = fpr[ft].UL.
void armEmitSWC1(u32 ft, u32 rs, s32 imm)
{
	armAsm->Ldr(RWARG2, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armEmitVtlbWrite(32, RWARG1, RWARG2);
}

// ========================================================================
// Float arithmetic (Phase 5.2b)
//
// The EE FPU is not IEEE-754. The interpreter (pcsx2/FPU.cpp, ground truth)
// implements the quirks with two pieces we reproduce here:
//   - fpuDouble(f): clamp each operand before the op (denormal/zero -> signed 0,
//     inf/NaN -> signed fmax);
//   - checkOverflow/checkUnderflow: clamp the result (inf -> signed fmax,
//     denormal -> signed 0) and set the FCR31 O/U cause+sticky flags.
// The op itself is host single-precision NEON (Fadd/Fsub/Fmul), bit-identical to
// the interpreter's `float OP float` (both IEEE round-to-nearest-even). Generators
// have no register allocator and make no calls, so they freely use the caller-saved
// w9-w13 as integer scratch and s29-s31 (RSSCRATCH*) for the float operands.
// ========================================================================

// Apply the EE fpuDouble() input clamp to the 32-bit float bits already in wraw,
// leaving the clamped single-precision float in dstS. Uses w9-w12 as scratch
// (wraw may alias w9). denormal/zero -> signed 0; inf/NaN -> signed fmax.
static void emitClampFpuDoubleBits(const a64::VRegister& dstS, const a64::Register& wraw)
{
	const a64::Register w = a64::w9;     // raw bits / clamped result
	const a64::Register wexp = a64::w10; // exponent field [30:23]
	const a64::Register wsign = a64::w11;
	const a64::Register winf = a64::w12;

	if (!w.Is(wraw))
		armAsm->Mov(w, wraw);
	armAsm->Ubfx(wexp, w, 23, 8);
	armAsm->And(wsign, w, kSignBit);
	// inf/NaN clamp candidate: sign | fmax
	armAsm->Mov(winf, kPosFmax);
	armAsm->Orr(winf, wsign, winf);
	// exp == 0  -> denormal/zero -> result = sign only
	armAsm->Cmp(wexp, 0);
	armAsm->Csel(w, wsign, w, a64::eq);
	// exp == 0xFF -> inf/NaN -> result = sign | fmax
	armAsm->Cmp(wexp, 0xFF);
	armAsm->Csel(w, winf, w, a64::eq);
	armAsm->Fmov(dstS, w);
}

// Load fpr/ACC at byteOffset, apply the EE fpuDouble() input clamp, leave the
// resulting single-precision float in dstS.
static void emitLoadFpuDouble(const a64::VRegister& dstS, u32 byteOffset)
{
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, byteOffset));
	emitClampFpuDoubleBits(dstS, a64::w9);
}

// Apply checkOverflow + checkUnderflow to the float result in srcS, update the
// FCR31 flags (when setFlags), and store the (possibly clamped) 32-bit result to
// the fpr/ACC slot at dstByteOffset. setFlags=false (DIV/SQRT/RSQRT pass 0 for the
// flag mask) clamps the value but touches no O/U flags.
static void emitStoreClampedResult(const a64::VRegister& srcS, u32 dstByteOffset, bool setFlags)
{
	const a64::Register w = a64::w9;     // result bits
	const a64::Register wabs = a64::w10;
	const a64::Register wtmp = a64::w11;
	const a64::Register wsign = a64::w12;
	const a64::Register wflags = a64::w13; // FCR31 (only touched when setFlags)

	armAsm->Fmov(w, srcS);
	if (setFlags)
		armAsm->Ldr(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));

	a64::Label overflow, doUnderflow, notUnderflow, done;

	// checkOverflow: (w & ~sign) == +Inf  ->  w = sign | fmax, set O|SO, skip underflow
	armAsm->And(wabs, w, ~kSignBit);
	armAsm->Mov(wtmp, kPosInfinity);
	armAsm->Cmp(wabs, wtmp);
	armAsm->B(&overflow, a64::eq);
	if (setFlags) // else-path clears the O cause flag
		armAsm->And(wflags, wflags, ~FPUflagO);
	armAsm->B(&doUnderflow);

	armAsm->Bind(&overflow);
	armAsm->And(wsign, w, kSignBit);
	armAsm->Mov(wtmp, kPosFmax);
	armAsm->Orr(w, wsign, wtmp);
	if (setFlags)
		armAsm->Orr(wflags, wflags, FPUflagO | FPUflagSO);
	armAsm->B(&done); // overflow returns true -> underflow not evaluated

	// checkUnderflow: exp==0 && mantissa!=0 (denormal) -> w &= sign, set U|SU
	armAsm->Bind(&doUnderflow);
	armAsm->And(wtmp, w, kExpMask);
	armAsm->Cbnz(wtmp, &notUnderflow); // exp != 0 -> not a denormal
	armAsm->And(wtmp, w, kMantMask);
	armAsm->Cbz(wtmp, &notUnderflow); // mantissa == 0 -> it's a true zero, not denormal
	armAsm->And(w, w, kSignBit);
	if (setFlags)
		armAsm->Orr(wflags, wflags, FPUflagU | FPUflagSU);
	armAsm->B(&done);

	armAsm->Bind(&notUnderflow);
	if (setFlags) // else-path clears the U cause flag
		armAsm->And(wflags, wflags, ~FPUflagU);

	armAsm->Bind(&done);
	if (setFlags)
		armAsm->Str(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
	armAsm->Str(w, a64::MemOperand(RESTATEPTR, dstByteOffset));
}

enum class FpuBinOp
{
	Add,
	Sub,
	Mul
};

// Shared body for the ADD/SUB/MUL (-> fpr[fd]) and ADDA/SUBA/MULA (-> ACC) family.
static void emitFpuBinary(FpuBinOp op, u32 dstByteOffset, u32 fs, u32 ft)
{
	emitLoadFpuDouble(RSSCRATCH, EE_FPR_OFFSET(fs));
	emitLoadFpuDouble(RSSCRATCH2, EE_FPR_OFFSET(ft));
	switch (op)
	{
		case FpuBinOp::Add: armAsm->Fadd(RSSCRATCH, RSSCRATCH, RSSCRATCH2); break;
		case FpuBinOp::Sub: armAsm->Fsub(RSSCRATCH, RSSCRATCH, RSSCRATCH2); break;
		case FpuBinOp::Mul: armAsm->Fmul(RSSCRATCH, RSSCRATCH, RSSCRATCH2); break;
	}
	emitStoreClampedResult(RSSCRATCH, dstByteOffset, /*setFlags*/ true);
}

void armEmitADD_S(u32 fd, u32 fs, u32 ft) { emitFpuBinary(FpuBinOp::Add, EE_FPR_OFFSET(fd), fs, ft); }
void armEmitSUB_S(u32 fd, u32 fs, u32 ft) { emitFpuBinary(FpuBinOp::Sub, EE_FPR_OFFSET(fd), fs, ft); }
void armEmitMUL_S(u32 fd, u32 fs, u32 ft) { emitFpuBinary(FpuBinOp::Mul, EE_FPR_OFFSET(fd), fs, ft); }
void armEmitADDA_S(u32 fs, u32 ft) { emitFpuBinary(FpuBinOp::Add, EE_ACC_OFFSET, fs, ft); }
void armEmitSUBA_S(u32 fs, u32 ft) { emitFpuBinary(FpuBinOp::Sub, EE_ACC_OFFSET, fs, ft); }
void armEmitMULA_S(u32 fs, u32 ft) { emitFpuBinary(FpuBinOp::Mul, EE_ACC_OFFSET, fs, ft); }

void armEmitDIV_S(u32 fd, u32 fs, u32 ft)
{
	const a64::Register wdivisor = a64::w9;
	const a64::Register wdividend = a64::w10;
	const a64::Register wtmp = a64::w11;
	const a64::Register wflags = a64::w13;

	a64::Label normal, done, dividendZero;

	armAsm->Ldr(wdivisor, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
	armAsm->Ldr(wdividend, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
	armAsm->And(wtmp, wdivisor, kExpMask);
	armAsm->Cbnz(wtmp, &normal);

	// checkDivideByZero(): denormal divisors count as zero. z/0 sets D|SD,
	// 0/0 sets I|SI, and the result is sign(divisor ^ dividend) | +fmax.
	armAsm->Ldr(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
	armAsm->And(wtmp, wdividend, kExpMask);
	armAsm->Cbz(wtmp, &dividendZero);
	armAsm->Orr(wflags, wflags, FPUflagD | FPUflagSD);
	armAsm->B(&done);
	armAsm->Bind(&dividendZero);
	armAsm->Orr(wflags, wflags, FPUflagI | FPUflagSI);

	armAsm->Bind(&done);
	armAsm->Eor(wtmp, wdivisor, wdividend);
	armAsm->And(wtmp, wtmp, kSignBit);
	armAsm->Orr(wtmp, wtmp, kPosFmax);
	armAsm->Str(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
	armAsm->Str(wtmp, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
	a64::Label end;
	armAsm->B(&end);

	armAsm->Bind(&normal);
	emitLoadFpuDouble(RSSCRATCH, EE_FPR_OFFSET(fs));
	emitLoadFpuDouble(RSSCRATCH2, EE_FPR_OFFSET(ft));
	armAsm->Fdiv(RSSCRATCH, RSSCRATCH, RSSCRATCH2);
	emitStoreClampedResult(RSSCRATCH, EE_FPR_OFFSET(fd), /*setFlags*/ false);

	armAsm->Bind(&end);
}

void armEmitSQRT_S(u32 fd, u32 ft)
{
	const a64::Register wraw = a64::w9;
	const a64::Register wtmp = a64::w10;
	const a64::Register wflags = a64::w13;

	a64::Label nonzero, positive, done;

	armAsm->Ldr(wraw, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
	armAsm->Ldr(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
	armAsm->And(wflags, wflags, ~(FPUflagI | FPUflagD));
	armAsm->And(wtmp, wraw, kExpMask);
	armAsm->Cbnz(wtmp, &nonzero);

	// +/-0 and denormals produce signed zero with I/D cause flags cleared.
	armAsm->And(wraw, wraw, kSignBit);
	armAsm->Str(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
	armAsm->Str(wraw, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
	armAsm->B(&done);

	armAsm->Bind(&nonzero);
	armAsm->Tbz(wraw, 31, &positive);
	armAsm->Orr(wflags, wflags, FPUflagI | FPUflagSI);
	armAsm->Bind(&positive);
	emitLoadFpuDouble(RSSCRATCH, EE_FPR_OFFSET(ft));
	armAsm->Fabs(RSSCRATCH, RSSCRATCH);
	armAsm->Fsqrt(RSSCRATCH, RSSCRATCH);
	emitStoreClampedResult(RSSCRATCH, EE_FPR_OFFSET(fd), /*setFlags*/ false);
	armAsm->Str(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));

	armAsm->Bind(&done);
}

void armEmitRSQRT_S(u32 fd, u32 fs, u32 ft)
{
	const a64::Register wraw = a64::w9;
	const a64::Register wtmp = a64::w10;
	const a64::Register wflags = a64::w13;

	a64::Label nonzero, positive, done;

	armAsm->Ldr(wraw, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
	armAsm->Ldr(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
	armAsm->And(wflags, wflags, ~(FPUflagI | FPUflagD));
	armAsm->And(wtmp, wraw, kExpMask);
	armAsm->Cbnz(wtmp, &nonzero);

	// Interpreter RSQRT zero path: set D|SD and return sign(ft) | +fmax.
	armAsm->Orr(wflags, wflags, FPUflagD | FPUflagSD);
	armAsm->And(wtmp, wraw, kSignBit);
	armAsm->Orr(wtmp, wtmp, kPosFmax);
	armAsm->Str(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
	armAsm->Str(wtmp, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
	armAsm->B(&done);

	armAsm->Bind(&nonzero);
	armAsm->Tbz(wraw, 31, &positive);
	armAsm->Orr(wflags, wflags, FPUflagI | FPUflagSI);
	armAsm->Bind(&positive);
	emitLoadFpuDouble(RSSCRATCH, EE_FPR_OFFSET(fs));
	emitLoadFpuDouble(RSSCRATCH2, EE_FPR_OFFSET(ft));
	armAsm->Fabs(RSSCRATCH2, RSSCRATCH2);
	armAsm->Fsqrt(RSSCRATCH2, RSSCRATCH2);
	armAsm->Fdiv(RSSCRATCH, RSSCRATCH, RSSCRATCH2);
	emitStoreClampedResult(RSSCRATCH, EE_FPR_OFFSET(fd), /*setFlags*/ false);
	armAsm->Str(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));

	armAsm->Bind(&done);
}

// ------------------------------------------------------------------------
// MADD/MSUB (-> fpr[fd]) and MADDA/MSUBA (-> ACC). The interpreter has a subtle
// asymmetry that we reproduce exactly:
//   MADD_S : temp = clamp(fs)*clamp(ft); fd = fpuDouble(ACC) (+/-) fpuDouble(temp)
//   MADDA_S: ACC.f (+/-)= clamp(fs)*clamp(ft)            (raw ACC, unclamped product)
// i.e. the fd-form re-clamps both the accumulator and the product before the add,
// while the ACC-form uses the raw stored ACC and the unclamped product. Both then
// run checkOverflow/checkUnderflow with the O|SO / U|SU flag side-effects.
static void emitFpuMulAcc(bool subtract, bool toAcc, u32 fd, u32 fs, u32 ft)
{
	// product = clamp(fs) * clamp(ft)  (single precision)
	emitLoadFpuDouble(RSSCRATCH, EE_FPR_OFFSET(fs));
	emitLoadFpuDouble(RSSCRATCH2, EE_FPR_OFFSET(ft));
	armAsm->Fmul(RSSCRATCH, RSSCRATCH, RSSCRATCH2);

	if (toAcc)
	{
		// MADDA/MSUBA: raw ACC (no clamp), unclamped product.
		armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_ACC_OFFSET));
		armAsm->Fmov(RSSCRATCH2, a64::w9);
	}
	else
	{
		// MADD/MSUB: re-clamp the product, then clamp the accumulator.
		armAsm->Fmov(a64::w9, RSSCRATCH);
		emitClampFpuDoubleBits(RSSCRATCH, a64::w9);
		emitLoadFpuDouble(RSSCRATCH2, EE_ACC_OFFSET);
	}

	// result = ACC (+/-) product  -> RSSCRATCH2 is the accumulator, RSSCRATCH the addend
	if (subtract)
		armAsm->Fsub(RSSCRATCH, RSSCRATCH2, RSSCRATCH);
	else
		armAsm->Fadd(RSSCRATCH, RSSCRATCH2, RSSCRATCH);

	emitStoreClampedResult(RSSCRATCH, toAcc ? EE_ACC_OFFSET : EE_FPR_OFFSET(fd), /*setFlags*/ true);
}

void armEmitMADD_S(u32 fd, u32 fs, u32 ft) { emitFpuMulAcc(/*sub*/ false, /*toAcc*/ false, fd, fs, ft); }
void armEmitMSUB_S(u32 fd, u32 fs, u32 ft) { emitFpuMulAcc(/*sub*/ true, /*toAcc*/ false, fd, fs, ft); }
void armEmitMADDA_S(u32 fs, u32 ft) { emitFpuMulAcc(/*sub*/ false, /*toAcc*/ true, 0, fs, ft); }
void armEmitMSUBA_S(u32 fs, u32 ft) { emitFpuMulAcc(/*sub*/ true, /*toAcc*/ true, 0, fs, ft); }

// ------------------------------------------------------------------------
// MAX_S/MIN_S: integer-domain fp_max/fp_min on the raw bit patterns (no fpuDouble
// clamp, no rounding), then clear the O|U cause flags. The interpreter:
//   fp_max(a,b) = (s32a<0 && s32b<0) ? min<s32>(a,b) : max<s32>(a,b)
//   fp_min(a,b) = (s32a<0 && s32b<0) ? max<s32>(a,b) : min<s32>(a,b)
// "both negative" is detected by sign bit 31 of (a & b).
static void emitFpuMinMax(bool isMax, u32 fd, u32 fs, u32 ft)
{
	const a64::Register wa = a64::w9;
	const a64::Register wb = a64::w10;
	const a64::Register wmax = a64::w11;
	const a64::Register wmin = a64::w12;
	const a64::Register wtmp = a64::w13;

	armAsm->Ldr(wa, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
	armAsm->Ldr(wb, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
	armAsm->Cmp(wa, wb); // signed
	armAsm->Csel(wmax, wa, wb, a64::gt);
	armAsm->Csel(wmin, wa, wb, a64::lt);
	// both negative <=> bit 31 of (a & b) set -> Tst leaves NE in that case
	armAsm->And(wtmp, wa, wb);
	armAsm->Tst(wtmp, kSignBit);
	if (isMax)
		armAsm->Csel(wa, wmin, wmax, a64::ne); // both neg -> min, else max
	else
		armAsm->Csel(wa, wmax, wmin, a64::ne); // both neg -> max, else min
	armAsm->Str(wa, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
	emitClearFCR31Flags(FPUflagO | FPUflagU);
}

void armEmitMAX_S(u32 fd, u32 fs, u32 ft) { emitFpuMinMax(/*isMax*/ true, fd, fs, ft); }
void armEmitMIN_S(u32 fd, u32 fs, u32 ft) { emitFpuMinMax(/*isMax*/ false, fd, fs, ft); }
