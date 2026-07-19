// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// contentMap identity must not survive content drift.
//
// A microProgram's contentHash is anchored at mVUcreateProg over the whole
// live micro-memory image and deliberately never recomputed (the contentMap
// key must stay stable). But under !doWholeProgCompare a program's content
// CAN change after anchoring: a write to bytes outside its compiled ranges
// leaves it cached (correct — range-aware invalidation), and a later
// extension compile into the written region reads the NEW live bytes and
// syncs them into prog.data via mVUcacheProg. The instance is then a mixed
// compile that no longer corresponds to its anchor image, while the
// contentMap still serves it for that anchor hash — with no memcmp confirm.
//
// Consequence (the A/B/A double-buffer pattern): re-upload the anchor image
// and the next full search rejects the program at the per-PC deque (ranges
// memcmp fails over the drifted region — correct!) but then re-installs the
// SAME stale program via the contentMap hash hit, executing code compiled
// from the other image. Silent wrong execution; also poisons the on-disk
// ProgCache, which persists payloads keyed by the same anchor hash.
//
// The test drives the production dispatcher:
//   1. establish program X over image A (a tail region at kTail is part of
//      the anchor image but not compiled — nothing jumps to it yet);
//   2. rewrite the tail (image B) + Clear — X survives (write is outside
//      its compiled ranges);
//   3. synthetic mid-program resume (start_pc=0, TPC=kTail — production
//      shape after a cycle-budget break): block-miss inside X extends it
//      over the tail, compiling the B bytes — X has now drifted from its
//      anchor;
//   4. restore the tail (image A again) + Clear — now overlaps X's grown
//      ranges, so X's quick slot is nuked;
//   5. dispatch (start_pc=0, TPC=kTail) again: the deque walk correctly
//      rejects X, but the contentMap lookup for hash(A) must NOT serve the
//      drifted X. Unfixed code serves it and executes the stale B-compiled
//      block: vi2 = 0xBB instead of the live image's 0xAA.

#include "harness/VuTestHarness.h"
#include "harness/RecompilerTestEnvironment.h"

#include "VU.h"
#include "VUmicro.h"

#include <gtest/gtest.h>
#include <cstring>

// Test hooks exported by pcsx2/arm64/microVU-arm64.cpp under
// PCSX2_RECOMPILER_TESTS (cross-TU pattern; see mvu_cache_exhaustion_tests).
namespace mvu_test_hooks
{
	bool QuickSlotOccupied(int vu_index, u32 start_pc_bytes);
} // namespace mvu_test_hooks

