// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// MMI allocator-coherence regression tests.
//
// The MMI ops routed through eeRecompileCodeXMM (PAND/POR/PXOR/PADDx/etc.)
// keep Rd's full 128 bits live in a NEON Q-reg owned by the EE allocator.
// Anything that subsequently reads the value must either:
//
//   - go through the allocator (so it sees the live NEON copy), or
//   - explicitly flush+invalidate the NEON slot before a direct-memory
//     read (`mmiFlushReg`-style — _deleteEEreg(reg, 1)).
//
// These tests exercise the boundary in both directions: converted-MMI → X
// where X is an unconverted MMI op (PCPYLD/UD/H, PSLLW, PMTHI, PMTLO), a
// 64-bit scalar GPR op (DADDU, OR), a 128-bit store (SQ), and another
// converted MMI op (allocator reuse, MODE_WRITE-only reuse, three-deep
// chains).
//
// Run() diffs JIT vs interp post-state; ExpectGpr128 additionally pins the
// expected architectural value. A divergence here is the JIT failing to
// pick up the in-NEON value (stale memory) or failing to write back all
// 128 bits (zeroed upper-half).

#include "harness/EeRecTestHarness.h"
#include "harness/RecompilerTestEnvironment.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {

constexpr u64 kALo = 0x1111111111111111ull;
constexpr u64 kAHi = 0x2222222222222222ull;
constexpr u64 kBLo = 0x3333333333333333ull;
constexpr u64 kBHi = 0x4444444444444444ull;

}  // namespace

// ============================================================================
//  Converted MMI → unconverted MMI op reading Rd via direct memory
// ============================================================================

// PAND writes v0 through the allocator (Rd in NEON); PCPYUD then reads v0
// for both Rs and Rt. PCPYUD's path is mmiFlushReg(Rs/Rt) + mmiLoadReg(memory) —
// if mmiFlushReg fails to write back the NEON copy, PCPYUD reads stale memory.
TEST(EeRecMmiCoherence, PandThenPcpyudReadsLiveAllocatorValue)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, kALo, kAHi);
	h.SetMmiPair(reg::a1, kBLo, kBHi);
	// Pre-seed v0 to a known-wrong sentinel so a stale-memory read shows up
	// as the sentinel value instead of the PAND result.
	h.SetMmiPair(reg::v0, 0xDEADBEEFDEADBEEFull, 0xCAFEBABECAFEBABEull);
	h.LoadProgram({
		ee::PAND(reg::v0, reg::a0, reg::a1),
		ee::PCPYUD(reg::v1, reg::v0, reg::v0),
	});
	h.Run();
	// PAND result: lo = a0.lo & a1.lo, hi = a0.hi & a1.hi.
	const u64 andLo = kALo & kBLo;
	const u64 andHi = kAHi & kBHi;
	h.ExpectMmiPair(reg::v0, andLo, andHi);
	// PCPYUD v1, v0, v0 → v1 = {v0.hi, v0.hi}.
	h.ExpectMmiPair(reg::v1, andHi, andHi);
}

// PAND writes v0; PCPYLD then assembles {v0.lo, v0.lo}. Same flush-coherence
// path but exercises the lower-half read.
TEST(EeRecMmiCoherence, PandThenPcpyldReadsLiveAllocatorValue)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, kALo, kAHi);
	h.SetMmiPair(reg::a1, kBLo, kBHi);
	h.SetMmiPair(reg::v0, 0xDEADBEEFDEADBEEFull, 0xCAFEBABECAFEBABEull);
	h.LoadProgram({
		ee::PAND(reg::v0, reg::a0, reg::a1),
		ee::PCPYLD(reg::v1, reg::v0, reg::v0),
	});
	h.Run();
	const u64 andLo = kALo & kBLo;
	const u64 andHi = kAHi & kBHi;
	h.ExpectMmiPair(reg::v0, andLo, andHi);
	// PCPYLD v1, rs, rt → {UD[0]=rt.lo, UD[1]=rs.lo}; rs = rt = v0 → both halves = v0.lo.
	h.ExpectMmiPair(reg::v1, andLo, andLo);
}

// PAND writes v0; PCPYH then duplicates v0.UH[0] across the lower 4 halfwords
// and v0.UH[4] across the upper 4. Bit pattern is engineered so the stale
// pre-PAND sentinel and the live PAND value have visibly different lane[0]
// and lane[4] halfwords.
TEST(EeRecMmiCoherence, PandThenPcpyhReadsLiveAllocatorValue)
{
	EeRecTestHarness h;
	// a0 lo = 0xAAAA in lane[0], a1 lo = 0x00FF → AND = 0x00AA in lane[0].
	// a0 hi has 0xBBBB in lane[4-of-128], a1 hi has 0x0F0F → AND = 0x0B0B.
	const u64 a0lo = 0x000000000000AAAAull;
	const u64 a1lo = 0x00000000000000FFull;
	const u64 a0hi = 0x000000000000BBBBull;
	const u64 a1hi = 0x0000000000000F0Full;
	h.SetMmiPair(reg::a0, a0lo, a0hi);
	h.SetMmiPair(reg::a1, a1lo, a1hi);
	h.SetMmiPair(reg::v0, 0xDEADBEEFDEADBEEFull, 0xCAFEBABECAFEBABEull);
	h.LoadProgram({
		ee::PAND(reg::v0, reg::a0, reg::a1),
		ee::PCPYH(reg::v1, reg::v0),
	});
	h.Run();
	h.ExpectMmiPair(reg::v0, a0lo & a1lo, a0hi & a1hi);
	// PCPYH: replicate v0.UH[0]=0x00AA into lower 4 lanes; v0.UH[4]=0x0B0B
	// into upper 4 lanes. (Lane[4] is the first halfword of the upper 64
	// bits, which is the low halfword of the upper UD = a0hi & a1hi.)
	h.ExpectMmiPair(reg::v1, 0x00AA00AA00AA00AAull, 0x0B0B0B0B0B0B0B0Bull);
}

// PAND writes v0; PSLLW shifts v0's 4 word lanes left by 4. PSLLW's path is
// mmiFlushReg(_Rt_)+mmiLoadReg → another direct-memory read that must see
// the live PAND result.
TEST(EeRecMmiCoherence, PandThenPsllwReadsLiveAllocatorValue)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0000000A0000000Bull, 0x0000000C0000000Dull);
	h.SetMmiPair(reg::a1, 0x00000000FFFFFFFFull, 0xFFFFFFFF00000000ull);
	h.SetMmiPair(reg::v0, 0xDEADBEEFDEADBEEFull, 0xCAFEBABECAFEBABEull);
	h.LoadProgram({
		ee::PAND(reg::v0, reg::a0, reg::a1),
		ee::PSLLW(reg::v1, reg::v0, 4),
	});
	h.Run();
	// PAND lane[0] = 0xB & 0xFFFFFFFF = 0xB; lane[1] = 0xA & 0 = 0;
	// lane[2] = 0xD & 0 = 0; lane[3] = 0xC & 0xFFFFFFFF = 0xC.
	const u64 andLo = 0x000000000000000Bull;
	const u64 andHi = 0x0000000C00000000ull;
	h.ExpectMmiPair(reg::v0, andLo, andHi);
	// PSLLW by 4 shifts each 32-bit lane: {0xB, 0x0, 0x0, 0xC} << 4
	// = {0xB0, 0x0, 0x0, 0xC0}.
	h.ExpectMmiPair(reg::v1, 0x00000000000000B0ull, 0x000000C000000000ull);
}

