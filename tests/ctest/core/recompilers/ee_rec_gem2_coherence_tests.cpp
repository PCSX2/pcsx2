// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// GE-M2 residency-coherence regression net. These are GREEN on the pre-flip
// baseline (the memory templates flush every guest reg per op, so nothing is
// allocator-resident when a hand-written op runs) and MUST stay green once the
// residency flip (Phases 3/4) keeps guest GPRs host-resident across ops.
//
// The killer reproduces the SotC block 0x800046d4 shape that broke GE-M1: an
// ADDU writes s1 (which becomes a DIRTY allocator-resident value under the
// flip), then MOVZ s1,v0,s1 reads s1 as BOTH its rt condition and its old rd
// value, then MULT reads s1 again. GE-M1 miscompiled this because the MOVZ leaf
// read s1 through the raw pin-only accessor (armLoadEERegPtr), which is blind to
// allocator residency, so it saw the STALE pre-ADDU value. The harness runs both
// the JIT and the interpreter from the same pre-state and diffs all
// architectural state, so a stale-read regression turns the diff red here (or,
// under a Devel build, trips the GE-M2 I3 tripwire first).
//
// The poison fixture keeps a wide band of guest regs live across a mix of the
// scratch-hungry ops (MOVZ / MULT / MMI PADDW) so a class-B scratch clobber of a
// pool register holding a bystander's resident value surfaces in the final diff.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;
using namespace mips::ee;

namespace {
// s1 (guest 17) is deliberately UNPINNED — the pins are $sp/$ra/$v0/$v1/$a0/$a1/
// $k0/$s0/$at, so s1 exercises the dynamic-allocator residency path, not a pin.
constexpr u32 kSentinelV0 = 0xABCD1234u;
} // namespace

// Move TAKEN: ADDU makes s1 == 0, so MOVZ copies v0 into s1. The MOVZ must read
// the freshly-written (resident) s1 for its condition and old value.
TEST(EeRecGeM2Coherence, MovzAfterDirtyWriteMoveTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x0000000000000005ull);
	h.SetGpr64(reg::a1, 0x00000000FFFFFFFBull); // -5 in the low 32 bits
	h.SetGpr64(reg::v0, kSentinelV0);
	h.SetGpr64(reg::v1, 0x0000000000000002ull);
	h.LoadProgram({
		ADDU(reg::s1, reg::a0, reg::a1),  // s1 = sext32(5 + -5) = 0  (dirty resident)
		MOVZ(reg::s1, reg::v0, reg::s1),  // s1 == 0 -> s1 = v0
		OR(reg::t0, reg::s1, reg::zero),  // read s1 back
		MULT(reg::s1, reg::v1),           // read s1 again (HI/LO = s1 * v1)
		MFLO(reg::a2),                    // a2 = LO
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::s1), static_cast<u64>(kSentinelV0));
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), static_cast<u64>(kSentinelV0));
}

// Move NOT taken: ADDU makes s1 == 7, so MOVZ leaves s1 unchanged. A stale read
// would compute the condition against the pre-ADDU s1 and could wrongly move.
TEST(EeRecGeM2Coherence, MovzAfterDirtyWriteMoveNotTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::s1, 0x1111111111111111ull); // pre-ADDU garbage (must not leak)
	h.SetGpr64(reg::a0, 0x0000000000000003ull);
	h.SetGpr64(reg::a1, 0x0000000000000004ull);
	h.SetGpr64(reg::v0, kSentinelV0);
	h.LoadProgram({
		ADDU(reg::s1, reg::a0, reg::a1),  // s1 = 7 (nonzero, dirty resident)
		MOVZ(reg::s1, reg::v0, reg::s1),  // s1 != 0 -> unchanged
		OR(reg::t0, reg::s1, reg::zero),  // read s1 back
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::s1), 7ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), 7ull);
}

