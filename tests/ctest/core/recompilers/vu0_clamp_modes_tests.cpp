// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU0 clamping-mode DiffJitVsInterp suite. The PS2 VU has no IEEE
// inf/NaN/denormal: every numerically-special result is clamped to a finite
// IEEE float. PCSX2 makes the clamping aggressiveness configurable via four
// per-VU EmuConfig knobs:
//
//   vu0Overflow        — basic overflow → ±MAX_FLOAT (0x7F7FFFFF / 0xFF7FFFFF)
//   vu0ExtraOverflow   — clamp at MUL/MADD intermediate steps (more strict)
//   vu0SignOverflow    — sign-aware overflow (preserves sign of operand)
//   vu0Underflow       — denormal flush-to-zero
//
// JIT and interp must agree under every combination — these tests pick a
// handful of canonical edge inputs (large-positive overflow, large-negative
// overflow, denormal-producing underflow) and sweep the knob matrix. The
// assertion is cross-engine agreement (GetVfBitsJit == GetVfBitsInterp) for
// each knob setting, not a hardcoded clamped value: whatever the active
// clamping mode produces, both engines must produce bit-for-bit. Each test
// scopes its config change with a RAII saver so fixture-shared state
// (EmuConfig is a global) doesn't leak across tests.
//

#include "harness/VuTestHarness.h"

#include "Config.h"
#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

// Set the I-bit so the zero lower word is positively suppressed rather than
// decoded as a real lower instruction (a bare {0, upper} decodes lower=0 as
// the LQ family — inert today only because _vuLQ early-returns on _Ft_==0).
// Matches the canonical lower-pipe-suppression form used across the suite.
inline VuOp UpperOnly(u32 upper) { return IBit(VuOp{VLitZero(), upper}); }

// RAII guard: snapshots the four VU0 clamp knobs in ctor, restores in dtor.
// Tests use it as `ClampGuard cg; cg.Set(true, false, true, false);` — each
// permutation flipped explicitly so the test reads as a matrix row.
struct ClampGuard
{
	bool prev_overflow;
	bool prev_extra;
	bool prev_sign;
	bool prev_underflow;

	ClampGuard()
	{
		prev_overflow  = EmuConfig.Cpu.Recompiler.vu0Overflow;
		prev_extra     = EmuConfig.Cpu.Recompiler.vu0ExtraOverflow;
		prev_sign      = EmuConfig.Cpu.Recompiler.vu0SignOverflow;
		prev_underflow = EmuConfig.Cpu.Recompiler.vu0Underflow;
	}
	~ClampGuard()
	{
		EmuConfig.Cpu.Recompiler.vu0Overflow     = prev_overflow;
		EmuConfig.Cpu.Recompiler.vu0ExtraOverflow = prev_extra;
		EmuConfig.Cpu.Recompiler.vu0SignOverflow = prev_sign;
		EmuConfig.Cpu.Recompiler.vu0Underflow    = prev_underflow;
	}

	void Set(bool overflow, bool extra, bool sign, bool underflow)
	{
		EmuConfig.Cpu.Recompiler.vu0Overflow     = overflow;
		EmuConfig.Cpu.Recompiler.vu0ExtraOverflow = extra;
		EmuConfig.Cpu.Recompiler.vu0SignOverflow = sign;
		EmuConfig.Cpu.Recompiler.vu0Underflow    = underflow;
	}
};

constexpr u32 kPosBigBits   = 0x7E800000u; // ~8.5e+37, multiplying by self overflows
constexpr u32 kNegBigBits   = 0xFE800000u; // -8.5e+37
constexpr u32 kPosTinyBits  = 0x00800000u; // smallest positive normal float

inline VuOp VMulxyzw(u32 fd, u32 fs, u32 ft) { return UpperOnly(VMUL_U(mask::xyzw, fd, fs, ft)); }

void RunAndExpectAllLanesAgree(VuTestHarness& h, u32 dst_vf)
{
	h.Run();
	EXPECT_EQ(h.GetVfBitsJit(dst_vf, 'x'), h.GetVfBitsInterp(dst_vf, 'x'));
	EXPECT_EQ(h.GetVfBitsJit(dst_vf, 'y'), h.GetVfBitsInterp(dst_vf, 'y'));
	EXPECT_EQ(h.GetVfBitsJit(dst_vf, 'z'), h.GetVfBitsInterp(dst_vf, 'z'));
	EXPECT_EQ(h.GetVfBitsJit(dst_vf, 'w'), h.GetVfBitsInterp(dst_vf, 'w'));
}

} // namespace

// =========================================================================
//  Underflow — VMUL of two tiny normals → 0 (denormal flush) when underflow
//  clamping is on.
// =========================================================================

TEST(Vu0ClampModes, UnderflowOnTinyProductFlushesToZero)
{
	ClampGuard cg;
	cg.Set(true, false, false, /*underflow*/true);

	VuTestHarness h(0);
	h.SetVfBits(vf::vf2, kPosTinyBits, kPosTinyBits, kPosTinyBits, kPosTinyBits);
	h.LoadProgram({
		VMulxyzw(vf::vf1, vf::vf2, vf::vf2),
		EBitNopPair(),
	});
	RunAndExpectAllLanesAgree(h, vf::vf1);
}

TEST(Vu0ClampModes, UnderflowOffTinyProductYieldsDenormal)
{
	ClampGuard cg;
	cg.Set(true, false, false, /*underflow*/false);

	VuTestHarness h(0);
	h.SetVfBits(vf::vf2, kPosTinyBits, kPosTinyBits, kPosTinyBits, kPosTinyBits);
	h.LoadProgram({
		VMulxyzw(vf::vf1, vf::vf2, vf::vf2),
		EBitNopPair(),
	});
	RunAndExpectAllLanesAgree(h, vf::vf1);
}

