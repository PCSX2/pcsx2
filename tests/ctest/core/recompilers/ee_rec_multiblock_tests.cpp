// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Cross-block control-flow for the EE. ExpectBlockLinked queries the block
// link multimap (recEeIsBlockLinked -> Arm64BaseBlocks::IsLinked): it returns
// true when a SetBranchImm patch site inside the source block targets the
// destination block. The J/BEQ branch handlers emit SetBranchImm, which records
// such a link via recBlocks.Link, so the cross-block link assertions here are
// live — control flows Block A -> Block B and the static link site is present.
//
// Parallels iop_multiblock_tests.cpp.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kProgramPc = RecompilerTestEnvironment::kProgramPc;
constexpr u32 kPark      = RecompilerTestEnvironment::kParkingPc;

// Two distinct basic blocks landing in the same 4KB region but separated by
// enough space to be independently compiled.
constexpr u32 kBlockAPc = kProgramPc;
constexpr u32 kBlockBPc = kProgramPc + 0x100;
} // namespace

TEST(EeRecMultiblock, JCrossesBlockBoundary)
{
	// Block A: ADDIU v0, zero, 1; J B; NOP
	// Block B: ADDIU v0, zero, 2; J park; NOP
	// The `j B` must land at kBlockBPc and continue executing.
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		ADDIU(reg::v0, reg::zero, 1),
		J(kBlockBPc),
		NOP,
	});
	// Block B is a separate write into the program-region memory.
	h.WriteU32(kBlockBPc + 0, ADDIU(reg::v0, reg::zero, 2));
	h.WriteU32(kBlockBPc + 4, J(kPark));
	h.WriteU32(kBlockBPc + 8, NOP);
	h.Run();
	// If the J fell through (no cross-block transfer), v0 would be 1.
	h.ExpectGpr64(reg::v0, 2ull);
	// The J opcode handler emits SetBranchImm, which records a link patch site
	// (recBlocks.Link) within block A targeting block B. The J instruction lives
	// at kBlockAPc + 4.
	h.ExpectBlockLinked(kBlockAPc + 4, kBlockBPc);
}

TEST(EeRecMultiblock, BeqTakenCrossesBlockBoundary)
{
	// BEQ a0,a0,+offset (always taken) to block B. Exercises the branch-
	// imm-to-imm path through the dispatcher, which the JIT will short-
	// circuit via LinkArm64.
	//
	// Offset from PC+4 = kBlockBPc means offset = (0x100 / 4) = 0x40.
	// But BEQ offset is in words and relative to PC+4, so:
	//   target = (PC+4) + (offset * 4)
	//   0x100 = 4 + (offset * 4)  →  offset = 0x3F
	// Start at kBlockAPc (= kProgramPc), so PC+4 = kProgramPc+4, target
	// = kBlockBPc = kProgramPc + 0x100  →  offset*4 = 0xFC  →  offset = 0x3F.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 42);
	h.LoadProgramNoTerm({
		BEQ(reg::a0, reg::a0, 0x3F),     // always taken
		NOP,                              // delay slot
	});
	h.WriteU32(kBlockBPc + 0, ADDIU(reg::v0, reg::zero, 7));
	h.WriteU32(kBlockBPc + 4, J(kPark));
	h.WriteU32(kBlockBPc + 8, NOP);
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
	// The BEQ opcode handler emits SetBranchImm for the taken target, recording
	// a link patch site within block A (the BEQ lives at kBlockAPc).
	h.ExpectBlockLinked(kBlockAPc, kBlockBPc);
}

// Tight backward-branch loop. The scanner (recRecompile) sees the BNE at +12
// with a backward target of +4 (> startpc, < branch index), so it ends the
// block AT +4 — the loop head becomes its own linkable block. Block
// granularity is architecturally invisible, so this guards correctness of the
// truncated-block loop rather than RED-proving the split: a 3-iteration
// countdown must still produce a0=0, a1=3.
TEST(EeRecMultiblock, BackwardBranchLoopBlockSplit)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 0);
	h.LoadProgramNoTerm({
		ADDIU(reg::a0, reg::zero, 3),   // +0  counter = 3 (startpc)
		ADDIU(reg::a1, reg::a1, 1),     // +4  loop head (backward branch target)
		ADDIU(reg::a0, reg::a0, -1),    // +8  counter--
		BNE(reg::a0, reg::zero, -3),    // +12 branch back to +4 when a0 != 0
		NOP,                            // +16 delay slot (always executes)
		J(kPark),                       // +20 exit
		NOP,                            // +24 J delay slot
	});
	h.Run();
	h.ExpectGpr64(reg::a0, 0ull);
	h.ExpectGpr64(reg::a1, 3ull);
}

TEST(EeRecMultiblock, ThreeBlockChainExecutesInOrder)
{
	// Block A → B → C via J chain. Each block sets a different GPR so the
	// test can verify all three executed in the right order.
	constexpr u32 kBlockCPc = kProgramPc + 0x200;
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		ADDIU(reg::a0, reg::zero, 1),
		J(kBlockBPc),
		NOP,
	});
	h.WriteU32(kBlockBPc + 0, ADDIU(reg::a1, reg::zero, 2));
	h.WriteU32(kBlockBPc + 4, J(kBlockCPc));
	h.WriteU32(kBlockBPc + 8, NOP);
	h.WriteU32(kBlockCPc + 0, ADDIU(reg::a2, reg::zero, 3));
	h.WriteU32(kBlockCPc + 4, J(kPark));
	h.WriteU32(kBlockCPc + 8, NOP);
	h.Run();
	h.ExpectGpr64(reg::a0, 1ull);
	h.ExpectGpr64(reg::a1, 2ull);
	h.ExpectGpr64(reg::a2, 3ull);
}