// PAND writes v0; PMTHI uses v0 as the source for the 128-bit HI register
// via mmiLoadReg(_Rs_). Check both the resulting HI (via PMFHI roundtrip)
// and the survival of v0.
TEST(EeRecMmiCoherence, PandThenPmthiReadsLiveAllocatorValue)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, kALo, kAHi);
	h.SetMmiPair(reg::a1, kBLo, kBHi);
	h.SetMmiPair(reg::v0, 0xDEADBEEFDEADBEEFull, 0xCAFEBABECAFEBABEull);
	h.SetHi64(0xAAAAAAAAAAAAAAAAull);
	h.LoadProgram({
		ee::PAND(reg::v0, reg::a0, reg::a1),
		ee::PMTHI(reg::v0),
		ee::PMFHI(reg::v1),
	});
	h.Run();
	const u64 andLo = kALo & kBLo;
	const u64 andHi = kAHi & kBHi;
	h.ExpectMmiPair(reg::v0, andLo, andHi);
	// HI = v0 = {andLo, andHi}; PMFHI v1 = HI.
	h.ExpectMmiPair(reg::v1, andLo, andHi);
}

// ============================================================================
//  Chained converted MMI ops — allocator reuse correctness
// ============================================================================

// Two converted MMI ops sharing Rd: PAND v0, a0, a1; POR v0, v0, a2.
// The allocator should keep v0 in the same Q-reg across both ops without a
// memory bounce. MODE_WRITE on the second op must NOT clobber the live
// MODE_READ data from the first.
TEST(EeRecMmiCoherence, ChainedPandThenPorSameDest)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xFFFFFFFF00000000ull, 0xFFFF0000FFFF0000ull);
	h.SetMmiPair(reg::a1, 0x00000000FFFFFFFFull, 0x0000FFFF0000FFFFull);
	h.SetMmiPair(reg::a2, 0x1111111111111111ull, 0x2222222222222222ull);
	h.LoadProgram({
		ee::PAND(reg::v0, reg::a0, reg::a1),
		ee::POR(reg::v0, reg::v0, reg::a2),
	});
	h.Run();
	// PAND: 0xFFFFFFFF00000000 & 0x00000000FFFFFFFF = 0; 0xFFFF0000FFFF0000
	// & 0x0000FFFF0000FFFF = 0.
	// POR v0, 0, a2 = a2.
	h.ExpectMmiPair(reg::v0, 0x1111111111111111ull, 0x2222222222222222ull);
}

// Three-deep chain: PAND, POR, PXOR all writing to the same destination
// register v0. Exercises repeated allocator reuse and ensures the running
// 128-bit value tracks correctly through three back-to-back updates.
TEST(EeRecMmiCoherence, ThreeDeepChainedMmiUpdatesSameDest)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x00FF00FF00FF00FFull, 0x00FF00FF00FF00FFull);
	h.SetMmiPair(reg::a1, 0x0F0F0F0F0F0F0F0Full, 0x0F0F0F0F0F0F0F0Full);
	h.SetMmiPair(reg::a2, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull);
	h.SetMmiPair(reg::a3, 0xAAAAAAAAAAAAAAAAull, 0x5555555555555555ull);
	h.LoadProgram({
		ee::PAND(reg::v0, reg::a0, reg::a1),  // v0 = 0x00FF & 0x0F0F = 0x000F (per byte: 0x0F0F0F0F)
		ee::POR (reg::v0, reg::v0, reg::a2),  // v0 |= 0xFF…FF → all FF
		ee::PXOR(reg::v0, reg::v0, reg::a3),  // v0 ^= a3
	});
	h.Run();
	const u64 expectedLo = 0xFFFFFFFFFFFFFFFFull ^ 0xAAAAAAAAAAAAAAAAull;
	const u64 expectedHi = 0xFFFFFFFFFFFFFFFFull ^ 0x5555555555555555ull;
	h.ExpectMmiPair(reg::v0, expectedLo, expectedHi);
}

// Source-reuse chain: PAND v0, a0, a1; POR v1, v0, a2. The second op
// reads v0 (live in NEON from the first op) and writes a different Rd.
TEST(EeRecMmiCoherence, ChainedPorReadsLivePandResult)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, kALo, kAHi);
	h.SetMmiPair(reg::a1, kBLo, kBHi);
	h.SetMmiPair(reg::a2, 0x0F0F0F0F0F0F0F0Full, 0xF0F0F0F0F0F0F0F0ull);
	h.SetMmiPair(reg::v0, 0xDEADBEEFDEADBEEFull, 0xCAFEBABECAFEBABEull);
	h.LoadProgram({
		ee::PAND(reg::v0, reg::a0, reg::a1),
		ee::POR (reg::v1, reg::v0, reg::a2),
	});
	h.Run();
	const u64 andLo = kALo & kBLo;
	const u64 andHi = kAHi & kBHi;
	h.ExpectMmiPair(reg::v0, andLo, andHi);
	h.ExpectMmiPair(reg::v1, andLo | 0x0F0F0F0F0F0F0F0Full, andHi | 0xF0F0F0F0F0F0F0F0ull);
}

// ============================================================================
//  Converted MMI → 128-bit store (SQ)
// ============================================================================

// PAND writes v0; SQ stores v0 to memory at sp+0. SQ must see the
// post-PAND value, not the seeded sentinel.
TEST(EeRecMmiCoherence, PandThenSqStoresLiveResult)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, kALo, kAHi);
	h.SetMmiPair(reg::a1, kBLo, kBHi);
	h.SetMmiPair(reg::v0, 0xDEADBEEFDEADBEEFull, 0xCAFEBABECAFEBABEull);
	const u32 kScratch = RecompilerTestEnvironment::kScratchAddr;
	h.SetGpr64(reg::sp, kScratch);
	h.TrackMemWindow(kScratch, 16);
	h.LoadProgram({
		ee::PAND(reg::v0, reg::a0, reg::a1),
		ee::SQ  (reg::v0, 0, reg::sp),
	});
	h.Run();
	const u64 andLo = kALo & kBLo;
	const u64 andHi = kAHi & kBHi;
	h.ExpectMmiPair(reg::v0, andLo, andHi);
	EXPECT_EQ(h.ReadU64(kScratch + 0), andLo) << "SQ stored stale .lo";
	EXPECT_EQ(h.ReadU64(kScratch + 8), andHi) << "SQ stored stale .hi";
}

// ============================================================================
//  Converted MMI → 64-bit scalar GPR read
// ============================================================================

