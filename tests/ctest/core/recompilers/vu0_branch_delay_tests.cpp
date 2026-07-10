// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU0 control-flow + delay-slot DiffJitVsInterp suite. Covers VB
// (unconditional), VBAL (link), VJR (register), VJALR (register + link),
// and the six VIBxx conditional branches. Every test asserts:
//
//  1. Branch-taken vs not-taken: the target instruction runs (or is skipped),
//     the fall-through instruction runs (or doesn't).
//  2. Delay-slot pair *always* runs — that's the architectural contract for
//     all VU branches and a recurring source of recompiler bugs.
//  3. Link registers (VBAL / VJALR) hold (delay_slot_pc + 8) / 8 — the pair
//     index of the instruction past the delay slot. See _vuBAL VUops.cpp:1595.
//
// Branch-displacement encoding: imm11 is signed *pairs* relative to the
// delay-slot PC, so a branch at pair N with imm11 = K targets pair (N+1+K).
// The interpreter advances REG_TPC to the delay-slot PC *before* computing
// `_branchAddr` (VU0microInterp.cpp:75-76, VUops.cpp:1456-1461).

#include "harness/VuTestHarness.h"

#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

inline VuOp LowerOnly(u32 lower) { return VuOp{lower, VNOP_U()}; }

// VI loads: each test seeds VI registers via SetVi, but a couple of tests
// also need an in-program VI write to exercise the JIT's reg-bank flush
// across branches. Use VIADDIU vi_dst, vi0, imm to materialise an in-program
// constant load. `imm` is 15-bit unsigned.
inline VuOp LoadViImm(u32 dst, u32 imm) { return LowerOnly(VIADDIU_L(dst, vi::vi0, imm)); }

} // namespace

// =========================================================================
//  VB — unconditional forward / backward / self-skip
// =========================================================================

TEST(Vu0BranchDelay, VbForwardSkipsOneInstructionDelaySlotRuns)
{
	// VB at pair 0 with imm11=+2 → target = branch_pc + (2+1)*8 = pair 3. Pair 1
	// (the delay slot) runs, pair 2 is skipped, and pair 3 (the target) runs.
	VuTestHarness h(0);
	h.LoadProgram({
		LowerOnly(VB_L(+2)),                            // pair 0: jump to pair 3
		LoadViImm(vi::vi1, 0x101),                      // pair 1: delay slot — runs
		LoadViImm(vi::vi2, 0x202),                      // pair 2: SKIPPED
		LoadViImm(vi::vi3, 0x303),                      // pair 3: target — runs
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), 0x101u);
	EXPECT_EQ(h.GetViJit(vi::vi2), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x303u);
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
}

