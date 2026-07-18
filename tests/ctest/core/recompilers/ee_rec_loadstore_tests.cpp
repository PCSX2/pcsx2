// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Load/store tests for the 64-bit EE.

#include "harness/EeRecTestHarness.h"

#include "Config.h"
#include "vtlb.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kScratch = RecompilerTestEnvironment::kScratchAddr;
}

TEST(EeRecLoadStore, LwSignExtends)
{
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0x80000000u);
	h.SetGpr64(reg::a0, kScratch);
	h.LoadProgram({LW(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFFFFFF80000000ull);
}

TEST(EeRecLoadStore, LwuZeroExtends)
{
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0x80000000u);
	h.SetGpr64(reg::a0, kScratch);
	h.LoadProgram({ee::LWU(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000000080000000ull);
}

TEST(EeRecLoadStore, LdFull64Bit)
{
	EeRecTestHarness h;
	h.WriteU64(kScratch, 0x1122334455667788ull);
	h.SetGpr64(reg::a0, kScratch);
	h.LoadProgram({ee::LD(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x1122334455667788ull);
}

TEST(EeRecLoadStore, SdFull64BitRoundtrip)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::a1, 0xDEADBEEFCAFEBABEull);
	h.TrackMemWindow(kScratch, 8);
	h.LoadProgram({ee::SD(reg::a1, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.ReadU64(kScratch), 0xDEADBEEFCAFEBABEull);
}

TEST(EeRecLoadStore, LbSignExtends)
{
	EeRecTestHarness h;
	h.WriteU8(kScratch, 0xFF);
	h.SetGpr64(reg::a0, kScratch);
	h.LoadProgram({LB(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFFFFFFFFFFFFFFull);
}

TEST(EeRecLoadStore, LbuZeroExtends)
{
	EeRecTestHarness h;
	h.WriteU8(kScratch, 0xFF);
	h.SetGpr64(reg::a0, kScratch);
	h.LoadProgram({LBU(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFull);
}

// ===========================================================================
//  Unaligned word loads — LWL / LWR
//
//  LE MIPS-III semantics (R5900OpcodeImpl.cpp:656-717): each op reads the
//  full 32-bit word at (EA & ~3) and merges with rt's low 32 bits via a
//  shift+mask pair indexed by shift = EA & 3. LWL preserves the low (3-shift)
//  bytes of rt; LWR preserves the high `shift` bytes. The merged 32-bit
//  result is sign-extended into the 64-bit GPR — except LWR with shift==0
//  takes a special sign-extending path while shifts 1..3 only write the low
//  32 bits and preserve the upper 32 unchanged. Tests cover the four shifts
//  per op plus the canonical LWR+LWL pair for an unaligned word load.
//
//  Memory layout: mem32[kScratch+0] = 0x44332211 (LE bytes 11 22 33 44).
//  Sentinel rt: 0xDEADBEEFCAFEBABE so preservation behavior is visible.
// ===========================================================================

TEST(EeRecLoadStore, LwlShift0PartialLoad)
{
	// shift=0: result32 = (rt[low] & 0x00FFFFFF) | (mem << 24)
	//        = 0x00FEBABE | 0x11000000 = 0x11FEBABE; sign-extends positive.
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0x44332211u);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	h.LoadProgram({LWL(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000000011FEBABEull);
}

TEST(EeRecLoadStore, LwlShift1)
{
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0x44332211u);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	h.LoadProgram({LWL(reg::v0, 1, reg::a0)});
	h.Run();
	// (0xCAFEBABE & 0xFFFF) | (0x44332211 << 16) = 0xBABE | 0x22110000 = 0x2211BABE
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x000000002211BABEull);
}

TEST(EeRecLoadStore, LwlShift2)
{
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0x44332211u);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	h.LoadProgram({LWL(reg::v0, 2, reg::a0)});
	h.Run();
	// (0xCAFEBABE & 0xFF) | (0x44332211 << 8) = 0xBE | 0x33221100 = 0x332211BE
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x00000000332211BEull);
}

TEST(EeRecLoadStore, LwlShift3FullWordSignExtendsNegative)
{
	// shift=3: full word load. Use bit-31-set memory to verify the s32→s64
	// sign-extension path through SD[0].
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0xFFEEDDCCu);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	h.LoadProgram({LWL(reg::v0, 3, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFFFFFFFFEEDDCCull);
}

TEST(EeRecLoadStore, LwrShift0FullWordSignExtendsNegative)
{
	// LWR shift==0: special-case sign-extends the merged 32-bit value to
	// the full 64-bit destination. shift!=0 paths preserve the upper 32 bits
	// instead — distinct codegen on the JIT side.
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0xFFEEDDCCu);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	h.LoadProgram({LWR(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFFFFFFFFEEDDCCull);
}

TEST(EeRecLoadStore, LwrShift1PreservesUpper32)
{
	// shift=1: result32 = (rt[low] & 0xFF000000) | (mem >> 8). Upper 32
	// bits of rt MUST stay untouched (no sign-extend on shift!=0).
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0x44332211u);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	h.LoadProgram({LWR(reg::v0, 1, reg::a0)});
	h.Run();
	// (0xCAFEBABE & 0xFF000000) | (0x44332211 >> 8) = 0xCA000000 | 0x00443322
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xDEADBEEFCA443322ull);
}

TEST(EeRecLoadStore, LwrShift2)
{
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0x44332211u);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	h.LoadProgram({LWR(reg::v0, 2, reg::a0)});
	h.Run();
	// (0xCAFEBABE & 0xFFFF0000) | (0x44332211 >> 16) = 0xCAFE0000 | 0x4433
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xDEADBEEFCAFE4433ull);
}

TEST(EeRecLoadStore, LwrShift3SingleByte)
{
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0x44332211u);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	h.LoadProgram({LWR(reg::v0, 3, reg::a0)});
	h.Run();
	// (0xCAFEBABE & 0xFFFFFF00) | (0x44332211 >> 24) = 0xCAFEBA00 | 0x44
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xDEADBEEFCAFEBA44ull);
}

TEST(EeRecLoadStore, LwrLwlPairUnalignedWordLoad)
{
	// Canonical unaligned-word load via LWR+LWL. Memory bytes
	// [0..7] = 99 11 22 33 44 AA BB CC are arranged so that the unaligned
	// word at byte offset 1 = 0x44332211 (LE: 11 22 33 44).
	EeRecTestHarness h;
	h.WriteU32(kScratch + 0, 0x33221199u); // bytes 0..3: 99 11 22 33
	h.WriteU32(kScratch + 4, 0xCCBBAA44u); // bytes 4..7: 44 AA BB CC
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	h.LoadProgram({
		LWR(reg::v0, 1, reg::a0),
		LWL(reg::v0, 4, reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000000044332211ull);
}

// ===========================================================================
//  Unaligned word stores — SWL / SWR
//
//  SWL/SWR mirror LWL/LWR but write to memory. They read the existing
//  word at (EA & ~3), merge `shift` bytes of rt into it via shift+mask
//  (R5900OpcodeImpl.cpp:813-858), and write the merged word back.
// ===========================================================================

TEST(EeRecLoadStore, SwlShift0SingleByte)
{
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0xAABBCCDDu);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr(reg::a1, 0x12345678u);
	h.TrackMemWindow(kScratch, 4);
	h.LoadProgram({SWL(reg::a1, 0, reg::a0)});
	h.Run();
	// new = (rt >> 24) | (mem & 0xFFFFFF00) = 0x12 | 0xAABBCC00
	EXPECT_EQ(h.ReadU32(kScratch), 0xAABBCC12u);
}

TEST(EeRecLoadStore, SwlShift3FullWord)
{
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0xAABBCCDDu);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr(reg::a1, 0x12345678u);
	h.TrackMemWindow(kScratch, 4);
	h.LoadProgram({SWL(reg::a1, 3, reg::a0)});
	h.Run();
	EXPECT_EQ(h.ReadU32(kScratch), 0x12345678u);
}

TEST(EeRecLoadStore, SwrShift0FullWord)
{
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0xAABBCCDDu);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr(reg::a1, 0x12345678u);
	h.TrackMemWindow(kScratch, 4);
	h.LoadProgram({SWR(reg::a1, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.ReadU32(kScratch), 0x12345678u);
}

TEST(EeRecLoadStore, SwrShift3SingleByte)
{
	EeRecTestHarness h;
	h.WriteU32(kScratch, 0xAABBCCDDu);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr(reg::a1, 0x12345678u);
	h.TrackMemWindow(kScratch, 4);
	h.LoadProgram({SWR(reg::a1, 3, reg::a0)});
	h.Run();
	// new = (rt << 24) | (mem & 0x00FFFFFF) = 0x78000000 | 0x00BBCCDD
	EXPECT_EQ(h.ReadU32(kScratch), 0x78BBCCDDu);
}

TEST(EeRecLoadStore, SwrSwlPairUnalignedWordStore)
{
	// Canonical unaligned-word store via SWR+SWL at byte offset 1.
	// rt's 4 bytes (LE: 78 56 34 12) land in mem at addresses [1..4].
	EeRecTestHarness h;
	h.WriteU32(kScratch + 0, 0xAAAAAAAAu);
	h.WriteU32(kScratch + 4, 0xBBBBBBBBu);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr(reg::a1, 0x12345678u);
	h.TrackMemWindow(kScratch, 8);
	h.LoadProgram({
		SWR(reg::a1, 1, reg::a0),
		SWL(reg::a1, 4, reg::a0),
	});
	h.Run();
	// mem32[0]_new = (rt << 8) | (mem & 0xFF) = 0x34567800 | 0xAA = 0x345678AA
	// mem32[4]_new = (rt >> 24) | (mem & 0xFFFFFF00) = 0x12 | 0xBBBBBB00
	EXPECT_EQ(h.ReadU32(kScratch + 0), 0x345678AAu);
	EXPECT_EQ(h.ReadU32(kScratch + 4), 0xBBBBBB12u);
}

// ===========================================================================
//  Unaligned dword loads/stores — LDL / LDR / SDL / SDR
//
//  R5900-only (MIPS-III); 8-way shift indexed by EA & 7. Same shape as
//  LWL/LWR but operating on full 64-bit values, so no sign-extension
//  branching exists. (R5900OpcodeImpl.cpp:741-901.)
// ===========================================================================

TEST(EeRecLoadStore, LdrLdlPairUnalignedDwordLoad)
{
	// Bytes [0..15] = CC 11 22 33 44 55 66 77 88 DD EE FF 00 00 00 00.
	// mem64[0] = 0x77665544332211CC, mem64[8] = 0x00000000FFEEDD88.
	// Unaligned dword at byte 1 = bytes [1..8] = 0x8877665544332211.
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0x77665544332211CCull);
	h.WriteU64(kScratch + 8, 0x00000000FFEEDD88ull);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	h.LoadProgram({
		ee::LDR(reg::v0, 1, reg::a0),
		ee::LDL(reg::v0, 8, reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x8877665544332211ull);
}

TEST(EeRecLoadStore, SdrSdlPairUnalignedDwordStore)
{
	// Stores rt's 8 bytes (LE: F0 DE BC 9A 78 56 34 12) at byte addresses
	// [1..8]. mem64[0] preserves byte 0; mem64[8] preserves bytes [9..15].
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0xAAAAAAAAAAAAAAAAull);
	h.WriteU64(kScratch + 8, 0xBBBBBBBBBBBBBBBBull);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::a1, 0x123456789ABCDEF0ull);
	h.TrackMemWindow(kScratch, 16);
	h.LoadProgram({
		ee::SDR(reg::a1, 1, reg::a0),
		ee::SDL(reg::a1, 8, reg::a0),
	});
	h.Run();
	// mem64[0]_new = (rt << 8) | (mem & 0xFF) = 0x3456789ABCDEF000 | 0xAA
	// mem64[8]_new = (rt >> 56) | (mem & 0xFFFFFFFFFFFFFF00) = 0x12 | 0xBBBB..00
	EXPECT_EQ(h.ReadU64(kScratch + 0), 0x3456789ABCDEF0AAull);
	EXPECT_EQ(h.ReadU64(kScratch + 8), 0xBBBBBBBBBBBBBB12ull);
}

// ===========================================================================
//  Live-register preservation across the unaligned store/load.
//
//  recUnalignedStoreWord/Double + recUnalignedLoadDouble provide inline
//  fastmem read-modify-write codegen rather than an interpreter fallback.
//  The EE GPR allocator parks guest registers in host
//  NEON lanes starting at v0 (caller-saved per AAPCS), so a register that is
//  NOT the store's Rs/Rt but is live across the op must survive it. The inline
//  path preserves them two ways: on the fastmem fast path no call happens at all
//  (and the backpatch thunk spills the live-register masks if a fault fires);
//  on the softmem slow path the C call obeys AAPCS, and the op's own scratch
//  (aligned addr / shift / Rt) lives in callee-saved temps. Run()'s JIT-vs-
//  interp auto-diff over all 32 GPRs is the oracle.
//
//  CAVEAT — these remain behavioral smoke-pins, not red/green repros: in this
//  build the harness takes the fastmem path, where no live lane is ever at risk.
//  Kept as a guard against future allocation changes.
// ===========================================================================

TEST(EeRecLoadStore, SdlSdrPreservesLiveRegistersAcrossInterpCall)
{
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0xAAAAAAAAAAAAAAAAull);
	h.WriteU64(kScratch + 8, 0xBBBBBBBBBBBBBBBBull);
	h.WriteU32(kScratch + 16, 0x00001000u); // non-const seed for t0
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::a1, 0x123456789ABCDEF0ull);
	h.TrackMemWindow(kScratch, 16);
	h.LoadProgram({
		// t0 = mem32[seed]  → non-const, allocated in a caller-saved NEON lane.
		LW(reg::t0, 16, reg::a0),
		// Derive several more non-const live values from t0.
		ADDIU(reg::t1, reg::t0, 0x11),
		ADDIU(reg::t2, reg::t0, 0x22),
		ADDIU(reg::t3, reg::t0, 0x33),
		ADDIU(reg::s0, reg::t0, 0x44),
		ADDIU(reg::s1, reg::t0, 0x55),
		// Unaligned store: its interpreter C-call must NOT clobber t0..s1.
		ee::SDR(reg::a1, 1, reg::a0),
		ee::SDL(reg::a1, 8, reg::a0),
		// Read the live registers back after the call so they must survive it.
		ADDU(reg::v0, reg::t0, reg::t1),
		ADDU(reg::v0, reg::v0, reg::t2),
		ADDU(reg::v0, reg::v0, reg::t3),
		ADDU(reg::v0, reg::v0, reg::s0),
		ADDU(reg::v0, reg::v0, reg::s1),
	});
	h.Run();
	// Spec-lock the survivors (t0 = 0x1000; sum below) in addition to the
	// auto-diff, so a clobber that happens to match interp is still caught.
	h.ExpectGpr64(reg::t0, 0x1000ull);
	h.ExpectGpr64(reg::v0, 0x1000ull * 6 + (0x11 + 0x22 + 0x33 + 0x44 + 0x55));
}

