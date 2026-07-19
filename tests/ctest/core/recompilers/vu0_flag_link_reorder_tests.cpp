// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
//
// Cross-block flag-link coverage the single-block flag-pipeline suite misses.
//
// These build multi-block programs that cross an EXACT-MATCH flag link at a
// NON-canonical producer exit phase: a self-loop whose top block reads a flag
// (which forces needExactMatch on the back-edge) while its body writes that
// same flag 3x (so the ring pointer ends at a non-zero phase). At such a link
// mVUsetupFlags must reconcile the predecessor's ring arrangement with the
// successor's expected one, or the successor reads stale flag instances.
//
// Bidirectionally verified while porting the compile-time flag-slot rework:
// each of these passes with the block-link flag handling correct and diverges
// (JIT vs interp) if it is disabled, so they pin behaviour a naive change would
// silently break. See vu0_flag_pipeline_tests.cpp for the single-block cases.

#include "harness/VuTestHarness.h"
#include "VU.h"
#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {
inline VuOp LowerOnly(u32 lower) { return VuOp{lower, VNOP_U()}; }
inline VuOp UpperOnly(u32 upper) { return VuOp{0, upper}; }
inline VuOp LoadViImm(u32 dst, u32 imm) { return LowerOnly(VIADDIU_L(dst, vi::vi0, imm)); }
inline VuOp BareNopPair() { return VuOp{0, VNOP_U()}; }

// A 2-iteration self-loop. `topReader` is the loop-top lower op that reads the
// flag under test (forcing an exact-match back-edge); `bodyWriter` upper op is
// emitted 3x to leave the flag ring at a non-canonical exit phase. The
// back-edge (pair 9 -> pair 2) links the loop-top block to itself across that
// phase.
std::vector<VuOp> SelfLoop(u32 topReader, u32 bodyWriter)
{
	return {
		LoadViImm(vi::vi2, 2),                     // 0: counter = 2
		LoadViImm(vi::vi3, 0xFFF),                 // 1: scratch mask
		LowerOnly(topReader),                      // 2: L1 — reads flag [branch target]
		UpperOnly(bodyWriter),                     // 3: flag write #1
		UpperOnly(bodyWriter),                     // 4: flag write #2
		UpperOnly(bodyWriter),                     // 5: flag write #3 (non-canonical phase)
		LowerOnly(VISUBIU_L(vi::vi2, vi::vi2, 1)), // 6: counter -= 1
		LowerOnly(0),                              // 7: hazard pad
		LowerOnly(0),                              // 8: hazard pad
		LowerOnly(VIBNE_L(vi::vi2, vi::vi0, -8)),  // 9: if counter != 0 -> pair 2
		LowerOnly(0),                              // 10: delay slot
		EBitNopPair(),                   // 11: terminate
	};
}
} // namespace

// Rotation guard: predecessor writes three distinct MAC flags in its last
// cycles, then the successor reads MAC early - so the correct read lands on an
// older pipeline instance, not the most-recent. Pins that the block-link flag
// handling preserves the full 4-deep pipeline order, not just the newest
// instance (a "read most-recent only" shortcut passes the other tests here but
// fails the delay-slot variant below). Ground truth is the interpreter.
TEST(Vu0FlagLinkReorder, MacRotationDistinctWrites)
{
	VuTestHarness h(0);
	h.SetVf(vf::vf2, 0.0f, 0.0f, 0.0f, 1.0f);   // -> (0,0,0,2): Z bits
	h.SetVf(vf::vf3, -1.0f, -1.0f, -1.0f, -1.0f); // -> (-1,-1,-1,0): S + Z_w
	h.SetVf(vf::vf4, 5.0f, 5.0f, 5.0f, 5.0f);   // -> (5,5,5,6): no flags
	h.LoadProgram({
		LoadViImm(vi::vi2, 3),                              // 0: counter
		LoadViImm(vi::vi3, 0xFFF),                          // 1: mask
		LowerOnly(VFMAND_L(vi::vi1, vi::vi3)),              // 2: L1 read MAC [target]
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf0)), // 3: mac A
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf3, vf::vf0)), // 4: mac B
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf4, vf::vf0)), // 5: mac C
		LowerOnly(VISUBIU_L(vi::vi2, vi::vi2, 1)),          // 6
		LowerOnly(0),                                       // 7
		LowerOnly(0),                                       // 8
		LowerOnly(VIBNE_L(vi::vi2, vi::vi0, -8)),           // 9
		LowerOnly(0),                                       // 10
		EBitNopPair(),                                      // 11
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
	EXPECT_EQ(h.GetViJit(REG_MAC_FLAG), h.GetViInterp(REG_MAC_FLAG));
}

