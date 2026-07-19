// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Self-modifying-code coverage for the EE recompiler.
//
// Parallels iop_smc_tests.cpp — which caught a real psxRecClearMem
// merge-semantics bug during IOP port testing. The same chain applies to
// the EE: memWrite → vtlb store → Cpu->Clear → recClear invalidates the
// cached block, next dispatch re-compiles.
//
// These tests are architecturally correct and serve as JIT regression gates
// for block compilation.

#include "harness/EeRecTestHarness.h"

#include "Memory.h"
#include "R5900.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kProgramPc = RecompilerTestEnvironment::kProgramPc;
} // namespace

TEST(EeRecSmc, GuestSwOverwritesInstructionAheadOfPc)
{
	// Block layout:
	//   0x00: LUI a1, hi(ADDIU v0, zero, 0x1337)
	//   0x04: ORI a1, a1, lo(...)
	//   0x08: SW a1, 0(a0)                  — self-modify the word at 0x10
	//   0x0C: NOP (alignment / give SW time to settle)
	//   0x10: ADDIU v0, zero, 0x0BAD        — replaced before we get here
	//   0x14: JR ra / NOP — appended by LoadProgram
	//
	// Interp-only. A guest SW into a page holding compiled code must
	// invalidate the covering block, but the test harness does not wire
	// page-protection SIGSEGV backpatching, so a guest SW into a compiled
	// page does not auto-invalidate. The harness-driven TriggerSmc() case
	// (next test) still works because it calls memWrite32 + recClear from
	// the *host* side; only guest-emitted SW misses here.
	constexpr u32 kNewInstr = ADDIU(reg::v0, reg::zero, 0x1337);
	const u16 hi = static_cast<u16>(kNewInstr >> 16);
	const u16 lo = static_cast<u16>(kNewInstr & 0xFFFFu);

	EeRecTestHarness h;
	h.SetGpr64(reg::a0, kProgramPc + 0x10);
	h.LoadProgram({
		LUI(reg::a1, hi),
		ORI(reg::a1, reg::a1, lo),
		SW(reg::a1, 0, reg::a0),              // overwrite insn @ 0x10
		NOP,
		ADDIU(reg::v0, reg::zero, 0x0BAD),    // becomes ADDIU v0,0x1337
	});
	h.RunInterpOnly();
	h.ExpectGpr64(reg::v0, 0x1337ull);
}

TEST(EeRecSmc, TriggerSmcHelperRewritesMemory)
{
	// Demonstrates the TriggerSmc harness helper. Program reads the word
	// at kProgramPc+0x100 (which the helper rewrites pre-run) into v0 and
	// returns. Tests the "harness rewrites then jits" discipline that
	// iop_smc_tests uses for programmatic SMC fixtures.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, kProgramPc + 0x100);
	h.LoadProgram({
		LW(reg::v0, 0, reg::a0),
	});
	h.TriggerSmc(kProgramPc + 0x100, 0xDEADBEEFu);
	h.TrackMemWindow(kProgramPc + 0x100, 4);
	h.Run();
	// LW sign-extends, 0xDEADBEEF has bit 31 set, so v0 is sign-extended.
	h.ExpectGpr64(reg::v0, 0xFFFFFFFFDEADBEEFull);
}

TEST(EeRecSmc, RewriteAdjacentWordDoesNotAffectCurrentInstruction)
{
	// Write to the word *after* the last real instruction. No effect on
	// the currently-executing block. Guard against an over-aggressive
	// Cpu->Clear implementation that invalidates unrelated words.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, kProgramPc + 0x200);
	h.LoadProgram({
		ADDIU(reg::v0, reg::zero, 42),
		SW(reg::v0, 0, reg::a0),                 // write beyond program
	});
	h.TrackMemWindow(kProgramPc + 0x200, 4);
	h.Run();
	h.ExpectGpr64(reg::v0, 42ull);
	EXPECT_EQ(h.ReadU32(kProgramPc + 0x200), 42u);
}

// Regression gate for recClear straddler-block fnptr reset.
//
// The bug: recClear's per-word reset loop only visited words inside
// [addr, end), missing block STARTs that lie before addr but whose body
// extends into the cleared range. Combined with Arm64BaseBlocks::Remove()
// patching only the compiled-code stub, this left BLOCK(startpc)->fnptr
// pointing at the just-overwritten stub. The next dispatch from startpc
// followed the stub's `B JITCompile` redirect into JITCompile, which then
// tripped the recRecompile fnptr assertion because BLOCK->fnptr was the
// stub address, not JITCompile. BLOCK(startpc)->fnptr must be reset to the
// JIT-compile entry so re-dispatch recompiles cleanly.
//
// The production trigger is the fastmem-backpatch path: vtlb calls
// Cpu->Clear(guest_pc, 1) with a MID-block PC. SimulateFastmemFault()
// mimics that single production entry.
TEST(EeRecSmc, StraddlerBlockRecClearResetsStartFnptr)
{
	EeRecTestHarness h;

	// 31-instruction block (block extent: kProgramPc..kProgramPc+0x7C, plus
	// the harness-appended JR ra/NOP at +0x7C/+0x80). Long enough that any
	// mid-block fault PC < endpc is the straddler-from-below scenario.
	std::initializer_list<u32> program = {
		ADDIU(reg::v0, reg::zero, 0),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
		ADDIU(reg::v0, reg::v0, 0x100), ADDIU(reg::v0, reg::v0, 0x100),
	};
	h.LoadProgram(program);

	// First pass: compile, run JIT + interp from a fresh cache, diff.
	// v0 = 30 * 0x100 = 0x1E00.
	h.Run(EeRecTestHarness::RunMode::FreshCache);
	h.ExpectGpr64(reg::v0, 0x1E00ull);

	// Single-word Clear at instruction index 16 (offset 0x40) — well inside
	// the block (block startpc=0..endpc=0x7C). The straddler-from-below case.
	h.SimulateFastmemFault(kProgramPc + 0x40);

	// Second pass: re-dispatch from kProgramPc. Block was invalidated mid-
	// extent. BLOCK(startpc)->fnptr must be reset to JITCompile so dispatch
	// recompiles cleanly; if it remains stale, the recRecompile fnptr
	// assertion fires (process abort).
	h.Run(EeRecTestHarness::RunMode::PreserveCache);
	h.ExpectGpr64(reg::v0, 0x1E00ull);
}

