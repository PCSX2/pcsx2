// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "harness/JitTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

TEST(IopShift, SllBasic)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x00000001u);
	h.LoadProgram({SLL(reg::v0, reg::a0, 4)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x00000010u);
}

TEST(IopShift, SllBy0IsMove)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xDEADBEEFu);
	h.LoadProgram({SLL(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xDEADBEEFu);
}

TEST(IopShift, SllBy31)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x00000001u);
	h.LoadProgram({SLL(reg::v0, reg::a0, 31)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x80000000u);
}

TEST(IopShift, SrlBasic)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x80000000u);
	h.LoadProgram({SRL(reg::v0, reg::a0, 4)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x08000000u);
}

TEST(IopShift, SraNegativeKeepsSign)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x80000000u);   // -2147483648
	h.LoadProgram({SRA(reg::v0, reg::a0, 1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xC0000000u);  // arithmetic: propagates 1
}

TEST(IopShift, SraPositive)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x0000FF00u);
	h.LoadProgram({SRA(reg::v0, reg::a0, 4)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x00000FF0u);
}

TEST(IopShift, Sllv)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x00000001u);
	h.SetGpr(reg::a1, 16);
	h.LoadProgram({SLLV(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x00010000u);
}

TEST(IopShift, SllvMasksShiftToLow5Bits)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x00000001u);
	h.SetGpr(reg::a1, 33);              // low 5 bits = 1
	h.LoadProgram({SLLV(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x00000002u);
}

TEST(IopShift, Srlv)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x00010000u);
	h.SetGpr(reg::a1, 8);
	h.LoadProgram({SRLV(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x00000100u);
}

TEST(IopShift, Srav)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xFFFF0000u);
	h.SetGpr(reg::a1, 4);
	h.LoadProgram({SRAV(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xFFFFF000u);
}

TEST(IopShift, SraArithmeticOnPositive)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x40000000u);
	h.LoadProgram({SRA(reg::v0, reg::a0, 2)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x10000000u);
}

TEST(IopShift, SrlBy0IsCopy)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xABCDEF01u);
	h.LoadProgram({SRL(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xABCDEF01u);
}

TEST(IopShift, SrlvLosesHighBits)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xFFFFFFFFu);
	h.SetGpr(reg::a1, 16);
	h.LoadProgram({SRLV(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x0000FFFFu);
}

// ---------------------------------------------------------------------------
// Variable-shift Rd == Rs aliasing.  Both Rd and Rs are the same GPR
// (e.g. `SLLV $a0, $a1, $a0`).  Allocator-order regression: if Rd is
// allocated MODE_WRITE before Rs is allocated MODE_READ, the slot tracking
// Rd/Rs gets a fresh write-only slot with no memory load, then Rs's alloc
// reuses that slot without loading — so the shift count is garbage.
// ---------------------------------------------------------------------------

TEST(IopShift, SllvRdEqualsRsAliasing)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 4u);            // shift count (also Rd)
	h.SetGpr(reg::a1, 0xFFu);
	h.LoadProgram({SLLV(reg::a0, reg::a1, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::a0), 0xFFu << 4);   // 0xFF0
	EXPECT_EQ(h.GetGprJit(reg::a0),    0xFFu << 4);
}

TEST(IopShift, SrlvRdEqualsRsAliasing)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 4u);
	h.SetGpr(reg::a1, 0xFF00u);
	h.LoadProgram({SRLV(reg::a0, reg::a1, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::a0), 0xFF00u >> 4);  // 0xFF0
	EXPECT_EQ(h.GetGprJit(reg::a0),    0xFF00u >> 4);
}

TEST(IopShift, SravRdEqualsRsAliasing)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 4u);
	h.SetGpr(reg::a1, 0xF0000000u);   // negative
	h.LoadProgram({SRAV(reg::a0, reg::a1, reg::a0)});
	h.Run();
	const u32 expected = static_cast<u32>(static_cast<s32>(0xF0000000u) >> 4);
	EXPECT_EQ(h.GetGprInterp(reg::a0), expected);
	EXPECT_EQ(h.GetGprJit(reg::a0),    expected);
}

// ---------------------------------------------------------------------------
// Variable-shift Rs-const fast path.  Prepend an `ORI rs, $zero, k` so the
// JIT propagates a const value into Rs at block-compile time, then the
// variable shift folds to an immediate shift.
// ---------------------------------------------------------------------------

TEST(IopShift, SllvWithConstRsFolds)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x0000FF00u);
	h.LoadProgram({
		ORI(reg::a1, reg::zero, 4),
		SLLV(reg::v0, reg::a0, reg::a1),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x000FF000u);
	EXPECT_EQ(h.GetGprJit(reg::v0),    0x000FF000u);
}

TEST(IopShift, SrlvWithConstRsFolds)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x00FF0000u);
	h.LoadProgram({
		ORI(reg::a1, reg::zero, 8),
		SRLV(reg::v0, reg::a0, reg::a1),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x0000FF00u);
	EXPECT_EQ(h.GetGprJit(reg::v0),    0x0000FF00u);
}

TEST(IopShift, SravWithConstRsHighBitsMasked)
{
	// Rs = 0x21 → low 5 bits = 1.  Verifies the 0x1F mask on the const-prop
	// emit (otherwise vixl would assert or emit a shift > 31).
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xF0000000u);
	h.LoadProgram({
		ORI(reg::a1, reg::zero, 0x21),
		SRAV(reg::v0, reg::a0, reg::a1),
	});
	h.Run();
	const u32 expected = static_cast<u32>(static_cast<s32>(0xF0000000u) >> 1);
	EXPECT_EQ(h.GetGprInterp(reg::v0), expected);
	EXPECT_EQ(h.GetGprJit(reg::v0),    expected);
}

TEST(IopShift, SllvWithConstRsZeroIsMove)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xDEADBEEFu);
	h.LoadProgram({
		ORI(reg::a1, reg::zero, 0),
		SLLV(reg::v0, reg::a0, reg::a1),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xDEADBEEFu);
	EXPECT_EQ(h.GetGprJit(reg::v0),    0xDEADBEEFu);
}
