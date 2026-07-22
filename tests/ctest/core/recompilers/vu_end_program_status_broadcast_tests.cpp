// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// End-of-program STATUS-flag broadcast invariant.
//
// When a microVU program terminates on the E bit, the pipeline is drained and
// mVU broadcasts the final committed STATUS flag to ALL FOUR shadow instances
// (VURegs.micro_statusflags[0..3]) — matching x86 microVU and ARMSX2. Those
// four instances are what the NEXT program reloads as its initial delayed-flag
// pipeline (see the entry restore in microVU-arm64.cpp), so leaving them as the
// raw pre-broadcast 4-deep pipeline tail makes the next program's early
// delayed-STATUS reads (FSAND/FSOR/FSEQ) see stale instances.
//
// The shared dispatcher `exitFunct` unconditionally re-saves the raw status
// registers gprF0..3 -> micro_statusflags on every block exit. On the E-bit
// path that runs AFTER the end-program helper's broadcast, so without a
// compensating write the re-save clobbered the broadcast (regression fixed in
// mVUGenerateEndProgramFlagsHelper's Helper B, which now mirrors the broadcast
// into gprF0..3). The bug was invisible to the JIT-vs-interp diff harness: the
// interpreter does not model micro_statusflags at all (only VU0's COP2 macro
// sync writes it), so PipelinePermissive deliberately ignores those fields.
// This suite therefore asserts the broadcast invariant DIRECTLY on the JIT's
// post-run snapshot rather than against the interpreter.
//
// The trigger is an E-bit program whose last few STATUS-writing FMACs commit
// DIFFERENT flags with no padding, so the raw 4-deep tail is non-uniform. Empty
// / uniform-status programs (e.g. Crash Twinsanity's pc0 loader) hide the bug —
// which is exactly why the cross-JIT .vucap oracle over that title stayed green.
// Real geometry programs in Ratchet & Clank UYA, Okami, DMC3, Dragon Quest VIII
// and others hit it every frame.

#include "harness/VuTestHarness.h"

#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

inline VuOp UpperOnly(u32 upper) { return VuOp{0, upper}; }

// Asserts the four STATUS shadow instances left by the JIT are identical — the
// end-of-program broadcast invariant. Returns the (common) value for callers
// that want to sanity-check it is the freshest committed status.
u32 ExpectStatusBroadcast(const VuTestHarness& h)
{
	const VURegs& g = h.JitSnapshot().regs;
	EXPECT_EQ(g.micro_statusflags[0], g.micro_statusflags[1])
		<< "micro_statusflags not broadcast after E-bit (instances 0 vs 1)";
	EXPECT_EQ(g.micro_statusflags[0], g.micro_statusflags[2])
		<< "micro_statusflags not broadcast after E-bit (instances 0 vs 2)";
	EXPECT_EQ(g.micro_statusflags[0], g.micro_statusflags[3])
		<< "micro_statusflags not broadcast after E-bit (instances 0 vs 3)";
	return g.micro_statusflags[0];
}

// The distinct-STATUS tail (no padding) is what makes the raw 4-deep status
// pipeline non-uniform at E-bit. But a tail with no padding also lands the
// microVU block-boundary flag commit one pipeline stage away from the
// interpreter's, so the FINAL committed REG_STATUS_FLAG / REG_MAC_FLAG legally
// disagree JIT-vs-interp (the flag-pipeline suite adds 4 NOPs of padding
// precisely to avoid this — but padding would also drain our status tail to
// uniform and hide the bug under test). Those VIs are orthogonal to the
// broadcast invariant we assert directly on the JIT snapshot, so exclude them
// from the harness's JIT-vs-interp diff.
void IgnoreCommittedFlagVis(VuTestHarness& h)
{
	h.IgnoreViInDiff(REG_STATUS_FLAG);
	h.IgnoreViInDiff(REG_MAC_FLAG);
	h.IgnoreViInDiff(REG_CLIP_FLAG);
}

// A tail of FMACs that commit distinct STATUS flags back-to-back (no pipeline
// padding), so the raw 4-deep status tail is non-uniform at E-bit:
//   VADD vf1, vf0, vf0  -> (0,0,0,1): x/y/z zero  -> Z current bits
//   VADD vf2, vf5, vf5  -> all positive nonzero   -> no current Z, sticky-Z only
//   VSUB vf3, vf0, vf5  -> all negative           -> S current bits
// Correct behaviour collapses all four instances to the freshest (the VSUB's).
void LoadDistinctStatusTail(VuTestHarness& h)
{
	IgnoreCommittedFlagVis(h);
	h.SetVf(vf::vf5, 5.0f, 6.0f, 7.0f, 8.0f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf0, vf::vf0)),
		UpperOnly(VADD_U(mask::xyzw, vf::vf2, vf::vf5, vf::vf5)),
		UpperOnly(VSUB_U(mask::xyzw, vf::vf3, vf::vf0, vf::vf5)),
		EBitNopPair(),
	});
}

} // namespace

// VU1 is the engine the bug was observed on (geometry programs E-biting with a
// live, non-uniform status pipeline).
TEST(VuEndProgramStatusBroadcast, Vu1EbitBroadcastsStatusToAllInstances)
{
	VuTestHarness h(1);
	LoadDistinctStatusTail(h);
	h.Run();
	ExpectStatusBroadcast(h);
}

// The dispatcher / end-program helper is shared, so VU0 must uphold the same
// invariant.
TEST(VuEndProgramStatusBroadcast, Vu0EbitBroadcastsStatusToAllInstances)
{
	VuTestHarness h(0);
	LoadDistinctStatusTail(h);
	h.Run();
	ExpectStatusBroadcast(h);
}

// A longer alternating chain (more distinct commits inside the 4-deep window)
// stresses the same invariant from a different starting pipeline.
TEST(VuEndProgramStatusBroadcast, Vu1EbitBroadcastsAfterAlternatingChain)
{
	VuTestHarness h(1);
	IgnoreCommittedFlagVis(h);
	h.SetVf(vf::vf5, 2.0f, 3.0f, 4.0f, 5.0f);
	h.SetVf(vf::vf6, -2.0f, -3.0f, -4.0f, -5.0f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf6, vf::vf0)),  // negative -> S
		UpperOnly(VADD_U(mask::xyzw, vf::vf2, vf::vf0, vf::vf0)),  // zero     -> Z
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf5, vf::vf5)),  // positive
		UpperOnly(VSUB_U(mask::xyzw, vf::vf4, vf::vf6, vf::vf6)),  // zero     -> Z
		EBitNopPair(),
	});
	h.Run();
	ExpectStatusBroadcast(h);
}

} // namespace recompiler_tests
