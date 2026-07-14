// SPDX-FileCopyrightText: 2026 yaps2
// SPDX-License-Identifier: GPL-3.0+

// FPU ADD/SUB guard-bit emulation (single-precision FAST path).
//
// The EE FPU has no guard bits to the right of the mantissa. On a subtraction
// (or a mixed-sign add) whose cancellation left-shifts the result, the bits that
// a compliant IEEE FPU would have carried in its guard positions must read as
// zero on PS2 hardware. The recompiler reproduces this by masking the low
// mantissa bits of the smaller-exponent operand by the exponent difference
// before the op - x86 FPU_ADD_SUB (iFPU.cpp:402, applied unconditionally in the
// fast path) and the arm64 fpuEmitGuardedAddSub (iFPU-arm64.cpp).
//
// THESE ARE JIT-ONLY TESTS. The shared interpreter's ADD_S/SUB_S (FPU.cpp) is a
// plain host float + float (fpuDouble() returns float and does no masking),
// so for guard-bit-sensitive inputs the interpreter produces the *bare* IEEE
// result, which is LESS hardware-accurate than the masked JIT result. The JIT
// therefore deliberately diverges from the interp here (like the DOUBLE-mode and
// FpuMulHack tests), so these use RunJitNoDiff() and assert GetFprBitsJit()
// against the masked value. Each test notes the bare (unpatched / interp) value
// it must NOT equal.
//
// Expected values were computed two independent ways and cross-checked (see the
// session scratchpad guardbit.c / vectors.c): the masking is a ~1 ULP effect,
// which is exactly the guard-bit magnitude. Values use a large operand minus a
// small operand with dirty low mantissa bits so the masked bits survive the
// cancellation into the result.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

#include <cstring>
#include <random>

using namespace recompiler_tests;
using namespace mips;

// diff = expd - expt = 2 -> mask the smaller operand's low (diff-1)=1 bit.
// 4.0 - (1.0 + 3ulp): masked keeps 0x3f800002 for the subtrahend.
//   masked 0x403fffff  vs  bare/interp 0x403ffffe.
TEST(EeRecFpuGuardBit, SubMasksOneGuardBit)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(1, 0x40800000u); // 4.0
	h.SetFprBits(2, 0x3f800003u); // 1.0 + 3ulp (dirty low bits)
	h.LoadProgram({ee::SUB_S(3, 1, 2)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(3), 0x403fffffu); // masked; bare would be 0x403ffffe
}

// diff = 3 -> mask low 2 bits. 8.0 - (1.0 + 15ulp).
//   masked 0x40dffffd  vs  bare 0x40dffffc.
TEST(EeRecFpuGuardBit, SubMasksTwoGuardBits)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(1, 0x41000000u); // 8.0
	h.SetFprBits(2, 0x3f80000fu); // 1.0 + 15ulp
	h.LoadProgram({ee::SUB_S(3, 1, 2)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(3), 0x40dffffdu); // masked; bare 0x40dffffc
}

// diff = 5 -> mask low 4 bits. 32.0 - (1.0 + 63ulp).
//   masked 0x41f7fffd  vs  bare 0x41f7fffc.
TEST(EeRecFpuGuardBit, SubMasksFourGuardBits)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(1, 0x42000000u); // 32.0
	h.SetFprBits(2, 0x3f80003fu); // 1.0 + 63ulp
	h.LoadProgram({ee::SUB_S(3, 1, 2)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(3), 0x41f7fffdu); // masked; bare 0x41f7fffc
}

// Mixed-sign ADD is a cancelling subtraction, so the same guard mask applies.
// 4.0 + -(1.0 + 3ulp) has |diff|=2 -> mask the smaller operand's low bit.
//   masked 0x403fffff  vs  bare 0x403ffffe.
TEST(EeRecFpuGuardBit, AddMixedSignMasksGuardBit)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(1, 0x40800000u); // +4.0
	h.SetFprBits(2, 0xbf800003u); // -(1.0 + 3ulp)
	h.LoadProgram({ee::ADD_S(3, 1, 2)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(3), 0x403fffffu); // masked; bare 0x403ffffe
}

// The mask must apply on the ACC +/- product accumulate too (family-wide wiring,
// per x86 recMADDtemp/recMSUBtemp -> FPU_ADD/FPU_SUB). MSUB: fd = ACC - fs*ft.
// ACC=4.0, fs*ft = (1.0+3ulp)*1.0 = 1.0+3ulp exactly -> 4.0 - (1.0+3ulp), |diff|=2.
//   masked 0x403fffff  vs  bare 0x403ffffe.
TEST(EeRecFpuGuardBit, MsubMasksGuardBitOnAccumulate)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetAccBits(0x40800000u);    // ACC = 4.0
	h.SetFprBits(1, 0x3f800003u); // fs = 1.0 + 3ulp
	h.SetFprBits(2, 0x3f800000u); // ft = 1.0  -> product = 1.0 + 3ulp (exact)
	h.LoadProgram({ee::MSUB_S(3, 1, 2)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(3), 0x403fffffu); // masked; bare 0x403ffffe
}

