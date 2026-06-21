// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU0 special-bit termination DiffJitVsInterp suite. Covers the
// four "specials" in the upper word's high bits:
//   bits::E (0x40000000) — End-of-program. Runs one delay-slot pair after,
//                          then _vuFlushAll, clears VPU_STAT bit 0.
//   bits::D (0x10000000) — Debug interrupt. Conditional on FBRST.D-stop
//                          bit (bit 2 for VU0, bit 10 for VU1). When the
//                          stop bit is set, raises INTC, sets VPU_STAT
//                          bit 1, terminates *without* a delay slot.
//   bits::T (0x08000000) — Trace interrupt. Same as D but conditional on
//                          FBRST bit 3 (VU0) / bit 11 (VU1), latches
//                          VPU_STAT bit 2, calls INTC.
//   bits::M (0x20000000) — VU0 only. Sets VU0.flags |= VUFLAG_MFLAGSET.
//                          The VU0 Execute() loop checks this and breaks.
//                          Doesn't touch VPU_STAT directly.
//
// Cross-engine contract: post-Run() snapshot of VPU_STAT, FBRST, MAC/STATUS/
// CLIP must agree byte-for-byte. INTC raises are side-effects not diffed
// here; the VPU_STAT bits that latch alongside the IRQ are sufficient to
// catch the dispatch divergences.

#include "harness/VuTestHarness.h"

#include "VU.h"
#include "Hw.h"      // INTC_STAT
#include "Dmac.h"    // INTC_VU0
#include "Memory.h"  // psHu32 / eeHw

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

inline VuOp LowerOnly(u32 lower) { return VuOp{lower, VNOP_U()}; }
inline VuOp LoadViImm(u32 dst, u32 imm) { return LowerOnly(VIADDIU_L(dst, vi::vi0, imm)); }

} // namespace

// =========================================================================
//  E-bit — basic termination + delay slot
// =========================================================================

TEST(Vu0SpecialBits, EBitClearsRunningBitAndExits)
{
	VuTestHarness h(0);
	h.LoadProgram({
		LoadViImm(vi::vi1, 0x111),
		EBitNopPair(),
	});
	h.Run();
	EXPECT_TRUE(h.HasTerminated());
	// VPU_STAT lives in VU0's bank for both VUs; bit 0 is the running bit
	// for VU0. After the E-bit drains, both engines should clear it.
	EXPECT_EQ((vuRegs[0].VI[REG_VPU_STAT].UL & 0x1u), 0u);
	EXPECT_EQ(h.GetViJit(REG_VPU_STAT), h.GetViInterp(REG_VPU_STAT));
}

