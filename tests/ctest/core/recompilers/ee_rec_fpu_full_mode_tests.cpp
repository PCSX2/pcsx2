// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// FPU "Full" / DOUBLE-precision mode coverage (CHECK_FPU_FULL, GameDB
// eeClampMode:3 — FFX, Max Payne, Dark Cloud 2, Klonoa 2, ~150 serials).
//
// These are JIT-ONLY tests. The shared interpreter (FPU.cpp `fpuDouble`) is
// single-precision and has no double path, so it cannot be the oracle: for the
// inputs that exercise the DOUBLE path the JIT legitimately diverges from the
// interp. Each test therefore uses RunJitNoDiff() and asserts GetFprBitsJit()
// against an independently hand-computed PS2 double-mode result.
//
// CAUTION for future test authors: RunJitNoDiff() sets interp_snapshot_ =
// jit_snapshot_ (the interp is not a valid oracle here). So in THIS file the
// interp-side accessors mirror the JIT — an EXPECT against InterpSnapshot() or
// a both-sides h.ExpectFpr() would pass tautologically. Assert only via the
// *Jit() accessors (GetFprBitsJit / GetAccBitsJit / JitSnapshot).
//
// The discriminator between full and fast mode is a PS2 "pseudo-infinity"
// operand (exp field 0xff, e.g. 0x7f800000 = a finite 2^128-scale number):
// full mode preserves it as 0x7f800000 (ToDouble complex path -> op ->
// ToPS2FPU to_complex path), while the single-precision fast path treats it as
// +Inf and clamps it to 0x7f7fffff. The PseudoInf* tests pin that the DOUBLE
// dispatch is taken: the fast-path value would fail them.

#include "harness/EeRecTestHarness.h"

#include "Config.h"

#include <cstring>
#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;
using namespace mips::ee;

namespace {
u32 FloatBits(float f)
{
	u32 bits;
	std::memcpy(&bits, &f, sizeof(bits));
	return bits;
}

constexpr u32 kFPUflagO  = 0x00008000;
constexpr u32 kFPUflagSO = 0x00000010;

// A PS2 single with exponent field 0xff is a valid finite number (1.0 * 2^128),
// not an IEEE infinity. Full mode must preserve it through an arithmetic op.
constexpr u32 kPs2HugePos = 0x7f800000; // +1.0 * 2^128
constexpr u32 kPs2MaxPos  = 0x7f7fffff; // +FLT_MAX (what the fast path clamps to)
} // namespace

// ---- Normal-range arithmetic: the DOUBLE pipeline must not corrupt ordinary
//      values (widen -> op -> narrow round-trips exactly for these). ----------

TEST(EeRecFpuFull, AddNormalRange)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetFprBits(0, FloatBits(2.5f));
	h.SetFprBits(1, FloatBits(1.25f));
	h.LoadProgram({ADD_S(2, 0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), FloatBits(3.75f));
}

TEST(EeRecFpuFull, SubNormalRange)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetFprBits(0, FloatBits(5.0f));
	h.SetFprBits(1, FloatBits(1.5f));
	h.LoadProgram({SUB_S(2, 0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), FloatBits(3.5f));
}

TEST(EeRecFpuFull, MulNormalRange)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetFprBits(0, FloatBits(3.0f));
	h.SetFprBits(1, FloatBits(4.0f));
	h.LoadProgram({MUL_S(2, 0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), FloatBits(12.0f));
}

// ---- Pseudo-infinity preservation: the strip-fix discriminator. ------------