TEST(EeRecLoadStore, LdlLdrPreservesLiveRegistersAcrossInterpCall)
{
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0x77665544332211CCull);
	h.WriteU64(kScratch + 8, 0x00000000FFEEDD88ull);
	h.WriteU32(kScratch + 16, 0x00002000u); // non-const seed for t0
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v1, 0xDEADBEEFCAFEBABEull); // LDL/LDR target (= Rt)
	h.LoadProgram({
		LW(reg::t0, 16, reg::a0),
		ADDIU(reg::t1, reg::t0, 0x11),
		ADDIU(reg::t2, reg::t0, 0x22),
		ADDIU(reg::t3, reg::t0, 0x33),
		ADDIU(reg::s0, reg::t0, 0x44),
		ADDIU(reg::s1, reg::t0, 0x55),
		// Unaligned load into v1; t0..s1 are live across its interpreter call.
		ee::LDR(reg::v1, 1, reg::a0),
		ee::LDL(reg::v1, 8, reg::a0),
		ADDU(reg::v0, reg::t0, reg::t1),
		ADDU(reg::v0, reg::v0, reg::t2),
		ADDU(reg::v0, reg::v0, reg::t3),
		ADDU(reg::v0, reg::v0, reg::s0),
		ADDU(reg::v0, reg::v0, reg::s1),
	});
	h.Run();
	h.ExpectGpr64(reg::t0, 0x2000ull);
	h.ExpectGpr64(reg::v0, 0x2000ull * 6 + (0x11 + 0x22 + 0x33 + 0x44 + 0x55));
	h.ExpectGpr64(reg::v1, 0x8877665544332211ull); // the unaligned-load result
}

