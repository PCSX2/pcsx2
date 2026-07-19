// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Allocator ↔ const-prop coupling net (EE-SRA campaign, stage S0).
//
// These are ARMED TRIPWIRES for the EE-SRA pin-ladder rungs, not bug
// reproductions: all of them pass on today's allocator. They pin the
// coupling seams that sank the 2026-04-04 GPR-allocation attempt
// (ed330fa07 → 495269bd2 retreat twelve minutes later; root cause was
// const-tracked regs interacting with allocation around branch compares)
// and the compile-state-divergence class that dominates the lrps2/neither
// 22-bug clobber corpus: branch-arm state forks, cross-block compile-state
// leaks, and fixed-register assumptions. Every rung that touches the
// allocator, const-prop flush, or branch state save/restore must keep this
// file green.
//
// Branch layout convention (see ee_rec_branch_tests.cpp): with the two
// arms laid out as [not-taken: ADDIU,J,NOP,NOP][taken: ADDIU,J,NOP]
// immediately after the delay slot, a branch offset of 5 always lands on
// the taken arm regardless of how many instructions precede the branch.
//
// Mutation-verification note (2026-07-05): three LoadBranchState mutations
// (drop g_cpuHasConstReg restore / drop the GPR-allocation-map restore /
// drop the const-VALUE restore) were each absorbed by the FULL suite, and
// correctly so — recBEQ_process/recSetBranchEQ call _eeFlushAllDirty()
// BEFORE the fork and recompile the delay slot ONCE PER ARM
// (iR5900Branch-arm64.cpp:156-174), so memory is canonical at every fork
// and each arm re-derives the delay slot's state from the flushed base.
// Today's branch machinery is structurally immune to this class because
// it surrenders all register residency at branches. The EE-SRA pins keep
// values resident ACROSS branches — bypassing that safety net — which is
// why the campaign's write-through contract (pin == memory, always) must
// hold at every fork; these tests are the behavioral net for that. The
// pin-mirror observability of this suite's read-backs was RED-verified
// via the MFC0 rd=25 staleness bug (see ee_rec_pinned_gpr_tests.cpp).

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;
using namespace mips::ee;

namespace {
constexpr u32 kPark = RecompilerTestEnvironment::kParkingPc;
constexpr s16 kArmOffset = 5;
} // namespace

// ---------------------------------------------------------------------
// Delay slot KILLS a const with a runtime value. The branch snapshot is
// taken with "t0 = const 5" in the tracker; the delay slot (which runs on
// BOTH arms of a non-likely branch) overwrites t0 with a runtime sum. If
// branch-state save/restore resurrects the stale const on either arm, that
// arm computes from 5 instead of 0x34.
// ---------------------------------------------------------------------

