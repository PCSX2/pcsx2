// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Regression coverage for the EE recompiler's timeout-loop speedhack
// (recSkipTimeoutLoop / its detection in recRecompile).
//
// The recompiler only treats a block as a skippable timeout loop when the
// block's terminating branch targets the block's OWN start (a self-loop):
//
//     s_nBlockFF = false;
//     if (s_branchTo == startpc) { ...analyse... }
//     else { is_timeout_loop = false; }
//
// Without that `else`, a block of the shape
//   addiu reg,reg,-1 / bne reg,zero,<FORWARD> / nop
// — i.e. the counter-decrement TOP of a real counted compute loop whose first
// branch is an early-exit guard that jumps FORWARD into the body — gets
// misclassified as a timeout loop. recSkipTimeoutLoop then fast-forwards the
// counter to zero and jumps to the block end, skipping the loop body entirely.
//
// A real game exercises this pattern in its per-frame DMA-list build loop: the
// counter-decrement TOP block ends in a forward early-exit branch, not a
// self-loop; misclassifying it as a timeout loop skips the body and corrupts
// the GIF DMA chain, so the game's completion wait loops never satisfy and
// gameplay hangs.
//
// This test runs jit vs interp through Run()'s auto-diff with the WaitLoop
// speedhack enabled. The timeout-loop skip is jit-only (the interpreter always
// executes the body), so a misdetected loop diverges and Run() fails.
//
// A genuine self-looping timeout loop is deliberately NOT auto-diffed here: when
// the speedhack fires it fast-forwards the counter to nextEventCycle and exits via
// the event path, leaving the counter only partially drained — i.e. it diverges
// from naive interpreter spinning BY DESIGN (correctness then depends on the
// in-game event the loop was waiting for, which the harness doesn't model). The
// self-loop guard provably leaves genuine timeout loops unaffected: a self-loop
// has s_branchTo == startpc, so the `else { is_timeout_loop = false; }` guard
// never fires for it.

#include "harness/EeRecTestHarness.h"

#include "Config.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kPark = RecompilerTestEnvironment::kParkingPc;

// RAII toggle for EmuConfig.Speedhacks.WaitLoop (the gate on recSkipTimeoutLoop).
struct ScopedWaitLoop
{
	bool prev;
	explicit ScopedWaitLoop(bool on) : prev(EmuConfig.Speedhacks.WaitLoop) { EmuConfig.Speedhacks.WaitLoop = on; }
	~ScopedWaitLoop() { EmuConfig.Speedhacks.WaitLoop = prev; }
};
} // namespace

// A counted compute loop whose TOP block is `addiu ctr,-1 / bne ctr,zero,FWD /
// nop`, where the bne is an early-exit guard jumping FORWARD into the body — NOT
// a self-loop. The body does observable work ($a1 += 5 per iteration). Without
// the self-loop guard, the jit would skip the body ($a1 stays 0) while the
// interpreter runs it ($a1 == 15). Run()'s jit-vs-interp auto-diff catches
// the divergence.
//
// Layout (offsets from kProgramPc):
//   +0x00  addiu t4,t4,-1          ; loop TOP (startpc); counter--
//   +0x04  bne   t4,zero,+0x14     ; early-exit guard -> body (FORWARD)
//   +0x08  nop                     ; delay slot
//   +0x0C  j     park              ; loop EXIT (taken when t4 falls through == 0)
//   +0x10  nop                     ; j delay slot
//   +0x14  addiu a1,a1,5           ; BODY: observable work
//   +0x18  bne   t4,zero,+0x00     ; loop back to TOP
//   +0x1C  nop                     ; delay slot
TEST(EeRecTimeoutLoop, ForwardEarlyExitGuardIsNotATimeoutLoop)
{
	ScopedWaitLoop wl(true);

	EeRecTestHarness h;
	h.SetGpr64(reg::t4, 4);   // 3 body iterations (t4: 4->3->2->1, then exits at 0)
	h.SetGpr64(reg::a1, 0);
	h.LoadProgramNoTerm({
		ADDIU(reg::t4, reg::t4, -1),       // +0x00 TOP
		BNE(reg::t4, reg::zero, 3),        // +0x04 -> body at +0x14 (PC+4 +0xC)
		NOP,                               // +0x08 delay slot
		J(kPark),                          // +0x0C exit
		NOP,                               // +0x10 j delay slot
		ADDIU(reg::a1, reg::a1, 5),        // +0x14 BODY
		BNE(reg::t4, reg::zero, -7),       // +0x18 -> TOP at +0x00 (PC+4 -0x1C)
		NOP,                               // +0x1C delay slot
	});
	h.Run();

	// If the loop body were skipped, a1 would be 0.
	h.ExpectGpr64(reg::a1, 15ull);
	h.ExpectGpr64(reg::t4, 0ull);
}