// PAND writes v0 (full 128 bits live in NEON); DADDU reads v0's lower 64
// via _eeMoveGPRtoR. _eeMoveGPRtoR IS allocator-aware (checks NEON for
// MODE_READ slot), but the converted MMI uses MODE_WRITE-only, so the
// allocator check might miss it and fall through to stale memory.
TEST(EeRecMmiCoherence, PandThenDadduReadsLiveLowerHalf)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, kALo, kAHi);
	h.SetMmiPair(reg::a1, kBLo, kBHi);
	h.SetMmiPair(reg::v0, 0xDEADBEEFDEADBEEFull, 0xCAFEBABECAFEBABEull);
	h.LoadProgram({
		ee::PAND (reg::v0, reg::a0, reg::a1),
		ee::DADDU(reg::v1, reg::v0, reg::zero),
	});
	h.Run();
	const u64 andLo = kALo & kBLo;
	const u64 andHi = kAHi & kBHi;
	h.ExpectMmiPair(reg::v0, andLo, andHi);
	// DADDU v1, v0, zero copies v0's lower 64 bits to v1 (and sign-extends
	// or zero-fills the upper 64 — DADDU writes UD[0] only).
	EXPECT_EQ(h.GetGpr64Jit(reg::v1), andLo);
}

// ============================================================================
//  Coverage for converted MMI ops not in ee_rec_mmi_simd_tests.cpp.
//  PEXT*/PPAC*/PMAX*/PMIN*/PABS*/PADSBH/PINTH/PINTEH are part of the
//  eeRecompileCodeXMM conversion but missing from the existing test suite.
//  Test against the interpreter to verify each NEON emit matches PS2 semantics.
// ============================================================================