// ===========================================================================
//  GE-18: recRAMCopy overlap check.
//
//  x86 model (ix86-32/iR5900.cpp:2636-2661): after compiling a block, walk
//  older blocks overlapping its range and memcmp each old block's
//  recRAMCopy snapshot against live memory; a mismatch means that old block
//  went stale through a write no protection path caught — recClear it.
//  The arm64 port disabled the walk (it compared the wrong region and
//  recompile-looped); only the snapshot memcpy survived.
//
//  The harness wires no page protection, so a raw PSM poke of a compiled
//  non-manual block leaves it genuinely stale — exactly the class the walk
//  exists to catch at the next overlapping compile.
// ===========================================================================

namespace {
// A plain RAM page well away from the program/scratch/manual regions.
constexpr u32 kOverlapRoutinePc = 0x00090000;

void SeedOverlapRoutine(EeRecTestHarness& h, u16 imm)
{
	h.WriteU32(kOverlapRoutinePc + 0, ADDIU(reg::v0, reg::zero, imm));
	h.WriteU32(kOverlapRoutinePc + 4, JR(reg::ra));
	h.WriteU32(kOverlapRoutinePc + 8, NOP);
}

// Caller preserving the harness return address around a JAL to `target`.
void LoadOverlapCaller(EeRecTestHarness& h, u32 target, u16 sentinel)
{
	h.LoadProgram({
		OR(reg::t0, reg::ra, reg::zero),
		ADDIU(reg::v0, reg::zero, sentinel),
		JAL(target),
		NOP,
		OR(reg::ra, reg::t0, reg::zero),
	});
}
} // namespace

TEST(EeRecSmc, OverlappingCompileClearsStaleBlock)
{
	EeRecTestHarness h;

	// 1. Compile + run the routine: v0 = 5. Its source is snapshotted into
	//    recRAMCopy at compile time.
	SeedOverlapRoutine(h, 5);
	LoadOverlapCaller(h, kOverlapRoutinePc, 0x111);
	h.Run(EeRecTestHarness::RunMode::FreshCache);
	h.ExpectGpr64(reg::v0, 5);

	// 2. RAW poke (no memWrite32, no recClear — the write class no
	//    protection path sees in the harness): v0 = 7 semantics now in RAM,
	//    but the compiled block still encodes v0 = 5.
	*(u32*)PSM(kOverlapRoutinePc) = ADDIU(reg::v0, reg::zero, 7);

	// 3. Compile an OVERLAPPING block by entering mid-routine (at the JR).
	//    The overlap walk must see the stale older block and recClear it.
	//    v0 keeps the caller sentinel through the JR ra block.
	LoadOverlapCaller(h, kOverlapRoutinePc + 4, 0x222);
	h.Run(EeRecTestHarness::RunMode::PreserveCache);
	h.ExpectGpr64(reg::v0, 0x222);

	// 4. Re-enter the routine at its head: a cleared block recompiles from
	//    current memory (v0 = 7); a stale survivor still executes v0 = 5.
	LoadOverlapCaller(h, kOverlapRoutinePc, 0x333);
	h.Run(EeRecTestHarness::RunMode::PreserveCache);
	h.ExpectGpr64(reg::v0, 7);
}

TEST(EeRecSmc, OverlapWalkIgnoresUnmodifiedNeighbors)
{
	// Guard for the disable-comment's recompile-loop concern: compiling an
	// overlapping block over an UNMODIFIED older block must NOT clear it.
	// (The old bug compared the new block's not-yet-snapshotted region —
	// always-mismatch — which recClear-looped. The x86 walk compares old
	// blocks' own snapshots, which match untouched memory.)
	EeRecTestHarness h;

	SeedOverlapRoutine(h, 9);
	LoadOverlapCaller(h, kOverlapRoutinePc, 0x111);
	h.Run(EeRecTestHarness::RunMode::FreshCache);
	h.ExpectGpr64(reg::v0, 9);

	// Overlapping compile with NO modification anywhere.
	LoadOverlapCaller(h, kOverlapRoutinePc + 4, 0x222);
	h.Run(EeRecTestHarness::RunMode::PreserveCache);
	h.ExpectGpr64(reg::v0, 0x222);

	// The head block still runs (recompiled or cached — result identical
	// either way; the real assertion is we didn't recompile-loop above and
	// the block graph stayed sane).
	LoadOverlapCaller(h, kOverlapRoutinePc, 0x333);
	h.Run(EeRecTestHarness::RunMode::PreserveCache);
	h.ExpectGpr64(reg::v0, 9);
}
