// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// SL-13 — COP2 clamp-constant broadcast residency (q25/q26).
//
// The macro-mode FMAC clamp bounds (±FLT_MAX) live register-resident in
// q25/q26, lazily re-materialized per block from the pinned s8/s9 scalars
// (2 Dups, zero memory) instead of being reloaded from
// _cpuRegistersPack.cop2Rec at every clamp site. These are contract tests
// for the compile-time validity discipline (cop2EnsureClampConsts,
// iCOP2-arm64.cpp):
//
//   - one establishment per straight-line clamp chain;
//   - re-establishment after any real C-call seam (iFlushCall);
//   - the VPU_STAT sync stubs re-materialize on their taken path, so
//     validity may ride through sync seams;
//   - q25/q26 are excluded from the EE NEON allocator pool and from the
//     COP2 macro-mode mVU pool (nothing in EE-block emission can clobber
//     them).
//
// The end-to-end value tests double as JIT-vs-interp oracles: an overflow
// FMAC must produce exactly ±FLT_MAX (0x7f7fffff), which fails if a clamp
// site ever sees garbage bounds. The emission-level pins (establishment
// counter, stub byte-scan, pool probes) catch policy regressions
// deterministically — the value tests alone would only fail when the C
// path happens to clobber q25/q26.

#include "harness/EeRecTestHarness.h"

#include "VU.h"
#include "VUmicro.h"
#include "Config.h"

#include <gtest/gtest.h>

// SL-13 test hooks (PCSX2_RECOMPILER_TESTS builds). Global scope — defined
// outside namespaces in the arm64 sources.
extern u32 g_cop2ClampConstEstablishCount; // iCOP2-arm64.cpp
int cop2TestGetSyncStubCount();            // iCOP2-arm64.cpp
const u8* cop2TestGetSyncStub(int kind);   // iCOP2-arm64.cpp
bool mVUTestProbe_NeonPoolUsable(int hostreg, bool cop2mode); // microVU-arm64.cpp
bool eeTestNeonRegIsReserved(int hostreg); // iCore-arm64.cpp

namespace recompiler_tests {

using namespace mips;
using namespace mips::ee;
using namespace vu;

namespace {

constexpr u32 mask_xyzw = 0xF;

// FLT_MAX per lane; ~2^114 per lane. Product overflows to +inf, which the
// mandatory PS2 result clamp turns into exactly +FLT_MAX (0x7f7fffff).
constexpr u32 kFltMaxBits = 0x7f7fffffu;
constexpr u32 kBigBits = 0x78000000u;

void SeedOverflowOperands(EeRecTestHarness& h, u32 fs, u32 ft)
{
	h.SeedVu0VfBits(fs, kFltMaxBits, kFltMaxBits, kFltMaxBits, kFltMaxBits);
	h.SeedVu0VfBits(ft, kBigBits, kBigBits, kBigBits, kBigBits);
	// The overflow FMACs set STATUS/MAC O-flags; with no consumer in the
	// program the JIT's flag-liveness elision (EP-2a, vuFlagHack default)
	// legitimately skips the architectural VI writes the interp performs.
	// Flag behavior is pinned elsewhere — these tests pin clamp VALUES.
	h.IgnoreVu0Vi(REG_STATUS_FLAG);
	h.IgnoreVu0Vi(REG_MAC_FLAG);
}

void ExpectClampedToFltMax(EeRecTestHarness& h, u32 vf)
{
	for (char l : {'x', 'y', 'z', 'w'})
	{
		EXPECT_EQ(h.GetVu0VfBitsJit(vf, l), kFltMaxBits) << "lane " << l;
		EXPECT_EQ(h.GetVu0VfBitsJit(vf, l), h.GetVu0VfBitsInterp(vf, l)) << "lane " << l;
	}
}

// Micro at PC 0: 32 NOP pairs (outlasts the 16-cycle sync kickstart window)
// then E-bit. Idempotent — pure NOPs, so JIT/interp replay divergence is
// impossible. Keeps VPU_STAT busy across the first post-VCALLMS sync seam.
void SeedLongNopMicro(EeRecTestHarness& h)
{
	u32 off = 0;
	for (int i = 0; i < 32; i++, off += 8)
		h.SeedVu0Microprogram(off, {NopPair()});
	h.SeedVu0Microprogram(off, {
		EBitNopPair(),
		NopPair(), // explicit E-bit delay pair — keep micro mem deterministic
	});
}

} // namespace

// =========================================================================
//  Emission policy — establishment counts
// =========================================================================

// A straight-line chain of clamping FMACs pays ONE establishment (2 Dups)
// for the whole block; every clamp site is then a bare Fminnm+Fmaxnm.
TEST(EeVu0Cop2ClampResidency, EstablishOncePerStraightLineChain)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	SeedOverflowOperands(h, 1, 2);
	h.LoadProgram({
		VMUL_C2(mask_xyzw, 3, 1, 2),
		VMUL_C2(mask_xyzw, 4, 1, 2),
		VMUL_C2(mask_xyzw, 5, 1, 2),
	});
	const u32 before = g_cop2ClampConstEstablishCount;
	h.Run();
	EXPECT_EQ(g_cop2ClampConstEstablishCount - before, 1u)
		<< "3 clamping FMACs in one block must establish q25/q26 exactly once";
	ExpectClampedToFltMax(h, 3);
	ExpectClampedToFltMax(h, 4);
	ExpectClampedToFltMax(h, 5);
}