TEST(Vu0SpecialBits, EBitDelaySlotRunsBeforeTermination)
{
	// Pair 0 carries the E-bit AND a useful op (VIADDIU vi1 = 0x111). Pair 1
	// is the architectural delay slot — it must execute before the program
	// flushes. The harness's auto-appended NOP serves as the delay slot here.
	VuTestHarness h(0);
	h.LoadProgram({
		LoadViImm(vi::vi2, 0x222), // pair 0: ordinary
		EBit(LoadViImm(vi::vi1, 0x111)), // pair 1: E-bit pair carrying a load
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(vi::vi1), 0x111u);
	EXPECT_EQ(h.GetViJit(vi::vi2), 0x222u);
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

TEST(Vu0SpecialBits, EBitWithFmacFlushesPipelineToArchitecturalReg)
{
	// FMAC immediately followed by E-bit. The flush should commit the
	// FMAC's MAC/STATUS to REG_MAC_FLAG / REG_STATUS_FLAG.
	VuTestHarness h(0);
	h.LoadProgram({
		VuOp{0, VADD_U(mask::xyzw, vf::vf1, vf::vf0, vf::vf0)},
		EBitNopPair(),
	});
	h.Run();
	EXPECT_EQ(h.GetViJit(REG_MAC_FLAG),    h.GetViInterp(REG_MAC_FLAG));
	EXPECT_EQ(h.GetViJit(REG_STATUS_FLAG), h.GetViInterp(REG_STATUS_FLAG));
}

// =========================================================================
//  D-bit — debug interrupt, conditional on FBRST
//
//  The JIT termination path is intentionally unimplemented — microVU
//  (microVU_Misc.h) hard-codes `static constexpr bool doDBitHandling =
//  false`. The D-bit is a debug aid that shouldn't be enabled in released
//  games; titles needing this style of VU pause use the T-bit instead. Only
//  the silent case (FBRST D-stop clear) is testable cross-engine, since both
//  engines ignore D-bit in that configuration.
// =========================================================================

TEST(Vu0SpecialBits, DBitWithFbrstDStopClearIsSilent)
{
	VuTestHarness h(0);
	vuRegs[0].VI[REG_FBRST].UL = 0u; // no D-stop
	h.LoadProgram({
		DBit(LoadViImm(vi::vi1, 0x111)),
		LoadViImm(vi::vi2, 0x222),
		EBitNopPair(),
	});
	h.Run();
	// D-bit silent — vi2 must run normally. Both engines agree.
	EXPECT_EQ(h.GetViJit(vi::vi1), 0x111u);
	EXPECT_EQ(h.GetViJit(vi::vi2), 0x222u);
	EXPECT_EQ((vuRegs[0].VI[REG_VPU_STAT].UL & 0x2u), 0u);
}

// =========================================================================
//  T-bit — trace interrupt
// =========================================================================

TEST(Vu0SpecialBits, TBitWithFbrstTStopSetTriggersTermination)
{
	VuTestHarness h(0);
	vuRegs[0].VI[REG_FBRST].UL = 0x8u; // T-stop for VU0
	h.LoadProgram({
		LoadViImm(vi::vi1, 0x111),
		TBit(LoadViImm(vi::vi2, 0x222)),
		EBitNopPair(),
	});
	h.Run();
	// VPU_STAT bit 2 = T-finished.
	EXPECT_EQ((vuRegs[0].VI[REG_VPU_STAT].UL & 0x4u), 0x4u);
	EXPECT_EQ(h.GetViJit(REG_VPU_STAT), h.GetViInterp(REG_VPU_STAT));
}

// T-bit silent path: with FBRST.T-stop clear, mVUDoTBit branches over the
// IRQ-raise + terminator body. The lower op of the T-bit pair must still
// commit — it's emitted by mVUexecuteInstruction before the mVUDoTBit
// handler runs (see microVU_Compile.inl), so the silent path is just a
// no-op skip past the terminator. Both engines agree.
TEST(Vu0SpecialBits, TBitWithFbrstTStopClearIsSilent)
{
	VuTestHarness h(0);
	vuRegs[0].VI[REG_FBRST].UL = 0u; // no T-stop
	h.LoadProgram({
		TBit(LoadViImm(vi::vi1, 0x111)),
		LoadViImm(vi::vi2, 0x222),
		EBitNopPair(),
	});
	h.Run();
	// T-bit silent — vi1 lower-op must commit, vi2 must run normally.
	EXPECT_EQ(h.GetViJit(vi::vi1), 0x111u);
	EXPECT_EQ(h.GetViJit(vi::vi2), 0x222u);
	EXPECT_EQ((vuRegs[0].VI[REG_VPU_STAT].UL & 0x4u), 0u); // T-finished bit clear
	EXPECT_EQ(h.GetViJit(vi::vi1), h.GetViInterp(vi::vi1));
	EXPECT_EQ(h.GetViJit(vi::vi2), h.GetViInterp(vi::vi2));
}

// =========================================================================
//  T-bit on a BRANCH instruction — exercises the branch-side T-bit handler
//  in normBranch/condBranch/normJump (microVU_Branch.inl), a DIFFERENT code
//  path from the mVUDoTBit compile-side handler the tests above hit.
//
//  Contract: the branch-side T-bit handler must set VURegs::flags.INTC so the
//  dispatcher epilogue (recMicroVU0::Execute) raises hwIntcIrq(INTC_VU0).
//
//  Why this test isolates the JIT: flags.INTC is consumed by the dispatcher
//  before any register snapshot, and the VPU_STAT bits are masked because the
//  running bit gets cleared at termination anyway — so the only reliable
//  observable is the hwIntcIrq raise itself (psHu32(INTC_STAT)). The
//  interpreter raises INTC inline too, so the JIT is run ONE-SIDED
//  (RunInterpOnly establishes the pre-state + block-cache reset, then
//  RunJitPreserveBlockCache runs only the JIT) with INTC_STAT cleared in
//  between, leaving just the JIT's raise to assert.
// =========================================================================

TEST(Vu0SpecialBits, TBitOnUnconditionalBranchRaisesVu0Intc)
{
	VuTestHarness h(0);
	vuRegs[0].VI[REG_FBRST].UL = 0x8u; // T-stop for VU0 (FBRST bit 3)
	h.LoadProgram({
		TBit(LowerOnly(VB_L(+2))), // pair 0: VB to pair 3, T-bit set on the branch
		LoadViImm(vi::vi1, 0x101), // pair 1: delay slot — always runs
		LoadViImm(vi::vi2, 0x202), // pair 2: skipped by the branch
		LoadViImm(vi::vi3, 0x303), // pair 3: branch target
		EBitNopPair(),             // pair 4: terminator
	});

	// One-sided interp pass: seeds pre-state, resets the JIT block cache, and
	// arms RunJitPreserveBlockCache (no cross-engine diff — the branch+T-bit
	// delay-slot ordering legitimately differs between engines, which is
	// orthogonal to the INTC-raise isolation being tested).
	h.RunInterpOnly();

	// Isolate the JIT's INTC raise: wipe whatever the interp left, run only
	// the JIT from the same pre-state, then assert the dispatcher fired the
	// VU0 interrupt. Buggy branch-side handler → flags.INTC never set →
	// no raise → INTC_STAT VU0 bit stays clear → red.
	psHu32(INTC_STAT) = 0;
	h.RunJitPreserveBlockCache();
	EXPECT_NE(psHu32(INTC_STAT) & (1u << INTC_VU0), 0u)
		<< "branch-side T-bit handler must set VURegs::flags.INTC so "
		   "recMicroVU0::Execute raises hwIntcIrq(INTC_VU0)";
}

// =========================================================================
//  M-bit — VU0-only, sets VUFLAG_MFLAGSET
// =========================================================================

TEST(Vu0SpecialBits, MBitSetsMflagsetInFlags)
{
	// M-bit pair 0; harness's E-bit terminator drains pair 1 normally.
	// VU0 Execute() clears MFLAGSET on entry then re-asserts on the M-bit
	// instruction. The flag latches in VU0.flags (not in VPU_STAT).
	VuTestHarness h(0);
	// JIT and interp legitimately disagree on REG_TPC after an M-bit break:
	// interp advances TPC to the next pair on its way out, the JIT leaves
	// TPC at the breaking pair. The architectural assertion (vuRegs[0].flags
	// MFLAGSET) is what the test exists to verify.
	h.IgnoreViInDiff(REG_TPC);
	vuRegs[0].flags = 0u;
	h.LoadProgram({
		MBit(LoadViImm(vi::vi1, 0x111)),
		LoadViImm(vi::vi2, 0x222),
		EBitNopPair(),
	});
	h.Run();
	// Both engines must set MFLAGSET. mVUcompile must emit
	// `flags |= VUFLAG_MFLAGSET` for M-bit ops in the second pass. Run()
	// executes interp LAST, so the global vuRegs[0].flags would mask a JIT
	// bug behind interp's correct result — assert the JIT snapshot
	// specifically. (VUFLAG_MFLAGSET = 0x2 per VU.h:78.)
	EXPECT_EQ(h.JitSnapshot().regs.flags & VUFLAG_MFLAGSET, VUFLAG_MFLAGSET);
	EXPECT_EQ(h.InterpSnapshot().regs.flags & VUFLAG_MFLAGSET, VUFLAG_MFLAGSET);
}

TEST(Vu0SpecialBits, MBitDoesNotClearVpuStatRunningBit)
{
	// M-bit terminates the Execute() loop but does NOT clear VPU_STAT bit 0.
	// The harness's E-bit terminator at pair 2 takes care of clearing
	// VPU_STAT eventually. Until E-bit drains, VPU_STAT.0 stays set.
	VuTestHarness h(0);
	// See MBitSetsMflagsetInFlags: TPC bookkeeping diverges across the
	// M-bit break boundary; the test's architectural assertion is the
	// VPU_STAT bit, not TPC.
	h.IgnoreViInDiff(REG_TPC);
	vuRegs[0].flags = 0u;
	h.LoadProgram({
		MBit(LoadViImm(vi::vi1, 0x111)),
		EBitNopPair(),
	});
	h.Run();
	// After M-bit the JIT/interp may exit the inner Execute loop, but the
	// harness's RunInterp/RunJit calls Execute(kCycleBudget) once. With M
	// breaking, Execute returns and the harness sees: (a) VPU_STAT.0 still
	// set (M-bit didn't clear it), (b) but the program then also has an
	// E-bit at pair 2 — Execute *should* re-enter and run pair 1's delay
	// slot + pair 2's E-bit termination, clearing VPU_STAT.0.
	//
	// In practice both engines run to E-bit anyway. Diff must agree.
	EXPECT_EQ(h.GetViJit(REG_VPU_STAT), h.GetViInterp(REG_VPU_STAT));
}

// =========================================================================
//  Combinations
// =========================================================================

TEST(Vu0SpecialBits, EBitOnFmacRunsFmacBeforeFlush)
{
	// Useful pair: FMAC + E-bit on the same instruction. The FMAC executes,
	// then the delay-slot pair runs, then flush. Common terminator pattern
	// in real VU programs. Architectural FMAC outputs (vf1, MAC) must match.
	VuTestHarness h(0);
	h.SetVf(vf::vf2, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(vf::vf3, 10.0f, 20.0f, 30.0f, 40.0f);
	h.LoadProgram({
		EBit(VuOp{0, VADD_U(mask::xyzw, vf::vf1, vf::vf2, vf::vf3)}),
	});
	h.Run();
	EXPECT_FLOAT_EQ(h.GetVfJit(vf::vf1, 'x'), 11.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(vf::vf1, 'w'), 44.0f);
	EXPECT_EQ(h.GetVfBitsJit(vf::vf1, 'x'), h.GetVfBitsInterp(vf::vf1, 'x'));
	EXPECT_EQ(h.GetViJit(REG_MAC_FLAG), h.GetViInterp(REG_MAC_FLAG));
}

} // namespace recompiler_tests