// ===========================================================================
//  LDL/LDR pair fusion (iR5900-arm64.cpp g_eeUnalignedFused).
//
//  A same-Rt/Rs LDL+LDR pair whose offsets differ by 7 is exactly one unaligned
//  64-bit load at the lower (LDR) address, which ARM64 does in a single LDR x.
//  The leading half emits the fused load and sets g_eeUnalignedFused; the
//  trailing half consumes it and emits nothing. Either lead order occurs (PS2 is
//  little-endian — LDR-first is the common MIPSEL idiom). Run()'s JIT-vs-interp
//  auto-diff is the correctness oracle; g_eeUnalignedFuseCount proves the fused
//  path actually fired (so these are red on un-fused code, green once it lands).
// ===========================================================================

extern u32 g_eeUnalignedFuseCount;
// Fusion is gated off when kProgramPc is in the fastmem faulting set. That set is
// process-global and accumulates across suites (an earlier test that faulted at
// kProgramPc would suppress fusion here), so the positive tests clear it to make
// the fired-count assertion reflect this block's own intent. (Correctness is
// order-independent — the unfused fallback is also exact — so the negative tests
// need no clear.)
extern void vtlb_ClearLoadStoreInfo();

TEST(EeRecLoadStore, LdlLdrFusionLdrFirst)
{
	// Bytes [1..8] = 0x8877665544332211 (see LdrLdlPairUnalignedDwordLoad).
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0x77665544332211CCull);
	h.WriteU64(kScratch + 8, 0x00000000FFEEDD88ull);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	vtlb_ClearLoadStoreInfo();
	const u32 before = g_eeUnalignedFuseCount;
	h.LoadProgram({
		ee::LDR(reg::v0, 1, reg::a0),
		ee::LDL(reg::v0, 8, reg::a0),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0x8877665544332211ull);
	EXPECT_GT(g_eeUnalignedFuseCount, before) << "LDR+LDL pair should have fused";
}

TEST(EeRecLoadStore, LdlLdrFusionLdlFirst)
{
	// Same data/result, LDL-first (the GCC idiom) — must fuse identically.
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0x77665544332211CCull);
	h.WriteU64(kScratch + 8, 0x00000000FFEEDD88ull);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	vtlb_ClearLoadStoreInfo();
	const u32 before = g_eeUnalignedFuseCount;
	h.LoadProgram({
		ee::LDL(reg::v0, 8, reg::a0),
		ee::LDR(reg::v0, 1, reg::a0),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0x8877665544332211ull);
	EXPECT_GT(g_eeUnalignedFuseCount, before) << "LDL+LDR pair should have fused";
}

// ── Unaligned loads into PINNED guest regs, consumed in-block ────────────────
//
// Regression tests for the 2026-07-17 UYA corruption: the unaligned-load
// handlers allocated a scalar slot for Rt even when Rt is pinned (GE-M2 I1
// violation). The merge result then lived in the slot while the pin kept the
// PRE-load value — and pin-preferring readers (_eeGetGPRSourceReg, i.e. every
// resident-template ALU op) consumed the stale pin until the next seam
// reconciled it. The fusion tests above load into $v0 but never consume it
// in-block, so the tail flush hid the bug; the ALU consume here is what makes
// the stale window observable.

TEST(EeRecLoadStore, LdrIntoPinnedTier1ThenAluConsume)
{
	// Unfused single LDR (shift != 0) into pinned $v0, consumed by DADDU.
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0x77665544332211CCull);
	h.SetGpr64(reg::t3, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	vtlb_ClearLoadStoreInfo();
	h.LoadProgram({
		ee::LDR(reg::v0, 1, reg::t3),
		ee::DADDU(reg::t0, reg::v0, reg::zero),
	});
	h.Run();
	// LDR @1: Rt = (mem >> 8) | (Rt & (~0 << 56))
	h.ExpectGpr64(reg::v0, 0xDE77665544332211ull);
	h.ExpectGpr64(reg::t0, 0xDE77665544332211ull);
}

TEST(EeRecLoadStore, LdlLdrFusedIntoPinnedTier2ThenAluConsume)
{
	// Fused LDL/LDR pair into pinned tier-2 $s0, consumed by ADDU.
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0x77665544332211CCull);
	h.WriteU64(kScratch + 8, 0x00000000FFEEDD88ull);
	h.SetGpr64(reg::t3, kScratch);
	h.SetGpr64(reg::s0, 0x0123456789ABCDEFull);
	vtlb_ClearLoadStoreInfo();
	const u32 before = g_eeUnalignedFuseCount;
	h.LoadProgram({
		ee::LDL(reg::s0, 8, reg::t3),
		ee::LDR(reg::s0, 1, reg::t3),
		ee::DADDU(reg::t1, reg::s0, reg::zero),
	});
	h.Run();
	h.ExpectGpr64(reg::s0, 0x8877665544332211ull);
	h.ExpectGpr64(reg::t1, 0x8877665544332211ull);
	EXPECT_GT(g_eeUnalignedFuseCount, before) << "LDL+LDR pair should have fused";
}

