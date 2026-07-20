// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// S4-1 hazard suite: COP2 transfer ops (QMFC2/QMTC2/CFC2/CTC2) interleaved
// with allocator-resident EE register state.
//
// The transfer emitters historically opened with iFlushCall(FLUSH_EVERYTHING),
// which made every interleave trivially coherent (nothing was ever resident
// across a transfer) — and made every transfer a block-wide writeback storm
// (the S4 icache finding: COP2-seam churn is the fat core of the EErec
// footprint). S4-1 drops the unconditional flush and routes the transfers
// through the register allocator, so these interleaves become the dangerous
// cases:
//
//   - QMTC2/CTC2 must READ the newest rt: a dirty NEON quad (MMI result), a
//     dirty scalar slot, a dirty lazy pin, or a propagated constant — never
//     stale cpuRegs memory.
//   - QMFC2/CFC2 must WRITE rt so every later consumer (MMI quad read, scalar
//     ALU, pin read, block-end writeback) sees the transferred value, and so
//     the non-written half of a partial write (CFC2 UD[1]; REG_R UL[1..3])
//     survives from the newest resident copy, not from stale memory.
//   - The conditional VU0 sync seam inside a transfer (analysis-marked, ONLY
//     place a transfer may still flush) must spill/reload residency around
//     the C call without losing values on either the taken or skipped path.
//
// Every test here is engine-diffed (JIT vs interp) by the harness post-state
// diff on top of the explicit expectations. The suite must be green BEFORE
// the S4-1 emitter change (FLUSH_EVERYTHING satisfies all contracts by brute
// force) and stay green after.

#include "harness/EeRecTestHarness.h"

#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace mips;
using namespace mips::ee;

namespace {

constexpr u32 r_v0 = 2; // statically pinned EE GPR (tier-1 pin)
constexpr u32 r_t0 = 8; // unpinned
constexpr u32 r_t1 = 9;
constexpr u32 r_t2 = 10;
constexpr u32 r_t3 = 11;

// Micro at PC 0: 32 NOP pairs (outlasts the 16-cycle kickstart window), then
// vf_dst = vf_src + vf_src, then E-bit. Same recipe as the PendingMicroSync
// suite in ee_vu0_cop2_macro_tests.cpp: idempotent across the harness's two
// runs because vf_src is never written, and long enough that VU0 is still
// mid-flight (VPU_STAT bit 0 set) when the op under test executes — so the
// transfer's conditional sync seam takes the CALL path, not the Tbz skip.
void SeedPendingMicroDoubling(EeRecTestHarness& h, u32 vf_dst, u32 vf_src)
{
	u32 off = 0;
	for (int i = 0; i < 32; i++, off += 8)
		h.SeedVu0Microprogram(off, {vu::NopPair()});
	h.SeedVu0Microprogram(off, {
		vu::VuOp{0, vu::VADD_U(vu::mask::xyzw, vf_dst, vf_src, vf_src)},
		vu::EBitNopPair(),
		vu::NopPair(), // explicit E-bit delay pair — keep micro mem deterministic
	});
}

} // namespace

// =========================================================================
//  QMTC2 source residency — the newest rt must reach VF
// =========================================================================

