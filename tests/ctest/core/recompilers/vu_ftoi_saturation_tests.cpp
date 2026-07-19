// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// FTOI{0,4,12,15} out-of-range / NaN saturation — DiffJitVsInterp.
//
// The PS2 VU float→int conversion saturates: the interp's floatToInt
// (VUops.cpp) returns 0x7FFFFFFF (INT_MAX) for a positive value whose exponent
// is >= that of 2^31 (which includes +Inf and any positive NaN), and
// 0x80000000 (INT_MIN) for the negative side. x86 PCSX2 mVU reproduces this via
// CVTTPS2DQ + a PCMP/PXOR correction.
//
// A bare FCVTZS saturates *finite* overflow and ±Inf correctly but converts
// NaN→0 — diverging from both the interp and x86. Real VU programs do feed
// positive NaN into FTOI0 (producing JIT=0 vs interp=0x7FFFFFFF). These tests
// pin the saturation contract so the arm64 emit matches the interp.

#include "harness/VuTestHarness.h"

#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

// I-bit set so the zero lower word is suppressed (becomes the VI[REG_I]
// immediate) rather than decoding as LQ vf0 — the canonical suppression idiom.
inline VuOp UpperOnly(u32 upper) { return IBit(VuOp{VLitZero(), upper}); }

// FTOI0 dst <- trunc(src). VFTOI0_U(mask, ft=dst, fs=src).
inline VuOp VFtoi0(u32 dst, u32 src) { return UpperOnly(VFTOI0_U(mask::xyzw, dst, src)); }

void RunAndExpectAllLanesAgree(VuTestHarness& h, u32 dst_vf)
{
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(dst_vf, 'x'), h.GetVfBitsInterp(dst_vf, 'x'));
	EXPECT_EQ(h.GetVfBitsJit(dst_vf, 'y'), h.GetVfBitsInterp(dst_vf, 'y'));
	EXPECT_EQ(h.GetVfBitsJit(dst_vf, 'z'), h.GetVfBitsInterp(dst_vf, 'z'));
	EXPECT_EQ(h.GetVfBitsJit(dst_vf, 'w'), h.GetVfBitsInterp(dst_vf, 'w'));
}

constexpr u32 kPosNan   = 0x7FFFFFFFu; // positive quiet NaN (sign 0, exp 0xFF)
constexpr u32 kNegNan   = 0xFFFFFFFFu; // negative NaN (sign 1)
constexpr u32 kPosInf   = 0x7F800000u;
constexpr u32 kNegInf   = 0xFF800000u;
constexpr u32 kPosBig   = 0x4F800000u; // 2^32, overflows s32 positive
constexpr u32 kNegBig   = 0xCF800000u; // -2^32
constexpr u32 kOne      = 0x3F800000u; // 1.0 → 1

} // namespace

// =========================================================================
//  NaN — interp/x86 give sign-based INT_MAX/INT_MIN; a bare FCVTZS gives 0.
//  JIT and interp must agree.
// =========================================================================

TEST(VuFtoiSaturation, PositiveNanSaturatesToIntMax)
{
	VuTestHarness h(0);
	h.SetVfBits(vf::vf2, kPosNan, kPosNan, kPosNan, kPosNan);
	h.LoadProgram({VFtoi0(vf::vf1, vf::vf2), EBitNopPair()});
	RunAndExpectAllLanesAgree(h, vf::vf1);
	// Pin the architectural value too (interp oracle): positive NaN → INT_MAX.
	EXPECT_EQ(h.GetVfBitsInterp(vf::vf1, 'x'), 0x7FFFFFFFu);
	EXPECT_EQ(h.GetVfBitsJit(vf::vf1, 'x'), 0x7FFFFFFFu);
}

TEST(VuFtoiSaturation, NegativeNanSaturatesToIntMin)
{
	VuTestHarness h(0);
	h.SetVfBits(vf::vf2, kNegNan, kNegNan, kNegNan, kNegNan);
	h.LoadProgram({VFtoi0(vf::vf1, vf::vf2), EBitNopPair()});
	RunAndExpectAllLanesAgree(h, vf::vf1);
	EXPECT_EQ(h.GetVfBitsInterp(vf::vf1, 'x'), 0x80000000u);
}

// =========================================================================
//  Finite overflow + Inf — FCVTZS handles these correctly; guard against
//  any NaN-path fix perturbing the finite-overflow behavior.
// =========================================================================

TEST(VuFtoiSaturation, PositiveOverflowAndInf)
{
	VuTestHarness h(0);
	h.SetVfBits(vf::vf2, kPosBig, kPosInf, kPosBig, kPosInf);
	h.LoadProgram({VFtoi0(vf::vf1, vf::vf2), EBitNopPair()});
	RunAndExpectAllLanesAgree(h, vf::vf1);
	EXPECT_EQ(h.GetVfBitsInterp(vf::vf1, 'x'), 0x7FFFFFFFu);
}

TEST(VuFtoiSaturation, NegativeOverflowAndInf)
{
	VuTestHarness h(0);
	h.SetVfBits(vf::vf2, kNegBig, kNegInf, kNegBig, kNegInf);
	h.LoadProgram({VFtoi0(vf::vf1, vf::vf2), EBitNopPair()});
	RunAndExpectAllLanesAgree(h, vf::vf1);
	EXPECT_EQ(h.GetVfBitsInterp(vf::vf1, 'x'), 0x80000000u);
}

// Mixed lanes — a NaN lane next to in-range lanes must not perturb the others.
TEST(VuFtoiSaturation, MixedNanAndInRange)
{
	VuTestHarness h(0);
	h.SetVfBits(vf::vf2, kPosNan, kOne, kNegNan, kOne);
	h.LoadProgram({VFtoi0(vf::vf1, vf::vf2), EBitNopPair()});
	RunAndExpectAllLanesAgree(h, vf::vf1);
	EXPECT_EQ(h.GetVfBitsInterp(vf::vf1, 'y'), 1u);
	EXPECT_EQ(h.GetVfBitsInterp(vf::vf1, 'w'), 1u);
}

} // namespace recompiler_tests
