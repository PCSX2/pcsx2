// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// COP0 + RFE coverage for the IOP recompiler. RFE emits
//   Status = (Status & 0xfffffff0) | ((Status & 0x3c) >> 2)
// matching the interpreter at pcsx2/R3000AOpcodeTables.cpp:160.
//
// Full exception dispatch (psxException → jump to 0x80000080, Status stack
// push) is NOT tested here — the harness suppresses the event scheduler so
// a manually-poked Cause/Status combo won't fire.

#include "harness/JitTestHarness.h"

#include "R3000A.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

TEST(IopCop0Exception, RfeShiftsStackedBitsIntoCurrent)
{
	// Status = 0x0000003C (bits 5:2 = 0b1111, bits 1:0 = 0).
	// RFE result expected:
	//   bits 3:0 ← (Status & 0x3c) >> 2  = 0xF
	//   bits 5:4 unchanged                 = 0b11
	//   → Status = 0x3F
	JitTestHarness h;
	h.SetCp0(12, 0x0000003Cu);
	h.LoadProgram({RFE});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.CP0.r[12], 0x0000003Fu);
	EXPECT_EQ(h.JitSnapshot().regs.CP0.r[12], 0x0000003Fu);
}

TEST(IopCop0Exception, RfeOnlySinglePositionShift)
{
	// Check that RFE shifts one position, not two. Status=0x10 (only
	// bit 4 = IEo set).
	//   (Status & 0x3c) >> 2 = 0x10 >> 2 = 0x04 → bit 2 set
	//   bits 5:4 unchanged → 0x10
	//   → Status = 0x14
	JitTestHarness h;
	h.SetCp0(12, 0x00000010u);
	h.LoadProgram({RFE});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.CP0.r[12], 0x00000014u);
}

TEST(IopCop0Exception, RfePreservesHighStatusBits)
{
	// Status = 0x12340030 — bits 31:6 should be entirely untouched by RFE.
	// bits 5:2 = 0b1100 → bits 3:0 = 0b1100 = 0xC; bits 5:4 unchanged.
	// → 0x1234003C
	JitTestHarness h;
	h.SetCp0(12, 0x12340030u);
	h.LoadProgram({RFE});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.CP0.r[12], 0x1234003Cu);
	EXPECT_EQ(h.JitSnapshot().regs.CP0.r[12], 0x1234003Cu);
}

TEST(IopCop0Exception, MtcThenRfePipelinesThroughGpr)
{
	// MTC0 a0 -> Status; RFE; MFC0 v0 <- Status. End-to-end through the
	// whole COP0 pipeline. Pre-state a0 = 0x3C, expected v0 = 0x3F.
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x0000003Cu);
	h.LoadProgram({
		MTC0(reg::a0, 12),   // Status <- a0 (0x3C)
		RFE,                  // Status = (Status & ~0xf) | ((Status & 0x3c)>>2) = 0x3F
		MFC0(reg::v0, 12),   // v0 <- Status
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x0000003Fu);
	EXPECT_EQ(h.GetGprJit(reg::v0), 0x0000003Fu);
}

TEST(IopCop0Exception, CauseRegisterIndependentOfRfe)
{
	// RFE operates on Status only. Cause must be untouched. Seed Cause
	// with an arbitrary pattern and verify it survives RFE.
	JitTestHarness h;
	h.SetCp0(12, 0x0000003Cu);
	h.SetCp0(13, 0xCAFEBABEu);                 // Cause
	h.SetCp0(14, 0x12345678u);                 // EPC
	h.LoadProgram({RFE});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.CP0.r[12], 0x0000003Fu) << "Status";
	EXPECT_EQ(h.InterpSnapshot().regs.CP0.r[13], 0xCAFEBABEu) << "Cause unchanged";
	EXPECT_EQ(h.InterpSnapshot().regs.CP0.r[14], 0x12345678u) << "EPC unchanged";
}

TEST(IopCop0Exception, TwoBackToBackRfesStackAgain)
{
	// Run RFE twice in one block. Each shifts (Status & 0x3c) >> 2 into
	// bits 3:0. Starting from 0x3C:
	//   After RFE #1: 0x3F   (bits 3:0 = 0xF copied from 5:2)
	//   After RFE #2: 0x3F   (bits 3:0 still 0xF copied from 5:2 = 0xF)
	// The second RFE is idempotent given this starting Status because the
	// stacked bits don't clear. Worth pinning as spec.
	JitTestHarness h;
	h.SetCp0(12, 0x0000003Cu);
	h.LoadProgram({RFE, RFE});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.CP0.r[12], 0x0000003Fu);
	EXPECT_EQ(h.JitSnapshot().regs.CP0.r[12], 0x0000003Fu);
}

// ---------------- SYSCALL / BREAK full dispatch ----------------
//
// Unlike interrupt exceptions (suppressed event scheduler — see file
// header), SYSCALL and BREAK are synchronous: the opcode itself calls
// psxException, which pushes the Status stack, latches Cause/EPC, and
// unconditionally vectors to 0x80000080 (BEV=0). A `jr ra` stub planted at
// the vector returns to the parking lot, giving a well-defined post-state.
// The JIT-vs-interp auto-diff guards the whole dispatch path — including
// that the recompiler's post-psxException re-dispatch takes the exception
// vector rather than falling through into the rest of the block.

namespace
{
	void InstallIopSyscallVectorStub(JitTestHarness& h)
	{
		h.LoadProgramAt(0x80000080u, {JR(reg::ra), NOP});
	}
} // namespace

TEST(IopCop0Exception, SyscallVectorsAndSkipsRestOfBlock)
{
	JitTestHarness h;
	InstallIopSyscallVectorStub(h);
	h.LoadProgram({
		SYSCALL_(),
		ADDIU(reg::v0, reg::zero, 99), // must NOT execute
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0u);
	// ExcCode=8 → Cause & 0x7C = 0x20; EPC = the SYSCALL itself.
	EXPECT_EQ(h.InterpSnapshot().regs.CP0.n.Cause & 0x7Cu, 0x20u);
	EXPECT_EQ(h.JitSnapshot().regs.CP0.n.Cause & 0x7Cu, 0x20u);
	EXPECT_EQ(h.InterpSnapshot().regs.CP0.n.EPC, RecompilerTestEnvironment::kProgramPc);
	EXPECT_EQ(h.JitSnapshot().regs.CP0.n.EPC, RecompilerTestEnvironment::kProgramPc);
}

TEST(IopCop0Exception, BreakVectorsAndSkipsRestOfBlock)
{
	JitTestHarness h;
	InstallIopSyscallVectorStub(h);
	h.LoadProgram({
		BREAK,
		ADDIU(reg::v0, reg::zero, 99), // must NOT execute
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0u);
	// ExcCode=9 → Cause & 0x7C = 0x24.
	EXPECT_EQ(h.InterpSnapshot().regs.CP0.n.Cause & 0x7Cu, 0x24u);
	EXPECT_EQ(h.JitSnapshot().regs.CP0.n.Cause & 0x7Cu, 0x24u);
	EXPECT_EQ(h.InterpSnapshot().regs.CP0.n.EPC, RecompilerTestEnvironment::kProgramPc);
	EXPECT_EQ(h.JitSnapshot().regs.CP0.n.EPC, RecompilerTestEnvironment::kProgramPc);
}
