// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// EE-issued COP2 macro-mode VU ops + VCALLMS microprogram kick.
//
// Macro mode: an EE instruction with primary opcode COP2 (0x12) and CO=1
// (bit 25) executes one VU upper-pipe op against VU0.VF[]. The funct field
// (bits[5:0]) selects the op (mirrors the VU upper-pipe primary table:
// 0x28=VADD, 0x2A=VMUL, 0x2C=VSUB, 0x2F=VMINI, ...). The XYZW destination
// mask occupies bits[24:21]. Macro mode does the denormalize/normalize
// dance around every op — unlike microprogram mode where flags stay packed
// across the entire program. The macro-mode COP2 path emits NEON-direct
// codegen and is among the highest-fragility hand-written NEON in the tree.
//
// VCALLMS: COP2 + CO=1 + funct=0x38, with the start-PC-divided-by-8 in
// bits[20:6] (15-bit imm). Behavior: _vu0FinishMicro() drains the pending
// microprogram, copies macro-mode flags into microVU's instances, kicks
// the program at the supplied PC, runs to E-bit. Tests seed the
// microprogram via SeedVu0Microprogram + verify post-state.

#include "harness/EeRecTestHarness.h"

#include "VU.h"
#include "Config.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace mips;
using namespace mips::ee;
using namespace vu;

namespace {

constexpr u32 mask_xyzw = 0xF;
constexpr u32 mask_yz   = 0x6; // bits[23:22]: y(23) and z(22)
constexpr u32 mask_x    = 0x8; // bit[24]: x only

inline VuOp UpperOnly(u32 upper) { return VuOp{0, upper}; }

} // namespace

// =========================================================================
//  COP2 macro-mode VADD / VSUB / VMUL — basic arithmetic
// =========================================================================

TEST(EeVu0Cop2Macro, VaddXyzwSumsLanes)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SeedVu0Vf(2, 10.0f, 20.0f, 30.0f, 40.0f);
	h.LoadProgram({VADD_C2(mask_xyzw, /*fd*/3, /*fs*/1, /*ft*/2)});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), 11.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'y'), 22.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'z'), 33.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'w'), 44.0f);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), h.GetVu0VfBitsInterp(3, l));
}

TEST(EeVu0Cop2Macro, VsubXyzwDifferencesLanes)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(1, 100.0f, 200.0f, 300.0f, 400.0f);
	h.SeedVu0Vf(2,  10.0f,  20.0f,  30.0f,  40.0f);
	h.LoadProgram({VSUB_C2(mask_xyzw, 3, 1, 2)});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), 90.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'w'), 360.0f);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), h.GetVu0VfBitsInterp(3, l));
}

TEST(EeVu0Cop2Macro, VmulXyzwProductsLanes)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(1, 2.0f, 3.0f, 4.0f, 5.0f);
	h.SeedVu0Vf(2, 10.0f, 10.0f, 10.0f, 10.0f);
	h.LoadProgram({VMUL_C2(mask_xyzw, 3, 1, 2)});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), 20.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'w'), 50.0f);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), h.GetVu0VfBitsInterp(3, l));
}

TEST(EeVu0Cop2Macro, VaddMaskedYZOnlyTouchesYZ)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SeedVu0Vf(2, 10.0f, 20.0f, 30.0f, 40.0f);
	h.SeedVu0Vf(3, 99.0f, 99.0f, 99.0f, 99.0f);
	h.LoadProgram({VADD_C2(mask_yz, 3, 1, 2)});
	h.Run();
	// x and w preserved (99), y and z written.
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), 99.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'y'), 22.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'z'), 33.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'w'), 99.0f);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), h.GetVu0VfBitsInterp(3, l));
}

TEST(EeVu0Cop2Macro, VmaxKeepsLargerLane)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(1, 5.0f, 1.0f, 8.0f, 3.0f);
	h.SeedVu0Vf(2, 4.0f, 7.0f, 2.0f, 6.0f);
	h.LoadProgram({VMAX_C2(mask_xyzw, 3, 1, 2)});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), 5.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'y'), 7.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'z'), 8.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'w'), 6.0f);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), h.GetVu0VfBitsInterp(3, l));
}

TEST(EeVu0Cop2Macro, VminiKeepsSmallerLane)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(1, 5.0f, 1.0f, 8.0f, 3.0f);
	h.SeedVu0Vf(2, 4.0f, 7.0f, 2.0f, 6.0f);
	h.LoadProgram({VMINI_C2(mask_xyzw, 3, 1, 2)});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), 4.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'w'), 3.0f);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), h.GetVu0VfBitsInterp(3, l));
}

TEST(EeVu0Cop2Macro, MultipleMacroOpsBackToBack)
{
	// Three back-to-back macro ops — exercises the JIT's denormalize/normalize
	// transitions across instructions (the highest-fragility area of the macro emit path).
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SeedVu0Vf(2, 10.0f, 10.0f, 10.0f, 10.0f);
	h.LoadProgram({
		VADD_C2(mask_xyzw, 3, 1, 2), // vf3 = vf1 + vf2
		VMUL_C2(mask_xyzw, 4, 3, 2), // vf4 = vf3 * vf2
		VSUB_C2(mask_xyzw, 5, 4, 1), // vf5 = vf4 - vf1
	});
	h.Run();
	for (char l : {'x','y','z','w'})
	{
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), h.GetVu0VfBitsInterp(3, l));
		EXPECT_EQ(h.GetVu0VfBitsJit(4, l), h.GetVu0VfBitsInterp(4, l));
		EXPECT_EQ(h.GetVu0VfBitsJit(5, l), h.GetVu0VfBitsInterp(5, l));
	}
}

// =========================================================================
//  COP2 macro-mode VIADD / VISUB / VIAND / VIOR — integer-bank ops
// =========================================================================

TEST(EeVu0Cop2Macro, ViaddTouchesViBank)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(1, 100);
	h.SeedVu0Vi(2, 250);
	h.LoadProgram({VIADD_C2(/*id*/3, /*is*/1, /*it*/2)});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(3), 350u);
	EXPECT_EQ(h.GetVu0ViJit(3), h.GetVu0ViInterp(3));
}

TEST(EeVu0Cop2Macro, ViandIorChainProducesExpectedMask)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(1, 0xF0F0u);
	h.SeedVu0Vi(2, 0x0FF0u);
	h.SeedVu0Vi(3, 0x000Fu);
	h.LoadProgram({
		VIAND_C2(4, 1, 2),  // vi4 = 0xF0F0 & 0x0FF0 = 0x00F0
		VIOR_C2 (5, 4, 3),  // vi5 = 0x00F0 | 0x000F = 0x00FF
	});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(4), 0x00F0u);
	EXPECT_EQ(h.GetVu0ViJit(5), 0x00FFu);
	EXPECT_EQ(h.GetVu0ViJit(4), h.GetVu0ViInterp(4));
	EXPECT_EQ(h.GetVu0ViJit(5), h.GetVu0ViInterp(5));
}

// =========================================================================
//  Macro-flag visibility — CFC2 of MAC/STATUS/CLIP immediately after a
//  macro VADD. Exercises the per-op flag denormalize at every
//  macro-instruction boundary.
// =========================================================================

TEST(EeVu0Cop2Macro, CfcMacFlagAfterVaddSeesUpdatedFlags)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// VADD vf3 = vf0 + vf0 → all-zero result lanes → MAC = 0x000E.
	h.LoadProgram({
		VADD_C2(mask_xyzw, /*fd*/3, /*fs*/0, /*ft*/0),
		CFC2(/*rt*/8, REG_MAC_FLAG),
	});
	h.Run();
	// Both engines should see the same MAC value in the EE GPR.
	EXPECT_EQ(h.GetGpr64Jit(8), h.GetGpr64Interp(8));
}

TEST(EeVu0Cop2Macro, CfcStatusFlagAfterVaddAgrees)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.LoadProgram({
		VADD_C2(mask_xyzw, 3, 0, 0),
		CFC2(8, REG_STATUS_FLAG),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Jit(8), h.GetGpr64Interp(8));
}

// =========================================================================
//  VCALLMS — kick a microprogram from EE
// =========================================================================

TEST(EeVu0Vcallms, BasicMicroprogramRunsToEbit)
{
	// Microprogram at byte offset 0:
	//   pair 0: VADD vf2, vf1, vf0 (FMAC; sums lanes)
	//   pair 1: E-bit NOP (terminate after 1 useful pair + delay slot)
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(1, 1.5f, 2.5f, 3.5f, 4.5f);
	h.SeedVu0Microprogram(0, {
		VuOp{0, vu::VADD_U(vu::mask::xyzw, /*fd*/2, /*fs*/1, /*ft*/0)},
		EBitNopPair(),
	});
	h.LoadProgram({VCALLMS(/*startpc_div8*/0)});
	h.Run();
	// vf0 = (0,0,0,1.0f); vf1 + vf0 = (1.5, 2.5, 3.5, 5.5).
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(2, 'x'), 1.5f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(2, 'y'), 2.5f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(2, 'z'), 3.5f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(2, 'w'), 5.5f);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(2, l), h.GetVu0VfBitsInterp(2, l));
}

TEST(EeVu0Vcallms, MicroprogramAtNonZeroStartPC)
{
	// Microprogram at byte offset 16 (pair 2). Two NOPs at offsets 0..15
	// would dispatch as garbage, but VCALLMS jumps directly to the start.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(4, 7.0f, 7.0f, 7.0f, 7.0f);
	h.SeedVu0Microprogram(16, {
		VuOp{0, vu::VADD_U(vu::mask::xyzw, /*fd*/5, /*fs*/4, /*ft*/4)}, // vf5 = 14
		EBitNopPair(),
	});
	h.LoadProgram({VCALLMS(/*startpc_div8*/16/8)}); // pair index 2
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(5, 'x'), 14.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(5, 'w'), 14.0f);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(5, l), h.GetVu0VfBitsInterp(5, l));
}