TEST(EeRecLoadStore, LwlIntoPinnedThenAluConsume)
{
	// LWL (shift 1) into pinned $a0, consumed by ADDU.
	EeRecTestHarness h;
	h.WriteU32(kScratch + 0, 0x44332211u);
	h.SetGpr64(reg::t3, kScratch);
	h.SetGpr64(reg::a0, 0x00000000000000EEull);
	vtlb_ClearLoadStoreInfo();
	h.LoadProgram({
		LWL(reg::a0, 1, reg::t3),
		ADDU(reg::t2, reg::a0, reg::zero),
	});
	h.Run();
	// LWL @1: Rt = sext32((Rt & 0x00ffff) | (loaded << 16)) = 0x221100EE
	h.ExpectGpr64(reg::a0, 0x00000000221100EEull);
	h.ExpectGpr64(reg::t2, 0x00000000221100EEull);
}

TEST(EeRecLoadStore, LwrIntoPinnedPreservesUpper32ThenAluConsume)
{
	// LWR with shift != 0 preserves Rt[63:32] — the pinned home must keep its
	// upper half through the partial merge.
	EeRecTestHarness h;
	h.WriteU32(kScratch + 0, 0x44332211u);
	h.SetGpr64(reg::a1, 0xAABBCCDD00000000ull);
	h.SetGpr64(reg::t3, kScratch);
	vtlb_ClearLoadStoreInfo();
	h.LoadProgram({
		LWR(reg::a1, 1, reg::t3),
		ee::DADDU(reg::t2, reg::a1, reg::zero),
	});
	h.Run();
	// LWR @1: Rt[31:0] = (Rt & 0xff000000) | (loaded >> 8); upper 32 preserved.
	h.ExpectGpr64(reg::a1, 0xAABBCCDD00443322ull);
	h.ExpectGpr64(reg::t2, 0xAABBCCDD00443322ull);
}

TEST(EeRecLoadStore, LdlLdrNoFusionWhenRtMismatch)
{
	// Different Rt on the two halves: not a pair, must NOT fuse. Run()'s auto-diff
	// covers the (independent partial-load) values; we pin the fuse-count gate.
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0x77665544332211CCull);
	h.WriteU64(kScratch + 8, 0x00000000FFEEDD88ull);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	h.SetGpr64(reg::v1, 0x1111111111111111ull);
	const u32 before = g_eeUnalignedFuseCount;
	h.LoadProgram({
		ee::LDR(reg::v0, 1, reg::a0),
		ee::LDL(reg::v1, 8, reg::a0),
	});
	h.Run();
	EXPECT_EQ(g_eeUnalignedFuseCount, before) << "Rt mismatch must not fuse";
}

TEST(EeRecLoadStore, LdlLdrNoFusionWhenOffsetNotSeven)
{
	// Offsets differ by 8, not 7 — not a contiguous pair, must NOT fuse.
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0x77665544332211CCull);
	h.WriteU64(kScratch + 8, 0x00000000FFEEDD88ull);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
	const u32 before = g_eeUnalignedFuseCount;
	h.LoadProgram({
		ee::LDR(reg::v0, 1, reg::a0),
		ee::LDL(reg::v0, 9, reg::a0),
	});
	h.Run();
	EXPECT_EQ(g_eeUnalignedFuseCount, before) << "offset delta != 7 must not fuse";
}

// ===========================================================================
//  SDL/SDR pair fusion (recVTLB-arm64.cpp recUnalignedStoreDouble).
//
//  The store mirror of the LDL/LDR fusion: a same-Rt/Rs SDL+SDR pair whose
//  offsets differ by 7 is exactly one unaligned 64-bit store of Rt at the lower
//  (SDR) address — the textbook MIPS idiom (SDL at D+7, SDR at D) — which ARM64
//  does in a single STR x. The leading half emits the fused store and sets
//  g_eeUnalignedFused; the trailing half consumes it and emits nothing. Either
//  lead order occurs (PS2 is little-endian; SDR-first is the common idiom).
//  Run()'s JIT-vs-interp auto-diff over the tracked store window is the
//  correctness oracle; g_eeUnalignedFuseCount proves the fused path fired.
// ===========================================================================

TEST(EeRecLoadStore, SdlSdrFusionSdrFirst)
{
	// Same data/result as SdrSdlPairUnalignedDwordStore: store a1's 8 bytes at
	// byte address kScratch+1 in one fused unaligned 64-bit store.
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0xAAAAAAAAAAAAAAAAull);
	h.WriteU64(kScratch + 8, 0xBBBBBBBBBBBBBBBBull);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::a1, 0x123456789ABCDEF0ull);
	h.TrackMemWindow(kScratch, 16);
	vtlb_ClearLoadStoreInfo();
	const u32 before = g_eeUnalignedFuseCount;
	h.LoadProgram({
		ee::SDR(reg::a1, 1, reg::a0),
		ee::SDL(reg::a1, 8, reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.ReadU64(kScratch + 0), 0x3456789ABCDEF0AAull);
	EXPECT_EQ(h.ReadU64(kScratch + 8), 0xBBBBBBBBBBBBBB12ull);
	EXPECT_GT(g_eeUnalignedFuseCount, before) << "SDR+SDL pair should have fused";
}

TEST(EeRecLoadStore, SdlSdrFusionSdlFirst)
{
	// Same data/result, SDL-first — must fuse identically.
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0xAAAAAAAAAAAAAAAAull);
	h.WriteU64(kScratch + 8, 0xBBBBBBBBBBBBBBBBull);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::a1, 0x123456789ABCDEF0ull);
	h.TrackMemWindow(kScratch, 16);
	vtlb_ClearLoadStoreInfo();
	const u32 before = g_eeUnalignedFuseCount;
	h.LoadProgram({
		ee::SDL(reg::a1, 8, reg::a0),
		ee::SDR(reg::a1, 1, reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.ReadU64(kScratch + 0), 0x3456789ABCDEF0AAull);
	EXPECT_EQ(h.ReadU64(kScratch + 8), 0xBBBBBBBBBBBBBB12ull);
	EXPECT_GT(g_eeUnalignedFuseCount, before) << "SDL+SDR pair should have fused";
}

TEST(EeRecLoadStore, SdlSdrNoFusionWhenRtMismatch)
{
	// Different Rt on the two halves: not a pair, must NOT fuse. Run()'s auto-diff
	// over the tracked window covers the (independent partial-store) result.
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0xAAAAAAAAAAAAAAAAull);
	h.WriteU64(kScratch + 8, 0xBBBBBBBBBBBBBBBBull);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::a1, 0x123456789ABCDEF0ull);
	h.SetGpr64(reg::a2, 0x0FEDCBA987654321ull);
	h.TrackMemWindow(kScratch, 16);
	const u32 before = g_eeUnalignedFuseCount;
	h.LoadProgram({
		ee::SDR(reg::a1, 1, reg::a0),
		ee::SDL(reg::a2, 8, reg::a0),
	});
	h.Run();
	EXPECT_EQ(g_eeUnalignedFuseCount, before) << "Rt mismatch must not fuse";
}

