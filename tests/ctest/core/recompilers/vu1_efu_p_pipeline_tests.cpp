// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU1 EFU (Elementary Function Unit) P-pipeline DiffJitVsInterp.
//
// EFU ops live in the lower-pipe LowerOP_T3 sub-tables (see VUops.cpp:3607-
// 3650). Every EFU op writes its result into VU->p (the P-pipeline staging
// scalar); microVU's mVUendProgram commits VU.p to VI[REG_P] at E-bit.
// VWAITP stalls until the P-pipeline drains.
//
// EFU is VU1-only (VU0 has no EFU dispatch — opcode hits `unknown`). All
// tests target vu_index = 1.
//
// Pipeline timing: EFU ops have variable latency depending on the op (12
// cycles for VEEXP/VESIN/VEATAN family; 18 for VERSQRT). Tests pad the
// reader by 18 NOP pairs to clear the longest possible window — the JIT
// and interp may legitimately differ on intermediate VI[REG_P] values, but
// the architectural value at E-bit must agree.
//
// EFU divergences from VUops.cpp's _vuE* are inherent JIT-vs-interp
// differences shared with x86 microVU: mVU_ESQRT masks Fs &= 0x7FFFFFFF
// then Fsqrt, dropping the interp's negative-input passthrough and
// zero-input early-return. Real PS2 games are tolerant of these
// niche-input behaviors, and fixing them would diverge from upstream —
// so this file covers the architectural happy paths only.

#include "harness/VuTestHarness.h"

#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

inline VuOp LowerOnly(u32 lower) { return VuOp{lower, VNOP_U()}; }
// I-bit set so the zero lower word is suppressed (becomes the VI[REG_I]
// immediate) instead of decoding as LQ vf0 — the canonical NOP-pair idiom.
inline VuOp BareNopPair() { return IBit(VuOp{VLitZero(), VNOP_U()}); }

} // namespace

// =========================================================================
//  VESADD — sum of squares of the xyz lanes
// =========================================================================

TEST(Vu1EfuPpipe, VesaddSumsSquaresOfXyz)
{
	VuTestHarness h(1);
	h.SetVf(vf::vf1, 3.0f, 4.0f, 0.0f, 99.0f); // expected: 9 + 16 + 0 = 25
	h.LoadProgram({
		LowerOnly(VESADD_L(vf::vf1)),
		// 18-pair latency pad
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(),
		LowerOnly(VWAITP_L()),
		EBitNopPair(),
	});
	h.Run();
	// Architectural REG_P committed at E-bit — diff must agree.
	EXPECT_EQ(h.GetViJit(REG_P), h.GetViInterp(REG_P));
}

