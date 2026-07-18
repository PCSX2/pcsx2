// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// SL-03 superblocks: forward conditional branches become continuation sites —
// the not-taken path compiles straight through (no tail, no head reload), the
// taken arm becomes a cold side exit outlined after the block tail. These
// tests pin block formation (via recEeBlockGuestSize), both runtime paths
// (Run() diffs the full architectural state against the interpreter), the
// delay-slot-on-both-paths contract, snapshot-flush coherence at taken exits,
// the S1×S2 composition (a loop with an internal forward branch becomes one
// resident self-loop), and the deliberate non-sites (backward, likely,
// unconditional-idiom, cap overflow).

#include "harness/EeRecTestHarness.h"

#include "R5900.h"
#include "vtlb.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

extern u32 recEeBlockGuestSize(u32 pc_query);
extern bool recEeBlockIsLoopResident(u32 pc_query);

namespace {
constexpr u32 kPark = RecompilerTestEnvironment::kParkingPc;
constexpr u32 kProgPc = RecompilerTestEnvironment::kProgramPc;
constexpr u32 kDataAddr = 0x00020000; // EE RAM scratch, away from the program

// Formation-asserting tests need a deterministic page state (the harness
// program page can flip to ProtMode_Manual across accumulated write→clear
// cycles in this process; manual blocks still superblock, but a fresh state
// keeps size assertions honest) — same recipe as the loop-residency tests.
void ResetRecAndPageProtection()
{
	recCpu.Reset();
	mmap_ResetBlockTracking();
}
} // namespace

// A forward BNE no longer ends the block: the compiled range spans the branch,
// its delay slot, both the skipped and merge instructions, and the terminal J.
TEST(EeRecSuperblock, ForwardBranchFusesBlock)
{
	ResetRecAndPageProtection();
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0); // not taken
	h.LoadProgramNoTerm({
		BNE(reg::t0, reg::zero, 2), // idx0 → idx4 when taken
		NOP,                        // idx1 ds
		ADDIU(reg::t1, reg::zero, 5), // idx2 fallthrough-only
		ADDIU(reg::t2, reg::zero, 7), // idx3 merge
		J(kPark), NOP,              // idx4/5
	});
	h.Run();
	h.ExpectGpr64(reg::t1, 5ull);
	h.ExpectGpr64(reg::t2, 7ull);
	// Pre-SL-03 this block was 2 insns (branch + delay slot).
	EXPECT_EQ(recEeBlockGuestSize(kProgPc), 6u);
}

// Same program, condition true: the cold taken side exit runs — it must skip
// the fallthrough-only instruction and land on the merge point.
TEST(EeRecSuperblock, TakenSideExitCorrect)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 1); // taken
	h.LoadProgramNoTerm({
		BNE(reg::t0, reg::zero, 2),
		NOP,
		ADDIU(reg::t1, reg::zero, 5),
		ADDIU(reg::t2, reg::zero, 7),
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t1, 0ull);
	h.ExpectGpr64(reg::t2, 7ull);
}

// The delay slot executes on BOTH paths: inline on the fallthrough, recompiled
// from the snapshot inside the side exit on the taken path.
TEST(EeRecSuperblock, DelaySlotExecutesOnBothPaths)
{
	for (const u64 cond : {0ull, 1ull})
	{
		EeRecTestHarness h;
		h.SetGpr64(reg::t0, cond);
		h.SetGpr64(reg::t3, 0);
		h.LoadProgramNoTerm({
			BNE(reg::t0, reg::zero, 2),      // idx0
			ADDIU(reg::t3, reg::t3, 11),     // idx1 ds: both paths
			ADDIU(reg::t1, reg::zero, 5),    // idx2 fallthrough-only
			ADDIU(reg::t2, reg::zero, 7),    // idx3 merge
			J(kPark), NOP,
		});
		h.Run();
		h.ExpectGpr64(reg::t3, 11ull);
		h.ExpectGpr64(reg::t1, cond ? 0ull : 5ull);
		h.ExpectGpr64(reg::t2, 7ull);
	}
}

