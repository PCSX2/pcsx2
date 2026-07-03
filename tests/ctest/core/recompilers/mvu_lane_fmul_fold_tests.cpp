// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// AX-14 — lane-indexed FMUL broadcast fold (unconditional since ABI v6).
//
// Broadcast-operand FMAC multiplies (MULbc / MULAbc / MADDbc / MADDAbc /
// MSUBbc / MSUBAbc) used to materialize FT.bc with a scratch allocReg + DUP
// and then run a full 4-lane FMUL. AArch64 folds the lane select into the
// multiply itself (FMUL Vd.4S, Vn.4S, Vm.S[lane]) — bit-identical per lane
// (single rounding either way), one fewer instruction, one fewer live NEON
// register. x86 SSE cannot express this, so there is no x86 twin to mirror.
// Idea from ARMSX2's tryEmitFmulLaneBroadcast (Tyler Bochard, GPLv3).
//
// The fold applies when the multiply step is the only Ft consumer, the op is
// not single-scalar, and no Ft clamp will be emitted (the same willClamp
// expression setupFtReg's opCase1 path uses) — under the default clamp mode
// that is the whole cFt-free transform-chain family. Ops whose clampType
// carries cFt (full-mask MULbc / MULAw) keep the materialized Dup path; the
// vu0 clamp-mode suite plus the mixed program below cover that side.
//
// These tests pin fold-path correctness against the interpreter across all
// four FMAC emit bodies, partial masks, and operand aliasing. The emitted
// SHAPE of the folded chain is pinned centrally by mvu_abi_digest_tests'
// broadcastChain probe (ABI v6 row), which also proves these tests aren't
// vacuously exercising a non-folding path.

#include "harness/VuTestHarness.h"

#include "Config.h"
#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

// I-bit suppresses the zero lower word (canonical upper-only form used
// across the suite).
inline VuOp UpperOnly(u32 upper) { return IBit(VuOp{VLitZero(), upper}); }

// Values chosen so every lane of every product/sum is a distinct, exactly-
// representable float: a wrong broadcast lane or a stale operand register
// shows up as a bit-level VF/ACC/MAC-flag diff, not a rounding coincidence.
void SeedCommonRegs(VuTestHarness& h)
{
	h.SetVf(1, 1.5f, -2.25f, 3.0f, 0.0625f);
	h.SetVf(2, 4.0f, 0.5f, -1.0f, 8.0f); // broadcast source in most programs
	h.SetVf(3, -0.75f, 12.5f, 0.125f, -6.0f);
	h.SetVf(4, 100.0f, -0.001f, 7.5f, 2.0f);
	h.SetVf(5, 0.5f, 3.25f, -9.0f, 1.0f);
	h.SetVf(6, -1.5f, 0.25f, 55.0f, -0.5f);
}

} // namespace

// The canonical 4x4 matrix-transform idiom the fold targets, covering all
// four FMAC emit bodies: FMACa (MULAbc), FMACb (MADDAbc/MSUBAbc), FMACc
// (MADDbc), FMACd (MSUBbc). Every op here carries a cFt-free clampType, so
// under default clamping the fold fires for each multiply step.
TEST(MvuLaneFmulFold, TransformChainFullMaskMatchesInterp)
{
	VuTestHarness h(0);
	SeedCommonRegs(h);
	h.LoadProgram({
		UpperOnly(VMULAx_U(mask::xyzw, vf::vf3, vf::vf2)),  // ACC  = vf3 * vf2.x   (FMACa, isACC)
		UpperOnly(VMADDAy_U(mask::xyzw, vf::vf4, vf::vf2)), // ACC += vf4 * vf2.y   (FMACb, add)
		UpperOnly(VMSUBAz_U(mask::xyzw, vf::vf5, vf::vf2)), // ACC -= vf5 * vf2.z   (FMACb, sub)
		UpperOnly(VMADDw_U(mask::xyzw, vf::vf7, vf::vf6, vf::vf2)), // vf7 = ACC + vf6 * vf2.w (FMACc)
		UpperOnly(bits::E | VMSUBx_U(mask::xyzw, vf::vf8, vf::vf6, vf::vf2)), // vf8 = ACC - vf6 * vf2.x (FMACd)
	});
	h.Run(); // auto-diffs VF/ACC/VI/flags JIT vs interp
}

// Partial write masks and operand aliasing: dest == bc-source, Fs == Ft,
// a VF0.w broadcast (constant 1.0), plus one full-mask MULbc — whose cFt
// clampType takes the materialized non-fold path under overflow clamping —
// mixed into the same program so both paths coexist in one block.
TEST(MvuLaneFmulFold, PartialMaskAliasingAndVf0MatchesInterp)
{
	VuTestHarness h(0);
	SeedCommonRegs(h);
	h.LoadProgram({
		UpperOnly(VMULy_U(mask::xyz, vf::vf1, vf::vf2, vf::vf1)),  // vf1.xyz = vf2 * vf1.y — dest aliases bc source
		UpperOnly(VMULz_U(mask::xyz, vf::vf3, vf::vf4, vf::vf4)),  // Fs == Ft aliasing
		UpperOnly(VMULAy_U(mask::xyzw, vf::vf5, vf::vf6)),         // seed ACC
		UpperOnly(VMADDAw_U(mask::xyzw, vf::vf5, vf::vf0)),        // ACC += vf5 * VF0.w (== 1.0)
		UpperOnly(VMULx_U(mask::xyzw, vf::vf8, vf::vf3, vf::vf2)), // full-mask MULbc: cFt clamp → materialized path
		UpperOnly(bits::E | VMADDz_U(mask::xyzw, vf::vf7, vf::vf1, vf::vf3)),
	});
	h.Run();
}

} // namespace recompiler_tests
