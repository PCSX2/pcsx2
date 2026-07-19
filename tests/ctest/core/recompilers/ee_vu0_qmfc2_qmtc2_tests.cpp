// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// EE↔VU0 QMFC2/QMTC2/MFC2/MTC2 handoff DiffJitVsInterp suite.
//
// QMFC2 / QMTC2 are 128-bit transfers between an EE GPR (the full 128-bit
// register file) and VU0.VF[fs]. MFC2 / MTC2 are 32-bit transfers reading
// or writing one VF lane (selected by the broadcast field — but in the EE
// encoding the lane is implicit and matches the lane index in VF.UL[]).
//
// Real games depend on the "full quadword" semantics of QMTC2 because VF
// registers always hold a 4-tuple of float32. The interlock bit on QMTC2
// triggers _vu0WaitMicro() — that sync-barrier path needs its own test in
// the VCALLMS suite; here we focus on the data-transfer correctness.

#include "harness/EeRecTestHarness.h"

#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace mips;
using namespace mips::ee;

namespace {

constexpr u32 r_t0 = 8;
constexpr u32 r_t1 = 9;

inline u32 FloatBits(float f) { u32 b; std::memcpy(&b, &f, sizeof(b)); return b; }

} // namespace

// =========================================================================
//  QMFC2 — read full 128-bit VF[fs] into EE GPR
// =========================================================================

TEST(EeVu0Qmfc2, ReadsAllFourLanesOfVf)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(1, 0xAABBCCDD, 0x11223344, 0x55667788, 0xDEADBEEF);
	h.LoadProgram({QMFC2(r_t0, 1)});
	h.Run();
	// EE GPR holds four 32-bit lanes packed [w hi64 | z | y | x lo64] in
	// the integer-typed view — VU lanes match GPR.UL[0..3].
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
	EXPECT_EQ(static_cast<u32>(h.GetGpr64Jit(r_t0)),       0xAABBCCDDu);
	EXPECT_EQ(static_cast<u32>(h.GetGpr64Jit(r_t0) >> 32), 0x11223344u);
}

TEST(EeVu0Qmfc2, ReadsVf0HardwiredZeroOneFloat)
{
	// VF[0] = (0, 0, 0, 1.0f). QMFC2 reads the full quadword.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.LoadProgram({QMFC2(r_t0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
}

TEST(EeVu0Qmfc2, ReadsVfWithSpecialFloatBitsPreservesPayload)
{
	// NaN payload preservation matters — the interpreter copies bits, the
	// JIT must do the same (no float-load that would canonicalize NaNs).
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(2, 0x7FC12345, 0xFFC67890, 0x7F800000, 0xFF800000);
	h.LoadProgram({QMFC2(r_t0, 2)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
}

// =========================================================================
//  QMTC2 — write full 128-bit EE GPR into VF[fs]
// =========================================================================

TEST(EeVu0Qmtc2, WritesAllFourLanesOfVf)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr128(r_t0, 0x1122334455667788ull, 0xAABBCCDDEEFF0011ull);
	h.LoadProgram({QMTC2(r_t0, 5)});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'x'), 0x55667788u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'y'), 0x11223344u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'z'), 0xEEFF0011u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'w'), 0xAABBCCDDu);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'x'), h.GetVu0VfBitsInterp(5, 'x'));
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'y'), h.GetVu0VfBitsInterp(5, 'y'));
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'z'), h.GetVu0VfBitsInterp(5, 'z'));
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'w'), h.GetVu0VfBitsInterp(5, 'w'));
}

TEST(EeVu0Qmtc2, WritesVfZeroIsNoop)
{
	// VF[0] is hardwired (0, 0, 0, 1.0f); QMTC2 to it must not overwrite.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr128(r_t0, 0xDEADBEEFCAFEBABEull, 0x0123456789ABCDEFull);
	h.LoadProgram({QMTC2(r_t0, 0)});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(0, 'x'), 0u);
	EXPECT_EQ(h.GetVu0VfBitsJit(0, 'y'), 0u);
	EXPECT_EQ(h.GetVu0VfBitsJit(0, 'z'), 0u);
	EXPECT_EQ(h.GetVu0VfBitsJit(0, 'w'), FloatBits(1.0f));
}

TEST(EeVu0Qmfc2Qmtc2RoundTrip, QmtcThenQmfcReturnsOriginal)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr128(r_t0, 0xCAFEBABEDEADBEEFull, 0x1234567890ABCDEFull);
	h.LoadProgram({
		QMTC2(r_t0, 7),
		QMFC2(r_t1, 7),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(r_t1), 0xCAFEBABEDEADBEEFull);
	EXPECT_EQ(h.GetGpr64Jit(r_t1), h.GetGpr64Interp(r_t1));
}

// =========================================================================
//  MFC2 / MTC2 — 32-bit single-lane transfer
// =========================================================================

