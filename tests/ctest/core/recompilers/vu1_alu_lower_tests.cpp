// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU1 lower-pipe non-Q ALU DiffJitVsInterp suite — parity counterpart to
// vu0_alu_lower_tests.cpp. Same opcode encodings driving mVUexecuteVU1
// instead of VU0. Covers VMOVE, VMR32, VMTIR, VMFIR; the eight VFTOIx /
// VITOFx fixed-point conversions; and VLQI / VSQI / VLQD / VSQD
// pre/post-increment loads/stores. Q-pipe ops live in their dedicated
// VU0 suite — adding a VU1 Q-pipe parity suite is a separate task.

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

TEST(Vu1AluLower, VmoveXyzwCopiesAllLanes)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VmoveMaskedLeavesUnmaskedLanes)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VmoveSelfIsIdentity)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VmR32RotatesByOneLane)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VmR32MaskedZWritesOnlyZ)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VmR32SelfAliasingRotatesIntoSelf)
{
	// fs == ft: the rotate must fully complete from the saved input —
	// regalloc has to either spill or use a temp, otherwise lane writes
	// stomp later lane reads. Classic aliasing-bug shape.
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VmtirCopiesLaneToVi)
{
	// Per _vuMTIR: VI[it].US[0] = u16(fs.f[fsf]) — the *low 16 bits of the
	// IEEE-754 representation* of the chosen lane. NOT a float→int cast.
	VuTestHarness h(1);
	h.SetVfBits(1, 0xCAFEBABE, 0xDEADBEEF, 0x12345678, 0xAAAA5555);
	// Pick fsf=2 (z lane) → VI[1] should get low 16 bits of 0x12345678 = 0x5678.
	h.LoadProgram({
		LowerOnly(VMTIR_L(vi::vi1, vf::vf1, /*fsf=*/2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(1), 0x5678u);
}

TEST(Vu1AluLower, VmfirSignExtendsViIntoAllLanes)
{
	// Per _vuMFIR: ft.SL[lane] = (s32)VI[is].SS[0] for each masked lane.
	// VI is 16-bit signed; result populates the s32 slot of each VF lane.
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VmfirMaskedOnlyTouchesSelectedLanes)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VitofZeroMatchesIntegerCast)
{
	// VITOF0: just (float)(s32)bits — no scaling.
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VitofFourScalesByExp4)
{
	// VITOF4: (float)bits / 2^4 — used for 28.4 fixed-point reads.
	VuTestHarness h(1);
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

TEST(Vu1AluLower, Vitof12ScalesByExp12)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, Vitof15ScalesByExp15)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, Vftoi0RoundsTowardZero)
{
	// VFTOI0: (s32)f — truncate toward zero.
	VuTestHarness h(1);
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

TEST(Vu1AluLower, Vftoi4ScalesUp16XThenTruncates)
{
	// VFTOI4: (s32)(f * 16). Used for 28.4 fixed-point writes.
	VuTestHarness h(1);
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

TEST(Vu1AluLower, Vftoi12ScalesUp4096X)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, Vftoi15ScalesUp32768X)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, Vftoi0SaturatesAtIntMaxOnHugeFloat)
{
	// Per floatToInt<>: any float whose exponent is ≥ 0x4F (≈ 2^31) saturates
	// to ±INT_MAX/INT_MIN. A bug here would be the runaway int-cast undefined
	// behaviour seen on the EE side (MTSA float-mul cast).
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VabsClearsSignBitPerLane)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VabsOnNegativeZeroProducesPositiveZero)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VlqiLoadsThenIncrementsViPointer)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VsqiStoresThenIncrementsViPointer)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VlqdPredecrementsThenLoads)
{
	VuTestHarness h(1);
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

TEST(Vu1AluLower, VsqdPredecrementsThenStores)
{
	VuTestHarness h(1);
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

// -------- VLQ base+imm single-lane (.w) mask coverage on VU1 --------
//
// The microVU LQ codegen is shared between VU0 and VU1. The full partial-mask
// matrix lives in the VU0 suite; this section covers consecutive `LQ.w` loads
// from mem[0..1] into vf14/vf15 on VU1. Encoded as the plain VLQ_L (base+imm
// form), the common microcode shape, not the auto-incrementing variants.

TEST(Vu1AluLower, VlqWMaskBaseImmSingleLane)
{
	VuTestHarness h(1);
	// mem[0].w = 1.0f, mem[1].w = 20000.0f.
	h.WriteMemU128(0,  0x44000000u, 0x44000000u, 0u, 0x3F800000u);
	h.WriteMemU128(16, 0x45600000u, 0x45600000u, 0u, 0x469C4000u);
	h.SetVfBits(14, 0u, 0u, 0u, 0u);
	h.SetVfBits(15, 0u, 0u, 0u, 0u);
	h.LoadProgram({
		LowerOnly(VLQ_L(mask::w, vf::vf14, vi::vi0, 0)),
		LowerOnly(VLQ_L(mask::w, vf::vf15, vi::vi0, 1)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(14, 'w'), 0x3F800000u) << "vf14.w should be mem[0].w (1.0f)";
	EXPECT_EQ(h.GetVfBitsJit(15, 'w'), 0x469C4000u) << "vf15.w should be mem[1].w (20000.0f)";
}

TEST(Vu1AluLower, VlqYLoadsOnlyYLane)
{
	VuTestHarness h(1);
	h.WriteMemU128(0, 0xAAAAu, 0xBADCAFEDu, 0xCCCCu, 0xDDDDu);
	h.SetVfBits(20, 0x11u, 0x22u, 0x33u, 0x44u);
	h.LoadProgram({
		LowerOnly(VLQ_L(mask::y, vf::vf20, vi::vi0, 0)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(20, 'x'), 0x11u);
	EXPECT_EQ(h.GetVfBitsJit(20, 'y'), 0xBADCAFEDu);
	EXPECT_EQ(h.GetVfBitsJit(20, 'z'), 0x33u);
	EXPECT_EQ(h.GetVfBitsJit(20, 'w'), 0x44u);
}

// =========================================================================
//  vi00 constant-address loadstore fold (upstream 6018936dc)
// =========================================================================
//
// When the base VI is vi00 (always reads 0) the loadstore address is a
// compile-time constant, so microVU folds it (mVUoptimizeConstantAddr) into a
// Mem-pointer load + one immediate add instead of the runtime
// moveVI + imm-add + mask/shift + base-add chain. These cover all four folded
// ops (LQ/SQ/ILW/ISW) at a NON-ZERO quad offset, exercising the VU1 0x3FF mask
// and the byte-offset add. h.Run() auto-diffs the folded JIT against the
// interpreter; the explicit checks pin the resolved address + value.

TEST(Vu1AluLower, LqVi00NonZeroOffsetFoldsToConstAddr)
{
	VuTestHarness h(1);
	h.WriteMemU128(5 * 16, 0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u);
	h.SetVfBits(10, 0, 0, 0, 0);
	h.LoadProgram({
		LowerOnly(VLQ_L(mask::xyzw, vf::vf10, vi::vi0, 5)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(10, 'x'), 0x11111111u);
	EXPECT_EQ(h.GetVfBitsJit(10, 'y'), 0x22222222u);
	EXPECT_EQ(h.GetVfBitsJit(10, 'z'), 0x33333333u);
	EXPECT_EQ(h.GetVfBitsJit(10, 'w'), 0x44444444u);
}

TEST(Vu1AluLower, SqVi00NonZeroOffsetFoldsToConstAddr)
{
	VuTestHarness h(1);
	h.WriteMemU128(7 * 16, 0, 0, 0, 0);
	h.SetVfBits(11, 0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu);
	h.LoadProgram({
		LowerOnly(VSQ_L(mask::xyzw, vf::vf11, vi::vi0, 7)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetMemU32Jit(7 * 16 + 0), 0xAAAAAAAAu);
	EXPECT_EQ(h.GetMemU32Jit(7 * 16 + 4), 0xBBBBBBBBu);
	EXPECT_EQ(h.GetMemU32Jit(7 * 16 + 8), 0xCCCCCCCCu);
	EXPECT_EQ(h.GetMemU32Jit(7 * 16 + 12), 0xDDDDDDDDu);
}

TEST(Vu1AluLower, IlwVi00OffsetLaneFoldsToConstAddr)
{
	VuTestHarness h(1);
	// mem[3].z = 0x1234; ILW.z reads the z lane (offsetSS=8) into vi5.
	h.WriteMemU128(3 * 16, 0, 0, 0x1234u, 0);
	h.SetVi(5, 0);
	h.LoadProgram({
		LowerOnly(VILW_L(mask::z, vi::vi5, vi::vi0, 3)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(5) & 0xFFFFu, 0x1234u);
}

TEST(Vu1AluLower, IswVi00OffsetFoldsToConstAddr)
{
	VuTestHarness h(1);
	h.WriteMemU128(9 * 16, 0, 0, 0, 0);
	h.SetVi(6, 0x5678);
	h.LoadProgram({
		LowerOnly(VISW_L(mask::xyzw, vi::vi6, vi::vi0, 9)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetMemU32Jit(9 * 16 + 0), 0x5678u);
	EXPECT_EQ(h.GetMemU32Jit(9 * 16 + 12), 0x5678u);
}

} // namespace recompiler_tests