TEST(EeRecLoadStore, SdlSdrNoFusionWhenOffsetNotSeven)
{
	// Offsets differ by 8, not 7 — not a contiguous pair, must NOT fuse.
	EeRecTestHarness h;
	h.WriteU64(kScratch + 0, 0xAAAAAAAAAAAAAAAAull);
	h.WriteU64(kScratch + 8, 0xBBBBBBBBBBBBBBBBull);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::a1, 0x123456789ABCDEF0ull);
	h.TrackMemWindow(kScratch, 16);
	const u32 before = g_eeUnalignedFuseCount;
	h.LoadProgram({
		ee::SDR(reg::a1, 1, reg::a0),
		ee::SDL(reg::a1, 9, reg::a0),
	});
	h.Run();
	EXPECT_EQ(g_eeUnalignedFuseCount, before) << "offset delta != 7 must not fuse";
}

// ===========================================================================
//  Exhaustive alignment sweeps.
//
//  The inline RMW codegen has a distinct shift/mask path per alignment plus a
//  degenerate-shift special case (word shift==3, dword s==0/s==7). These sweeps
//  hit every alignment 0..3 (word) / 0..7 (dword) for all six ops, with the
//  interpreter as the oracle: Run() auto-diffs JIT vs interp post-state,
//  including the tracked store-target memory window.
// ===========================================================================

TEST(EeRecLoadStore, SwlAllAlignments)
{
	for (s16 off = 0; off <= 3; off++)
	{
		SCOPED_TRACE(testing::Message() << "offset=" << off);
		EeRecTestHarness h;
		h.WriteU32(kScratch, 0x11223344u);
		h.SetGpr64(reg::a0, kScratch);
		h.SetGpr64(reg::a1, 0x99887766AABBCCDDull); // Rt.UL[0] = 0xAABBCCDD
		h.TrackMemWindow(kScratch, 4);
		h.LoadProgram({SWL(reg::a1, off, reg::a0)});
		h.Run();
	}
}

TEST(EeRecLoadStore, SwrAllAlignments)
{
	for (s16 off = 0; off <= 3; off++)
	{
		SCOPED_TRACE(testing::Message() << "offset=" << off);
		EeRecTestHarness h;
		h.WriteU32(kScratch, 0x11223344u);
		h.SetGpr64(reg::a0, kScratch);
		h.SetGpr64(reg::a1, 0x99887766AABBCCDDull);
		h.TrackMemWindow(kScratch, 4);
		h.LoadProgram({SWR(reg::a1, off, reg::a0)});
		h.Run();
	}
}

TEST(EeRecLoadStore, SdlAllAlignments)
{
	for (s16 off = 0; off <= 7; off++)
	{
		SCOPED_TRACE(testing::Message() << "offset=" << off);
		EeRecTestHarness h;
		h.WriteU64(kScratch, 0x1122334455667788ull);
		h.SetGpr64(reg::a0, kScratch);
		h.SetGpr64(reg::a1, 0x99AABBCCDDEEFF00ull);
		h.TrackMemWindow(kScratch, 8);
		h.LoadProgram({ee::SDL(reg::a1, off, reg::a0)});
		h.Run();
	}
}

TEST(EeRecLoadStore, SdrAllAlignments)
{
	for (s16 off = 0; off <= 7; off++)
	{
		SCOPED_TRACE(testing::Message() << "offset=" << off);
		EeRecTestHarness h;
		h.WriteU64(kScratch, 0x1122334455667788ull);
		h.SetGpr64(reg::a0, kScratch);
		h.SetGpr64(reg::a1, 0x99AABBCCDDEEFF00ull);
		h.TrackMemWindow(kScratch, 8);
		h.LoadProgram({ee::SDR(reg::a1, off, reg::a0)});
		h.Run();
	}
}

TEST(EeRecLoadStore, LdlAllAlignments)
{
	for (s16 off = 0; off <= 7; off++)
	{
		SCOPED_TRACE(testing::Message() << "offset=" << off);
		EeRecTestHarness h;
		h.WriteU64(kScratch, 0x1122334455667788ull);
		h.SetGpr64(reg::a0, kScratch);
		h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull); // pre-existing Rt (preserved bytes)
		h.LoadProgram({ee::LDL(reg::v0, off, reg::a0)});
		h.Run();
	}
}

TEST(EeRecLoadStore, LdrAllAlignments)
{
	for (s16 off = 0; off <= 7; off++)
	{
		SCOPED_TRACE(testing::Message() << "offset=" << off);
		EeRecTestHarness h;
		h.WriteU64(kScratch, 0x1122334455667788ull);
		h.SetGpr64(reg::a0, kScratch);
		h.SetGpr64(reg::v0, 0xDEADBEEFCAFEBABEull);
		h.LoadProgram({ee::LDR(reg::v0, off, reg::a0)});
		h.Run();
	}
}

// ===========================================================================
//  Multi-register unrolled unaligned 32-byte copy.
//
//  An unaligned memcpy unrolled 32 bytes/iter — four LDL/LDR load-pairs into
//  v0/a2/a3/t0, then four SDL/SDR store-pairs to (a0):
//
//      LDL v0,7(v1)  LDR v0,0(v1)    ; dword 0
//      LDL a2,15(v1) LDR a2,8(v1)    ; dword 1
//      LDL a3,23(v1) LDR a3,16(v1)   ; dword 2
//      LDL t0,31(v1) LDR t0,24(v1)   ; dword 3
//      SDL v0,7(a0)  SDR v0,0(a0)
//      SDL a2,15(a0) SDR a2,8(a0)
//      SDL a3,23(a0) SDR a3,16(a0)
//      SDL t0,31(a0) SDR t0,24(a0)
//
//  Single LDL/LDR/SDL/SDR pairs pass (above); this is the full-block shape with
//  four destination registers live across the loads-then-stores, swept over all
//  source/destination alignments. Run()'s JIT-vs-interp auto-diff (GPRs +
//  tracked dest memory) is the oracle; the per-byte checks also spec-lock the
//  copy so a both-wrong result is still caught.
// ===========================================================================
TEST(EeRecLoadStore, MultiRegUnalignedDwordCopyBlock)
{
	constexpr u32 kSrc = kScratch;
	constexpr u32 kDst = kScratch + 256;
	for (u32 sa = 0; sa < 8; ++sa)
	{
		for (u32 da = 0; da < 8; ++da)
		{
			SCOPED_TRACE(testing::Message() << "src_align=" << sa << " dst_align=" << da);
			EeRecTestHarness h;
			const u32 src = kSrc + sa;
			const u32 dst = kDst + da;
			// 40 distinct source bytes so any mis-shift is visible in the copy.
			for (u32 i = 0; i < 40; ++i)
				h.WriteU8(src + i, static_cast<u8>(0x10 + i));
			// Destination sentinel (bytes outside [0,32) must be preserved).
			for (u32 i = 0; i < 48; ++i)
				h.WriteU8(kDst + i, 0xA5);
			h.SetGpr64(reg::v1, src);
			h.SetGpr64(reg::a0, dst);
			h.TrackMemWindow(kDst, 48);
			h.LoadProgram({
				ee::LDL(reg::v0, 7,  reg::v1), ee::LDR(reg::v0, 0,  reg::v1),
				ee::LDL(reg::a2, 15, reg::v1), ee::LDR(reg::a2, 8,  reg::v1),
				ee::LDL(reg::a3, 23, reg::v1), ee::LDR(reg::a3, 16, reg::v1),
				ee::LDL(reg::t0, 31, reg::v1), ee::LDR(reg::t0, 24, reg::v1),
				ee::SDL(reg::v0, 7,  reg::a0), ee::SDR(reg::v0, 0,  reg::a0),
				ee::SDL(reg::a2, 15, reg::a0), ee::SDR(reg::a2, 8,  reg::a0),
				ee::SDL(reg::a3, 23, reg::a0), ee::SDR(reg::a3, 16, reg::a0),
				ee::SDL(reg::t0, 31, reg::a0), ee::SDR(reg::t0, 24, reg::a0),
			});
			h.Run(); // auto-diffs JIT vs interp (GPRs + tracked dest memory)
			for (u32 i = 0; i < 32; ++i)
				EXPECT_EQ(h.ReadU8(dst + i), static_cast<u8>(0x10 + i)) << "copied byte " << i;
		}
	}
}

