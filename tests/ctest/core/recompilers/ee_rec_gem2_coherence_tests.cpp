// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
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

#include <set>

// EE dynamic-allocator pool membership predicate (arm64/iCore-arm64.cpp). Reads
// EE_ALLOCATABLE_MASK directly, so it is the bulletproof structural pin for the
// pool carve — see PoolExcludesArgAndScratchRegs below. Forward-declared (global
// scope, no namespace) to avoid pulling the vixl-heavy iCore-arm64.h into a test.
bool _isAllocatableArm64GPR(int armreg);

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

// SQ reads a full 128-bit guest GPR through the raw quad-load + residency-merge
// path (recVTLB SQ, which only iFlushCall(FLUSH_CONSTANT_REGS) — it does NOT
// write back scalar allocator slots). Under the flip, the lower 64 bits live
// dirty in a scalar slot produced by the preceding ADDU; armMergeEEResidentIntoQuad
// must Ins that over the stale memory lower half. The upper 64 come from memory.
TEST(EeRecGeM2Coherence, SqAfterScalarWriteMergesDirtyLowerHalf)
{
	EeRecTestHarness h;
	constexpr u32 kAddr = RecompilerTestEnvironment::kScratchAddr;
	h.SetGpr128(reg::s2, 0x3333333344444444ull, 0x1111111122222222ull); // lo, hi
	h.SetGpr64(reg::t0, 0x00000000AAAA0000ull);
	h.SetGpr64(reg::t1, 0x000000000000BBBBull);
	h.SetGpr64(reg::t3, kAddr);
	h.TrackMemWindow(kAddr, 16);
	h.LoadProgram({
		ADDU(reg::s2, reg::t0, reg::t1), // s2.lo = sext32(0xAAAABBBB); s2.hi preserved
		SQ(reg::s2, 0, reg::t3),         // store the full 128 bits of s2
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::s2), 0xFFFFFFFFAAAABBBBull);
	EXPECT_EQ(h.ReadU64(kAddr + 0), 0xFFFFFFFFAAAABBBBull); // lower half (dirty-merged)
	EXPECT_EQ(h.ReadU64(kAddr + 8), 0x1111111122222222ull); // upper half (from memory)
}

// COP2 macro ops routed through the mVU emitter (the REC_COP2_mVU0_ARM64 set:
// VMTIR/VMFIR/VILWR/VISWR/...) emit INLINE in the EE block and allocate VU0
// integer (VI) hosts first-fit from the {x14, x15, x28} pool (microRegAlloc cop2
// mode; mVUmacroSetupCOP2State reset(true)). x14/x15 are ALSO EE guest-GPR
// allocatable hosts, so under the residency flip a guest scalar can be live in
// x14/x15 exactly when the macro op wants them. The no-C-call routed ops emit no
// unconditional iFlushCall, so mVUmacroEmitPrologue (microVU-arm64.cpp) must
// itself writeback+free x14/x15/x28 from the EE cache before the mVU emit. This
// keeps a wide band of dirty guest scalars live across a VMTIR (1 VI -> x14) and
// a VILWR (2 VIs -> x14+x15); if the flip left a guest reg resident in a host the
// macro op then reallocated and the prologue dropped its eviction, the clobber
// surfaces in the Run() auto-diff (EE + VU0) here and at the block-end flush.
// (x28 is a LATENT case: no current routed op allocates a third VI slot, so
// first-fit never reaches it. It is held by the same unconditional prologue free
// plus the epilogue NEON-watermark tripwire, which fires if an emitter ever grows
// past the evicted window.) Green on the pre-flip baseline: nothing is
// EE-resident, so the prologue frees empty slots and the macro ops own the pool.
TEST(EeRecGeM2Coherence, Vu0RoutedMacroOpPreservesLiveScalarBand)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(2, 0x11111111u, 0x22222222u, 0x33333333u, 0x44445555u);
	h.SeedVu0Vi(5, 3); // VILWR address source (reads VUmem[VI[5]*16].x; value auto-diffed)
	// Distinct 64-bit sentinels across a broad band of unpinned guest regs (the
	// pins are $sp/$ra/$v0/$v1/$a0/$a1/$k0/$s0/$at — all avoided here).
	h.SetGpr64(reg::t0, 0x1010101010101010ull);
	h.SetGpr64(reg::t1, 0x2020202020202020ull);
	h.SetGpr64(reg::t2, 0x3030303030303030ull);
	h.SetGpr64(reg::t3, 0x4040404040404040ull);
	h.SetGpr64(reg::t5, 0x0000000000000005ull);
	h.SetGpr64(reg::t6, 0x00000000FFFFFFFBull);
	h.SetGpr64(reg::s1, 0x6161616161616161ull);
	h.SetGpr64(reg::s2, 0x7272727272727272ull);
	h.SetGpr64(reg::s4, 0x8484848484848484ull);
	h.SetGpr64(reg::s5, 0x9595959595959595ull);
	h.LoadProgram({
		// Dirty a broad band right before the macro ops so several land in the
		// {x14,x15} pool slots as MODE_WRITE residents under the flip.
		ADDU (reg::t4, reg::t5, reg::t6),          // t4 = 0, dirty resident
		ADDU (reg::t7, reg::t0, reg::t1),          // dirty resident
		ADDU (reg::t8, reg::t2, reg::t3),          // dirty resident
		ADDU (reg::t9, reg::s1, reg::s2),          // dirty resident
		ADDU (reg::a2, reg::s4, reg::s5),          // dirty resident (a2/a3 unpinned)
		ADDU (reg::a3, reg::t0, reg::t3),          // dirty resident
		// Routed macro ops that reallocate VI hosts x14 (VMTIR) and x14+x15 (VILWR).
		VMTIR_C2(/*fsf=x*/0, /*it*/1, /*fs*/2),    // VI[1] = VF[2].x low16 = 0x1111
		VILWR_C2(/*mask=x*/0x8, /*it*/2, /*is*/5), // VI[2] = VUmem[VI[5]*16].x (2 VI hosts)
		VMTIR_C2(/*fsf=y*/1, /*it*/3, /*fs*/2),    // VI[3] = VF[2].y low16 = 0x2222
		// Read the dirtied band back AFTER the macro ops.
		DADDU(reg::s3, reg::t4, reg::t7),
		DADDU(reg::s6, reg::t8, reg::t9),
		DADDU(reg::s7, reg::a2, reg::a3),
	});
	h.Run();
	// VMTIR results are deterministic; the VILWR value is memory-defined but the
	// exhaustive JIT==interp check is the Run() auto-diff (EE + VU0).
	EXPECT_EQ(h.GetVu0ViJit(1), 0x1111u);
	EXPECT_EQ(h.GetVu0ViJit(3), 0x2222u);
	EXPECT_EQ(h.GetVu0ViJit(1), h.GetVu0ViInterp(1));
	EXPECT_EQ(h.GetVu0ViJit(2), h.GetVu0ViInterp(2));
	EXPECT_EQ(h.GetVu0ViJit(3), h.GetVu0ViInterp(3));
	// Spot-check pure-source bystanders (never a dest -> keep their sentinels).
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), 0x1010101010101010ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::s5), 0x9595959595959595ull);
}

