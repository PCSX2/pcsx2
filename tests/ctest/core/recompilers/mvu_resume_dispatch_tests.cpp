// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Resume-aware dispatch invariants (VE-07).
//
// ~98% of VU0 dispatches under the run-ahead sync model are *resumes*: a
// cycle-budget break at a block's mVUtestCycles saves that block's own
// pState/TPC (copyPLStateResume) and the next Execute re-enters the very
// block that broke. The dispatch fast path exploits that structural
// invariant: the break parks the block's hostEntry in mVU.resumeEntry,
// Execute consumes it once and enters startFunctResume, skipping
// mVUlookupProg entirely.
//
// The soundness argument this file pins:
//   1. ORACLE — the parked pointer must equal what the full production
//      lookup would resolve for (TPC, lpState) at that moment. If these
//      ever diverge, the fast path runs the wrong block variant.
//   2. Consume-once — only a budget break arms; E-bit completion leaves
//      the slot empty (VPU_STAT gating makes a stale slot at completion
//      harmless-looking right up until the next kick — so it must be
//      empty, not merely unused).
//   3. Disarm on ANY micro-mem write (mVUclear) — same unconditional
//      contract as the carried lpState zero: a write can change what a
//      fresh search-resolve would find, including for range-disjoint
//      survivors (x86's carried-key contract; the Twinsanity lesson).
//   4. Disarm on a fresh kick (SetStartPC) — a kick selects the quick
//      slot by start_pc and enters at the kicked PC; consuming a resume
//      parked by an abandoned run would jump mid-program.
//   5. Equivalence — a run sliced across many starved dispatches (each
//      after the first riding the resume path) ends bit-identical to one
//      unsliced run.

#include "harness/VuTestHarness.h"
#include "harness/RecompilerTestEnvironment.h"

#include "VU.h"
#include "VUmicro.h"

#include <gtest/gtest.h>

// Test hooks exported by pcsx2/arm64/microVU-arm64.cpp under
// PCSX2_RECOMPILER_TESTS (same cross-TU pattern as
// mvu_lpstate_invariant_tests.cpp).
namespace mvu_test_hooks
{
	void* GetResumeEntry(int vu_index);
	void* ResolveLookupEntry(int vu_index);
} // namespace mvu_test_hooks

// The resume fast path is gated off while ProgCache recording is on (the
// full path keeps observed.record seeing resume TPCs). Force it off here
// so these tests exercise the fast path regardless of suite ordering
// (mvu_abi_digest_tests / mvu_progcache_disk_tests toggle recording).
namespace mVUPersist
{
	void SetRecordingEnabled(bool enabled);
	bool IsRecordingEnabled();
} // namespace mVUPersist

