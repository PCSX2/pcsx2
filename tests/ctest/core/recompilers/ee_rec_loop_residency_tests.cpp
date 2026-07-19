// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// SL-1 loop-carried residency: a self-loop block (terminal branch targeting
// its own startpc) compiles with a preheader that pins the loop-live GPRs in
// host registers and a flush-free back-edge that reconciles to the loop-top
// allocator state instead of the full tail flush + linked-B round trip.
//
// Path coverage note: under the harness, nextEventCycle == cycle at entry, so
// the FIRST back-edge always takes the event side exit (the cold spill stub →
// DispatcherEvent), and recEventTest's rescheduling then lets subsequent
// iterations run the resident B loop-top path until the loop exits. Every
// multi-iteration Run() therefore exercises the spill stub, the head
// re-entry/preheader reload, the resident back-edge, and the exit arm — and
// Run() diffs the full architectural state against the interpreter.

#include "harness/EeRecTestHarness.h"

#include "R5900.h"
#include "vtlb.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

extern bool recEeBlockIsLoopResident(u32 pc_query);
extern bool recEeLoopBackedgeInfo(u32 pc_query, uptr* site, uptr* stub);

namespace {
constexpr u32 kPark = RecompilerTestEnvironment::kParkingPc;
constexpr u32 kProgPc = RecompilerTestEnvironment::kProgramPc;
constexpr u32 kDataAddr = 0x00020000; // EE RAM scratch, away from the program

// The harness program page accumulates write→clear cycles across tests in
// this process and eventually flips to ProtMode_Manual — which legitimately
// excludes SL-1 loop residency (the inline SMC check must run per iteration).
// Tests that assert the introspection positively need a deterministic page
// state: full rec reset (clears manual_page/manual_counter) + vtlb block
// tracking reset (clears the page's protection mode back to None).
void ResetRecAndPageProtection()
{
	recCpu.Reset();
	mmap_ResetBlockTracking();
}
} // namespace

// Counting loop over non-pin-table regs (t0/t1/t2 → loop-pinned by the
// preheader): the accumulator must carry across iterations in a host reg and
// still land in memory on every observable exit.
TEST(EeRecLoopResidency, AccumulatorCarriesAcrossIterations)
{
	ResetRecAndPageProtection();
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0);    // accumulator
	h.SetGpr64(reg::t1, 7);    // iteration count
	h.SetGpr64(reg::t2, 3);    // step
	h.LoadProgramNoTerm({
		// 0x00 loop:
		ADDU(reg::t0, reg::t0, reg::t2),
		ADDIU(reg::t1, reg::t1, -1),
		BNE(reg::t1, reg::zero, -3), // → 0x00
		NOP,                          // delay slot
		ADDIU(reg::v0, reg::zero, 0x55),
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t0, 21ull);
	h.ExpectGpr64(reg::t1, 0ull);
	h.ExpectGpr64(reg::v0, 0x55ull);
	EXPECT_TRUE(recEeBlockIsLoopResident(kProgPc));
}

// The loop-resident introspection must NOT fire for a forward-branch block.
TEST(EeRecLoopResidency, ForwardBranchBlockNotResident)
{
	ResetRecAndPageProtection();
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 1);
	h.LoadProgramNoTerm({
		BNE(reg::t0, reg::zero, 2), // forward
		NOP,
		NOP,
		ADDIU(reg::v0, reg::zero, 9),
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 9ull);
	EXPECT_FALSE(recEeBlockIsLoopResident(kProgPc));
}

// Live work in the delay slot (the classic decrement-in-slot shape): the
// taken arm compiles the slot before the back-edge, so the slot's write must
// ride the resident state like any body op.
TEST(EeRecLoopResidency, DelaySlotWorkInLoop)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0);
	h.SetGpr64(reg::t1, 5);
	h.LoadProgramNoTerm({
		// 0x00 loop:
		ADDU(reg::t0, reg::t0, reg::t1), // t0 += t1 (5+4+3+2+1)
		BNE(reg::t1, reg::zero, -2),     // → 0x00 (checks pre-decrement t1... see below)
		ADDIU(reg::t1, reg::t1, -1),     // delay slot: t1--
		ADDIU(reg::v0, reg::zero, 0x66),
		J(kPark), NOP,
	});
	h.Run();
	// t1 = 5,4,3,2,1 accumulate 15; the pass that sees t1==0 adds 0, falls
	// through, and its delay slot still decrements to -1.
	h.ExpectGpr64(reg::t0, 15ull);
	h.ExpectGpr64(reg::t1, 0xffffffffffffffffull);
	h.ExpectGpr64(reg::v0, 0x66ull);
}

// Constant created INSIDE the loop body: the back-edge must materialize it
// (loop-top code was compiled with the reg non-const) — a dropped
// materialization reads stale state on iteration 2+.
TEST(EeRecLoopResidency, ConstInsideLoopMaterializedAcrossBackedge)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0);
	h.SetGpr64(reg::t1, 4);
	h.SetGpr64(reg::t2, 0xdead);
	h.LoadProgramNoTerm({
		// 0x00 loop:
		ORI(reg::t2, reg::zero, 5),      // t2 = const 5 (created in-body)
		ADDU(reg::t0, reg::t0, reg::t2),
		ADDIU(reg::t1, reg::t1, -1),
		BNE(reg::t1, reg::zero, -4),     // → 0x00
		NOP,
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t0, 20ull);
	h.ExpectGpr64(reg::t2, 5ull);
}