// Structural pin for the pool carve (GE-M2 Phase-3 fix b36606dbb). The EE
// dynamic-allocator pool MUST exclude every host reg that is a C-call argument
// register (x0-x3 = RWARG1-4) or hardcoded emitter scratch — a resident guest
// value in such a host is silently clobbered by the raw scratch/arg use (the
// UYA regression: cop2EmitFlagUpdate raw-writes w2/w3, and pre-fix x2/x3 were
// still in the pool). x0/x1 were carved in Phase 0a; x2/x3 in the fix. This is
// the bulletproof guard: it reads EE_ALLOCATABLE_MASK directly and cannot go
// silently insensitive the way the residency-shape poison test below can. If
// anyone re-admits x2/x3 (or any arg/scratch reg) to EE_ALLOCATABLE_MASK, this
// goes red immediately with an exact reg number.
TEST(EeRecGeM2Coherence, PoolExcludesArgAndScratchRegs)
{
	// The complete EE-allocatable set after GE-M2: caller-saved temporaries
	// x4-x7 and x14/x15 (the latter shared with the mVU macro VI pool, fenced),
	// plus x28 (the only callee-saved pool member). Everything else is a pin,
	// reserved scratch, an arg reg, or a platform/framework reservation.
	const std::set<int> kExpectedAllocatable = {4, 5, 6, 7, 14, 15, 28};
	for (int reg = 0; reg <= 30; ++reg)
	{
		const bool expected = kExpectedAllocatable.count(reg) != 0;
		EXPECT_EQ(_isAllocatableArm64GPR(reg), expected)
			<< "x" << reg << " EE-allocatable mismatch (expected "
			<< (expected ? "allocatable" : "reserved") << ")";
	}
	// The regression this whole campaign fixed: the four C-call arg registers
	// (RWARG1-4 = x0-x3) must never be handed to a resident guest GPR.
	for (int argReg = 0; argReg <= 3; ++argReg)
		EXPECT_FALSE(_isAllocatableArm64GPR(argReg))
			<< "x" << argReg << " (RWARG" << (argReg + 1)
			<< ") must stay carved out of the EE pool";
}