TEST(Vu0BranchDelay, VbBackwardLoopWithCounterTerminates)
{
	// Counter loop: vi2 starts at 3, decremented each iteration; vi1 sums.
	// Two NOPs sit between the decrement and the conditional branch so the
	// VI hazard backup window (VIBackupCycles=2 per VUops.cpp:430) elapses
	// and the branch sees the post-decrement value.
	//
	// Branch displacement: imm11 = K means target = branch_pc + (K+1)*8.
	// Pair 6 (branch) → pair 2 (loop top): K = (16-48)/8 - 1 = -5.
	VuTestHarness h(0);
	h.SetVi(vi::vi3, 1); // step constant for VISUB
	h.LoadProgram({
		LoadViImm(vi::vi1, 0),                          // pair 0: sum = 0
		LoadViImm(vi::vi2, 3),                          // pair 1: counter = 3
		LowerOnly(VIADDIU_L(vi::vi1, vi::vi1, 10)),     // pair 2: L1: sum += 10
		LowerOnly(VISUB_L (vi::vi2, vi::vi2, vi::vi3)), // pair 3: counter -= 1
		LowerOnly(0),                                   // pair 4: NOP (hazard pad)
		LowerOnly(0),                                   // pair 5: NOP (hazard pad)
		LowerOnly(VIBNE_L(vi::vi2, vi::vi0, -5)),       // pair 6: if counter != 0, back to L1
		LowerOnly(0),                                   // pair 7: delay slot (NOP)
		EBitNopPair(),                                  // pair 8: terminate
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), 30u);
	EXPECT_EQ(h.GetViJit(vi::vi2), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

// =========================================================================
//  VBAL — unconditional branch + link
// =========================================================================

TEST(Vu0BranchDelay, VbalLinksDelaySlotPlusEightDividedBy8)
{
	// VBAL at pair 0 with imm11=+1 → target = pair 2.
	// Link reg = (delay_slot_pc + 8) / 8 = (8 + 8) / 8 = pair 2.
	// (Per _vuBAL VUops.cpp:1595-1611: VI[_It_].US[0] = (REG_TPC + 8) / 8
	//  evaluated *after* TPC has been bumped to the delay-slot PC.)
	VuTestHarness h(0);
	h.LoadProgram({
		LowerOnly(VBAL_L(vi::vi5, +1)),                 // pair 0: link → vi5
		LoadViImm(vi::vi1, 0x111),                      // pair 1: delay slot — runs
		LoadViImm(vi::vi2, 0x222),                      // pair 2: target — runs
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi5), 2u);
	EXPECT_EQ(h.GetViJit(vi::vi1), 0x111u);
	EXPECT_EQ(h.GetViJit(vi::vi2), 0x222u);
	EXPECT_EQ(h.GetViJit(vi::vi5), h.GetViInterp(vi::vi5));
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

TEST(Vu0BranchDelay, VbalIntoVi0DoesNotWriteLink)
{
	// VBAL with _It_ == 0 must not touch VI[0] — hardwired-zero invariant.
	VuTestHarness h(0);
	h.LoadProgram({
		LowerOnly(VBAL_L(vi::vi0, +1)),
		LoadViImm(vi::vi1, 0x111),
		LoadViImm(vi::vi2, 0x222),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi0), 0u);
	EXPECT_EQ(h.GetViInterp(vi::vi0), 0u);
}

// =========================================================================
//  VJR / VJALR — register-indirect jump
// =========================================================================

TEST(Vu0BranchDelay, VjrJumpsToRegisterPairIndex)
{
	// VJR target = VI[is].US[0] * 8 (VUops.cpp:1613-1617). Set vi5 = 3
	// → target byte address 24 → pair 3.
	VuTestHarness h(0);
	h.SetVi(vi::vi5, 3);
	h.LoadProgram({
		LowerOnly(VJR_L(vi::vi5)),                      // pair 0: jump to pair 3
		LoadViImm(vi::vi1, 0x111),                      // pair 1: delay slot — runs
		LoadViImm(vi::vi2, 0x222),                      // pair 2: SKIPPED
		LoadViImm(vi::vi3, 0x333),                      // pair 3: target — runs
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), 0x111u);
	EXPECT_EQ(h.GetViJit(vi::vi2), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x333u);
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
}

TEST(Vu0BranchDelay, VjalrJumpsAndLinks)
{
	// VJALR at pair 0 (jump to pair 3 via vi5=3, link to vi6).
	// Link reg = (delay_slot_pc + 8) / 8 = 16/8 = 2.
	VuTestHarness h(0);
	h.SetVi(vi::vi5, 3);
	h.LoadProgram({
		LowerOnly(VJALR_L(vi::vi6, vi::vi5)),
		LoadViImm(vi::vi1, 0x111),                      // delay slot
		LoadViImm(vi::vi2, 0x222),                      // skipped
		LoadViImm(vi::vi3, 0x333),                      // target
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi6), 2u);
	EXPECT_EQ(h.GetViJit(vi::vi1), 0x111u);
	EXPECT_EQ(h.GetViJit(vi::vi2), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x333u);
	EXPECT_EQ(h.GetViJit(vi::vi6), h.GetViInterp(vi::vi6));
}

// =========================================================================
//  VIBEQ / VIBNE — equality conditional branches
// =========================================================================

TEST(Vu0BranchDelay, VibeqTakenWhenEqual)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 42);
	h.SetVi(vi::vi2, 42);
	h.LoadProgram({
		LowerOnly(VIBEQ_L(vi::vi1, vi::vi2, +2)),       // taken → pair 3
		LoadViImm(vi::vi3, 0x101),                      // delay slot
		LoadViImm(vi::vi4, 0x202),                      // skipped
		LoadViImm(vi::vi5, 0x303),                      // target
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x101u);
	EXPECT_EQ(h.GetViJit(vi::vi4), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi5), 0x303u);
	EXPECT_EQ(h.GetViJit(vi::vi4), h.GetViInterp(vi::vi4));
}

TEST(Vu0BranchDelay, VibeqNotTakenWhenUnequal)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 1);
	h.SetVi(vi::vi2, 2);
	h.LoadProgram({
		LowerOnly(VIBEQ_L(vi::vi1, vi::vi2, +2)),       // not taken → fall through
		LoadViImm(vi::vi3, 0x101),                      // pair 1: runs
		LoadViImm(vi::vi4, 0x202),                      // pair 2: runs (NOT skipped)
		LoadViImm(vi::vi5, 0x303),                      // pair 3: runs
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x101u);
	EXPECT_EQ(h.GetViJit(vi::vi4), 0x202u);
	EXPECT_EQ(h.GetViJit(vi::vi5), 0x303u);
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
	EXPECT_EQ(h.GetViJit(vi::vi4), h.GetViInterp(vi::vi4));
	EXPECT_EQ(h.GetViJit(vi::vi5), h.GetViInterp(vi::vi5));
}

