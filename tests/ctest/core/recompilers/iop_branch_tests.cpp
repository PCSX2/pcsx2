// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "harness/JitTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kPark = RecompilerTestEnvironment::kParkingPc;

// A shared program layout for conditional-branch tests:
//
//   0x00: <branch>           if taken, skip 4 instrs (target = 0x18)
//   0x04: NOP                delay slot (always executed)
//   0x08: ADDIU v0, zero, 1  NOT-TAKEN marker
//   0x0C: J kParkingPc       sink
//   0x10: NOP
//   0x14: NOP                (unused padding)
//   0x18: ADDIU v0, zero, 2  TAKEN marker
//   0x1C: J kParkingPc       sink
//   0x20: NOP
//
// Offset from the branch (at 0x00) is `(0x18 - (0x04)) / 4` = 5 (signed, fits in 16 bits).
// Loads a conditional-branch test program directly into the harness.
inline void LoadBranchLayout(JitTestHarness& h, u32 branch_instr)
{
	h.LoadProgramNoTerm({
		branch_instr, NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
}

// Branch offset for the taken path in MakeBranchLayout. The branch is at
// program PC + 0. MIPS branch target = (pc of branch + 4) + (offset << 2).
// Target = program PC + 0x18 → offset = (0x18 - 0x04) / 4 = 5.
constexpr s16 kTakenOffset = 5;
} // namespace

TEST(IopBranch, BeqTaken)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 42);
	h.SetGpr(reg::a1, 42);
	LoadBranchLayout(h, BEQ(reg::a0, reg::a1, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 2u);
}

TEST(IopBranch, BeqNotTaken)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 42);
	h.SetGpr(reg::a1, 43);
	LoadBranchLayout(h, BEQ(reg::a0, reg::a1, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 1u);
}

TEST(IopBranch, BneTaken)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 42);
	h.SetGpr(reg::a1, 43);
	LoadBranchLayout(h, BNE(reg::a0, reg::a1, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 2u);
}

TEST(IopBranch, BneNotTaken)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 42);
	h.SetGpr(reg::a1, 42);
	LoadBranchLayout(h, BNE(reg::a0, reg::a1, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 1u);
}

TEST(IopBranch, BgezTaken)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 42);
	LoadBranchLayout(h, BGEZ(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 2u);
}

TEST(IopBranch, BgezNotTaken)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, static_cast<u32>(-1));
	LoadBranchLayout(h, BGEZ(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 1u);
}

TEST(IopBranch, BltzTaken)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, static_cast<u32>(-5));
	LoadBranchLayout(h, BLTZ(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 2u);
}

// Const-folded Rs path in rpsxBranchZero: no explicit _psxFlushAllDirty()
// is needed. Rs is const-folded by a preceding immediate load, so the branch
// resolves statically. The delay slot dirties t0 — proving the delay slot
// still executes and commits without the explicit flush.
//
//   0x00: ADDIU a0, zero, imm   const-fold Rs
//   0x04: BLTZ  a0, +4          → target 0x18
//   0x08: ADDIU t0, zero, 7     delay slot (dirties t0)
//   0x0C: ADDIU v0, zero, 1     not-taken marker
//   0x10: J park; 0x14: NOP
//   0x18: ADDIU v0, zero, 2     taken marker
//   0x1C: J park; 0x20: NOP
TEST(IopBranch, BltzConstRsTakenDelaySlotCommits)
{
	JitTestHarness h;
	h.LoadProgramNoTerm({
		ADDIU(reg::a0, reg::zero, static_cast<s16>(-1)),   // a0 = const -1
		BLTZ(reg::a0, 4),                                   // -1 < 0 → taken
		ADDIU(reg::t0, reg::zero, 7),                       // delay slot
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 2u);                 // taken
	EXPECT_EQ(h.GetGprInterp(reg::t0), 7u);                 // delay slot ran
}

TEST(IopBranch, BltzConstRsNotTakenDelaySlotCommits)
{
	JitTestHarness h;
	h.LoadProgramNoTerm({
		ADDIU(reg::a0, reg::zero, 1),                       // a0 = const +1
		BLTZ(reg::a0, 4),                                   // 1 >= 0 → not taken
		ADDIU(reg::t0, reg::zero, 7),                       // delay slot
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 1u);                 // not taken
	EXPECT_EQ(h.GetGprInterp(reg::t0), 7u);                 // delay slot ran
}

TEST(IopBranch, BgtzTakenZero)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0);
	LoadBranchLayout(h, BGTZ(reg::a0, kTakenOffset));
	h.Run();
	// 0 is not > 0 — BGTZ NOT taken.
	EXPECT_EQ(h.GetGprInterp(reg::v0), 1u);
}

TEST(IopBranch, BgtzTakenPositive)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 7);
	LoadBranchLayout(h, BGTZ(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 2u);
}

TEST(IopBranch, BlezZero)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0);
	LoadBranchLayout(h, BLEZ(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 2u);
}

TEST(IopBranch, BgezalLinkRegSet)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 42);
	LoadBranchLayout(h, BGEZAL(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 2u);
	// BGEZAL writes pc+8 to ra *regardless* of whether the branch is taken.
	EXPECT_EQ(h.GetGprInterp(reg::ra), RecompilerTestEnvironment::kProgramPc + 8);
}

TEST(IopBranch, BltzalTaken)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, static_cast<u32>(-7));
	LoadBranchLayout(h, BLTZAL(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 2u);
	EXPECT_EQ(h.GetGprInterp(reg::ra), RecompilerTestEnvironment::kProgramPc + 8);
}

TEST(IopBranch, BltzalNotTaken)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 5);
	LoadBranchLayout(h, BLTZAL(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 1u);
	// BLTZAL unconditionally writes ra, even when branch isn't taken.
	EXPECT_EQ(h.GetGprInterp(reg::ra), RecompilerTestEnvironment::kProgramPc + 8);
}

TEST(IopBranch, DelaySlotAlwaysExecutes)
{
	// Build a layout where the delay slot (always executed) writes v0 = 42.
	// Whether or not the branch is taken, v0 must equal 42.
	JitTestHarness h;
	h.LoadProgramNoTerm({
		BEQ(reg::zero, reg::zero, kTakenOffset),
		ADDIU(reg::v0, reg::zero, 42),  // delay slot
		ADDIU(reg::a0, reg::zero, 99),  // not-taken body (skipped)
		J(kPark), NOP, NOP,
		ADDIU(reg::a0, reg::zero, 11),  // taken body
		J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 42u);
	EXPECT_EQ(h.GetGprInterp(reg::a0), 11u);  // branch was taken
}
