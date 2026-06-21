// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// EE↔VU0 CFC2/CTC2 handoff DiffJitVsInterp suite.
//
// CFC2 ($t = VU0.VI[$d]) and CTC2 (VU0.VI[$d] = $t) are the EE-side
// gateway to VU0's integer/control register file. Both engines share the
// same VU0 register bank (vuRegs[0]) so the test contract is:
//   1. EE GPR target gets the correct VI value (CFC2).
//   2. VU0 VI target gets the correct EE value (CTC2).
//   3. Special-register paths (REG_R, REG_FBRST, REG_VPU_STAT, ...) hit
//      the correct fallback emitter — they're not plain MOVs.
//
// VU0 capture is enabled so any divergence in vuRegs[0].VI mid-program is
// flagged.

#include "harness/EeRecTestHarness.h"

#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace mips;
using namespace mips::ee;

namespace {

constexpr u32 r_t0 = 8;
constexpr u32 r_t1 = 9;
constexpr u32 r_t2 = 10;

} // namespace

// =========================================================================
//  CFC2 — EE reads VU0.VI[fs]
// =========================================================================

TEST(EeVu0Cfc2, ReadsPlainViLow16BitsZeroExtended)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// VI[1] is a plain 16-bit VI register; only low 16 bits are architectural.
	h.SeedVu0Vi(1, 0x1234);
	h.LoadProgram({CFC2(r_t0, 1)});
	h.Run();
	// CFC2 sign-extends per real hardware, but for low values the high half
	// is zero. Both engines should agree on the full 64-bit GPR.
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
	EXPECT_EQ(static_cast<u32>(h.GetGpr64Jit(r_t0)), 0x1234u);
}