TEST(EeRecFpuFull, AddPseudoInfPreserved)
{
	// 0x7f800000 + 0.0 : full mode keeps the PS2 2^128 value; the single-prec
	// fast path would treat it as +Inf and clamp to +FLT_MAX (0x7f7fffff).
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetFprBits(0, kPs2HugePos);
	h.SetFprBits(1, FloatBits(0.0f));
	h.LoadProgram({ADD_S(2, 0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), kPs2HugePos);
	EXPECT_NE(h.GetFprBitsJit(2), kPs2MaxPos); // would be this on the fast path
}

TEST(EeRecFpuFull, SubPseudoInfPreserved)
{
	// 0x7f800000 - 0.0 : same preservation through the SUB path.
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetFprBits(0, kPs2HugePos);
	h.SetFprBits(1, FloatBits(0.0f));
	h.LoadProgram({SUB_S(2, 0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), kPs2HugePos);
}

// ---- Overflow clamp + sticky flags: ToPS2FPU_Full to_overflow path. --------

TEST(EeRecFpuFull, MulOverflowClampsAndSetsStickyFlags)
{
	// 1.0*2^127 (0x7f000000) * 8.0 = 2^130 > PS2 max -> clamp to the PS2 FPU
	// maximum and raise O|SO in FCR31. Note the full-mode max is 0x7fffffff
	// (exp 0xff is a *valid* PS2 exponent), NOT IEEE FLT_MAX 0x7f7fffff — the
	// fast single-precision path clamps to 0x7f7fffff and never touches FCR31,
	// so both the value and the O flag are full-mode discriminators.
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetFcr31(0);
	h.SetFprBits(0, 0x7f000000u); // 1.0 * 2^127
	h.SetFprBits(1, FloatBits(8.0f));
	h.LoadProgram({MUL_S(2, 0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), 0x7fffffffu); // PS2 FPU max (not FLT_MAX)
	EXPECT_NE(h.GetFprBitsJit(2), kPs2MaxPos);  // fast path would give this
	EXPECT_NE(h.JitSnapshot().fprs.fprc[31] & (kFPUflagO | kFPUflagSO), 0u);
}

// ---- Accumulator-target ops (ADDA/SUBA/MULA write ACC, not Fd). -------------

TEST(EeRecFpuFull, AddaPseudoInfPreservedToAcc)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetFprBits(0, kPs2HugePos);
	h.SetFprBits(1, FloatBits(0.0f));
	h.LoadProgram({ADDA_S(0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetAccBitsJit(), kPs2HugePos);
}

TEST(EeRecFpuFull, MulaNormalRangeToAcc)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetFprBits(0, FloatBits(2.0f));
	h.SetFprBits(1, FloatBits(3.0f));
	h.LoadProgram({MULA_S(0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetAccBitsJit(), FloatBits(6.0f));
}

TEST(EeRecFpuFull, SubaNormalRangeToAcc)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetFprBits(0, FloatBits(10.0f));
	h.SetFprBits(1, FloatBits(2.0f));
	h.LoadProgram({SUBA_S(0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetAccBitsJit(), FloatBits(8.0f));
}

// ---- MADD/MSUB family (Fd = ACC +/- Fs*Ft, two roundings) ------------------
//      DOUBLE recMaddsub: full multiply -> guard-mask ACC -> branch on product/
//      ACC overflow -> accumulate in double. ------------------------------------

TEST(EeRecFpuFull, MaddNormalRange)
{
	// 2.0 + 3.0*4.0 = 14.0
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetAccBits(FloatBits(2.0f));
	h.SetFprBits(0, FloatBits(3.0f));
	h.SetFprBits(1, FloatBits(4.0f));
	h.LoadProgram({MADD_S(2, 0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), FloatBits(14.0f));
}

TEST(EeRecFpuFull, MsubNormalRange)
{
	// 20.0 - 3.0*4.0 = 8.0
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetAccBits(FloatBits(20.0f));
	h.SetFprBits(0, FloatBits(3.0f));
	h.SetFprBits(1, FloatBits(4.0f));
	h.LoadProgram({MSUB_S(2, 0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), FloatBits(8.0f));
}

TEST(EeRecFpuFull, MaddPseudoInfProductPreserved)
{
	// ACC=0 + (1.0*2^128)*1.0 : the product is a PS2 pseudo-inf (0x7f800000).
	// Full mode preserves it through the multiply and the (0+x) accumulate;
	// the fast path would clamp the product to FLT_MAX (0x7f7fffff).
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetAccBits(FloatBits(0.0f));
	h.SetFprBits(0, kPs2HugePos); // 1.0 * 2^128
	h.SetFprBits(1, FloatBits(1.0f));
	h.LoadProgram({MADD_S(2, 0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), kPs2HugePos);
	EXPECT_NE(h.GetFprBitsJit(2), kPs2MaxPos);
}

TEST(EeRecFpuFull, MsubPseudoInfNegatesProduct)
{
	// 0.0 - (1.0*2^128)*1.0 = -(2^128) = 0xff800000 (negative pseudo-inf).
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetAccBits(FloatBits(0.0f));
	h.SetFprBits(0, kPs2HugePos);
	h.SetFprBits(1, FloatBits(1.0f));
	h.LoadProgram({MSUB_S(2, 0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), 0xff800000u);
}

TEST(EeRecFpuFull, MaddProductOverflowClampsAndSetsFlags)
{
	// (1.0*2^127)*8.0 = 2^130 overflows PS2 range -> the multiply saturates on
	// the product-overflow path: result is +PS2-max with O|SO set. (ACC=1.0 is
	// dominated by the saturated product either way, so this pins the clamp +
	// sticky flags, not the accumulate-skip itself.)
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetFcr31(0);
	h.SetAccBits(FloatBits(1.0f)); // dominated by the 2^130 product
	h.SetFprBits(0, 0x7f000000u);  // 1.0 * 2^127
	h.SetFprBits(1, FloatBits(8.0f));
	h.LoadProgram({MADD_S(2, 0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), 0x7fffffffu);
	EXPECT_NE(h.JitSnapshot().fprs.fprc[31] & (kFPUflagO | kFPUflagSO), 0u);
}

TEST(EeRecFpuFull, MaddaNormalRangeToAcc)
{
	// MADDA writes ACC: 1.0 + 2.0*3.0 = 7.0
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetAccBits(FloatBits(1.0f));
	h.SetFprBits(0, FloatBits(2.0f));
	h.SetFprBits(1, FloatBits(3.0f));
	h.LoadProgram({MADDA_S(0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetAccBitsJit(), FloatBits(7.0f));
}

TEST(EeRecFpuFull, MsubaNormalRangeToAcc)
{
	// MSUBA writes ACC: 10.0 - 2.0*3.0 = 4.0
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetAccBits(FloatBits(10.0f));
	h.SetFprBits(0, FloatBits(2.0f));
	h.SetFprBits(1, FloatBits(3.0f));
	h.LoadProgram({MSUBA_S(0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetAccBitsJit(), FloatBits(4.0f));
}

TEST(EeRecFpuFull, MaddaProductOverflowSetsAccflag)
{
	// MADDA with an overflowing product: ACC clamps to PS2-max and the sticky
	// ACCflag bit must be set so a *subsequent* op sees the saturated ACC. This
	// is the accumulator-overflow propagation path unique to the *A variants.
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuFullMode();
	h.SetFcr31(0);
	h.SetAccBits(FloatBits(1.0f));
	h.SetFprBits(0, 0x7f000000u); // 1.0 * 2^127
	h.SetFprBits(1, FloatBits(8.0f));
	h.LoadProgram({MADDA_S(0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetAccBitsJit(), 0x7fffffffu);
	EXPECT_NE(h.JitSnapshot().fprs.fprc[31] & (kFPUflagO | kFPUflagSO), 0u);
	EXPECT_NE(h.JitSnapshot().fprs.ACCflag & 1u, 0u);
}
