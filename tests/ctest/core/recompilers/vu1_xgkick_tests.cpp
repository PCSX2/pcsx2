// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU1 XGKICK DiffJitVsInterp suite.
//
// XGKICK initiates a GIF Path 1 transfer from VU1 memory. The production
// path runs through `gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, ...)`
// → MTGS, which asserts when no GS thread is running. Under
// `PCSX2_RECOMPILER_TESTS`, Gif_Unit's TransferGSPacketData early-returns
// when `gif_test_hooks::g_path1_sink` is non-null, appending packet bytes
// to the sink. VuTestHarness installs the sink in its constructor and
// captures separate per-pass byte streams via `Path1PacketBytesJit/Interp`.
//
// State-machine note: the JIT and interp track XGKICK *internal* scratch
// state differently. The interp's `_vuXGKICK` writes
// `VU1.xgkickaddr/diff/cyclecount/enable` directly. The JIT non-XGKICKHACK
// path (`mVU_XGKICK_`) computes addr/size in locals and fires the GIF
// transfer without touching VU1.xgkick* — so
// post-state on those scratch fields legitimately diverges. The
// architectural truth is the GIF Path 1 byte stream that reaches GS, so
// these tests use VuDiffMode::XgkickPacketEquivalent (silences the
// xgkick scratch-field diff) and assert byte-for-byte equivalence on the
// captured packet streams.

#include "harness/VuTestHarness.h"

#include "Config.h"
#include "VU.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

inline VuOp LowerOnly(u32 lower) { return VuOp{lower, VNOP_U()}; }
inline VuOp BareNopPair() { return VuOp{0, VNOP_U()}; }

constexpr u32 kEopOnlyTagLower = 0x00008000u;

void WriteEopOnlyTagToVu1(VuTestHarness& h, u32 addr)
{
	h.WriteMemU128(addr, kEopOnlyTagLower, 0u, 0u, 0u);
}

} // namespace

// Crash Twinsanity wedge root cause (2026-07-09): with the XgKickHack gamefix
// ON (GameDB forces it for Twinsanity), every mVU_XGKICK_SYNC site must not
// lose dirty VI register state. The sync's top-of-function
// flushCallerSavedRegisters() only covers x9-x15; a VI allocated into a
// CALLEE-SAVED pool register (w26) was only spilled by the mVUbackupRegs
// emitted BETWEEN the runtime `xgkickenable` / `cyclecount >= 2` branches and
// the C call — so at runtime the spill is skipped whenever no kick is pending
// (xgkickenable==0), while the allocator's compile-time state says the reg is
// clean. The value dies in the callee-saved reg at dispatcher exit; the E-bit
// end flush writes nothing. Live signature: pc0x0's JALR link (vi15=0x0a) and
// the terminal handler's vi12=0 lost => VIF1 VEW deadlock. x86 shares the
// emit structure but is immune (its mVU GPR pool is all caller-saved).
//
// Program shape: three distinct VI writes so the third allocation lands in
// the callee-saved pool reg, then an ISW (isMemWrite => kickcycles flush =>
// SYNC site emitted before it) while that VI is dirty, then E-bit. No XGKICK
// op at all — xgkickenable stays 0, so the buggy conditional spill is always
// skipped at runtime.
TEST(Vu1Xgkick, XgKickHackSyncMustNotLoseCalleeSavedViWrites)
{
	VuTestHarness h(1);

	const bool saved_hack = EmuConfig.Gamefixes.XgKickHack;
	EmuConfig.Gamefixes.XgKickHack = true;

	h.LoadProgram({
		LowerOnly(VIADDIU_L(vi::vi1, vi::vi0, 5)),          // vi1 = 5 (caller-saved alloc)
		LowerOnly(VIADDIU_L(vi::vi2, vi::vi0, 7)),          // vi2 = 7 (caller-saved alloc)
		LowerOnly(VIADDIU_L(vi::vi3, vi::vi0, 9)),          // vi3 = 9 (callee-saved alloc)
		LowerOnly(VISW_L(mask::x, vi::vi1, vi::vi0, 9)),    // ISW => SYNC site while vi3 dirty
		EBitNopPair(),
	});
	h.Run();

	EmuConfig.Gamefixes.XgKickHack = saved_hack;

	EXPECT_EQ(h.GetViJit(vi::vi1), 5u);
	EXPECT_EQ(h.GetViJit(vi::vi2), 7u);
	EXPECT_EQ(h.GetViJit(vi::vi3), 9u)
		<< "VI write lost across an XgKickHack mVU_XGKICK_SYNC site: the "
		   "callee-saved spill was emitted inside the sync's runtime "
		   "conditionals (Crash Twinsanity vi12/vi15 wedge)";
}