// Dirty scalar + dirty 128-bit NEON state at the branch: the side exit's
// flush is emitted from the branch-point snapshot and must publish exactly
// that state before linking out (Run() diffs everything vs the interpreter).
TEST(EeRecSuperblock, TakenExitFlushesDirtyState)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 3);
	h.SetGpr64(reg::t1, 4);
	h.SetGpr128(reg::t4, 0x0000000100000002ull, 0x0000000300000004ull);
	h.SetGpr128(reg::t5, 0x0000001000000010ull, 0x0000001000000010ull);
	h.LoadProgramNoTerm({
		ADDU(reg::t2, reg::t0, reg::t1),   // t2 = 7, dirty resident
		ee::PADDW(reg::t4, reg::t4, reg::t5), // NEON quad dirty
		BNE(reg::t2, reg::zero, 2),        // taken → side exit must flush t2/t4
		NOP,
		ADDIU(reg::t2, reg::zero, 0),      // fallthrough-only (skipped)
		ADDIU(reg::v0, reg::zero, 9),
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t2, 7ull);
	h.ExpectGpr128(reg::t4, 0x0000001100000012ull, 0x0000001300000014ull);
	h.ExpectGpr64(reg::v0, 9ull);
}

// A compile-time constant created before the branch stays const through the
// continuation — the value must still be architecturally correct on both
// paths (the taken exit materializes it in its flush).
TEST(EeRecSuperblock, ConstRidesThroughContinuation)
{
	for (const u64 cond : {0ull, 1ull})
	{
		EeRecTestHarness h;
		h.SetGpr64(reg::t0, cond);
		h.LoadProgramNoTerm({
			ORI(reg::t2, reg::zero, 0x123),  // const before the branch
			BNE(reg::t0, reg::zero, 2),
			NOP,
			ADDIU(reg::t1, reg::zero, 5),    // fallthrough-only
			ADDU(reg::t3, reg::t2, reg::t2), // merge: uses the const
			J(kPark), NOP,
		});
		h.Run();
		h.ExpectGpr64(reg::t2, 0x123ull);
		h.ExpectGpr64(reg::t3, 0x246ull);
		h.ExpectGpr64(reg::t1, cond ? 0ull : 5ull);
	}
}

// Two continuation sites in one block, all four taken/not-taken combinations.
TEST(EeRecSuperblock, MultipleContinuationSites)
{
	for (const u64 c0 : {0ull, 1ull})
	{
		for (const u64 c1 : {0ull, 1ull})
		{
			EeRecTestHarness h;
			h.SetGpr64(reg::t0, c0);
			h.SetGpr64(reg::t1, c1);
			h.LoadProgramNoTerm({
				BNE(reg::t0, reg::zero, 2),   // idx0 → idx3 (skip the t2 add, land ON the 2nd branch)
				NOP,
				ADDIU(reg::t2, reg::t2, 1),   // idx2: only when !c0
				BNE(reg::t1, reg::zero, 2),   // idx3 → idx6 (skip the t3 add) — both paths reach this
				NOP,
				ADDIU(reg::t3, reg::t3, 1),   // idx5: only when !c1
				NOP,                          // idx6
				ADDIU(reg::v0, reg::zero, 9), // idx7
				J(kPark), NOP,
			});
			h.Run();
			h.ExpectGpr64(reg::t2, c0 ? 0ull : 1ull);
			h.ExpectGpr64(reg::t3, c1 ? 0ull : 1ull);
			h.ExpectGpr64(reg::v0, 9ull);
		}
	}
}

// Backward conditionals still end the block (loops stay S1's shape).
TEST(EeRecSuperblock, BackwardBranchStillEndsBlock)
{
	ResetRecAndPageProtection();
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0);
	h.SetGpr64(reg::t1, 3);
	h.LoadProgramNoTerm({
		// 0x00 loop:
		ADDIU(reg::t0, reg::t0, 1),
		ADDIU(reg::t1, reg::t1, -1),
		BNE(reg::t1, reg::zero, -3), // → 0x00
		NOP,
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t0, 3ull);
	EXPECT_EQ(recEeBlockGuestSize(kProgPc), 4u); // loop only — not fused onward
}

// Likely branches keep ending the block (wave-1 scope: their delay slot is
// taken-path-only, a different continuation shape).
TEST(EeRecSuperblock, LikelyBranchStillEndsBlock)
{
	ResetRecAndPageProtection();
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0);
	h.LoadProgramNoTerm({
		ee::BNEL(reg::t0, reg::zero, 2), // forward likely — terminal
		NOP,
		ADDIU(reg::t1, reg::zero, 5),
		ADDIU(reg::t2, reg::zero, 7),
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t1, 5ull);
	h.ExpectGpr64(reg::t2, 7ull);
	EXPECT_EQ(recEeBlockGuestSize(kProgPc), 2u);
}

