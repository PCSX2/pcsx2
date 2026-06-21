// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU0 lower-pipe non-Q ALU DiffJitVsInterp suite. Covers VMOVE,
// VMR32, VMTIR, VMFIR; the eight VFTOIx / VITOFx fixed-point conversions;
// and VLQI / VSQI / VLQD / VSQD pre/post-increment loads/stores. Q-pipe
// ops (VDIV / VSQRT / VRSQRT + VWAITQ) live in the dedicated Q-pipeline
// suite so the timing-fragile cycle-distance permutations stay isolated.

#include "harness/VuTestHarness.h"

#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

// Pair the supplied lower-word op with an upper-word NOP (FD_11 sub 0x0B).
// No I-bit needed since the lower already carries a real opcode.
inline VuOp LowerOnly(u32 lower) { return VuOp{lower, VNOP_U()}; }

} // namespace

// -------- VMOVE (lower-pipe register copy) --------

TEST(Vu0AluLower, VmoveXyzwCopiesAllLanes)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.5f, 2.5f, 3.5f, 4.5f);
	h.SetVf(2, 99.0f, 99.0f, 99.0f, 99.0f);
	h.LoadProgram({
		LowerOnly(VMOVE_L(mask::xyzw, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'x'), 1.5f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'y'), 2.5f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'z'), 3.5f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'w'), 4.5f);
}

TEST(Vu0AluLower, VmoveMaskedLeavesUnmaskedLanes)
{
	VuTestHarness h(0);
	h.SetVf(1, 11.0f, 22.0f, 33.0f, 44.0f);
	h.SetVf(2, -1.0f, -2.0f, -3.0f, -4.0f);
	h.LoadProgram({
		LowerOnly(VMOVE_L(mask::y | mask::w, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'x'), -1.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'y'), 22.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'z'), -3.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'w'), 44.0f);
}

TEST(Vu0AluLower, VmoveSelfIsIdentity)
{
	VuTestHarness h(0);
	h.SetVf(5, 7.0f, -8.0f, 9.0f, -10.0f);
	h.LoadProgram({
		LowerOnly(VMOVE_L(mask::xyzw, vf::vf5, vf::vf5)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(5, 'x'), 7.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(5, 'y'), -8.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(5, 'z'), 9.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(5, 'w'), -10.0f);
}

// -------- VMR32 (rotate xyzw → yzwx) --------

TEST(Vu0AluLower, VmR32RotatesByOneLane)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(2, 99.0f, 99.0f, 99.0f, 99.0f);
	h.LoadProgram({
		LowerOnly(VMR32_L(mask::xyzw, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	// Per _vuMR32: ft.x = fs.y, ft.y = fs.z, ft.z = fs.w, ft.w = fs.x.
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'x'), 2.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'y'), 3.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'z'), 4.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'w'), 1.0f);
}

TEST(Vu0AluLower, VmR32MaskedZWritesOnlyZ)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(2, 100.0f, 200.0f, 300.0f, 400.0f);
	h.LoadProgram({
		LowerOnly(VMR32_L(mask::z, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'x'), 100.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'y'), 200.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'z'), 4.0f);     // pre-rotation src lane = w
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'w'), 400.0f);
}