TEST(EeRecMmiCoherence, PextlwInterleavesLowerWords)
{
	// PS2: rd.UL[0]=rt.UL[0], rd.UL[1]=rs.UL[0], rd.UL[2]=rt.UL[1], rd.UL[3]=rs.UL[1]
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xAAAAAAAA11111111ull, 0xFFFFFFFFFFFFFFFFull); // rs.UL[0]=0x11.., rs.UL[1]=0xAA..
	h.SetMmiPair(reg::a1, 0xBBBBBBBB22222222ull, 0xFFFFFFFFFFFFFFFFull); // rt.UL[0]=0x22.., rt.UL[1]=0xBB..
	h.LoadProgram({ee::PEXTLW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// Expected: lane[0]=rt[0]=0x22222222, lane[1]=rs[0]=0x11111111,
	//           lane[2]=rt[1]=0xBBBBBBBB, lane[3]=rs[1]=0xAAAAAAAA
	h.ExpectMmiPair(reg::v0, 0x1111111122222222ull, 0xAAAAAAAABBBBBBBBull);
}

TEST(EeRecMmiCoherence, PextuwInterleavesUpperWords)
{
	// PS2: rd.UL[0]=rt.UL[2], rd.UL[1]=rs.UL[2], rd.UL[2]=rt.UL[3], rd.UL[3]=rs.UL[3]
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x1111111111111111ull, 0xAAAAAAAA33333333ull);
	h.SetMmiPair(reg::a1, 0x2222222222222222ull, 0xBBBBBBBB44444444ull);
	h.LoadProgram({ee::PEXTUW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// lane[0]=rt.UL[2]=0x44444444, lane[1]=rs.UL[2]=0x33333333,
	// lane[2]=rt.UL[3]=0xBBBBBBBB, lane[3]=rs.UL[3]=0xAAAAAAAA
	h.ExpectMmiPair(reg::v0, 0x3333333344444444ull, 0xAAAAAAAABBBBBBBBull);
}

TEST(EeRecMmiCoherence, PpacwPacksLowerWordsOfEachHalf)
{
	// PS2: rd.UL[0]=rt.UL[0], rd.UL[1]=rt.UL[2], rd.UL[2]=rs.UL[0], rd.UL[3]=rs.UL[2]
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xFFFFFFFF11111111ull, 0xFFFFFFFF22222222ull);
	h.SetMmiPair(reg::a1, 0xFFFFFFFF33333333ull, 0xFFFFFFFF44444444ull);
	h.LoadProgram({ee::PPACW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x4444444433333333ull, 0x2222222211111111ull);
}

TEST(EeRecMmiCoherence, PmaxwReturnsLanewiseSignedMax)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x00000005FFFFFFFEull, 0x80000000FFFFFFFFull); // {-2, 5, -1, INT_MIN}
	h.SetMmiPair(reg::a1, 0x00000003FFFFFFF6ull, 0x000000017FFFFFFFull); // {-10, 3, INT_MAX, 1}
	h.LoadProgram({ee::PMAXW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// Per-lane signed max: {-2,5}vs{-10,3} → {-2,5}; {INT_MIN,-1}vs{INT_MAX,1} → {INT_MAX, 1}
	h.ExpectMmiPair(reg::v0, 0x00000005FFFFFFFEull, 0x000000017FFFFFFFull);
}

TEST(EeRecMmiCoherence, PminhReturnsLanewiseSignedMin)
{
	EeRecTestHarness h;
	// 8 halfword lanes; small mixed positives/negatives
	h.SetMmiPair(reg::a0, 0x0001FFFF00020003ull, 0x80007FFFFFFE0004ull);
	h.SetMmiPair(reg::a1, 0xFFFE00020001FFF0ull, 0x000180000003FFFFull);
	h.LoadProgram({ee::PMINH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// Per-lane signed min — recompute from the inputs above.
	auto laneMin = [](u16 x, u16 y) -> u16 { return ((s16)x < (s16)y) ? x : y; };
	auto pack = [&](u64 a, u64 b) {
		u64 r = 0;
		for (int i = 0; i < 4; ++i)
		{
			u16 x = (u16)(a >> (i*16));
			u16 y = (u16)(b >> (i*16));
			r |= (u64)laneMin(x, y) << (i*16);
		}
		return r;
	};
	h.ExpectMmiPair(reg::v0,
		pack(0x0001FFFF00020003ull, 0xFFFE00020001FFF0ull),
		pack(0x80007FFFFFFE0004ull, 0x000180000003FFFFull));
}

TEST(EeRecMmiCoherence, PabswAbsoluteValueOfSignedWords)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a1, 0xFFFFFFFB80000000ull, 0x000000077FFFFFFFull); // {INT_MIN, -5, INT_MAX, 7}
	h.LoadProgram({ee::PABSW(reg::v0, reg::a1)});  // PABSW v0, rt=a1
	h.Run();
	// PABSW PS2 spec: saturates INT_MIN → INT_MAX (NOT 0x80000000).
	h.ExpectMmiPair(reg::v0, 0x000000057FFFFFFFull, 0x000000077FFFFFFFull);
}

TEST(EeRecMmiCoherence, PadsbhSubsLowerHalfAddsUpperHalf)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x00100020003000A0ull, 0x0080007000600050ull);
	h.SetMmiPair(reg::a1, 0x0001000200030004ull, 0x0008000700060005ull);
	h.LoadProgram({ee::PADSBH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// Lower 4 halfwords: rs - rt; upper 4 halfwords: rs + rt
	h.ExpectMmiPair(reg::v0,
		0x000F001E002D009Cull,        // {0x10-1, 0x20-2, 0x30-3, 0xA0-4}
		0x0088007700660055ull);       // {0x80+8, 0x70+7, 0x60+6, 0x50+5}
}

// ============================================================================
//  Three-way allocator pressure — exercise q8-q15 saturation.
// ============================================================================

// Six MMI ops in flight, each writing a different destination, sharing
// source registers. Should drive the allocator's eviction path on q8-q15.
TEST(EeRecMmiCoherence, SixMmiOpsExerciseAllocatorEviction)
{
	EeRecTestHarness h;
	const u64 base = 0x0011223344556677ull;
	for (u32 r = 4; r < 12; ++r)
		h.SetMmiPair(r, base + r, base + r + 0x100);
	h.LoadProgram({
		ee::PAND(12,  4,  5),
		ee::POR (13,  6,  7),
		ee::PXOR(14,  8,  9),
		ee::PADDW(15, 10, 11),
		ee::PSUBW(16,  4,  6),
		ee::PADDH(17,  5,  7),
	});
	h.Run();
	h.ExpectMmiPair(12, (base + 4) & (base + 5), (base + 0x104) & (base + 0x105));
	h.ExpectMmiPair(13, (base + 6) | (base + 7), (base + 0x106) | (base + 0x107));
	h.ExpectMmiPair(14, (base + 8) ^ (base + 9), (base + 0x108) ^ (base + 0x109));
}

// ============================================================================
//  Operand-aliasing regressions — Rd == Rs / Rt with multi-emit MMI ops.
//
//  When Rd aliases Rs (or Rt), the allocator returns the SAME Q-reg for qd
//  and qs, so writing qd first clobbers qs for any subsequent emit step.
//  PADSBH / PINTH / PINTEH (multi-instruction emits referencing qs/qt after
//  the qd write) are the canonical at-risk shapes — they stage *intermediate*
//  halves in scratch registers but still keep qs/qt live across the sequence.
// ============================================================================

// PADSBH with Rd == Rs: the emit computes the low-half difference into qd
// (which clobbers qs when qd == qs), then needs the original qs again for the
// upper-half sum — so a non-alias-safe sequence would read a corrupted source.
// Expected behavior: rd.UH[0..3] = orig_rs.UH[0..3] - rt.UH[0..3];
//                    rd.UH[4..7] = orig_rs.UH[4..7] + rt.UH[4..7].
TEST(EeRecMmiCoherence, PadsbhRdAliasesRs)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x00100020003000A0ull, 0x0080007000600050ull);
	h.SetMmiPair(reg::a1, 0x0001000200030004ull, 0x0008000700060005ull);
	// Rd = Rs = a0; this is the aliasing case.
	h.LoadProgram({ee::PADSBH(reg::a0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::a0,
		0x000F001E002D009Cull,        // {0x10-1, 0x20-2, 0x30-3, 0xA0-4} (sub of original a0 - a1)
		0x0088007700660055ull);       // {0x80+8, 0x70+7, 0x60+6, 0x50+5} (add of original a0 + a1)
}

// The symmetric Rd == Rt aliasing case: qt (rather than qs) shares the
// destination Q-reg, so the emit must not corrupt rt before its last use.
TEST(EeRecMmiCoherence, PadsbhRdAliasesRt)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x00100020003000A0ull, 0x0080007000600050ull);
	h.SetMmiPair(reg::a1, 0x0001000200030004ull, 0x0008000700060005ull);
	h.LoadProgram({ee::PADSBH(reg::a1, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::a1,
		0x000F001E002D009Cull,
		0x0088007700660055ull);
}

// PINTH with Rd == Rs: a multi-step cross-lane interleave emit.
// PS2 PINTH: rd = interleave(rt.UH[0..3], rs.UH[4..7]).
TEST(EeRecMmiCoherence, PinthRdAliasesRs)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0011002200330044ull, 0x0055006600770088ull);
	h.SetMmiPair(reg::a1, 0xAAAABBBBCCCCDDDDull, 0xEEEEFFFF11112222ull);
	h.LoadProgram({ee::PINTH(reg::a0, reg::a0, reg::a1)});
	h.Run();
	// PS2 interp: rd.US[0]=rt.US[0], rd.US[1]=rs.US[4],
	//             rd.US[2]=rt.US[1], rd.US[3]=rs.US[5],
	//             rd.US[4]=rt.US[2], rd.US[5]=rs.US[6],
	//             rd.US[6]=rt.US[3], rd.US[7]=rs.US[7].
	// rs lower halfwords {0x44, 0x33, 0x22, 0x11};
	//    upper halfwords {0x88, 0x77, 0x66, 0x55}
	// rt lower {0xDDDD, 0xCCCC, 0xBBBB, 0xAAAA};
	//    upper {0x2222, 0x1111, 0xFFFF, 0xEEEE}
	// Interleave rt.low with rs.upper:
	//   {rt[0]=0xDDDD, rs[4]=0x88, rt[1]=0xCCCC, rs[5]=0x77,
	//    rt[2]=0xBBBB, rs[6]=0x66, rt[3]=0xAAAA, rs[7]=0x55}
	h.ExpectMmiPair(reg::a0,
		0x0077CCCC0088DDDDull,
		0x0055AAAA0066BBBBull);
}

// PINTEH with Rd == Rs: a multi-step even-lane deinterleave-then-interleave emit.
TEST(EeRecMmiCoherence, PintehRdAliasesRs)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0011002200330044ull, 0x0055006600770088ull);
	h.SetMmiPair(reg::a1, 0xAAAABBBBCCCCDDDDull, 0xEEEEFFFF11112222ull);
	h.LoadProgram({ee::PINTEH(reg::a0, reg::a0, reg::a1)});
	h.Run();
	// PS2 PINTEH: rd = interleave even halfwords of rs and rt.
	// rs evens (UH[0], UH[2], UH[4], UH[6]) = {0x44, 0x22, 0x88, 0x66}
	// rt evens                              = {0xDDDD, 0xBBBB, 0x2222, 0xFFFF}
	// rd = {rt_e[0]=0xDDDD, rs_e[0]=0x44, rt_e[1]=0xBBBB, rs_e[1]=0x22,
	//       rt_e[2]=0x2222, rs_e[2]=0x88, rt_e[3]=0xFFFF, rs_e[3]=0x66}
	h.ExpectMmiPair(reg::a0,
		0x0022BBBB0044DDDDull,
		0x0066FFFF00882222ull);
}