TEST(EeVu0Vcallms, MicroprogramSetsViVisibleViaCfc2)
{
	// Microprogram writes vi3 = 0x1234 then E-bits. EE then CFC2's it.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Microprogram(0, {
		VuOp{vu::VIADDIU_L(/*it*/3, /*is*/0, 0x1234), vu::VNOP_U()},
		EBitNopPair(),
	});
	h.LoadProgram({
		VCALLMS(0),
		CFC2(/*rt*/8, /*fs*/3),
	});
	h.Run();
	EXPECT_EQ(static_cast<u32>(h.GetGpr64Jit(8)), 0x1234u);
	EXPECT_EQ(h.GetGpr64Jit(8), h.GetGpr64Interp(8));
}

TEST(EeVu0Vcallmsr, KickFromCmsar1Register)
{
	// VCALLMSR reads the start PC from VI[REG_CMSAR1] (* 8). Seed it.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(6, 100.0f, 200.0f, 300.0f, 400.0f);
	// Place microprogram at byte offset 0; CMSAR1 = 0 (pair index 0).
	vuRegs[0].VI[REG_CMSAR1].UL = 0;
	h.SeedVu0Microprogram(0, {
		VuOp{0, vu::VADD_U(vu::mask::xyzw, /*fd*/7, /*fs*/6, /*ft*/0)},
		EBitNopPair(),
	});
	h.LoadProgram({VCALLMSR()});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(7, 'x'), 100.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(7, 'w'), 401.0f); // + vf0.w (1.0)
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(7, l), h.GetVu0VfBitsInterp(7, l));
}

// =========================================================================
//  COP2 macro-mode VSQI — VF→Mem store with VI post-increment
// =========================================================================
//
// VSQI fs, vi(it)++: Mem[VI[it] * 16] = VF[fs]; VI[it]++. Exercises the
// COP2 wrapper that drives mVU_SQI from macro-mode dispatch. Witnesses:
//   1. VI[it] post-increment — direct via GetVu0Vi*.
//   2. VU0 mem write — indirect via a follow-on VLQI that reads the same
//      address into a VF reg (VLQI is still interp fallback so it's a
//      shared decoder between passes; the store is the only divergence
//      surface, so a roundtrip mismatch pins the JIT VSQI side).

TEST(EeVu0Cop2Macro, VsqiPostIncrementsViAndStoresFullQuad)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// VI[1] is the address-index for VSQI; VI[3] re-reads the same slot via
	// VLQI. Seed them to the same starting index (5 → byte offset 0x50).
	h.SeedVu0Vi(1, 5);
	h.SeedVu0Vi(3, 5);
	h.SeedVu0VfBits(2, 0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u);
	h.LoadProgram({
		VSQI_C2(mask_xyzw, /*fs*/2, /*it*/1),
		VLQI_C2(mask_xyzw, /*ft*/5, /*is*/3),
	});
	h.Run();
	// VI[1] post-incremented to 6 on both engines.
	EXPECT_EQ(h.GetVu0ViJit(1), 6u);
	EXPECT_EQ(h.GetVu0ViJit(1), h.GetVu0ViInterp(1));
	// VI[3] also post-incremented by VLQI.
	EXPECT_EQ(h.GetVu0ViJit(3), 6u);
	EXPECT_EQ(h.GetVu0ViJit(3), h.GetVu0ViInterp(3));
	// VF[5] round-trips the stored bits — JIT VSQI hit the right Mem slot
	// with the right lane order.
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'x'), 0x11111111u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'y'), 0x22222222u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'z'), 0x33333333u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'w'), 0x44444444u);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(5, l), h.GetVu0VfBitsInterp(5, l));
}

TEST(EeVu0Cop2Macro, VsqiSequentialStoresAdvanceVi)
{
	// Three back-to-back VSQI to verify VI post-inc accumulates correctly
	// (each VSQI uses VI[1] as the address and bumps it by 1) and each
	// stored quad lands at a distinct Mem slot.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(1, 10);
	h.SeedVu0Vi(3, 10); // VLQI readback starts at the same slot
	h.SeedVu0VfBits(2,  0xAAAAAAA1u, 0xAAAAAAA2u, 0xAAAAAAA3u, 0xAAAAAAA4u);
	h.SeedVu0VfBits(4,  0xBBBBBBB1u, 0xBBBBBBB2u, 0xBBBBBBB3u, 0xBBBBBBB4u);
	h.SeedVu0VfBits(6,  0xCCCCCCC1u, 0xCCCCCCC2u, 0xCCCCCCC3u, 0xCCCCCCC4u);
	h.LoadProgram({
		VSQI_C2(mask_xyzw, /*fs*/2, /*it*/1),
		VSQI_C2(mask_xyzw, /*fs*/4, /*it*/1),
		VSQI_C2(mask_xyzw, /*fs*/6, /*it*/1),
		// Read the three stored slots back into VF[7], VF[8], VF[9]
		VLQI_C2(mask_xyzw, /*ft*/7, /*is*/3),
		VLQI_C2(mask_xyzw, /*ft*/8, /*is*/3),
		VLQI_C2(mask_xyzw, /*ft*/9, /*is*/3),
	});
	h.Run();
	// VI[1] = 10 + 3 = 13
	EXPECT_EQ(h.GetVu0ViJit(1), 13u);
	EXPECT_EQ(h.GetVu0ViJit(1), h.GetVu0ViInterp(1));
	// Each slot round-trips its source quad in order.
	EXPECT_EQ(h.GetVu0VfBitsJit(7, 'x'), 0xAAAAAAA1u);
	EXPECT_EQ(h.GetVu0VfBitsJit(8, 'x'), 0xBBBBBBB1u);
	EXPECT_EQ(h.GetVu0VfBitsJit(9, 'x'), 0xCCCCCCC1u);
	for (u32 r : {7u, 8u, 9u})
		for (char l : {'x','y','z','w'})
			EXPECT_EQ(h.GetVu0VfBitsJit(r, l), h.GetVu0VfBitsInterp(r, l));
}

TEST(EeVu0Cop2Macro, VsqiMaskedPartialWritePreservesOtherLanes)
{
	// Partial-mask VSQI: only specific lanes are written. The mVU_SQI emit
	// path branches on full vs partial (Ldr+Merge+Str). Validates the
	// partial path.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// Pre-fill Mem[VI[1]*16] with a sentinel by storing VF[3] full-quad first.
	h.SeedVu0Vi(1, 20);
	h.SeedVu0Vi(3, 20); // readback
	h.SeedVu0VfBits(3,  0xDEAD0001u, 0xDEAD0002u, 0xDEAD0003u, 0xDEAD0004u);
	// Source for masked store: only Y+W should land.
	h.SeedVu0VfBits(2,  0xFFFFFFFFu, 0xCAFE2222u, 0xFFFFFFFFu, 0xCAFE4444u);
	h.LoadProgram({
		VSQI_C2(mask_xyzw, /*fs*/3, /*it*/1),       // seed Mem with sentinel; VI[1]=21
		VSQI_C2(/*y+w=*/0x5, /*fs*/2, /*it*/1),     // partial — only Y and W lanes
		// readback: VF[5]=full seed, VF[6]=masked merge
		VLQI_C2(mask_xyzw, /*ft*/5, /*is*/3),
		VLQI_C2(mask_xyzw, /*ft*/6, /*is*/3),
	});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(1), 22u);
	// VF[5] is the seed quad — full-mask store path.
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'x'), 0xDEAD0001u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'y'), 0xDEAD0002u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'z'), 0xDEAD0003u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'w'), 0xDEAD0004u);
	// VF[6] is the merged quad: X,Z = previous-slot sentinel (zero — fresh
	// Mem cell from VI[1] = 21), Y,W = from VF[2].
	EXPECT_EQ(h.GetVu0VfBitsJit(6, 'y'), 0xCAFE2222u);
	EXPECT_EQ(h.GetVu0VfBitsJit(6, 'w'), 0xCAFE4444u);
	for (u32 r : {5u, 6u})
		for (char l : {'x','y','z','w'})
			EXPECT_EQ(h.GetVu0VfBitsJit(r, l), h.GetVu0VfBitsInterp(r, l));
}

// =========================================================================
//  COP2 macro-mode VLQI / VLQD / VSQD — load with VI post-inc, load+store
//  with VI pre-dec
// =========================================================================
//
// All three reuse the same mVU emit pipeline as VSQI. The VLQI path also
// happens to be exercised as a readback witness in the VSQI tests above —
// the dedicated tests here pin VLQI's correctness on its own (masked
// partial load merge, multi-step pre-decrement).

TEST(EeVu0Cop2Macro, VlqiMaskedReadPreservesUntouchedLanesOfTargetVf)
{
	// VLQI with partial mask: only the masked lanes of VF[ft] are overwritten.
	// Exercises mVU_LQI's mVUloadMem partial-mask path.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(1, 30);
	h.SeedVu0Vi(3, 30);
	// Seed source quad in Mem via VSQI of VF[2].
	h.SeedVu0VfBits(2,  0xAAAA0001u, 0xAAAA0002u, 0xAAAA0003u, 0xAAAA0004u);
	// Pre-fill destination VF[7] with sentinel — masked lanes should survive.
	h.SeedVu0VfBits(7,  0xDEAD1111u, 0xDEAD2222u, 0xDEAD3333u, 0xDEAD4444u);
	h.LoadProgram({
		VSQI_C2(mask_xyzw, /*fs*/2, /*it*/1),     // Mem[30*16] = VF[2]; VI[1]=31
		VLQI_C2(/*x+z=*/0xA, /*ft*/7, /*is*/3),    // VF[7].xz <- Mem; .yw preserved; VI[3]=31
	});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(3), 31u);
	EXPECT_EQ(h.GetVu0VfBitsJit(7, 'x'), 0xAAAA0001u); // loaded
	EXPECT_EQ(h.GetVu0VfBitsJit(7, 'y'), 0xDEAD2222u); // preserved
	EXPECT_EQ(h.GetVu0VfBitsJit(7, 'z'), 0xAAAA0003u); // loaded
	EXPECT_EQ(h.GetVu0VfBitsJit(7, 'w'), 0xDEAD4444u); // preserved
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(7, l), h.GetVu0VfBitsInterp(7, l));
}

