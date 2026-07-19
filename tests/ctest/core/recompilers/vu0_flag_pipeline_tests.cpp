// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU0 flag-pipeline DiffJitVsInterp suite. Covers MAC/STATUS/CLIP
// flag generation by FMAC ops + readback by VFM*/VFS*/VFC* lower-pipe ops.
//
// MAC flag layout (per VUflags.cpp:15-47): each lane gets a 4-bit nibble
// {O, U, S, Z} at bit positions {12-15, 8-11, 4-7, 0-3}. Within a nibble:
//   shift = 3 (x), 2 (y), 1 (z), 0 (w).
// So bit 3 = Z_x, bit 2 = Z_y, ..., bit 7 = S_x, bit 11 = U_x, bit 15 = O_x.
//
// STATUS flag (per VU_STAT_UPDATE VUflags.cpp:89-97): bit 0x1 = any Z, 0x2 =
// any S, 0x4 = any U, 0x8 = any O. Plus sticky/D/I bits the interpreter
// preserves but FMACs don't directly touch.
//
// CLIP flag (per VCLIP VUops.cpp): 24-bit rolling history. Each VCLIP shifts
// the prior 18 bits left by 6 and ORs in 6 new bits {x>w, y>w, z>w, x<-w,
// y<-w, z<-w}.
//
// Pipeline: FMAC results commit to REG_MAC_FLAG / REG_STATUS_FLAG via a
// 4-stage rolling buffer (VU->fmac[0..3] in the interp; mVU.macFlag in the
// JIT). Reads earlier than the commit boundary read stale values. These
// tests space FMAC -> reader by 4+ pairs to clear the pipeline window.

#include "harness/VuTestHarness.h"

#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

inline VuOp LowerOnly(u32 lower) { return VuOp{lower, VNOP_U()}; }
inline VuOp UpperOnly(u32 upper) { return VuOp{0, upper}; }

// Plain NOP pair without I-bit. Reads as zeros in the lower; the upper is
// the architectural NOP. Used for pipeline padding between an FMAC writer
// and a flag reader so the 4-stage commit boundary is crossed.
inline VuOp BareNopPair() { return VuOp{0, VNOP_U()}; }

} // namespace

// =========================================================================
//  MAC flag — Z bits via zero result
// =========================================================================