// Mix MMI with scalar reg pressure — alternating MMI and 64-bit ALU ops
// to confuse the NEON-vs-scalar-GPR allocator hand-off.
TEST(EeRecMmiCoherence, MmiInterleavedWith64BitAlu)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0000000000000005ull, 0x00000000000000A0ull);
	h.SetMmiPair(reg::a1, 0x0000000000000003ull, 0x0000000000000050ull);
	h.SetGpr64(reg::a2, 100);
	h.SetGpr64(reg::a3, 7);
	h.LoadProgram({
		ee::PAND (reg::v0, reg::a0, reg::a1),       // v0 = {5&3, 0xA0&0x50} = {1, 0}
		ee::DADDU(reg::v1, reg::a2, reg::a3),        // v1 = 100 + 7 = 107
		ee::POR  (reg::t0, reg::v0, reg::a0),       // t0 = v0 | a0 = {5, 0xA0}
		ee::DSUBU(reg::t1, reg::v1, reg::a3),        // t1 = 107 - 7 = 100
		ee::PXOR (reg::t2, reg::t0, reg::a1),       // t2 = t0 ^ a1 = {5^3, 0xA0^0x50} = {6, 0xF0}
	});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x0000000000000001ull, 0x0000000000000000ull);
	EXPECT_EQ(h.GetGpr64Jit(reg::v1), 107u);
	h.ExpectMmiPair(reg::t0, 0x0000000000000005ull, 0x00000000000000A0ull);
	EXPECT_EQ(h.GetGpr64Jit(reg::t1), 100u);
	h.ExpectMmiPair(reg::t2, 0x0000000000000006ull, 0x00000000000000F0ull);
}

// ============================================================================
//  Upper-64 preservation when a scalar op writes only UD[0] of a 128-bit-live
//  register.
//
//  MMI ops keep Rd live in a NEON slot with MODE_WRITE; the slot's 128 bits
//  are authoritative over memory. Scalar ops that target the same register
//  (LUI, MFLO, MOVZ, ADDIU, ...) call `_deleteEEreg(reg, 0)` which drops the
//  NEON slot WITHOUT writing it back. The 32/64-bit scalar then writes only
//  UD[0]. UD[1] of the slot — which the interpreter unambiguously preserves —
//  is silently lost.
//
//  This surfaces as visual artifacts when vertex/color-pack data loses its
//  upper 64 between a pack/unpack MMI and a subsequent scalar update of the
//  packed result.
// ============================================================================

// PADDW writes v3 (lower + upper both nonzero). LUI v3 then sets UD[0] only —
// UD[1] must remain the PADDW upper-64 result. Reading v3 via SQ (which goes
// through `mmiFlushReg`/`_deleteEEreg(reg,1)`) reveals whether the upper 64
// was preserved.
TEST(EeRecMmiCoherence, PaddwThenLuiPreservesUpper64)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0000000100000002ull, 0x0000000300000004ull); // {1,2,3,4}
	h.SetMmiPair(reg::a1, 0x0000000500000006ull, 0x0000000700000008ull); // {5,6,7,8}
	h.LoadProgram({
		ee::PADDW(reg::v0, reg::a0, reg::a1),       // v0 = {6,8,10,12}
		LUI      (reg::v0, 0x1234),                  // v0.UD[0] = 0x12340000; v0.UD[1] preserved
	});
	h.Run();
	// Interpreter LUI semantics: UD[0] = sign-extend((u32)imm << 16) = 0x12340000.
	// PADDW result UL[2]=4+8=0xC, UL[3]=3+7=0xA → UD[1] = (0xA<<32)|0xC = 0x0A_0000000C.
	h.ExpectMmiPair(reg::v0, 0x0000000012340000ull, 0x0000000A0000000Cull);
}

// MFLO writes UD[0] only.
TEST(EeRecMmiCoherence, PaddwThenMfloPreservesUpper64)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0000000100000002ull, 0x0000000300000004ull);
	h.SetMmiPair(reg::a1, 0x0000000500000006ull, 0x0000000700000008ull);
	h.SetLo64(0xDEADBEEFCAFEBABEull);
	h.LoadProgram({
		ee::PADDW(reg::v0, reg::a0, reg::a1),       // v0 = {6,8,10,12}
		MFLO     (reg::v0),                          // v0.UD[0] = LO; UD[1] preserved
	});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xDEADBEEFCAFEBABEull, 0x0000000A0000000Cull);
}

// MOVZ writes UD[0] only when the condition fires.
TEST(EeRecMmiCoherence, PaddwThenMovzPreservesUpper64)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0000000100000002ull, 0x0000000300000004ull);
	h.SetMmiPair(reg::a1, 0x0000000500000006ull, 0x0000000700000008ull);
	h.SetGpr64(reg::a2, 0xAAAAAAAA55555555ull);     // value to move
	h.SetGpr64(reg::a3, 0);                          // condition: zero → MOVZ fires
	h.LoadProgram({
		ee::PADDW(reg::v0, reg::a0, reg::a1),       // v0 = {6,8,10,12}
		ee::MOVZ (reg::v0, reg::a2, reg::a3),       // v0.UD[0] = a2; UD[1] preserved
	});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xAAAAAAAA55555555ull, 0x0000000A0000000Cull);
}

// ADDIU writes UD[0] (sign-extend 32-bit add to 64-bit). UD[1] preserved.
TEST(EeRecMmiCoherence, PaddwThenAddiuPreservesUpper64)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0000000100000002ull, 0x0000000300000004ull);
	h.SetMmiPair(reg::a1, 0x0000000500000006ull, 0x0000000700000008ull);
	h.LoadProgram({
		ee::PADDW(reg::v0, reg::a0, reg::a1),       // v0 = {6,8,10,12}; UD[0]=0x00000008_00000006
		ADDIU    (reg::v0, reg::v0, 1),              // v0.UD[0] = sign-extend((s32)UL[0] + 1)
	});
	h.Run();
	// PADDW UL[0] = a0.UL[0] (=0x2) + a1.UL[0] (=0x6) = 8. ADDIU sign-extends (8+1)=9 to UD[0].
	// UD[1] preserved = (PADDW UL[3]=0xA, UL[2]=0xC) → 0x0A_0000000C.
	h.ExpectMmiPair(reg::v0, 0x0000000000000009ull, 0x0000000A0000000Cull);
}

// Pack/unpack on alloc path produces a 128-bit result; scalar op then writes
// UD[0] only. This is the archetypal pattern: PEXTLW assembles a packed
// vertex word, then a scalar instruction tweaks the lower half.
TEST(EeRecMmiCoherence, PextlwThenLuiPreservesUpper64)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xAAAAAAAA11111111ull, 0xCCCCCCCC33333333ull);
	h.SetMmiPair(reg::a1, 0xBBBBBBBB22222222ull, 0xDDDDDDDD44444444ull);
	h.LoadProgram({
		ee::PEXTLW(reg::v0, reg::a0, reg::a1),      // v0 = {0x22,0x11,0xBB,0xAA}
		LUI       (reg::v0, 0x1234),                 // v0.UD[0] = 0x12340000; UD[1] preserved
	});
	h.Run();
	// PEXTLW result UD[1] = {0xBBBBBBBB, 0xAAAAAAAA} = 0xAAAAAAAABBBBBBBB
	h.ExpectMmiPair(reg::v0, 0x0000000012340000ull, 0xAAAAAAAABBBBBBBBull);
}