TEST(EeVu0Cop2Macro, VsqdPredecrementsViAndStoresFullQuad)
{
	// VSQD: --VI[it]; Mem[VI[it]*16] = VF[fs]. Pre-decrement is the
	// arithmetic difference vs VSQI; the rest of the emit pipeline is shared.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// VI[1] starts at 40 → VSQD decrements to 39, stores at slot 39.
	// VLQD readback decrements VI[3] from 40 to 39 then loads slot 39 → match.
	h.SeedVu0Vi(1, 40);
	h.SeedVu0Vi(3, 40);
	h.SeedVu0VfBits(2,  0xBBBB0001u, 0xBBBB0002u, 0xBBBB0003u, 0xBBBB0004u);
	h.LoadProgram({
		VSQD_C2(mask_xyzw, /*fs*/2, /*it*/1),  // VI[1] = 39; Mem[39*16] = VF[2]
		VLQD_C2(mask_xyzw, /*ft*/5, /*is*/3),  // VI[3] = 39; VF[5] = Mem[39*16]
	});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(1), 39u);
	EXPECT_EQ(h.GetVu0ViJit(3), 39u);
	EXPECT_EQ(h.GetVu0ViJit(1), h.GetVu0ViInterp(1));
	EXPECT_EQ(h.GetVu0ViJit(3), h.GetVu0ViInterp(3));
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'x'), 0xBBBB0001u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'w'), 0xBBBB0004u);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(5, l), h.GetVu0VfBitsInterp(5, l));
}

// =========================================================================
//  COP2 macro-mode VMTIR / VMFIR / VILWR / VISWR — integer-bank transfers
// =========================================================================
//
// VMTIR fsf, it, fs: VI[it] = VF[fs].lane(fsf) (extract a single 32-bit
//   lane, store low 16 bits to VI bank).
// VMFIR mask, ft, is: VF[ft].mask = sign_extend(VI[is]) to 32-bit and
//   broadcast across selected lanes.
// VILWR mask, it, is: VI[it] = Mem[VI[is] * 16].lane(mask) (lane index
//   comes from mask — pick the single set bit).
// VISWR mask, it, is: Mem[VI[it] * 16].lane(mask) = VI[is].

TEST(EeVu0Cop2Macro, VmtirCopiesLaneFromVfToVi)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(2, 0x11111111u, 0x22222222u, 0x33333333u, 0x44447777u);
	h.LoadProgram({
		VMTIR_C2(/*fsf=w*/3, /*it*/1, /*fs*/2),  // VI[1] = VF[2].w low16 = 0x7777
		VMTIR_C2(/*fsf=x*/0, /*it*/2, /*fs*/2),  // VI[2] = VF[2].x low16 = 0x1111
	});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(1), 0x7777u);
	EXPECT_EQ(h.GetVu0ViJit(2), 0x1111u);
	EXPECT_EQ(h.GetVu0ViJit(1), h.GetVu0ViInterp(1));
	EXPECT_EQ(h.GetVu0ViJit(2), h.GetVu0ViInterp(2));
}

TEST(EeVu0Cop2Macro, VmfirBroadcastsSignExtendedViToMaskedVfLanes)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// Negative VI value (16-bit sign-extends to 32-bit FFFFC000).
	h.SeedVu0Vi(1, 0xC000u);
	// Pre-fill VF[3] sentinel — masked lanes should survive.
	h.SeedVu0VfBits(3, 0xDEAD1111u, 0xDEAD2222u, 0xDEAD3333u, 0xDEAD4444u);
	h.LoadProgram({
		VMFIR_C2(/*y+z=*/0x6, /*ft*/3, /*is*/1),  // VF[3].yz <- 0xFFFFC000; .xw preserved.
	});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(3, 'x'), 0xDEAD1111u);
	EXPECT_EQ(h.GetVu0VfBitsJit(3, 'y'), 0xFFFFC000u);
	EXPECT_EQ(h.GetVu0VfBitsJit(3, 'z'), 0xFFFFC000u);
	EXPECT_EQ(h.GetVu0VfBitsJit(3, 'w'), 0xDEAD4444u);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), h.GetVu0VfBitsInterp(3, l));
}

TEST(EeVu0Cop2Macro, VilwrViswrRoundtripViThroughMem)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(1, 0x1234u);  // value (interp: _It_ = value source)
	h.SeedVu0Vi(2, 60);       // VISWR addr base (interp: _Is_ = addr)
	h.SeedVu0Vi(3, 60);       // VILWR addr (same slot)
	h.LoadProgram({
		// Mem[VI[2]*16].xyzw = VI[1] = 0x1234. _It_ = value, _Is_ = addr.
		VISWR_C2(mask_xyzw, /*it*/1, /*is*/2),
		// VI[5] = Mem[VI[3]*16].x = 0x1234. ILWR's mask picks one lane.
		VILWR_C2(/*x=*/0x8, /*it*/5, /*is*/3),
	});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(5), 0x1234u);
	EXPECT_EQ(h.GetVu0ViJit(5), h.GetVu0ViInterp(5));
}

TEST(EeVu0Cop2Macro, VsqdVlqdInterleavedAdvanceCorrectly)
{
	// Two pre-decrement stores followed by two pre-decrement loads. Stresses
	// VI tracking across multiple SPEC2 dispatches in one block.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(1, 50);   // store ptr (decrements to 49, then 48)
	h.SeedVu0Vi(3, 50);   // load ptr (decrements to 49, then 48)
	h.SeedVu0VfBits(2,  0xC0DE0001u, 0xC0DE0002u, 0xC0DE0003u, 0xC0DE0004u);
	h.SeedVu0VfBits(4,  0xCAFE0001u, 0xCAFE0002u, 0xCAFE0003u, 0xCAFE0004u);
	h.LoadProgram({
		VSQD_C2(mask_xyzw, /*fs*/2, /*it*/1),  // VI[1]=49; Mem[49*16]=VF[2]
		VSQD_C2(mask_xyzw, /*fs*/4, /*it*/1),  // VI[1]=48; Mem[48*16]=VF[4]
		VLQD_C2(mask_xyzw, /*ft*/7, /*is*/3),  // VI[3]=49; VF[7]=Mem[49*16] (VF[2])
		VLQD_C2(mask_xyzw, /*ft*/8, /*is*/3),  // VI[3]=48; VF[8]=Mem[48*16] (VF[4])
	});
	h.Run();
	EXPECT_EQ(h.GetVu0ViJit(1), 48u);
	EXPECT_EQ(h.GetVu0ViJit(3), 48u);
	// VF[7] should hold the FIRST stored quad (slot 49) = VF[2].
	EXPECT_EQ(h.GetVu0VfBitsJit(7, 'x'), 0xC0DE0001u);
	// VF[8] should hold the SECOND stored quad (slot 48) = VF[4].
	EXPECT_EQ(h.GetVu0VfBitsJit(8, 'x'), 0xCAFE0001u);
	for (u32 r : {7u, 8u})
		for (char l : {'x','y','z','w'})
			EXPECT_EQ(h.GetVu0VfBitsJit(r, l), h.GetVu0VfBitsInterp(r, l));
}

// ---- R-register / LFSR group (RINIT, RGET, RNEXT, RXOR) ----
// VRINIT  fsf, fs: VI[REG_R] = 0x3F800000 | (VF[fs].lane(fsf) & 0x7FFFFF).
// VRGET   mask, ft: VF[ft].mask = VI[REG_R] (full 32-bit, broadcast).
// VRNEXT  mask, ft: advance LFSR R, then same as VRGET.
// VRXOR   fsf, fs: VI[REG_R] = 0x3F800000 | ((R ^ VF[fs].lane(fsf)) & 0x7FFFFF).

TEST(EeVu0Cop2Macro, VrinitVrgetRoundtripsRegisterR)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// VF[2].y mantissa = 0x12345 (low 23 bits). RINIT keeps only low 23.
	h.SeedVu0VfBits(2, 0xDEAD0000u, 0x80012345u, 0xDEAD0000u, 0xDEAD0000u);
	h.LoadProgram({
		VRINIT_C2(/*fsf=y*/1, /*fs*/2),
		VRGET_C2 (mask_xyzw, /*ft*/3),
	});
	h.Run();
	const u32 expected_r = 0x3F800000u | (0x80012345u & 0x007FFFFFu);
	EXPECT_EQ(h.GetVu0ViJit(REG_R), expected_r);
	EXPECT_EQ(h.GetVu0ViJit(REG_R), h.GetVu0ViInterp(REG_R));
	for (char l : {'x','y','z','w'})
	{
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), expected_r);
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), h.GetVu0VfBitsInterp(3, l));
	}
}