TEST(Vu0AluLower, VmR32SelfAliasingRotatesIntoSelf)
{
	// fs == ft: the rotate must fully complete from the saved input —
	// regalloc has to either spill or use a temp, otherwise lane writes
	// stomp later lane reads. Classic aliasing-bug shape.
	VuTestHarness h(0);
	h.SetVf(3, 10.0f, 20.0f, 30.0f, 40.0f);
	h.LoadProgram({
		LowerOnly(VMR32_L(mask::xyzw, vf::vf3, vf::vf3)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 20.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 30.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 40.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 10.0f);
}

// -------- VMTIR / VMFIR (VF ↔ VI scalar transfer) --------

TEST(Vu0AluLower, VmtirCopiesLaneToVi)
{
	// Per _vuMTIR: VI[it].US[0] = u16(fs.f[fsf]) — the *low 16 bits of the
	// IEEE-754 representation* of the chosen lane. NOT a float→int cast.
	VuTestHarness h(0);
	h.SetVfBits(1, 0xCAFEBABE, 0xDEADBEEF, 0x12345678, 0xAAAA5555);
	// Pick fsf=2 (z lane) → VI[1] should get low 16 bits of 0x12345678 = 0x5678.
	h.LoadProgram({
		LowerOnly(VMTIR_L(vi::vi1, vf::vf1, /*fsf=*/2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(1), 0x5678u);
}

TEST(Vu0AluLower, VmfirSignExtendsViIntoAllLanes)
{
	// Per _vuMFIR: ft.SL[lane] = (s32)VI[is].SS[0] for each masked lane.
	// VI is 16-bit signed; result populates the s32 slot of each VF lane.
	VuTestHarness h(0);
	h.SetVi(2, 0xFFFFu);  // -1 as s16
	h.SetVfBits(3, 0u, 0u, 0u, 0u);
	h.LoadProgram({
		LowerOnly(VMFIR_L(mask::xyzw, vf::vf3, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	// VI[2] = -1 (s16) → (s32)-1 = 0xFFFFFFFF stored as 32-bit pattern in VF.
	EXPECT_EQ(h.GetVfBitsJit(3, 'x'), 0xFFFFFFFFu);
	EXPECT_EQ(h.GetVfBitsJit(3, 'y'), 0xFFFFFFFFu);
	EXPECT_EQ(h.GetVfBitsJit(3, 'z'), 0xFFFFFFFFu);
	EXPECT_EQ(h.GetVfBitsJit(3, 'w'), 0xFFFFFFFFu);
}

TEST(Vu0AluLower, VmfirMaskedOnlyTouchesSelectedLanes)
{
	VuTestHarness h(0);
	h.SetVi(4, 0x1234u);
	h.SetVfBits(5, 0xAAAA1111u, 0xBBBB2222u, 0xCCCC3333u, 0xDDDD4444u);
	h.LoadProgram({
		LowerOnly(VMFIR_L(mask::y, vf::vf5, vi::vi4)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(5, 'x'), 0xAAAA1111u);
	EXPECT_EQ(h.GetVfBitsJit(5, 'y'), 0x00001234u);
	EXPECT_EQ(h.GetVfBitsJit(5, 'z'), 0xCCCC3333u);
	EXPECT_EQ(h.GetVfBitsJit(5, 'w'), 0xDDDD4444u);
}

// -------- VITOFx / VFTOIx (fixed-point conversions; UPPER pipe) --------

namespace {
inline VuOp UpperOnlyPair(u32 upper) { return IBit(VuOp{VLitZero(), upper}); }
} // namespace

TEST(Vu0AluLower, VitofZeroMatchesIntegerCast)
{
	// VITOF0: just (float)(s32)bits — no scaling.
	VuTestHarness h(0);
	h.SetVfBits(1, 0u, 1u, static_cast<u32>(-1), 0x80000000u);
	h.LoadProgram({
		UpperOnlyPair(VITOF0_U(mask::xyzw, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	// Diff already polices the architectural answer; sanity-check the JIT side.
	EXPECT_EQ(h.GetVfBitsJit(2, 'x'), h.GetVfBitsInterp(2, 'x'));
	EXPECT_EQ(h.GetVfBitsJit(2, 'y'), h.GetVfBitsInterp(2, 'y'));
	EXPECT_EQ(h.GetVfBitsJit(2, 'z'), h.GetVfBitsInterp(2, 'z'));
	EXPECT_EQ(h.GetVfBitsJit(2, 'w'), h.GetVfBitsInterp(2, 'w'));
}

TEST(Vu0AluLower, VitofFourScalesByExp4)
{
	// VITOF4: (float)bits / 2^4 — used for 28.4 fixed-point reads.
	VuTestHarness h(0);
	h.SetVfBits(1, 16u, 32u, 64u, 256u);
	h.LoadProgram({
		UpperOnlyPair(VITOF4_U(mask::xyzw, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'x'), 1.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'y'), 2.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'z'), 4.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'w'), 16.0f);
}

TEST(Vu0AluLower, Vitof12ScalesByExp12)
{
	VuTestHarness h(0);
	h.SetVfBits(1, 4096u, 8192u, 16384u, 32768u);
	h.LoadProgram({
		UpperOnlyPair(VITOF12_U(mask::xyzw, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'x'), 1.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'y'), 2.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'z'), 4.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'w'), 8.0f);
}

TEST(Vu0AluLower, Vitof15ScalesByExp15)
{
	VuTestHarness h(0);
	h.SetVfBits(1, 32768u, 65536u, 131072u, 262144u);
	h.LoadProgram({
		UpperOnlyPair(VITOF15_U(mask::xyzw, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'x'), 1.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'y'), 2.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'z'), 4.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'w'), 8.0f);
}

TEST(Vu0AluLower, Vftoi0RoundsTowardZero)
{
	// VFTOI0: (s32)f — truncate toward zero.
	VuTestHarness h(0);
	h.SetVf(1, 1.7f, -1.7f, 2.999f, -0.5f);
	h.LoadProgram({
		UpperOnlyPair(VFTOI0_U(mask::xyzw, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'x')), 1);
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'y')), -1);
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'z')), 2);
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'w')), 0);
}

TEST(Vu0AluLower, Vftoi4ScalesUp16XThenTruncates)
{
	// VFTOI4: (s32)(f * 16). Used for 28.4 fixed-point writes.
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 0.5f, 0.0625f, -2.5f);
	h.LoadProgram({
		UpperOnlyPair(VFTOI4_U(mask::xyzw, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'x')), 16);
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'y')), 8);
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'z')), 1);
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'w')), -40);
}

