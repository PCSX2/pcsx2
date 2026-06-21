// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// EE↔VU1 handoff via VIF1 MSCAL/MSCALF/MSCNT.
//
// VU1 microprogram dispatch goes through VIF1: an EE program (or DMA tag
// chain) writes a VIF1 command into the FIFO, the VIF processor parses the
// command in `vifCode_MSCAL/MSCALF/MSCNT` (Vif_Codes.cpp:420-507), and
// `vu1ExecMicro` then kicks the microprogram on `CpuVU1`.
//
// The harness here uses EeRecTestHarness::EnableVu1VifCapture() to bypass
// the DMAC + FIFO and inject MSCAL-family commands directly:
//   - the EE program is a no-op (LoadProgram({NOP}));
//   - QueueVif1Mscal/Mscalf/Mscnt records the commands to fire;
//   - SetGifPath1Busy / SetVif1WaitForVu / SetVif1Doublebuffer pre-stage
//     the VIF1 state each pass should see;
//   - Run() fires both passes — once with CpuVU1 = CpuMicroVU1 (JIT VU1),
//     once with CpuVU1 = CpuIntVU1 — and snapshots VU1 + dispatch-side
//     state on each side, asserting they agree.
//
// Path-1-busy is faked via gif_test_hooks::g_force_path1_busy, the same
// PCSX2_RECOMPILER_TESTS-gated stub the XGKICK suite uses.

#include "harness/EeRecTestHarness.h"
#include "harness/VuTestHarness.h"

#include "VU.h"
#include "Vif.h"

#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace mips;
using namespace mips::ee;
using namespace vu;

namespace {

// VADD vf2, vf1, vf0 (xyzw) — the canonical "kick fired" assertion: vf2
// holds vf1 + vf0 = (vf1.x, vf1.y, vf1.z, vf1.w + 1) after one program run.
constexpr u32 kVaddVf2Vf1Vf0 = VADD_U(/*mask*/ mask::xyzw, /*fd*/ 2, /*fs*/ 1, /*ft*/ 0);

VuOp VaddPair() { return VuOp{VLitZero(), kVaddVf2Vf1Vf0}; }

} // namespace

// MSCAL with imm=0 kicks a VU1 microprogram starting at VU1.Micro byte 0.
// Verifies the dispatch path runs the program to E-bit termination on both
// the JIT and interp VU1 engines, and both arrive at the same vf2.
TEST(EeVu1Vif, Mscal_KickMicroProgramAtZero)
{
	EeRecTestHarness h;
	h.EnableVu1VifCapture();
	h.SeedVu1Microprogram(0, {VaddPair(), EBitNopPair(), NopPair()});
	h.SeedVu1Vf(1, 5.0f, 7.0f, 11.0f, 0.0f);
	h.LoadProgram({NOP});
	h.QueueVif1Mscal(0);
	h.Run();
	EXPECT_TRUE(h.HasVu1TerminatedJit());
	EXPECT_TRUE(h.HasVu1TerminatedInterp());
	h.ExpectVu1Vf(2, 5.0f, 7.0f, 11.0f, 1.0f); // vf0.w = 1.0 hardware constant
}

// MSCALF stalls if GIF path 1 is busy: vifCode_MSCALF sets vif1Regs.stat.VGW
// and DMA-stalls without dispatching the microprogram. Both engines must
// agree on the stall and leave vf2 untouched.
TEST(EeVu1Vif, Mscalf_StallsOnGifPath1Busy)
{
	EeRecTestHarness h;
	h.EnableVu1VifCapture();
	h.SeedVu1Microprogram(0, {VaddPair(), EBitNopPair(), NopPair()});
	h.SeedVu1Vf(1, 5.0f, 7.0f, 11.0f, 0.0f);
	h.SeedVu1Vf(2, 0.0f, 0.0f, 0.0f, 0.0f);
	h.LoadProgram({NOP});
	h.SetGifPath1Busy(true);
	h.QueueVif1Mscalf(0);
	h.Run();
	// Microprogram never ran — vf2 holds its seeded zero value.
	h.ExpectVu1Vf(2, 0.0f, 0.0f, 0.0f, 0.0f);
	// VGW (bit 3 of stat) must be set on both sides.
	EXPECT_NE(h.GetVif1StatJit()    & VIF1_STAT_VGW, 0u);
	EXPECT_NE(h.GetVif1StatInterp() & VIF1_STAT_VGW, 0u);
}