// VCALLMS is a real C-call seam (iFlushCall(FLUSH_INTERPRETER)) — validity
// must NOT ride through it; the next clamp site re-establishes.
TEST(EeVu0Cop2ClampResidency, ReestablishAfterCCallSeam)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(REG_VPU_STAT, 0);
	SeedOverflowOperands(h, 1, 2);
	// Trivial immediate-E micro: the VCALLMS is here purely as an in-block
	// C-call seam.
	h.SeedVu0Microprogram(0, {
		EBitNopPair(),
		NopPair(),
	});
	h.LoadProgram({
		VMUL_C2(mask_xyzw, 3, 1, 2),
		VCALLMS(0),
		VMUL_C2(mask_xyzw, 4, 1, 2),
	});
	const u32 before = g_cop2ClampConstEstablishCount;
	h.Run();
	EXPECT_EQ(g_cop2ClampConstEstablishCount - before, 2u)
		<< "the C-call seam must invalidate; the post-seam FMAC re-establishes";
	ExpectClampedToFltMax(h, 3);
	ExpectClampedToFltMax(h, 4);
}

// =========================================================================
//  Runtime — clamp correctness through a TAKEN sync seam
// =========================================================================

// VCALLMS kicks a >16-cycle micro, so the following FMAC's analysis-marked
// sync seam (EEINST_COP2_FINISH_VU0 → SyncFinish stub) is TAKEN at runtime:
// the stub's C path drains the micro (arbitrary caller-saved NEON traffic),
// and the FMAC's clamp must still produce exactly ±FLT_MAX afterwards.
TEST(EeVu0Cop2ClampResidency, ClampCorrectAfterTakenSyncSeam)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(REG_VPU_STAT, 0);
	SeedOverflowOperands(h, 1, 2);
	SeedLongNopMicro(h);
	h.LoadProgram({
		VMUL_C2(mask_xyzw, 3, 1, 2), // pre-kick clamp (establishes)
		VCALLMS(0),                  // kick — VPU_STAT busy
		VMUL_C2(mask_xyzw, 4, 1, 2), // sync seam TAKEN (drains micro), then clamp
		VMUL_C2(mask_xyzw, 5, 1, 2), // rides the post-seam establishment
	});
	h.Run();
	ExpectClampedToFltMax(h, 3);
	ExpectClampedToFltMax(h, 4);
	ExpectClampedToFltMax(h, 5);
}

// =========================================================================
//  Fork discipline — establishment state across branch arms
// =========================================================================

// The flag joins BranchCompileState: establishment before the branch is
// restored for the sibling fork, so neither arm re-establishes and both
// arms' clamps stay correct. Run both runtime paths.
TEST(EeVu0Cop2ClampResidency, ForkArmsShareEstablishment)
{
	for (const u64 t0 : {u64(0), u64(1)}) // not-taken / taken
	{
		EeRecTestHarness h;
		h.EnableVu0Capture();
		h.EnableCop1();
		h.SetGpr64(reg::t0, t0);
		SeedOverflowOperands(h, 1, 2);
		h.LoadProgram({
			VMUL_C2(mask_xyzw, 3, 1, 2),  // establishes before the fork
			BEQ(reg::t0, reg::zero, 2),   // fork
			VMUL_C2(mask_xyzw, 4, 1, 2),  // delay slot — clamps on both paths
			VMUL_C2(mask_xyzw, 5, 1, 2),  // fall-through arm
			VMUL_C2(mask_xyzw, 6, 1, 2),  // branch target
		});
		h.Run();
		ExpectClampedToFltMax(h, 3);
		ExpectClampedToFltMax(h, 4);
		ExpectClampedToFltMax(h, 6);
		if (t0 != 0)
			ExpectClampedToFltMax(h, 5); // fall-through executed too
	}
}

// =========================================================================
//  Emission pins — sync stubs and allocator pools
// =========================================================================