namespace recompiler_tests {

using namespace vu;

namespace {

constexpr u32 kBigBudget = 4096;
constexpr u32 kDisjointBase = 0x3000; // byte offset well past the program

inline VuOp LowerOnly(u32 lower) { return VuOp{lower, VNOP_U()}; }

void WritePairToMicro(u32 byte_addr, const VuOp& op)
{
	std::memcpy(vuRegs[1].Micro + byte_addr + 0, &op.lower, 4);
	std::memcpy(vuRegs[1].Micro + byte_addr + 4, &op.upper, 4);
}

// Production-shaped dispatch seeding — see mvu_lpstate_invariant_tests.cpp.
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

// The lpstate-test loop program: multi-block, so a starved dispatch breaks
// at the loop block's mVUtestCycles. vi5 counts iterations (ends at 4),
// vi1 counts down to 0, vi2 gets 0x55 at the E-bit end.
void LoadLoopProgram(VuTestHarness& h)
{
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
}

class RecordingOffScope
{
	bool m_prev;

public:
	RecordingOffScope()
		: m_prev(mVUPersist::IsRecordingEnabled())
	{
		mVUPersist::SetRecordingEnabled(false);
	}
	~RecordingOffScope() { mVUPersist::SetRecordingEnabled(m_prev); }
};

} // namespace

// (1) + (2): a budget break arms the resume slot with EXACTLY the pointer
// the full production lookup resolves; consuming it runs the program to a
// correct E-bit end that leaves the slot empty.
TEST(MvuResumeDispatch, BudgetBreakArmsLookupResolutionAndEbitEndDisarms)
{
	RecordingOffScope recording_off;
	VuTestHarness h(1);
	LoadLoopProgram(h);
	h.Run(); // JIT-vs-interp equivalence of the program itself

	// Fresh full run through the production dispatcher: completion must
	// never leave a parked resume. (vi5 only ever increments — zero it per
	// run so the completion asserts are absolute.)
	vuRegs[1].VI[vi::vi5].UL = 0;
	SeedVu1Dispatch(0);
	CpuMicroVU1.Execute(kBigBudget);
	ASSERT_FALSE(Vu1Busy()) << "choreography: full budget must reach the E-bit";
	EXPECT_EQ(mvu_test_hooks::GetResumeEntry(1), nullptr)
		<< "an E-bit completion must not arm the resume slot";

	// Starved re-dispatch: the loop block's cycle test breaks mid-program
	// and parks that block's hostEntry.
	vuRegs[1].VI[vi::vi5].UL = 0;
	SeedVu1Dispatch(0);
	CpuMicroVU1.Execute(1);
	ASSERT_TRUE(Vu1Busy()) << "precondition: the starved dispatch must break mid-program";
	ASSERT_NE(vuRegs[1].VI[REG_TPC].UL, 0u)
		<< "precondition: TPC must point at the interior resume pc";

	void* const parked = mvu_test_hooks::GetResumeEntry(1);
	ASSERT_NE(parked, nullptr) << "a budget break must arm the resume slot";

	// THE ORACLE: the parked pointer is what mVUlookupProg would resolve
	// for the current (TPC, lpState). This is the entire soundness claim
	// of the fast path.
	EXPECT_EQ(parked, mvu_test_hooks::ResolveLookupEntry(1))
		<< "parked resume must equal the full lookup's resolution";

	// Consume it through the production Execute; program completes with
	// the same results as an unsliced run and the slot is empty again.
	CpuMicroVU1.Execute(kBigBudget);
	ASSERT_FALSE(Vu1Busy()) << "resume must run to the E-bit end";
	EXPECT_EQ(vuRegs[1].VI[vi::vi1].UL & 0xFFFFu, 0u);
	EXPECT_EQ(vuRegs[1].VI[vi::vi5].UL & 0xFFFFu, 4u);
	EXPECT_EQ(vuRegs[1].VI[vi::vi2].UL & 0xFFFFu, 0x55u);
	EXPECT_EQ(mvu_test_hooks::GetResumeEntry(1), nullptr)
		<< "the resume is consume-once; completion must leave the slot empty";
}

// (5): slicing a run across many starved dispatches — every dispatch after
// the first entering through the resume fast path — ends in the same
// architectural state as one unsliced run.
TEST(MvuResumeDispatch, SlicedRunMatchesUnslicedRun)
{
	RecordingOffScope recording_off;
	VuTestHarness h(1);
	LoadLoopProgram(h);
	h.Run();

	// Unsliced reference. (vi5 only ever increments — zero the observed
	// VIs so both runs start from identical state.)
	vuRegs[1].VI[vi::vi5].UL = 0;
	vuRegs[1].VI[vi::vi1].UL = 0;
	vuRegs[1].VI[vi::vi2].UL = 0;
	vuRegs[1].VI[vi::vi4].UL = 0;
	SeedVu1Dispatch(0);
	CpuMicroVU1.Execute(kBigBudget);
	ASSERT_FALSE(Vu1Busy());
	const u32 ref_vi1 = vuRegs[1].VI[vi::vi1].UL;
	const u32 ref_vi2 = vuRegs[1].VI[vi::vi2].UL;
	const u32 ref_vi4 = vuRegs[1].VI[vi::vi4].UL;
	const u32 ref_vi5 = vuRegs[1].VI[vi::vi5].UL;

	// Sliced: starve every dispatch. Reset the iteration counter the
	// program accumulates (vi5) so the two runs start identically.
	vuRegs[1].VI[vi::vi5].UL = 0;
	vuRegs[1].VI[vi::vi1].UL = 0;
	vuRegs[1].VI[vi::vi2].UL = 0;
	vuRegs[1].VI[vi::vi4].UL = 0;
	SeedVu1Dispatch(0);
	int resumed_dispatches = 0;
	for (int i = 0; i < 256 && Vu1Busy(); i++)
	{
		if (mvu_test_hooks::GetResumeEntry(1))
			resumed_dispatches++;
		CpuMicroVU1.Execute(1);
	}
	ASSERT_FALSE(Vu1Busy()) << "sliced run failed to complete within bound";
	EXPECT_GT(resumed_dispatches, 0)
		<< "choreography: the sliced run must actually ride the resume path";

	EXPECT_EQ(vuRegs[1].VI[vi::vi1].UL, ref_vi1);
	EXPECT_EQ(vuRegs[1].VI[vi::vi2].UL, ref_vi2);
	EXPECT_EQ(vuRegs[1].VI[vi::vi4].UL, ref_vi4);
	EXPECT_EQ(vuRegs[1].VI[vi::vi5].UL, ref_vi5);
}

// (3): ANY micro-mem write disarms — even one disjoint from the running
// program's compiled ranges (whose quick slot survives range-aware
// mVUclear). The subsequent dispatch takes the full path and still
// completes correctly.
TEST(MvuResumeDispatch, DisjointClearDisarmsParkedResume)
{
	RecordingOffScope recording_off;
	VuTestHarness h(1);
	LoadLoopProgram(h);
	h.Run();

	SeedVu1Dispatch(0);
	CpuMicroVU1.Execute(1);
	ASSERT_TRUE(Vu1Busy());
	ASSERT_NE(mvu_test_hooks::GetResumeEntry(1), nullptr);

	// Write far past the program, then Clear that range. Range-aware
	// invalidation keeps the program quick-cached; the resume must drop
	// anyway (x86's unconditional carried-key contract).
	WritePairToMicro(kDisjointBase, LowerOnly(VIADDIU_L(vi::vi3, vi::vi0, 7)));
	CpuMicroVU1.Clear(kDisjointBase, 8);
	EXPECT_EQ(mvu_test_hooks::GetResumeEntry(1), nullptr)
		<< "any micro-mem write must disarm the parked resume";

	// Note: this Clear also zeroes the carried lpState (production
	// contract), so the full-path resume below re-resolves the interior
	// block with a zeroed search key. Completion must still be correct.
	CpuMicroVU1.Execute(kBigBudget);
	ASSERT_FALSE(Vu1Busy());
	EXPECT_EQ(vuRegs[1].VI[vi::vi1].UL & 0xFFFFu, 0u);
	EXPECT_EQ(vuRegs[1].VI[vi::vi2].UL & 0xFFFFu, 0x55u);
}

// (4): a fresh kick (SetStartPC) disarms. Production shape: a run is
// force-abandoned (vu0Finish-style VPU_STAT clear without a dispatcher
// exit) leaving a parked resume, then the game kicks the program again.
// Consuming the stale resume would re-enter mid-loop instead of at the
// kicked PC.
TEST(MvuResumeDispatch, FreshKickDisarmsParkedResume)
{
	RecordingOffScope recording_off;
	VuTestHarness h(1);
	LoadLoopProgram(h);
	h.Run();

	SeedVu1Dispatch(0);
	CpuMicroVU1.Execute(1);
	ASSERT_TRUE(Vu1Busy());
	ASSERT_NE(mvu_test_hooks::GetResumeEntry(1), nullptr);

	// Force-abandon the run (the forced-finish shape: C side clears the
	// running bit; no dispatcher exit runs, so nothing else disarms).
	vuRegs[0].VI[REG_VPU_STAT].UL &= ~0x100u;
	vuRegs[1].VI[REG_VPU_STAT].UL = vuRegs[0].VI[REG_VPU_STAT].UL;

	// Fresh kick at pc0.
	SeedVu1Dispatch(0);
	EXPECT_EQ(mvu_test_hooks::GetResumeEntry(1), nullptr)
		<< "SetStartPC (fresh kick) must disarm the parked resume";

	// The kicked run must start at pc0: vi1 is re-seeded to 4 by pair 0
	// and counts down to 0; vi5 accumulates the abandoned run's progress
	// plus a full 4 fresh iterations.
	const u32 vi5_before = vuRegs[1].VI[vi::vi5].UL & 0xFFFFu;
	CpuMicroVU1.Execute(kBigBudget);
	ASSERT_FALSE(Vu1Busy());
	EXPECT_EQ(vuRegs[1].VI[vi::vi1].UL & 0xFFFFu, 0u);
	EXPECT_EQ(vuRegs[1].VI[vi::vi2].UL & 0xFFFFu, 0x55u);
	EXPECT_EQ(vuRegs[1].VI[vi::vi5].UL & 0xFFFFu, vi5_before + 4u)
		<< "fresh kick must re-run from pc0 (vi1 = 4), not resume mid-loop";
}

} // namespace recompiler_tests
