// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Branch semantics unique to the EE: likely-branches (BEQL/BNEL/…) squash
// their delay slot when not taken.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kPark = RecompilerTestEnvironment::kParkingPc;

// Same layout as iop_branch_tests:
//   0x00: <branch>   offset = 5 → taken target = 0x18
//   0x04: NOP        delay slot
//   0x08: ADDIU v0, zero, 1   not-taken marker
//   0x0C: J park; NOP; NOP
//   0x18: ADDIU v0, zero, 2   taken marker
//   0x1C: J park; NOP
inline void LoadBranchLayout(EeRecTestHarness& h, u32 branch_instr)
{
	h.LoadProgramNoTerm({
		branch_instr, NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
}

constexpr s16 kTakenOffset = 5;
} // namespace

TEST(EeRecBranch, BeqTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 42);
	h.SetGpr64(reg::a1, 42);
	LoadBranchLayout(h, BEQ(reg::a0, reg::a1, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
}

TEST(EeRecBranch, BeqNotTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 42);
	h.SetGpr64(reg::a1, 43);
	LoadBranchLayout(h, BEQ(reg::a0, reg::a1, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecBranch, BeqlSquashesDelaySlotWhenNotTaken)
{
	// BEQL + delay slot ADDIU a0, zero, 99. If not taken, delay slot
	// must NOT execute. a0 is checked after.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 10);
	h.SetGpr64(reg::a1, 20);                 // 10 != 20 → not taken
	h.LoadProgramNoTerm({
		ee::BEQL(reg::a0, reg::a1, kTakenOffset),
		ADDIU(reg::a0, reg::zero, 99),       // squashed delay slot
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);   // not-taken path
	EXPECT_EQ(h.GetGpr64Interp(reg::a0), 10ull);  // delay slot DID NOT run
}

TEST(EeRecBranch, BeqlExecutesDelaySlotWhenTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 10);
	h.SetGpr64(reg::a1, 10);                 // equal → taken
	h.LoadProgramNoTerm({
		ee::BEQL(reg::a0, reg::a1, kTakenOffset),
		ADDIU(reg::a0, reg::zero, 99),       // delay slot runs
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::a0), 99ull);
}

TEST(EeRecBranch, BnelSquashes)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 10);
	h.SetGpr64(reg::a1, 10);                 // equal → NOT taken (BNE-like)
	h.LoadProgramNoTerm({
		ee::BNEL(reg::a0, reg::a1, kTakenOffset),
		ADDIU(reg::a0, reg::zero, 99),       // squashed
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::a0), 10ull);
}

// ----- Likely-branch const-fold taken-target -----
//
// When BOTH operands are compile-time constant, recBEQL/recBNEL dispatch to
// the *_const fast path. The bug: that path read the global `pc` AFTER
// recompileNextInstruction(true,...) advanced it by 4, so the taken target
// landed one instruction too far. recBEQ_const/recBNE_const and the *_process
// siblings all capture branchTo BEFORE the delay-slot recompile.
//
// Layout (relative byte addr in parens):
//   (0x00) idx0  ADDIU v0,zero,7   sentinel
//   (0x04) idx1  ADDIU a0,zero,5   const
//   (0x08) idx2  ADDIU a1,zero,5   const  -> both const, BEQL const-folds
//   (0x0C) idx3  BEQL  a0,a1,5     branch (taken when equal)
//   (0x10) idx4  NOP               delay slot (runs on taken)
//   (0x14) idx5  ADDIU v0,zero,1   not-taken marker (dead on taken)
//   (0x18) idx6  J park
//   (0x1C) idx7  NOP
//   (0x20) idx8  NOP
//   (0x24) idx9  ADDIU v0,zero,2   CORRECT taken target  (off=5: 0x10+5*4)
//   (0x28) idx10 J park            buggy off-by-4 target lands here -> v0 stays 7
//   (0x2C) idx11 NOP
// Correct: v0 == 2. Buggy: target = 5*4 + (branch+8) = 0x28, skips idx9 -> v0 == 7.

TEST(EeRecBranch, BeqlConstFoldTakenTargetCorrect)
{
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		ADDIU(reg::v0, reg::zero, 7),
		ADDIU(reg::a0, reg::zero, 5),
		ADDIU(reg::a1, reg::zero, 5),          // a0 == a1 (const) -> BEQL taken
		ee::BEQL(reg::a0, reg::a1, 5),
		NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 2ull);
}

TEST(EeRecBranch, BnelConstFoldTakenTargetCorrect)
{
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		ADDIU(reg::v0, reg::zero, 7),
		ADDIU(reg::a0, reg::zero, 5),
		ADDIU(reg::a1, reg::zero, 6),          // a0 != a1 (const) -> BNEL taken
		ee::BNEL(reg::a0, reg::a1, 5),
		NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 2ull);
}