// MOVN counterpart: ADDU makes s1 != 0, so MOVN copies v0 into s1.
TEST(EeRecGeM2Coherence, MovnAfterDirtyWriteMoveTaken)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x0000000000000003ull);
	h.SetGpr64(reg::a1, 0x0000000000000004ull);
	h.SetGpr64(reg::v0, kSentinelV0);
	h.LoadProgram({
		ADDU(reg::s1, reg::a0, reg::a1),  // s1 = 7 (nonzero, dirty resident)
		MOVN(reg::s1, reg::v0, reg::s1),  // s1 != 0 -> s1 = v0
		OR(reg::t0, reg::s1, reg::zero),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::s1), static_cast<u64>(kSentinelV0));
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), static_cast<u64>(kSentinelV0));
}

// A write immediately re-read by the NEXT op with no intervening flush is the
// tightest residency chain: under the flip the reader must observe the dirty
// producer's value directly, not a stale memory copy.
TEST(EeRecGeM2Coherence, WriteThenImmediateReadChain)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0x0000000012340000ull);
	h.SetGpr64(reg::t1, 0x0000000000005678ull);
	h.LoadProgram({
		ADDU(reg::s2, reg::t0, reg::t1),  // s2 = 0x12345678 (dirty resident)
		ADDU(reg::s3, reg::s2, reg::t1),  // reads s2 immediately
		ADDU(reg::s4, reg::s3, reg::s2),  // reads s3 and s2
		OR(reg::t2, reg::s4, reg::zero),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::s2), 0x0000000012345678ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::s3), 0x0000000012340000ull + 0x5678ull + 0x5678ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t2), h.GetGpr64Interp(reg::s4));
}

// Pool-poison: keep a wide band of guest regs live across MOVZ / MULT / MMI so a
// class-B scratch clobber (a hand-emitted op stepping on a pool register that
// holds a bystander's resident value) shows up in the final JIT-vs-interp diff.
TEST(EeRecGeM2Coherence, PoolPoisonWideBandSurvivesMixedOps)
{
	EeRecTestHarness h;
	// Distinct sentinels in a broad set of unpinned/allocatable guest regs.
	h.SetGpr64(reg::t0, 0x0101010101010101ull);
	h.SetGpr64(reg::t1, 0x0202020202020202ull);
	h.SetGpr64(reg::t2, 0x0303030303030303ull);
	h.SetGpr64(reg::t3, 0x0404040404040404ull);
	h.SetGpr64(reg::t4, 0x0000000000000005ull);
	h.SetGpr64(reg::t5, 0x00000000FFFFFFFBull);
	h.SetGpr64(reg::s2, 0x0707070707070707ull);
	h.SetGpr64(reg::s3, 0x0808080808080808ull);
	h.SetGpr64(reg::s4, 0x0000000000000009ull);
	h.SetGpr64(reg::v0, kSentinelV0);
	h.SetGpr64(reg::v1, 0x0000000000000002ull);
	h.LoadProgram({
		ADDU(reg::t4, reg::t4, reg::t5),  // t4 = 0, dirty resident
		MOVZ(reg::t4, reg::v0, reg::t4),  // t4 == 0 -> v0 (movz reads/writes t4)
		MULT(reg::s2, reg::v1),           // HI/LO = s2 * v1
		MFLO(reg::t6),                    // t6 = LO
		PADDW(reg::s3, reg::t0, reg::t1), // MMI -> NEON residency for s3
		ADDU(reg::t7, reg::t2, reg::t3),  // more live scalars
		DADDU(reg::t8, reg::s4, reg::t7), // reads s4 + t7
		OR(reg::t9, reg::s2, reg::t0),    // read s2 back (post-MULT)
	});
	h.Run();
	// The exhaustive check is the harness auto-diff (every field JIT==interp);
	// spot-check a couple of bystanders that must be untouched by the mix.
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), 0x0101010101010101ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t4), static_cast<u64>(kSentinelV0));
}
