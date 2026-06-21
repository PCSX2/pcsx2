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