TEST(EeVu0Cop2Macro, VrnextAdvancesLfsrAndBroadcastsAgreesWithInterp)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(2, 0u, 0x000ABCDEu, 0u, 0u);
	h.LoadProgram({
		VRINIT_C2(/*fsf=y*/1, /*fs*/2),
		VRNEXT_C2(mask_xyzw, /*ft*/4),
		VRNEXT_C2(mask_xyzw, /*ft*/5),
	});
	h.Run();
	// LFSR is bit-exact across hosts; demand JIT == interp.
	EXPECT_EQ(h.GetVu0ViJit(REG_R), h.GetVu0ViInterp(REG_R));
	for (u32 r : {4u, 5u})
		for (char l : {'x','y','z','w'})
			EXPECT_EQ(h.GetVu0VfBitsJit(r, l), h.GetVu0VfBitsInterp(r, l));
	// VF[5] should hold the LATER (advanced twice) R; VF[4] the once-advanced.
	EXPECT_NE(h.GetVu0VfBitsJit(4, 'x'), h.GetVu0VfBitsJit(5, 'x'));
}

TEST(EeVu0Cop2Macro, VrxorXorsMantissaIntoRegisterR)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(2, 0u, 0u, 0u, 0x800ABCDEu);  // .w mantissa = 0xABCDE
	h.SeedVu0VfBits(3, 0x00055555u, 0u, 0u, 0u);  // .x mantissa = 0x55555
	h.LoadProgram({
		VRINIT_C2(/*fsf=w*/3, /*fs*/2),  // R = 0x3F800000 | 0x0ABCDE
		VRXOR_C2 (/*fsf=x*/0, /*fs*/3),  // R = 0x3F800000 | ((0x0ABCDE ^ 0x55555) & 0x7FFFFF)
	});
	h.Run();
	const u32 init_mantissa = 0x800ABCDEu & 0x007FFFFFu;       // 0x0ABCDE
	const u32 xor_with      = 0x00055555u & 0x007FFFFFu;       // 0x055555
	const u32 expected_r    = 0x3F800000u | (init_mantissa ^ xor_with);
	EXPECT_EQ(h.GetVu0ViJit(REG_R), expected_r);
	EXPECT_EQ(h.GetVu0ViJit(REG_R), h.GetVu0ViInterp(REG_R));
}

// ---- VU0->VU1 access path (mVUaddrFix bit 0x400 branch) ----
//
// VU0 macro-mode load/store with a VI address that has bit 0x400 set must
// route through VU1.VF[] (see VUops.cpp _vuLQ + GET_VU_MEM: byte addr bit
// 0x4000, quadword addr bit 0x400). The arm64 mVUaddrFix VU0->VU1 path
// emits a byte/u128-unit conversion that is easy to get wrong — a silent
// SEGV is the failure mode when VSQI/VLQI route through this emitter.

TEST(EeVu0Cop2Macro, VlqiAcrossVu0Vu1AccessBoundaryLoadsVu1Vf)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// VI[1] = 0x40A: bit 0x400 set => VU0 access into VU1.VF[10].
	h.SeedVu0Vi(1, 0x40Au);
	// Seed VU1.VF[10] directly — both passes read the same source state
	// (VLQI is read-only on VU1; no need for capture_vu1_ snapshotting).
	h.SeedVu1VfBits(10, 0xCAFEBABEu, 0xDEADBEEFu, 0x12345678u, 0x9ABCDEF0u);
	// Pre-fill VF[5] sentinel so partial-mask bugs would show up too.
	h.SeedVu0VfBits(5, 0x55555555u, 0x55555555u, 0x55555555u, 0x55555555u);
	h.LoadProgram({
		VLQI_C2(mask_xyzw, /*ft*/5, /*is*/1),
	});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'x'), 0xCAFEBABEu);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'y'), 0xDEADBEEFu);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'z'), 0x12345678u);
	EXPECT_EQ(h.GetVu0VfBitsJit(5, 'w'), 0x9ABCDEF0u);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(5, l), h.GetVu0VfBitsInterp(5, l));
	// VI[1] should still post-increment to 0x40B even on the VU1 access path.
	EXPECT_EQ(h.GetVu0ViJit(1), 0x40Bu);
	EXPECT_EQ(h.GetVu0ViJit(1), h.GetVu0ViInterp(1));
}

// =========================================================================
//  COP2 macro-mode VOPMSUB / VOPMULA — outer-product cross math
// =========================================================================
//
// PS2 OPMSUB:  VF[fd].xyz = ACC.xyz - VF[fs].yzx * VF[ft].zxy   (W untouched)
// PS2 OPMULA:  ACC.xyz    =           VF[fs].yzx * VF[ft].zxy   (ACC.w untouched)
//
// Hardware always writes XYZ lanes only — the W lane of the destination is
// preserved regardless of what the instruction's dest mask encodes. The
// PS2 SDK disassembler hard-codes ".xyz" for these ops; the interpreter
// (VUops.cpp _vuOPMSUB / _vuOPMULA) only writes XYZ.

TEST(EeVu0Cop2Macro, VopmsubXyzCrossProductMatchesInterp)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// fd seeded with a sentinel W to verify it is preserved.
	h.SeedVu0VfBits(3, 0x11111111u, 0x22222222u, 0x33333333u, 0xCAFEBABEu);
	// fs, ft, ACC chosen so the cross-product math is non-degenerate.
	h.SeedVu0Vf(1, /*x*/1.0f, /*y*/2.0f, /*z*/3.0f, /*w*/4.0f);
	h.SeedVu0Vf(2, /*x*/5.0f, /*y*/6.0f, /*z*/7.0f, /*w*/8.0f);
	h.SeedVu0Acc(/*x*/100.0f, /*y*/200.0f, /*z*/300.0f, /*w*/400.0f);

	// Encode with dest=XYZ (the standard, what SDK assemblers emit).
	h.LoadProgram({VOPMSUB_C2(/*mask*/0xE, /*fd*/3, /*fs*/1, /*ft*/2)});
	h.Run();

	// Hardware semantics:
	//   fd.x = ACC.x - fs.y * ft.z = 100 - 2*7 = 86
	//   fd.y = ACC.y - fs.z * ft.x = 200 - 3*5 = 185
	//   fd.z = ACC.z - fs.x * ft.y = 300 - 1*6 = 294
	//   fd.w = preserved sentinel
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), 86.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'y'), 185.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'z'), 294.0f);
	EXPECT_EQ(h.GetVu0VfBitsJit(3, 'w'), 0xCAFEBABEu);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), h.GetVu0VfBitsInterp(3, l));
}

TEST(EeVu0Cop2Macro, VopmsubDestXyzwStillPreservesW)
{
	// Some games / hand-written code emit VOPMSUB with dest=XYZW even though
	// the assembler convention is XYZ. PS2 hardware ignores the W bit of the
	// dest field for OPMSUB — only XYZ are ever written. The interpreter
	// matches this (VUops.cpp:866-868 only writes i.x/i.y/i.z, never i.w).
	// JIT must too.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(3, 0x11111111u, 0x22222222u, 0x33333333u, 0xCAFEBABEu);
	h.SeedVu0Vf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SeedVu0Vf(2, 5.0f, 6.0f, 7.0f, 8.0f);
	h.SeedVu0Acc(100.0f, 200.0f, 300.0f, 400.0f);

	h.LoadProgram({VOPMSUB_C2(/*mask*/0xF, /*fd*/3, /*fs*/1, /*ft*/2)});
	h.Run();

	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), 86.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'y'), 185.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'z'), 294.0f);
	// W must be preserved even though mask said XYZW.
	EXPECT_EQ(h.GetVu0VfBitsJit(3, 'w'), 0xCAFEBABEu);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), h.GetVu0VfBitsInterp(3, l));
}

TEST(EeVu0Cop2Macro, VopmulaXyzWritesAccLeavesAccWUntouched)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SeedVu0Vf(2, 5.0f, 6.0f, 7.0f, 8.0f);
	// Sentinel W in ACC — expected to be preserved.
	h.SeedVu0AccBits(0x10000000u, 0x20000000u, 0x30000000u, 0xCAFEBABEu);

	h.LoadProgram({VOPMULA_C2(/*mask*/0xE, /*fs*/1, /*ft*/2)});
	h.Run();

	// ACC.x = fs.y * ft.z = 2*7 = 14
	// ACC.y = fs.z * ft.x = 3*5 = 15
	// ACC.z = fs.x * ft.y = 1*6 =  6
	// ACC.w = preserved.
	const u32 acc_x_bits = h.GetVu0AccBitsJit('x');
	float acc_x;
	std::memcpy(&acc_x, &acc_x_bits, sizeof(acc_x));
	EXPECT_FLOAT_EQ(acc_x, 14.0f);
	EXPECT_EQ(h.GetVu0AccBitsJit('w'), 0xCAFEBABEu);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0AccBitsJit(l), h.GetVu0AccBitsInterp(l));
}

TEST(EeVu0Cop2Macro, VopmulaDestXyzwStillPreservesAccW)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SeedVu0Vf(2, 5.0f, 6.0f, 7.0f, 8.0f);
	h.SeedVu0AccBits(0x10000000u, 0x20000000u, 0x30000000u, 0xCAFEBABEu);

	h.LoadProgram({VOPMULA_C2(/*mask*/0xF, /*fs*/1, /*ft*/2)});
	h.Run();

	EXPECT_EQ(h.GetVu0AccBitsJit('w'), 0xCAFEBABEu);
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0AccBitsJit(l), h.GetVu0AccBitsInterp(l));
}

// =========================================================================
//  VFTOIx NaN saturation
// =========================================================================
//
// COP2 macro-mode VFTOI0/4/12/15 must saturate NaN inputs to a sign-based
// INT_MAX / INT_MIN, matching mVU_FTOIx and the interpreter
// (floatToInt -> (sign ? 0x80000000 : 0x7fffffff)).
// NEON Fcvtzs returns 0 for NaN; the macro path must emit a sign-based
// fixup after Fcvtzs to match interp on NaN lanes. Finite overflow and
// ±Inf already saturate correctly inside Fcvtzs.

