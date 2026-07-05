// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
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

#include "Config.h"
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
static constexpr u32 FPUflagC = 0x00800000;  // compare condition bit

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
void armEmitLWC1(u32 ft, u32 rs, s32 imm, u32 pc)
{
	armEmitEffectiveAddr(RWARG1, rs, imm);
	// Single-instruction backpatch fastmem 32-bit load into RXRET; falls back to the vtlb
	// helper for faulting PCs / when fastmem is off. LWC1 stages the FP value through a GPR,
	// so the memory access is a plain 32-bit integer access (is_fpr=false).
	if (!armTryEmitFastmemScalar32(pc, /*is_load*/ true, RXRET))
		armEmitVtlbRead(32, /*sign*/ false, RXRET, RWARG1);
	armAsm->Str(RWRET, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
}

// ------------------------------------------------------------------------
// SWC1: mem32[GPR[rs] + imm] = fpr[ft].UL.
void armEmitSWC1(u32 ft, u32 rs, s32 imm, u32 pc)
{
	armAsm->Ldr(RWARG2, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
	armEmitEffectiveAddr(RWARG1, rs, imm);
	// Single-instruction backpatch fastmem 32-bit store of the FPR value (in RWARG2); falls
	// back to the vtlb helper for faulting PCs / when fastmem is off.
	if (!armTryEmitFastmemScalar32(pc, /*is_load*/ false, RWARG2))
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

// ========================================================================
// Full clamp mode (eeClampMode 3 / fpuFullMode) — faithful port of the x86
// DOUBLE path (pcsx2/x86/iFPUd.cpp).
//
// The PS2 FPU has NO inf/NaN: a single with exponent 0xFF is just a very large
// *finite* number. The default (single-precision) path mirrors the interpreter,
// which clamps those to ±fmax via fpuDouble() *before* every op — fine for most
// games, but it throws away magnitude. Full mode instead promotes operands to
// IEEE double WITHOUT that clamp (emitToDouble), so over-range intermediates
// survive the computation as real numbers, then converts the result back with
// proper overflow/underflow thresholds (emitToPS2FPUFull). Games whose GameIndex
// sets eeClampMode=3 (e.g. NFS Carbon) depend on this.
// ========================================================================

// emitToDoubleFromBits: PS2 single bits already in `wbits` -> IEEE double in dstD,
// with NO fmax clamp. exp != 0xFF converts exactly (incl. denormals/zero). exp ==
// 0xFF is reconstructed as the equivalent large finite double: sign<<63 | 1151<<52
// | mant<<29 (mirrors x86 ToDouble's lower-exp / convert / raise-exp dance).
// Clobbers wbits (and its X alias) plus the scratch X reg `xtmp`. dstD must not be
// d29/d31; callers pass RDSCRATCH/RDSCRATCH2.
static void emitToDoubleFromBits(const a64::VRegister& dstD, const a64::Register& wbits,
	const a64::Register& xtmp)
{
	const a64::Register xbits = wbits.X();

	a64::Label special, done;

	armAsm->Ubfx(xtmp.W(), wbits, 23, 8);
	armAsm->Cmp(xtmp.W(), 0xFF);
	armAsm->B(&special, a64::eq);

	// Normal / denormal / zero: single -> double is exact.
	armAsm->Fmov(dstD.S(), wbits);
	armAsm->Fcvt(dstD, dstD.S());
	armAsm->B(&done);

	armAsm->Bind(&special);
	armAsm->Ubfx(xtmp, xbits, 0, 23); // mantissa
	armAsm->Lsl(xtmp, xtmp, 29);
	armAsm->And(xbits, xbits, 0x80000000); // sign bit (bit31)
	armAsm->Lsl(xbits, xbits, 32);         // -> bit63
	armAsm->Orr(xbits, xbits, xtmp);
	armAsm->Mov(xtmp, 0x47F0000000000000); // 1151 << 52
	armAsm->Orr(xbits, xbits, xtmp);
	armAsm->Fmov(dstD, xbits);

	armAsm->Bind(&done);
}

// emitToDouble: load PS2 single at byteOffset and convert (no clamp). Uses w9/x9
// and x11 as scratch.
static void emitToDouble(const a64::VRegister& dstD, u32 byteOffset)
{
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, byteOffset));
	emitToDoubleFromBits(dstD, a64::w9, a64::x11);
}

// emitFpuAddSub: x86 FPU_ADD_SUB (iFPUd.cpp) ported in-place on the two raw single
// bit-patterns wA/wB. The EE FPU lacks IEEE guard bits, so before an add/sub the
// low mantissa bits of the *smaller* operand (the one shifted right during
// alignment) are masked off according to the exponent difference. Runs on the raw
// bits BEFORE emitToDoubleFromBits (an exp-0xFF operand isn't IEEE-inf here, it's a
// real number). Scratch: w11-w14.
static void emitFpuAddSub(const a64::Register& wA, const a64::Register& wB)
{
	const a64::Register wdiff = a64::w11;
	const a64::Register wmask = a64::w12;
	const a64::Register wexpA = a64::w13;
	const a64::Register wexpB = a64::w14;

	a64::Label bigPos, posDiff, bigNeg, done;

	armAsm->Ubfx(wexpA, wA, 23, 8);
	armAsm->Ubfx(wexpB, wB, 23, 8);
	armAsm->Sub(wdiff, wexpA, wexpB); // signed exponent difference

	armAsm->Cmp(wdiff, 25);
	armAsm->B(&bigPos, a64::ge); // expA >> expB: flush B to its sign
	armAsm->Cmp(wdiff, 0);
	armAsm->B(&posDiff, a64::gt); // 1..24: mask low bits of B
	armAsm->B(&done, a64::eq);    // equal exponents: nothing to mask
	armAsm->Cmp(wdiff, -25);
	armAsm->B(&bigNeg, a64::le); // expB >> expA: flush A to its sign

	// diff in -24..-1: mask low (|diff|-1) bits of A.
	armAsm->Neg(wdiff, wdiff);
	armAsm->Sub(wdiff, wdiff, 1);
	armAsm->Mov(wmask, 0xffffffff);
	armAsm->Lsl(wmask, wmask, wdiff);
	armAsm->And(wA, wA, wmask);
	armAsm->B(&done);

	armAsm->Bind(&bigPos);
	armAsm->And(wB, wB, kSignBit);
	armAsm->B(&done);

	armAsm->Bind(&posDiff);
	armAsm->Sub(wdiff, wdiff, 1);
	armAsm->Mov(wmask, 0xffffffff);
	armAsm->Lsl(wmask, wmask, wdiff);
	armAsm->And(wB, wB, wmask);
	armAsm->B(&done);

	armAsm->Bind(&bigNeg);
	armAsm->And(wA, wA, kSignBit);

	armAsm->Bind(&done);
}

// emitToPS2FPUFullCore: IEEE double result in srcD -> PS2 single bits left in w9,
// with the EE overflow/underflow behaviour (x86 ToPS2FPU_Full). When setFlags,
// FCR31 is updated and stored: O|U cleared up front, then O|SO on true overflow /
// U|SU on underflow. When also `acc` (the op writes ACC: ADDA/SUBA/MULA and the
// MADDA/MSUBA accumulate), fpuRegs.ACCflag bit0 is cleared up front and set on
// overflow — recMaddsub tests it to propagate an overflowed ACC. `addsub` selects
// the EE ADD/SUB underflow behaviour: the normalized mantissa bits are kept with
// exp=0 instead of being flushed (MUL/DIV-style ops flush to signed zero).
// srcD must be RDSCRATCH (d30); d29/d31 are used as scratch. Integer scratch: w9-w14.
static void emitToPS2FPUFullCore(const a64::VRegister& srcD, bool setFlags, bool acc, bool addsub)
{
	const a64::Register w = a64::w9;       // result single bits
	const a64::Register wtmp = a64::w10;
	const a64::Register wsign = a64::w11;  // aliases x11
	const a64::Register xbits = a64::x12;
	const a64::Register wflags = a64::w13;
	const a64::Register xc = a64::x14;
	const a64::VRegister absD = RDSCRATCH3; // d29
	const a64::VRegister dC = RDSCRATCH2;   // d31

	// Match x86 ToPS2FPU_Full: clear both O and U cause bits (and the ACC overflow
	// flag for ACC-writing ops) up front, then only set them on the relevant paths.
	if (setFlags)
	{
		armAsm->Ldr(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
		armAsm->And(wflags, wflags, ~(FPUflagO | FPUflagU));
		if (acc)
		{
			armAsm->Ldr(wtmp, a64::MemOperand(RESTATEPTR, EE_ACCFLAG_OFFSET));
			armAsm->And(wtmp, wtmp, ~1);
			armAsm->Str(wtmp, a64::MemOperand(RESTATEPTR, EE_ACCFLAG_OFFSET));
		}
	}

	armAsm->Fabs(absD, srcD);

	a64::Label toComplex, toOverflow, toUnderflow, uflush, done;

	// |x| >= 2^128 (single exp 0xFF territory) -> complex / overflow handling.
	armAsm->Mov(xc, 0x47F0000000000000); // 2^128
	armAsm->Fmov(dC, xc);
	armAsm->Fcmp(absD, dC);
	armAsm->B(&toComplex, a64::ge);

	// |x| < 2^-126 (smallest normal single) -> underflow / flush.
	armAsm->Mov(xc, 0x3810000000000000); // 2^-126
	armAsm->Fmov(dC, xc);
	armAsm->Fcmp(absD, dC);
	armAsm->B(&toUnderflow, a64::lt);

	// Normal range: plain double -> single. (O/U already cleared up front.)
	armAsm->Fcvt(srcD.S(), srcD);
	armAsm->Fmov(w, srcD.S());
	armAsm->B(&done);

	armAsm->Bind(&toComplex);
	// 2^128 <= |x| < 2^129 : representable as a PS2 exp-0xFF single (not overflow).
	armAsm->Mov(xc, 0x4800000000000000); // 2^129
	armAsm->Fmov(dC, xc);
	armAsm->Fcmp(absD, dC);
	armAsm->B(&toOverflow, a64::ge);
	armAsm->Fmov(xbits, srcD);
	armAsm->Mov(xc, 0x0010000000000000); // lower double exp by one
	armAsm->Sub(xbits, xbits, xc);
	armAsm->Fmov(dC, xbits);
	armAsm->Fcvt(dC.S(), dC);            // -> single, exp 0xFE
	armAsm->Fmov(w, dC.S());
	armAsm->Add(w, w, 0x00800000);       // raise single exp -> 0xFF (O/U already cleared)
	armAsm->B(&done);

	armAsm->Bind(&toOverflow);
	// True overflow: result = sign | 0x7FFFFFFF — the PS2 maximum (exp 0xFF, full
	// mantissa; x86 SetMaxValue / s_const.pos), NOT the IEEE fmax 0x7F7FFFFF.
	armAsm->Fmov(xbits, srcD);
	armAsm->Lsr(a64::x11, xbits, 32);
	armAsm->And(wsign, wsign, kSignBit);
	armAsm->Mov(w, 0x7FFFFFFF);
	armAsm->Orr(w, w, wsign);
	if (setFlags)
	{
		armAsm->Orr(wflags, wflags, FPUflagO | FPUflagSO);
		if (acc)
		{
			armAsm->Ldr(wtmp, a64::MemOperand(RESTATEPTR, EE_ACCFLAG_OFFSET));
			armAsm->Orr(wtmp, wtmp, 1);
			armAsm->Str(wtmp, a64::MemOperand(RESTATEPTR, EE_ACCFLAG_OFFSET));
		}
	}
	armAsm->B(&done);

	armAsm->Bind(&toUnderflow);
	// x86 tests the *double* against zero (the converted single could flush under
	// the host FZ bit and hide the underflow): exact zero -> plain convert, no
	// flags. Nonzero -> U|SU; ADD/SUB keep the normalized mantissa bits with exp=0
	// (the EE doesn't flush the mantissa on add/sub), other ops flush to signed 0.
	if (setFlags)
	{
		armAsm->Fcmp(srcD, 0.0);
		armAsm->B(&uflush, a64::eq);
		armAsm->Orr(wflags, wflags, FPUflagU | FPUflagSU);
		if (addsub)
		{
			armAsm->Fmov(xbits, srcD);
			armAsm->Ubfx(a64::x9, xbits, 29, 23); // double mantissa[51:29] -> single mantissa, exp=0
			armAsm->Lsr(xbits, xbits, 63);        // sign bit
			armAsm->Orr(a64::x9, a64::x9, a64::Operand(xbits, a64::LSL, 31));
			armAsm->B(&done);
		}
	}
	armAsm->Bind(&uflush);
	armAsm->Fcvt(srcD.S(), srcD);
	armAsm->Fmov(w, srcD.S());
	armAsm->And(w, w, kSignBit); // flush to signed zero

	armAsm->Bind(&done);
	if (setFlags)
		armAsm->Str(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
}

// emitToPS2FPUFull: core + store of the result single to the fpr/ACC slot.
static void emitToPS2FPUFull(const a64::VRegister& srcD, u32 dstByteOffset, bool setFlags,
	bool acc, bool addsub)
{
	emitToPS2FPUFullCore(srcD, setFlags, acc, addsub);
	armAsm->Str(a64::w9, a64::MemOperand(RESTATEPTR, dstByteOffset));
}

// Write a compile-time FPCR bitmask to the host FPCR (the x86 analogue is
// xLDMXCSR). The EE FPCR config is fixed for the lifetime of compiled blocks
// (recs are reset when CPU config changes), so embedding the constant is safe.
// Clobbers x16 (vixl scratch), like aVU's FPCR swap.
static void emitSetHostFPCR(u64 bitmask)
{
	armAsm->Mov(RXVIXLSCRATCH, bitmask);
	armAsm->Msr(a64::FPCR, RXVIXLSCRATCH);
}

enum class FpuBinOp
{
	Add,
	Sub,
	Mul
};

// Shared body for the ADD/SUB/MUL (-> fpr[fd]) and ADDA/SUBA/MULA (-> ACC) family.
// When fpuFullMode: promote both operands to IEEE double before the arithmetic, then
// convert back. This prevents intermediate overflow that single-precision can hit on
// games like NFS Carbon (eeClampMode=3 in GameIndex).
static void emitFpuBinary(FpuBinOp op, u32 dstByteOffset, u32 fs, u32 ft)
{
	if (EmuConfig.Cpu.Recompiler.fpuFullMode)
	{
		if (op == FpuBinOp::Mul)
		{
			emitToDouble(RDSCRATCH, EE_FPR_OFFSET(fs));   // d30 = ToDouble(fs), no clamp
			if (fs == ft)
			{
				// fs*fs (squares are common): one load+convert feeds both operands.
				armAsm->Fmul(RDSCRATCH, RDSCRATCH, RDSCRATCH);
			}
			else
			{
				emitToDouble(RDSCRATCH2, EE_FPR_OFFSET(ft)); // d31 = ToDouble(ft), no clamp
				armAsm->Fmul(RDSCRATCH, RDSCRATCH, RDSCRATCH2);
			}
		}
		else if (fs == ft)
		{
			// Equal operands: the guard-bit masking (emitFpuAddSub) is an exact no-op
			// (exponent difference 0 takes its untouched early-out), and both
			// conversions yield the same double — convert once and reuse it.
			armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
			emitToDoubleFromBits(RDSCRATCH, a64::w9, a64::x11);
			if (op == FpuBinOp::Add)
				armAsm->Fadd(RDSCRATCH, RDSCRATCH, RDSCRATCH);
			else
				armAsm->Fsub(RDSCRATCH, RDSCRATCH, RDSCRATCH);
		}
		else
		{
			// ADD/SUB: apply the EE guard-bit mantissa masking (x86 FPU_ADD_SUB) on
			// the raw single operands first, then promote and add/sub in double.
			armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
			armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
			emitFpuAddSub(a64::w9, a64::w10);
			emitToDoubleFromBits(RDSCRATCH, a64::w9, a64::x11);
			emitToDoubleFromBits(RDSCRATCH2, a64::w10, a64::x12);
			if (op == FpuBinOp::Add)
				armAsm->Fadd(RDSCRATCH, RDSCRATCH, RDSCRATCH2);
			else
				armAsm->Fsub(RDSCRATCH, RDSCRATCH, RDSCRATCH2);
		}
		// ADDA/SUBA/MULA write ACC and must track the ACC overflow flag; ADD/SUB get
		// the EE's underflow mantissa-preserve behaviour (x86 recFPUOp/FPU_MUL).
		emitToPS2FPUFull(RDSCRATCH, dstByteOffset, /*setFlags*/ true,
			/*acc*/ dstByteOffset == EE_ACC_OFFSET, /*addsub*/ op != FpuBinOp::Mul);
		return;
	}

	emitLoadFpuDouble(RSSCRATCH, EE_FPR_OFFSET(fs)); // s30 = fpuDouble(fs)
	if (fs != ft)
		emitLoadFpuDouble(RSSCRATCH2, EE_FPR_OFFSET(ft)); // s31 = fpuDouble(ft)
	const a64::VRegister& rhs = (fs == ft) ? RSSCRATCH : RSSCRATCH2;
	switch (op)
	{
		case FpuBinOp::Add: armAsm->Fadd(RSSCRATCH, RSSCRATCH, rhs); break;
		case FpuBinOp::Sub: armAsm->Fsub(RSSCRATCH, RSSCRATCH, rhs); break;
		case FpuBinOp::Mul: armAsm->Fmul(RSSCRATCH, RSSCRATCH, rhs); break;
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
	if (EmuConfig.Cpu.Recompiler.fpuFullMode)
	{
		// Faithful port of x86 recDIV_S_xmm/recDIVhelper1 (iFPUd.cpp). The whole op
		// (including the double conversions) runs under the dedicated DIV round mode
		// (FPUDivFPCR, default nearest), and I|D are cleared every DIV.
		const bool swapRound = EmuConfig.Cpu.FPUFPCR.bitmask != EmuConfig.Cpu.FPUDivFPCR.bitmask;
		if (swapRound)
			emitSetHostFPCR(EmuConfig.Cpu.FPUDivFPCR.bitmask);

		const a64::Register ws = a64::w9;
		const a64::Register wt = a64::w10;
		const a64::Register wtmp = a64::w11;
		const a64::Register wflags = a64::w13;

		a64::Label normal, fsZero, byZeroDone, end;

		armAsm->Ldr(ws, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
		armAsm->Ldr(wt, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
		armAsm->Ldr(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
		armAsm->And(wflags, wflags, ~(FPUflagI | FPUflagD));

		// Divisor zero/denormal (the x86 compare runs under DAZ, so exp==0 == zero).
		armAsm->And(wtmp, wt, kExpMask);
		armAsm->Cbnz(wtmp, &normal);

		// 0/0 -> I|SI, x/0 -> D|SD; result = sign(fs^ft) | 0x7FFFFFFF
		// (x86: regd ^= regt, then SetMaxValue ORs in 0x7FFFFFFF).
		armAsm->And(wtmp, ws, kExpMask);
		armAsm->Cbz(wtmp, &fsZero);
		armAsm->Orr(wflags, wflags, FPUflagD | FPUflagSD);
		armAsm->B(&byZeroDone);
		armAsm->Bind(&fsZero);
		armAsm->Orr(wflags, wflags, FPUflagI | FPUflagSI);
		armAsm->Bind(&byZeroDone);
		armAsm->Eor(wtmp, ws, wt);
		armAsm->And(wtmp, wtmp, kSignBit);
		armAsm->Orr(wtmp, wtmp, 0x7FFFFFFF);
		armAsm->Str(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
		armAsm->Str(wtmp, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
		armAsm->B(&end);

		armAsm->Bind(&normal);
		armAsm->Str(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
		emitToDoubleFromBits(RDSCRATCH, ws, a64::x11);
		emitToDoubleFromBits(RDSCRATCH2, wt, a64::x11);
		armAsm->Fdiv(RDSCRATCH, RDSCRATCH, RDSCRATCH2);
		emitToPS2FPUFull(RDSCRATCH, EE_FPR_OFFSET(fd), /*setFlags*/ false, false, false);

		armAsm->Bind(&end);
		if (swapRound)
			emitSetHostFPCR(EmuConfig.Cpu.FPUFPCR.bitmask);
		return;
	}

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
	if (EmuConfig.Cpu.Recompiler.fpuFullMode)
	{
		// Faithful port of x86 recSQRT_S_xmm (iFPUd.cpp): runs under round-to-nearest;
		// clears I|D; a negative input (including -0) sets I|SI and is made positive.
		// No zero/denormal shortcut — sqrt of a denormal goes through the double path
		// and underflows back to +0 in emitToPS2FPUFull.
		const bool swapRound = EmuConfig.Cpu.FPUFPCR.GetRoundMode() != FPRoundMode::Nearest;
		if (swapRound)
		{
			FPControlRegister nearest = EmuConfig.Cpu.FPUFPCR;
			nearest.SetRoundMode(FPRoundMode::Nearest);
			emitSetHostFPCR(nearest.bitmask);
		}

		const a64::Register wraw = a64::w9;
		const a64::Register wflags = a64::w13;

		a64::Label positive;

		armAsm->Ldr(wraw, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
		armAsm->Ldr(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
		armAsm->And(wflags, wflags, ~(FPUflagI | FPUflagD));
		armAsm->Tbz(wraw, 31, &positive);
		armAsm->Orr(wflags, wflags, FPUflagI | FPUflagSI);
		armAsm->And(wraw, wraw, ~kSignBit); // make positive
		armAsm->Bind(&positive);
		armAsm->Str(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));

		emitToDoubleFromBits(RDSCRATCH, wraw, a64::x11);
		armAsm->Fsqrt(RDSCRATCH, RDSCRATCH);
		emitToPS2FPUFull(RDSCRATCH, EE_FPR_OFFSET(fd), /*setFlags*/ false, false, false);

		if (swapRound)
			emitSetHostFPCR(EmuConfig.Cpu.FPUFPCR.bitmask);
		return;
	}

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
	if (EmuConfig.Cpu.Recompiler.fpuFullMode)
	{
		// Faithful port of x86 recRSQRT_S_xmm/recRSQRThelper1 (iFPUd.cpp): runs under
		// round-to-nearest; clears I|D; negative ft (incl. -0) sets I|SI and is made
		// positive; ft==0 (or denormal, DAZ semantics) sets I|SI on 0/0 else D|SD and
		// the result is sign(fs) | 0x7FFFFFFF (SetMaxValue on the untouched fs).
		const bool swapRound = EmuConfig.Cpu.FPUFPCR.GetRoundMode() != FPRoundMode::Nearest;
		if (swapRound)
		{
			FPControlRegister nearest = EmuConfig.Cpu.FPUFPCR;
			nearest.SetRoundMode(FPRoundMode::Nearest);
			emitSetHostFPCR(nearest.bitmask);
		}

		const a64::Register ws = a64::w9;
		const a64::Register wt = a64::w10;
		const a64::Register wtmp = a64::w11;
		const a64::Register wflags = a64::w13;

		a64::Label tPositive, tNonzero, fsZero, zeroDone, end;

		armAsm->Ldr(ws, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
		armAsm->Ldr(wt, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
		armAsm->Ldr(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
		armAsm->And(wflags, wflags, ~(FPUflagI | FPUflagD));

		armAsm->Tbz(wt, 31, &tPositive);
		armAsm->Orr(wflags, wflags, FPUflagI | FPUflagSI);
		armAsm->And(wt, wt, ~kSignBit); // make positive
		armAsm->Bind(&tPositive);

		armAsm->And(wtmp, wt, kExpMask);
		armAsm->Cbnz(wtmp, &tNonzero);

		// ft == 0: 0/0 -> I|SI, x/0 -> D|SD; result = sign(fs) | 0x7FFFFFFF.
		armAsm->And(wtmp, ws, kExpMask);
		armAsm->Cbz(wtmp, &fsZero);
		armAsm->Orr(wflags, wflags, FPUflagD | FPUflagSD);
		armAsm->B(&zeroDone);
		armAsm->Bind(&fsZero);
		armAsm->Orr(wflags, wflags, FPUflagI | FPUflagSI);
		armAsm->Bind(&zeroDone);
		armAsm->And(wtmp, ws, kSignBit);
		armAsm->Orr(wtmp, wtmp, 0x7FFFFFFF);
		armAsm->Str(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
		armAsm->Str(wtmp, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
		armAsm->B(&end);

		armAsm->Bind(&tNonzero);
		armAsm->Str(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
		emitToDoubleFromBits(RDSCRATCH2, wt, a64::x11); // d31 = ft
		emitToDoubleFromBits(RDSCRATCH, ws, a64::x11);  // d30 = fs
		armAsm->Fsqrt(RDSCRATCH2, RDSCRATCH2);
		armAsm->Fdiv(RDSCRATCH, RDSCRATCH, RDSCRATCH2);
		emitToPS2FPUFull(RDSCRATCH, EE_FPR_OFFSET(fd), /*setFlags*/ false, false, false);

		armAsm->Bind(&end);
		if (swapRound)
			emitSetHostFPCR(EmuConfig.Cpu.FPUFPCR.bitmask);
		return;
	}

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
	if (EmuConfig.Cpu.Recompiler.fpuFullMode)
	{
		// Faithful port of x86 recMaddsub (iFPUd.cpp). The product is computed in
		// double and rounded back to a PS2 single FIRST (setting the O/U flags), then
		// the accumulate runs with overflow propagation: a product overflow forces
		// ±MAX (sign-flipped for MSUB) and skips the add entirely; a previously
		// overflowed ACC (fpuRegs.ACCflag, set by the ACC-writing ops) forces the
		// clamped ACC. Only then is the add/sub done in double and re-rounded.
		const a64::Register wprod = a64::w9; // the core's result register
		const a64::Register wacc = a64::w10;
		const a64::Register wtmp = a64::w11;

		// FPU_MUL: product = ToPS2FPU(ToDouble(fs) * ToDouble(ft)) -> w9, flags set.
		emitToDouble(RDSCRATCH, EE_FPR_OFFSET(fs));
		if (fs == ft)
			armAsm->Fmul(RDSCRATCH, RDSCRATCH, RDSCRATCH); // fs*fs: reuse the conversion
		else
		{
			emitToDouble(RDSCRATCH2, EE_FPR_OFFSET(ft));
			armAsm->Fmul(RDSCRATCH, RDSCRATCH, RDSCRATCH2);
		}
		emitToPS2FPUFullCore(RDSCRATCH, /*setFlags*/ true, /*acc*/ false, /*addsub*/ false);

		armAsm->Ldr(wacc, a64::MemOperand(RESTATEPTR, EE_ACC_OFFSET));
		emitFpuAddSub(wacc, wprod); // EE guard-bit masking on (ACC, product)

		a64::Label mulOvf, ovfCommon, end;

		armAsm->Ldr(wtmp, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
		armAsm->Tst(wtmp, FPUflagO);
		armAsm->B(&mulOvf, a64::ne); // the product overflowed
		emitToDoubleFromBits(RDSCRATCH, wprod, a64::x11); // d30 = product
		armAsm->Ldr(wtmp, a64::MemOperand(RESTATEPTR, EE_ACCFLAG_OFFSET));
		armAsm->Tbnz(wtmp, 0, &ovfCommon); // ACC overflowed earlier -> clamped ACC
		emitToDoubleFromBits(RDSCRATCH2, wacc, a64::x11); // d31 = ACC
		if (subtract)
			armAsm->Fsub(RDSCRATCH, RDSCRATCH2, RDSCRATCH); // ACC - product
		else
			armAsm->Fadd(RDSCRATCH, RDSCRATCH2, RDSCRATCH); // ACC + product
		emitToPS2FPUFull(RDSCRATCH, toAcc ? EE_ACC_OFFSET : EE_FPR_OFFSET(fd),
			/*setFlags*/ true, /*acc*/ toAcc, /*addsub*/ true);
		armAsm->B(&end);

		armAsm->Bind(&mulOvf);
		if (subtract)
			armAsm->Eor(wprod, wprod, kSignBit); // MSUB propagates -product
		armAsm->Mov(wacc, wprod);
		armAsm->Bind(&ovfCommon);
		// Result = sign | 0x7FFFFFFF (x86 SetMaxValue); O|SO set, U cleared, and the
		// ACC-writing forms mark the ACC as overflowed.
		armAsm->And(wprod, wacc, kSignBit);
		armAsm->Orr(wprod, wprod, 0x7FFFFFFF);
		armAsm->Ldr(wtmp, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
		armAsm->And(wtmp, wtmp, ~(FPUflagO | FPUflagU));
		armAsm->Orr(wtmp, wtmp, FPUflagO | FPUflagSO);
		armAsm->Str(wtmp, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
		if (toAcc)
		{
			armAsm->Ldr(wtmp, a64::MemOperand(RESTATEPTR, EE_ACCFLAG_OFFSET));
			armAsm->Orr(wtmp, wtmp, 1);
			armAsm->Str(wtmp, a64::MemOperand(RESTATEPTR, EE_ACCFLAG_OFFSET));
		}
		armAsm->Str(wprod, a64::MemOperand(RESTATEPTR, toAcc ? EE_ACC_OFFSET : EE_FPR_OFFSET(fd)));
		armAsm->Bind(&end);
		return;
	}

	// product = clamp(fs) * clamp(ft)
	emitLoadFpuDouble(RSSCRATCH, EE_FPR_OFFSET(fs));
	if (fs == ft)
		armAsm->Fmul(RSSCRATCH, RSSCRATCH, RSSCRATCH); // fs*fs: reuse the clamped load
	else
	{
		emitLoadFpuDouble(RSSCRATCH2, EE_FPR_OFFSET(ft));
		armAsm->Fmul(RSSCRATCH, RSSCRATCH, RSSCRATCH2);
	}

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

// ------------------------------------------------------------------------
// C.EQ/C.LT/C.LE: set or clear the FCR31 C condition bit from a clamped compare.
//   _ContVal_ = (fpuDouble(fs) cond fpuDouble(ft)) ? (_ContVal_ | C) : (_ContVal_ & ~C)
// Both operands are fpuDouble-clamped (so finite, never NaN/inf), making the ARM
// float condition codes a direct match for the interpreter's C++ float comparison.
static void emitFpuCompare(a64::Condition cond, u32 fs, u32 ft)
{
	const a64::Register wflags = a64::w13;
	const a64::Register wset = a64::w14;
	const a64::Register wclr = a64::w15;

	if (EmuConfig.Cpu.Recompiler.fpuFullMode)
	{
		emitToDouble(RDSCRATCH, EE_FPR_OFFSET(fs));   // no fmax clamp
		emitToDouble(RDSCRATCH2, EE_FPR_OFFSET(ft));
		armAsm->Fcmp(RDSCRATCH, RDSCRATCH2);
	}
	else
	{
		emitLoadFpuDouble(RSSCRATCH, EE_FPR_OFFSET(fs));
		emitLoadFpuDouble(RSSCRATCH2, EE_FPR_OFFSET(ft));
		armAsm->Fcmp(RSSCRATCH, RSSCRATCH2);
	}
	armAsm->Ldr(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
	armAsm->Orr(wset, wflags, FPUflagC);
	armAsm->And(wclr, wflags, ~FPUflagC);
	armAsm->Csel(wflags, wset, wclr, cond);
	armAsm->Str(wflags, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
}

void armEmitC_F(u32 fs, u32 ft) { (void)fs; (void)ft; emitClearFCR31Flags(FPUflagC); }
void armEmitC_EQ(u32 fs, u32 ft) { emitFpuCompare(a64::eq, fs, ft); }
void armEmitC_LT(u32 fs, u32 ft) { emitFpuCompare(a64::lt, fs, ft); }
void armEmitC_LE(u32 fs, u32 ft) { emitFpuCompare(a64::le, fs, ft); }

// ------------------------------------------------------------------------
// CVT_W: float -> signed int32 with the EE's saturation, no fpuDouble clamp:
//   if (exp field <= 0x4E800000) fd = (s32)float (round toward zero)
//   else fd = (sign) ? 0x80000000 : 0x7fffffff
void armEmitCVT_W(u32 fd, u32 fs)
{
	const a64::Register w = a64::w9;
	const a64::Register wtmp = a64::w10;
	const a64::Register wcmp = a64::w11;
	const a64::Register wres = a64::w12;
	const a64::Register wneg = a64::w13;

	a64::Label convert, store;

	armAsm->Ldr(w, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
	armAsm->And(wtmp, w, kExpMask);
	armAsm->Mov(wcmp, 0x4E800000);
	armAsm->Cmp(wtmp, wcmp);
	armAsm->B(&convert, a64::ls); // in range (unsigned <=)

	// out of range -> saturate by sign
	armAsm->Mov(wres, 0x7fffffff);
	armAsm->Mov(wneg, 0x80000000);
	armAsm->Tst(w, kSignBit);
	armAsm->Csel(wres, wneg, wres, a64::ne);
	armAsm->B(&store);

	armAsm->Bind(&convert);
	armAsm->Fmov(RSSCRATCH, w);
	armAsm->Fcvtzs(wres, RSSCRATCH); // round toward zero, matches (s32)float

	armAsm->Bind(&store);
	armAsm->Str(wres, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
}

// ------------------------------------------------------------------------
// CVT_S: signed int32 (raw fpr bits) -> float. fd = (float)(s32)fpr[fs].
void armEmitCVT_S(u32 fd, u32 fs)
{
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
	armAsm->Scvtf(RSSCRATCH, a64::w9);
	armAsm->Fmov(a64::w9, RSSCRATCH);
	armAsm->Str(a64::w9, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
}