// Word twin of the dword pressure test above: an unaligned 32-byte memcpy built
// from LWL/LWR (load) + SWL/SWR (store), eight words in flight at once so the Rt
// allocation for each LWL/LWR spills a guest reg under pressure. recUnalignedWord
// left the loaded word in w0 across that alloc; the spill (or Rt landing in x0)
// clobbered it, so the loaded bytes came out wrong — only ever visible under
// pressure (single-op LWL/LWR above stay clean). This is the FlatOut 2 boot hang:
// the property-table parser reads packed name pointers via LWL/LWR and got
// garbage. Mirror of the Black LDL/LDR fix in recUnalignedLoadDouble.
TEST(EeRecLoadStore, MultiRegUnalignedWordCopyBlock)
{
	constexpr u32 kSrc = kScratch;
	constexpr u32 kDst = kScratch + 256;
	for (u32 sa = 0; sa < 4; ++sa)
	{
		for (u32 da = 0; da < 4; ++da)
		{
			SCOPED_TRACE(testing::Message() << "src_align=" << sa << " dst_align=" << da);
			EeRecTestHarness h;
			const u32 src = kSrc + sa;
			const u32 dst = kDst + da;
			// 32 distinct source bytes so any mis-shift/clobber is visible.
			for (u32 i = 0; i < 32; ++i)
				h.WriteU8(src + i, static_cast<u8>(0x10 + i));
			for (u32 i = 0; i < 40; ++i)
				h.WriteU8(kDst + i, 0xA5); // sentinel; bytes outside [0,32) must survive
			h.SetGpr64(reg::v1, src);
			h.SetGpr64(reg::a0, dst);
			h.TrackMemWindow(kDst, 40);
			// Eight unaligned words loaded into eight regs (all live across each
			// other's LWL/LWR Rt alloc), then stored back — the register pressure
			// that exposes the w0 clobber.
			h.LoadProgram({
				LWL(reg::v0, 3,  reg::v1), LWR(reg::v0, 0,  reg::v1),
				LWL(reg::a2, 7,  reg::v1), LWR(reg::a2, 4,  reg::v1),
				LWL(reg::a3, 11, reg::v1), LWR(reg::a3, 8,  reg::v1),
				LWL(reg::t0, 15, reg::v1), LWR(reg::t0, 12, reg::v1),
				LWL(reg::t1, 19, reg::v1), LWR(reg::t1, 16, reg::v1),
				LWL(reg::t2, 23, reg::v1), LWR(reg::t2, 20, reg::v1),
				LWL(reg::t3, 27, reg::v1), LWR(reg::t3, 24, reg::v1),
				LWL(reg::t4, 31, reg::v1), LWR(reg::t4, 28, reg::v1),
				SWL(reg::v0, 3,  reg::a0), SWR(reg::v0, 0,  reg::a0),
				SWL(reg::a2, 7,  reg::a0), SWR(reg::a2, 4,  reg::a0),
				SWL(reg::a3, 11, reg::a0), SWR(reg::a3, 8,  reg::a0),
				SWL(reg::t0, 15, reg::a0), SWR(reg::t0, 12, reg::a0),
				SWL(reg::t1, 19, reg::a0), SWR(reg::t1, 16, reg::a0),
				SWL(reg::t2, 23, reg::a0), SWR(reg::t2, 20, reg::a0),
				SWL(reg::t3, 27, reg::a0), SWR(reg::t3, 24, reg::a0),
				SWL(reg::t4, 31, reg::a0), SWR(reg::t4, 28, reg::a0),
			});
			h.Run(); // auto-diffs JIT vs interp (GPRs + tracked dest memory)
			for (u32 i = 0; i < 32; ++i)
				EXPECT_EQ(h.ReadU8(dst + i), static_cast<u8>(0x10 + i)) << "copied byte " << i;
		}
	}
}

// ===========================================================================
//  EE-SRA 3 Arm B — softmem (fastmem-off) twins of the two pressure blocks.
//  Turning fastmem off does what the fastmem versions never can: it disables
//  LDL/LDR and SDL/SDR pair fusion (gated on CHECK_FASTMEM), so the NON-FUSED
//  recUnalignedLoadDouble / recUnalignedStoreDouble paths run — the ones Arm B
//  reworked from three callee-saved temps down to ONE by parking the full
//  effective address and recomputing the aligned address, s and shift8 after
//  the read (and re-deriving Rt per path). It also routes each access through
//  the inline softmem read/write sequence. Sweeping every source/destination
//  alignment under eight-registers-live pressure is the oracle: a wrong
//  recompute or a clobbered address surfaces as a mis-shifted or corrupt copy.
// ===========================================================================
TEST(EeRecLoadStore, MultiRegUnalignedDwordCopyBlockSoftmem)
{
	const bool old_fastmem = EmuConfig.Cpu.Recompiler.EnableFastmem;
	EmuConfig.Cpu.Recompiler.EnableFastmem = false;
	constexpr u32 kSrc = kScratch;
	constexpr u32 kDst = kScratch + 256;
	for (u32 sa = 0; sa < 8; ++sa)
	{
		for (u32 da = 0; da < 8; ++da)
		{
			SCOPED_TRACE(testing::Message() << "src_align=" << sa << " dst_align=" << da);
			EeRecTestHarness h;
			const u32 src = kSrc + sa;
			const u32 dst = kDst + da;
			for (u32 i = 0; i < 40; ++i)
				h.WriteU8(src + i, static_cast<u8>(0x10 + i));
			for (u32 i = 0; i < 48; ++i)
				h.WriteU8(kDst + i, 0xA5);
			h.SetGpr64(reg::v1, src);
			h.SetGpr64(reg::a0, dst);
			h.TrackMemWindow(kDst, 48);
			h.LoadProgram({
				ee::LDL(reg::v0, 7,  reg::v1), ee::LDR(reg::v0, 0,  reg::v1),
				ee::LDL(reg::a2, 15, reg::v1), ee::LDR(reg::a2, 8,  reg::v1),
				ee::LDL(reg::a3, 23, reg::v1), ee::LDR(reg::a3, 16, reg::v1),
				ee::LDL(reg::t0, 31, reg::v1), ee::LDR(reg::t0, 24, reg::v1),
				ee::SDL(reg::v0, 7,  reg::a0), ee::SDR(reg::v0, 0,  reg::a0),
				ee::SDL(reg::a2, 15, reg::a0), ee::SDR(reg::a2, 8,  reg::a0),
				ee::SDL(reg::a3, 23, reg::a0), ee::SDR(reg::a3, 16, reg::a0),
				ee::SDL(reg::t0, 31, reg::a0), ee::SDR(reg::t0, 24, reg::a0),
			});
			h.Run();
			for (u32 i = 0; i < 32; ++i)
				EXPECT_EQ(h.ReadU8(dst + i), static_cast<u8>(0x10 + i)) << "copied byte " << i;
		}
	}
	EmuConfig.Cpu.Recompiler.EnableFastmem = old_fastmem;
}