TEST(Vu0FlagPipeline, FmandReadsZeroFlagAfterFmacZeroResult)
{
	// VADD vf1, vf0, vf0 → vf1 = vf0 = (0, 0, 0, 1). Lanes x/y/z are zero
	// and lane w is non-zero, so MAC = bits[3:1] set (Z_x, Z_y, Z_z) and
	// bit 4 (S_w) clear → 0x000E.
	// FMOR vi1, vi0 → vi1 = (MAC & 0xFFFF) | 0 = MAC.
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 0xFFFFu); // pre-poison so we can see the write
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf0, vf::vf0)),
		BareNopPair(),
		BareNopPair(),
		BareNopPair(),
		BareNopPair(),
		LowerOnly(VFMOR_L(vi::vi1, vi::vi0)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

TEST(Vu0FlagPipeline, FmandMaskedReadsOnlyRequestedBits)
{
	// FMAND vi1, vi2 → vi1 = vi2 & MAC. Mask vi2 = 0x000F (Z bits only).
	VuTestHarness h(0);
	h.SetVi(vi::vi2, 0x000Fu);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf0, vf::vf0)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFMAND_L(vi::vi1, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

TEST(Vu0FlagPipeline, FmeqMatchesExactMacBits)
{
	// FMEQ vi1, vi2: vi1 = (MAC == vi2) ? 1 : 0. Set vi2 = 0x000E (the
	// expected MAC for VADD vf1, vf0, vf0).
	VuTestHarness h(0);
	h.SetVi(vi::vi2, 0x000Eu);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf0, vf::vf0)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFMEQ_L(vi::vi1, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

// =========================================================================
//  MAC flag — S bits via signed result
// =========================================================================

TEST(Vu0FlagPipeline, FmacWithNegativeResultSetsSignBits)
{
	// vf1 = (-1.5, -2.5, -3.5, -4.5); VSUB vf2, vf0, vf1 → vf2 ≈ +vf1
	// (since vf0 = 0,0,0,1, w lane is 1 - (-4.5) = 5.5 → positive).
	// So x,y,z negative-input becomes positive output and S bits clear;
	// instead, do VADD vf2, vf1, vf0 — keeps negative lanes for x,y,z.
	VuTestHarness h(0);
	h.SetVf(vf::vf1, -1.5f, -2.5f, -3.5f, -4.5f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf2, vf::vf1, vf::vf0)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFMOR_L(vi::vi1, vi::vi0)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

// =========================================================================
//  STATUS flag — VU_STAT_UPDATE rollup of MAC bits
// =========================================================================

TEST(Vu0FlagPipeline, FsandReadsStatusZBitAfterFmacZeroResult)
{
	// MAC has Z bits set → STATUS bit 0x1 set. FSAND vi1, 0xFFF reads
	// (STATUS & 0xFFF) & 0xFFF — i.e. all 12 status bits.
	VuTestHarness h(0);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf0, vf::vf0)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFSAND_L(vi::vi1, 0xFFFu)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

TEST(Vu0FlagPipeline, FseqMatchesStatusExactly)
{
	// STATUS after VADD vf1, vf0, vf0 should be 0x001 (Z bit set, all sticky
	// bits and D/I bits clear at start). FSEQ vi1, 0x001 → 1.
	VuTestHarness h(0);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf0, vf::vf0)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFSEQ_L(vi::vi1, 0x001u)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

TEST(Vu0FlagPipeline, FsorReadsStatusOred)
{
	// FSOR vi1, imm: vi1 = (STATUS & 0xFFF) | imm.
	VuTestHarness h(0);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf0, vf::vf0)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFSOR_L(vi::vi1, 0x800u)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

// =========================================================================
//  STATUS flag — FSSET writes sticky bits
// =========================================================================

TEST(Vu0FlagPipeline, FssetWritesStickyAndPreservesNonSticky)
{
	// FSSET imm12: VU->statusflag = (imm & 0xFC0) | (statusflag & 0x3F).
	// Sets sticky bits (top 6 of low 12), preserves currents (bottom 6).
	VuTestHarness h(0);
	h.LoadProgram({
		LowerOnly(VFSSET_L(0xFC0u)),                    // turn on all sticky
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFSAND_L(vi::vi1, 0xFC0u)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

// =========================================================================
//  CLIP flag — FCSET / FCGET / FCAND / FCOR / FCEQ
// =========================================================================

TEST(Vu0FlagPipeline, FcsetThenFcgetReadsBack12Bits)
{
	// FCSET writes the 24-bit imm to clipflag. FCGET reads (CLIP & 0x0FFF)
	// — i.e. only the bottom 12 bits.
	VuTestHarness h(0);
	h.LoadProgram({
		LowerOnly(VFCSET_L(0xABCDEFu)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFCGET_L(vi::vi1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

TEST(Vu0FlagPipeline, FcandSetsVi1OneOnAnyMatchingBit)
{
	// FCAND: VI[1] = ((CLIP & 0xFFFFFF) & imm24) != 0 ? 1 : 0.
	// CLIP set to 0x0F0F0F, mask 0x000001 → matches bit 0 → vi1 = 1.
	VuTestHarness h(0);
	h.LoadProgram({
		LowerOnly(VFCSET_L(0x0F0F0Fu)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFCAND_L(0x000001u)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

TEST(Vu0FlagPipeline, FcandSetsVi1ZeroOnNoMatchingBit)
{
	VuTestHarness h(0);
	h.LoadProgram({
		LowerOnly(VFCSET_L(0x0F0F0Fu)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFCAND_L(0xF0F0F0u)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

TEST(Vu0FlagPipeline, FcorSetsVi1OneWhenAllOnesAfterOr)
{
	// FCOR: vi1 = ((CLIP | imm24) == 0xFFFFFF) ? 1 : 0.
	// CLIP = 0xF0F0F0; imm = 0x0F0F0F → OR = 0xFFFFFF → vi1 = 1.
	VuTestHarness h(0);
	h.LoadProgram({
		LowerOnly(VFCSET_L(0xF0F0F0u)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFCOR_L(0x0F0F0Fu)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

TEST(Vu0FlagPipeline, FceqSetsVi1OneOnExactMatch)
{
	VuTestHarness h(0);
	h.LoadProgram({
		LowerOnly(VFCSET_L(0x123456u)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFCEQ_L(0x123456u)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

TEST(Vu0FlagPipeline, FceqSetsVi1ZeroOnMismatch)
{
	VuTestHarness h(0);
	h.LoadProgram({
		LowerOnly(VFCSET_L(0x123456u)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VFCEQ_L(0x123455u)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
}

// =========================================================================
//  Pipeline timing — back-to-back FMAC then read (no padding) is the
//  fragile case that catches commit-stage bugs in the JIT.
// =========================================================================

TEST(Vu0FlagPipeline, FsandImmediatelyAfterFmacReadsStatusPipelineCorrectly)
{
	VuTestHarness h(0);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf0, vf::vf0)),
		LowerOnly(VFSAND_L(vi::vi1, 0xFFFu)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
	EXPECT_EQ(h.GetViJit(REG_STATUS_FLAG), h.GetViInterp(REG_STATUS_FLAG));
}

// =========================================================================
//  Architectural-state invariants — REG_MAC_FLAG / REG_STATUS_FLAG /
//  REG_CLIP_FLAG always agree post-program even if intermediate reads
//  diverge. This is the bare minimum the JIT should preserve.
// =========================================================================

TEST(Vu0FlagPipeline, RegMacFlagMatchesAfterChainOfFmacs)
{
	// Three back-to-back FMACs producing distinct flag patterns. After
	// E-bit drains the pipeline, REG_MAC_FLAG must hold the *last* FMAC's
	// flag — the prior two are gone. Both engines should land here.
	VuTestHarness h(0);
	h.SetVf(vf::vf2, 1.0f, 0.0f, -3.0f, 4.0f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf0, vf::vf0)), // all-Z input
		BareNopPair(),
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf2, vf::vf0)), // mixed
		BareNopPair(),
		UpperOnly(VSUB_U(mask::xyzw, vf::vf3, vf::vf0, vf::vf2)), // negate vf2
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_MAC_FLAG),    h.GetViInterp(REG_MAC_FLAG));
	EXPECT_EQ(h.GetViJit(REG_STATUS_FLAG), h.GetViInterp(REG_STATUS_FLAG));
	EXPECT_EQ(h.GetViJit(REG_CLIP_FLAG),   h.GetViInterp(REG_CLIP_FLAG));
}

} // namespace recompiler_tests