TEST(EeVu0Cop2Macro, Vftoi0SaturatesNanSignBased)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// x: +NaN, y: -NaN, z: +2.5 (-> 2 trunc), w: -2.5 (-> -2 trunc)
	h.SeedVu0VfBits(1, 0x7FC00000u, 0xFFC00000u, 0x40200000u, 0xC0200000u);
	h.LoadProgram({VFTOI0_C2(mask_xyzw, /*ft=dst*/2, /*fs=src*/1)});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(2, 'x'), 0x7FFFFFFFu);          // +NaN -> INT_MAX
	EXPECT_EQ(h.GetVu0VfBitsJit(2, 'y'), 0x80000000u);          // -NaN -> INT_MIN
	EXPECT_EQ(h.GetVu0VfBitsJit(2, 'z'), 2u);
	EXPECT_EQ(h.GetVu0VfBitsJit(2, 'w'), static_cast<u32>(-2)); // 0xFFFFFFFE
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(2, l), h.GetVu0VfBitsInterp(2, l));
}

TEST(EeVu0Cop2Macro, Vftoi4SaturatesNanAndScalesFinite)
{
	// VFTOI4 scales by 2^4 before truncating; the NaN fixup is identical and
	// exercises the helper's fbits!=0 branch.
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// x: +NaN, y: -NaN, z: +2.5 (*16 = 40), w: -2.5 (*16 = -40)
	h.SeedVu0VfBits(1, 0x7FC00000u, 0xFFC00000u, 0x40200000u, 0xC0200000u);
	h.LoadProgram({VFTOI4_C2(mask_xyzw, /*ft=dst*/2, /*fs=src*/1)});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(2, 'x'), 0x7FFFFFFFu);
	EXPECT_EQ(h.GetVu0VfBitsJit(2, 'y'), 0x80000000u);
	EXPECT_EQ(h.GetVu0VfBitsJit(2, 'z'), 40u);
	EXPECT_EQ(h.GetVu0VfBitsJit(2, 'w'), static_cast<u32>(-40));
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(2, l), h.GetVu0VfBitsInterp(2, l));
}

// =========================================================================
//  VCLIP micro_clipflags broadcast
// =========================================================================
//
// COP2 macro-mode VCLIP writes the new clip flag to VU0.clipflag and
// VI[REG_CLIP_FLAG], but must also broadcast it into all four lanes of
// micro_clipflags: a subsequent VU0 microprogram loads its clip-flag
// instances directly from micro_clipflags at prologue.
// Without the broadcast those instances are stale (pre-VCLIP). micro_clipflags
// is pipeline state (PipelinePermissive ignores it in the auto-diff), so the
// JIT post-state is asserted directly.

TEST(EeVu0Cop2Macro, VclipBroadcastsClipflagToMicroClipflags)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	// fs.xyz large positive magnitudes vs ft.w = 1.0 bound → several clip bits.
	h.SeedVu0VfBits(1, 0x40000000u /*2.0 x*/, 0x40800000u /*4.0 y*/,
	                   0x41000000u /*8.0 z*/, 0u);
	h.SeedVu0VfBits(2, 0u, 0u, 0u, 0x3F800000u /*1.0 w bound*/);
	// Sentinel so the strip-fix check is deterministic: without the broadcast
	// micro_clipflags keeps this value rather than the computed clip flag.
	for (int i = 0; i < 4; ++i)
		vuRegs[0].micro_clipflags[i] = 0xDEADBEEFu;
	// Reset the internal running clip (VCLIP shifts it <<6 then ORs new bits);
	// otherwise a prior VCLIP test's residue contaminates this one. Mirrors the
	// explicit resets in VclipSignedIntegerCompareWithNaNLane / ...DenormalBoundAndShift.
	vuRegs[0].clipflag = 0u;

	h.LoadProgram({VCLIP_C2(/*ft*/2, /*fs*/1)});
	h.Run();

	const u32 clip = h.Vu0JitSnapshot().regs.VI[REG_CLIP_FLAG].UL;
	ASSERT_NE(clip, 0u) << "test program must produce a nonzero clip flag";
	ASSERT_NE(clip, 0xDEADBEEFu);
	for (int lane = 0; lane < 4; ++lane)
		EXPECT_EQ(h.Vu0JitSnapshot().regs.micro_clipflags[lane], clip)
			<< "micro_clipflags lane " << lane;
}

// =========================================================================
//  VCLIP vectorized signed-integer clip test
// =========================================================================
//
// VCLIP packs six clip bits (+x@0,-x@1,+y@2,-y@3,+z@4,-z@5) from the signed-
// integer comparison (s32)(fs.lane ^ {0,0x80000000}) > value, where value =
// |ft.w| (or 0x007FFFFF when ft.w is denormal). The arm64 codegen vectorizes
// this with two NEON Cmgt (SCMGT) compares; this MUST stay an INTEGER compare,
// not an FP compare — the interp oracle compares bit patterns as s32, so a NaN
// lane participates (NaN's exponent=0xFF makes a positive-NaN a large positive
// s32). An FP-compare rewrite (Fcmgt) would return false for NaN and diverge.
//
// Discriminator mixes a positive clip, a negative clip, and a +NaN lane:
//   ft.w = 1.0 -> value = 0x3F800000
//   fs.x = +2.0 (0x40000000)  -> s32 > value           -> +x  (bit0, 0x01)
//   fs.y = -2.0 (0xC0000000)  -> (fs^sign) > value      -> -y  (bit3, 0x08)
//   fs.z = +NaN (0x7FC00000)  -> s32 > value (integer!) -> +z  (bit4, 0x10)
// expected clip = 0x19.

TEST(EeVu0Cop2Macro, VclipSignedIntegerCompareWithNaNLane)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(1, 0x40000000u /*+2.0 x*/, 0xC0000000u /*-2.0 y*/,
	                   0x7FC00000u /*+NaN z*/, 0u);
	h.SeedVu0VfBits(2, 0u, 0u, 0u, 0x3F800000u /*1.0 w bound*/);
	vuRegs[0].clipflag = 0u; // internal running clip (<<6'd by VCLIP); not VI[REG_CLIP_FLAG]
	h.LoadProgram({VCLIP_C2(/*ft*/2, /*fs*/1)});
	h.Run();
	EXPECT_EQ(h.Vu0JitSnapshot().regs.VI[REG_CLIP_FLAG].UL, 0x19u);
	EXPECT_EQ(h.Vu0JitSnapshot().regs.VI[REG_CLIP_FLAG].UL,
	          h.Vu0InterpSnapshot().regs.VI[REG_CLIP_FLAG].UL);
}

// Denormal ft.w forces value = 0x007FFFFF, and the previous clip flag shifts
// left by 6 before the new bits merge. fs.x = 0x00800000 (smallest normal,
// 0x00800000 > 0x007FFFFF as s32) sets +x; fs.y/z below the bound set nothing.
// Seeds a prior clip so the <<6 path is exercised: 0x01 -> 0x40, then |+x(0x01).

TEST(EeVu0Cop2Macro, VclipDenormalBoundAndShift)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(1, 0x00800000u /*x just over denormal bound*/,
	                   0x00400000u /*y denormal, below*/, 0u, 0u);
	h.SeedVu0VfBits(2, 0u, 0u, 0u, 0x00000001u /*denormal w*/);
	vuRegs[0].clipflag = 0x01u; // prior internal flag, shifted <<6 -> 0x40
	h.LoadProgram({VCLIP_C2(/*ft*/2, /*fs*/1)});
	h.Run();
	EXPECT_EQ(h.Vu0JitSnapshot().regs.VI[REG_CLIP_FLAG].UL, 0x41u); // 0x40 | +x
	EXPECT_EQ(h.Vu0JitSnapshot().regs.VI[REG_CLIP_FLAG].UL,
	          h.Vu0InterpSnapshot().regs.VI[REG_CLIP_FLAG].UL);
}

// =========================================================================
//  VMULAw broadcast-Ft pre-clamp on full mask
// =========================================================================
//
// MULAw with all four dest lanes active clamps the broadcast Ft before the
// multiply (x86 mVU_MULAw — the "Superman - Shadow Of Apokolips" gamefix).
// Discriminator: Fs=0, Ft.w=+Inf. With the pre-clamp
// Ft -> FLT_MAX and 0*FLT_MAX = 0, matching the interp (which clamps both
// operands via vuDouble). Without it 0*Inf = NaN, which the result-clamp turns
// into +FLT_MAX (0x7f7fffff) — a JIT-vs-interp divergence.

TEST(EeVu0Cop2Macro, VmulawClampsBroadcastFtOnFullMask)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(1, 0u, 0u, 0u, 0u);          // fs = 0
	h.SeedVu0VfBits(2, 0u, 0u, 0u, 0x7F800000u); // ft.w = +Inf (broadcast lane)
	h.LoadProgram({VMULAw_C2(mask_xyzw, /*fs*/1, /*ft*/2)});
	h.Run();
	for (char l : {'x','y','z','w'})
	{
		EXPECT_EQ(h.GetVu0AccBitsJit(l), 0u) << "ACC lane " << l;
		EXPECT_EQ(h.GetVu0AccBitsJit(l), h.GetVu0AccBitsInterp(l)) << "lane " << l;
	}
}

