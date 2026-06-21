// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "harness/JitTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kPark = RecompilerTestEnvironment::kParkingPc;
constexpr u32 kProg = RecompilerTestEnvironment::kProgramPc;
}

// A block that ends with `jr ra` where ra was pre-set to the parking lot.
// Uses the harness's auto-appended terminator; primary purpose is to
// confirm the test-lifecycle plumbing works without the harness's own
// synthetic terminator getting in the way.
TEST(IopJump, JrToParkingLot)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xCAFEu);
	h.LoadProgram({ORI(reg::v0, reg::a0, 0)});   // simple copy
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xCAFEu);
	// After the JR ra; nop terminator, pc should sit inside the parking
	// lot's own `j kPark; nop` infinite loop.
	EXPECT_EQ(h.InterpSnapshot().regs.pc, kPark);
}

TEST(IopJump, JalSetsLinkRegister)
{
	JitTestHarness h;
	// jal writes PC+8 (delay slot return) into r31 = ra.
	// Jump to kPark (the parking lot's `j self` instruction) so execution
	// sticks in a controlled loop instead of drifting into uninitialized
	// memory past the parking lot.
	h.LoadProgramNoTerm({
		JAL(kPark),            // target = parking lot head; returns kProg+8 into ra
		NOP,                    // delay slot (always executed)
		NOP, NOP, NOP,          // filler (not reached)
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::ra), kProg + 8);
}

TEST(IopJump, JalrReturnLinkCustomRd)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, kPark);   // target — parking lot head
	h.LoadProgramNoTerm({
		JALR(reg::v0, reg::a0), // link into v0
		NOP,                     // delay slot
	});
	h.Run();
	// v0 should hold the return address = instruction after delay slot.
	EXPECT_EQ(h.GetGprInterp(reg::v0), kProg + 8);
}

TEST(IopJump, JExactTarget)
{
	JitTestHarness h;
	h.LoadProgramNoTerm({
		J(kPark),
		NOP,
	});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.pc, kPark);
}