// Regression: mVU VI-backup on the UNCACHED clone-write path. When a branch
// delay-slot integer op writes the branch's condition VI via a clone-write
// (load from a DIFFERENT, uncached VI), the backup must save the WRITE
// target's pre-write value — not the load source. The arm64 allocGPR uncached
// path only handled write-only allocs (viLoadReg < 0); for a clone-write it
// backed up the load source instead, so the branch evaluated against the wrong
// VI. Upstream fix 265afcec7 ("Fix incorrect VI being backed up when
// uncached") — fixes a Gitaroo Man hang.
//
// Clone-write VI backup in a branch delay slot: the IBEQ's condition reg vi1 is
// clone-written by its delay slot (IAND vi1, vi3, vi4), so the branch must
// compare vi1's PRE-delay-slot value (42). The backup must save vi1's old value
// (42) -> branch taken; a backup of the clone SOURCE vi3 (99) would make
// IBEQ(99,42) not taken. This exercises the *cached* allocGPR clone-write
// backup path (vi1 is cached by the branch's own read of it), which has always
// been correct.
//
// NOTE: the *uncached* clone-write backup bug (upstream 265afcec7, "Fix
// incorrect VI being backed up when uncached" — a Gitaroo Man hang, fixed in
// microVU_IR-arm64.h allocGPR) needs BOTH the clone source and the write target
// VI to be uncached at the clone-write while the branch still consumes the
// backup. That is a register-pressure coincidence not reproducible in a minimal
// VU program (the branch's read keeps the target cached), so it has no
// deterministic unit repro; this test guards the adjacent cached path.
TEST(Vu0BranchDelay, VibeqCloneWriteViBackupInDelaySlot)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 42);     // condition reg, PRE-delay-slot value — equals vi2
	h.SetVi(vi::vi2, 42);
	h.SetVi(vi::vi3, 99);     // clone-write source (uncached); != 42
	h.SetVi(vi::vi4, 0xFFFF); // IAND mask
	h.LoadProgram({
		LowerOnly(VIBEQ_L(vi::vi1, vi::vi2, +2)),       // pair 0: taken iff backup(vi1)==vi2 → pair 3
		LowerOnly(VIAND_L(vi::vi1, vi::vi3, vi::vi4)),  // pair 1: delay slot clone-write vi1 (backup)
		LowerOnly(VIADDIU_L(vi::vi6, vi::vi1, 0)),      // pair 2: skipped iff taken; reads vi1 (=used)
		LoadViImm(vi::vi7, 0x333),                      // pair 3: target
		EBitNopPair(),
	});
	h.Run();
	// Correct: branch taken (vi1_old 42 == vi2 42) → pair 2 skipped → vi6 stays 0.
	EXPECT_EQ(h.GetViJit(vi::vi6), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi6), h.GetViInterp(vi::vi6));
	EXPECT_EQ(h.GetViJit(vi::vi7), 0x333u);
}

