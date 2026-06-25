// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU FMAC overflow-flag emulation under the VUOverflowHack gamefix (DT-04).
//
// PS2 VU floats have no Inf/NaN; an arithmetic overflow saturates to ±FLT_MAX
// and raises the FMAC overflow (O) flag. Real hosts can't tell a saturated
// overflow from a genuine FLT_MAX result without soft-float, so upstream gates
// the O-flag emulation behind the per-game VUOverflowHack gamefix (Superman
// Returns checks VU overflow flags). x86 implements this in
// microVU_Upper.inl's mVUupdateFlags (the `if (sFLAG.doFlag &&
// CHECK_VUOVERFLOWHACK)` block): any lane whose |result| >= FLT_MAX sets STATUS
// O+S (0x820000) and, when the MAC flag is emitted, the per-lane O bits.
//
// The arm64 mVUupdateFlags dropped that block — the VUOverflowHack bit was
// plumbed into the mVU options sentinel (so it busted the program cache) but
// produced NO codegen, a silent no-op. Restored per Discord triage DT-04.
//
// The hack is JIT-only: the shared VU interpreter has no overflow-flag
// emulation, so the JIT's STATUS/MAC flags diverge from interp BY DESIGN when
// the hack fires. The positive test therefore opts the flag VIs out of the
// auto-diff (the VF result still matches and is diffed normally) and asserts
// the JIT STATUS directly; the control test (hack OFF) keeps the full diff and
// asserts the bits are absent.
//
// Witness construction: seed vf1 = FLT_MAX in every lane and VMUL vf2,vf1,vf0.
// vf0 is the VU constant (0,0,0,1), so vf2 = (0,0,0,FLT_MAX) — lane w lands
// exactly on the saturation boundary with NO host Inf and NO dependency on the
// overflow-clamp recompiler setting, so the JIT and interp VF results are
// bit-identical. Only the overflow FLAG differs.

#include "harness/VuTestHarness.h"

#include "VU.h"
#include "Config.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

inline VuOp UpperOnly(u32 upper) { return VuOp{0, upper}; }
inline VuOp BareNopPair() { return VuOp{0, VNOP_U()}; }

constexpr u32 kFltMaxBits = 0x7f7fffffu; // largest finite VU/IEEE single

// The hack ORs 0x820000 into the mVU *internal* (denormalized) status word.
// mVUallocSFLAGc normalizes on commit (internal & 0xffff0000) >> 14, so the
// committed/architectural STATUS read back from VI[REG_STATUS_FLAG] gains
// bit 17->bit 3 (O = 0x8) and bit 23->bit 9 (OS sticky = 0x200) → 0x208.
constexpr u32 kStatusO  = 0x008u; // architectural overflow flag
constexpr u32 kStatusOS = 0x200u; // architectural sticky-overflow flag

// RAII toggle for the VUOverflowHack gamefix (the CHECK_VUOVERFLOWHACK gate).
struct ScopedVuOverflowHack
{
	bool prev;
	explicit ScopedVuOverflowHack(bool on) : prev(EmuConfig.Gamefixes.VUOverflowHack) { EmuConfig.Gamefixes.VUOverflowHack = on; }
	~ScopedVuOverflowHack() { EmuConfig.Gamefixes.VUOverflowHack = prev; }
};

} // namespace

// Hack ON: a lane at FLT_MAX must raise STATUS O+S (0x820000) in the JIT.
TEST(VuOverflowHack, FltMaxLaneSetsStatusOverflowBits)
{
	ScopedVuOverflowHack hack(true);

	VuTestHarness h(0);
	h.SetVfBits(vf::vf1, kFltMaxBits, kFltMaxBits, kFltMaxBits, kFltMaxBits);

	// The O-flag emulation is JIT-only; let the flag VIs diverge from interp.
	// (The VF result is identical on both sides and still diffed.)
	h.IgnoreViInDiff(REG_STATUS_FLAG);
	h.IgnoreViInDiff(REG_MAC_FLAG);

	h.LoadProgram({
		UpperOnly(VMUL_U(mask::xyzw, vf::vf2, vf::vf1, vf::vf0)), // vf2 = (0,0,0,FLT_MAX)
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		EBitNopPair(),
	});
	h.Run();

	// VF parity sanity: lane w == FLT_MAX on both engines (the diff already
	// asserts equality; pin the actual value too).
	EXPECT_EQ(h.GetVfBitsJit(vf::vf2, 'w'), kFltMaxBits);
	EXPECT_EQ(h.GetVfBitsInterp(vf::vf2, 'w'), kFltMaxBits);

	// The hack fired: the committed STATUS gained the (normalized) overflow
	// flags in the JIT — O and the OS sticky — while the interp (FLT_MAX*1 is an
	// exact result, no genuine overflow, and no emulation) leaves O clear.
	EXPECT_EQ(h.GetViJit(REG_STATUS_FLAG) & (kStatusO | kStatusOS), kStatusO | kStatusOS);
	EXPECT_EQ(h.GetViInterp(REG_STATUS_FLAG) & kStatusO, 0u);
}

// Hack OFF (control): identical program, but the overflow block must not emit.
// JIT == interp (full auto-diff) and the overflow bits are absent on both.
TEST(VuOverflowHack, NoOverflowBitsWhenHackDisabled)
{
	ScopedVuOverflowHack hack(false);

	VuTestHarness h(0);
	h.SetVfBits(vf::vf1, kFltMaxBits, kFltMaxBits, kFltMaxBits, kFltMaxBits);

	h.LoadProgram({
		UpperOnly(VMUL_U(mask::xyzw, vf::vf2, vf::vf1, vf::vf0)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		EBitNopPair(),
	});
	h.Run(); // full diff: with the hack off the JIT must match interp exactly

	EXPECT_EQ(h.GetViJit(REG_STATUS_FLAG) & kStatusO, 0u);
	EXPECT_EQ(h.GetViJit(REG_STATUS_FLAG), h.GetViInterp(REG_STATUS_FLAG));
}

} // namespace recompiler_tests
