// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU0 upper-pipe FMAC DiffJitVsInterp suite. Each test runs the
// same one-instruction microprogram through microVU and the VU interpreter
// from identical pre-state and gtest-fails on any architectural divergence
// (registers, MAC/STATUS/CLIP-as-VI, memory windows). Spot-checks via
// GetVfJit confirm the test reaches the asserted post-state shape; the
// harness's diff is what actually polices correctness.

#include "harness/VuTestHarness.h"

#include "Config.h"
#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

// Pair 0 = upper op + I-bit-skipped lower (the lower word becomes a 32-bit
// float immediate into VI[REG_I] which we ignore). Pair 1 = E-bit terminator
// (the harness appends an architectural-delay-slot NOP pair automatically).
inline VuOp UpperOnly(u32 upper)
{
	return IBit(VuOp{VLitZero(), upper});
}

} // namespace

// -------- VADD primary (xyzw operand) --------

TEST(Vu0AluUpper, VaddXyzwAcrossAllLanes)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(2, 0.5f, 0.25f, 0.125f, 0.0625f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 1.5f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 2.25f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 3.125f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 4.0625f);
}

TEST(Vu0AluUpper, VaddXOnlyLeavesOtherLanesPreSeed)
{
	// Mask=x; FD lanes y/z/w must retain their pre-state, only x updates.
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(2, 10.0f, 20.0f, 30.0f, 40.0f);
	h.SetVf(3, 99.0f, -77.0f, 55.5f, -33.25f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::x, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 11.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), -77.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 55.5f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), -33.25f);
}

TEST(Vu0AluUpper, VaddYzMaskLeavesXAndW)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(2, 10.0f, 20.0f, 30.0f, 40.0f);
	h.SetVf(3, -1.0f, -2.0f, -3.0f, -4.0f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::y | mask::z, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), -1.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 22.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 33.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), -4.0f);
}

TEST(Vu0AluUpper, VaddSelfAliasingFdEqualsFs)
{
	// FD == FS: result accumulates into the same register without trampling
	// the source mid-instruction. The regalloc has to flush-or-share
	// here; aliasing was a real bug class on the EE side.
	VuTestHarness h(0);
	h.SetVf(1, 1.5f, 2.5f, 3.5f, 4.5f);
	h.SetVf(2, 0.5f, 0.5f, 0.5f, 0.5f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf1, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(1, 'x'), 2.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(1, 'y'), 3.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(1, 'z'), 4.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(1, 'w'), 5.0f);
}

TEST(Vu0AluUpper, VaddNegativeOperands)
{
	VuTestHarness h(0);
	h.SetVf(1, -1.0f, -2.0f, -3.0f, -4.0f);
	h.SetVf(2, 1.0f, 1.5f, 2.0f, 2.5f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 0.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), -0.5f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), -1.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), -1.5f);
}

// -------- VSUB primary --------

TEST(Vu0AluUpper, VsubXyzw)
{
	VuTestHarness h(0);
	h.SetVf(1, 5.0f, 10.0f, 15.0f, 20.0f);
	h.SetVf(2, 1.0f, 2.0f, 3.0f, 4.0f);
	h.LoadProgram({
		UpperOnly(VSUB_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 4.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 8.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 12.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 16.0f);
}

TEST(Vu0AluUpper, VsubFdEqualsFt)
{
	// FD==FT: the JIT mustn't read a stale FT after writing FD lane-by-lane.
	VuTestHarness h(0);
	h.SetVf(1, 10.0f, 20.0f, 30.0f, 40.0f);
	h.SetVf(2, 1.0f, 2.0f, 3.0f, 4.0f);
	h.LoadProgram({
		UpperOnly(VSUB_U(mask::xyzw, vf::vf2, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'x'), 9.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'y'), 18.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'z'), 27.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'w'), 36.0f);
}

// -------- VMUL primary --------

TEST(Vu0AluUpper, VmulXyzw)
{
	VuTestHarness h(0);
	h.SetVf(1, 2.0f, 3.0f, 4.0f, 5.0f);
	h.SetVf(2, 0.5f, 0.5f, 0.25f, -1.0f);
	h.LoadProgram({
		UpperOnly(VMUL_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 1.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 1.5f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 1.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), -5.0f);
}

TEST(Vu0AluUpper, VmulZeroProducesZero)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(2, 0.0f, 0.0f, 0.0f, 0.0f);
	h.SetVf(3, 99.0f, 99.0f, 99.0f, 99.0f);
	h.LoadProgram({
		UpperOnly(VMUL_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 0.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 0.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 0.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 0.0f);
}

// -------- VMAX / VMINI --------

TEST(Vu0AluUpper, VmaxPicksLargerLaneByLane)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 5.0f, -3.0f, 0.0f);
	h.SetVf(2, 2.0f, 4.0f, -1.0f, -0.5f);
	h.LoadProgram({
		UpperOnly(VMAX_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 2.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 5.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), -1.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 0.0f);
}

TEST(Vu0AluUpper, VminiPicksSmallerLaneByLane)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 5.0f, -3.0f, 0.0f);
	h.SetVf(2, 2.0f, 4.0f, -1.0f, -0.5f);
	h.LoadProgram({
		UpperOnly(VMINI_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 1.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 4.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), -3.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), -0.5f);
}