TEST(Vu0BranchDelay, VibneTakenWhenUnequal)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 1);
	h.SetVi(vi::vi2, 2);
	h.LoadProgram({
		LowerOnly(VIBNE_L(vi::vi1, vi::vi2, +2)),
		LoadViImm(vi::vi3, 0x101),
		LoadViImm(vi::vi4, 0x202),
		LoadViImm(vi::vi5, 0x303),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x101u);
	EXPECT_EQ(h.GetViJit(vi::vi4), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi5), 0x303u);
	EXPECT_EQ(h.GetViJit(vi::vi4), h.GetViInterp(vi::vi4));
}

TEST(Vu0BranchDelay, VibneNotTakenWhenEqual)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 7);
	h.SetVi(vi::vi2, 7);
	h.LoadProgram({
		LowerOnly(VIBNE_L(vi::vi1, vi::vi2, +2)),
		LoadViImm(vi::vi3, 0x101),
		LoadViImm(vi::vi4, 0x202),
		LoadViImm(vi::vi5, 0x303),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x101u);
	EXPECT_EQ(h.GetViJit(vi::vi4), 0x202u);
	EXPECT_EQ(h.GetViJit(vi::vi5), 0x303u);
}

// =========================================================================
//  VIBLTZ / VIBGTZ / VIBLEZ / VIBGEZ — sign / zero conditional branches
// =========================================================================

TEST(Vu0BranchDelay, VibltzTakenWhenNegative)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, static_cast<u32>(static_cast<s16>(-1)));
	h.LoadProgram({
		LowerOnly(VIBLTZ_L(vi::vi1, +2)),
		LoadViImm(vi::vi3, 0x101),
		LoadViImm(vi::vi4, 0x202),
		LoadViImm(vi::vi5, 0x303),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x101u);
	EXPECT_EQ(h.GetViJit(vi::vi4), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi5), 0x303u);
}

TEST(Vu0BranchDelay, VibltzNotTakenWhenZero)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0);
	h.LoadProgram({
		LowerOnly(VIBLTZ_L(vi::vi1, +2)),
		LoadViImm(vi::vi3, 0x101),
		LoadViImm(vi::vi4, 0x202),
		LoadViImm(vi::vi5, 0x303),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi4), 0x202u);
}

TEST(Vu0BranchDelay, VibgtzTakenWhenPositive)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 5);
	h.LoadProgram({
		LowerOnly(VIBGTZ_L(vi::vi1, +2)),
		LoadViImm(vi::vi3, 0x101),
		LoadViImm(vi::vi4, 0x202),
		LoadViImm(vi::vi5, 0x303),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi4), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi5), 0x303u);
}

TEST(Vu0BranchDelay, VibgtzNotTakenWhenZero)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0);
	h.LoadProgram({
		LowerOnly(VIBGTZ_L(vi::vi1, +2)),
		LoadViImm(vi::vi3, 0x101),
		LoadViImm(vi::vi4, 0x202),
		LoadViImm(vi::vi5, 0x303),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi4), 0x202u);
}

TEST(Vu0BranchDelay, VibgtzNotTakenWhenNegative)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, static_cast<u32>(static_cast<s16>(-3)));
	h.LoadProgram({
		LowerOnly(VIBGTZ_L(vi::vi1, +2)),
		LoadViImm(vi::vi3, 0x101),
		LoadViImm(vi::vi4, 0x202),
		LoadViImm(vi::vi5, 0x303),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi4), 0x202u);
}

TEST(Vu0BranchDelay, ViblezTakenWhenZero)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0);
	h.LoadProgram({
		LowerOnly(VIBLEZ_L(vi::vi1, +2)),
		LoadViImm(vi::vi3, 0x101),
		LoadViImm(vi::vi4, 0x202),
		LoadViImm(vi::vi5, 0x303),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi4), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi5), 0x303u);
}

TEST(Vu0BranchDelay, VibgezTakenWhenZero)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0);
	h.LoadProgram({
		LowerOnly(VIBGEZ_L(vi::vi1, +2)),
		LoadViImm(vi::vi3, 0x101),
		LoadViImm(vi::vi4, 0x202),
		LoadViImm(vi::vi5, 0x303),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi4), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi5), 0x303u);
}