// ---------------------------------------------------------------------------
// Negative-shape sweep.
//
// The timeout-loop skip fires only when ALL of these hold (the EE recompiler's
// recRecompile detection + StartRecomp guard + recSkipTimeoutLoop):
//   (1) is_timeout_loop: the block is ONLY [nops] addiu reg,reg,-N [nops]
//       bne reg,zero,target nop — any other instruction clears it, a second
//       decrement clears it, a bne whose Rt!=zero clears it;
//   (2) timeout_reg >= 0 && timeout_has_bne;
//   (3) s_branchTo == startpc — the branch is a SELF-loop (the guard the N1
//       test above covers).
// Each negative below violates exactly ONE condition while otherwise looking
// timeout-loop-shaped, and asserts jit == interp (Run()'s auto-diff) with
// WaitLoop ON — i.e. the block must be recompiled normally, not skipped.
//
// A POSITIVE test (a genuine all-nop self-loop that SHOULD be skipped) is
// deliberately omitted: when the skip fires it fast-forwards the counter to the
// next event and exits via the event path, leaving the counter only partially
// drained — it diverges from naive interpreter spinning BY DESIGN, so Run()'s
// auto-diff cannot validate it. The positive side is covered system-level by a
// WaitLoop-on-vs-off EE-RAM differential.
// ---------------------------------------------------------------------------

// A genuine SELF-loop (bne back to startpc — passes condition 3) whose block has
// a SECOND in-place decrement in the body (`addiu a1,a1,-1`). The canonical
// timeout loop has exactly one decrement; a second one is off-pattern and the
// block must be recompiled normally. This is defended in depth: the
// "timeout_reg already set" term clears is_timeout_loop on the second addiu, and
// even if that term were removed the second addiu would reassign timeout_reg to
// $a1, after which the "timeout_reg != bne's Rs" backstop rejects the
// `bne t4` (verified: breaking the first term alone leaves this test green — the
// backstop catches it). So N2 locks the combined invariant "a multi-decrement
// self-loop is never a timeout loop"; the observable side effect ($a1, a register
// recSkipTimeoutLoop never touches) goes stale if the block is ever skipped.
//
//   +0x00 TOP: addiu t4,t4,-1        ; counter-- (timeout_reg = t4)
//   +0x04      addiu a1,a1,-1        ; BODY: off-pattern 2nd decrement
//   +0x08      bne   t4,zero,TOP     ; SELF-loop back to +0x00
//   +0x0C      nop                   ; delay slot
//   +0x10      j     park            ; loop exit
//   +0x14      nop
TEST(EeRecTimeoutLoop, SelfLoopWithAluBodyIsNotSkipped)
{
	ScopedWaitLoop wl(true);

	EeRecTestHarness h;
	h.SetGpr64(reg::t4, 4);   // 4 iterations: t4 4->3->2->1->0
	h.SetGpr64(reg::a1, 100); // body decrements a1 once per iteration
	h.LoadProgramNoTerm({
		ADDIU(reg::t4, reg::t4, -1),   // +0x00 TOP
		ADDIU(reg::a1, reg::a1, -1),   // +0x04 body
		BNE(reg::t4, reg::zero, -3),   // +0x08 -> TOP (PC+4 -0xC)
		NOP,                           // +0x0C delay slot
		J(kPark),                      // +0x10 exit
		NOP,                           // +0x14 j delay slot
	});
	h.Run();

	h.ExpectGpr64(reg::a1, 96ull); // 100 - 4; stays 100 if the body were skipped
	h.ExpectGpr64(reg::t4, 0ull);
}

// A SELF-loop whose body performs a memory store. The `sw` (opcode !=
// addiu/bne/nop) is the SOLE is_timeout_loop killer here (a different branch
// than N2's second-decrement path), so this isolates that guard. If the loop
// were skipped the store would never land and the seeded sentinel would survive.
//
//   +0x00 TOP: addiu t4,t4,-1        ; counter-- (timeout_reg = t4)
//   +0x04      sw    t4,0(t2)        ; store counter -> clears is_timeout_loop
//   +0x08      bne   t4,zero,TOP     ; SELF-loop back to +0x00
//   +0x0C      nop                   ; delay slot
//   +0x10      j     park            ; loop exit
//   +0x14      nop
TEST(EeRecTimeoutLoop, SelfLoopWithStoreBodyIsNotSkipped)
{
	ScopedWaitLoop wl(true);

	constexpr u32 kData = RecompilerTestEnvironment::kScratchAddr;

	EeRecTestHarness h;
	h.SetGpr64(reg::t4, 3);     // 3 iterations: stores t4 = 2, 1, then 0 (last store wins)
	h.SetGpr64(reg::t2, kData); // store address
	h.WriteU32(kData, 0xEEEEEEEEu); // sentinel: survives iff the body is skipped
	h.TrackMemWindow(kData, 4);     // include the store in Run()'s jit-vs-interp diff
	h.LoadProgramNoTerm({
		ADDIU(reg::t4, reg::t4, -1),   // +0x00 TOP
		SW(reg::t4, 0, reg::t2),       // +0x04 store counter
		BNE(reg::t4, reg::zero, -3),   // +0x08 -> TOP (PC+4 -0xC)
		NOP,                           // +0x0C delay slot
		J(kPark),                      // +0x10 exit
		NOP,                           // +0x14 j delay slot
	});
	h.Run();

	// Body ran on both sides: the final counter value (0) reached memory, not the sentinel.
	EXPECT_EQ(h.ReadU32(kData), 0u);
	h.ExpectGpr64(reg::t4, 0ull);
}