// Boundary: |exp diff| = 1 masks zero bits, so the fast path must still match the
// interpreter exactly (no divergence). 2.0 - (1.0 + 3ulp): diff = 1.
// Uses Run()'s JIT-vs-interp auto-diff to pin that the guard change left the
// common (no-mask) path bit-identical to the interpreter.
TEST(EeRecFpuGuardBit, ExpDiffOneIsUnmaskedAndMatchesInterp)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(1, 0x40000000u); // 2.0
	h.SetFprBits(2, 0x3f800003u); // 1.0 + 3ulp
	h.LoadProgram({ee::SUB_S(3, 1, 2)});
	h.Run();
	h.ExpectFpr(3, 0x3f7ffffau); // == interp; no guard masking at diff==1
}

// Randomized differential test: the emitted guard masking must match the x86
// FPU_ADD_SUB model (iFPU.cpp:402) bit-for-bit on random operands. This pins the
// *emitted code*, not just the six hand-picked vectors above, and it is
// implementation-agnostic: it passes for the inline-branchy, shared-stub, and
// branchless-NEON emitters, so it catches any divergence between them.
//
// Operands are drawn from the guard-bit-sensitive region -- exponents spread
// across the whole |diff| range (including the |diff| >= 25 sign-only cliff and
// the |diff| <= 1 no-mask boundary) with dirty low mantissa bits, which is where
// masking is observable. Denormal/NaN/Inf exponents are excluded: the PS2 FPU has
// no such values, the fast path's result clamp folds them, and the interpreter
// disagrees there for reasons unrelated to guard bits.
namespace
{
	u32 ModelAddSub(u32 a, u32 b, bool issub)
	{
		const int expa = static_cast<int>((a >> 23) & 0xff);
		const int expb = static_cast<int>((b >> 23) & 0xff);
		const int d = expa - expb;
		if (d >= 25)
			b &= 0x80000000u;
		else if (d > 0)
			b &= (0xffffffffu << (d - 1));
		else if (d <= -25)
			a &= 0x80000000u;
		else if (d < 0)
			a &= (0xffffffffu << (-d - 1));

		float fa, fb;
		std::memcpy(&fa, &a, 4);
		std::memcpy(&fb, &b, 4);
		const float r = issub ? fa - fb : fa + fb;
		u32 rb;
		std::memcpy(&rb, &r, 4);
		return rb;
	}
} // namespace

TEST(EeRecFpuGuardBit, RandomizedMatchesX86Model)
{
	std::mt19937 rng(0xC0FFEE);
	std::uniform_int_distribution<u32> exp_dist(1, 254);
	std::uniform_int_distribution<u32> mant_dist(0, 0x7fffff);
	std::uniform_int_distribution<u32> sign_dist(0, 1);

	int checked = 0;
	for (int i = 0; i < 2000; i++)
	{
		const u32 a = (sign_dist(rng) << 31) | (exp_dist(rng) << 23) | mant_dist(rng);
		const u32 b = (sign_dist(rng) << 31) | (exp_dist(rng) << 23) | mant_dist(rng);

		// Skip operand pairs whose *result* leaves the PS2 range: the result clamp
		// (fpuClampResult) then dominates and the guard mask is unobservable.
		const u32 expect_add = ModelAddSub(a, b, false);
		const u32 expect_sub = ModelAddSub(a, b, true);
		if ((expect_add & 0x7f800000u) >= 0x7f800000u || (expect_sub & 0x7f800000u) >= 0x7f800000u)
			continue;

		{
			EeRecTestHarness h;
			h.EnableCop1();
			h.SetFprBits(1, a);
			h.SetFprBits(2, b);
			h.LoadProgram({ee::ADD_S(3, 1, 2)});
			h.RunJitNoDiff();
			ASSERT_EQ(h.GetFprBitsJit(3), expect_add)
				<< "ADD.S a=" << std::hex << a << " b=" << b;
		}
		{
			EeRecTestHarness h;
			h.EnableCop1();
			h.SetFprBits(1, a);
			h.SetFprBits(2, b);
			h.LoadProgram({ee::SUB_S(3, 1, 2)});
			h.RunJitNoDiff();
			ASSERT_EQ(h.GetFprBitsJit(3), expect_sub)
				<< "SUB.S a=" << std::hex << a << " b=" << b;
		}
		checked++;
	}
	EXPECT_GT(checked, 1500) << "too many pairs skipped; the test is not exercising the mask";
}