TEST(EeRecLoadStore, MultiRegUnalignedWordCopyBlockSoftmem)
{
	const bool old_fastmem = EmuConfig.Cpu.Recompiler.EnableFastmem;
	EmuConfig.Cpu.Recompiler.EnableFastmem = false;
	constexpr u32 kSrc = kScratch;
	constexpr u32 kDst = kScratch + 256;
	for (u32 sa = 0; sa < 4; ++sa)
	{
		for (u32 da = 0; da < 4; ++da)
		{
			SCOPED_TRACE(testing::Message() << "src_align=" << sa << " dst_align=" << da);
			EeRecTestHarness h;
			const u32 src = kSrc + sa;
			const u32 dst = kDst + da;
			for (u32 i = 0; i < 32; ++i)
				h.WriteU8(src + i, static_cast<u8>(0x10 + i));
			for (u32 i = 0; i < 40; ++i)
				h.WriteU8(kDst + i, 0xA5);
			h.SetGpr64(reg::v1, src);
			h.SetGpr64(reg::a0, dst);
			h.TrackMemWindow(kDst, 40);
			h.LoadProgram({
				LWL(reg::v0, 3,  reg::v1), LWR(reg::v0, 0,  reg::v1),
				LWL(reg::a2, 7,  reg::v1), LWR(reg::a2, 4,  reg::v1),
				LWL(reg::a3, 11, reg::v1), LWR(reg::a3, 8,  reg::v1),
				LWL(reg::t0, 15, reg::v1), LWR(reg::t0, 12, reg::v1),
				LWL(reg::t1, 19, reg::v1), LWR(reg::t1, 16, reg::v1),
				LWL(reg::t2, 23, reg::v1), LWR(reg::t2, 20, reg::v1),
				LWL(reg::t3, 27, reg::v1), LWR(reg::t3, 24, reg::v1),
				LWL(reg::t4, 31, reg::v1), LWR(reg::t4, 28, reg::v1),
				SWL(reg::v0, 3,  reg::a0), SWR(reg::v0, 0,  reg::a0),
				SWL(reg::a2, 7,  reg::a0), SWR(reg::a2, 4,  reg::a0),
				SWL(reg::a3, 11, reg::a0), SWR(reg::a3, 8,  reg::a0),
				SWL(reg::t0, 15, reg::a0), SWR(reg::t0, 12, reg::a0),
				SWL(reg::t1, 19, reg::a0), SWR(reg::t1, 16, reg::a0),
				SWL(reg::t2, 23, reg::a0), SWR(reg::t2, 20, reg::a0),
				SWL(reg::t3, 27, reg::a0), SWR(reg::t3, 24, reg::a0),
				SWL(reg::t4, 31, reg::a0), SWR(reg::t4, 28, reg::a0),
			});
			h.Run();
			for (u32 i = 0; i < 32; ++i)
				EXPECT_EQ(h.ReadU8(dst + i), static_cast<u8>(0x10 + i)) << "copied byte " << i;
		}
	}
	EmuConfig.Cpu.Recompiler.EnableFastmem = old_fastmem;
}

// ===========================================================================
//  GE-07: fastmem no longer evicts allocator state at L/S sites — live
//  register preservation across a GENUINE fastmem fault.
//
//  An MMIO access (INTC_STAT, 0x1000F000) through a runtime (non-const)
//  address takes the fastmem path, SIGSEGVs on the unmapped arena page, and
//  is backpatched to the RecStubs thunk, whose C call must save/restore the
//  live allocator registers per the masks recorded at emit time. Before
//  GE-07 those masks were ~always 0 (iFlushCall evicted everything first);
//  these tests are the first to exercise nonzero masks with dirty NEON
//  (128-bit MMI) and GPR state crossing the fault. Run()'s JIT-vs-interp
//  auto-diff plus the explicit 128-bit expects are the oracle.
// ===========================================================================

TEST(EeRecLoadStore, FastmemFaultLoadPreservesLiveMmiState)
{
	EeRecTestHarness h;
	h.WriteU32(kScratch + 0, 0x00003000u);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::a1, 0x1000F000u); // INTC_STAT: MMIO page → fastmem fault → thunk
	h.SetGpr128(reg::t4, 0x1111'2222'3333'4444ull, 0x5555'6666'7777'8888ull);
	h.SetGpr128(reg::t5, 0x0101'0101'0101'0101ull, 0x0202'0202'0202'0202ull);
	vtlb_ClearLoadStoreInfo(); // ensure this compile takes fastmem (set is process-global)
	h.LoadProgram({
		LW(reg::t0, 0, reg::a0),          // non-const live scalar
		ee::PADDW(reg::t6, reg::t4, reg::t5), // dirty 128-bit NEON result, left resident
		LW(reg::v1, 0, reg::a1),          // faults → thunk C call with live state
		ee::PADDW(reg::t7, reg::t6, reg::t5), // consumes the resident quad AFTER the call
		ADDU(reg::v0, reg::t0, reg::t0),
	});
	h.Run();
	h.ExpectGpr64(reg::t0, 0x3000ull);
	h.ExpectGpr64(reg::v0, 0x6000ull);
	h.ExpectGpr128(reg::t6, 0x1212'2323'3434'4545ull, 0x5757'6868'7979'8a8aull);
	h.ExpectGpr128(reg::t7, 0x1313'2424'3535'4646ull, 0x5959'6a6a'7b7b'8c8cull);
	// Prove the FAULT path actually ran (not a softmem compile that would pass
	// vacuously): a genuine fastmem fault records the guest PC of the MMIO LW.
	bool faulted = false;
	for (u32 a = RecompilerTestEnvironment::kProgramPc; a < RecompilerTestEnvironment::kProgramPc + 0x20; a += 4)
		faulted |= vtlb_IsFaultingPC(a);
	EXPECT_TRUE(faulted) << "MMIO LW did not take the fastmem-fault/backpatch path";
}