// 128-bit MMI write in the loop: the NEON quad goes dirty each iteration and
// the reconcile writes it back (S0 carries no NEON entries); the dual-
// residence invalidation rules must hold across the back-edge.
TEST(EeRecLoopResidency, MmiWriteInLoop)
{
	EeRecTestHarness h;
	h.SetGpr128(reg::t3, 0x0000000100000002ull, 0x0000000300000004ull);
	h.SetGpr128(reg::t2, 0x0000001000000010ull, 0x0000001000000010ull);
	h.SetGpr64(reg::t1, 3);
	h.LoadProgramNoTerm({
		// 0x00 loop:
		ee::PADDW(reg::t3, reg::t3, reg::t2),
		ADDIU(reg::t1, reg::t1, -1),
		BNE(reg::t1, reg::zero, -3), // → 0x00
		NOP,
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr128(reg::t3, 0x0000003100000032ull, 0x0000003300000034ull);
}

// Loads and stores inside the loop (inline fastmem): stores publish through
// the vtlb while scalar state rides registers; the tracked window diffs the
// memory result against the interpreter.
TEST(EeRecLoopResidency, StoreLoadInLoop)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0);
	h.SetGpr64(reg::t1, 6);
	h.SetGpr64(reg::t4, kDataAddr);
	h.TrackMemWindow(kDataAddr, 8);
	h.LoadProgramNoTerm({
		// 0x00 loop:
		SW(reg::t0, 0, reg::t4),
		LW(reg::t5, 0, reg::t4),
		ADDU(reg::t0, reg::t0, reg::t5), // t0 doubles (0,0,0...) — use +1 too
		ADDIU(reg::t0, reg::t0, 1),
		ADDIU(reg::t1, reg::t1, -1),
		BNE(reg::t1, reg::zero, -6), // → 0x00
		NOP,
		SW(reg::t0, 4, reg::t4),
		J(kPark), NOP,
	});
	h.Run();
	// Values evolve t0 -> 2*t0+1 per iteration from 0: 1,3,7,15,31,63.
	h.ExpectGpr64(reg::t0, 63ull);
	EXPECT_EQ(h.ReadU32(kDataAddr), 31u);     // last in-loop store (pre-final add)
	EXPECT_EQ(h.ReadU32(kDataAddr + 4), 63u); // post-loop store
}

// SMC invalidation of the loop block between runs: the second dispatch must
// recompile (fresh semantics), not reuse stale resident code.
TEST(EeRecLoopResidency, SmcInvalidateLoopBlockRecompiles)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0);
	h.SetGpr64(reg::t1, 4);
	h.SetGpr64(reg::t2, 1);
	h.LoadProgramNoTerm({
		// 0x00 loop:
		ADDU(reg::t0, reg::t0, reg::t2),
		ADDIU(reg::t1, reg::t1, -1),
		BNE(reg::t1, reg::zero, -3),
		NOP,
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t0, 4ull);

	// Rewrite the accumulate step: ADDU → ADDIU t0, t0, 7 via the vtlb write
	// path (fires Cpu->Clear on the compiled block).
	h.TriggerSmc(kProgPc, ADDIU(reg::t0, reg::t0, 7));

	h.SetGpr64(reg::t0, 0);
	h.SetGpr64(reg::t1, 4);
	h.SetGpr64(reg::t2, 1);
	h.Run(EeRecTestHarness::RunMode::PreserveCache);
	h.ExpectGpr64(reg::t0, 28ull);
}

// recClear must atomically repoint the internal back-edge B to the block's
// cold spill stub (Arm64BaseBlocks::Remove) — the entry redirect alone cannot
// catch it, and a cleared self-loop must exit its stale code at the next
// back-edge, not the next event.
TEST(EeRecLoopResidency, RecClearRepointsBackedgeToSpillStub)
{
	ResetRecAndPageProtection();
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0);
	h.SetGpr64(reg::t1, 4);
	h.LoadProgramNoTerm({
		// 0x00 loop:
		ADDIU(reg::t0, reg::t0, 1),
		ADDIU(reg::t1, reg::t1, -1),
		BNE(reg::t1, reg::zero, -3),
		NOP,
		J(kPark), NOP,
	});
	h.Run();
	uptr site = 0, stub = 0;
	ASSERT_TRUE(recEeLoopBackedgeInfo(kProgPc, &site, &stub));
	ASSERT_NE(site, 0u);
	ASSERT_NE(stub, 0u);

	// Pre-clear the site holds a backward B (negative imm26 → to the loop-top,
	// which precedes it). Any recClear covering the block must rewrite it to a
	// forward B targeting the spill stub.
	const u32 before = *reinterpret_cast<const u32*>(site);
	EXPECT_EQ(before >> 26, 0x5u); // unconditional B
	// Deterministic clear via the production SIGSEGV-backpatch entry point
	// (recCpu.Clear on the faulting pc).
	h.SimulateFastmemFault(kProgPc);

	uptr site2 = 0, stub2 = 0;
	EXPECT_FALSE(recEeLoopBackedgeInfo(kProgPc, &site2, &stub2)) << "block still present after clear";
	const u32 after = *reinterpret_cast<const u32*>(site);
	const u32 expected = 0x14000000u | ((static_cast<u32>((stub - site) >> 2)) & 0x03FFFFFFu);
	EXPECT_EQ(after, expected);
}

// Many iterations: crosses several event-check reschedules, so the loop
// alternates spill-stub exits and resident re-entries repeatedly.
TEST(EeRecLoopResidency, ManyIterationsSurviveEventExits)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0);
	h.SetGpr64(reg::t1, 100);
	h.LoadProgramNoTerm({
		// 0x00 loop:
		ADDIU(reg::t0, reg::t0, 1),
		ADDIU(reg::t1, reg::t1, -1),
		BNE(reg::t1, reg::zero, -3),
		NOP,
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t0, 100ull);
	h.ExpectGpr64(reg::t1, 0ull);
}