// Rotation guard (delay-slot variant): a distinct MAC-writing FMAC sits in the
// branch DELAY SLOT — the very last cycle of the block — so the predecessor's
// tail has flag writes inside the link's live window (the reorder's sortFlag
// returns >1 distinct instance). The successor's early MAC read must land on an
// older instance; a "read most-recent only" shortcut reads the wrong one here
// (JIT 0x4a vs interp 0x0). FMACs in branch delay slots are common in real VU
// code, so this is the load-bearing case for the block-link reorder.
TEST(Vu0FlagLinkReorder, MacRotationDelaySlotFmac)
{
	VuTestHarness h(0);
	h.SetVf(vf::vf2, 0.0f, 0.0f, 0.0f, 1.0f);
	h.SetVf(vf::vf3, -1.0f, -1.0f, -1.0f, -1.0f);
	h.SetVf(vf::vf4, 5.0f, 5.0f, 5.0f, 5.0f);
	h.SetVf(vf::vf5, 0.0f, -2.0f, 0.0f, 3.0f);
	h.LoadProgram({
		LoadViImm(vi::vi2, 3),                              // 0
		LoadViImm(vi::vi3, 0xFFF),                          // 1
		LowerOnly(VFMAND_L(vi::vi1, vi::vi3)),              // 2: L1 read MAC [target]
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf0)), // 3
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf3, vf::vf0)), // 4
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf4, vf::vf0)), // 5
		LowerOnly(VISUBIU_L(vi::vi2, vi::vi2, 1)),          // 6
		LowerOnly(0),                                       // 7
		LowerOnly(VIBNE_L(vi::vi2, vi::vi0, -7)),           // 8: -> pair 2 (delay = pair 9)
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf5, vf::vf0)), // 9: DELAY SLOT FMAC (mac D)
		EBitNopPair(),                                      // 10
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
	EXPECT_EQ(h.GetViJit(REG_MAC_FLAG), h.GetViInterp(REG_MAC_FLAG));
}

TEST(Vu0FlagLinkReorder, MacFlagAcrossExactMatchBackEdge)
{
	VuTestHarness h(0);
	h.SetVf(vf::vf2, 1.0f, 0.0f, -3.0f, 4.0f); // mixed lanes -> non-trivial MAC
	h.LoadProgram(SelfLoop(VFMAND_L(vi::vi1, vi::vi3),
		VADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf0)));
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
	EXPECT_EQ(h.GetViJit(REG_MAC_FLAG), h.GetViInterp(REG_MAC_FLAG));
}

TEST(Vu0FlagLinkReorder, ClipFlagAcrossExactMatchBackEdge)
{
	VuTestHarness h(0);
	h.SetVf(vf::vf2, 5.0f, -6.0f, 7.0f, 2.0f); // xyz vs w drives VCLIP bits
	h.LoadProgram(SelfLoop(VFCAND_L(0x000001u), VCLIP_U(vf::vf2, vf::vf2)));
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
	EXPECT_EQ(h.GetViJit(REG_CLIP_FLAG), h.GetViInterp(REG_CLIP_FLAG));
}

TEST(Vu0FlagLinkReorder, StatusFlagAcrossExactMatchBackEdge)
{
	VuTestHarness h(0);
	h.SetVf(vf::vf2, 1.0f, 0.0f, -3.0f, 4.0f);
	h.LoadProgram(SelfLoop(VFSAND_L(vi::vi1, 0xFFFu),
		VADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf0)));
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
	EXPECT_EQ(h.GetViJit(REG_STATUS_FLAG), h.GetViInterp(REG_STATUS_FLAG));
}

// Control: the same 3 flag writes + read WITHOUT a branch (single block, no
// exact-match link). Shows the divergences above are specific to the
// cross-block link, not to flag generation/readback.
TEST(Vu0FlagLinkReorder, StatusSingleBlockControl)
{
	VuTestHarness h(0);
	h.SetVf(vf::vf2, 1.0f, 0.0f, -3.0f, 4.0f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf0)),
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf0)),
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf0)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFSAND_L(vi::vi1, 0xFFFu)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_STATUS_FLAG), h.GetViInterp(REG_STATUS_FLAG));
}