namespace recompiler_tests {

using namespace vu;

namespace {

constexpr u32 kTail = 0x20; // byte offset of the extension/drift region
constexpr u32 kBigBudget = 4096;

inline VuOp LowerOnly(u32 lower) { return VuOp{lower, VNOP_U()}; }

void WritePairToMicro(u32 byte_addr, const VuOp& op)
{
	std::memcpy(vuRegs[1].Micro + byte_addr + 0, &op.lower, 4);
	std::memcpy(vuRegs[1].Micro + byte_addr + 4, &op.upper, 4);
}

// Production-shaped dispatch seeding with independent start_pc (selects the
// quick[] slot / program identity) and TPC (the entry pc within it). With
// tpc_pair != start_pc/8 this is exactly the state a cycle-budget break
// leaves behind: resume mid-program through the original program's slot.
void SeedVu1Dispatch(u32 start_pc_bytes, u32 tpc_pair)
{
	vuRegs[1].VI[REG_TPC].UL = tpc_pair;
	CpuMicroVU1.SetStartPC(start_pc_bytes);
	vuRegs[0].VI[REG_VPU_STAT].UL |= 0x100u;
	vuRegs[1].VI[REG_VPU_STAT].UL = vuRegs[0].VI[REG_VPU_STAT].UL;
}

bool Vu1Busy()
{
	return (vuRegs[0].VI[REG_VPU_STAT].UL & 0x100u) != 0;
}

void WriteTail(u32 vi2_imm)
{
	WritePairToMicro(kTail + 0x00,
		EBit(LowerOnly(VIADDIU_L(vi::vi2, vi::vi0, vi2_imm)))); // tail: vi2 = imm, E-bit
	WritePairToMicro(kTail + 0x08, LowerOnly(0));               // E-bit delay slot
}

} // namespace

TEST(MvuContentMapDrift, DriftedProgramMustNotBeServedByAnchorHash)
{
	VuTestHarness h(1);

	// Main program at pc0 — straight-line, E-bit. The tail region is never
	// reached from here; it is only entered via the synthetic resume below.
	h.LoadProgram({
		LowerOnly(VIADDIU_L(vi::vi1, vi::vi0, 0x77)), // pair 0: vi1 = 0x77
		EBit(LowerOnly(VIADDIU_L(vi::vi2, vi::vi0, 0x11))), // pair 1: vi2 = 0x11, E-bit
	});
	h.Run();
	ASSERT_EQ(h.GetViJit(vi::vi2), 0x11u);

	// Image A: main program + tail-A. Written before X is established so the
	// tail bytes are part of X's anchor image. (Run()'s interp pass reset the
	// block cache, so the program cache is empty here.)
	WriteTail(0xAA);
	CpuMicroVU1.Clear(kTail, 0x10);

	// Establish X over image A through the production dispatcher. Its
	// compiled ranges cover only the main program, not the tail.
	SeedVu1Dispatch(0, 0);
	CpuMicroVU1.Execute(kBigBudget);
	ASSERT_FALSE(Vu1Busy());
	ASSERT_EQ(vuRegs[1].VI[vi::vi2].UL & 0xFFFFu, 0x11u);
	ASSERT_TRUE(mvu_test_hooks::QuickSlotOccupied(1, 0));

	// Image B: rewrite the tail. Outside X's compiled ranges → X survives.
	WriteTail(0xBB);
	CpuMicroVU1.Clear(kTail, 0x10);
	ASSERT_TRUE(mvu_test_hooks::QuickSlotOccupied(1, 0))
		<< "choreography: X must survive the tail write (outside its ranges)";

	// Synthetic mid-program resume into the tail: quick-hit on X at
	// start_pc=0, block-miss at TPC=kTail → the tail is compiled INTO X from
	// the live (image-B) bytes. X's ranges grow over the tail and its data
	// is synced to B there — X has drifted from its anchor image A.
	SeedVu1Dispatch(0, kTail / 8);
	CpuMicroVU1.Execute(kBigBudget);
	ASSERT_FALSE(Vu1Busy());
	ASSERT_EQ(vuRegs[1].VI[vi::vi2].UL & 0xFFFFu, 0xBBu)
		<< "choreography: the resume must execute the freshly compiled "
		   "image-B tail";

	// Image A again (the double-buffer flip back). The write now overlaps
	// X's grown ranges → X's quick slot is nuked.
	WriteTail(0xAA);
	CpuMicroVU1.Clear(kTail, 0x10);
	ASSERT_FALSE(mvu_test_hooks::QuickSlotOccupied(1, 0))
		<< "choreography: the tail write must invalidate X now that its "
		   "ranges cover the tail (proves the extension happened)";

	// Full search at the same dispatch shape: the per-PC deque correctly
	// rejects X (its data no longer matches live bytes over the tail), and
	// the contentMap lookup for hash(image A) must NOT serve the drifted X.
	// Live image says the tail sets vi2=0xAA; the drifted X's compiled tail
	// block sets 0xBB.
	SeedVu1Dispatch(0, kTail / 8);
	CpuMicroVU1.Execute(kBigBudget);
	ASSERT_FALSE(Vu1Busy());
	EXPECT_EQ(vuRegs[1].VI[vi::vi2].UL & 0xFFFFu, 0xAAu)
		<< "drifted program served by its stale anchor hash: the contentMap "
		   "hit bypassed the range-compare and executed code compiled from "
		   "image B against live image A";
}

} // namespace recompiler_tests
