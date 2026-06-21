// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Single-dest-lane MADDA/MSUBA must preserve ACC's non-written lanes —
// DiffJitVsInterp.
//
// The PS2 ACC-accumulate FMACs (MADDA/MSUBA) write only the lanes named by the
// dest mask; the other ACC lanes are untouched. A single-lane MADDA/MSUBA (or
// a broadcast form that takes the same single-lane path) must leave the three
// unwritten ACC lanes at their prior values, not zero them.
//
// These tests pin the lane-preservation contract. They use the non-broadcast
// VMADDA_U/VMSUBA_U at single-lane masks, which route through the identical
// accumulate path as the broadcast forms.

#include "harness/VuTestHarness.h"

#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

// I-bit set so the zero lower word is suppressed (becomes the VI[REG_I]
// immediate) rather than decoding as LQ vf0 — the canonical suppression idiom.
inline VuOp UpperOnly(u32 upper) { return IBit(VuOp{VLitZero(), upper}); }

// ACC ← Fs * Ft, full xyzw (FMACa PS path — known-good, used to seed ACC).
inline VuOp Mula(u32 fs, u32 ft) { return UpperOnly(VMULA_U(mask::xyzw, fs, ft)); }
// ACC.<mask> ← ACC.<mask> + Fs.<mask> * Ft.<mask>.
inline VuOp Madda(u32 m, u32 fs, u32 ft) { return UpperOnly(VMADDA_U(m, fs, ft)); }
// ACC.<mask> ← ACC.<mask> - Fs.<mask> * Ft.<mask>.
inline VuOp Msuba(u32 m, u32 fs, u32 ft) { return UpperOnly(VMSUBA_U(m, fs, ft)); }

constexpr u32 k2 = 0x40000000u; // 2.0
constexpr u32 k3 = 0x40400000u; // 3.0
constexpr u32 k4 = 0x40800000u; // 4.0
constexpr u32 k5 = 0x40A00000u; // 5.0
constexpr u32 k1 = 0x3F800000u; // 1.0

void ExpectAccLanesAgree(VuTestHarness& h)
{
	h.Run();
	const VURegs& j = h.JitSnapshot().regs;
	const VURegs& i = h.InterpSnapshot().regs;
	EXPECT_EQ(j.ACC.UL[0], i.ACC.UL[0]) << "ACC.x";
	EXPECT_EQ(j.ACC.UL[1], i.ACC.UL[1]) << "ACC.y";
	EXPECT_EQ(j.ACC.UL[2], i.ACC.UL[2]) << "ACC.z";
	EXPECT_EQ(j.ACC.UL[3], i.ACC.UL[3]) << "ACC.w";
}

} // namespace

// =========================================================================
//  The regression: a single-lane MADDA must leave the other 3 ACC lanes
//  exactly as the seeding MULA left them.
// =========================================================================

TEST(VuMaddaAccLane, MaddaWPreservesXyz)
{
	VuTestHarness h(0);
	h.SetVfBits(vf::vf1, k2, k3, k4, k5);   // ACC seed operand
	h.SetVfBits(vf::vf2, k1, k1, k1, k1);   // *1.0 → ACC = [2,3,4,5]
	h.SetVfBits(vf::vf3, 0, 0, 0, k2);      // Fs.w = 2.0
	h.SetVfBits(vf::vf4, 0, 0, 0, k3);      // Ft.w = 3.0
	// ACC.w ← 5 + 2*3 = 11; ACC.x/y/z must stay [2,3,4].
	h.LoadProgram({Mula(vf::vf1, vf::vf2), NopPair(), NopPair(), NopPair(),
		Madda(mask::w, vf::vf3, vf::vf4), EBitNopPair()});
	ExpectAccLanesAgree(h);
	const VURegs& i = h.InterpSnapshot().regs;
	EXPECT_EQ(i.ACC.UL[0], k2); // interp oracle: x preserved
	EXPECT_EQ(i.ACC.UL[1], k3);
	EXPECT_EQ(i.ACC.UL[2], k4);
}