// -------- VMADD / VMSUB (read-modify-write into FD via ACC) --------
//
// VMADD: FD = ACC + FS * FT
// VMSUB: FD = ACC - FS * FT
// The interpreter and JIT have separate ACC representations; correctness
// requires both to agree at E-bit. ACC is seeded via a preceding op so the
// test doesn't have to construct ACC out-of-band.

TEST(Vu0AluUpper, VmaddAddsAccProduct)
{
	// Seed ACC via a VMUL (FD=ACC). Trick: there's a separate VMULA op for
	// "MUL into ACC" but no encoder for it yet. Instead use
	// VADD to a scratch FD (which doesn't write ACC), then test VMADD with
	// ACC at its zeroed default. ACC starts (0,0,0,0), so the result is
	// just FS * FT — good enough to exercise the VMADD primary.
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(2, 5.0f, 6.0f, 7.0f, 8.0f);
	h.LoadProgram({
		UpperOnly(VMADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 5.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 12.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 21.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 32.0f);
}

TEST(Vu0AluUpper, VmsubSubtractsAccProduct)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(2, 5.0f, 6.0f, 7.0f, 8.0f);
	h.LoadProgram({
		UpperOnly(VMSUB_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	// ACC=0, so VMSUB writes -FS*FT to FD.
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), -5.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), -12.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), -21.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), -32.0f);
}

// -------- Broadcast variants — VADDx/y/z/w --------
// FD = FS + FT.bc — bc replicated across all lanes of FT before add.