// BEQ rs==rt is the unconditional-`b` idiom: always taken, so the fallthrough
// is unreachable and the block must end there.
TEST(EeRecSuperblock, UnconditionalBeqIdiomEndsBlock)
{
	ResetRecAndPageProtection();
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		BEQ(reg::zero, reg::zero, 2), // b → idx4
		NOP,
		ADDIU(reg::t1, reg::zero, 5), // unreachable
		NOP,
		ADDIU(reg::v0, reg::zero, 9), // idx4
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t1, 0ull);
	h.ExpectGpr64(reg::v0, 9ull);
	EXPECT_EQ(recEeBlockGuestSize(kProgPc), 2u);
}

// BNE rs==rt never takes: pure fallthrough, fused with no side exit.
TEST(EeRecSuperblock, NeverTakenBneSameRegContinues)
{
	ResetRecAndPageProtection();
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 42);
	h.LoadProgramNoTerm({
		BNE(reg::t0, reg::t0, 2),
		NOP,
		ADDIU(reg::t1, reg::zero, 5),
		ADDIU(reg::t2, reg::zero, 7),
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t1, 5ull);
	h.ExpectGpr64(reg::t2, 7ull);
	EXPECT_EQ(recEeBlockGuestSize(kProgPc), 6u);
}

// Compile-time const-resolved-taken branch is terminal (everything after is
// unreachable on this compile); const-resolved-not-taken continues inline.
TEST(EeRecSuperblock, ConstResolvedTakenEndsBlock)
{
	ResetRecAndPageProtection();
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		ORI(reg::t0, reg::zero, 1),   // t0 = const 1
		BNE(reg::t0, reg::zero, 2),   // resolved taken → idx4
		NOP,
		ADDIU(reg::t2, reg::zero, 7), // unreachable
		ADDIU(reg::v0, reg::zero, 9), // idx4
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t2, 0ull);
	h.ExpectGpr64(reg::v0, 9ull);
	EXPECT_EQ(recEeBlockGuestSize(kProgPc), 3u); // ORI + BNE + ds
}

TEST(EeRecSuperblock, ConstResolvedNotTakenContinues)
{
	ResetRecAndPageProtection();
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		ORI(reg::t0, reg::zero, 0),   // t0 = const 0
		BNE(reg::t0, reg::zero, 2),   // resolved not-taken → falls through
		NOP,
		ADDIU(reg::t2, reg::zero, 7),
		ADDIU(reg::v0, reg::zero, 9),
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t2, 7ull);
	h.ExpectGpr64(reg::v0, 9ull);
	EXPECT_EQ(recEeBlockGuestSize(kProgPc), 7u);
}

// The per-block site cap (8): the ninth forward conditional ends the block.
TEST(EeRecSuperblock, SiteCapEndsBlockAtNinthBranch)
{
	ResetRecAndPageProtection();
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0);
	std::vector<u32> prog;
	for (int i = 0; i < 9; i++)
	{
		prog.push_back(BNE(reg::t0, reg::zero, 1)); // → its own fallthrough
		prog.push_back(NOP);
	}
	prog.push_back(ADDIU(reg::v0, reg::zero, 9));
	prog.push_back(J(kPark));
	prog.push_back(NOP);
	h.LoadProgramNoTerm(prog);
	h.Run();
	h.ExpectGpr64(reg::v0, 9ull);
	EXPECT_EQ(recEeBlockGuestSize(kProgPc), 18u); // 8 fused pairs + terminal pair
}

// The S1×S2 composition prize: a loop whose body contains a forward
// conditional used to be split into two blocks — now it is ONE self-loop
// superblock and SL-1 makes it loop-resident. Both body paths iterate.
TEST(EeRecSuperblock, LoopWithInternalForwardBranchBecomesResident)
{
	for (const u64 skip : {0ull, 1ull})
	{
		ResetRecAndPageProtection();
		EeRecTestHarness h;
		h.SetGpr64(reg::t0, 0);
		h.SetGpr64(reg::t1, 5);
		h.SetGpr64(reg::t2, 0);
		h.SetGpr64(reg::t3, skip);
		h.LoadProgramNoTerm({
			// 0x00 loop:
			ADDIU(reg::t0, reg::t0, 1),   // idx0: iteration count
			BNE(reg::t3, reg::zero, 2),   // idx1: forward → idx4 (skip the add)
			NOP,                          // idx2 ds
			ADDIU(reg::t2, reg::t2, 1),   // idx3: only when !skip
			ADDIU(reg::t1, reg::t1, -1),  // idx4: merge
			BNE(reg::t1, reg::zero, -6),  // idx5: → 0x00
			NOP,                          // idx6 ds
			J(kPark), NOP,
		});
		h.Run();
		h.ExpectGpr64(reg::t0, 5ull);
		h.ExpectGpr64(reg::t1, 0ull);
		h.ExpectGpr64(reg::t2, skip ? 0ull : 5ull);
		EXPECT_EQ(recEeBlockGuestSize(kProgPc), 7u) << "loop body not fused";
		EXPECT_TRUE(recEeBlockIsLoopResident(kProgPc)) << "fused loop not resident";
	}
}