TEST(Vu0AluLower, Vftoi12ScalesUp4096X)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 0.5f, 0.0f, -1.0f);
	h.LoadProgram({
		UpperOnlyPair(VFTOI12_U(mask::xyzw, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'x')), 4096);
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'y')), 2048);
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'z')), 0);
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'w')), -4096);
}

TEST(Vu0AluLower, Vftoi15ScalesUp32768X)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 0.5f, 0.25f, -1.0f);
	h.LoadProgram({
		UpperOnlyPair(VFTOI15_U(mask::xyzw, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'x')), 32768);
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'y')), 16384);
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'z')), 8192);
	EXPECT_EQ(static_cast<s32>(h.GetVfBitsJit(2, 'w')), -32768);
}

TEST(Vu0AluLower, Vftoi0SaturatesAtIntMaxOnHugeFloat)
{
	// Per floatToInt<>: any float whose exponent is ≥ 0x4F (≈ 2^31) saturates
	// to ±INT_MAX/INT_MIN. A bug here would be the runaway int-cast undefined
	// behaviour seen on the EE side (MTSA float-mul cast).
	VuTestHarness h(0);
	h.SetVfBits(1, 0x7F7FFFFFu, 0xFF7FFFFFu, 0u, 0u);  // ±FLT_MAX, 0
	h.LoadProgram({
		UpperOnlyPair(VFTOI0_U(mask::x | mask::y, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(2, 'x'), 0x7FFFFFFFu);  // INT_MAX
	EXPECT_EQ(h.GetVfBitsJit(2, 'y'), 0x80000000u);  // INT_MIN
}

// -------- VABS (UPPER, FD_01 sub 0x07) — included here since it's a unary
//          ALU op alongside VITOF/VFTOI. Bit-strip-the-sign-bit semantics. --

TEST(Vu0AluLower, VabsClearsSignBitPerLane)
{
	VuTestHarness h(0);
	h.SetVf(1, -1.0f, 2.0f, -3.5f, 0.0f);
	h.LoadProgram({
		UpperOnlyPair(VABS_U(mask::xyzw, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'x'), 1.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'y'), 2.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'z'), 3.5f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'w'), 0.0f);
}

TEST(Vu0AluLower, VabsOnNegativeZeroProducesPositiveZero)
{
	VuTestHarness h(0);
	h.SetVfBits(1, 0x80000000u, 0u, 0x80000000u, 0u);
	h.LoadProgram({
		UpperOnlyPair(VABS_U(mask::xyzw, vf::vf2, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(2, 'x'), 0u);
	EXPECT_EQ(h.GetVfBitsJit(2, 'y'), 0u);
	EXPECT_EQ(h.GetVfBitsJit(2, 'z'), 0u);
	EXPECT_EQ(h.GetVfBitsJit(2, 'w'), 0u);
}

// -------- VLQI / VSQI / VLQD / VSQD (auto-increment loads/stores) --------

TEST(Vu0AluLower, VlqiLoadsThenIncrementsViPointer)
{
	VuTestHarness h(0);
	h.WriteMemU128(0x40, 0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u);
	h.SetVi(3, 0x40 / 16);  // VI in units of 16-byte quads
	h.SetVfBits(2, 0u, 0u, 0u, 0u);
	h.TrackMemWindow(0x40, 16);
	h.LoadProgram({
		LowerOnly(VLQI_L(mask::xyzw, vf::vf2, vi::vi3)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(2, 'x'), 0x11111111u);
	EXPECT_EQ(h.GetVfBitsJit(2, 'y'), 0x22222222u);
	EXPECT_EQ(h.GetVfBitsJit(2, 'z'), 0x33333333u);
	EXPECT_EQ(h.GetVfBitsJit(2, 'w'), 0x44444444u);
	EXPECT_EQ(h.GetViJit(3), (0x40 / 16) + 1);
}

TEST(Vu0AluLower, VsqiStoresThenIncrementsViPointer)
{
	VuTestHarness h(0);
	h.SetVfBits(5, 0xDEADBEEFu, 0xCAFEBABEu, 0xFEEDFACEu, 0x12345678u);
	h.SetVi(4, 0x80 / 16);
	h.TrackMemWindow(0x80, 16);
	h.LoadProgram({
		LowerOnly(VSQI_L(mask::xyzw, vf::vf5, vi::vi4)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetMemU32Jit(0x80), 0xDEADBEEFu);
	EXPECT_EQ(h.GetMemU32Jit(0x84), 0xCAFEBABEu);
	EXPECT_EQ(h.GetMemU32Jit(0x88), 0xFEEDFACEu);
	EXPECT_EQ(h.GetMemU32Jit(0x8C), 0x12345678u);
	EXPECT_EQ(h.GetViJit(4), (0x80 / 16) + 1);
}

TEST(Vu0AluLower, VlqdPredecrementsThenLoads)
{
	VuTestHarness h(0);
	h.WriteMemU128(0x60, 0xAAAA0000u, 0xBBBB0000u, 0xCCCC0000u, 0xDDDD0000u);
	h.SetVi(2, (0x60 / 16) + 1);  // pre-decrement → load from 0x60
	h.SetVfBits(7, 0u, 0u, 0u, 0u);
	h.TrackMemWindow(0x60, 16);
	h.LoadProgram({
		LowerOnly(VLQD_L(mask::xyzw, vf::vf7, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(7, 'x'), 0xAAAA0000u);
	EXPECT_EQ(h.GetViJit(2), (0x60 / 16));
}

TEST(Vu0AluLower, VsqdPredecrementsThenStores)
{
	VuTestHarness h(0);
	h.SetVfBits(8, 0xEEEE1111u, 0xEEEE2222u, 0xEEEE3333u, 0xEEEE4444u);
	h.SetVi(6, (0xA0 / 16) + 1);  // pre-decrement → store at 0xA0
	h.TrackMemWindow(0xA0, 16);
	h.LoadProgram({
		LowerOnly(VSQD_L(mask::xyzw, vf::vf8, vi::vi6)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetMemU32Jit(0xA0), 0xEEEE1111u);
	EXPECT_EQ(h.GetMemU32Jit(0xAC), 0xEEEE4444u);
	EXPECT_EQ(h.GetViJit(6), 0xA0 / 16);
}

// -------- VLQ / VSQ + LQI/SQI/LQD/SQD partial-mask coverage --------
//
// A partial-mask LQ writes exactly the masked lanes of the destination VF,
// placing the correct memory word into each — e.g. mem[3] (the W word) into
// the W lane for a .w mask — and leaves every unmasked lane untouched. These
// tests pin that single-lane and multi-lane masking behavior.
//
// `Run()`'s auto-diff between JIT and interp is the primary gate; the
// explicit `EXPECT_EQ(GetVfBitsJit(...))` lines pin down the architectural
// intent and survive even on the unlikely chance JIT and interp happen
// to agree on a wrong value.

TEST(Vu0AluLower, VlqWritesOnlyWLane)
{
	VuTestHarness h(0);
	h.WriteMemU128(0, 0xAAAA1111u, 0xBBBB2222u, 0xCCCC3333u, 0xDEADBEEFu);
	h.SetVfBits(14, 0xFEEDFACEu, 0xFEEDFACEu, 0xFEEDFACEu, 0xFEEDFACEu);
	h.LoadProgram({
		LowerOnly(VLQ_L(mask::w, vf::vf14, vi::vi0, 0)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(14, 'x'), 0xFEEDFACEu);
	EXPECT_EQ(h.GetVfBitsJit(14, 'y'), 0xFEEDFACEu);
	EXPECT_EQ(h.GetVfBitsJit(14, 'z'), 0xFEEDFACEu);
	EXPECT_EQ(h.GetVfBitsJit(14, 'w'), 0xDEADBEEFu);
}

TEST(Vu0AluLower, VlqWritesOnlyZLane)
{
	VuTestHarness h(0);
	h.WriteMemU128(0, 0xAAAA1111u, 0xBBBB2222u, 0xCCCC3333u, 0xDEADBEEFu);
	h.SetVfBits(7, 0u, 0u, 0u, 0u);
	h.LoadProgram({
		LowerOnly(VLQ_L(mask::z, vf::vf7, vi::vi0, 0)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(7, 'x'), 0u);
	EXPECT_EQ(h.GetVfBitsJit(7, 'y'), 0u);
	EXPECT_EQ(h.GetVfBitsJit(7, 'z'), 0xCCCC3333u);
	EXPECT_EQ(h.GetVfBitsJit(7, 'w'), 0u);
}

TEST(Vu0AluLower, VlqWritesOnlyYLane)
{
	VuTestHarness h(0);
	h.WriteMemU128(0, 0xAAAA1111u, 0xBBBB2222u, 0xCCCC3333u, 0xDEADBEEFu);
	h.SetVfBits(5, 0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u);
	h.LoadProgram({
		LowerOnly(VLQ_L(mask::y, vf::vf5, vi::vi0, 0)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(5, 'x'), 0x11111111u);
	EXPECT_EQ(h.GetVfBitsJit(5, 'y'), 0xBBBB2222u);
	EXPECT_EQ(h.GetVfBitsJit(5, 'z'), 0x33333333u);
	EXPECT_EQ(h.GetVfBitsJit(5, 'w'), 0x44444444u);
}

TEST(Vu0AluLower, VlqWritesOnlyXLane)
{
	// X is the simplest single-lane mask; kept as a regression sentinel
	// alongside the Y/Z/W cases.
	VuTestHarness h(0);
	h.WriteMemU128(0, 0xAAAA1111u, 0xBBBB2222u, 0xCCCC3333u, 0xDEADBEEFu);
	h.SetVfBits(3, 0u, 0xCAFEBABEu, 0xCAFEBABEu, 0xCAFEBABEu);
	h.LoadProgram({
		LowerOnly(VLQ_L(mask::x, vf::vf3, vi::vi0, 0)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(3, 'x'), 0xAAAA1111u);
	EXPECT_EQ(h.GetVfBitsJit(3, 'y'), 0xCAFEBABEu);
	EXPECT_EQ(h.GetVfBitsJit(3, 'z'), 0xCAFEBABEu);
	EXPECT_EQ(h.GetVfBitsJit(3, 'w'), 0xCAFEBABEu);
}

TEST(Vu0AluLower, VlqMultiLaneZwLeavesXyAlone)
{
	// Multi-lane partial mask: only the masked lanes (Z and W) are written
	// from memory and the unmasked X/Y lanes are preserved. Covered here so a
	// future refactor of the masking path doesn't regress the multi-lane case.
	VuTestHarness h(0);
	h.WriteMemU128(16, 0xAA01u, 0xBB02u, 0xCC03u, 0xDD04u);
	h.SetVfBits(8, 0x77777777u, 0x88888888u, 0x99999999u, 0xAAAAAAAAu);
	h.LoadProgram({
		LowerOnly(VLQ_L(mask::z | mask::w, vf::vf8, vi::vi0, 1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(8, 'x'), 0x77777777u);
	EXPECT_EQ(h.GetVfBitsJit(8, 'y'), 0x88888888u);
	EXPECT_EQ(h.GetVfBitsJit(8, 'z'), 0xCC03u);
	EXPECT_EQ(h.GetVfBitsJit(8, 'w'), 0xDD04u);
}

TEST(Vu0AluLower, VlqWAfterIBitLiteralDoesNotLeakLiteral)
{
	// An I-bit literal sets VI[REG_I] to a recognizable sentinel, then a
	// MULi.w op uses I. The subsequent `LQ.w` to a different VF must observe
	// mem.w, not a value cached in the same host register lane by the
	// preceding broadcast multiply. A lane-handling bug would show
	// vf14.w == 0xCAFEBABE (or some other residue from the I broadcast)
	// instead of the loaded value.
	VuTestHarness h(0);
	h.WriteMemU128(0, 0xAAAA1111u, 0xBBBB2222u, 0xCCCC3333u, 0xDEADBEEFu);
	h.SetVfBits(0, 0u, 0u, 0u, 0x3F800000u);  // VF0.w = 1.0f (architectural)
	h.SetVfBits(14, 0u, 0u, 0u, 0u);
	h.LoadProgram({
		IBit(VuOp{VLitI(0xCAFEBABEu), VNOP_U()}),               // I = sentinel
		VuOp{0u, VMULi_U(mask::w, vf::vf31, vf::vf0)},          // vf31.w = vf0.w * I
		LowerOnly(VLQ_L(mask::w, vf::vf14, vi::vi0, 0)),         // vf14.w = mem[0].w
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(14, 'w'), 0xDEADBEEFu);
	EXPECT_NE(h.GetVfBitsJit(14, 'w'), 0xCAFEBABEu);
}

TEST(Vu0AluLower, VsqStoresOnlyWByte)
{
	// SQ partial-mask doesn't share the LQ lane-convention issue (it merges
	// into a scratch register that's then Str'd directly to memory, so
	// natural-lane placement is correct end-to-end). Lock down the
	// correctness with explicit coverage.
	VuTestHarness h(0);
	h.WriteMemU128(32, 0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u);
	h.SetVfBits(9, 0xAA01u, 0xBB02u, 0xCC03u, 0xDEADBEEFu);
	h.LoadProgram({
		LowerOnly(VSQ_L(mask::w, vf::vf9, vi::vi0, 2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetMemU32Jit(32 + 0), 0x11111111u);
	EXPECT_EQ(h.GetMemU32Jit(32 + 4), 0x22222222u);
	EXPECT_EQ(h.GetMemU32Jit(32 + 8), 0x33333333u);
	EXPECT_EQ(h.GetMemU32Jit(32 + 12), 0xDEADBEEFu);
}

TEST(Vu0AluLower, VlqiPartialMaskWAdvancesViAndLoadsW)
{
	// Existing VLQI test only exercises the full-mask path. Partial-mask
	// LQI shares the same lane-convention constraint as LQ — cover it.
	VuTestHarness h(0);
	h.WriteMemU128(0x40, 0xA1u, 0xA2u, 0xA3u, 0xCAFEBABEu);
	h.SetVi(3, 0x40 / 16);
	h.SetVfBits(2, 0x11u, 0x22u, 0x33u, 0x44u);
	h.LoadProgram({
		LowerOnly(VLQI_L(mask::w, vf::vf2, vi::vi3)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(2, 'x'), 0x11u);
	EXPECT_EQ(h.GetVfBitsJit(2, 'y'), 0x22u);
	EXPECT_EQ(h.GetVfBitsJit(2, 'z'), 0x33u);
	EXPECT_EQ(h.GetVfBitsJit(2, 'w'), 0xCAFEBABEu);
	EXPECT_EQ(h.GetViJit(3), (0x40 / 16) + 1);
}

TEST(Vu0AluLower, VlqdPartialMaskZDecrementsViAndLoadsZ)
{
	VuTestHarness h(0);
	h.WriteMemU128(0x60, 0xA1u, 0xA2u, 0xFEEDFACEu, 0xA4u);
	h.SetVi(2, (0x60 / 16) + 1);
	h.SetVfBits(7, 0u, 0u, 0u, 0u);
	h.LoadProgram({
		LowerOnly(VLQD_L(mask::z, vf::vf7, vi::vi2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(7, 'x'), 0u);
	EXPECT_EQ(h.GetVfBitsJit(7, 'y'), 0u);
	EXPECT_EQ(h.GetVfBitsJit(7, 'z'), 0xFEEDFACEu);
	EXPECT_EQ(h.GetVfBitsJit(7, 'w'), 0u);
	EXPECT_EQ(h.GetViJit(2), 0x60 / 16);
}

TEST(Vu0AluLower, VsqiPartialMaskYAdvancesViAndStoresY)
{
	VuTestHarness h(0);
	h.WriteMemU128(0x80, 0x11u, 0x22u, 0x33u, 0x44u);
	h.SetVfBits(5, 0xAAu, 0xBADCAFEDu, 0xCCu, 0xDDu);
	h.SetVi(4, 0x80 / 16);
	h.LoadProgram({
		LowerOnly(VSQI_L(mask::y, vf::vf5, vi::vi4)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetMemU32Jit(0x80 + 0), 0x11u);
	EXPECT_EQ(h.GetMemU32Jit(0x80 + 4), 0xBADCAFEDu);
	EXPECT_EQ(h.GetMemU32Jit(0x80 + 8), 0x33u);
	EXPECT_EQ(h.GetMemU32Jit(0x80 + 12), 0x44u);
	EXPECT_EQ(h.GetViJit(4), (0x80 / 16) + 1);
}

TEST(Vu0AluLower, VsqdPartialMaskWDecrementsViAndStoresW)
{
	VuTestHarness h(0);
	h.WriteMemU128(0xA0, 0x11u, 0x22u, 0x33u, 0x44u);
	h.SetVfBits(8, 0xAAu, 0xBBu, 0xCCu, 0xC0FFEEFFu);
	h.SetVi(6, (0xA0 / 16) + 1);
	h.LoadProgram({
		LowerOnly(VSQD_L(mask::w, vf::vf8, vi::vi6)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetMemU32Jit(0xA0 + 0), 0x11u);
	EXPECT_EQ(h.GetMemU32Jit(0xA0 + 4), 0x22u);
	EXPECT_EQ(h.GetMemU32Jit(0xA0 + 8), 0x33u);
	EXPECT_EQ(h.GetMemU32Jit(0xA0 + 12), 0xC0FFEEFFu);
	EXPECT_EQ(h.GetViJit(6), 0xA0 / 16);
}

// -------- VU1 spot check — same lower-pipe encoding works on the larger bank --

TEST(Vu0AluLower, VmoveAndVmR32WorkOnVu1)
{
	VuTestHarness h(1);
	h.SetVf(10, 5.0f, 6.0f, 7.0f, 8.0f);
	h.SetVf(11, 0.0f, 0.0f, 0.0f, 0.0f);
	h.LoadProgram({
		LowerOnly(VMOVE_L(mask::xyzw, vf::vf11, vf::vf10)),
		LowerOnly(VMR32_L(mask::xyzw, vf::vf12, vf::vf10)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(11, 'x'), 5.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(11, 'w'), 8.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(12, 'x'), 6.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(12, 'w'), 5.0f);
}

} // namespace recompiler_tests
