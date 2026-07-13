// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VE-02 inline last-hit probe (mVU.prog.lastHit) tests.
//
// The dispatcher stub caches the last (start_pc, lpState.quick64) →
// hostEntry resolution and dispatches through it inline, skipping the
// mVUlookupProg BL entirely (microVU-arm64.cpp mVUdispatcherAB). These
// tests pin:
//   1. Result correctness across the three dispatch tiers a repeated
//      identical kick traverses: full compile (slow path) → C quick-path
//      lookup (which seeds the probe) → inline probe hit.
//   2. The invalidation contract: a micro-mem write reaching the JIT
//      through the production path (vuMicroWrite → BaseVUmicroCPU::Clear
//      → mVUclear) must drop the probe cache along with the quick slots,
//      so the next dispatch re-resolves against the fresh code. A stale
//      inline hit would silently execute the OLD program's block — the
//      wrong-hit failure class this cache risks.
//
// Uses VuTestHarness (NOT EeRecTestHarness): the EE harness pins
// CpuVU0 = interpreter, so EE-side VCALLMS tests never enter the mVU
// dispatcher stub. RunJitPreserveBlockCache re-dispatches the JIT
// without the mVUreset the normal Run() performs, which is what lets
// the probe survive across dispatches here.

#include "harness/VuTestHarness.h"

#include "VUmicro.h"

#include <cstring>

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

TEST(MvuLastHitProbe, RepeatedVu0DispatchesStayCorrect)
{
	VuTestHarness h(0);
	h.LoadProgram({
		VuOp{VIADDIU_L(/*it*/3, /*is*/0, 5), VNOP_U()},
		EBitNopPair(),
	});
	h.Run(); // dispatch 1: fresh block cache, full compile
	EXPECT_EQ(h.GetViJit(3), 5u);
	EXPECT_EQ(h.GetViJit(3), h.GetViInterp(3));

	h.RunJitPreserveBlockCache(); // dispatch 2: C quick path — seeds the probe
	EXPECT_EQ(h.GetViJit(3), 5u);

	h.RunJitPreserveBlockCache(); // dispatch 3: inline probe hit
	EXPECT_EQ(h.GetViJit(3), 5u);
}

TEST(MvuLastHitProbe, MicroMemWritePlusClearInvalidatesProbe)
{
	VuTestHarness h(0);
	h.LoadProgram({
		VuOp{VIADDIU_L(/*it*/3, /*is*/0, 5), VNOP_U()},
		EBitNopPair(),
	});
	h.Run();
	h.RunJitPreserveBlockCache(); // seed
	h.RunJitPreserveBlockCache(); // inline hit — probe is hot now
	ASSERT_EQ(h.GetViJit(3), 5u);

	// Rewrite pair-0's lower op in place (vi3 = 5 → vi3 = 9) the way the
	// live write path does: raw bytes, then BaseVUmicroCPU::Clear (which
	// funnels into mVUclear — the seam that must drop mVU.prog.lastHit).
	const u32 new_lower = VIADDIU_L(/*it*/3, /*is*/0, 9);
	std::memcpy(vuRegs[0].Micro, &new_lower, 4);
	CpuMicroVU0.Clear(0, 8);

	h.RunJitPreserveBlockCache(); // must re-resolve and run the NEW program
	EXPECT_EQ(h.GetViJit(3), 9u) << "stale inline last-hit: the probe served "
	                                "the pre-rewrite block after mVUclear";
}

} // namespace recompiler_tests