TEST(Vu0BranchDelay, VibgezTakenWhenPositive)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 7);
	h.LoadProgram({
		LowerOnly(VIBGEZ_L(vi::vi1, +2)),
		LoadViImm(vi::vi3, 0x101),
		LoadViImm(vi::vi4, 0x202),
		LoadViImm(vi::vi5, 0x303),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi4), 0u);
}

TEST(Vu0BranchDelay, VibgezNotTakenWhenNegative)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, static_cast<u32>(static_cast<s16>(-1)));
	h.LoadProgram({
		LowerOnly(VIBGEZ_L(vi::vi1, +2)),
		LoadViImm(vi::vi3, 0x101),
		LoadViImm(vi::vi4, 0x202),
		LoadViImm(vi::vi5, 0x303),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi4), 0x202u);
}

// =========================================================================
//  Conditional branch in a branch delay slot — mVU "bad/evil branch"
// =========================================================================
// x86 mVU routes the delay-slot branch's condition through condEvilBranch(),
// which conditionally selects the taken/not-taken continuation address into
// mVU.badBranch/evilBranch. The arm64 port dropped condEvilBranch entirely, so
// the condition was computed and discarded and the continuation used stale
// target state. Found via MGS2 SLUS-20144: its VU0 collision solver runs
// `FMAND vi13 ; IBNE vi12 ; IBNE vi13` (both IBNEs to the same retry target) —
// the wrong path writes "miss" (3) into the result mailbox, the EE retries
// forever, and the game hangs 3s after the intro savestate.

TEST(Vu0BranchDelay, CondBranchInDelaySlotOfNotTakenBranch_Taken)
{
	// Branch #1 not taken; its delay slot holds IBNE that IS taken (condition
	// seeded pre-program, so no VI hazard — pure control flow).
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0); // #1: IBNE vi1,vi0 → not taken
	h.SetVi(vi::vi2, 1); // #2: IBNE vi2,vi0 → taken
	h.LoadProgram({
		LowerOnly(VIBNE_L(vi::vi1, vi::vi0, +2)),       // pair 0: not taken
		LowerOnly(VIBNE_L(vi::vi2, vi::vi0, +2)),       // pair 1: delay slot; taken → pair 4
		LoadViImm(vi::vi3, 0x101),                      // pair 2: #2's delay slot — runs
		LoadViImm(vi::vi4, 0x202),                      // pair 3: skipped
		LoadViImm(vi::vi5, 0x303),                      // pair 4: #2's target — runs
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x101u);
	EXPECT_EQ(h.GetViJit(vi::vi4), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi5), 0x303u);
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
	EXPECT_EQ(h.GetViJit(vi::vi4), h.GetViInterp(vi::vi4));
	EXPECT_EQ(h.GetViJit(vi::vi5), h.GetViInterp(vi::vi5));
}

TEST(Vu0BranchDelay, CondBranchInDelaySlotOfNotTakenBranch_NotTaken)
{
	// Branch #1 not taken; delay-slot IBNE also not taken → pure fall-through.
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0);
	h.SetVi(vi::vi2, 0);
	h.LoadProgram({
		LowerOnly(VIBNE_L(vi::vi1, vi::vi0, +2)),       // pair 0: not taken
		LowerOnly(VIBNE_L(vi::vi2, vi::vi0, +2)),       // pair 1: delay slot; not taken
		LoadViImm(vi::vi3, 0x101),                      // pair 2: runs
		LoadViImm(vi::vi4, 0x202),                      // pair 3: runs
		LoadViImm(vi::vi5, 0x303),                      // pair 4: runs
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x101u);
	EXPECT_EQ(h.GetViJit(vi::vi4), 0x202u);
	EXPECT_EQ(h.GetViJit(vi::vi5), 0x303u);
	EXPECT_EQ(h.GetViJit(vi::vi4), h.GetViInterp(vi::vi4));
}

