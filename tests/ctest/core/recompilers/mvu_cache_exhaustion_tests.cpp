// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// mVU code-cache exhaustion must trigger the mVUcleanUp "Program cache limit
// reached" reset, NOT a process abort.
//
// Regression test for the arm64-only crash diagnosed from Rocknix SM8650
// core dumps (OutRun 2006, 2026-07-02): mVUreset bound the persistent vixl
// MacroAssembler's capacity at prog.x86end — the reset THRESHOLD, which
// already has mVUcacheSafeZone subtracted — instead of the physical end of
// the rec region. vixl cannot grow an externally-owned buffer, so the first
// compile session that needed to overshoot x86end died in CodeBuffer::Grow
// (VIXL_ASSERT / realloc-on-mmap abort) before mVUcleanUp's bounds check
// could ever fire. On x86 the raw emitter overshoots into the safe zone
// harmlessly and the check catches it afterward; the arm64 capacity must
// therefore extend past the threshold to the region end.
//
// The test shrinks VU1's reset threshold to a few dozen KB (test hook), then
// streams hash-distinct microprograms through the production dispatch path
// until the cache fills several times over. Unfixed code SIGABRTs mid-loop;
// fixed code resets and keeps compiling correct programs. Cursor rewinds
// observed via vu_capture_internal::GetCompiledRange prove the reset path
// actually ran rather than the cache never filling.

#include "harness/VuTestHarness.h"
#include "harness/RecompilerTestEnvironment.h"

#include "VU.h"
#include "VUmicro.h"

#include <gtest/gtest.h>
#include <vector>

// Test hooks exported by pcsx2/arm64/microVU-arm64.cpp under
// PCSX2_RECOMPILER_TESTS. Declared here instead of including
// microVU-arm64.h, whose __fi definitions can't safely cross TUs
// (same pattern as vu_capture_internal in harness/VuReplay.cpp).
namespace mvu_test_hooks
{
	void ShrinkCacheForTest(int vu_index, size_t bytes_after_start);
	void RestoreCacheGeometry(int vu_index);
} // namespace mvu_test_hooks

namespace vu_capture_internal
{
	void GetCompiledRange(int vu_index, const u8** out_start, const u8** out_end);
} // namespace vu_capture_internal

namespace recompiler_tests {

using namespace vu;

namespace {

// The threshold the test shrinks VU1's cache to. Big enough for dozens of
// small programs (so the pre-fill iterations behave normally), small enough
// that the loop below fills it several times.
constexpr size_t kShrunkCacheBytes = 32 * 1024;

// Chain length: 1 seed IADDIU + (kChainPairs - 2) increments + 1 E-bit
// increment. Longer programs emit more host code per iteration, keeping the
// iteration count low.
constexpr int kChainPairs = 48;

std::vector<VuOp> MakeChainProgram(u32 seed_imm)
{
	std::vector<VuOp> pairs;
	pairs.reserve(kChainPairs);
	// vi1 = seed_imm — the varying immediate makes every program's microcode
	// (and therefore its content hash) unique, forcing a fresh compile.
	pairs.push_back(VuOp{VIADDIU_L(vi::vi1, vi::vi0, seed_imm), VNOP_U()});
	for (int i = 0; i < kChainPairs - 2; i++)
		pairs.push_back(VuOp{VIADDIU_L(vi::vi1, vi::vi1, 1), VNOP_U()});
	pairs.push_back(EBit(VuOp{VIADDIU_L(vi::vi1, vi::vi1, 1), VNOP_U()}));
	return pairs;
}

constexpr u32 ExpectedVi1(u32 seed_imm)
{
	return seed_imm + (kChainPairs - 1);
}

u8* CurrentWriteCursor(int vu_index)
{
	const u8* start = nullptr;
	const u8* ptr = nullptr;
	vu_capture_internal::GetCompiledRange(vu_index, &start, &ptr);
	return const_cast<u8*>(ptr);
}

// Restores production cache geometry even when an ASSERT bails out of the
// test body early. The trailing block-cache reset rebinds the vixl buffer
// capacity from the restored x86end so later fixtures see stock behavior.
struct RestoreVu1GeometryOnExit
{
	~RestoreVu1GeometryOnExit()
	{
		mvu_test_hooks::RestoreCacheGeometry(1);
		RecompilerTestEnvironment::ResetVuBlockCache(1);
	}
};

} // namespace

TEST(MvuCacheExhaustion, Vu1FillPastThresholdResetsInsteadOfAborting)
{
	VuTestHarness h(1);

	// Shrink the reset threshold, then reset so mVUreset rebinds the vixl
	// buffer capacity from the patched geometry (the value under test).
	mvu_test_hooks::ShrinkCacheForTest(1, kShrunkCacheBytes);
	RecompilerTestEnvironment::ResetVuBlockCache(1);
	RestoreVu1GeometryOnExit restore_guard;

	// Iteration 0 through Run(): establishes the harness pre-state snapshot
	// and diffs JIT vs interp once so the program shape itself is validated.
	h.LoadProgram(MakeChainProgram(1000));
	h.Run();
	ASSERT_EQ(h.GetViJit(vi::vi1), ExpectedVi1(1000));

	// Stream hash-distinct programs through the production dispatch path
	// without the harness's per-run block-cache reset. Each iteration mirrors
	// a game uploading new microcode: write VU.Micro, invalidate via the
	// production recMicroVU1::Clear, execute through the real dispatcher.
	// Every execution exits through the dispatcher stub whose bounds check
	// triggers mVUcleanUp's "Program cache limit reached" reset once the
	// write cursor crosses the (shrunken) x86end.
	u8* cursor = CurrentWriteCursor(1);
	int resets_observed = 0;
	int iterations = 0;
	for (int i = 1; i <= 500 && resets_observed < 2; i++)
	{
		iterations = i;
		const u32 seed = 1000u + static_cast<u32>(i);
		h.LoadProgram(MakeChainProgram(seed));
		CpuMicroVU1.Clear(0, 0x4000);
		h.RunJitPreserveBlockCache();
		ASSERT_EQ(h.GetViJit(vi::vi1), ExpectedVi1(seed))
			<< "wrong result at iteration " << i
			<< " (first bad program after " << resets_observed << " cache resets)";

		u8* now = CurrentWriteCursor(1);
		if (now < cursor)
			resets_observed++;
		cursor = now;
	}

	// The loop wrote kChainPairs-sized programs until the cursor rewound
	// twice. If it never rewound, the cache never filled (threshold not
	// honored) — and on unfixed code this point is unreachable: the process
	// aborts inside vixl CodeBuffer::Grow when the first fill hits capacity.
	EXPECT_GE(resets_observed, 2)
		<< "cache never reset within " << iterations << " iterations";
}

} // namespace recompiler_tests