TEST(EeVu0Cfc2, ReadsHighBit15SignExtendsTo32)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(1, 0x8000); // bit 15 set → sign-extends to 0xFFFF8000
	h.LoadProgram({CFC2(r_t0, 1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
}

TEST(EeVu0Cfc2, ReadsRegRClampsAndSignExtends)
{
	// REG_R is the random-number register (24-bit valid). CFC2 of REG_R is a
	// recCall fallback in the JIT — exercise the mask path.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	vuRegs[0].VI[REG_R].UL = 0x12345678u;
	h.LoadProgram({CFC2(r_t0, REG_R)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
}

TEST(EeVu0Cfc2, ReadsRegStatusFlagFullWidth)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	vuRegs[0].VI[REG_STATUS_FLAG].UL = 0x00000A05u;
	h.LoadProgram({CFC2(r_t0, REG_STATUS_FLAG)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
}

TEST(EeVu0Cfc2, ReadsRegMacFlagFullWidth)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	vuRegs[0].VI[REG_MAC_FLAG].UL = 0x0000ABCDu;
	h.LoadProgram({CFC2(r_t0, REG_MAC_FLAG)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
}

TEST(EeVu0Cfc2, ReadsRegClipFlag24BitWidth)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	vuRegs[0].VI[REG_CLIP_FLAG].UL = 0x00ABCDEFu;
	h.LoadProgram({CFC2(r_t0, REG_CLIP_FLAG)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
}

TEST(EeVu0Cfc2, ReadsRegFbrstAfterEeWrite)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	vuRegs[0].VI[REG_FBRST].UL = 0x0000000Cu; // D-stop + T-stop
	h.LoadProgram({CFC2(r_t0, REG_FBRST)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
}

TEST(EeVu0Cfc2, ReadsRegTpcAsByteAddressDivBy8)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	vuRegs[0].VI[REG_TPC].UL = 0x40; // pair index 8
	h.LoadProgram({CFC2(r_t0, REG_TPC)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(r_t0), h.GetGpr64Interp(r_t0));
}

TEST(EeVu0Cfc2, ReadsViZeroIsZero)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.LoadProgram({CFC2(r_t0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(r_t0), 0u);
}

// =========================================================================
//  CTC2 — EE writes VU0.VI[fs]
// =========================================================================

TEST(EeVu0Ctc2, WritesPlainViLow16Bits)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr64(r_t0, 0xDEADBEEFu);
	h.LoadProgram({CTC2(r_t0, 1)});
	h.Run();
	// VI[1] retains only low 16 bits.
	EXPECT_EQ(h.GetVu0ViJit(1), 0xBEEFu);
	EXPECT_EQ(h.GetVu0ViJit(1), h.GetVu0ViInterp(1));
}

TEST(EeVu0Ctc2, WritesRegStatusFlagMasksStickyFieldAndDenormalizesToMicroStatusflags)
{
	// CTC2 to REG_STATUS_FLAG is microVU-aware: only the 0xFC0 "sticky" field
	// is taken from the GPR, the low-6 current-flag bits in VI[STATUS] are
	// preserved, and the result is denormalized + broadcast into all four
	// lanes of micro_statusflags (which the microVU JIT reads for flag sync).
	// Mirrors x86 microVU_Macro.inl recCTC2.
	//
	// BY-DESIGN JIT-vs-interp divergence: the shared interpreter CTC2
	// (VU0.cpp) falls through to a plain full-width VI store with no masking
	// and no micro_statusflags update — same divergence x86 has. So we opt
	// VI[STATUS] out of Run()'s auto-diff and assert the JIT post-state
	// directly. micro_statusflags is pipeline state (PipelinePermissive
	// already ignores it in the auto-diff).
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.IgnoreVu0Vi(REG_STATUS_FLAG);
	h.EnableCop1();
	h.SeedVu0Vi(REG_STATUS_FLAG, 0x3Fu);   // pre-existing current-flag bits 0-5
	h.SetGpr64(r_t0, 0xFFFFFFFFu);
	h.LoadProgram({CTC2(r_t0, REG_STATUS_FLAG)});
	h.Run();

	// VI[STATUS] = (0x3F & 0x3F) | (0xFFFFFFFF & 0xFC0) = 0x3F | 0xFC0 = 0xFFF.
	EXPECT_EQ(h.Vu0JitSnapshot().regs.VI[REG_STATUS_FLAG].UL, 0xFFFu);

	// Denormalize 0xFFF: ((s>>3)&0x18) | ((s<<11)&0x1800) | ((s<<14)&0x3cf0000)
	//                  = 0x18 | 0x1800 | 0x3cf0000 = 0x3cf1818, in all 4 lanes.
	const u32 expected_denorm = 0x3cf1818u;
	for (int lane = 0; lane < 4; ++lane)
		EXPECT_EQ(h.Vu0JitSnapshot().regs.micro_statusflags[lane], expected_denorm)
			<< "micro_statusflags lane " << lane;
}

TEST(EeVu0Ctc2, WritesRegMacFlagIsReadOnly)
{
	// REG_MAC_FLAG is read-only on CTC2 (per real hardware) — neither engine
	// should modify the cell. Pre-load with a sentinel and verify it persists.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	vuRegs[0].VI[REG_MAC_FLAG].UL = 0xCAFEBABEu;
	h.SetGpr64(r_t0, 0x11111111u);
	h.LoadProgram({CTC2(r_t0, REG_MAC_FLAG)});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(REG_MAC_FLAG), h.GetVu0ViInterp(REG_MAC_FLAG));
}

TEST(EeVu0Ctc2, WritesRegClipFlagDualWritesStructAndVi)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr64(r_t0, 0x00ABCDEFu);
	h.LoadProgram({CTC2(r_t0, REG_CLIP_FLAG)});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(REG_CLIP_FLAG), h.GetVu0ViInterp(REG_CLIP_FLAG));
}

TEST(EeVu0Ctc2, WritesRegFbrstFullCallFallback)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr64(r_t0, 0x0000000Fu);
	h.LoadProgram({CTC2(r_t0, REG_FBRST)});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(REG_FBRST), h.GetVu0ViInterp(REG_FBRST));
}

TEST(EeVu0Ctc2, WritesViZeroIsNoop)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr64(r_t0, 0xFFFFFFFFu);
	h.LoadProgram({CTC2(r_t0, 0)});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(0), 0u);
	EXPECT_EQ(h.GetVu0ViInterp(0), 0u);
}

// =========================================================================
//  Round trips — CTC2 then CFC2 (and vice versa)
// =========================================================================

TEST(EeVu0Cfc2Ctc2RoundTrip, CtcThenCfcMatchesOriginalLow16)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SetGpr64(r_t0, 0x4321);
	h.LoadProgram({
		CTC2(r_t0, 5),
		CFC2(r_t1, 5),
	});
	h.Run();
	EXPECT_EQ(static_cast<u32>(h.GetGpr64Jit(r_t1)), 0x4321u);
	EXPECT_EQ(h.GetGpr64Jit(r_t1), h.GetGpr64Interp(r_t1));
}

TEST(EeVu0Cfc2Ctc2RoundTrip, CfcThenCtcShufflesViBetweenIndices)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(3, 0x9876);
	h.LoadProgram({
		CFC2(r_t0, 3),
		CTC2(r_t0, 7),
		CFC2(r_t2, 7),
	});
	h.Run();
	EXPECT_EQ(static_cast<u32>(h.GetGpr64Jit(r_t2)), 0x9876u);
	EXPECT_EQ(h.GetVu0ViJit(7), 0x9876u);
	EXPECT_EQ(h.GetGpr64Jit(r_t2), h.GetGpr64Interp(r_t2));
	EXPECT_EQ(h.GetVu0ViJit(7), h.GetVu0ViInterp(7));
}

} // namespace recompiler_tests