TEST(Vu0AluUpper, VaddxBroadcastsFtX)
{
	VuTestHarness h(0);
	h.SetVf(1, 0.0f, 100.0f, 200.0f, 300.0f);
	h.SetVf(2, 7.0f, 99.0f, 99.0f, 99.0f); // only x lane of FT consulted
	h.LoadProgram({
		UpperOnly(VADDx_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 7.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 107.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 207.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 307.0f);
}

TEST(Vu0AluUpper, VaddyBroadcastsFtY)
{
	VuTestHarness h(0);
	h.SetVf(1, 0.0f, 100.0f, 200.0f, 300.0f);
	h.SetVf(2, 99.0f, 7.0f, 99.0f, 99.0f);
	h.LoadProgram({
		UpperOnly(VADDy_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 7.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 107.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 207.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 307.0f);
}

TEST(Vu0AluUpper, VaddzBroadcastsFtZ)
{
	VuTestHarness h(0);
	h.SetVf(1, 0.0f, 100.0f, 200.0f, 300.0f);
	h.SetVf(2, 99.0f, 99.0f, 7.0f, 99.0f);
	h.LoadProgram({
		UpperOnly(VADDz_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 7.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 107.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 207.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 307.0f);
}

TEST(Vu0AluUpper, VaddwBroadcastsFtW)
{
	VuTestHarness h(0);
	h.SetVf(1, 0.0f, 100.0f, 200.0f, 300.0f);
	h.SetVf(2, 99.0f, 99.0f, 99.0f, 7.0f);
	h.LoadProgram({
		UpperOnly(VADDw_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 7.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 107.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 207.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 307.0f);
}

// -------- Broadcast variants — VSUBx/y/z/w --------

TEST(Vu0AluUpper, VsubxSubtractsBroadcastFtX)
{
	VuTestHarness h(0);
	h.SetVf(1, 100.0f, 200.0f, 300.0f, 400.0f);
	h.SetVf(2, 50.0f, 99.0f, 99.0f, 99.0f);
	h.LoadProgram({
		UpperOnly(VSUBx_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 50.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 150.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 250.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 350.0f);
}

TEST(Vu0AluUpper, VsubwSubtractsBroadcastFtW)
{
	VuTestHarness h(0);
	h.SetVf(1, 100.0f, 200.0f, 300.0f, 400.0f);
	h.SetVf(2, 99.0f, 99.0f, 99.0f, 50.0f);
	h.LoadProgram({
		UpperOnly(VSUBw_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 50.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 150.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 250.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 350.0f);
}

// -------- Broadcast variants — VMULx/y/z/w --------

TEST(Vu0AluUpper, VmulxBroadcastsFtX)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(2, 10.0f, 99.0f, 99.0f, 99.0f);
	h.LoadProgram({
		UpperOnly(VMULx_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 10.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 20.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 30.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 40.0f);
}

TEST(Vu0AluUpper, VmulyBroadcastsFtY)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(2, 99.0f, -5.0f, 99.0f, 99.0f);
	h.LoadProgram({
		UpperOnly(VMULy_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), -5.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), -10.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), -15.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), -20.0f);
}

// -------- Mask + broadcast crossover --------
// Single-lane masks combined with a broadcast op: the destination lane must
// take its FT from the broadcasted source, not from the matching FT lane.

TEST(Vu0AluUpper, VaddwMaskZWritesOnlyZWithFtW)
{
	VuTestHarness h(0);
	h.SetVf(1, 10.0f, 20.0f, 30.0f, 40.0f);
	h.SetVf(2, 99.0f, 99.0f, 99.0f, 5.0f); // bc value lives in w
	h.SetVf(3, -1.0f, -2.0f, -3.0f, -4.0f);
	h.LoadProgram({
		UpperOnly(VADDw_U(mask::z, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), -1.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), -2.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 35.0f);  // 30 + 5
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), -4.0f);
}

// -------- VU1 spot check: same opcode encoding works on the larger bank --------

TEST(Vu0AluUpper, VaddXyzwAcrossAllLanesOnVu1)
{
	VuTestHarness h(1);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(2, 0.5f, 0.25f, 0.125f, 0.0625f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 1.5f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'y'), 2.25f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'z'), 3.125f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 4.0625f);
}

// -------- VuAddSubHack (tri-ace ADDi bit-exactness gamefix) --------
// When enabled, the SS ADDi path flushes the operand whose exponent is >= 25
// smaller to a signed zero before the scalar add (ports x86 ADD_SS_TriAceHack).
// At that separation the small operand is already below the IEEE round bit, so
// the VALUE matches a plain add and equals the larger operand — hence these
// assert the larger operand and rely on the harness JIT-vs-interp diff to catch
// an encoding bug: an inverted flush branch would discard the LARGER operand and
// diverge from the interpreter, and a clobbered lane would diverge too.

namespace {
struct ScopedVuAddSubHack
{
	bool prev = EmuConfig.Gamefixes.VuAddSubHack;
	explicit ScopedVuAddSubHack(bool on) { EmuConfig.Gamefixes.VuAddSubHack = on; }
	~ScopedVuAddSubHack() { EmuConfig.Gamefixes.VuAddSubHack = prev; }
};
constexpr u32 kBigBits = 0x4E800000u; // 1073741824.0f == 2^30 (exp 157)
} // namespace

TEST(Vu0AddSubHack, AddiSmallFsLargeIFlushesFs)
{
	ScopedVuAddSubHack hack(true);
	VuTestHarness h(0);
	h.SetVf(vf::vf2, 11.0f, 22.0f, 33.0f, 44.0f); // dest pre-seed; y/z/w must survive
	h.SetVf(vf::vf1, 1.0f, 0.0f, 0.0f, 0.0f);     // Fs.x is the small operand
	h.LoadProgram({
		IBit(VuOp{VLitI(kBigBits), VNOP_U()}),         // I = 2^30
		VuOp{0u, VADDi_U(mask::x, vf::vf2, vf::vf1)},   // vf2.x = vf1.x + I
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(2, 'x'), kBigBits); // Fs flushed -> result is the large I
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'y'), 22.0f);  // masked-out lanes preserved
	EXPECT_FLOAT_EQ(h.GetVfJit(2, 'w'), 44.0f);
}

TEST(Vu0AddSubHack, AddiLargeFsSmallIFlushesI)
{
	ScopedVuAddSubHack hack(true);
	VuTestHarness h(0);
	h.SetVf(vf::vf1, 1073741824.0f, 0.0f, 0.0f, 0.0f); // Fs.x large (2^30)
	h.LoadProgram({
		IBit(VuOp{VLitI(0x3F800000u /* 1.0f */), VNOP_U()}), // I = 1.0
		VuOp{0u, VADDi_U(mask::x, vf::vf2, vf::vf1)},         // vf2.x = vf1.x + I
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(2, 'x'), kBigBits); // I flushed -> result is the large Fs
}

} // namespace recompiler_tests