// ============================================================================
//  Pack/unpack-specific patterns: r0 source idioms, aliasing, chains.
//  Artifacts surface when pack/unpack ops are on the alloc path; lane-wise
//  alloc-path ops don't trigger. These tests target what's STRUCTURALLY
//  different about pack/unpack (cross-lane Zip/Uzp + r0-zero-extend idiom).
// ============================================================================

// PEXTLW with _Rs_ == r0 — common zero-extend-lower-words idiom.
// The JIT allocates r0 as a NEON slot. Verify the result still matches interp.
TEST(EeRecMmiCoherence, PextlwWithRsZeroZeroExtends)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a1, 0x1111111122222222ull, 0xCCCCCCCCDDDDDDDDull);
	h.LoadProgram({ee::PEXTLW(reg::v0, reg::zero, reg::a1)});
	h.Run();
	// PS2: rd.UL[0]=rt.UL[0]=0x22222222; rd.UL[1]=rs.UL[0]=0
	//      rd.UL[2]=rt.UL[1]=0x11111111; rd.UL[3]=rs.UL[1]=0
	h.ExpectMmiPair(reg::v0, 0x0000000022222222ull, 0x0000000011111111ull);
}

// PEXTLB with _Rs_ == r0 — zero-extend lower bytes to halfwords.
TEST(EeRecMmiCoherence, PextlbWithRsZeroZeroExtends)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a1, 0x0102030405060708ull, 0x090A0B0C0D0E0F10ull);
	h.LoadProgram({ee::PEXTLB(reg::v0, reg::zero, reg::a1)});
	h.Run();
	// PS2 PEXTLB: rd.UC[2i]=rt.UC[i], rd.UC[2i+1]=rs.UC[i] for i in 0..7.
	// With rs=0: each rt byte becomes the LOW byte of a halfword (high=0).
	// rt UD[0]=0x0102030405060708 ⇒ rt.UC[0..7] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01}.
	// rd bytes UC[0..15] = {0x08, 0, 0x07, 0, 0x06, 0, 0x05, 0, 0x04, 0, 0x03, 0, 0x02, 0, 0x01, 0}.
	// As halfwords (lo→hi): {0x0008, 0x0007, 0x0006, 0x0005, 0x0004, 0x0003, 0x0002, 0x0001}.
	h.ExpectMmiPair(reg::v0, 0x0005000600070008ull, 0x0001000200030004ull);
}

// PEXTLW with _Rd_ == _Rs_ aliasing — allocator gives same Q-reg for qs and qd.
// The interleave is well-defined when destination aliases source, but verify.
TEST(EeRecMmiCoherence, PextlwRdAliasesRs)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xAAAAAAAA11111111ull, 0xCCCCCCCC33333333ull);
	h.SetMmiPair(reg::a1, 0xBBBBBBBB22222222ull, 0xDDDDDDDD44444444ull);
	h.LoadProgram({ee::PEXTLW(reg::a0, reg::a0, reg::a1)});  // _Rd_=_Rs_=a0
	h.Run();
	h.ExpectMmiPair(reg::a0, 0x1111111122222222ull, 0xAAAAAAAABBBBBBBBull);
}

// PEXTLW with _Rd_ == _Rt_ aliasing.
TEST(EeRecMmiCoherence, PextlwRdAliasesRt)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xAAAAAAAA11111111ull, 0xCCCCCCCC33333333ull);
	h.SetMmiPair(reg::a1, 0xBBBBBBBB22222222ull, 0xDDDDDDDD44444444ull);
	h.LoadProgram({ee::PEXTLW(reg::a1, reg::a0, reg::a1)});  // _Rd_=_Rt_=a1
	h.Run();
	h.ExpectMmiPair(reg::a1, 0x1111111122222222ull, 0xAAAAAAAABBBBBBBBull);
}

// PEXTLW with _Rs_ == _Rt_ (same register twice).
TEST(EeRecMmiCoherence, PextlwSameSource)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xAAAAAAAA11111111ull, 0xCCCCCCCC33333333ull);
	h.LoadProgram({ee::PEXTLW(reg::v0, reg::a0, reg::a0)});  // Rs == Rt
	h.Run();
	// Each lower word of a0 interleaved with itself: {a0[0], a0[0], a0[1], a0[1]}
	h.ExpectMmiPair(reg::v0, 0x1111111111111111ull, 0xAAAAAAAAAAAAAAAAull);
}

// Vertex-unpack-style chain: 4 PEXTL ops with r0 building progressive zero-extension.
TEST(EeRecMmiCoherence, ChainedPextlZeroExtendsByteToWord)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xFFFFFFFFFFFFFFFFull, 0x0102030405060708ull);
	// Use upper half of a0 (low bytes interpretation: 0x08,0x07,...,0x01)
	h.LoadProgram({
		ee::PEXTUB(reg::v0, reg::zero, reg::a0),  // 0-extend upper bytes of a0 to halfwords in v0
		ee::PEXTLH(reg::v1, reg::zero, reg::v0),  // 0-extend lower halfwords of v0 to words in v1
	});
	h.Run();
	// PEXTUB(v0, rs=0, rt=a0): interleave the upper 8 bytes of a0 with 0 →
	// halfwords {0x0008,0x0007,0x0006,0x0005, 0x0004,0x0003,0x0002,0x0001}.
	h.ExpectMmiPair(reg::v0, 0x0005000600070008ull, 0x0001000200030004ull);
	// PEXTLH(v1, rs=0, rt=v0): zero-extend the lower 4 halfwords of v0 to words
	// → {0x00000008,0x00000007,0x00000006,0x00000005}.
	h.ExpectMmiPair(reg::v1, 0x0000000700000008ull, 0x0000000500000006ull);
}

// Long chain: many pack/unpack ops with allocator reuse + scalar interleaving.
// If there's any latent bug in slot reuse across pack/unpack chains, this
// flushes it out. Mimics real vertex-unpacking blocks.
TEST(EeRecMmiCoherence, LongPackUnpackChainWithScalarMixin)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x1111111122222222ull, 0x3333333344444444ull);
	h.SetMmiPair(reg::a1, 0x5555555566666666ull, 0x7777777788888888ull);
	h.SetMmiPair(reg::a2, 0x99999999AAAAAAAAull, 0xBBBBBBBBCCCCCCCCull);
	h.SetMmiPair(reg::a3, 0xDDDDDDDDEEEEEEEEull, 0xFFFFFFFF00000000ull);
	h.SetGpr64(reg::t0, 1);
	h.SetGpr64(reg::t1, 2);
	h.LoadProgram({
		ee::PEXTLW(reg::v0, reg::a0, reg::a1),        // pack lower words
		ee::PEXTUW(reg::v1, reg::a2, reg::a3),        // pack upper words
		ee::PPACW (reg::s0, reg::v0, reg::v1),        // compress
		ee::PEXTLH(reg::s1, reg::v0, reg::v1),        // halfword interleave
		ee::PEXTLB(reg::s2, reg::a0, reg::a2),        // byte interleave
		ee::PEXTUB(reg::s3, reg::a1, reg::a3),        // byte interleave upper
		ee::PPACH (reg::s4, reg::s2, reg::s3),        // halfword pack
		ee::PPACB (reg::s5, reg::s4, reg::s1),        // byte pack
		ADDIU     (reg::t2, reg::t0, 100),             // scalar interleave
		ee::PEXTLW(reg::s6, reg::s5, reg::s4),        // continue pack chain
		ee::DADDU (reg::t3, reg::t1, reg::t2),        // scalar
		ee::PEXTUW(reg::s7, reg::s6, reg::s0),        // final pack
	});
	h.Run();
	// h.Run() diffs JIT vs interp automatically; any divergence on any
	// register reads as a failure here. No ExpectMmiPair needed.
}

