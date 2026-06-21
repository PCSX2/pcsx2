// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU harness self-validation. These smoke tests confirm that
// VuSnapshot / VuTestHarness / VuEncode round-trip cleanly through
// the interpreter — the foundation the DiffJitVsInterp suites
// will rest on. Each test runs the same program twice through the
// interpreter from identical pre-state and expects zero divergence;
// any diff means the harness itself is wrong.

#include "harness/VuTestHarness.h"

#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

TEST(VuHarness, EBitNopProgramTerminatesOnVu0)
{
	VuTestHarness h(0);
	h.LoadProgram({EBitNopPair()});
	h.Run();
	// REG_TPC after Execute is in pair-index form (Execute reverses the
	// internal <<= 3 with a >>= 3 on exit). Two pairs executed (the user-
	// supplied E-bit pair + the harness-appended delay-slot NOP) means
	// TPC advances from 0 to 2.
	EXPECT_EQ(h.GetViInterp(REG_TPC), 2u);
	EXPECT_TRUE(h.HasTerminated());
}

TEST(VuHarness, EBitNopProgramTerminatesOnVu1)
{
	VuTestHarness h(1);
	h.LoadProgram({EBitNopPair()});
	h.Run();
	EXPECT_EQ(h.GetViInterp(REG_TPC), 2u);
	EXPECT_TRUE(h.HasTerminated());
}

TEST(VuHarness, VfPreStateSurvivesNopProgramVu0)
{
	VuTestHarness h(0);
	h.SetVf(5, 1.5f, -2.25f, 3.75f, 0.5f);
	h.LoadProgram({EBitNopPair()});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfInterp(5, 'x'),  1.5f);
	EXPECT_FLOAT_EQ(h.GetVfInterp(5, 'y'), -2.25f);
	EXPECT_FLOAT_EQ(h.GetVfInterp(5, 'z'),  3.75f);
	EXPECT_FLOAT_EQ(h.GetVfInterp(5, 'w'),  0.5f);
}

TEST(VuHarness, ViPreStateSurvivesNopProgramVu0)
{
	VuTestHarness h(0);
	h.SetVi(7, 0xABCD);
	h.LoadProgram({EBitNopPair()});
	h.Run();
	EXPECT_EQ(h.GetViInterp(7), 0xABCDu);
}

TEST(VuHarness, MemoryWindowRoundTripsVu0)
{
	VuTestHarness h(0);
	h.WriteMemU32(0x100, 0xDEADBEEFu);
	h.TrackMemWindow(0x100, 4);
	h.LoadProgram({EBitNopPair()});
	h.Run();
	EXPECT_EQ(h.GetMemU32Interp(0x100), 0xDEADBEEFu);
}

TEST(VuHarness, MemoryWindowRoundTripsVu1)
{
	VuTestHarness h(1);
	h.WriteMemU128(0x200, 0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u);
	h.TrackMemWindow(0x200, 16);
	h.LoadProgram({EBitNopPair()});
	h.Run();
	EXPECT_EQ(h.GetMemU32Interp(0x200 + 0),  0x11111111u);
	EXPECT_EQ(h.GetMemU32Interp(0x200 + 4),  0x22222222u);
	EXPECT_EQ(h.GetMemU32Interp(0x200 + 8),  0x33333333u);
	EXPECT_EQ(h.GetMemU32Interp(0x200 + 12), 0x44444444u);
}

TEST(VuHarness, VaddProducesCorrectSumOnVu0)
{
	// VF[3] ← VF[1] + VF[2] across all four lanes — primitive test that
	// VuEncode's upper-pipe FMAC encoder, the interpreter dispatch, and
	// the snapshot/diff machinery all line up.
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(2, 0.5f, 0.25f, 0.125f, 0.0625f);
	h.LoadProgram({
		// Pair 0: VADD.xyzw vf3, vf1, vf2 (upper) | I-bit-skip lower
		IBit(VuOp{VLitZero(), VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)}),
		// Pair 1: E-bit NOP — the architectural delay slot.
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfInterp(3, 'x'), 1.5f);
	EXPECT_FLOAT_EQ(h.GetVfInterp(3, 'y'), 2.25f);
	EXPECT_FLOAT_EQ(h.GetVfInterp(3, 'z'), 3.125f);
	EXPECT_FLOAT_EQ(h.GetVfInterp(3, 'w'), 4.0625f);
}

TEST(VuHarness, IaddiuMutatesViOnVu0)
{
	// VI[2] ← VI[1] + 0x100. Validates the lower-pipe IADDIU encoder
	// (split immediate field) end-to-end through the interpreter.
	VuTestHarness h(0);
	h.SetVi(1, 0x10);
	h.LoadProgram({
		VuOp{VIADDIU_L(vi::vi2, vi::vi1, 0x100), VNOP_U()},
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViInterp(2), 0x110u);
}

TEST(VuHarness, IaddiuMutatesViOnVu1)
{
	VuTestHarness h(1);
	h.SetVi(3, 0x80);
	h.LoadProgram({
		VuOp{VIADDIU_L(vi::vi4, vi::vi3, 0x4), VNOP_U()},
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViInterp(4), 0x84u);
}

} // namespace recompiler_tests