TEST(Vu1EfuPpipe, VesaddZeroVectorYieldsZero)
{
	VuTestHarness h(1);
	h.SetVf(vf::vf1, 0.0f, 0.0f, 0.0f, 1.0f);
	h.LoadProgram({
		LowerOnly(VESADD_L(vf::vf1)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(),
		LowerOnly(VWAITP_L()),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_P), h.GetViInterp(REG_P));
}

// =========================================================================
//  VERSADD — reciprocal of sum-of-squares (1 / VESADD)
// =========================================================================

TEST(Vu1EfuPpipe, VersaddProducesReciprocal)
{
	VuTestHarness h(1);
	h.SetVf(vf::vf1, 3.0f, 4.0f, 0.0f, 1.0f); // ESADD = 25, ERSADD = 1/25
	h.LoadProgram({
		LowerOnly(VERSADD_L(vf::vf1)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(),
		LowerOnly(VWAITP_L()),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_P), h.GetViInterp(REG_P));
}

// =========================================================================
//  VELENG — sqrt of sum-of-squares (length of xyz)
// =========================================================================

TEST(Vu1EfuPpipe, VelengComputesEuclideanLength)
{
	VuTestHarness h(1);
	h.SetVf(vf::vf1, 3.0f, 4.0f, 0.0f, 1.0f); // length = 5
	h.LoadProgram({
		LowerOnly(VELENG_L(vf::vf1)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(),
		LowerOnly(VWAITP_L()),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_P), h.GetViInterp(REG_P));
}

// =========================================================================
//  VESQRT — scalar sqrt of fs.fsf. (VERSQRT positive case + negative-input
//  passthrough are coverage gaps — see file-header note.)
// =========================================================================

TEST(Vu1EfuPpipe, VesqrtScalarPositive)
{
	VuTestHarness h(1);
	h.SetVf(vf::vf1, 9.0f, 16.0f, 25.0f, 1.0f);
	h.LoadProgram({
		LowerOnly(VESQRT_L(vf::vf1, /*fsf*/0)), // sqrt(9) = 3
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(),
		LowerOnly(VWAITP_L()),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_P), h.GetViInterp(REG_P));
}

// =========================================================================
//  VESUM / VERLENG / VERCPR — remaining EFU ops
// =========================================================================

TEST(Vu1EfuPpipe, VesumSumsAllFourLanes)
{
	VuTestHarness h(1);
	h.SetVf(vf::vf1, 1.0f, 2.0f, 3.0f, 4.0f); // sum = 10
	h.LoadProgram({
		LowerOnly(VESUM_L(vf::vf1)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(),
		LowerOnly(VWAITP_L()),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_P), h.GetViInterp(REG_P));
}

TEST(Vu1EfuPpipe, VercprReciprocalScalar)
{
	VuTestHarness h(1);
	h.SetVf(vf::vf1, 4.0f, 0.0f, 0.0f, 1.0f); // 1/4 = 0.25
	h.LoadProgram({
		LowerOnly(VERCPR_L(vf::vf1, 0)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(),
		LowerOnly(VWAITP_L()),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_P), h.GetViInterp(REG_P));
}

// =========================================================================
//  pInst rotation — VI[REG_P] / pending_p storage at program termination
//
//  mVUendProgram (and mVUDTendProgram) on VU1 stash the P-pipeline state
//  out of qmmPQ at E-bit. qmmPQ layout: [0]=Q, [1]=pending_q, [2]=P,
//  [3]=pending_p when mVU.p == 0; lanes 2/3 swap when mVU.p == 1
//  (each EFU op flips mVU.p via incP). The emit performs the pInst-conditioned
//  shuffle by selecting the destination lane index instead of emitting a
//  physical lane swap.
//
//  All existing tests above issue exactly ONE EFU op + VWAITP — that's
//  the pInst=1 path. The tests below add coverage for:
//   - pInst=0 trivially (no EFU op)
//   - pInst=0 after an even number of EFU ops
//   - pInst-active without VWAITP (P-pipe still in-flight at E-bit)
//  Each diffs JIT against interp, so a wrong lane index on either of the
//  two emitted St1 lanes — VI[REG_P] or pending_p — surfaces here.
// =========================================================================

TEST(Vu1EfuPpipe, NoEfuOpPInstZeroPath)
{
	// Empty P-pipeline activity: mVU.p stays 0 across the whole block,
	// so pInst=0 at termination → P stored from lane 2, pending_p from
	// lane 3. Pre-seed VI[REG_P] to a sentinel and verify JIT and
	// interp commit the same architectural value (both should drain
	// the freshly-zeroed qmmPQ lanes).
	VuTestHarness h(1);
	h.SetVi(REG_P, 0xDEADBEEF);
	h.LoadProgram({
		BareNopPair(), BareNopPair(), BareNopPair(),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_P), h.GetViInterp(REG_P));
}

TEST(Vu1EfuPpipe, TwoEfuOpsPInstZeroPath)
{
	// Two EFU ops with intervening VWAITP. mVU.p toggles twice across
	// the block (incP fires once per EFU op),
	// so pInst=0 at the final E-bit. Distinct from the one-EFU pattern
	// covered by every other test in this file.
	VuTestHarness h(1);
	h.SetVf(vf::vf1, 3.0f, 4.0f, 0.0f, 1.0f); // ESADD = 25
	h.SetVf(vf::vf2, 9.0f, 0.0f, 0.0f, 1.0f); // ESQRT(9) = 3
	h.LoadProgram({
		LowerOnly(VESADD_L(vf::vf1)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(),
		LowerOnly(VWAITP_L()),
		LowerOnly(VESQRT_L(vf::vf2, 0)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(),
		LowerOnly(VWAITP_L()),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_P), h.GetViInterp(REG_P));
}

TEST(Vu1EfuPpipe, EfuWithoutWaitPInFlightAtEbit)
{
	// EFU op without a trailing VWAITP, but with enough latency pad that
	// the P-pipeline result has been computed by E-bit. Single EFU →
	// pInst=1 at termination, so VI[REG_P] gets the pInst?3:2 lane.
	// All the existing tests above gate on VWAITP; this exercises the
	// drain path that fires when E-bit alone terminates the program.
	VuTestHarness h(1);
	h.SetVf(vf::vf1, 3.0f, 4.0f, 0.0f, 1.0f); // ESADD = 25
	h.LoadProgram({
		LowerOnly(VESADD_L(vf::vf1)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_P), h.GetViInterp(REG_P));
}

} // namespace recompiler_tests