TEST(VuMaddaAccLane, MaddaXPreservesYzw)
{
	VuTestHarness h(0);
	h.SetVfBits(vf::vf1, k2, k3, k4, k5);
	h.SetVfBits(vf::vf2, k1, k1, k1, k1);
	h.SetVfBits(vf::vf3, k2, 0, 0, 0);
	h.SetVfBits(vf::vf4, k3, 0, 0, 0);
	h.LoadProgram({Mula(vf::vf1, vf::vf2), NopPair(), NopPair(), NopPair(),
		Madda(mask::x, vf::vf3, vf::vf4), EBitNopPair()});
	ExpectAccLanesAgree(h);
	const VURegs& i = h.InterpSnapshot().regs;
	EXPECT_EQ(i.ACC.UL[1], k3); // y/z/w preserved
	EXPECT_EQ(i.ACC.UL[2], k4);
	EXPECT_EQ(i.ACC.UL[3], k5);
}

TEST(VuMaddaAccLane, MsubaWPreservesXyz)
{
	VuTestHarness h(0);
	h.SetVfBits(vf::vf1, k2, k3, k4, k5);
	h.SetVfBits(vf::vf2, k1, k1, k1, k1);
	h.SetVfBits(vf::vf3, 0, 0, 0, k2);
	h.SetVfBits(vf::vf4, 0, 0, 0, k3);
	// MSUBA.w: ACC.w ← 5 - 2*3 = -1; ACC.x/y/z stay [2,3,4].
	h.LoadProgram({Mula(vf::vf1, vf::vf2), NopPair(), NopPair(), NopPair(),
		Msuba(mask::w, vf::vf3, vf::vf4), EBitNopPair()});
	ExpectAccLanesAgree(h);
	const VURegs& i = h.InterpSnapshot().regs;
	EXPECT_EQ(i.ACC.UL[0], k2);
	EXPECT_EQ(i.ACC.UL[1], k3);
	EXPECT_EQ(i.ACC.UL[2], k4);
}

// =========================================================================
//  Regression guards for the paths the fix must NOT disturb.
// =========================================================================

// Full-mask MADDA writes all four lanes (NEON_PS path) — must stay correct.
TEST(VuMaddaAccLane, MaddaXyzwWritesAllLanes)
{
	VuTestHarness h(0);
	h.SetVfBits(vf::vf1, k2, k3, k4, k5);
	h.SetVfBits(vf::vf2, k1, k1, k1, k1);
	h.SetVfBits(vf::vf3, k2, k2, k2, k2);
	h.SetVfBits(vf::vf4, k3, k3, k3, k3);
	h.LoadProgram({Mula(vf::vf1, vf::vf2), NopPair(), NopPair(), NopPair(),
		Madda(mask::xyzw, vf::vf3, vf::vf4), EBitNopPair()});
	ExpectAccLanesAgree(h);
}

// Partial multi-lane mask (.xy) uses the tempACC+mergeRegs path — must preserve
// the unmasked lanes too.
TEST(VuMaddaAccLane, MaddaXyPreservesZw)
{
	VuTestHarness h(0);
	h.SetVfBits(vf::vf1, k2, k3, k4, k5);
	h.SetVfBits(vf::vf2, k1, k1, k1, k1);
	h.SetVfBits(vf::vf3, k2, k2, 0, 0);
	h.SetVfBits(vf::vf4, k3, k3, 0, 0);
	h.LoadProgram({Mula(vf::vf1, vf::vf2), NopPair(), NopPair(), NopPair(),
		Madda(mask::x | mask::y, vf::vf3, vf::vf4), EBitNopPair()});
	ExpectAccLanesAgree(h);
	const VURegs& i = h.InterpSnapshot().regs;
	EXPECT_EQ(i.ACC.UL[2], k4); // z/w preserved
	EXPECT_EQ(i.ACC.UL[3], k5);
}

} // namespace recompiler_tests