TEST(Vu1Xgkick, EopOnlyTagEmitsMatchingPath1Stream)
{
	VuTestHarness h(1);
	h.SetDiffMode(VuDiffMode::XgkickPacketEquivalent);
	WriteEopOnlyTagToVu1(h, 0);
	h.SetVi(vi::vi5, 0);
	h.LoadProgram({
		LowerOnly(VXGKICK_L(vi::vi5)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.Path1PacketBytesJit(), h.Path1PacketBytesInterp());
	// One EOP-only tag = 16 bytes per kick — exactly one kick here.
	EXPECT_EQ(h.Path1PacketBytesInterp().size(), 16u);
}

TEST(Vu1Xgkick, EBitFlushesInflightXgkick)
{
	VuTestHarness h(1);
	h.SetDiffMode(VuDiffMode::XgkickPacketEquivalent);
	WriteEopOnlyTagToVu1(h, 0);
	h.SetVi(vi::vi5, 0);
	h.LoadProgram({
		LowerOnly(VXGKICK_L(vi::vi5)),
		EBitNopPair(),
	});
	h.Run();
	// Even with E-bit one pair away, both engines must drain the kick.
	EXPECT_EQ(h.Path1PacketBytesJit(), h.Path1PacketBytesInterp());
	EXPECT_EQ(h.Path1PacketBytesInterp().size(), 16u);
}

TEST(Vu1Xgkick, BackToBackXgkicksEmitMatchingPath1Stream)
{
	VuTestHarness h(1);
	h.SetDiffMode(VuDiffMode::XgkickPacketEquivalent);
	WriteEopOnlyTagToVu1(h, 0x000);
	WriteEopOnlyTagToVu1(h, 0x100);
	h.SetVi(vi::vi5, 0x000 / 16);
	h.SetVi(vi::vi6, 0x100 / 16);
	h.LoadProgram({
		LowerOnly(VXGKICK_L(vi::vi5)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		LowerOnly(VXGKICK_L(vi::vi6)),
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.Path1PacketBytesJit(), h.Path1PacketBytesInterp());
	// Two EOP-only kicks = 32 bytes total.
	EXPECT_EQ(h.Path1PacketBytesInterp().size(), 32u);
}

// IBNE that does not branch, with XGKICK in its (always-executed) delay slot,
// immediately followed by E-bit. The simple-XGKICK tests above don't place
// XGKICK in a branch delay slot; the branch-taken control-flow analysis at
// compile time can cause the JIT to emit different code for a delay-slot
// XGKICK adjacent to an E-bit, so this pins the not-branching case.
TEST(Vu1Xgkick, IbneDelaySlotXgkickThenEBit_NotBranching)
{
	VuTestHarness h(1);
	h.SetDiffMode(VuDiffMode::XgkickPacketEquivalent);
	WriteEopOnlyTagToVu1(h, 0);
	h.SetVi(vi::vi5, 0);
	// IBNE compares two registers that are always equal (vi0 == vi0 == 0),
	// so the branch never fires. The XGKICK in the delay slot must still
	// execute, then E-bit terminates.
	h.LoadProgram({
		VuOp{VIBNE_L(vi::vi0, vi::vi0, -1), VNOP_U()},  // pc=0x00 IBNE not taken
		LowerOnly(VXGKICK_L(vi::vi5)),                    // pc=0x08 delay slot
		EBitNopPair(),                                    // pc=0x10 E-bit
	});
	h.Run();
	EXPECT_EQ(h.Path1PacketBytesJit(), h.Path1PacketBytesInterp());
	EXPECT_EQ(h.Path1PacketBytesInterp().size(), 16u);
}

// Same pattern as above but with a backward IBNE target (branch into an
// earlier loop body). The branch still does not fire, so control falls
// through to the XGKICK in the delay slot. A backward branch may flip the
// JIT's block-chaining heuristic compared to the forward variant above.
TEST(Vu1Xgkick, IbneBackwardDelaySlotXgkickThenEBit_NotBranching)
{
	VuTestHarness h(1);
	h.SetDiffMode(VuDiffMode::XgkickPacketEquivalent);
	WriteEopOnlyTagToVu1(h, 0);
	h.SetVi(vi::vi5, 0);
	// Pad the front of the program with NOPs so the IBNE has somewhere to
	// branch backward to. Branch target = (pc_of_delay_slot + 1*8) + imm*8.
	// With IBNE at pc=0x18, delay slot at 0x20, target = 0x28 + (-5)*8 = 0x00.
	h.LoadProgram({
		BareNopPair(), BareNopPair(), BareNopPair(),                     // pc=0x00..0x10
		VuOp{VIBNE_L(vi::vi0, vi::vi0, -5), VNOP_U()},                    // pc=0x18 IBNE not taken
		LowerOnly(VXGKICK_L(vi::vi5)),                                    // pc=0x20 delay slot
		EBitNopPair(),                                                    // pc=0x28 E-bit
	});
	h.Run();
	EXPECT_EQ(h.Path1PacketBytesJit(), h.Path1PacketBytesInterp());
	EXPECT_EQ(h.Path1PacketBytesInterp().size(), 16u);
}

// Reproducer: MTIR loads vi from a vf lane whose full 32-bit bits have dirty
// top 16 bits. XGKICK then reads vi. The arm64 microRegAlloc allocGPR
// caches the full 32-bit Umov result from MTIR; when XGKICK calls allocGPR
// without zext_if_dirty=true, it stores the dirty 32-bit value to mVU.VIxgkick
// and passes it as `addr` to mVU_XGKICK_. Because mVU_XGKICK_ does
// `(addr & 0x3ff) * 16`, the low-10 bits survive — so the masked vumem offset
// is the same as if we'd truncated to u16 first. Both engines should hit the
// same vumem byte offset and emit identical PATH1.
//
// Setup: vf7.x = 0x4b000208 (low-16 = 0x0208, & 0x3ff = 0x208). MTIR vi5
// from vf7.x. XGKICK vi5. Stage an EOP tag at vumem[0x208 * 16 = 0x2080]
// so both engines emit one 16-byte tag.
TEST(Vu1Xgkick, MtirDirtyHighBits_XgkickLandsOnSameVumemQword)
{
	VuTestHarness h(1);
	h.SetDiffMode(VuDiffMode::XgkickPacketEquivalent);
	WriteEopOnlyTagToVu1(h, 0x2080);
	h.SetVfBits(vf::vf7, 0x4b000208u, 0u, 0u, 0u);
	h.LoadProgram({
		// MTIR.x vi5 = low-16(vf7.x) per VU spec. JIT cache may hold the
		// full 32-bit Umov result; interp truncates to u16.
		VuOp{VMTIR_L(vi::vi5, vf::vf7, /*fsf=x*/0), VNOP_U()},
		BareNopPair(),
		LowerOnly(VXGKICK_L(vi::vi5)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.Path1PacketBytesJit(), h.Path1PacketBytesInterp())
		<< "JIT and interp computed different vumem offsets for XGKICK after "
		   "MTIR from a vf lane with dirty top 16 bits. "
		   "JIT: " << h.Path1PacketBytesJit().size() << " B, "
		   "Interp: " << h.Path1PacketBytesInterp().size() << " B.";
	EXPECT_EQ(h.Path1PacketBytesInterp().size(), 16u)
		<< "Expected one 16-byte EOP tag emitted from vumem[0x2080]";
}

// Probe: do JIT and interp diverge by 1 ULP on (1.0 / 3.0) → MULq → MTIR?
// If yes, the JIT's vi5 lands at a different qword than interp's vi5.
// To detect that, stage a *distinct* EOP-terminated GIF tag at every qword
// of vumem — each tag carries its own qword index in the data lane. The
// PATH1 byte stream then directly encodes which qword the engine kicked
// from, and a divergent vi5 produces a divergent PATH1 byte.
//
// Each tag is 16 bytes (NLOOP=0, EOP=1, qword index in the upper 64 bits)
// so size = 16, no wrap, no harness sink loss.
TEST(Vu1Xgkick, DivOneOverThreeFmacChainPreservesXgkickAddr)
{
	VuTestHarness h(1);
	h.SetDiffMode(VuDiffMode::XgkickPacketEquivalent);
	// Stage one distinct EOP-only tag per qword of vumem (1024 qwords =
	// 16384 bytes = whole VU1 mem). Tag's lower bits are the EOP marker
	// (bit 15 = 1), upper 64 bits encode the qword index.
	for (u32 q = 0; q < 1024; ++q)
		h.WriteMemU128(q * 16, kEopOnlyTagLower, 0u, q, 0u);

	h.SetVfBits(vf::vf1, 0x3F800000u, 0u, 0u, 0u); // 1.0
	h.SetVfBits(vf::vf2, 0x40400000u, 0u, 0u, 0u); // 3.0
	h.LoadProgram({
		// Q = 1.0 / 3.0  (irrational; FP rounding in DIV may diverge)
		VuOp{VDIV_L(vf::vf1, /*fsf=x*/0, vf::vf2, /*ftf=x*/0), VNOP_U()},
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		BareNopPair(), BareNopPair(), BareNopPair(),
		// vf3.x = vf1.x * Q = 1.0 * 0.333…
		VuOp{VNOP_U(), VMULq_U(mask::x, vf::vf3, vf::vf1)},
		BareNopPair(),
		// MTIR.x vi5 = low-16(vf3.x bits)
		VuOp{VMTIR_L(vi::vi5, vf::vf3, /*fsf=x*/0), VNOP_U()},
		BareNopPair(),
		LowerOnly(VXGKICK_L(vi::vi5)),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.Path1PacketBytesJit(), h.Path1PacketBytesInterp())
		<< "JIT and interp emit different PATH1 after DIV(1/3) → MULq → MTIR. "
		   "JIT vi5 = different qword than interp vi5 — likely FP precision "
		   "divergence in the DIV or MULq emit path.";
	// Each engine's emitted byte stream should be exactly one 16-byte tag.
	EXPECT_EQ(h.Path1PacketBytesInterp().size(), 16u);
}

// An overflow-clamped FMAC AFTER an XGKICK must match the interpreter. The
// XGKICK is a mid-block C call (mVU_XGKICK_DELAY) sitting between the block
// prologue and a clamp emitter, so it is the one place a clamp reads state that
// a call has had the chance to disturb — whatever mVUclamp1 sources its bounds
// from must survive the call. Nothing else in the suite pairs the two.
//
// vu1Overflow ON puts the trailing VMUL on the micro-mode mVUclamp1 path, and
// vf2^2 overflows FLT_MAX so the clamp genuinely engages (both engines land on
// +MAX_FLOAT — asserted below, so the test cannot pass by never clamping).
TEST(Vu1Xgkick, ClampAfterXgkickMatchesInterp)
{
	const bool prevOv = EmuConfig.Cpu.Recompiler.vu1Overflow;
	const bool prevEx = EmuConfig.Cpu.Recompiler.vu1ExtraOverflow;
	EmuConfig.Cpu.Recompiler.vu1Overflow = true;
	EmuConfig.Cpu.Recompiler.vu1ExtraOverflow = false;

	VuTestHarness h(1);
	h.SetDiffMode(VuDiffMode::XgkickPacketEquivalent);
	WriteEopOnlyTagToVu1(h, 0);
	h.SetVi(vi::vi5, 0);
	// ~8.5e37 in every lane; squaring overflows FLT_MAX -> clamp to +MAX_FLOAT.
	h.SetVfBits(vf::vf2, 0x7E800000u, 0x7E800000u, 0x7E800000u, 0x7E800000u);
	h.LoadProgram({
		LowerOnly(VXGKICK_L(vi::vi5)),                              // fires mVU_XGKICK_DELAY (C call)
		BareNopPair(), BareNopPair(), BareNopPair(), BareNopPair(),
		IBit(VuOp{VLitZero(), VMUL_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf2)}), // overflow -> clamp
		EBitNopPair(),
	});
	h.Run();

	EmuConfig.Cpu.Recompiler.vu1Overflow = prevOv;
	EmuConfig.Cpu.Recompiler.vu1ExtraOverflow = prevEx;

	EXPECT_EQ(h.GetVfBitsJit(vf::vf1, 'x'), h.GetVfBitsInterp(vf::vf1, 'x'))
		<< "clamp after XGKICK diverged — the mVU_XGKICK_DELAY C call disturbed "
		   "what mVUclamp1 clamps against";
	EXPECT_EQ(h.GetVfBitsJit(vf::vf1, 'y'), h.GetVfBitsInterp(vf::vf1, 'y'));
	EXPECT_EQ(h.GetVfBitsJit(vf::vf1, 'z'), h.GetVfBitsInterp(vf::vf1, 'z'));
	EXPECT_EQ(h.GetVfBitsJit(vf::vf1, 'w'), h.GetVfBitsInterp(vf::vf1, 'w'));
	// Proves the clamp actually engaged (both engines reach +MAX_FLOAT).
	EXPECT_EQ(h.GetVfBitsInterp(vf::vf1, 'x'), 0x7F7FFFFFu);
}

} // namespace recompiler_tests