// Same chain but with self-aliasing every other op.
TEST(EeRecMmiCoherence, PackUnpackChainWithSelfAlias)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xDEADBEEFCAFEBABEull, 0x0123456789ABCDEFull);
	h.SetMmiPair(reg::a1, 0xFEEDFACEC0FFEE42ull, 0xFEDCBA9876543210ull);
	h.SetMmiPair(reg::a2, 0xAAAA5555AAAA5555ull, 0x5555AAAA5555AAAAull);
	h.LoadProgram({
		ee::PEXTLW(reg::v0, reg::a0, reg::a1),
		ee::PEXTUW(reg::v0, reg::v0, reg::a2),    // self-alias _Rd_==_Rs_
		ee::PPACW (reg::v0, reg::a0, reg::v0),    // self-alias _Rd_==_Rt_
		ee::PEXTLH(reg::v0, reg::v0, reg::v0),    // _Rs_==_Rt_==_Rd_
		ee::PPACB (reg::v0, reg::v0, reg::a1),    // self-alias again
	});
	h.Run();
}

// Pressure test: 16 pack/unpack ops writing 8 different destinations to
// force allocator eviction with pack/unpack still pending.
TEST(EeRecMmiCoherence, PackUnpackAllocatorEvictionPressure)
{
	EeRecTestHarness h;
	for (u32 r = 4; r < 12; ++r)
		h.SetMmiPair(r, 0x1100000000000000ull + r, 0x2200000000000000ull + r);
	// 16 ops: writes to r12..r19; reads from r4..r11
	h.LoadProgram({
		ee::PEXTLW(12,  4,  5),
		ee::PEXTUW(13,  6,  7),
		ee::PPACW (14,  8,  9),
		ee::PPACH (15, 10, 11),
		ee::PEXTLB(16,  4,  6),
		ee::PEXTUB(17,  5,  7),
		ee::PPACB (18,  8, 10),
		ee::PEXTLH(19,  9, 11),
		ee::PEXTLW(20, 12, 13),  // reads outputs from above
		ee::PEXTUW(21, 14, 15),
		ee::PPACW (22, 16, 17),
		ee::PPACH (23, 18, 19),
		ee::PEXTLB(24, 20, 21),
		ee::PEXTUB(25, 22, 23),
		ee::PPACB (26, 24, 25),
		ee::PEXTLH(27, 26, 20),
	});
	h.Run();
}

// =====================================================================================================
//  MODE_WRITE-only NEON slot must be authoritative for _eeMoveGPRtoR
//
//  An MMI op via eeRecompileCodeXMM allocates Rd's NEON slot with MODE_WRITE
//  only (no MODE_READ flag — XMMINFO_READD is not set for MMI). _eeMoveGPRtoR
//  must check both MODE_READ and MODE_WRITE when searching for a live NEON
//  slot, or it falls through to a stale cpuRegs.GPR.r[reg].UD[0]
//  load. Every EE load/store via recComputeAddr / recPrepStoreValue funnels
//  through _eeMoveGPRtoR — so if the base or store-value register was last
//  touched by an MMI op, the address computation must use the live NEON value.
//
//  Shape: PEXTLW writes $t0 (NEON MODE_WRITE-only). The very next SW uses $t0
//  as base. The correct behavior is for SW to use the post-PEXTLW $t0 value,
//  not the value held in memory before the PEXTLW.
// =====================================================================================================
TEST(EeRecMmiCoherence, PextlwDestThenStoreUsesLiveBase)
{
	constexpr u32 kStaleAddr = RecompilerTestEnvironment::kScratchAddr + 0x100;
	constexpr u32 kLiveAddr  = RecompilerTestEnvironment::kScratchAddr + 0x200;

	EeRecTestHarness h;
	// Pre-populate sentinels at both candidate addresses so we can tell which one got the SW.
	h.WriteU32(kStaleAddr, 0x11111111u);
	h.WriteU32(kLiveAddr,  0x22222222u);
	h.TrackMemWindow(kStaleAddr, 4);
	h.TrackMemWindow(kLiveAddr,  4);

	// $t0 starts pointing at kStaleAddr (the wrong address, if _eeMoveGPRtoR misses the live slot).
	h.SetGpr64(reg::t0, kStaleAddr);
	// PEXTLW Rd, Rs, Rt: rd.UL[0] = rt.UL[0]. So setting a1.UL[0] = kLiveAddr makes
	// the post-PEXTLW $t0 point at kLiveAddr.
	h.SetMmiPair(reg::a0,
		(0xDEAD0000ull << 32) | 0xCAFE0000ull,                  // UL[0]=0xCAFE0000, UL[1]=0xDEAD0000
		0xFEEDFACEFEEDFACEull);
	h.SetMmiPair(reg::a1,
		(0xBEEF0000ull << 32) | static_cast<u64>(kLiveAddr),    // UL[0]=kLiveAddr, UL[1]=0xBEEF0000
		0xFEEDC0DEFEEDC0DEull);
	h.SetGpr64(reg::a2, 0xCAFEBABEu);

	h.LoadProgram({
		ee::PEXTLW(reg::t0, reg::a0, reg::a1),  // $t0.UL[0] = a1.UL[0] = kLiveAddr
		SW        (reg::a2, 0, reg::t0),         // store $a2 at addr $t0+0 (should be kLiveAddr)
	});
	h.Run();  // diffs JIT vs interp.

	EXPECT_EQ(h.ReadU32(kLiveAddr),  0xCAFEBABEu);
	EXPECT_EQ(h.ReadU32(kStaleAddr), 0x11111111u);  // must be untouched
}