// MSCNT (vu1ExecMicro with addr=-1) keeps VU1.VI[REG_TPC] from the previous
// kick. Inject MSCAL(0) followed by MSCNT against a 2-pair program; the
// program runs once for MSCAL (vf2 += vf0), terminates at TPC=2, then MSCNT
// resumes from TPC=2, walks the 0x7FE zero-pairs to wrap-around, and re-hits
// the program at byte 0 — vf2 ends up with two increments.
TEST(EeVu1Vif, Mscnt_ReusesPreviousMicroprogramPC)
{
	EeRecTestHarness h;
	h.EnableVu1VifCapture();
	// vf2.xyzw += vf0.w (=1) in each lane via VADDw. Two passes → +2.
	const u32 vaddw = VADDw_U(mask::xyzw, /*fd*/ 2, /*fs*/ 2, /*ft*/ 0);
	h.SeedVu1Microprogram(0, {VuOp{VLitZero(), vaddw}, EBitNopPair(), NopPair()});
	h.SeedVu1Vf(2, 0.0f, 0.0f, 0.0f, 0.0f);
	h.LoadProgram({NOP});
	h.QueueVif1Mscal(0);
	h.QueueVif1Mscnt();
	h.Run();
	// Program ran twice — vf2 incremented from 0 to 2 in every lane.
	h.ExpectVu1Vf(2, 2.0f, 2.0f, 2.0f, 2.0f);
}

// vif1.waitforvu blocks subsequent VIF1 dispatch — vifCode_MSCAL pass1 sets
// DMASTALL and returns without queuing the microprogram. Both engines must
// agree: program never runs, waitforvu still latched.
TEST(EeVu1Vif, VifWaitForVuFromInflightMicroprogram)
{
	EeRecTestHarness h;
	h.EnableVu1VifCapture();
	h.SeedVu1Microprogram(0, {VaddPair(), EBitNopPair(), NopPair()});
	h.SeedVu1Vf(1, 5.0f, 7.0f, 11.0f, 0.0f);
	h.SeedVu1Vf(2, 0.0f, 0.0f, 0.0f, 0.0f);
	h.LoadProgram({NOP});
	h.SetVif1WaitForVu(true);
	h.QueueVif1Mscal(0);
	h.Run();
	// Microprogram never ran — vf2 unchanged.
	h.ExpectVu1Vf(2, 0.0f, 0.0f, 0.0f, 0.0f);
}

// Double-buffered TOP: with stat.DBF clear and two MSCALs, vifRegs.tops
// alternates between (base+ofst) and base, while vifRegs.top latches the
// pre-swap tops on each kick. After two MSCALs:
//   kick #1 — top = 0x000 (initial), tops = base+ofst, DBF = 1
//   kick #2 — top = base+ofst,        tops = base,      DBF = 0
TEST(EeVu1Vif, DoubleBufferedTopAdvancesOnEachKick)
{
	EeRecTestHarness h;
	h.EnableVu1VifCapture();
	h.SeedVu1Microprogram(0, {EBitNopPair(), NopPair()});
	h.LoadProgram({NOP});
	h.SetVif1Doublebuffer(/*base*/ 0x10, /*ofst*/ 0x20);
	h.QueueVif1Mscal(0);
	h.QueueVif1Mscal(0);
	h.Run();
	EXPECT_EQ(h.GetVif1TopJit(),    0x30u); // base+ofst (latched at kick #2)
	EXPECT_EQ(h.GetVif1TopInterp(),  0x30u);
	EXPECT_EQ(h.GetVif1TopsJit(),   0x10u); // base (DBF flipped back)
	EXPECT_EQ(h.GetVif1TopsInterp(), 0x10u);
}

} // namespace recompiler_tests
