// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// lpState protocol invariant: after a VU microprogram runs to its E-bit end,
// the carried block-search key (mVU.prog.lpState) must be the all-zero state.
//
// The protocol has three writers:
//   - copyPLState (cycle-budget / M-bit mid-program exits) writes a live
//     nonzero *resume key* so the next dispatch resumes with correct state;
//   - mVU{0,1}clearlpStateJIT, called by emitted code at every E-bit program
//     end, zeroes the key — latched on `if (!prog.cleared)`;
//   - mVUclear zeroes it when micro memory is written.
//
// The E-bit latch is sound only under the invariant "cleared==1 implies every
// quick[] slot is null, so the next dispatch must search-resolve (resetting
// cleared=0) before any block runs". The range-aware mVUclear (e1f1417b3)
// broke that invariant by setting cleared=1 while leaving range-disjoint
// survivors quick-cached: survivors quick-hit without resetting cleared, the
// E-bit zero is skipped, and a stale mid-program resume key (blockType, flag
// instances, xgkickcycles, VI stall info) leaks into the entry-block search
// key of the NEXT freshly dispatched program — which then compiles a
// wrong-shaped entry block (e.g. blockType!=0 truncates the compile to a
// single instruction via `endCount = pState->blockType ? 1 : ...`). Root
// cause of the Crash Twinsanity VIF1/VU1 display-list wedge (2026-07-09);
// the leaked key is also serialized into savestates (vuJITFreeze freezes
// lpState), making them dead-on-load under the JIT.
//
// The test reproduces the leak through the production dispatch path:
//   1. run program P at pc0 to E-bit (quick[0] cached, lpState zero);
//   2. upload+run a second program Q at 0x3000 (its own quick slot, ranges
//      disjoint from P's);
//   3. rewrite one pair inside Q's compiled range + Clear → range-aware
//      partial invalidation: Q's slot nulled, P's slot survives;
//   4. re-dispatch P with a starved cycle budget → quick-hit on the survivor
//      → mid-program budget break saves a nonzero resume key into lpState;
//   5. resume P to its E-bit end;
//   6. assert lpState is all-zero again. Unfixed code skips the zero (the
//      cleared latch went stale in step 3) and leaks the resume key.

#include "harness/VuTestHarness.h"
#include "harness/RecompilerTestEnvironment.h"

#include "VU.h"
#include "VUmicro.h"

#include <gtest/gtest.h>
#include <cstring>

// Test hook exported by pcsx2/arm64/microVU-arm64.cpp under
// PCSX2_RECOMPILER_TESTS. Declared here instead of including
// microVU-arm64.h, whose __fi definitions can't safely cross TUs
// (same pattern as mvu_cache_exhaustion_tests.cpp).
namespace mvu_test_hooks
{
	bool LpStateIsZero(int vu_index);
	u32 GetProgCleared(int vu_index);
	bool QuickSlotOccupied(int vu_index, u32 start_pc_bytes);
} // namespace mvu_test_hooks