TEST(EeVu0Cop2TransferResidency, Qmtc2ReadsMmiQuadResidentSource)
{
	// PADDW leaves rt dirty in a NEON quad slot; memory is stale for all
	// 128 bits when QMTC2 reads it.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr128(r_t0, 0x0000000100000002ull, 0x0000000300000004ull);
	h.SetGpr128(r_t1, 0x0000001000000020ull, 0x0000003000000040ull);
	h.LoadProgram({
		PADDW(r_t2, r_t0, r_t1),
		QMTC2(r_t2, 5),
	});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'x'), 0x00000022u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'y'), 0x00000011u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'z'), 0x00000044u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'w'), 0x00000033u);
	for (char l : {'x', 'y', 'z', 'w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(5, l), h.GetVu0VfBitsInterp(5, l));
}

TEST(EeVu0Cop2TransferResidency, Qmtc2SourceStaysQuadResidentForMmi)
{
	// After QMTC2 the source should still be readable by a following MMI op
	// (allocator MODE_READ residency) — and correct either way.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr128(r_t0, 0x0000000A0000000Bull, 0x0000000C0000000Dull);
	h.LoadProgram({
		QMTC2(r_t0, 6),
		PADDW(r_t1, r_t0, r_t0),
	});
	h.Run();
	h.ExpectGpr128(r_t1, 0x0000001400000016ull, 0x000000180000001Aull);
	EXPECT_EQ(h.GetVu0VfBitsJit(6, 'x'), 0x0000000Bu);
	EXPECT_EQ(h.GetVu0VfBitsJit(6, 'w'), 0x0000000Cu);
}

TEST(EeVu0Cop2TransferResidency, Qmtc2MergesScalarResidentLower64)
{
	// DADDU leaves rt's lower 64 dirty in a scalar slot; the seeded upper 64
	// stay live only in memory. QMTC2 must merge both halves.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr128(r_t2, 0x1111111111111111ull, 0xAAAAAAAABBBBBBBBull);
	h.SetGpr64(r_t0, 0x0123456789ABCDEFull);
	h.SetGpr64(r_t1, 1);
	h.LoadProgram({
		DADDU(r_t2, r_t0, r_t1),
		QMTC2(r_t2, 6),
	});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(6, 'x'), 0x89ABCDF0u);
	EXPECT_EQ(h.GetVu0VfBitsJit(6, 'y'), 0x01234567u);
	EXPECT_EQ(h.GetVu0VfBitsJit(6, 'z'), 0xBBBBBBBBu);
	EXPECT_EQ(h.GetVu0VfBitsJit(6, 'w'), 0xAAAAAAAAu);
	for (char l : {'x', 'y', 'z', 'w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(6, l), h.GetVu0VfBitsInterp(6, l));
}

TEST(EeVu0Cop2TransferResidency, Qmtc2MergesDirtyPinnedLower64)
{
	// Same shape with a statically pinned rt: under lazy-dirty the newest
	// lower 64 live ONLY in the pin host register.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr128(r_v0, 0x2222222222222222ull, 0xCCCCCCCCDDDDDDDDull);
	h.SetGpr64(r_t0, 0x00000000F0000001ull);
	h.SetGpr64(r_t1, 0x0000000010000002ull);
	h.LoadProgram({
		DADDU(r_v0, r_t0, r_t1),
		QMTC2(r_v0, 7),
	});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(7, 'x'), 0x00000003u);
	EXPECT_EQ(h.GetVu0VfBitsJit(7, 'y'), 0x00000001u);
	EXPECT_EQ(h.GetVu0VfBitsJit(7, 'z'), 0xDDDDDDDDu);
	EXPECT_EQ(h.GetVu0VfBitsJit(7, 'w'), 0xCCCCCCCCu);
	for (char l : {'x', 'y', 'z', 'w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(7, l), h.GetVu0VfBitsInterp(7, l));
}

TEST(EeVu0Cop2TransferResidency, Qmtc2ReadsConstPropagatedLower64)
{
	// LUI/ORI make rt compile-time const (never stored to memory before the
	// transfer); upper 64 still come from the seeded memory image.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr128(r_t0, 0x000000000000DEADull, 0xCCCCCCCCDDDDDDDDull);
	h.LoadProgram({
		LUI(r_t0, 0x1234),
		ORI(r_t0, r_t0, 0x5678),
		QMTC2(r_t0, 8),
	});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(8, 'x'), 0x12345678u);
	EXPECT_EQ(h.GetVu0VfBitsJit(8, 'y'), 0x00000000u);
	EXPECT_EQ(h.GetVu0VfBitsJit(8, 'z'), 0xDDDDDDDDu);
	EXPECT_EQ(h.GetVu0VfBitsJit(8, 'w'), 0xCCCCCCCCu);
	for (char l : {'x', 'y', 'z', 'w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(8, l), h.GetVu0VfBitsInterp(8, l));
}

// =========================================================================
//  QMFC2 destination residency — every later consumer sees the transfer
// =========================================================================

TEST(EeVu0Cop2TransferResidency, Qmfc2DestReadableByMmi)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(1, 0x10, 0x20, 0x30, 0x40);
	h.LoadProgram({
		QMFC2(r_t0, 1),
		PADDW(r_t1, r_t0, r_t0),
	});
	h.Run();
	h.ExpectGpr128(r_t0, 0x0000002000000010ull, 0x0000004000000030ull);
	h.ExpectGpr128(r_t1, 0x0000004000000020ull, 0x0000008000000060ull);
}

TEST(EeVu0Cop2TransferResidency, Qmfc2DestReadableByScalar)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(2, 0xCAFE0001, 0x00000002, 0x0BAD0003, 0x00000004);
	h.LoadProgram({
		QMFC2(r_t0, 2),
		DADDU(r_t1, r_t0, 0),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(r_t1), 0x00000002CAFE0001ull);
	EXPECT_EQ(h.GetGpr64Jit(r_t1), h.GetGpr64Interp(r_t1));
}

TEST(EeVu0Cop2TransferResidency, Qmfc2OverwritesDirtyScalarDest)
{
	// A pending scalar write to rt must NOT resurface after QMFC2's full
	// 128-bit overwrite (discarded-writeback ordering).
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr64(r_t1, 5);
	h.SetGpr64(r_t2, 7);
	h.SeedVu0VfBits(3, 0x31313131, 0x32323232, 0x33333333, 0x34343434);
	h.LoadProgram({
		DADDU(r_t0, r_t1, r_t2),
		QMFC2(r_t0, 3),
	});
	h.Run();
	h.ExpectGpr128(r_t0, 0x3232323231313131ull, 0x3434343433333333ull);
}

TEST(EeVu0Cop2TransferResidency, Qmfc2OverwritesConstDest)
{
	// rt is compile-time const when QMFC2 overwrites it; the stale constant
	// must not resurrect through const-prop in a later consumer.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(5, 0x51515151, 0x52525252, 0x53535353, 0x54545454);
	h.LoadProgram({
		ORI(r_t0, 0, 0xBEEF),
		QMFC2(r_t0, 5),
		QMTC2(r_t0, 6),
	});
	h.Run();
	h.ExpectGpr128(r_t0, 0x5252525251515151ull, 0x5454545453535353ull);
	for (char l : {'x', 'y', 'z', 'w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(6, l), h.GetVu0VfBitsJit(5, l));
}

TEST(EeVu0Cop2TransferResidency, Qmfc2ToPinnedDestPropagates)
{
	// Pinned dest: the transferred lower 64 must reach the pin (or the quad
	// must stay authoritative over it) for the scalar consumer AND the final
	// register image.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(4, 0x41414141, 0x42424242, 0x43434343, 0x44444444);
	h.LoadProgram({
		QMFC2(r_v0, 4),
		DADDU(r_t1, r_v0, 0),
	});
	h.Run();
	h.ExpectGpr128(r_v0, 0x4242424241414141ull, 0x4444444443434343ull);
	EXPECT_EQ(h.GetGpr64Jit(r_t1), 0x4242424241414141ull);
}

TEST(EeVu0Cop2TransferResidency, MacroVfWriteThenQmfc2ReadsFreshVf)
{
	// A cache-aware macro FMAC leaves VF3 dirty in the compile-time VF cache
	// (q16-q20); the transfer's raw VF read must see the flushed value.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(1, 1.5f, 2.5f, 3.5f, 4.5f);
	h.SeedVu0Vf(2, 0.5f, 0.5f, 0.5f, 0.5f);
	h.LoadProgram({
		VADD_C2(/*mask*/0xF, /*fd*/3, /*fs*/1, /*ft*/2),
		QMFC2(r_t0, 3),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
	EXPECT_EQ(static_cast<u32>(h.GetGpr64Jit(r_t0)), 0x40000000u); // 2.0f
	EXPECT_EQ(static_cast<u32>(h.GetGpr64Jit(r_t0) >> 32), 0x40400000u); // 3.0f
}

// =========================================================================
//  CFC2 / CTC2 — VI transfers against resident rt
// =========================================================================

TEST(EeVu0Cop2TransferResidency, Ctc2WritesScalarResidentValue)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr64(r_t0, 0x1200);
	h.SetGpr64(r_t1, 0x0034);
	h.LoadProgram({
		DADDU(r_t2, r_t0, r_t1),
		CTC2(r_t2, 5),
	});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(5), 0x1234u);
	EXPECT_EQ(h.GetVu0ViJit(5), h.GetVu0ViInterp(5));
}

TEST(EeVu0Cop2TransferResidency, Ctc2WritesMmiQuadResidentLane0)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr128(r_t0, 0x0000000100000E00ull, 0x0000000300000004ull);
	h.SetGpr128(r_t1, 0x0000001000000021ull, 0x0000003000000040ull);
	h.LoadProgram({
		PADDW(r_t2, r_t0, r_t1),
		CTC2(r_t2, 6),
	});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(6), 0x0E21u);
	EXPECT_EQ(h.GetVu0ViJit(6), h.GetVu0ViInterp(6));
}

TEST(EeVu0Cop2TransferResidency, Cfc2IntoMmiResidentDestPreservesUpper64)
{
	// CFC2 writes rt.UD[0] only; UD[1] must survive from the newest resident
	// copy (the PADDW quad), not from stale memory.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(7, 0x8001); // bit 15 set → sign-extends through CFC2's sxtw
	h.SetGpr128(r_t1, 0x0000000100000002ull, 0x0000000300000004ull);
	h.SetGpr128(r_t2, 0x0000001000000020ull, 0x0000003000000040ull);
	h.LoadProgram({
		PADDW(r_t0, r_t1, r_t2),
		CFC2(r_t0, 7),
	});
	h.Run();
	// Plain VI reads are zero-extended 16→32, then sign-extended 32→64.
	h.ExpectGpr128(r_t0, 0x0000000000008001ull, 0x0000003300000044ull);
}

TEST(EeVu0Cop2TransferResidency, Cfc2RegRPartialWriteOverScalarResident)
{
	// The REG_R path writes rt.UL[0] ONLY: UL[1] must survive from the dirty
	// scalar slot's value, UD[1] from memory.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(REG_R, 0x00412345); // only bits 0-22 are architecturally real
	h.SetGpr128(r_t0, 0ull, 0xEEEEEEEEFFFFFFFFull);
	h.SetGpr64(r_t1, 0xABCD432100000005ull);
	h.SetGpr64(r_t2, 3);
	h.LoadProgram({
		DADDU(r_t0, r_t1, r_t2),
		CFC2(r_t0, REG_R),
	});
	h.Run();
	h.ExpectGpr128(r_t0, 0xABCD432100412345ull, 0xEEEEEEEEFFFFFFFFull);
}

TEST(EeVu0Cop2TransferResidency, Cfc2RegRPartialWriteOverQuadResident)
{
	// Same partial-write contract with rt quad-dirty from an MMI op: UL[1]
	// and UD[1] come from the quad.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(REG_R, 0x00654321);
	h.SetGpr128(r_t1, 0x0000000100000002ull, 0x0000000300000004ull);
	h.SetGpr128(r_t2, 0x0000001000000020ull, 0x0000003000000040ull);
	h.LoadProgram({
		PADDW(r_t0, r_t1, r_t2),
		CFC2(r_t0, REG_R),
	});
	h.Run();
	h.ExpectGpr128(r_t0, 0x0000001100654321ull, 0x0000003300000044ull);
}

TEST(EeVu0Cop2TransferResidency, Cfc2ThroughPinnedDestIntoQmtc2)
{
	// CFC2 into a pinned rt, then QMTC2 of the same rt: the chain must carry
	// the just-transferred VI value (sign-extended) plus rt's prior upper 64.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(8, 0x0BEE);
	h.SetGpr128(r_v0, 0x1111111122222222ull, 0x3333333344444444ull);
	h.LoadProgram({
		CFC2(r_v0, 8),
		QMTC2(r_v0, 9),
	});
	h.Run();
	h.ExpectGpr128(r_v0, 0x0000000000000BEEull, 0x3333333344444444ull);
	EXPECT_EQ(h.GetVu0VfBitsJit(9, 'x'), 0x00000BEEu);
	EXPECT_EQ(h.GetVu0VfBitsJit(9, 'y'), 0x00000000u);
	EXPECT_EQ(h.GetVu0VfBitsJit(9, 'z'), 0x44444444u);
	EXPECT_EQ(h.GetVu0VfBitsJit(9, 'w'), 0x33333333u);
}

// =========================================================================
//  Sync seam inside a transfer — residency must ride the C call
// =========================================================================
// VCALLMS leaves VU0 running (VPU_STAT bit 0 set — the 32-pair micro
// outlasts the kickstart window), so the analysis-marked sync inside the
// following transfer takes the CALL path at runtime. Values resident at the
// seam (MMI quad, scalar slot) must survive the spill/reload, and the
// transfer's own data must agree with the interpreter afterwards.

TEST(EeVu0Cop2TransferResidency, Qmfc2SyncSeamPreservesResidency)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(REG_VPU_STAT, 0); // control regs survive EnableVu0Capture — start clean
	// The JIT sync path and the interp single-stepper drain the pending micro
	// to different depths (run-ahead cycle accounting), so TPC legitimately
	// diverges — same benign class as the VPU_STAT exclusion in the BC2
	// suite. The EE-side residency assertions below are the test's contract.
	h.IgnoreVu0Vi(REG_TPC);
	h.SeedVu0Vf(1, 2.0f, 2.0f, 2.0f, 2.0f);
	h.SeedVu0Vf(2, 32.0f, 32.0f, 32.0f, 32.0f); // micro rewrites to 4.0
	SeedPendingMicroDoubling(h, /*vf_dst*/2, /*vf_src*/1);
	h.SetGpr128(r_t1, 0x0000000100000002ull, 0x0000000300000004ull);
	h.SetGpr128(r_t2, 0x0000001000000020ull, 0x0000003000000040ull);
	h.SetGpr64(r_t0, 0x0F0F0F0F0F0F0F0Full);
	h.LoadProgram({
		VCALLMS(0),
		PADDW(r_t3, r_t1, r_t2),  // quad-dirty across the seam
		DADDU(r_v0, r_t0, r_t2),  // pinned scalar result across the seam
		QMFC2(r_t0, 2),           // sync fires here (VPU_STAT set at runtime)
		PADDW(r_t1, r_t0, r_t3),  // consumes both post-seam residencies
	});
	h.Run();
	h.ExpectGpr128(r_t3, 0x0000001100000022ull, 0x0000003300000044ull);
	EXPECT_EQ(h.GetGpr64Jit(r_v0), h.GetGpr64Interp(r_v0));
	// Whatever VF2 value the (partially or fully drained) micro exposes, the
	// two engines must agree on what QMFC2 read and what the MMI op computed.
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
	EXPECT_EQ(h.GetGpr64Jit(r_t1), h.GetGpr64Interp(r_t1));

	// The test deliberately ends with the micro still mid-flight; park VU0 so
	// the leftover running state (VPU_STAT bit 0 + parked TPC survive across
	// harness instances) can't leak into later tests.
	vuRegs[0].VI[REG_VPU_STAT].UL = 0;
}

TEST(EeVu0Cop2TransferResidency, Qmtc2SyncSeamReloadsResidentSource)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(REG_VPU_STAT, 0);
	h.SeedVu0Vf(1, 2.0f, 2.0f, 2.0f, 2.0f);
	h.SeedVu0Vf(2, 32.0f, 32.0f, 32.0f, 32.0f);
	SeedPendingMicroDoubling(h, /*vf_dst*/2, /*vf_src*/1);
	h.SetGpr128(r_t1, 0x0000000100000002ull, 0x0000000300000004ull);
	h.SetGpr128(r_t2, 0x0000001000000020ull, 0x0000003000000040ull);
	h.LoadProgram({
		VCALLMS(0),
		PADDW(r_t3, r_t1, r_t2), // quad-dirty source at the seam
		QMTC2(r_t3, 5),          // sync fires here, then reads t3
	});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'x'), 0x00000022u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'y'), 0x00000011u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'z'), 0x00000044u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'w'), 0x00000033u);
	for (char l : {'x', 'y', 'z', 'w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(5, l), h.GetVu0VfBitsInterp(5, l));
	h.ExpectGpr128(r_t3, 0x0000001100000022ull, 0x0000003300000044ull);

	// Park the mid-flight micro — see Qmfc2SyncSeamPreservesResidency.
	vuRegs[0].VI[REG_VPU_STAT].UL = 0;
}

} // namespace recompiler_tests