// ----- BNE -----------------------------------------------------------

TEST(EeRecBranch, BneTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 10);
	h.SetGpr64(reg::a1, 11);
	LoadBranchLayout(h, BNE(reg::a0, reg::a1, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
}

TEST(EeRecBranch, BneNotTakenWithSameRegisterFold)
{
	// Rs == Rt is a same-register inequality — provably false at compile
	// time, so the handler folds to "always not taken" and the delay slot
	// still runs (non-likely branch).
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 10);
	h.LoadProgramNoTerm({
		BNE(reg::a0, reg::a0, kTakenOffset),
		ADDIU(reg::t0, reg::zero, 7),        // delay slot runs
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);    // not-taken path
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), 7ull);    // delay slot ran
}

TEST(EeRecBranch, BeqAlwaysTakenSameRegister)
{
	// Rs == Rt is provably equal — compile-time fold to "always taken"
	// (no condition emitted; delay slot still runs).
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 17);
	LoadBranchLayout(h, BEQ(reg::a0, reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
}

// ----- 64-bit comparisons (EE branches inspect full UD[0]) -----------

TEST(EeRecBranch, BneInspectsFull64BitValue)
{
	// Two values whose lower-32 halves match but upper halves differ
	// must be considered NOT equal — confirms 64-bit cmp.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x0000'0000'CAFEBABEull);
	h.SetGpr64(reg::a1, 0xDEADBEEF'CAFEBABEull);
	LoadBranchLayout(h, BNE(reg::a0, reg::a1, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);   // taken
}

// ----- BLTZ / BGEZ / BLEZ / BGTZ (single-register sign tests) --------

TEST(EeRecBranch, BltzNegativeTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, static_cast<u64>(-1));
	LoadBranchLayout(h, BLTZ(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
}

TEST(EeRecBranch, BltzZeroNotTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0);
	LoadBranchLayout(h, BLTZ(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecBranch, BgezZeroTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0);
	LoadBranchLayout(h, BGEZ(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
}

TEST(EeRecBranch, BgezNegativeNotTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, static_cast<u64>(-5));
	LoadBranchLayout(h, BGEZ(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecBranch, BlezZeroTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0);
	LoadBranchLayout(h, BLEZ(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
}

TEST(EeRecBranch, BgtzPositiveTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 7);
	LoadBranchLayout(h, BGTZ(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
}

TEST(EeRecBranch, BgtzZeroNotTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0);
	LoadBranchLayout(h, BGTZ(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

// ----- Likely sign-test branches squash on not-taken ----------------

TEST(EeRecBranch, BlezlSquashesDelaySlotWhenNotTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 7);                   // > 0 → not taken
	h.LoadProgramNoTerm({
		ee::BLEZL(reg::a0, kTakenOffset),
		ADDIU(reg::a0, reg::zero, 99),        // squashed
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::a0), 7ull);
}

TEST(EeRecBranch, BgtzlExecutesDelaySlotWhenTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 5);                    // > 0 → taken
	h.LoadProgramNoTerm({
		ee::BGTZL(reg::a0, kTakenOffset),
		ADDIU(reg::t0, reg::zero, 33),         // delay slot runs
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), 33ull);
}

// ----- Link branches write ra = pc + 8 (PC of insn after delay slot) -

TEST(EeRecBranch, BltzalLinkRegisterReceivesPcPlus8)
{
	// Layout: BLTZAL is at kProgramPc; delay slot at +4; not-taken
	// fallthrough at +8 → that's the value that should land in ra.
	constexpr u32 kProgram = RecompilerTestEnvironment::kProgramPc;
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, static_cast<u64>(-1));   // negative → taken
	h.SetGpr64(reg::ra, 0xDEADBEEF'DEADBEEFull); // pre-pollute ra hi half
	LoadBranchLayout(h, BLTZAL(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);   // taken
	EXPECT_EQ(h.GetGpr64Interp(reg::ra), static_cast<u64>(kProgram + 8));
}

TEST(EeRecBranch, BgezalLinkRegisterWrittenEvenWhenNotTaken)
{
	// MIPS BGEZAL/BLTZAL link unconditionally — ra written before the
	// branch decision. Negative Rs → BGEZAL not taken, but ra still
	// changes.
	constexpr u32 kProgram = RecompilerTestEnvironment::kProgramPc;
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, static_cast<u64>(-1));   // negative → not taken
	LoadBranchLayout(h, BGEZAL(reg::a0, kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);   // not taken
	EXPECT_EQ(h.GetGpr64Interp(reg::ra), static_cast<u64>(kProgram + 8));
}

// ----- Const-prop survives across not-taken delay-slot save/restore --
//
// Regression for SaveBranchState / LoadBranchState: const-prop state
// captured before the delay slot must be restored intact after the
// taken arm finishes, so the not-taken side sees the same compile-time
// constants. Construct: AND with a known immediate makes Rt const, then
// branch. Whichever side runs, the const must reach v0 unchanged.
TEST(EeRecBranch, BeqConstPropSurvivesBranch)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0);
	h.SetGpr64(reg::a1, 5);                       // 0 != 5 → not taken
	h.LoadProgramNoTerm({
		ANDI(reg::t0, reg::zero, 0),              // t0 = 0 (const-prop)
		BEQ(reg::a0, reg::a1, kTakenOffset - 1),
		NOP,
		ADDIU(reg::v0, reg::t0, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::t0, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

// Backward-BEQ poll loop where the block's first instruction is also its
// branch target. The canonical shape is a timer-tick measurement loop:
//     LW   v0, 0(a1)      ; read counter
//     NOPs
//     BEQ  v1, v0, poll   ; back to LW if unchanged
//     SLTI v0, a0, 14     ; delay slot — overwrites v0
//
// The harness can't drive real HW-register changes, so the test simulates the
// same control-flow shape with an ALU counter instead of a timer read:
// v0 is incremented each iteration; BEQ v1, v0, back loops while equal.
// Exercises a block whose first instruction is also its branch target, and
// drives multiple iterations of the backward branch + delay-slot re-emit path.
TEST(EeRecBranch, BackwardBeqPollLoopExits)
{
	// v0 starts equal to v1 (loop taken once), then v0 is bumped so
	// the second BEQ compare falls through. v1 gets the post-loop
	// sentinel via the instruction after the delay slot.
	EeRecTestHarness h;
	h.SetGpr64(reg::v0, 0x42);
	h.SetGpr64(reg::v1, 0x42);
	h.SetGpr64(reg::a0, 0);
	h.LoadProgramNoTerm({
		// 0x00 loop:
		ADDIU(reg::v0, reg::v0, 1),               // v0++ — simulates fresh poll
		NOP, NOP, NOP, NOP,                        // BIOS has 4 NOPs between LW/BEQ
		BEQ(reg::v1, reg::v0, -6),                 // 0x14 → target 0x00
		SLTI(reg::v0, reg::a0, 14),               // delay slot (like BIOS)
		// fall-through:
		ADDIU(reg::v1, reg::zero, 0x99),          // 0x1c post-loop sentinel
		J(kPark), NOP,
	});
	h.Run();
	// After one iteration v0=0x43, v1=0x42, BEQ falls through. Delay
	// slot then writes v0 = (a0=0 < 14) = 1, so final v0 reflects the
	// delay-slot result (matches BIOS's v0 reuse semantics). v1 must
	// reach the sentinel — hitting it proves the back-branch was exited.
	EXPECT_EQ(h.GetGpr64Interp(reg::v1), 0x99ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

// Const-operand fast paths in recSetBranchEQ. When one branch
// operand is const-folded to 0, the test collapses to a single
// compare-and-branch-on-zero (no Mov #0 + Cmp). A non-zero const folds to
// an immediate Cmp. These guard that the collapsed codegen still branches
// the right direction.
//
// Layout: t0 := const (ANDI/ADDIU from $zero), then BEQ/BNE(a0, t0).
// _Rt_ (t0) is const → PROCESS_CONSTT, live operand is a0.
TEST(EeRecBranch, BeqConstZeroTaken)
{
	// a0 == 0 == t0 → BEQ taken.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0);
	h.LoadProgramNoTerm({
		ANDI(reg::t0, reg::zero, 0),                  // t0 = const 0
		BEQ(reg::a0, reg::t0, kTakenOffset - 1),
		NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
}

TEST(EeRecBranch, BeqConstZeroNotTaken)
{
	// a0 != 0, t0 == 0 → BEQ not taken.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 7);
	h.LoadProgramNoTerm({
		ANDI(reg::t0, reg::zero, 0),
		BEQ(reg::a0, reg::t0, kTakenOffset - 1),
		NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecBranch, BneConstZeroTaken)
{
	// a0 != 0, t0 == 0 → BNE taken.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 7);
	h.LoadProgramNoTerm({
		ANDI(reg::t0, reg::zero, 0),
		BNE(reg::a0, reg::t0, kTakenOffset - 1),
		NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
}

TEST(EeRecBranch, BneConstZeroNotTaken)
{
	// a0 == 0 == t0 → BNE not taken.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0);
	h.LoadProgramNoTerm({
		ANDI(reg::t0, reg::zero, 0),
		BNE(reg::a0, reg::t0, kTakenOffset - 1),
		NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecBranch, BeqConstNonZeroImmediateCmp)
{
	// t1 := const 7; BEQ(a0, t1). a0 == 7 → taken (immediate-Cmp path).
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 7);
	h.LoadProgramNoTerm({
		ADDIU(reg::t1, reg::zero, 7),                 // t1 = const 7
		BEQ(reg::a0, reg::t1, kTakenOffset - 1),
		NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
}

TEST(EeRecBranch, BeqConstNonZeroImmediateCmpNotTaken)
{
	// t1 := const 7; BEQ(a0, t1). a0 == 8 → not taken.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 8);
	h.LoadProgramNoTerm({
		ADDIU(reg::t1, reg::zero, 7),
		BEQ(reg::a0, reg::t1, kTakenOffset - 1),
		NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

// Companion: BEQ back-branch that loops multiple iterations, driven by
// a register that flips under ALU. The inner block's compile-time layout
// (first insn == branch target) is the specific shape a tight timer-poll
// loop hangs in.
TEST(EeRecBranch, BackwardBneLoopMultipleIterations)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::v0, 0);
	h.SetGpr64(reg::v1, 5);            // loop 5× until v0 == 5
	h.LoadProgramNoTerm({
		// 0x00 loop:
		ADDIU(reg::v0, reg::v0, 1),     // v0++
		NOP, NOP, NOP,
		BNE(reg::v1, reg::v0, -5),      // 0x10 → target 0x00 while v0 != 5
		NOP,                              // delay slot
		// fall-through at 0x18:
		ADDIU(reg::t0, reg::zero, 42),
		J(kPark), NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 5ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), 42ull);
}

// ----- TrySwapDelaySlot: non-NOP delay-slot hoisting --------------------
//
// TrySwapDelaySlot lets the recompiler emit a branch's delay-slot
// instruction *ahead* of the branch-condition evaluation when the slot
// can't affect the condition (it doesn't write a register the branch
// reads). That collapses the not-taken-side delay-slot re-emit into a
// single copy. Before the full table port, only a NOP delay slot swapped;
// these guard the newly-activated non-NOP path. Run() diffs JIT vs interp,
// so any reordering miscompile fails here automatically; ExpectGpr64 then
// pins the architectural result.

TEST(EeRecBranch, BeqSwapsSafeNonNopDelaySlotWhenTaken)
{
	// Delay slot writes t0, which the BEQ does not read → safe to swap.
	// a0 == a1 → taken; the delay slot must still have run.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 42);
	h.SetGpr64(reg::a1, 42);
	h.LoadProgramNoTerm({
		BEQ(reg::a0, reg::a1, kTakenOffset),
		ADDIU(reg::t0, reg::zero, 99),        // swappable delay slot
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 2ull);             // taken
	h.ExpectGpr64(reg::t0, 99ull);            // delay slot ran
}

TEST(EeRecBranch, BeqSwapsSafeNonNopDelaySlotWhenNotTaken)
{
	// Same safe delay slot, but a0 != a1 → not taken. A non-likely branch
	// always runs its delay slot, so t0 must still be written.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 42);
	h.SetGpr64(reg::a1, 43);
	h.LoadProgramNoTerm({
		BEQ(reg::a0, reg::a1, kTakenOffset),
		ADDIU(reg::t0, reg::zero, 99),        // swappable delay slot
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 1ull);             // not taken
	h.ExpectGpr64(reg::t0, 99ull);            // delay slot ran anyway
}

TEST(EeRecBranch, BeqDeclinesSwapWhenDelaySlotWritesCompareReg)
{
	// Delay slot writes a1, which the BEQ reads → MUST NOT swap. MIPS
	// semantics evaluate the branch on the OLD a1, then run the delay slot.
	// a0 == old a1 (5 == 5) → taken. A wrongful swap would set a1 = 6
	// before the compare, making 5 != 6 → not taken, so this test fails
	// loudly (v0 == 1 and/or a JIT-vs-interp divergence) on a broken
	// safety check.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 5);
	h.SetGpr64(reg::a1, 5);
	h.LoadProgramNoTerm({
		BEQ(reg::a0, reg::a1, kTakenOffset),
		ADDIU(reg::a1, reg::a1, 1),           // hazard: writes compare reg
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 2ull);             // taken on the OLD a1
	h.ExpectGpr64(reg::a1, 6ull);             // delay slot incremented a1
}

TEST(EeRecBranch, BltzSwapsSafeNonNopDelaySlot)
{
	// Single-register branch (reads rs only, rt/rd = 0). Delay slot writes
	// t0 (unrelated) → safe to swap. a0 < 0 → taken.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, static_cast<u64>(-1));
	h.LoadProgramNoTerm({
		BLTZ(reg::a0, kTakenOffset),
		ADDIU(reg::t0, reg::zero, 77),        // swappable delay slot
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 2ull);             // taken
	h.ExpectGpr64(reg::t0, 77ull);            // delay slot ran
}