// A counted SELF-loop whose terminating `bne` compares the counter against a
// NONZERO register (a "spin until t4 == a2" wait), not against zero. The Rt!=0
// check clears is_timeout_loop, so it must run to its natural exit.
//
//   +0x00 TOP: addiu t4,t4,-1        ; counter-- (timeout_reg = t4)
//   +0x04      bne   t4,a2,TOP       ; Rt = a2 (nonzero) -> clears is_timeout_loop
//   +0x08      nop                   ; delay slot
//   +0x0C      j     park            ; loop exit (t4 == a2)
//   +0x10      nop
TEST(EeRecTimeoutLoop, CounterComparedAgainstNonzeroRegIsNotSkipped)
{
	ScopedWaitLoop wl(true);

	EeRecTestHarness h;
	h.SetGpr64(reg::t4, 5); // loop while t4 != a2: 5->4->3->2 (stops at 2)
	h.SetGpr64(reg::a2, 2);
	h.LoadProgramNoTerm({
		ADDIU(reg::t4, reg::t4, -1),   // +0x00 TOP
		BNE(reg::t4, reg::a2, -2),     // +0x04 -> TOP (PC+4 -0x8)
		NOP,                           // +0x08 delay slot
		J(kPark),                      // +0x0C exit
		NOP,                           // +0x10 j delay slot
	});
	h.Run();

	// Natural exit leaves t4 == a2 == 2; a wrongly-fired skip would drive it toward 0.
	h.ExpectGpr64(reg::t4, 2ull);
	h.ExpectGpr64(reg::a2, 2ull);
}

// ===========================================================================
// Wait-loop (s_nBlockFF) DETECTOR coverage (AX-07). The detector was widened
// from "all-NOP self-loop only" to the x86 hazard tracker (ix86-32/
// iR5900.cpp:2438-2515): a self-loop qualifies as long as it never writes a
// register it already read (constant/loaded registers excepted) and touches
// no machine state beyond registers — the shape of real hardware-poll idle
// loops. The verdict itself isn't observable through architectural state
// (both engines are cycle-budget bounded), so these tests read it via the
// g_eeRecLastBlockFF hook after a JIT-only compile+run.
// ===========================================================================

extern bool g_eeRecLastBlockFF;

// Classic hardware-poll idle loop: lw STATUS; andi; beq back. The old
// NOP-only detector rejected this (red before the AX-07 port).
TEST(EeRecWaitLoopDetect, PollLoadTestBranchIsWaitLoop)
{
	constexpr u32 kData = RecompilerTestEnvironment::kScratchAddr;

	EeRecTestHarness h;
	h.SetGpr64(reg::s0, kData);
	h.WriteU32(kData, 0);              // polled value never changes
	h.LoadProgramNoTerm({
		LW(reg::t0, 0, reg::s0),       // +0x00 TOP: poll
		ANDI(reg::t1, reg::t0, 1),     // +0x04 test
		BEQ(reg::t1, reg::zero, -3),   // +0x08 -> TOP while bit clear
		NOP,                           // +0x0C delay slot
		J(kPark),                      // +0x10 exit (if bit ever set)
		NOP,                           // +0x14
	});
	h.RunJitNoDiff();                  // spins to the cycle budget; bounded
	EXPECT_TRUE(g_eeRecLastBlockFF)
		<< "load/test/branch poll self-loop must be detected as a wait loop";
}

// A self-loop that WRITES a register it already read (a genuine compute
// loop) must never be classified as a wait loop — the hazard rule.
TEST(EeRecWaitLoopDetect, SelfLoopWithReadThenWriteHazardIsNot)
{
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		ADDIU(reg::t0, reg::t0, 1),    // +0x00 TOP: t0 read then written
		BEQ(reg::zero, reg::zero, -2), // +0x04 -> TOP always
		NOP,                           // +0x08 delay slot
	});
	h.RunJitNoDiff();
	EXPECT_FALSE(g_eeRecLastBlockFF)
		<< "counter self-loop (write-after-read) must NOT be a wait loop";
}

// The old detector's only accepted shape keeps working.
TEST(EeRecWaitLoopDetect, AllNopSelfLoopStillDetected)
{
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		BEQ(reg::zero, reg::zero, -1), // +0x00 TOP -> TOP
		NOP,                           // +0x04 delay slot
	});
	h.RunJitNoDiff();
	EXPECT_TRUE(g_eeRecLastBlockFF);
}
