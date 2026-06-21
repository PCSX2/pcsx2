// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "harness/JitTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

TEST(IopCop0, Mfc0ReadsStatus)
{
	JitTestHarness h;
	// Bit 16 (IsolateCache / IsC) in CP0.Status *must be clear* — otherwise
	// iopMemWrite32 silently discards the LoadProgram bytes and the program executes
	// zeros (nops). 0x12340EEF has bit 16 clear.
	h.SetCp0(12, 0x12340EEFu);    // CP0.Status
	h.LoadProgram({MFC0(reg::v0, 12)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x12340EEFu);
}

TEST(IopCop0, Mfc0ReadsEpc)
{
	JitTestHarness h;
	h.SetCp0(14, 0xDEADBEEFu);    // EPC has no IsC interaction
	h.LoadProgram({MFC0(reg::v0, 14)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xDEADBEEFu);
}

TEST(IopCop0, Mfc0ReadsToA0)
{
	JitTestHarness h;
	h.SetCp0(13, 0x77777777u);    // Cause register — no IsC interaction
	h.LoadProgram({MFC0(reg::a0, 13)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::a0), 0x77777777u);
}

TEST(IopCop0, Mtc0WritesEpc)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x12345678u);
	h.LoadProgram({MTC0(reg::a0, 14)});  // CP0.EPC
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.CP0.r[14], 0x12345678u);
}

TEST(IopCop0, MtcThenMfcRoundtrip)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xABCDEF01u);
	h.LoadProgram({
		MTC0(reg::a0, 13),    // CP0.Cause
		MFC0(reg::v0, 13),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xABCDEF01u);
}

// Register-pressure regression guards for COP0 moves: rpsxMFC0/MTC0 use
// allocator-aware Rt access (MODE_WRITE alloc / source read) rather than
// flushing the whole host-reg pool. JitTestHarness::Run() diffs JIT vs interp
// across all fields including CP0, so the failure mode — MTC0 reading a
// dirty-allocated Rt from stale GPR memory — surfaces as a divergence here.
// The earlier MTC0 tests set Rt via SetGpr (memory already current), so they
// cannot catch this.

TEST(IopCop0, Mtc0ReadsDirtyAllocatedRt)
{
	// LW writes its result straight to GPR memory, so to get a dirty-in-host-reg
	// Rt, the program follows it with an ALU op (non-const operands): ADDU writes its result
	// MODE_WRITE into a host reg and leaves psxRegs.GPR.r[t0] memory stale. MTC0
	// must read that host reg, not memory.
	JitTestHarness h;
	constexpr u32 kData = RecompilerTestEnvironment::kScratchAddr;
	h.SetGpr(reg::a0, kData);
	h.WriteU32(kData, 0x10000001u);
	h.LoadProgram({
		LW(reg::t1, 0, reg::a0),          // t1 = 0x10000001 (in memory)
		ADDU(reg::t0, reg::t1, reg::t1),  // t0 = 0x20000002, dirty in host reg
		MTC0(reg::t0, 14),                // CP0.EPC = t0
		MFC0(reg::v0, 14),                // v0 = CP0.EPC
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x20000002u);
	EXPECT_EQ(h.InterpSnapshot().regs.CP0.r[14], 0x20000002u);
}

TEST(IopCop0, Mfc0ResultFeedsLaterUse)
{
	// MFC0 writes v0 (MODE_WRITE alloc); a following ADDIU consumes it. Confirms
	// the written value propagates to later uses without a host-reg flush.
	JitTestHarness h;
	h.SetCp0(14, 0x0000002Au);    // EPC = 42
	h.LoadProgram({
		MFC0(reg::v0, 14),            // v0 = CP0.EPC = 42
		ADDIU(reg::v1, reg::v0, 1),   // v1 = v0 + 1 = 43
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x2Au);
	EXPECT_EQ(h.GetGprInterp(reg::v1), 0x2Bu);
}