// =========================================================================
//  VMADDx broadcast-Fs pre-clamp
// =========================================================================
//
// MADDx/y/z/w clamp the Fs operand before the multiply: x86 mVU_MADDx passes
// cFs, and the interp routes Fs through vuDouble. The arm64 COP2 macro must
// do the same. Discriminator: Fs=+Inf, Ft.x=0, ACC=0.
// With the pre-clamp Fs -> FLT_MAX and FLT_MAX*0 = 0, so fd = ACC + 0 = 0,
// matching interp. Without it Inf*0 = NaN, which the result-clamp folds to
// +FLT_MAX (0x7f7fffff) — a JIT-vs-interp divergence.
//
// (MSUBx/y/z/w pass clampType=0 in x86 mVU_FMACd, so Fs is intentionally NOT
// clamped there — a shared by-design JIT divergence, not touched by this fix.)

TEST(EeVu0Cop2Macro, VmaddxClampsBroadcastFs)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(1, 0x7F800000u, 0x7F800000u, 0x7F800000u, 0x7F800000u); // fs = +Inf
	h.SeedVu0VfBits(2, 0u, 0u, 0u, 0u); // ft.x = 0 (broadcast lane)
	h.SeedVu0AccBits(0u, 0u, 0u, 0u);   // acc = 0
	h.LoadProgram({VMADDx_C2(mask_xyzw, /*fd*/3, /*fs*/1, /*ft*/2)});
	h.Run();
	for (char l : {'x','y','z','w'})
	{
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), 0u) << "fd lane " << l;
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), h.GetVu0VfBitsInterp(3, l)) << "lane " << l;
	}
}

// =========================================================================
//  VMULy broadcast-Fs pre-clamp
// =========================================================================
//
// MULx/y/z/w clamp Fs before the multiply on EVERY mask, and additionally clamp
// the broadcast Ft when all four lanes are active: x86 mVU_MULx passes
// (_XYZW_PS)?(cFs|cFt):cFs, and the interp routes both operands through
// vuDouble (TOTA / Disgaea / Ice Age on VU0). The arm64 COP2 macro must clamp
// Fs before the multiply, not only the result.
//
// This uses a PARTIAL (x-only) dest mask so only the always-on cFs clamp fires
// (cFt is gated on the full mask), pinning the behaviour that distinguishes MUL
// from ADD/SUB. Discriminator: fs.x=+Inf, ft.y=0 (MULy broadcasts lane y). With
// the pre-clamp fs.x -> FLT_MAX and FLT_MAX*0 = 0, matching interp. Without it
// Inf*0 = NaN, which the result-clamp folds to +FLT_MAX (0x7f7fffff) — a
// JIT-vs-interp divergence.

TEST(EeVu0Cop2Macro, VmulyClampsBroadcastFsOnPartialMask)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(1, 0x7F800000u, 0u, 0u, 0u); // fs.x = +Inf
	h.SeedVu0VfBits(2, 0u, 0u, 0u, 0u);          // ft.y = 0 (broadcast lane)
	h.SeedVu0VfBits(3, 0u, 0u, 0u, 0u);          // fd = 0
	h.LoadProgram({VMULy_C2(mask_x, /*fd*/3, /*fs*/1, /*ft*/2)});
	h.Run();
	EXPECT_EQ(h.GetVu0VfBitsJit(3, 'x'), 0u) << "fd.x";
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0VfBitsJit(3, l), h.GetVu0VfBitsInterp(3, l)) << "lane " << l;
}

// =========================================================================
//  VMULAx/y/z broadcast-Fs pre-clamp to ACC
// =========================================================================
//
// The accumulator multiply variants MULAx/y/z clamp Fs before the multiply on
// EVERY mask (x86 mVU_MULAx/y/z pass cFs; the interp routes both operands
// through vuDouble). The COP2 macro path must clamp Fs, not only the result.
// Same shape as the MUL→fd fix but writing ACC instead of VF[fd]. Discriminator:
// fs.x=+Inf, ft.z=0 (MULAz broadcasts lane z), partial x-only mask so only the
// always-on cFs clamp fires. With the pre-clamp fs.x -> FLT_MAX, FLT_MAX*0 = 0
// (interp). Without it Inf*0 = NaN, result-clamped to +FLT_MAX — a divergence.

TEST(EeVu0Cop2Macro, VmulazClampsBroadcastFsOnPartialMask)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(1, 0x7F800000u, 0u, 0u, 0u); // fs.x = +Inf
	h.SeedVu0VfBits(2, 0u, 0u, 0u, 0u);          // ft.z = 0 (broadcast lane)
	h.SeedVu0AccBits(0u, 0u, 0u, 0u);            // acc = 0
	h.LoadProgram({VMULAz_C2(mask_x, /*fs*/1, /*ft*/2)});
	h.Run();
	EXPECT_EQ(h.GetVu0AccBitsJit('x'), 0u) << "acc.x";
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0AccBitsJit(l), h.GetVu0AccBitsInterp(l)) << "lane " << l;
}

// MULAw additionally clamps Fs (not only the broadcast Ft on full mask).
// x86 mVU_MULAw passes cFs always plus cFt on the full mask. Discriminator
// pins the cFs gap: fs.x=+Inf,
// ft.w=0, partial x-only mask (cFt does not fire). With cFs fs.x -> FLT_MAX and
// FLT_MAX*0 = 0 (interp); without it Inf*0 = NaN -> +FLT_MAX.

TEST(EeVu0Cop2Macro, VmulawClampsBroadcastFsOnPartialMask)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0VfBits(1, 0x7F800000u, 0u, 0u, 0u); // fs.x = +Inf
	h.SeedVu0VfBits(2, 0u, 0u, 0u, 0u);          // ft.w = 0 (broadcast lane)
	h.SeedVu0AccBits(0u, 0u, 0u, 0u);            // acc = 0
	h.LoadProgram({VMULAw_C2(mask_x, /*fs*/1, /*ft*/2)});
	h.Run();
	EXPECT_EQ(h.GetVu0AccBitsJit('x'), 0u) << "acc.x";
	for (char l : {'x','y','z','w'})
		EXPECT_EQ(h.GetVu0AccBitsJit(l), h.GetVu0AccBitsInterp(l)) << "lane " << l;
}

// =========================================================================
//  COP2 condition branches — BC2F / BC2T / BC2FL / BC2TL.
//  Native codegen for BC2x: CP2COND is bit 8 of VU0.VI[REG_VPU_STAT]
//  (COP2.cpp:11): BC2F taken when clear,
//  BC2T when set; FL/TL squash the delay slot when not taken. Run() diffs
//  jit-vs-interp and ExpectGpr64 pins both, so each test triple-checks.
// =========================================================================
namespace {
constexpr u32 kCop2Park = RecompilerTestEnvironment::kParkingPc;
constexpr s16 kCop2TakenOffset = 5;       // branch@0x00, offset 5 → target 0x18
constexpr u32 kVpuStatCp2 = 0x100;        // CP2COND = bit 8

// Cross-test isolation + seed. EnableVu0Capture only resets VF[0]/VI[0], so a
// preceding macro-VU fixture can leave VU0 with a stale pending micro / ebit /
// VPU_STAT, which the JIT branch's flush drains (clearing VPU_STAT) while the
// interp path doesn't — a spurious VU0 post-state diff. ZeroGlobals(0) gives a
// clean idle VU0 before seeding CP2COND. Mirrors the EnableVu1VifCapture reset.
inline void SetupCop2Branch(EeRecTestHarness& h, u32 vpu_stat)
{
	VuSnapshot::ZeroGlobals(0);
	h.EnableCop1();
	h.EnableVu0Capture();
	h.SeedVu0Vi(REG_VPU_STAT, vpu_stat);
	// The branch only READS CP2COND (bit 8). But the JIT dispatcher
	// (recEeExecuteBlock) runs the EE event-test/VU0-sync path on its way to
	// the parking PC and recomputes VPU_STAT, whereas the interp single-stepper
	// doesn't — so the post-run VPU_STAT diverges even though the branch
	// arithmetic (which marker runs) is identical and correct. Exclude VPU_STAT
	// from the VU0 diff; branch correctness is pinned by ExpectGpr64 + DiffEe.
	h.IgnoreVu0Vi(REG_VPU_STAT);
}

// branch@0x00 / NOP delay@0x04 / ADDIU v0,1 (not-taken)@0x08 / J park /
// NOP / NOP / ADDIU v0,2 (taken)@0x18 / J park / NOP
inline void LoadCop2BranchLayout(EeRecTestHarness& h, u32 branch_instr)
{
	h.LoadProgramNoTerm({
		branch_instr, NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kCop2Park), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kCop2Park), NOP,
	});
}
} // namespace

TEST(EeVu0Cop2Macro, Bc2tTakenWhenCondSet)
{
	EeRecTestHarness h;
	SetupCop2Branch(h, kVpuStatCp2);          // CP2COND = 1 → BC2T taken
	LoadCop2BranchLayout(h, BC2T(kCop2TakenOffset));
	h.Run();
	h.ExpectGpr64(reg::v0, 2ull);
}

TEST(EeVu0Cop2Macro, Bc2tNotTakenWhenCondClear)
{
	EeRecTestHarness h;
	SetupCop2Branch(h, 0);                     // CP2COND = 0 → BC2T not taken
	LoadCop2BranchLayout(h, BC2T(kCop2TakenOffset));
	h.Run();
	h.ExpectGpr64(reg::v0, 1ull);
}

TEST(EeVu0Cop2Macro, Bc2fTakenWhenCondClear)
{
	EeRecTestHarness h;
	SetupCop2Branch(h, 0);                     // CP2COND = 0 → BC2F taken
	LoadCop2BranchLayout(h, BC2F(kCop2TakenOffset));
	h.Run();
	h.ExpectGpr64(reg::v0, 2ull);
}

TEST(EeVu0Cop2Macro, Bc2fNotTakenWhenCondSet)
{
	EeRecTestHarness h;
	SetupCop2Branch(h, kVpuStatCp2);          // CP2COND = 1 → BC2F not taken
	LoadCop2BranchLayout(h, BC2F(kCop2TakenOffset));
	h.Run();
	h.ExpectGpr64(reg::v0, 1ull);
}