// =========================================================================
//  Sign-overflow knob — vu0SignOverflow drops sign-aware overflow handling.
//  Use VSUB to produce a large negative result and verify both engines
//  clamp identically with and without the knob.
// =========================================================================

TEST(Vu0ClampModes, SignOverflowOnLargeNegativeResultClampsToNegMax)
{
	ClampGuard cg;
	cg.Set(true, false, /*sign*/true, false);

	VuTestHarness h(0);
	h.SetVfBits(vf::vf2, kPosBigBits, kPosBigBits, kPosBigBits, kPosBigBits);
	h.SetVfBits(vf::vf3, kNegBigBits, kNegBigBits, kNegBigBits, kNegBigBits);
	h.LoadProgram({
		UpperOnly(VSUB_U(mask::xyzw, vf::vf1, vf::vf3, vf::vf2)), // big_neg - big_pos
		EBitNopPair(),
	});
	RunAndExpectAllLanesAgree(h, vf::vf1);
}

TEST(Vu0ClampModes, SignOverflowOffLargeNegativeResultClampsViaBaseOverflow)
{
	ClampGuard cg;
	cg.Set(true, false, /*sign*/false, false);

	VuTestHarness h(0);
	h.SetVfBits(vf::vf2, kPosBigBits, kPosBigBits, kPosBigBits, kPosBigBits);
	h.SetVfBits(vf::vf3, kNegBigBits, kNegBigBits, kNegBigBits, kNegBigBits);
	h.LoadProgram({
		UpperOnly(VSUB_U(mask::xyzw, vf::vf1, vf::vf3, vf::vf2)),
		EBitNopPair(),
	});
	RunAndExpectAllLanesAgree(h, vf::vf1);
}

// =========================================================================
//  Extra-overflow knob — clamps inside MUL/MADD intermediate steps. Use a
//  VMADD (vf1 = ACC + fs*ft) whose product overflows, so the knob clamps the
//  intermediate to MAX_FLOAT before the accumulator add.
// =========================================================================

TEST(Vu0ClampModes, ExtraOverflowOnMaddIntermediateClampsBeforeAdd)
{
	ClampGuard cg;
	cg.Set(true, /*extra*/true, false, false);

	VuTestHarness h(0);
	// Pre-load ACC via VADDA, then VMADD: vf1 = ACC + (vf2 * vf3). The product
	// vf2*vf3 overflows (big*big), which the extra-overflow knob clamps to
	// MAX_FLOAT at the MADD intermediate, before the ACC add.
	h.SetVfBits(vf::vf2, kPosBigBits, kPosBigBits, kPosBigBits, kPosBigBits);
	h.SetVfBits(vf::vf3, kPosBigBits, kPosBigBits, kPosBigBits, kPosBigBits);
	h.LoadProgram({
		UpperOnly(VADDA_U(mask::xyzw, vf::vf2, vf::vf3)),          // ACC = vf2 + vf3
		UpperOnly(0u | VNOP_U()),
		UpperOnly(VMADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf3)), // vf1 = ACC + vf2*vf3
		EBitNopPair(),
	});
	RunAndExpectAllLanesAgree(h, vf::vf1);
}

TEST(Vu0ClampModes, ExtraOverflowOffSameProgramAgreesOnFinalValue)
{
	ClampGuard cg;
	cg.Set(true, /*extra*/false, false, false);

	VuTestHarness h(0);
	h.SetVfBits(vf::vf2, kPosBigBits, kPosBigBits, kPosBigBits, kPosBigBits);
	h.SetVfBits(vf::vf3, kPosBigBits, kPosBigBits, kPosBigBits, kPosBigBits);
	h.LoadProgram({
		UpperOnly(VADDA_U(mask::xyzw, vf::vf2, vf::vf3)),
		UpperOnly(0u | VNOP_U()),
		UpperOnly(VMADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf3)),
		EBitNopPair(),
	});
	RunAndExpectAllLanesAgree(h, vf::vf1);
}

// =========================================================================
//  Overflow knob — VMUL of two large positive values produces +Inf in IEEE,
//  which the interp clamps to MAX_FLOAT during the MAC flag update. The JIT
//  only applies its post-multiply overflow clamp when ExtraOverflow is on —
//  so this case must include extra-overflow to exercise the clamp.
// =========================================================================

TEST(Vu0ClampModes, OverflowOnPositiveVmulClampsToMaxFloat)
{
	ClampGuard cg;
	cg.Set(/*overflow*/true, /*extra*/true, false, false);

	VuTestHarness h(0);
	h.SetVfBits(vf::vf2, kPosBigBits, kPosBigBits, kPosBigBits, kPosBigBits);
	h.LoadProgram({
		VMulxyzw(vf::vf1, vf::vf2, vf::vf2),
		EBitNopPair(),
	});
	RunAndExpectAllLanesAgree(h, vf::vf1);
}

TEST(Vu0ClampModes, OverflowOnNegativeVmulClampsToNegMaxFloat)
{
	ClampGuard cg;
	cg.Set(/*overflow*/true, /*extra*/true, false, false);

	VuTestHarness h(0);
	// Big positive × big negative → big-negative-overflow → -MAX_FLOAT.
	h.SetVfBits(vf::vf2, kPosBigBits, kPosBigBits, kPosBigBits, kPosBigBits);
	h.SetVfBits(vf::vf3, kNegBigBits, kNegBigBits, kNegBigBits, kNegBigBits);
	h.LoadProgram({
		VMulxyzw(vf::vf1, vf::vf2, vf::vf3),
		EBitNopPair(),
	});
	RunAndExpectAllLanesAgree(h, vf::vf1);
}

} // namespace recompiler_tests