// Control for the E-bit lookahead fix below: the same unconsumed-MAC program,
// but with the E-bit in the block that writes MAC (no branch). This always
// passed - eBitPass1 already forces needExactMatch|=7 on VU0 when it sees the
// E-bit in the block being compiled. It is the asymmetry with the cross-block
// case that made the bug: keep this passing, or the in-block rule has regressed.
TEST(Vu0FlagLinkReorder, UnconsumedMacFlagInBlockEbitControl)
{
	VuTestHarness h(0);
	h.SetVf(vf::vf2, 1.0f, 0.0f, -3.0f, 4.0f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf0)),
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf0)),
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf0)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_MAC_FLAG), h.GetViInterp(REG_MAC_FLAG));
}

// Regression: a flag written but never read by any instruction still has to be
// finalised correctly, because mVUendProgram stores it into VI[REG_*_FLAG] and
// COP2 can read it back on VU0. Here the loop writes MAC 3x/iter and reads only
// STATUS, and the E-bit sits in a successor block - so _mVUflagPass's lookahead
// used to hit the E-bit and break without marking any flag needed. The loop body
// then elided the very MAC writes the E-bit stores, and REG_MAC_FLAG finalised
// from a never-written ring instance (findFlagInst's all-(-1) fallback, slot 0):
// JIT=0x0 vs interp=0x24. Guards the ABI-14 fix; also covers STATUS/CLIP, which
// is why no test in this file needs IgnoreViInDiff any more.
TEST(Vu0FlagLinkReorder, UnconsumedMacFlagFinalizedAtSuccessorBlockEbit)
{
	VuTestHarness h(0);
	h.SetVf(vf::vf2, 1.0f, 0.0f, -3.0f, 4.0f);
	h.LoadProgram(SelfLoop(VFSAND_L(vi::vi1, 0xFFFu),
		VADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf0)));
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_MAC_FLAG), h.GetViInterp(REG_MAC_FLAG));
}

// Same program on VU1. The E-bit lookahead fix is deliberately not gated on
// isVU0 (unlike the in-block eBitPass1 rule it copies, which is VU0-only because
// COP2 reads VU0's flags back): mVUdispatcherA reloads the flag ring from
// VI[REG_*_FLAG] at program entry, so a stale instance left by one VU1 program
// is observable by the next one that reads MAC before writing it. Failed the
// same way as the VU0 case (JIT=0x0 vs interp=0x24) while the gate was in place.
TEST(Vu0FlagLinkReorder, Vu1UnconsumedMacFlagFinalizedAtSuccessorBlockEbit)
{
	VuTestHarness h(1);
	h.SetVf(vf::vf2, 1.0f, 0.0f, -3.0f, 4.0f);
	h.LoadProgram(SelfLoop(VFSAND_L(vi::vi1, 0xFFFu),
		VADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf0)));
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_MAC_FLAG), h.GetViInterp(REG_MAC_FLAG));
}

// The other half of the E-bit rule: an FMAC inside the E-bit block. Here the
// forcing has to come from eBitPass1 (the in-block rule), not from the lookahead
// - the block that ends the program computes the flag itself. Both rules were
// isVU0-gated; ungating only the lookahead left this case broken on VU1, where
// REG_MAC_FLAG finalised from the branch delay slot's VADD (0x40) instead of the
// E-bit block's own VMUL (0x60). Caught by the VU1 ABI-digest probe, which runs
// this exact program shape through the JIT-vs-interp diff.
TEST(Vu0FlagLinkReorder, Vu1FmacInsideEbitBlockFinalizesOwnMac)
{
	VuTestHarness h(1);
	h.SetVf(vf::vf1, 1.5f, -2.25f, 3.0f, 0.0625f);
	h.SetVf(vf::vf2, 4.0f, 0.5f, -1.0f, 8.0f);
	h.SetVi(vi::vi1, 1); // branch taken
	h.LoadProgram({
		LowerOnly(VIBNE_L(vi::vi1, vi::vi0, 3)),                  // 0: taken -> pair 4
		UpperOnly(VADD_U(mask::xyzw, vf::vf4, vf::vf1, vf::vf2)), // 1: delay slot (MAC 0x40)
		UpperOnly(VSUB_U(mask::xyzw, vf::vf5, vf::vf1, vf::vf2)), // 2: skipped
		BareNopPair(),                                            // 3
		VuOp{0, bits::E | VMUL_U(mask::xyzw, vf::vf6, vf::vf1, vf::vf2)}, // 4: E-bit FMAC (MAC 0x60)
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_MAC_FLAG), h.GetViInterp(REG_MAC_FLAG));
}

} // namespace recompiler_tests