TEST(EeVu0Mfc2, ReadsLaneXOfVf)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(3, 0x11111111, 0x22222222, 0x33333333, 0x44444444);
	h.LoadProgram({MFC2(r_t0, 3)});
	h.Run();
	// MFC2 with sa=0 reads lane x. JIT and interp must agree on the
	// 32-bit slice and on its sign-extension into the 64-bit GPR.
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
}

TEST(EeVu0Mtc2, WritesLaneXOfVf)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(4, 0x11111111, 0x22222222, 0x33333333, 0x44444444);
	h.SetGpr64(r_t0, 0xBEEFCAFE);
	h.LoadProgram({MTC2(r_t0, 4)});
	h.Run();
	// Other lanes must remain untouched. Both engines must agree.
	EXPECT_EQ(h.GetVu0VfBitsJit(4, 'y'), h.GetVu0VfBitsInterp(4, 'y'));
	EXPECT_EQ(h.GetVu0VfBitsJit(4, 'z'), h.GetVu0VfBitsInterp(4, 'z'));
	EXPECT_EQ(h.GetVu0VfBitsJit(4, 'w'), h.GetVu0VfBitsInterp(4, 'w'));
	EXPECT_EQ(h.GetVu0VfBitsJit(4, 'x'), h.GetVu0VfBitsInterp(4, 'x'));
}

// =========================================================================
//  LQC2 / SQC2 — quadword load/store between VU0.VF and EE memory
// =========================================================================

TEST(EeVu0Lqc2, LoadsQuadFromMemoryIntoVf)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// EE main RAM at 0x100100. Write a quadword and load it via LQC2.
	h.WriteU32(0x00100100, 0xAAAA1111);
	h.WriteU32(0x00100104, 0xBBBB2222);
	h.WriteU32(0x00100108, 0xCCCC3333);
	h.WriteU32(0x0010010C, 0xDDDD4444);
	h.SetGpr64(r_t0, 0x00100100);
	h.LoadProgram({LQC2(/*ft*/6, /*base*/r_t0, /*offset*/0)});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(6, 'x'), 0xAAAA1111u);
	EXPECT_EQ(h.GetVu0VfBitsJit(6, 'y'), 0xBBBB2222u);
	EXPECT_EQ(h.GetVu0VfBitsJit(6, 'z'), 0xCCCC3333u);
	EXPECT_EQ(h.GetVu0VfBitsJit(6, 'w'), 0xDDDD4444u);
	EXPECT_EQ(h.GetVu0VfBitsJit(6, 'x'), h.GetVu0VfBitsInterp(6, 'x'));
}

TEST(EeVu0Lqc2, LoadsWithSignedOffset)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.WriteU32(0x00100200, 0x12121212);
	h.WriteU32(0x00100204, 0x34343434);
	h.WriteU32(0x00100208, 0x56565656);
	h.WriteU32(0x0010020C, 0x78787878);
	h.SetGpr64(r_t0, 0x00100210);
	h.LoadProgram({LQC2(/*ft*/8, /*base*/r_t0, /*offset*/-16)});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(8, 'x'), 0x12121212u);
	EXPECT_EQ(h.GetVu0VfBitsJit(8, 'w'), 0x78787878u);
	EXPECT_EQ(h.GetVu0VfBitsJit(8, 'x'), h.GetVu0VfBitsInterp(8, 'x'));
}

TEST(EeVu0Sqc2, StoresQuadFromVfIntoMemory)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.TrackMemWindow(0x00100300, 16);
	h.SeedVu0VfBits(9, 0xAAAA1111, 0xBBBB2222, 0xCCCC3333, 0xDDDD4444);
	h.SetGpr64(r_t0, 0x00100300);
	h.LoadProgram({SQC2(/*ft*/9, /*base*/r_t0, /*offset*/0)});
	h.Run();
	EXPECT_EQ(h.ReadU32(0x00100300), 0xAAAA1111u);
	EXPECT_EQ(h.ReadU32(0x00100304), 0xBBBB2222u);
	EXPECT_EQ(h.ReadU32(0x00100308), 0xCCCC3333u);
	EXPECT_EQ(h.ReadU32(0x0010030C), 0xDDDD4444u);
}

TEST(EeVu0LqSqRoundTrip, SqThenLqReturnsOriginalVf)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(10, 0xCAFE0001, 0xCAFE0002, 0xCAFE0003, 0xCAFE0004);
	h.SetGpr64(r_t0, 0x00100400);
	h.LoadProgram({
		SQC2(/*ft*/10, /*base*/r_t0, 0),
		LQC2(/*ft*/11, /*base*/r_t0, 0),
	});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(11, 'x'), 0xCAFE0001u);
	EXPECT_EQ(h.GetVu0VfBitsJit(11, 'y'), 0xCAFE0002u);
	EXPECT_EQ(h.GetVu0VfBitsJit(11, 'z'), 0xCAFE0003u);
	EXPECT_EQ(h.GetVu0VfBitsJit(11, 'w'), 0xCAFE0004u);
}

} // namespace recompiler_tests