TEST(EeVu0Cop2Macro, Bc2tlTakenExecutesDelaySlot)
{
	// Likely + taken: delay slot DOES execute. Put a marker in the delay slot.
	EeRecTestHarness h;
	SetupCop2Branch(h, kVpuStatCp2);          // CP2COND = 1 → BC2TL taken
	h.SetGpr(reg::t0, 7);
	h.LoadProgramNoTerm({
		BC2TL(kCop2TakenOffset),
		ADDIU(reg::t0, reg::zero, 99),        // delay slot — runs when taken
		ADDIU(reg::v0, reg::zero, 1), J(kCop2Park), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kCop2Park), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 2ull);             // taken
	h.ExpectGpr64(reg::t0, 99ull);            // delay slot executed
}

TEST(EeVu0Cop2Macro, Bc2tlNotTakenSquashesDelaySlot)
{
	// Likely + not taken: delay slot is SQUASHED. t0 keeps its seeded value.
	EeRecTestHarness h;
	SetupCop2Branch(h, 0);                     // CP2COND = 0 → BC2TL not taken
	h.SetGpr(reg::t0, 7);
	h.LoadProgramNoTerm({
		BC2TL(kCop2TakenOffset),
		ADDIU(reg::t0, reg::zero, 99),        // delay slot — squashed when not taken
		ADDIU(reg::v0, reg::zero, 1), J(kCop2Park), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kCop2Park), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 1ull);             // not taken
	h.ExpectGpr64(reg::t0, 7ull);             // delay slot squashed
}

// =========================================================================
//  vuFlagHack status-liveness skip (bc3729c93)
//
//  setupMacroOp_arm64/endMacroOp_arm64 skip the status-flag denormalize/
//  normalize when vuFlagHack is on AND the COP2FlagHackPass marks the op's
//  status output dead (no CFC2 consumes it). The recompiler test environment
//  pins vuFlagHack OFF for JIT-vs-interp determinism (a dead-flag skip would
//  otherwise diverge from the always-accurate interpreter), so these two
//  tests opt back into the production default to cover the gate directly.
// =========================================================================

namespace {
struct ScopedFlagHack
{
	bool saved;
	explicit ScopedFlagHack(bool on) : saved(EmuConfig.Speedhacks.vuFlagHack) { EmuConfig.Speedhacks.vuFlagHack = on; }
	~ScopedFlagHack() { EmuConfig.Speedhacks.vuFlagHack = saved; }
};
} // namespace

TEST(EeVu0Cop2MacroFlagHack, StatusLiveAcrossCfc2NotSkipped)
{
	// VADD result (-4, 0, 4, 5): lane x sets the sign bit, lane y the zero bit.
	// The following CFC2 reads VI[REG_STATUS_FLAG], so COP2FlagHackPass marks the
	// VADD EEINST_COP2_STATUS_FLAG -> cop2StatusFlagLive() keeps the normalize and
	// the JIT-emitted status must match the interpreter.
	EeRecTestHarness h;
	ScopedFlagHack flagHack(true);
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(1, -5.0f, 0.0f, 3.0f, 4.0f);
	h.SeedVu0Vf(2,  1.0f, 0.0f, 1.0f, 1.0f);
	h.LoadProgram({
		VADD_C2(mask_xyzw, /*fd*/3, /*fs*/1, /*ft*/2),
		CFC2(reg::t0, REG_STATUS_FLAG),
	});
	h.Run(); // auto-diffs JIT vs interp, including the CFC2-read status in t0
	EXPECT_EQ(h.GetGpr64Jit(reg::t0), h.GetGpr64Interp(reg::t0));
	EXPECT_NE(h.GetGpr64Jit(reg::t0), 0u); // status was actually populated
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), -4.0f);
}

TEST(EeVu0Cop2MacroFlagHack, StatusDeadStandaloneSkipsButKeepsResult)
{
	// No CFC2 reads the status, so EEINST_COP2_STATUS_FLAG stays clear and the
	// denormalize/normalize dance is skipped. The arithmetic must be unaffected.
	// Status is dead, so the interpreter (which always updates it) legitimately
	// diverges — run the JIT in isolation and assert only the live result.
	EeRecTestHarness h;
	ScopedFlagHack flagHack(true);
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vf(1, -5.0f, 0.0f, 3.0f, 4.0f);
	h.SeedVu0Vf(2,  1.0f, 0.0f, 1.0f, 1.0f);
	h.LoadProgram({VADD_C2(mask_xyzw, /*fd*/3, /*fs*/1, /*ft*/2)});
	h.RunJitNoDiff();
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), -4.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'y'),  0.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'z'),  4.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'w'),  5.0f);
}

// =========================================================================
//  Pending-VU0-micro drain on DIV-unit / no-op COP2 macro ops
// =========================================================================
//
// x86 syncs EVERY COP2-CO special op at the dispatch wrapper: recCOP2_SPEC1
// (microVU_Macro.inl) emits an analysis-gated mVUFinishVU0() before the
// table call. The arm64 port moved that sync per-op (setupMacroOp_arm64, or
// a direct cop2EmitConditionalSync at the top of the hand-rolled bodies) —
// and six hand-rolled ops missed it: VDIV, VSQRT, VRSQRT, VCLIP, VNOP,
// VWAITQ. COP2MicroFinishPass marks the FIRST COP2-CO op after a
// VCALLMS/VU0-store with EEINST_COP2_FINISH_VU0 and then clears its pending
// state — so an op that doesn't consume the mark doesn't merely delay the
// drain, it DROPS it for the rest of the block.
//
// Recipe: VCALLMS kicks a microprogram longer than the 16-cycle kickstart
// window (CalculateMinRunCycles floor; the harness CpuVU0 is the
// interpreter, which honors the bound), so the micro is still mid-flight
// when the next COP2 op executes. The micro's LAST act (idempotent — Run()
// replays it in both passes) writes a VF operand of the op under test; the
// op must drain the micro BEFORE reading its operands. Interp oracle:
// COP2_SPECIAL in VU0.cpp calls _vu0FinishMicro() unconditionally. Q
// results are captured into a VF inside the same block via VADDq so the
// assertions are immune to any later drain.

namespace {

// Micro at PC 0: 32 NOP pairs (outlasts the 16-cycle kickstart), then
// vf_dst = vf_src + vf_src, then E-bit. Idempotent across re-runs because
// vf_src is never written.
void SeedPendingMicroDoubling(EeRecTestHarness& h, u32 vf_dst, u32 vf_src)
{
	u32 off = 0;
	for (int i = 0; i < 32; i++, off += 8)
		h.SeedVu0Microprogram(off, {NopPair()});
	h.SeedVu0Microprogram(off, {
		VuOp{0, vu::VADD_U(vu::mask::xyzw, vf_dst, vf_src, vf_src)},
		EBitNopPair(),
		NopPair(), // explicit E-bit delay pair — keep micro mem deterministic
	});
}

} // namespace

TEST(EeVu0Cop2PendingMicroSync, VdivDrainsPendingMicroBeforeReadingOperands)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(REG_VPU_STAT, 0); // control regs survive EnableVu0Capture — start clean
	h.SeedVu0Vf(1,  2.0f,  2.0f,  2.0f,  2.0f);
	h.SeedVu0Vf(2, 32.0f, 32.0f, 32.0f, 32.0f); // stale divisor; micro rewrites it to 4.0
	SeedPendingMicroDoubling(h, /*vf_dst*/2, /*vf_src*/1);
	h.LoadProgram({
		VCALLMS(0),
		VDIV_C2(/*fsf*/0, /*ftf*/0, /*fs*/1, /*ft*/2), // Q = vf1.x / vf2.x
		VADDq_C2(mask_x, /*fd*/3, /*fs*/0),            // vf3.x = vf0.x + Q = Q
	});
	h.Run();
	// The drain must land vf2 = 4.0 BEFORE VDIV reads it: Q = 2/4 = 0.5.
	// Unsynced JIT reads the stale 32.0 → Q = 0.0625.
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(2, 'x'), 4.0f);
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), 0.5f);
	EXPECT_EQ(h.GetVu0VfBitsJit(2, 'x'), h.GetVu0VfBitsInterp(2, 'x'));
	EXPECT_EQ(h.GetVu0VfBitsJit(3, 'x'), h.GetVu0VfBitsInterp(3, 'x'));
}

TEST(EeVu0Cop2PendingMicroSync, VsqrtDrainsPendingMicroBeforeReadingOperands)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(REG_VPU_STAT, 0);
	h.SeedVu0Vf(1,  2.0f,  2.0f,  2.0f,  2.0f);
	h.SeedVu0Vf(2, 32.0f, 32.0f, 32.0f, 32.0f); // micro rewrites to 4.0
	SeedPendingMicroDoubling(h, /*vf_dst*/2, /*vf_src*/1);
	h.LoadProgram({
		VCALLMS(0),
		VSQRT_C2(/*ftf*/0, /*ft*/2),        // Q = sqrt(|vf2.x|)
		VADDq_C2(mask_x, /*fd*/3, /*fs*/0), // vf3.x = Q
	});
	h.Run();
	// Drained: Q = sqrt(4) = 2. Unsynced: sqrt(32) ≈ 5.657.
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), 2.0f);
	EXPECT_EQ(h.GetVu0VfBitsJit(2, 'x'), h.GetVu0VfBitsInterp(2, 'x'));
	EXPECT_EQ(h.GetVu0VfBitsJit(3, 'x'), h.GetVu0VfBitsInterp(3, 'x'));
}

