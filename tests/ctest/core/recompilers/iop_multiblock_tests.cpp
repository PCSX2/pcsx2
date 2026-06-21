// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Multi-block dispatch tests for the IOP recompiler. A block ends with a
// branch, ExecuteBlock returns, the dispatcher looks up the next block via
// psxRecLUT, and compiles it on first hit. These tests exercise that
// cross-block dispatch path.
//
// The branch tests in iop_branch_tests.cpp exercise cross-block flow within
// a single LoadProgramNoTerm() layout. These tests load each block at a
// distinct address via LoadProgramAt(), so the dispatcher re-dispatch is
// obvious and distinct recBlocks entries are created.

#include "harness/JitTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kProgramPc = RecompilerTestEnvironment::kProgramPc;   // 0x00010000
constexpr u32 kParkingPc = RecompilerTestEnvironment::kParkingPc;   // 0x001F0000

// Second / third blocks at distinct pages from the primary program region.
// Chosen well outside [kProgramPc, kProgramPc + 4KB) and inside legal
// 16-bit BEQ range from the primary block (offset ≤ 0x3FFF words).
constexpr u32 kBlock2Pc = 0x00014000;
constexpr u32 kBlock3Pc = 0x00018000;

constexpr s16 ShortBranchOffset(u32 branch_pc, u32 target_pc)
{
	// MIPS target = (branch_pc + 4) + (offset << 2).
	return static_cast<s16>((static_cast<s32>(target_pc) - static_cast<s32>(branch_pc + 4)) / 4);
}
} // namespace

TEST(IopMultiBlock, JAcrossBlocksPropagatesState)
{
	// Block 1 sets v0, jumps to block 2; block 2 adds to v0 and returns.
	JitTestHarness h;
	h.LoadProgramAt(kProgramPc, {
		ADDIU(reg::v0, reg::zero, 10),   // v0 = 10
		J(kBlock2Pc),
		NOP,                             // delay slot
	}, /*append_jr_ra_term=*/false);
	h.LoadProgramAt(kBlock2Pc, {
		ADDIU(reg::v0, reg::v0, 100),    // v0 = v0 + 100
	}, /*append_jr_ra_term=*/true);
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 110u);
}

TEST(IopMultiBlock, DelaySlotAcrossBlockBoundary)
{
	// The J in block 1 has a non-trivial delay slot that mutates v0. The
	// MIPS arch guarantees the delay-slot instruction executes before the
	// control transfer finishes, so v0 is 15 when block 2 begins.
	JitTestHarness h;
	h.LoadProgramAt(kProgramPc, {
		ADDIU(reg::v0, reg::zero, 10),
		J(kBlock2Pc),
		ADDIU(reg::v0, reg::v0, 5),      // delay slot: v0 = 15
	}, /*append_jr_ra_term=*/false);
	h.LoadProgramAt(kBlock2Pc, {
		ADDIU(reg::v0, reg::v0, 100),    // v0 = 115
	}, /*append_jr_ra_term=*/true);
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 115u);
}

TEST(IopMultiBlock, JalLinksToReturnAddress)
{
	// JAL stores PC_of_JAL + 8 into ra. Block 2 reads ra into v0 and
	// returns. Pre-set s0 = kParkingPc so block 1's continuation can
	// jr to the parking lot without clobbering the JAL's linkage.
	JitTestHarness h;
	h.SetGpr(reg::s0, kParkingPc);
	h.LoadProgramAt(kProgramPc, {
		JAL(kBlock2Pc),                     // 0x10000: ra = 0x10008
		NOP,                                 // 0x10004: delay slot
		// 0x10008: block 2 returns here
		ADDU(reg::v1, reg::ra, reg::zero),  // v1 = ra (= 0x10008)
		JR(reg::s0),                         // jr parking
		NOP,
	}, /*append_jr_ra_term=*/false);
	h.LoadProgramAt(kBlock2Pc, {
		ADDU(reg::v0, reg::ra, reg::zero),  // v0 = ra (captured)
		JR(reg::ra),
		NOP,
	}, /*append_jr_ra_term=*/false);
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), kProgramPc + 8);
	EXPECT_EQ(h.GetGprInterp(reg::v1), kProgramPc + 8);
}