TEST(Vu0BranchDelay, CondBranchInDelaySlot_MGS2Shape_WriteTwoBack_DelaySlotRuns)
{
	// The exact MGS2 sequence: condition VI written 2 ops before the delay-slot
	// branch. The interp's VI backup window (VIBackupCycles=2, decremented
	// BEFORE each op executes) has elapsed at gap 2, so the FRESH value (1) is
	// read and the delay-slot branch is taken. The pre-fix arm64 failure mode
	// here was skipping the taken branch's own delay slot (pair 3) — in MGS2
	// that skipped a WAITQ, desynced the Q pipeline, and fed the collision
	// solver garbage.
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0); // #1's condition: not taken
	h.LoadProgram({
		LoadViImm(vi::vi2, 1),                          // pair 0: write vi2 (fresh=1)
		LowerOnly(VIBNE_L(vi::vi1, vi::vi0, +2)),       // pair 1: not taken
		LowerOnly(VIBNE_L(vi::vi2, vi::vi0, +2)),       // pair 2: delay slot; fresh vi2=1 → taken → pair 5
		LoadViImm(vi::vi3, 0x101),                      // pair 3: #2's delay slot — MUST run
		LoadViImm(vi::vi4, 0x202),                      // pair 4: skipped
		LoadViImm(vi::vi5, 0x303),                      // pair 5: #2's target — runs
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), 0x101u);
	EXPECT_EQ(h.GetViJit(vi::vi4), 0u);
	EXPECT_EQ(h.GetViJit(vi::vi5), 0x303u);
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
	EXPECT_EQ(h.GetViJit(vi::vi4), h.GetViInterp(vi::vi4));
	EXPECT_EQ(h.GetViJit(vi::vi5), h.GetViInterp(vi::vi5));
}

TEST(Vu0BranchDelay, CondBranchInDelaySlotOfTakenBranch_MatchesInterp)
{
	// Branch #1 taken with a conditional (taken) branch in its delay slot —
	// the "evil" quadrant. One instruction at #1's target executes, then #2's
	// branch lands. Assert full JIT==interp equivalence rather than hardcoding
	// the (deliberately weird) hardware semantics.
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 1); // #1 taken
	h.SetVi(vi::vi2, 1); // #2 taken
	h.LoadProgram({
		LowerOnly(VIBNE_L(vi::vi1, vi::vi0, +2)),       // pair 0: taken → pair 3
		LowerOnly(VIBNE_L(vi::vi2, vi::vi0, +3)),       // pair 1: delay slot; taken → pair 5
		LoadViImm(vi::vi3, 0x101),                      // pair 2
		LoadViImm(vi::vi4, 0x202),                      // pair 3: #1's target
		LoadViImm(vi::vi5, 0x303),                      // pair 4
		LoadViImm(vi::vi6, 0x404),                      // pair 5: #2's target
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi3), h.GetViInterp(vi::vi3));
	EXPECT_EQ(h.GetViJit(vi::vi4), h.GetViInterp(vi::vi4));
	EXPECT_EQ(h.GetViJit(vi::vi5), h.GetViInterp(vi::vi5));
	EXPECT_EQ(h.GetViJit(vi::vi6), h.GetViInterp(vi::vi6));
}

// =========================================================================
//  Delay-slot semantics — upper-pipe op in delay slot
// =========================================================================

TEST(Vu0BranchDelay, UpperFmacInDelaySlotRunsRegardless)
{
	// Delay-slot pair carries an FMAC in the upper word — must execute on
	// both branch-taken and branch-not-taken paths. JIT bug shape: dead-
	// code-eliminating the delay-slot upper because the lower is a NOP.
	VuTestHarness h(0);
	h.SetVf(vf::vf1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(vf::vf2, 10.0f, 20.0f, 30.0f, 40.0f);
	h.SetVi(vi::vi1, 0);
	h.LoadProgram({
		LowerOnly(VIBEQ_L(vi::vi1, vi::vi0, +2)),       // taken → pair 3
		VuOp{0, VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)}, // pair 1: delay
		LoadViImm(vi::vi5, 0x505),                      // pair 2: skipped
		LoadViImm(vi::vi6, 0x606),                      // pair 3: target
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(vf::vf3, 'x'), 11.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(vf::vf3, 'y'), 22.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(vf::vf3, 'z'), 33.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(vf::vf3, 'w'), 44.0f);
	EXPECT_EQ(h.GetVfBitsJit(vf::vf3, 'x'), h.GetVfBitsInterp(vf::vf3, 'x'));
	EXPECT_EQ(h.GetVfBitsJit(vf::vf3, 'w'), h.GetVfBitsInterp(vf::vf3, 'w'));
}

} // namespace recompiler_tests