// SMC anywhere in the fused range must invalidate the whole superblock: a
// rewrite AFTER the continuation branch changes the recompiled semantics.
TEST(EeRecSuperblock, SmcInFusedRegionRecompiles)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0);
	h.LoadProgramNoTerm({
		BNE(reg::t0, reg::zero, 2), // idx0
		NOP,
		ADDIU(reg::t1, reg::zero, 5), // idx2
		ADDIU(reg::t2, reg::zero, 7), // idx3 ← rewritten below
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t2, 7ull);

	h.TriggerSmc(kProgPc + 3 * 4, ADDIU(reg::t2, reg::zero, 0x55));

	h.SetGpr64(reg::t0, 0);
	h.Run(EeRecTestHarness::RunMode::PreserveCache);
	h.ExpectGpr64(reg::t2, 0x55ull);
}

// Branch-into-delay-slot loop (UYA's dcache-flush idiom): a forward
// conditional at the block's FIRST instruction becomes a continuation site,
// and a later backward branch targets that site's DELAY SLOT. The
// backward-split clamp must NOT truncate the block to zero length (which
// compiles to an unconditional self-linked B with no event check — a hard
// wedge); it falls back to ending at the backward branch. Regression test
// for the SL-05 live-game hang: on regression this test hangs (CI timeout)
// rather than failing an assertion.
TEST(EeRecSuperblock, HeadBranchDelaySlotLoopDoesNotWedge)
{
	ResetRecAndPageProtection();
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0);
	h.SetGpr64(reg::t2, 3);
	h.LoadProgramNoTerm({
		BEQ(reg::t2, reg::zero, 4),  // idx0: exit → idx5 (continuation site at startpc)
		ADDIU(reg::t2, reg::t2, -1), // idx1: ds — the backward target below
		ADDIU(reg::t0, reg::t0, 1),  // idx2: body
		BGTZ(reg::t2, -3),       // idx3: → idx1 (into the delay slot)
		NOP,                         // idx4: ds
		ADDIU(reg::v0, reg::zero, 9),// idx5: exit
		J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::t0, 3ull);
	h.ExpectGpr64(reg::t2, 0ull);
	h.ExpectGpr64(reg::v0, 9ull);
	// Degenerate-clamp fallback: block spans head branch through the backward
	// branch (+ds) instead of splitting to zero length.
	EXPECT_EQ(recEeBlockGuestSize(kProgPc), 5u);
}

// Store/load traffic straddling a continuation site: inline fastmem stores
// publish before the branch, loads after it read them, both paths.
TEST(EeRecSuperblock, MemoryTrafficAcrossContinuation)
{
	for (const u64 cond : {0ull, 1ull})
	{
		EeRecTestHarness h;
		h.SetGpr64(reg::t0, cond);
		h.SetGpr64(reg::t4, kDataAddr);
		h.SetGpr64(reg::t1, 0x1234);
		h.TrackMemWindow(kDataAddr, 8);
		h.LoadProgramNoTerm({
			SW(reg::t1, 0, reg::t4),      // idx0
			BNE(reg::t0, reg::zero, 2),   // idx1 → idx5
			NOP,
			ADDIU(reg::t1, reg::t1, 1),   // idx3: only when !cond
			NOP,                          // idx4
			LW(reg::t2, 0, reg::t4),      // idx5: reads the pre-branch store
			SW(reg::t1, 4, reg::t4),
			J(kPark), NOP,
		});
		h.Run();
		h.ExpectGpr64(reg::t2, 0x1234ull);
		EXPECT_EQ(h.ReadU32(kDataAddr + 4), cond ? 0x1234u : 0x1235u);
	}
}