TEST(IopMultiBlock, BeqAcrossBlocksTaken)
{
	// BEQ within legal offset range to block 2.
	JitTestHarness h;
	h.SetGpr(reg::a0, 42);
	h.SetGpr(reg::a1, 42);
	const s16 off = ShortBranchOffset(kProgramPc, kBlock2Pc);
	h.LoadProgramAt(kProgramPc, {
		BEQ(reg::a0, reg::a1, off),       // taken — go to block 2
		NOP,                               // delay slot
		ADDIU(reg::v0, reg::zero, 0xBAD), // fall-through (should not run)
		J(kParkingPc),
		NOP,
	}, /*append_jr_ra_term=*/false);
	h.LoadProgramAt(kBlock2Pc, {
		ADDIU(reg::v0, reg::zero, 0x600D),   // taken marker
	}, /*append_jr_ra_term=*/true);
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x600Du);
}

TEST(IopMultiBlock, BeqAcrossBlocksNotTaken)
{
	// BEQ with mismatched inputs — fall through. Block 2 is compiled only
	// if the branch fires, so in this test block 2 is never entered.
	JitTestHarness h;
	h.SetGpr(reg::a0, 42);
	h.SetGpr(reg::a1, 43);
	const s16 off = ShortBranchOffset(kProgramPc, kBlock2Pc);
	h.LoadProgramAt(kProgramPc, {
		BEQ(reg::a0, reg::a1, off),       // not taken
		NOP,                               // delay slot
		ADDIU(reg::v0, reg::zero, 0x0A11), // fall-through marker (positive 16-bit, no sign extend)
		J(kParkingPc),
		NOP,
	}, /*append_jr_ra_term=*/false);
	// Block 2 still laid down for safety (poison value so an accidental
	// dispatch is obvious).
	h.LoadProgramAt(kBlock2Pc, {
		ADDIU(reg::v0, reg::zero, 0xBAD),
	}, /*append_jr_ra_term=*/true);
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x0A11u);
}

TEST(IopMultiBlock, ThreeBlockChain)
{
	// Block 1 → block 2 → block 3 → parking. Each block adds to v0.
	JitTestHarness h;
	h.LoadProgramAt(kProgramPc, {
		ADDIU(reg::v0, reg::zero, 1),
		J(kBlock2Pc),
		NOP,
	}, /*append_jr_ra_term=*/false);
	h.LoadProgramAt(kBlock2Pc, {
		ADDIU(reg::v0, reg::v0, 10),
		J(kBlock3Pc),
		NOP,
	}, /*append_jr_ra_term=*/false);
	h.LoadProgramAt(kBlock3Pc, {
		ADDIU(reg::v0, reg::v0, 100),
	}, /*append_jr_ra_term=*/true);
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 111u);
}

TEST(IopMultiBlock, ReEnterFirstBlockAfterReturn)
{
	// Block 1 JALs block 2; block 2 returns; block 1's continuation runs
	// more instructions; block 1 exits via an explicit jump to parking.
	// Proves that after dispatcher re-enters block 1, the cached block is
	// still valid (or is recompiled from the correct address).
	JitTestHarness h;
	h.SetGpr(reg::s0, kParkingPc);
	h.SetGpr(reg::v0, 0);
	h.LoadProgramAt(kProgramPc, {
		ADDIU(reg::v0, reg::zero, 1),       // 0x10000: v0 = 1
		JAL(kBlock2Pc),                      // 0x10004: ra = 0x1000C
		NOP,                                 // 0x10008: delay slot
		// 0x1000C — block 2 returns here
		ADDIU(reg::v0, reg::v0, 100),        // v0 += 100
		JR(reg::s0),                         // parking
		NOP,
	}, /*append_jr_ra_term=*/false);
	h.LoadProgramAt(kBlock2Pc, {
		ADDIU(reg::v0, reg::v0, 10),         // v0 += 10
		JR(reg::ra),
		NOP,
	}, /*append_jr_ra_term=*/false);
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 111u);
}

TEST(IopMultiBlock, JalrThenReturn)
{
	// JALR variant (register-indirect jump + link). s0 holds block 2's
	// address; JALR jumps there and links ra.
	JitTestHarness h;
	h.SetGpr(reg::s0, kBlock2Pc);
	h.SetGpr(reg::s1, kParkingPc);
	h.LoadProgramAt(kProgramPc, {
		ADDIU(reg::v0, reg::zero, 5),
		JALR(reg::ra, reg::s0),              // ra = 0x1000C, jumps to s0
		NOP,                                  // delay slot
		// 0x1000C — return continuation
		ADDIU(reg::v0, reg::v0, 20),
		JR(reg::s1),
		NOP,
	}, /*append_jr_ra_term=*/false);
	h.LoadProgramAt(kBlock2Pc, {
		ADDIU(reg::v0, reg::v0, 300),
		JR(reg::ra),
		NOP,
	}, /*append_jr_ra_term=*/false);
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 325u);
}