TEST(EeRecLoadStore, FastmemFaultStorePreservesLiveMmiState)
{
	EeRecTestHarness h;
	h.WriteU32(kScratch + 0, 0x00004000u);
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::a1, 0x1000F000u); // INTC_STAT is write-1-to-clear; state 0 stays 0
	h.SetGpr128(reg::t4, 0x1111'2222'3333'4444ull, 0x5555'6666'7777'8888ull);
	h.SetGpr128(reg::t5, 0x0101'0101'0101'0101ull, 0x0202'0202'0202'0202ull);
	vtlb_ClearLoadStoreInfo();
	h.LoadProgram({
		LW(reg::t0, 0, reg::a0),
		ee::PADDW(reg::t6, reg::t4, reg::t5),
		SW(reg::t0, 0, reg::a1),          // store fault → thunk C call
		ee::PADDW(reg::t7, reg::t6, reg::t5),
		ADDU(reg::v0, reg::t0, reg::t0),
	});
	h.Run();
	h.ExpectGpr64(reg::t0, 0x4000ull);
	h.ExpectGpr64(reg::v0, 0x8000ull);
	h.ExpectGpr128(reg::t6, 0x1212'2323'3434'4545ull, 0x5757'6868'7979'8a8aull);
	h.ExpectGpr128(reg::t7, 0x1313'2424'3535'4646ull, 0x5959'6a6a'7b7b'8c8cull);
	bool faulted = false;
	for (u32 a = RecompilerTestEnvironment::kProgramPc; a < RecompilerTestEnvironment::kProgramPc + 0x20; a += 4)
		faulted |= vtlb_IsFaultingPC(a);
	EXPECT_TRUE(faulted) << "MMIO SW did not take the fastmem-fault/backpatch path";
}

// ---- GE-08: SWC1 reads a resident FPR slot via Fmov (slot kept) -------------

TEST(EeRecLoadStore, Swc1ResidentValueStoresAndSurvivesFault)
{
	// f6 is left NEON-resident (dirty) by ADD.S; the first SWC1 must store its
	// CURRENT value without a memory round-trip, the MMIO SWC1 faults with the
	// slot live across the thunk C call, and the final ADD.S consumes the
	// still-resident slot. Exact powers of two keep the clamped FPU math exact.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(4, 0x40000000u); // 2.0f
	h.SetFprBits(5, 0x3F800000u); // 1.0f
	h.SetGpr64(reg::a0, kScratch);
	h.SetGpr64(reg::a1, 0x1000F000u); // INTC_STAT (W1C; harness state 0 stays 0)
	vtlb_ClearLoadStoreInfo();
	h.LoadProgram({
		ee::ADD_S(6, 4, 5),           // f6 = 3.0f, resident + dirty
		ee::SWC1(6, 0, reg::a0),      // resident-store fast path
		ee::SWC1(6, 0, reg::a1),      // MMIO store → fastmem fault → thunk
		ee::ADD_S(7, 6, 5),           // f7 = 4.0f from the still-resident f6
		ee::SWC1(7, 8, reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.ReadU32(kScratch + 0), 0x40400000u); // 3.0f
	EXPECT_EQ(h.ReadU32(kScratch + 8), 0x40800000u); // 4.0f
	bool faulted = false;
	for (u32 a = RecompilerTestEnvironment::kProgramPc; a < RecompilerTestEnvironment::kProgramPc + 0x20; a += 4)
		faulted |= vtlb_IsFaultingPC(a);
	EXPECT_TRUE(faulted) << "MMIO SWC1 did not take the fastmem-fault/backpatch path";
}

// ---- GE-10: LWC1 loads straight into the allocated FPR slot ------------------

TEST(EeRecLoadStore, Lwc1LoadsToResidentSlotThenArith)
{
	// LWC1's fastmem LDR targets ft's allocated S register; the following
	// ADD.S must consume the resident slot, and the block-end flush must land
	// the loaded value in fpr memory for the post-state diff.
	EeRecTestHarness h;
	h.EnableCop1();
	h.WriteU32(kScratch, 0x40400000u); // 3.0f
	h.SetFprBits(3, 0x3F800000u);      // 1.0f
	h.SetGpr64(reg::a0, kScratch);
	h.LoadProgram({
		ee::LWC1(1, 0, reg::a0),      // f1 = 3.0f
		ee::ADD_S(2, 1, 3),           // f2 = 4.0f
	});
	h.Run();
	EXPECT_EQ(h.GetFprBitsInterp(1), 0x40400000u);
	EXPECT_EQ(h.GetFprBitsInterp(2), 0x40800000u);
}

TEST(EeRecLoadStore, Lwc1OverwritesStaleDirtyResidentDest)
{
	// f1 is left resident+dirty by ADD.S, then LWC1 overwrites it. The loaded
	// value must win — for the next consumer AND the block-end flush (a
	// stale-slot writeback over the load is the classic hazard the old shape
	// avoided with DELETE_REG_FREE_NO_WRITEBACK).
	EeRecTestHarness h;
	h.EnableCop1();
	h.WriteU32(kScratch, 0x41200000u); // 10.0f
	h.SetFprBits(3, 0x3F800000u);      // 1.0f
	h.SetFprBits(4, 0x40000000u);      // 2.0f
	h.SetGpr64(reg::a0, kScratch);
	h.LoadProgram({
		ee::ADD_S(1, 3, 4),            // f1 = 3.0f (resident, dirty)
		ee::LWC1(1, 0, reg::a0),       // f1 = 10.0f overwrites the slot
		ee::ADD_S(2, 1, 3),            // f2 = 11.0f
	});
	h.Run();
	EXPECT_EQ(h.GetFprBitsInterp(1), 0x41200000u);
	EXPECT_EQ(h.GetFprBitsInterp(2), 0x41300000u);
}

TEST(EeRecLoadStore, Lwc1FaultPathLandsInResidentSlot)
{
	// MMIO LWC1 faults; the backpatch thunk's slow path must Fmov the
	// C-handler result into the allocated S register (the is_fpr load tail),
	// with OTHER live NEON state (dirty f6) saved/restored around the call.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(4, 0x40000000u); // 2.0f
	h.SetFprBits(5, 0x3F800000u); // 1.0f
	h.SetGpr64(reg::a1, 0x1000F000u); // INTC_STAT (harness state 0 → loads 0)
	vtlb_ClearLoadStoreInfo();
	h.LoadProgram({
		ee::ADD_S(6, 4, 5),           // f6 = 3.0f resident+dirty across the fault
		ee::LWC1(1, 0, reg::a1),      // MMIO load → fault → thunk → f1 = 0.0f
		ee::ADD_S(7, 1, 4),           // f7 = 2.0f from the faulted-in f1
		ee::ADD_S(8, 6, 5),           // f8 = 4.0f from the still-resident f6
	});
	h.Run();
	EXPECT_EQ(h.GetFprBitsInterp(1), 0x00000000u);
	EXPECT_EQ(h.GetFprBitsInterp(7), 0x40000000u);
	EXPECT_EQ(h.GetFprBitsInterp(8), 0x40800000u);
	bool faulted = false;
	for (u32 a = RecompilerTestEnvironment::kProgramPc; a < RecompilerTestEnvironment::kProgramPc + 0x20; a += 4)
		faulted |= vtlb_IsFaultingPC(a);
	EXPECT_TRUE(faulted) << "MMIO LWC1 did not take the fastmem-fault/backpatch path";
}