namespace recompiler_tests {

using namespace vu;

namespace {

constexpr u32 kQBase = 0x3000; // byte offset of program Q in VU1.Micro
constexpr u32 kBigBudget = 4096;

inline VuOp LowerOnly(u32 lower) { return VuOp{lower, VNOP_U()}; }

void WritePairToMicro(u32 byte_addr, const VuOp& op)
{
	std::memcpy(vuRegs[1].Micro + byte_addr + 0, &op.lower, 4);
	std::memcpy(vuRegs[1].Micro + byte_addr + 4, &op.upper, 4);
}

// Minimal production-shaped dispatch seeding (mirrors VuTestHarness::
// SeedEntryState without the block-cache reset): TPC holds a pair index
// (recMicroVU1::Execute shifts it to a byte address), start_pc a byte
// address (it selects the quick[] slot), and the VU1 running bit lives in
// VU0's VPU_STAT.
void SeedVu1Dispatch(u32 start_pc_bytes)
{
	vuRegs[1].VI[REG_TPC].UL = start_pc_bytes / 8u;
	CpuMicroVU1.SetStartPC(start_pc_bytes);
	vuRegs[0].VI[REG_VPU_STAT].UL |= 0x100u;
	vuRegs[1].VI[REG_VPU_STAT].UL = vuRegs[0].VI[REG_VPU_STAT].UL;
}

bool Vu1Busy()
{
	return (vuRegs[0].VI[REG_VPU_STAT].UL & 0x100u) != 0;
}

} // namespace

TEST(MvuLpStateInvariant, EbitEndZeroesLpStateAfterPartialRangeClear)
{
	VuTestHarness h(1);

	// Program P: multi-block loop so a starved dispatch breaks mid-program.
	// The VADD in the loop body rotates flag instances so the loop block's
	// entry key — what the budget break saves into lpState — is nonzero.
	// Two NOP pads sit between the decrement and the branch so the VI hazard
	// backup window (VIBackupCycles=2, VUops.cpp:430) elapses and the branch
	// sees the post-decrement value (same shape as vu0_branch_delay_tests).
	h.SetVfBits(1, 0x3F800000u, 0x3F800000u, 0x3F800000u, 0x3F800000u);
	h.SetVi(vi::vi5, 0);
	h.LoadProgram({
		LowerOnly(VIADDIU_L(vi::vi1, vi::vi0, 4)),           // pair 0: vi1 = 4
		VuOp{VIADDIU_L(vi::vi5, vi::vi5, 1),
			VADD_U(mask::xyzw, vf::vf1, vf::vf1, vf::vf0)},  // pair 1: L1: vi5 += 1
		LowerOnly(VIADDI_L(vi::vi1, vi::vi1, -1)),           // pair 2: vi1 -= 1
		LowerOnly(0),                                        // pair 3: NOP (hazard pad)
		LowerOnly(0),                                        // pair 4: NOP (hazard pad)
		LowerOnly(VIBNE_L(vi::vi1, vi::vi0, -5)),            // pair 5: if vi1 != 0 → L1
		LowerOnly(VIADDIU_L(vi::vi4, vi::vi0, 0xAA)),        // pair 6: branch delay
		EBit(LowerOnly(VIADDIU_L(vi::vi2, vi::vi0, 0x55))),  // pair 7: E-bit end
	});
	h.Run();
	ASSERT_EQ(h.GetViJit(vi::vi1), 0u);
	ASSERT_EQ(h.GetViJit(vi::vi5), 4u);
	ASSERT_EQ(h.GetViJit(vi::vi2), 0x55u);

	// Run()'s interp pass re-seeds with a block-cache reset AFTER the JIT
	// pass, so the mVU program cache is empty here. Re-establish P through
	// the production dispatcher: search-resolve creates the program (and
	// resets the cleared latch), the E-bit end zeroes lpState.
	SeedVu1Dispatch(0);
	CpuMicroVU1.Execute(kBigBudget);
	ASSERT_FALSE(Vu1Busy()) << "P must run to its E-bit end";
	ASSERT_TRUE(mvu_test_hooks::QuickSlotOccupied(1, 0))
		<< "choreography: P must be quick-cached at pc0";
	ASSERT_EQ(mvu_test_hooks::GetProgCleared(1), 0u)
		<< "choreography: the search-resolve must have reset the cleared latch";
	ASSERT_TRUE(mvu_test_hooks::LpStateIsZero(1))
		<< "baseline: lpState must be zero after a completed E-bit program";

	// Program Q at kQBase, written directly (LoadProgram only writes pc0).
	// Its bytes differ from the image P was created against, so the
	// dispatcher creates a second microProgram whose compiled ranges are
	// disjoint from P's.
	WritePairToMicro(kQBase + 0x00, LowerOnly(VIADDIU_L(vi::vi3, vi::vi0, 7)));
	WritePairToMicro(kQBase + 0x08, EBit(LowerOnly(VIADDIU_L(vi::vi3, vi::vi3, 1))));
	WritePairToMicro(kQBase + 0x10, LowerOnly(VIADDIU_L(vi::vi4, vi::vi0, 0)));
	CpuMicroVU1.Clear(kQBase, 0x18);

	SeedVu1Dispatch(kQBase);
	CpuMicroVU1.Execute(kBigBudget);
	ASSERT_FALSE(Vu1Busy()) << "Q must run to its E-bit end";
	ASSERT_EQ(vuRegs[1].VI[vi::vi3].UL & 0xFFFFu, 8u);
	ASSERT_TRUE(mvu_test_hooks::QuickSlotOccupied(1, 0))
		<< "choreography: P must still be quick-cached at pc0";
	ASSERT_TRUE(mvu_test_hooks::QuickSlotOccupied(1, kQBase))
		<< "choreography: Q must be quick-cached at kQBase";

	// Partial invalidation: rewrite one pair INSIDE Q's compiled range.
	// Range-aware mVUclear nulls Q's quick slot; P's slot (disjoint ranges)
	// survives.
	WritePairToMicro(kQBase + 0x08, EBit(LowerOnly(VIADDIU_L(vi::vi3, vi::vi3, 2))));
	CpuMicroVU1.Clear(kQBase + 0x08, 8);
	ASSERT_FALSE(mvu_test_hooks::QuickSlotOccupied(1, kQBase))
		<< "choreography: the write must invalidate Q's quick slot";
	ASSERT_TRUE(mvu_test_hooks::QuickSlotOccupied(1, 0))
		<< "choreography: P's quick slot must SURVIVE the disjoint write "
		   "(if this fires, P's ranges unexpectedly cover the write)";

	// Starved re-dispatch of P through the surviving quick slot: the
	// cycle-budget test fails at the loop block's entry and the emitted
	// early-exit (copyPLState + mVUendProgram(0)) saves that block's
	// nonzero pipeline state into lpState as the resume key.
	SeedVu1Dispatch(0);
	CpuMicroVU1.Execute(1);
	ASSERT_TRUE(Vu1Busy())
		<< "precondition: the starved dispatch must break mid-program";
	ASSERT_NE(vuRegs[1].VI[REG_TPC].UL, 0u)
		<< "precondition: TPC must point at the interior resume pc";
	ASSERT_FALSE(mvu_test_hooks::LpStateIsZero(1))
		<< "precondition: the budget break must save a nonzero resume key "
		   "(if this fires, enrich program P so the loop block's entry key "
		   "is nonzero)";

	// Resume P to its E-bit end. The emitted end-of-program helper
	// (mVU1clearlpStateJIT) must zero lpState here; on unfixed code the
	// stale cleared==1 latch — set by the partial invalidation above and
	// never reset because P quick-hit — skips it, and the resume key leaks
	// into the next fresh dispatch's entry-block search key.
	CpuMicroVU1.Execute(kBigBudget);
	ASSERT_FALSE(Vu1Busy()) << "P must run to its E-bit end";
	EXPECT_EQ(vuRegs[1].VI[vi::vi1].UL & 0xFFFFu, 0u);
	EXPECT_EQ(vuRegs[1].VI[vi::vi2].UL & 0xFFFFu, 0x55u);
	EXPECT_TRUE(mvu_test_hooks::LpStateIsZero(1))
		<< "lpState must return to the all-zero state after a completed "
		   "E-bit program — a leaked resume key here poisons the next "
		   "program's entry-block search key (Crash Twinsanity wedge)";
}

} // namespace recompiler_tests
