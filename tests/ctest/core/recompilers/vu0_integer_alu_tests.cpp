// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU0 lower-pipe integer ALU DiffJitVsInterp suite. Covers the
// VI-bank arithmetic ops: VIADD/VISUB/VIAND/VIOR (LowerOP sub-table) and
// the immediate forms VIADDI (5-bit signed), VIADDIU/VISUBIU (15-bit
// unsigned). All VI registers are 16-bit on the architecture; the snapshot
// diffs the low 16 bits and ignores the hardwired-zero upper half.
//
// Edge cases under test:
// - Writes to VI[0] are silently dropped (hardwired-zero invariant).
// - Add/sub wrap behaviour at the 16-bit boundary.
// - VIADDI sign extension from a 5-bit immediate (range -16..15).
// - VIADDIU/VISUBIU 15-bit immediate split between bits[14:11] and
//   bits[10:0] (the encoder hides the split — encoder regression bait).
// - VIAND/VIOR operate on US[0] (zero-extended low 16) per VUops.cpp:1046.

#include "harness/VuTestHarness.h"

#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

inline VuOp LowerOnly(u32 lower) { return VuOp{lower, VNOP_U()}; }

} // namespace

// -------- VIADD --------

TEST(Vu0IntegerAlu, ViaddSimpleSignedSum)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 100);
	h.SetVi(vi::vi2, 250);
	h.LoadProgram({
		LowerOnly(VIADD_L(vi::vi3, vi::vi1, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 350u);
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
}

TEST(Vu0IntegerAlu, ViaddSignedNegativePlusPositive)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, static_cast<u32>(static_cast<s16>(-200)));
	h.SetVi(vi::vi2, 50);
	h.LoadProgram({
		LowerOnly(VIADD_L(vi::vi3, vi::vi1, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), static_cast<u32>(static_cast<s16>(-150)) & 0xFFFFu);
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
}

TEST(Vu0IntegerAlu, ViaddOverflowWrapsLow16)
{
	// 0x7FFF + 0x0010 = 0x800F, bit-15 overflow into negative.
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0x7FFFu);
	h.SetVi(vi::vi2, 0x0010u);
	h.LoadProgram({
		LowerOnly(VIADD_L(vi::vi3, vi::vi1, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x800Fu);
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
}

TEST(Vu0IntegerAlu, ViaddIntoVi0IsNoop)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 100);
	h.SetVi(vi::vi2, 50);
	h.LoadProgram({
		LowerOnly(VIADD_L(vi::vi0, vi::vi1, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi0), 0u);
	EXPECT_EQ(h.GetViInterp(vi::vi0), 0u);
}

// -------- VISUB --------

TEST(Vu0IntegerAlu, VisubBasicDifference)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 1000);
	h.SetVi(vi::vi2, 250);
	h.LoadProgram({
		LowerOnly(VISUB_L(vi::vi3, vi::vi1, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 750u);
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
}

TEST(Vu0IntegerAlu, VisubUnderflowWrapsLow16)
{
	// 0 - 1 = -1 -> 0xFFFF (16-bit two's complement).
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0);
	h.SetVi(vi::vi2, 1);
	h.LoadProgram({
		LowerOnly(VISUB_L(vi::vi3, vi::vi1, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0xFFFFu);
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
}

TEST(Vu0IntegerAlu, VisubChainsCorrectly)
{
	// vi3 = vi1 - vi2; vi4 = vi3 - vi1
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 500);
	h.SetVi(vi::vi2, 200);
	h.LoadProgram({
		LowerOnly(VISUB_L(vi::vi3, vi::vi1, vi::vi2)),
		LowerOnly(VISUB_L(vi::vi4, vi::vi3, vi::vi1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 300u);
	EXPECT_EQ(h.GetViJit(vi::vi4), static_cast<u32>(static_cast<s16>(-200)) & 0xFFFFu);
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
	EXPECT_EQ(h.GetViJit(vi::vi4), h.GetViInterp(vi::vi4));
}

// -------- VIAND --------

TEST(Vu0IntegerAlu, ViandBitwiseAnd)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0xF0F0u);
	h.SetVi(vi::vi2, 0x0FFFu);
	h.LoadProgram({
		LowerOnly(VIAND_L(vi::vi3, vi::vi1, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x00F0u);
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
}

TEST(Vu0IntegerAlu, ViandPreservesAllOnes)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0xABCDu);
	h.SetVi(vi::vi2, 0xFFFFu);
	h.LoadProgram({
		LowerOnly(VIAND_L(vi::vi3, vi::vi1, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0xABCDu);
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
}

// -------- VIOR --------

TEST(Vu0IntegerAlu, ViorBitwiseOr)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0x00F0u);
	h.SetVi(vi::vi2, 0x0F00u);
	h.LoadProgram({
		LowerOnly(VIOR_L(vi::vi3, vi::vi1, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x0FF0u);
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
}

TEST(Vu0IntegerAlu, ViorWithZeroIsIdentity)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0xBEEFu);
	h.SetVi(vi::vi2, 0x0000u);
	h.LoadProgram({
		LowerOnly(VIOR_L(vi::vi3, vi::vi1, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0xBEEFu);
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
}

// -------- VIADDI (5-bit signed immediate) --------

TEST(Vu0IntegerAlu, ViaddiPositiveSmallImm)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 100);
	h.LoadProgram({
		LowerOnly(VIADDI_L(vi::vi2, vi::vi1, 7)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi2), 107u);
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

TEST(Vu0IntegerAlu, ViaddiNegativeImmSignExtends)
{
	// imm5 = -1 (0x1F) → sign-extends to -1, so vi2 = vi1 + (-1).
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 100);
	h.LoadProgram({
		LowerOnly(VIADDI_L(vi::vi2, vi::vi1, -1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi2), 99u);
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

TEST(Vu0IntegerAlu, ViaddiImmMinusSixteenBoundary)
{
	// imm5 = -16 = 0x10. _vuIADDI sign-extends bit-4 → 0xFFF0 → -16.
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 1000);
	h.LoadProgram({
		LowerOnly(VIADDI_L(vi::vi2, vi::vi1, -16)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi2), 984u);
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

TEST(Vu0IntegerAlu, ViaddiImmPlusFifteenBoundary)
{
	// imm5 = 0x0F = 15 (bit-4 clear → no sign extension).
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 1000);
	h.LoadProgram({
		LowerOnly(VIADDI_L(vi::vi2, vi::vi1, 15)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi2), 1015u);
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

TEST(Vu0IntegerAlu, ViaddiIntoVi0IsNoop)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 100);
	h.LoadProgram({
		LowerOnly(VIADDI_L(vi::vi0, vi::vi1, 5)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi0), 0u);
	EXPECT_EQ(h.GetViInterp(vi::vi0), 0u);
}

// -------- VIADDIU (15-bit unsigned immediate) --------

TEST(Vu0IntegerAlu, ViaddiuLowImmInLow11Bits)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 100);
	h.LoadProgram({
		LowerOnly(VIADDIU_L(vi::vi2, vi::vi1, 0x3FF)), // 1023, low-11
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi2), 100u + 0x3FFu);
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

TEST(Vu0IntegerAlu, ViaddiuHighImmInHi4Bits)
{
	// 0x7800 = 0b0111100000000000 — exercises only the bits[14:11] half.
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0);
	h.LoadProgram({
		LowerOnly(VIADDIU_L(vi::vi2, vi::vi1, 0x7800)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi2), 0x7800u);
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

TEST(Vu0IntegerAlu, ViaddiuMixedHighAndLowImmBits)
{
	// 0x5555 — spans hi4 + low11 of the encoder split. Encoder regression bait.
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0x0001u);
	h.LoadProgram({
		LowerOnly(VIADDIU_L(vi::vi2, vi::vi1, 0x5555)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi2), 0x5556u);
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

TEST(Vu0IntegerAlu, ViaddiuMaxImmIsZeroX7FFF)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0x0001u);
	h.LoadProgram({
		LowerOnly(VIADDIU_L(vi::vi2, vi::vi1, 0x7FFF)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi2), 0x8000u);
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

// -------- VISUBIU --------

TEST(Vu0IntegerAlu, VisubiuBasic)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 1000);
	h.LoadProgram({
		LowerOnly(VISUBIU_L(vi::vi2, vi::vi1, 250)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi2), 750u);
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

TEST(Vu0IntegerAlu, VisubiuLargeImm)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0x7FFFu);
	h.LoadProgram({
		LowerOnly(VISUBIU_L(vi::vi2, vi::vi1, 0x4321)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi2), 0x7FFFu - 0x4321u);
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

TEST(Vu0IntegerAlu, VisubiuUnderflowWraps)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 5);
	h.LoadProgram({
		LowerOnly(VISUBIU_L(vi::vi2, vi::vi1, 100)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi2), static_cast<u32>(static_cast<s16>(5 - 100)) & 0xFFFFu);
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

// -------- Mixed sequence smoke test --------

TEST(Vu0IntegerAlu, MixedSequenceBuildsExpectedValue)
{
	// vi2 = vi1 + 100        ; VIADDIU
	// vi3 = vi2 + (-7)       ; VIADDI
	// vi4 = vi3 & 0x00FF     ; VIAND  via vi5=0x00FF
	// vi6 = vi4 | 0x8000     ; VIOR   via vi7=0x8000
	// vi8 = vi6 - vi5        ; VISUB
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 1000);
	h.SetVi(vi::vi5, 0x00FFu);
	h.SetVi(vi::vi7, 0x8000u);
	h.LoadProgram({
		LowerOnly(VIADDIU_L(vi::vi2, vi::vi1, 100)),
		LowerOnly(VIADDI_L (vi::vi3, vi::vi2, -7)),
		LowerOnly(VIAND_L  (vi::vi4, vi::vi3, vi::vi5)),
		LowerOnly(VIOR_L   (vi::vi6, vi::vi4, vi::vi7)),
		LowerOnly(VISUB_L  (vi::vi8, vi::vi6, vi::vi5)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi2), 1100u);
	EXPECT_EQ(h.GetViJit(vi::vi3), 1093u);
	EXPECT_EQ(h.GetViJit(vi::vi4), 1093u & 0x00FFu);
	EXPECT_EQ(h.GetViJit(vi::vi6), (1093u & 0x00FFu) | 0x8000u);
	EXPECT_EQ(h.GetViJit(vi::vi8), (((1093u & 0x00FFu) | 0x8000u) - 0x00FFu) & 0xFFFFu);

	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
	EXPECT_EQ(h.GetViJit(vi::vi4), h.GetViInterp(vi::vi4));
	EXPECT_EQ(h.GetViJit(vi::vi6), h.GetViInterp(vi::vi6));
	EXPECT_EQ(h.GetViJit(vi::vi8), h.GetViInterp(vi::vi8));
}

// =========================================================================
//  ISW must store the wrapped 16-bit VI value after in-place arithmetic.
//
//  VI registers are 16-bit architecturally. After an in-place RMW that wraps
//  the 16-bit boundary (e.g. VIADDI of 0xFFFF + 1 = 0x10000), a subsequent
//  ISW must store the wrapped 16-bit value (0x0000), not the wider 32-bit
//  intermediate. Storing the wider value corrupts the destination word — in
//  real microcode that lands in GIFtag PRIM/NREG fields, which the GIF then
//  rejects, silently dropping entire packets.
//
//  Trigger:
//    1. VIAND vi3, vi2, vi1     ; bring vi1 into the VI register state
//    2. VIADDI vi1, vi1, +1     ; in-place RMW; 0xFFFF + 1 = 0x10000
//    3. ISW.x vi1, 0(vi0)       ; must store 0x00000000, not 0x00010000
// =========================================================================

TEST(Vu0IntegerAlu, IswReadsZeroExtendedAfterInPlaceArithmetic)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0xFFFFu);
	h.SetVi(vi::vi2, 0x00FFu);
	h.WriteMemU32(0, 0xDEADBEEFu); // sentinel — both engines should overwrite
	h.LoadProgram({
		LowerOnly(VIAND_L (vi::vi3, vi::vi2, vi::vi1)), // bring vi1 into play
		LowerOnly(VIADDI_L(vi::vi1, vi::vi1, 1)),       // in-place RMW; wraps 16 bits
		LowerOnly(VISW_L  (mask::x, vi::vi1, vi::vi0, 0)),
		EBitNopPair(),
	});
	h.Run();
	// Architectural vi1 = (0xFFFF + 1) & 0xFFFF = 0x0000 → ISW writes 0.
	EXPECT_EQ(h.GetViJit(vi::vi1), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
	EXPECT_EQ(h.GetMemU32Jit(0), 0u);
	EXPECT_EQ(h.GetMemU32Jit(0), h.GetMemU32Interp(0));
}

} // namespace recompiler_tests