// Symmetric: PEXTLW writes the STORE-VALUE register (not the base).
// recPrepStoreValue also goes through _eeMoveGPRtoR.
TEST(EeRecMmiCoherence, PextlwDestThenStoreUsesLiveValue)
{
	constexpr u32 kAddr = RecompilerTestEnvironment::kScratchAddr + 0x300;

	EeRecTestHarness h;
	h.WriteU32(kAddr, 0xDEADDEADu);
	h.TrackMemWindow(kAddr, 4);

	// Pre-MMI value of $v0 (lower 32) is 0xFACEFACE — this is the "stale" sentinel.
	h.SetGpr64(reg::v0, 0xFACEFACEu);
	h.SetGpr64(reg::a0, kAddr);
	// a1.UL[0] = 0xCAFEBABE → after PEXTLW, $v0.UL[0] = 0xCAFEBABE.
	h.SetMmiPair(reg::a1,
		(0xBEEF0000ull << 32) | 0xCAFEBABEull,
		0x0000000000000000ull);
	h.SetMmiPair(reg::a2, 0x00000000DEAD0000ull, 0x0000000000000000ull);

	h.LoadProgram({
		ee::PEXTLW(reg::v0, reg::a2, reg::a1),  // $v0.UL[0] = 0xCAFEBABE
		SW        (reg::v0, 0, reg::a0),         // store $v0 at $a0 (= kAddr)
	});
	h.Run();

	// SW writes the live (post-PEXTLW) $v0.UL[0] = 0xCAFEBABE.
	EXPECT_EQ(h.ReadU32(kAddr), 0xCAFEBABEu);
}

// Symmetric: PEXTLW writes the BASE register, LW reads from it (load side).
TEST(EeRecMmiCoherence, PextlwDestThenLoadUsesLiveBase)
{
	constexpr u32 kStaleAddr = RecompilerTestEnvironment::kScratchAddr + 0x400;
	constexpr u32 kLiveAddr  = RecompilerTestEnvironment::kScratchAddr + 0x500;

	EeRecTestHarness h;
	h.WriteU32(kStaleAddr, 0x77777777u);  // wrong base: stale value
	h.WriteU32(kLiveAddr,  0x88888888u);  // correct base: live PEXTLW result
	h.SetGpr64(reg::t0, kStaleAddr);
	h.SetMmiPair(reg::a0, 0x0ull, 0x0ull);
	h.SetMmiPair(reg::a1,
		(0xBEEF0000ull << 32) | static_cast<u64>(kLiveAddr),
		0x0ull);

	h.LoadProgram({
		ee::PEXTLW(reg::t0, reg::a0, reg::a1),  // $t0.UL[0] = kLiveAddr
		LW        (reg::v0, 0, reg::t0),         // $v0 = mem32[$t0]
	});
	h.Run();

	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFFFFFF88888888ull);  // sign-extended
}

// Symmetric: PPACW (different cross-lane shuffle) — same _eeMoveGPRtoR path.
TEST(EeRecMmiCoherence, PpacwDestThenStoreUsesLiveBase)
{
	constexpr u32 kStaleAddr = RecompilerTestEnvironment::kScratchAddr + 0x600;
	constexpr u32 kLiveAddr  = RecompilerTestEnvironment::kScratchAddr + 0x700;

	EeRecTestHarness h;
	h.WriteU32(kStaleAddr, 0xAAAAAAAAu);
	h.WriteU32(kLiveAddr,  0xBBBBBBBBu);
	h.TrackMemWindow(kStaleAddr, 4);
	h.TrackMemWindow(kLiveAddr,  4);

	h.SetGpr64(reg::t0, kStaleAddr);
	// PPACW: rd.UL[0] = rt.UL[0]. Set a1.UL[0] = kLiveAddr.
	h.SetMmiPair(reg::a0, 0x0ull, 0x0ull);
	h.SetMmiPair(reg::a1,
		(0xBEEF0000ull << 32) | static_cast<u64>(kLiveAddr),
		0x0ull);
	h.SetGpr64(reg::a2, 0x55555555u);

	h.LoadProgram({
		ee::PPACW(reg::t0, reg::a0, reg::a1),   // $t0.UL[0] = a1.UL[0] = kLiveAddr
		SW       (reg::a2, 0, reg::t0),
	});
	h.Run();

	EXPECT_EQ(h.ReadU32(kLiveAddr),  0x55555555u);
	EXPECT_EQ(h.ReadU32(kStaleAddr), 0xAAAAAAAAu);
}

// Lane-wise MMI op for symmetry: should ALSO produce a live base. Confirms the
// fix covers lane-wise ops, not just pack/unpack; the staleness is in the
// allocator/load path and applies to all MODE_WRITE-only MMI slots.
TEST(EeRecMmiCoherence, PaddwDestThenStoreUsesLiveBase)
{
	constexpr u32 kStaleAddr = RecompilerTestEnvironment::kScratchAddr + 0x800;
	constexpr u32 kLiveAddr  = RecompilerTestEnvironment::kScratchAddr + 0x900;

	EeRecTestHarness h;
	h.WriteU32(kStaleAddr, 0xCCCCCCCCu);
	h.WriteU32(kLiveAddr,  0xDDDDDDDDu);
	h.TrackMemWindow(kStaleAddr, 4);
	h.TrackMemWindow(kLiveAddr,  4);

	h.SetGpr64(reg::t0, kStaleAddr);
	// PADDW: rd.UL[i] = rs.UL[i] + rt.UL[i] (lane-wise). Set a0+a1 lane 0 = kLiveAddr.
	h.SetMmiPair(reg::a0, static_cast<u64>(kLiveAddr - 0x1000), 0x0ull);
	h.SetMmiPair(reg::a1, 0x1000ull, 0x0ull);
	h.SetGpr64(reg::a2, 0x66666666u);

	h.LoadProgram({
		ee::PADDW(reg::t0, reg::a0, reg::a1),   // $t0.UL[0] = (kLiveAddr - 0x1000) + 0x1000 = kLiveAddr
		SW       (reg::a2, 0, reg::t0),
	});
	h.Run();

	EXPECT_EQ(h.ReadU32(kLiveAddr),  0x66666666u);
	EXPECT_EQ(h.ReadU32(kStaleAddr), 0xCCCCCCCCu);
}


// Symmetric: PPACW then scalar write.
TEST(EeRecMmiCoherence, PpacwThenAddiuPreservesUpper64)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xFFFFFFFF11111111ull, 0xFFFFFFFF22222222ull);
	h.SetMmiPair(reg::a1, 0xFFFFFFFF33333333ull, 0xFFFFFFFF44444444ull);
	h.LoadProgram({
		ee::PPACW(reg::v0, reg::a0, reg::a1),       // v0 = {0x44,0x33,0x22,0x11} lo→hi
		ADDIU    (reg::v0, reg::v0, 0),              // v0.UD[0] = sign-extend(v0.UL[0]+0); UD[1] preserved
	});
	h.Run();
	// PPACW: rd.UL[0]=rt.UL[0]=0x33333333, rd.UL[1]=rt.UL[2]=0x44444444 (sign-extended via ADDIU's
	// 32-bit add over UL[0]=0x33333333 → 0x33333333 sign-extended to UD[0]).
	// UD[1] preserved = {rs.UL[0]=0x11111111, rs.UL[2]=0x22222222} = 0x22222222_11111111
	h.ExpectMmiPair(reg::v0, 0x0000000033333333ull, 0x2222222211111111ull);
}