// Every shared VU0-sync stub must re-materialize q25/q26 on its taken path:
// scan each stub's emitted words for the exact Dup pair before the taken
// path's terminating Ret. Encodings verified against the ARM ARM:
//   dup v25.4s, v8.s[0] = 0x4E040519
//   dup v26.4s, v9.s[0] = 0x4E04053A
TEST(EeVu0Cop2ClampResidency, SyncStubsReDupClampConsts)
{
	// Any Run() (re)generates the dispatchers + stubs.
	EeRecTestHarness h;
	h.LoadProgram({NOP});
	h.Run();

	constexpr u32 kDupMax = 0x4E040519u;
	constexpr u32 kDupMin = 0x4E04053Au;
	constexpr u32 kRet = 0xD65F03C0u;

	ASSERT_GT(cop2TestGetSyncStubCount(), 0);
	for (int kind = 0; kind < cop2TestGetSyncStubCount(); kind++)
	{
		const u32* words = reinterpret_cast<const u32*>(cop2TestGetSyncStub(kind));
		ASSERT_NE(words, nullptr) << "stub " << kind;
		// Stub layout: fast-path Ret first, taken path ends in the 2nd Ret.
		bool sawDupMax = false, sawDupMin = false;
		int rets = 0;
		int i = 0;
		for (; i < 96 && rets < 2; i++)
		{
			if (words[i] == kDupMax)
				sawDupMax = true;
			else if (words[i] == kDupMin)
				sawDupMin = true;
			else if (words[i] == kRet)
				rets++;
		}
		EXPECT_EQ(rets, 2) << "stub " << kind << " shape drifted (scan window)";
		EXPECT_TRUE(sawDupMax) << "stub " << kind << " taken path lost the q25 re-Dup";
		EXPECT_TRUE(sawDupMin) << "stub " << kind << " taken path lost the q26 re-Dup";
	}
}

// q25/q26 are reserved out of the EE NEON allocator pool (like q8/q9); the
// rest of the pool is untouched.
TEST(EeVu0Cop2ClampResidency, EeAllocatorReservesClampRegs)
{
	for (int reserved : {8, 9, 25, 26})
		EXPECT_TRUE(eeTestNeonRegIsReserved(reserved)) << "q" << reserved;
	for (int usable : {0, 7, 10, 15, 16, 24, 27, 28})
		EXPECT_FALSE(eeTestNeonRegIsReserved(usable)) << "q" << usable;
}

// COP2 macro mode runs mVU emitters (the mVU-reuse wrappers) inline in EE
// blocks WITHOUT a C-call seam, and clamp validity deliberately rides
// through them — so the macro-mode mVU NEON pool must exclude q25/q26.
// Micro mode keeps the full pool.
TEST(EeVu0Cop2ClampResidency, MacroModeNeonPoolExcludesClampRegs)
{
	EXPECT_FALSE(mVUTestProbe_NeonPoolUsable(25, /*cop2mode*/ true));
	EXPECT_FALSE(mVUTestProbe_NeonPoolUsable(26, /*cop2mode*/ true));
	EXPECT_TRUE(mVUTestProbe_NeonPoolUsable(25, /*cop2mode*/ false));
	EXPECT_TRUE(mVUTestProbe_NeonPoolUsable(26, /*cop2mode*/ false));
	EXPECT_TRUE(mVUTestProbe_NeonPoolUsable(0, /*cop2mode*/ true));
	EXPECT_TRUE(mVUTestProbe_NeonPoolUsable(24, /*cop2mode*/ true));
	EXPECT_TRUE(mVUTestProbe_NeonPoolUsable(27, /*cop2mode*/ true));
}

// The mVU-reuse wrappers must not disturb validity: FMAC → VMFIR (mVU-reuse
// wrapper, no C call) → FMAC pays ONE establishment, and the post-wrapper
// clamp is still exact.
TEST(EeVu0Cop2ClampResidency, ValidityRidesThroughMvuReuseWrapper)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(5, 0x1234);
	SeedOverflowOperands(h, 1, 2);
	h.LoadProgram({
		VMUL_C2(mask_xyzw, 3, 1, 2),
		VMFIR_C2(mask_xyzw, /*ft*/ 7, /*is*/ 5), // mVU-reuse wrapper op
		VMUL_C2(mask_xyzw, 4, 1, 2),
	});
	const u32 before = g_cop2ClampConstEstablishCount;
	h.Run();
	EXPECT_EQ(g_cop2ClampConstEstablishCount - before, 1u)
		<< "the mVU-reuse wrapper must not invalidate (its pool excludes q25/q26)";
	ExpectClampedToFltMax(h, 3);
	ExpectClampedToFltMax(h, 4);
	for (char l : {'x', 'y', 'z', 'w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(7, l), h.GetVu0VfBitsInterp(7, l)) << "lane " << l;
}

} // namespace recompiler_tests