static void RunDelaySlotKillsConst(bool taken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 1);
	h.SetGpr64(reg::a1, taken ? 1 : 2);
	h.SetGpr64(reg::a2, 0x30);
	h.SetGpr64(reg::a3, 0x04);
	h.LoadProgramNoTerm({
		ORI(reg::t0, reg::zero, 5),        // t0 const at the branch point
		BEQ(reg::a0, reg::a1, kArmOffset),
		DADDU(reg::t0, reg::a2, reg::a3),  // delay (both arms): kills the const
		ADDIU(reg::v0, reg::t0, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::t0, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t0, 0x34ull);
	h.ExpectGpr64(reg::v0, taken ? 0x36ull : 0x35ull);
}

TEST(EeRecRegallocCoupling, DelaySlotKillsConstNotTaken) { RunDelaySlotKillsConst(false); }
TEST(EeRecRegallocCoupling, DelaySlotKillsConstTaken)    { RunDelaySlotKillsConst(true); }

// ---------------------------------------------------------------------
// Likely branch forks the const state: BEQL's delay slot executes ONLY on
// the taken arm, so at compile time t0 is const 5 on the not-taken arm and
// const 99 on the taken arm. Leaking either value into the other arm is
// the classic branch-arm compile-state divergence.
// ---------------------------------------------------------------------

static void RunLikelyDelayConstForks(bool taken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 1);
	h.SetGpr64(reg::a1, taken ? 1 : 2);
	h.LoadProgramNoTerm({
		ORI(reg::t0, reg::zero, 5),
		ee::BEQL(reg::a0, reg::a1, kArmOffset),
		ORI(reg::t0, reg::zero, 99),       // delay: taken arm only
		ADDIU(reg::v0, reg::t0, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::t0, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t0, taken ? 99ull : 5ull);
	h.ExpectGpr64(reg::v0, taken ? 101ull : 6ull);
}

TEST(EeRecRegallocCoupling, LikelyDelayConstForksNotTaken) { RunLikelyDelayConstForks(false); }
TEST(EeRecRegallocCoupling, LikelyDelayConstForksTaken)    { RunLikelyDelayConstForks(true); }

// ---------------------------------------------------------------------
// Const materialized into a PINNED reg ($sp) inside the delay slot: the
// const flush must route through the write-through pin store on BOTH arm
// compiles, and both arms' read-backs consume the mirror. This is the
// exact surface the EE-SRA rungs widen (const-prop → pin table around
// branch state).
// ---------------------------------------------------------------------

static void RunDelayConstIntoPinned(bool taken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 1);
	h.SetGpr64(reg::a1, taken ? 1 : 2);
	h.SetGpr64(reg::sp, 0x0000000001F00010ull); // stale-mirror sentinel
	h.LoadProgramNoTerm({
		BEQ(reg::a0, reg::a1, kArmOffset),
		ORI(reg::sp, reg::zero, 0x7B0),    // delay: const into pinned $sp
		ADDIU(reg::v0, reg::sp, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::sp, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::sp, 0x7B0ull);
	h.ExpectGpr64(reg::v0, taken ? 0x7B2ull : 0x7B1ull);
}

TEST(EeRecRegallocCoupling, DelayConstIntoPinnedNotTaken) { RunDelayConstIntoPinned(false); }
TEST(EeRecRegallocCoupling, DelayConstIntoPinnedTaken)    { RunDelayConstIntoPinned(true); }

// ---------------------------------------------------------------------
// Pinned RMW across branch arms: a runtime (non-const) $sp write before
// the branch, an RMW of it in the delay slot, and reads on both arms —
// SaveBranchState/LoadBranchState must carry the pin-backed allocation
// state, not just the const tracker.
// ---------------------------------------------------------------------

static void RunPinnedRmwAcrossArms(bool taken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 1);
	h.SetGpr64(reg::a1, taken ? 1 : 2);
	h.SetGpr64(reg::a2, 0x20000);
	h.SetGpr64(reg::a3, 0x00030);
	h.LoadProgramNoTerm({
		DADDU(reg::sp, reg::a2, reg::a3),  // runtime $sp = 0x20030
		BEQ(reg::a0, reg::a1, kArmOffset),
		ADDIU(reg::sp, reg::sp, -16),      // delay: pinned RMW → 0x20020
		ADDIU(reg::v0, reg::sp, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::sp, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::sp, 0x20020ull);
	h.ExpectGpr64(reg::v0, taken ? 0x20022ull : 0x20021ull);
}

TEST(EeRecRegallocCoupling, PinnedRmwAcrossArmsNotTaken) { RunPinnedRmwAcrossArms(false); }
TEST(EeRecRegallocCoupling, PinnedRmwAcrossArmsTaken)    { RunPinnedRmwAcrossArms(true); }

// ---------------------------------------------------------------------
// Fixed-reg (HI/LO) traffic between the const's creation and its uses:
// MULT/MFLO force allocator activity in the window where t0 is
// const-tracked, then the branch compares the MFLO result and the delay
// slot reads the const. Mixes all three ingredients of the April failure.
// ---------------------------------------------------------------------

static void RunConstAcrossMultBranch(bool taken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, taken ? 63 : 1); // MULT 7*9 = 63
	h.SetGpr64(reg::a2, 7);
	h.SetGpr64(reg::a3, 9);
	h.LoadProgramNoTerm({
		ORI(reg::t0, reg::zero, 6),        // const, must survive the MULT
		MULT(reg::a2, reg::a3),            // LO/HI fixed-reg writes
		MFLO(reg::t3),
		BEQ(reg::t3, reg::a0, kArmOffset),
		ADDIU(reg::t4, reg::t0, 3),        // delay: const read → 9
		DADDU(reg::v0, reg::t3, reg::t0), J(kPark), NOP, NOP,
		DADDU(reg::v0, reg::t4, reg::t0), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t3, 63ull);
	h.ExpectGpr64(reg::t4, 9ull);
	h.ExpectGpr64(reg::v0, taken ? 15ull : 69ull);
}

TEST(EeRecRegallocCoupling, ConstAcrossMultBranchNotTaken) { RunConstAcrossMultBranch(false); }
TEST(EeRecRegallocCoupling, ConstAcrossMultBranchTaken)    { RunConstAcrossMultBranch(true); }

// ---------------------------------------------------------------------
// Cross-block compile-state leak: block A holds two live consts at its
// exit (one created in the J's delay slot); block B — separately compiled
// and statically linked — reads both from memory. Compile-time register
// or const state must never cross a block edge; this is the transposition
// of lrps2's compile-time-dirty-mask hazard onto our const tracker, and
// the invariant every EE-SRA rung (and especially any future lazy-dirty
// mode) must preserve.
// ---------------------------------------------------------------------

TEST(EeRecRegallocCoupling, CrossBlockConstNotLeaked)
{
	constexpr u32 kBlockAPc = RecompilerTestEnvironment::kProgramPc;
	constexpr u32 kBlockBPc = kBlockAPc + 0x100;

	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		ORI(reg::t0, reg::zero, 7),
		ADDIU(reg::t0, reg::t0, 1),        // t0 const 8
		J(kBlockBPc),
		ORI(reg::t1, reg::zero, 3),        // delay: second const, born at exit
	});
	h.WriteU32(kBlockBPc + 0, DADDU(reg::v0, reg::t0, reg::t1));
	h.WriteU32(kBlockBPc + 4, J(kPark));
	h.WriteU32(kBlockBPc + 8, NOP);
	h.Run();
	h.ExpectGpr64(reg::v0, 11ull);
	h.ExpectBlockLinked(kBlockAPc + 8, kBlockBPc);
}

// ---------------------------------------------------------------------
// Arm A (const-into-pin) — a multi-word const chain (LUI/ORI) materialized
// into a PINNED reg, flushed at the block exit, and consumed by a
// separately-compiled, statically-linked block through the PIN. Block B
// starts with fresh const state, so its read routes armLoadEERegPtr → the
// mirror, NOT const-prop; the static link carries no C call so both a
// callee-saved pin ($v0 → x29) and a caller-saved one ($v1 → x12) ride
// live. This is the exact seam Arm A rewrites: _flushConstReg materializes
// the immediate straight into the pin (Mov pin,#imm) instead of routing it
// through the scratch triad — a wrong value or a stranded mirror reddens
// the read-back. The trailing ExpectGpr64 on $v0/$v1 also pins the
// write-through STR (memory must stay canonical).
// ---------------------------------------------------------------------
TEST(EeRecRegallocCoupling, ConstChainMaterializedIntoPinnedCrossBlock)
{
	constexpr u32 kBlockAPc = RecompilerTestEnvironment::kProgramPc;
	constexpr u32 kBlockBPc = kBlockAPc + 0x100;

	EeRecTestHarness h;
	h.SetGpr64(reg::v0, 0x0BADBADBADBADBADull); // stale-mirror sentinels
	h.SetGpr64(reg::v1, 0x0BADBADBADBADBADull);
	h.LoadProgramNoTerm({
		LUI(reg::v0, 0x1234),
		ORI(reg::v0, reg::v0, 0x5678),     // $v0 const 0x12345678 (movz/movk into a pin)
		LUI(reg::v1, 0xFFFF),
		ORI(reg::v1, reg::v1, 0x8001),     // $v1 const 0xFFFF...8001 (sign-ext, movn into a pin)
		J(kBlockBPc),
		NOP,                                // delay slot
	});
	h.WriteU32(kBlockBPc + 0,  DADDU(reg::t0, reg::v0, reg::zero)); // read $v0 via the pin
	h.WriteU32(kBlockBPc + 4,  DADDU(reg::t1, reg::v1, reg::zero)); // read $v1 via the pin
	h.WriteU32(kBlockBPc + 8,  J(kPark));
	h.WriteU32(kBlockBPc + 12, NOP);
	h.Run();
	h.ExpectGpr64(reg::t0, 0x0000000012345678ull);
	h.ExpectGpr64(reg::t1, 0xFFFFFFFFFFFF8001ull);
	h.ExpectGpr64(reg::v0, 0x0000000012345678ull); // write-through STR kept memory canonical
	h.ExpectGpr64(reg::v1, 0xFFFFFFFFFFFF8001ull);
	h.ExpectBlockLinked(kBlockAPc + 16, kBlockBPc); // J is the 5th insn
}

// ---------------------------------------------------------------------
// Arm A (const-into-pin), the dominant real-world shape: JAL sets $ra to a
// compile-time-const link address (pc+8). recJAL const-props it and the
// block-exit flush lands it in the $ra pin (x23) — under Arm A via a direct
// Mov-imm — after which the JAL target block reads it back through the pin.
// This is the "every JAL $ra link" triad the arm collapses; a broken
// materialization strands the return address that any real callee's JR $ra
// would then fault on. ExpectGpr64($ra) also holds the write-through STR.
// ---------------------------------------------------------------------
TEST(EeRecRegallocCoupling, JalRaLinkMaterializedIntoPinCrossBlock)
{
	constexpr u32 kBlockAPc = RecompilerTestEnvironment::kProgramPc;
	constexpr u32 kBlockBPc = kBlockAPc + 0x100;

	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		JAL(kBlockBPc),     // $ra = kBlockAPc + 8 (const link), static-link to B
		NOP,                // delay slot
	});
	h.WriteU32(kBlockBPc + 0, DADDU(reg::t0, reg::ra, reg::zero)); // read the $ra link via the pin
	h.WriteU32(kBlockBPc + 4, J(kPark));
	h.WriteU32(kBlockBPc + 8, NOP);
	h.Run();
	h.ExpectGpr64(reg::t0, static_cast<u64>(kBlockAPc + 8)); // link = JAL_pc + 8
	h.ExpectGpr64(reg::ra, static_cast<u64>(kBlockAPc + 8)); // write-through STR kept memory canonical
	h.ExpectBlockLinked(kBlockAPc, kBlockBPc); // JAL is the 1st insn
}