// Behavioral reproduction of the exact UYA clobber shape. A hand-emitted COP2
// FMAC op runs cop2EmitFlagUpdate at the tail of every flag-writing macro op
// (iCOP2-arm64.cpp), which raw-writes w2/w3 (== x2/x3) as flag scratch WITHOUT
// booking them in the allocator. Pre-fix, with x2/x3 in the pool, a guest GPR
// left resident there by a preceding EE ALU op under the Phase-3 flip was
// silently destroyed — pervasive corruption (grey render + boot hang) that the
// offline stepdiff gate missed.
//
// Sensitivity depends on the second FMAC op NOT flushing the caller-saved band
// at setup. The shared COP2 analysis (iR5900Analysis COP2MicroFinishPass) tags
// only the FIRST COP2 op of a sync-free run with EEINST_COP2_FINISH_VU0; that
// op's setup does the iFlushCall(FLUSH_FREE_XMM|FLUSH_FREE_VU0) that evicts
// caller-saved x2-x7. The SECOND FMAC op gets no sync/finish tag, so its
// cop2EmitConditionalSync early-returns with no flush — its cop2EmitFlagUpdate
// runs while the band re-populated between the two ops is still resident. So:
//   VADD #1        -> absorbs the FINISH flush (frees x2-x7)
//   wide ADDU band -> repopulates the low pool regs (x2/x3 first-free pre-fix)
//   VADD #2        -> no flush; cop2EmitFlagUpdate raw-writes w2/w3
//   read the band  -> a clobbered x2/x3 member surfaces in the Run() auto-diff
// Post-fix the band never lands in x2/x3 (carved), so w2/w3 hit dead hosts and
// the diff stays clean. Verified RED with x2/x3 temporarily re-added to
// EE_ALLOCATABLE_MASK, GREEN with the b36606dbb carve.
TEST(EeRecGeM2Coherence, HandEmittedCop2FlagUpdatePreservesResidentBand)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// Non-zero VF operands so the two FMACs actually compute a MAC/status flag
	// (the flag path is what drives the w2/w3 scratch writes).
	h.SeedVu0VfBits(2, 0x3F800000u, 0x40000000u, 0xC0000000u, 0x3F000000u);
	h.SeedVu0VfBits(3, 0x40400000u, 0xBF800000u, 0x41000000u, 0x00000000u);
	h.SeedVu0VfBits(5, 0x41200000u, 0x40800000u, 0xC1000000u, 0x3E800000u);
	h.SeedVu0VfBits(6, 0x40000000u, 0x40A00000u, 0xBF000000u, 0x40C00000u);

	// Distinct 64-bit sentinels across a wide band of unpinned/allocatable guest
	// regs, dirtied BETWEEN the two FMAC ops so several are resident in the low
	// pool hosts (x2/x3 first-free pre-fix) exactly when VADD #2 scribbles w2/w3.
	h.SetGpr64(reg::t0, 0x1010101010101010ull);
	h.SetGpr64(reg::t1, 0x2020202020202020ull);
	h.SetGpr64(reg::t2, 0x3030303030303030ull);
	h.SetGpr64(reg::t3, 0x4040404040404040ull);
	h.SetGpr64(reg::t5, 0x0000000012340000ull);
	h.SetGpr64(reg::t6, 0x0000000000005678ull);
	h.SetGpr64(reg::s2, 0x7272727272727272ull);
	h.SetGpr64(reg::s3, 0x8383838383838383ull);
	h.SetGpr64(reg::s4, 0x9494949494949494ull);
	h.SetGpr64(reg::s5, 0xA5A5A5A5A5A5A5A5ull);

	h.LoadProgram({
		VADD_C2(0xF, /*fd*/4, /*fs*/2, /*ft*/3), // op1: absorbs the FINISH flush
		// Wide band dirtied after the flush -> resident in x2/x3/x4/... pre-fix.
		ADDU (reg::t4, reg::t5, reg::t6),         // first-free host (x2 pre-fix)
		ADDU (reg::t7, reg::t0, reg::t1),         // (x3 pre-fix)
		ADDU (reg::t8, reg::t2, reg::t3),
		ADDU (reg::a2, reg::s2, reg::s3),
		ADDU (reg::a3, reg::s4, reg::s5),
		DADDU(reg::t9, reg::s2, reg::s4),
		VADD_C2(0xF, /*fd*/7, /*fs*/5, /*ft*/6),  // op2: no flush; w2/w3 scribbled
		// Read the band back so a clobbered resident member propagates (the
		// block-end flush would surface it too, but explicit reads make the
		// residency dependency load-bearing).
		DADDU(reg::s6, reg::t4, reg::t7),
		DADDU(reg::s7, reg::t8, reg::a2),
		OR   (reg::v1, reg::a3, reg::t9),         // v1 is pinned; still auto-diffed
	});
	h.Run();

	// The exhaustive gate is the Run() auto-diff (every EE + VU0 field
	// JIT==interp). Spot-check the two prime clobber targets (first two ADDU
	// dests -> the lowest-numbered free pool hosts) and a derived read.
	EXPECT_EQ(h.GetGpr64Jit(reg::t4), h.GetGpr64Interp(reg::t4));
	EXPECT_EQ(h.GetGpr64Jit(reg::t7), h.GetGpr64Interp(reg::t7));
	EXPECT_EQ(h.GetGpr64Jit(reg::s6), h.GetGpr64Interp(reg::s6));
	EXPECT_EQ(h.GetGpr64Interp(reg::t4),
	          0x0000000012340000ull + 0x5678ull); // sext32(t5 + t6)
}