TEST(EeVu0Cop2PendingMicroSync, VrsqrtDrainsPendingMicroBeforeReadingOperands)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(REG_VPU_STAT, 0);
	h.SeedVu0Vf(1,  2.0f,  2.0f,  2.0f,  2.0f);
	h.SeedVu0Vf(2, 32.0f, 32.0f, 32.0f, 32.0f); // micro rewrites to 4.0
	SeedPendingMicroDoubling(h, /*vf_dst*/2, /*vf_src*/1);
	h.LoadProgram({
		VCALLMS(0),
		VRSQRT_C2(/*fsf*/0, /*ftf*/0, /*fs*/1, /*ft*/2), // Q = vf1.x / sqrt(|vf2.x|)
		VADDq_C2(mask_x, /*fd*/3, /*fs*/0),              // vf3.x = Q
	});
	h.Run();
	// Drained: Q = 2/sqrt(4) = 1. Unsynced: 2/sqrt(32) ≈ 0.354.
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), 1.0f);
	EXPECT_EQ(h.GetVu0VfBitsJit(2, 'x'), h.GetVu0VfBitsInterp(2, 'x'));
	EXPECT_EQ(h.GetVu0VfBitsJit(3, 'x'), h.GetVu0VfBitsInterp(3, 'x'));
}

TEST(EeVu0Cop2PendingMicroSync, VclipDrainsPendingMicroBeforeReadingOperands)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(REG_VPU_STAT, 0);
	// Micro rewrites vf1 (VCLIP's fs) from below-bound to above-bound:
	// stale vf1 = 0.1 (no clip bits) → drained vf1 = 2*0.75 = 1.5 > |1.0|.
	h.SeedVu0Vf(1, 0.1f, 0.1f, 0.1f, 0.0f);
	h.SeedVu0Vf(3, 0.75f, 0.75f, 0.75f, 0.0f);
	h.SeedVu0VfBits(2, 0u, 0u, 0u, 0x3F800000u /*1.0 w bound*/);
	vuRegs[0].clipflag = 0u; // internal running clip (<<6'd by VCLIP)
	SeedPendingMicroDoubling(h, /*vf_dst*/1, /*vf_src*/3);
	h.LoadProgram({
		VCALLMS(0),
		VCLIP_C2(/*ft*/2, /*fs*/1),
	});
	h.Run();
	// Drained: +x,+y,+z set → 0b010101 = 0x15. Unsynced: 0x00.
	EXPECT_EQ(h.GetVu0ViJit(REG_CLIP_FLAG), 0x15u);
	EXPECT_EQ(h.GetVu0ViJit(REG_CLIP_FLAG), h.GetVu0ViInterp(REG_CLIP_FLAG));
	EXPECT_EQ(h.GetVu0VfBitsJit(1, 'x'), h.GetVu0VfBitsInterp(1, 'x'));
}

// =========================================================================
//  Single-lane dest-mask sweep (fast-path coverage)
// =========================================================================
//
// cop2ApplyDestMaskExplicit / cop2ApplyDestMaskACCExplicit special-case
// popcount(xyzw)==1: a single 32-bit lane store (Str s-reg for lane 0,
// Add+St1 for lanes 1-3) instead of the load+mask+BSL+store merge. These
// sweeps pin the lane mapping (mask bit3=x→lane0 ... bit0=w→lane3) and the
// untouched-lane preservation for every single-lane mask, for both the VF
// and ACC variants. A wrong lane map or a clobbered neighbor lane goes red.

TEST(EeVu0Cop2Macro, SingleLaneDestMaskSweepVadd)
{
	struct { u32 mask; char lane; float expect; } cases[] = {
		{0x8, 'x', 11.0f}, // 1+10
		{0x4, 'y', 22.0f}, // 2+20
		{0x2, 'z', 33.0f}, // 3+30
		{0x1, 'w', 44.0f}, // 4+40
	};
	for (const auto& c : cases)
	{
		SCOPED_TRACE(testing::Message() << "mask=0x" << std::hex << c.mask << " lane=" << c.lane);
		EeRecTestHarness h;
		h.EnableVu0Capture();
		h.EnableCop1();
		h.SeedVu0Vf(1, 1.0f, 2.0f, 3.0f, 4.0f);
		h.SeedVu0Vf(2, 10.0f, 20.0f, 30.0f, 40.0f);
		h.SeedVu0Vf(3, 99.0f, 98.0f, 97.0f, 96.0f); // per-lane sentinels
		h.LoadProgram({VADD_C2(c.mask, /*fd*/3, /*fs*/1, /*ft*/2)});
		h.Run();
		const float sentinels[4] = {99.0f, 98.0f, 97.0f, 96.0f};
		const char lanes[4] = {'x', 'y', 'z', 'w'};
		for (int i = 0; i < 4; i++)
		{
			if (lanes[i] == c.lane)
				EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, lanes[i]), c.expect);
			else
				EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, lanes[i]), sentinels[i]);
			EXPECT_EQ(h.GetVu0VfBitsJit(3, lanes[i]), h.GetVu0VfBitsInterp(3, lanes[i]));
		}
	}
}

TEST(EeVu0Cop2Macro, SingleLaneDestMaskSweepAccVmulax)
{
	// VMULAx: ACC.masked_lane = fs.lane * ft.x (broadcast). ft.x = 10.
	struct { u32 mask; char lane; float expect; } cases[] = {
		{0x8, 'x', 10.0f},  // 1*10
		{0x4, 'y', 20.0f},  // 2*10
		{0x2, 'z', 30.0f},  // 3*10
		{0x1, 'w', 40.0f},  // 4*10
	};
	for (const auto& c : cases)
	{
		SCOPED_TRACE(testing::Message() << "mask=0x" << std::hex << c.mask << " lane=" << c.lane);
		EeRecTestHarness h;
		h.EnableVu0Capture();
		h.EnableCop1();
		h.SeedVu0Vf(1, 1.0f, 2.0f, 3.0f, 4.0f);
		h.SeedVu0Vf(2, 10.0f, 0.0f, 0.0f, 0.0f);
		h.SeedVu0AccBits(0x42C60000u, 0x42C40000u, 0x42C20000u, 0x42C00000u); // 99,98,97,96
		h.LoadProgram({VMULAx_C2(c.mask, /*fs*/1, /*ft*/2)});
		h.Run();
		const u32 sentinels[4] = {0x42C60000u, 0x42C40000u, 0x42C20000u, 0x42C00000u};
		const char lanes[4] = {'x', 'y', 'z', 'w'};
		for (int i = 0; i < 4; i++)
		{
			if (lanes[i] == c.lane)
			{
				const u32 bits = h.GetVu0AccBitsJit(lanes[i]);
				float got;
				std::memcpy(&got, &bits, sizeof(got));
				EXPECT_FLOAT_EQ(got, c.expect);
			}
			else
			{
				EXPECT_EQ(h.GetVu0AccBitsJit(lanes[i]), sentinels[i]);
			}
			EXPECT_EQ(h.GetVu0AccBitsJit(lanes[i]), h.GetVu0AccBitsInterp(lanes[i]));
		}
	}
}

// VNOP/VWAITQ are architectural no-ops, but they still consume the
// EEINST_COP2_FINISH_VU0 mark: if the analysis puts the mark on them and
// they drop it, the following COP2 ops in the block run UNSYNCED (the pass
// cleared its pending state when it placed the mark).

TEST(EeVu0Cop2PendingMicroSync, VnopConsumesFinishMarkAndDrainsPendingMicro)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(REG_VPU_STAT, 0);
	h.SeedVu0Vf(1,  2.0f,  2.0f,  2.0f,  2.0f);
	h.SeedVu0Vf(2, 32.0f, 32.0f, 32.0f, 32.0f); // micro rewrites to 4.0
	SeedPendingMicroDoubling(h, /*vf_dst*/2, /*vf_src*/1);
	h.LoadProgram({
		VCALLMS(0),
		VNOP_C2(),                                 // carries the FINISH mark
		VADD_C2(mask_x, /*fd*/3, /*fs*/2, /*ft*/0), // vf3.x = vf2.x + 0 (unmarked)
	});
	h.Run();
	// Drained at VNOP: vf3.x = 4.0. Mark dropped: vf3.x = stale 32.0.
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), 4.0f);
	EXPECT_EQ(h.GetVu0VfBitsJit(2, 'x'), h.GetVu0VfBitsInterp(2, 'x'));
	EXPECT_EQ(h.GetVu0VfBitsJit(3, 'x'), h.GetVu0VfBitsInterp(3, 'x'));
}

TEST(EeVu0Cop2PendingMicroSync, VwaitqConsumesFinishMarkAndDrainsPendingMicro)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.EnableCop1();
	h.SeedVu0Vi(REG_VPU_STAT, 0);
	h.SeedVu0Vf(1,  2.0f,  2.0f,  2.0f,  2.0f);
	h.SeedVu0Vf(2, 32.0f, 32.0f, 32.0f, 32.0f); // micro rewrites to 4.0
	SeedPendingMicroDoubling(h, /*vf_dst*/2, /*vf_src*/1);
	h.LoadProgram({
		VCALLMS(0),
		VWAITQ_C2(),                               // carries the FINISH mark
		VADD_C2(mask_x, /*fd*/3, /*fs*/2, /*ft*/0), // vf3.x = vf2.x + 0 (unmarked)
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVu0VfJit(3, 'x'), 4.0f);
	EXPECT_EQ(h.GetVu0VfBitsJit(2, 'x'), h.GetVu0VfBitsInterp(2, 'x'));
	EXPECT_EQ(h.GetVu0VfBitsJit(3, 'x'), h.GetVu0VfBitsInterp(3, 'x'));
}

} // namespace recompiler_tests
