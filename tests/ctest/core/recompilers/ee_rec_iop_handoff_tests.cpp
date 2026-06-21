// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// EE → IOP handoff / cross-CPU SMC coverage.
//
// Real PS2 software uploads IOP code from the EE side via stores into the
// IOP RAM subsystem-bus window at 0x1C00_0000. The store chain:
//
//   EE SW                                        [interp step]
//     → vtlb handler for 0x1C00_0000 region
//     → _ext_memWrite32<9>(addr, v)
//     → iopMemWrite32(addr & ~0x1C00_0000, v)
//     → psxCpu->Clear(addr & ~3, 1)
//     → recClearIOP → psxRecClearMem
//     → iopClearRecLUT slot for the target page
//
// If any link is broken the IOP JIT dispatches the OLD cached block on
// re-entry at the same PC. These tests pin that end-to-end. The EE path
// runs through the EeRecTestHarness which executes both JIT and
// interp — where the EE rec delegates to the interpreter,
// the diff is tautological; where it emits real JIT code, that
// emission is exercised here.
//
// JitTestHarness ctor hoists `psxCpu = &psxRec` so `Clear` is meaningful
// (otherwise psxInt's Clear is a no-op). The test then drives the EE,
// then `iop.SetPc(...); iop.RunResume();` re-enters the IOP JIT WITHOUT
// force-invalidating — any stale block surfaces.

#include "harness/EeRecTestHarness.h"
#include "harness/JitTestHarness.h"

#include "IopMem.h"
#include "Memory.h"
#include "R3000A.h"

#include <cstring>
#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {

constexpr u32 kIopProgramPc = RecompilerTestEnvironment::kProgramPc;   // 0x0001'0000
constexpr u32 kIopBusWindow = 0x1C00'0000u;                             // EE view of IOP RAM

// EE virtual-address of an IOP physical address through the subsystem
// bus window. Works for direct (physical) and kseg mirrors via the VMap.
constexpr u32 EeToIop(u32 iop_addr) { return kIopBusWindow | iop_addr; }

// IOP opcode encodings the tests pin as spec. Computed here so a
// divergence is readable at the assertion site ("expected 0x24020064
// = ADDIU v0, zero, 100").
constexpr u32 kAddiuV0Zero100  = ADDIU(reg::v0, reg::zero, 100);   // 0x24020064
constexpr u32 kAddiuV0Zero200  = ADDIU(reg::v0, reg::zero, 200);   // 0x240200C8

} // namespace

// ---------------------------------------------------------------------------
//  Isolation: vtlb → iopMemWrite → psxCpu->Clear chain, NO EE CPU in flight
// ---------------------------------------------------------------------------
// This test deliberately does NOT step the EE interpreter. It calls the
// EE-side vtlb entry point (`memWrite32`) directly from C, which hits the
// same handler the EE SW would — memWrite32 → _ext_memWrite32<9>
// → iopMemWrite32 → psxCpu->Clear. The goal is to isolate the vtlb chain
// from the "EE CPU running → cpuEventTest → dispatch IOP JIT" path, which
// is what the other tests in this file go through. If this test passes
// but the stepping tests crash, the fault is in the cpuEventTest→IOP-JIT
// entry, not in the EE→IOP memory handoff itself.
TEST(EeRecIopHandoff, DirectMemWriteThroughEeVtlbInvalidatesIopJitBlock)
{
	JitTestHarness iop;
	iop.LoadProgram({
		ADDIU(reg::v0, reg::zero, 100),
	});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::v0), 100u)
		<< "initial compile + run should set v0 = 100";

	// Direct EE-side store — no intCpu.Step(), no cpuEventTest, no IOP
	// JIT dispatch via the EE scheduler. `memWrite32` takes an EE virtual
	// address; 0x1C01_0000 routes to the iop_memory vtlb handler which
	// ends up in iopMemWrite32(0x00010000, ...) → psxCpu->Clear.
	memWrite32(EeToIop(kIopProgramPc), kAddiuV0Zero200);

	ASSERT_EQ(iop.ReadU32(kIopProgramPc), kAddiuV0Zero200)
		<< "direct EE memWrite32 didn't reach IOP RAM — vtlb handler chain "
		   "is broken independent of any CPU stepping";

	// Re-enter only the IOP JIT. If psxCpu->Clear fired as part of the
	// store chain, the LUT slot is empty and the JIT re-compiles the new
	// opcode. If Clear was skipped (e.g. psxCpu pointed at interp whose
	// Clear is a no-op), the stale block runs and v0 stays 100.
	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::v0), 200u)
		<< "IOP JIT cache not invalidated by the vtlb → iopMemWrite → Clear "
		   "chain — even without EE CPU in flight";
}

TEST(EeRecIopHandoff, EeSwLandsInIopRam)
{
	// Foundation: the 0x1C00_0000 window is live and routes EE stores
	// to the IOP memory system. No IOP JIT state assertions — just
	// prove bytes reach IOP RAM.
	JitTestHarness iop;   // hoists psxCpu = &psxRec for the test scope
	EeRecTestHarness ee;
	ee.SetGpr(reg::a0, EeToIop(0x0000'1000u));
	ee.SetGpr(reg::a1, 0xDEAD'BEEFu);
	ee.LoadProgram({
		SW(reg::a1, 0, reg::a0),
	});
	ee.Run();

	EXPECT_EQ(iop.ReadU32(0x0000'1000u), 0xDEADBEEFu)
		<< "EE SW through 0x1C00_0000 bus window must land in IOP RAM";
}

TEST(EeRecIopHandoff, EeSwInvalidatesCachedIopBlock)
{
	// Seed and compile an IOP block that produces v0 = 100.
	JitTestHarness iop;
	iop.LoadProgram({
		ADDIU(reg::v0, reg::zero, 100),
	});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::v0), 100u)
		<< "initial compile + run should set v0 = 100";
	ASSERT_EQ(iop.ReadU32(kIopProgramPc), kAddiuV0Zero100)
		<< "program word not actually in IOP RAM";

	// From the EE, overwrite the first word with a 200-producing ADDIU.
	EeRecTestHarness ee;
	ee.SetGpr(reg::a0, EeToIop(kIopProgramPc));
	ee.SetGpr(reg::a1, kAddiuV0Zero200);
	ee.LoadProgram({
		SW(reg::a1, 0, reg::a0),
	});
	ee.Run();

	ASSERT_EQ(iop.ReadU32(kIopProgramPc), kAddiuV0Zero200)
		<< "EE SW did not land in IOP RAM — vtlb → iopMemWrite chain broken";

	// Re-enter the IOP JIT. If the EE's SW correctly triggered the
	// psxCpu->Clear → LUT evict chain, the dispatcher MUST re-compile
	// the new opcode at kIopProgramPc. If it didn't, v0 stays 100
	// (stale cached block).
	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::v0), 200u)
		<< "IOP JIT cache was not invalidated by EE-side SW — this is "
		   "the EE→IOP handoff path that IOP program upload relies on";
}

TEST(EeRecIopHandoff, EeSwThroughKseg0MirrorInvalidates)
{
	// EE writes to 0x9C00_0000 + off (kseg0 cached mirror of physical
	// 0x1C00_0000). The EE VMap pins 0x80..0x9F to physical 0..0x1F so the
	// store routes through the same iop_memory handler. Same outcome as the
	// physical-window test — lock that.
	JitTestHarness iop;
	iop.LoadProgram({ADDIU(reg::v0, reg::zero, 100)});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::v0), 100u);

	EeRecTestHarness ee;
	ee.SetGpr(reg::a0, 0x9C00'0000u | kIopProgramPc);  // kseg0 mirror
	ee.SetGpr(reg::a1, kAddiuV0Zero200);
	ee.LoadProgram({
		SW(reg::a1, 0, reg::a0),
	});
	ee.Run();

	ASSERT_EQ(iop.ReadU32(kIopProgramPc), kAddiuV0Zero200);

	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::v0), 200u);
}

TEST(EeRecIopHandoff, EeSbInvalidatesCachedIopBlock)
{
	// Byte-width SMC. MIPS is little-endian, so the low byte of the
	// ADDIU encoding is imm[7:0]. Replacing the byte at offset 0
	// switches the immediate from 100 (0x64) to 200 (0xC8) without
	// touching the rest of the instruction.
	JitTestHarness iop;
	iop.LoadProgram({ADDIU(reg::v0, reg::zero, 100)});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::v0), 100u);

	EeRecTestHarness ee;
	ee.SetGpr(reg::a0, EeToIop(kIopProgramPc));
	ee.SetGpr(reg::a1, 0xC8u);      // new imm low byte
	ee.LoadProgram({
		SB(reg::a1, 0, reg::a0),
	});
	ee.Run();

	ASSERT_EQ(iop.ReadU32(kIopProgramPc), kAddiuV0Zero200)
		<< "byte-width EE write did not surgically replace the low byte";

	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::v0), 200u)
		<< "byte-width SMC from EE did not invalidate the IOP JIT block";
}

TEST(EeRecIopHandoff, EeShInvalidatesCachedIopBlock)
{
	// Half-width SMC. Low 16 bits of the ADDIU encoding ARE the
	// immediate — replacing them switches the constant.
	JitTestHarness iop;
	iop.LoadProgram({ADDIU(reg::v0, reg::zero, 100)});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::v0), 100u);

	EeRecTestHarness ee;
	ee.SetGpr(reg::a0, EeToIop(kIopProgramPc));
	ee.SetGpr(reg::a1, 0x00C8u);    // new imm16
	ee.LoadProgram({
		SH(reg::a1, 0, reg::a0),
	});
	ee.Run();

	ASSERT_EQ(iop.ReadU32(kIopProgramPc), kAddiuV0Zero200);

	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::v0), 200u)
		<< "half-width SMC from EE did not invalidate the IOP JIT block";
}

TEST(EeRecIopHandoff, MultiWordEeReplaceInvalidatesBlock)
{
	// Three-instruction IOP block; EE replaces all three words with
	// new opcodes. The block must re-compile as a whole.
	//
	// Before:  ADDIU v0,zero,10 ;  ADDIU v1,zero,20 ;  ADDU t0,v0,v1  → t0=30
	// After:   ADDIU v0,zero,7  ;  ADDIU v1,zero,35 ;  ADDU t0,v0,v1  → t0=42
	JitTestHarness iop;
	iop.LoadProgram({
		ADDIU(reg::v0, reg::zero, 10),
		ADDIU(reg::v1, reg::zero, 20),
		ADDU (reg::t0, reg::v0, reg::v1),
	});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::t0), 30u);

	// EE program does three SWs, one per word. The second SetGpr for
	// a1 is reassigned between stores.
	constexpr u32 kNewW0 = ADDIU(reg::v0, reg::zero, 7);
	constexpr u32 kNewW1 = ADDIU(reg::v1, reg::zero, 35);
	constexpr u32 kNewW2 = ADDU (reg::t0, reg::v0, reg::v1);   // same shape

	EeRecTestHarness ee;
	ee.SetGpr(reg::a0, EeToIop(kIopProgramPc));
	ee.SetGpr(reg::a1, kNewW0);
	ee.SetGpr(reg::a2, kNewW1);
	ee.SetGpr(reg::a3, kNewW2);
	ee.LoadProgram({
		SW(reg::a1, 0x0, reg::a0),
		SW(reg::a2, 0x4, reg::a0),
		SW(reg::a3, 0x8, reg::a0),
	});
	ee.Run();

	ASSERT_EQ(iop.ReadU32(kIopProgramPc + 0x0), kNewW0);
	ASSERT_EQ(iop.ReadU32(kIopProgramPc + 0x4), kNewW1);
	ASSERT_EQ(iop.ReadU32(kIopProgramPc + 0x8), kNewW2);

	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::t0), 42u)
		<< "multi-word EE replacement did not fully invalidate the IOP block";
	EXPECT_EQ(iop.GetGprJit(reg::v0), 7u);
	EXPECT_EQ(iop.GetGprJit(reg::v1), 35u);
}

TEST(EeRecIopHandoff, EeSwOutsideCodeRegionLeavesBlockCached)
{
	// Invalidation scope: EE writes far from the IOP code region. The
	// cached block must stay intact; a RunResume executes the old
	// opcode without recompilation.
	JitTestHarness iop;
	iop.LoadProgram({ADDIU(reg::v0, reg::zero, 42)});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::v0), 42u);

	// Scratch address deliberately in a far-away 64KB page — the IOP
	// LUT is indexed by `addr >> 16`, so this SW MUST hit a different
	// slot than the program's.
	constexpr u32 kFarIopAddr = RecompilerTestEnvironment::kScratchAddr;  // 0x0002'0000
	static_assert(kFarIopAddr >> 16 != kIopProgramPc >> 16,
		"scratch addr must land in a different LUT page");

	EeRecTestHarness ee;
	ee.SetGpr(reg::a0, EeToIop(kFarIopAddr));
	ee.SetGpr(reg::a1, 0xBAAD'BEEFu);
	ee.LoadProgram({
		SW(reg::a1, 0, reg::a0),
	});
	ee.Run();

	EXPECT_EQ(iop.ReadU32(kFarIopAddr), 0xBAADBEEFu)
		<< "far-from-code EE SW did not actually land";

	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::v0), 42u)
		<< "EE SW to an unrelated address should not have disturbed the "
		   "cached IOP block at kIopProgramPc";
}

TEST(EeRecIopHandoff, CacheIsolationBitBlocksEeWrite)
{
	// Hardware quirk spec-lock: when the IOP's CP0 Status.IsC (bit 16)
	// is set, iopMemWrite32 short-circuits BEFORE writing and BEFORE
	// calling psxCpu->Clear. Even though the store
	// came from the EE side, the IOP's CP0.Status gates the IOP
	// memory backend. This test documents and locks that behavior; if it
	// changes, real BIOS boot may regress.
	JitTestHarness iop;
	iop.LoadProgram({ADDIU(reg::v0, reg::zero, 100)});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::v0), 100u);

	// Raise cache-isolation bit on the IOP — must happen AFTER iop.Run()
	// (which resets psxRegs from pre_snapshot between JIT and interp
	// phases) and BEFORE ee.Run() (whose SW triggers iopMemWrite32).
	psxRegs.CP0.n.Status |= 0x1'0000u;

	EeRecTestHarness ee;
	ee.SetGpr(reg::a0, EeToIop(kIopProgramPc));
	ee.SetGpr(reg::a1, kAddiuV0Zero200);
	ee.LoadProgram({
		SW(reg::a1, 0, reg::a0),
	});
	ee.Run();

	// Store should have been dropped by the isolation gate.
	EXPECT_EQ(iop.ReadU32(kIopProgramPc), kAddiuV0Zero100)
		<< "cache-isolation bit should have blocked the EE SW from landing";

	// Restore default mode before the follow-on RunResume so the IOP's
	// dispatcher doesn't hit the bit in flight.
	psxRegs.CP0.n.Status &= ~0x1'0000u;

	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::v0), 100u)
		<< "IOP block should still return 100 — the EE SW was isolated "
		   "out, so neither the RAM nor the JIT cache changed";
}

TEST(EeRecIopHandoff, SuccessiveEeSwsEachCauseRecompile)
{
	// Two rounds of EE-SMC-then-IOP-run back to back. Each round
	// should see the JIT recompile with the most recent opcode. If
	// the second Clear is swallowed (e.g. early-exit on a no-longer-
	// block-head LUT slot), the third run returns the value from round
	// 2 instead of round 3.
	JitTestHarness iop;
	iop.LoadProgram({ADDIU(reg::v0, reg::zero, 100)});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::v0), 100u);

	// Round 1: EE overwrites with ADDIU v0, zero, 200.
	{
		EeRecTestHarness ee;
		ee.SetGpr(reg::a0, EeToIop(kIopProgramPc));
		ee.SetGpr(reg::a1, kAddiuV0Zero200);
		ee.LoadProgram({SW(reg::a1, 0, reg::a0)});
		ee.Run();
	}
	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	ASSERT_EQ(iop.GetGprJit(reg::v0), 200u) << "round 1: cache not invalidated";

	// Round 2: EE overwrites again with ADDIU v0, zero, 300.
	constexpr u32 kAddiuV0Zero300 = ADDIU(reg::v0, reg::zero, 300);
	{
		EeRecTestHarness ee;
		ee.SetGpr(reg::a0, EeToIop(kIopProgramPc));
		ee.SetGpr(reg::a1, kAddiuV0Zero300);
		ee.LoadProgram({SW(reg::a1, 0, reg::a0)});
		ee.Run();
	}
	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::v0), 300u)
		<< "round 2: second EE SMC failed to invalidate the block re-compiled "
		   "in round 1";
}

// ---------------------------------------------------------------------------
//  Additional handoff-path coverage
// ---------------------------------------------------------------------------

TEST(EeRecIopHandoff, EeSdWritesTwoWordsAndInvalidatesBothBlocks)
{
	// EE 64-bit SD to the IOP bus window. The EE store path splits the 64-bit
	// store into two sequential iopMemWrite32 calls — one per word — each
	// of which independently fires psxCpu->Clear. A single EE instruction
	// should invalidate both words.
	JitTestHarness iop;
	iop.LoadProgram({
		ADDIU(reg::v0, reg::zero, 10),
		ADDIU(reg::v1, reg::zero, 20),
		ADDU (reg::t0, reg::v0, reg::v1),
	});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::t0), 30u);

	// Two new opcodes packed into a single 64-bit value. MIPS is little-
	// endian, so UD[0]'s low 32 bits land at [addr+0] and high 32 at
	// [addr+4]. The target layout: word@+0 = kNewW0 (v0=7), word@+4 = kNewW1 (v1=35).
	constexpr u32 kNewW0 = ADDIU(reg::v0, reg::zero, 7);
	constexpr u32 kNewW1 = ADDIU(reg::v1, reg::zero, 35);
	const u64 packed = static_cast<u64>(kNewW0) | (static_cast<u64>(kNewW1) << 32);

	EeRecTestHarness ee;
	ee.SetGpr(reg::a0, EeToIop(kIopProgramPc));
	ee.SetGpr64(reg::a1, packed);
	ee.LoadProgram({
		ee::SD(reg::a1, 0, reg::a0),
	});
	ee.Run();

	ASSERT_EQ(iop.ReadU32(kIopProgramPc + 0x0), kNewW0)
		<< "low half of SD didn't land at +0";
	ASSERT_EQ(iop.ReadU32(kIopProgramPc + 0x4), kNewW1)
		<< "high half of SD didn't land at +4";

	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::t0), 42u)
		<< "64-bit SD from EE didn't invalidate both half-words of the IOP "
		   "block — one or the other Clear call may have been skipped";
	EXPECT_EQ(iop.GetGprJit(reg::v0), 7u);
	EXPECT_EQ(iop.GetGprJit(reg::v1), 35u);
}

TEST(EeRecIopHandoff, EeSwRewritesToJumpThenRecompiles)
{
	// SMC that installs CONTROL FLOW. Existing tests all rewrite ALU ops —
	// the JIT might cache a block's control-flow shape separately (e.g.
	// end-of-block detection, fall-through prediction). Rewriting to a J
	// proves the re-lifted block observes the new terminator.
	//
	// Pre-stage block 2 in IOP RAM so that once block 1 jumps there, the
	// dispatcher can compile+run it on demand. Block 2 is written but
	// NOT compiled during the initial Run() — only block 1 is.
	constexpr u32 kIopBlock2Pc = 0x0002'0000u;   // far from kIopProgramPc
	static_assert(kIopBlock2Pc >> 16 != kIopProgramPc >> 16,
		"block 2 must land in a different LUT page to avoid co-invalidation");

	JitTestHarness iop;
	iop.LoadProgramAt(kIopBlock2Pc, {
		ADDIU(reg::t0, reg::zero, 42),
	}, /*append_jr_ra_term=*/true);
	iop.LoadProgram({
		ADDIU(reg::v0, reg::zero, 100),
		ADDIU(reg::v1, reg::zero, 200),
	});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::v0), 100u);
	ASSERT_EQ(iop.GetGprJit(reg::v1), 200u);
	ASSERT_EQ(iop.GetGprJit(reg::t0), 0u)
		<< "block 2 must not have run during the initial pass";

	// From the EE, replace words 0 and 1 of block 1 with `J kIopBlock2Pc`
	// and its delay-slot NOP. Word 2 (the old JR ra terminator) and word 3
	// (its NOP delay slot) stay in RAM but are unreachable after the J.
	constexpr u32 kNewJ   = J(kIopBlock2Pc);
	constexpr u32 kNewNop = NOP;

	EeRecTestHarness ee;
	ee.SetGpr(reg::a0, EeToIop(kIopProgramPc));
	ee.SetGpr(reg::a1, kNewJ);
	ee.SetGpr(reg::a2, kNewNop);
	ee.LoadProgram({
		SW(reg::a1, 0x0, reg::a0),
		SW(reg::a2, 0x4, reg::a0),
	});
	ee.Run();

	ASSERT_EQ(iop.ReadU32(kIopProgramPc + 0x0), kNewJ);
	ASSERT_EQ(iop.ReadU32(kIopProgramPc + 0x4), kNewNop);

	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::t0), 42u)
		<< "rewritten block 1 didn't actually transfer control to block 2 — "
		   "either the J wasn't re-lifted or block 2 wasn't entered";
	// v0 and v1 retain the values iop.Run() left them — the rewritten block
	// 1 no longer writes to them.
	EXPECT_EQ(iop.GetGprJit(reg::v0), 100u);
	EXPECT_EQ(iop.GetGprJit(reg::v1), 200u);
}

TEST(EeRecIopHandoff, EeCopyLoopStreamingOpcodesAcrossWindow)
{
	// Models the BIOS-style code-upload loop: EE runs
	//   loop: LW rt,0(src); SW rt,0(dst); ADDIU src,+4; ADDIU dst,+4;
	//         ADDIU cnt,-1; BNE cnt,zero,loop; NOP
	// streaming N IOP opcodes across the bus window in one shot. Each SW
	// must fire its own Clear; the JIT must survive bulk invalidation and
	// re-compile the final block from RAM.
	JitTestHarness iop;
	iop.LoadProgram({
		ADDIU(reg::v0, reg::zero, 10),
		ADDIU(reg::v1, reg::zero, 20),
		ADDU (reg::t0, reg::v0, reg::v1),
		ADDIU(reg::s0, reg::zero, 99),
	});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::t0), 30u);
	ASSERT_EQ(iop.GetGprJit(reg::s0), 99u);

	// Pre-seed a 4-word source buffer in EE RAM far from the EE program.
	constexpr u32 kEeSrcBuffer = 0x0003'0000u;
	constexpr u32 kNewWords[] = {
		ADDIU(reg::v0, reg::zero, 7),
		ADDIU(reg::v1, reg::zero, 35),
		ADDU (reg::t0, reg::v0, reg::v1),
		ADDIU(reg::s0, reg::zero, 123),
	};

	EeRecTestHarness ee;
	for (size_t i = 0; i < std::size(kNewWords); ++i)
		ee.WriteU32(kEeSrcBuffer + static_cast<u32>(i * 4), kNewWords[i]);

	ee.SetGpr(reg::a0, kEeSrcBuffer);                // src
	ee.SetGpr(reg::a1, EeToIop(kIopProgramPc));      // dst
	ee.SetGpr(reg::a2, static_cast<u32>(std::size(kNewWords)));  // counter

	// Layout (word offsets from kProgramPc):
	//   0 : LW    a3, 0(a0)    ; loop:
	//   4 : SW    a3, 0(a1)
	//   8 : ADDIU a0, a0, 4
	//  12 : ADDIU a1, a1, 4
	//  16 : ADDIU a2, a2, -1
	//  20 : BNE   a2, zero, -6 ; → word 0
	//  24 : NOP                ; delay slot
	//  25 : JR ra / NOP        ; auto-appended terminator
	//
	// BNE at +20 targets +0. offset = (0 - (20+4))/4 = -6.
	ee.LoadProgram({
		LW   (reg::a3, 0, reg::a0),
		SW   (reg::a3, 0, reg::a1),
		ADDIU(reg::a0, reg::a0, 4),
		ADDIU(reg::a1, reg::a1, 4),
		ADDIU(reg::a2, reg::a2, -1),
		BNE  (reg::a2, reg::zero, -6),
		NOP,
	});
	ee.Run();

	for (size_t i = 0; i < std::size(kNewWords); ++i)
	{
		ASSERT_EQ(iop.ReadU32(kIopProgramPc + static_cast<u32>(i * 4)), kNewWords[i])
			<< "copy loop word " << i << " didn't land";
	}

	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::v0), 7u);
	EXPECT_EQ(iop.GetGprJit(reg::v1), 35u);
	EXPECT_EQ(iop.GetGprJit(reg::t0), 42u);
	EXPECT_EQ(iop.GetGprJit(reg::s0), 123u)
		<< "bulk EE-copy didn't invalidate the IOP block — one or more of "
		   "the 4 Clear calls was dropped";
}

TEST(EeRecIopHandoff, Sif1StyleDirectRamCopyPlusClear)
{
	// Spec-lock for the SIF1 DMA upload path. Unlike the
	// vtlb-driven tests above, SIF1 bypasses the IOP vtlb entirely and:
	//   1. memcpy's into iopPhysMem (direct pointer into iopMem->Main)
	//   2. manually calls psxCpu->Clear(madr, readSize) once per chunk
	// If `psxCpu->Clear` stops invalidating the LUT correctly (or readSize
	// stops being passed in words), real SIF1 uploads would run stale code.
	// This test mirrors the two-step shape with a synthetic "DMA chunk."
	JitTestHarness iop;
	iop.LoadProgram({
		ADDIU(reg::v0, reg::zero, 100),
		ADDIU(reg::v1, reg::zero, 200),
	});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::v0), 100u);
	ASSERT_EQ(iop.GetGprJit(reg::v1), 200u);

	// 2-word synthetic upload — matches the two-iopMemWrite32 shape but
	// through the SIF1 raw-RAM path instead.
	const u32 new_words[] = {
		ADDIU(reg::v0, reg::zero, 7),
		ADDIU(reg::v1, reg::zero, 35),
	};
	std::memcpy(iopPhysMem(kIopProgramPc), new_words, sizeof(new_words));
	// readSize in Sif1.cpp is in words (passed as arg 2 of Clear); this chunk
	// is 2 words.
	psxCpu->Clear(kIopProgramPc, 2);

	// Sanity: the synthetic copy touched RAM. (iop.ReadU32 goes through
	// iopMemRead32 which also goes through the LUT, but the RAM region is
	// the same.)
	ASSERT_EQ(iop.ReadU32(kIopProgramPc + 0x0), new_words[0]);
	ASSERT_EQ(iop.ReadU32(kIopProgramPc + 0x4), new_words[1]);

	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::v0), 7u);
	EXPECT_EQ(iop.GetGprJit(reg::v1), 35u)
		<< "SIF1-shape (memcpy + explicit Clear) didn't invalidate the "
		   "cached IOP block";
}

namespace {

// Shared body for the byte-offset alignment sub-tests. Loads an ADDIU
// v0,zero,100 at kIopProgramPc, has the EE SB a single byte at `offset`,
// then re-runs the IOP JIT and checks that the cached block was rebuilt
// to reflect the post-write opcode (which must produce `expected_result`
// in `result_reg`). The point is that `psxCpu->Clear(addr & ~3, 1)` in
// iopMemWrite8 must word-round to the same aligned address regardless of
// which byte of the word was hit.
void RunSbOffsetCase(u32 offset, u8 new_byte, u32 expected_word,
                     u32 result_reg, u32 expected_result)
{
	JitTestHarness iop;
	iop.LoadProgram({ADDIU(reg::v0, reg::zero, 100)});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::v0), 100u);

	EeRecTestHarness ee;
	ee.SetGpr(reg::a0, EeToIop(kIopProgramPc));
	ee.SetGpr(reg::a1, new_byte);
	ee.LoadProgram({
		SB(reg::a1, static_cast<s16>(offset), reg::a0),
	});
	ee.Run();

	ASSERT_EQ(iop.ReadU32(kIopProgramPc), expected_word)
		<< "post-SB word read-back doesn't match expected encoding at offset "
		<< offset;

	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(result_reg), expected_result)
		<< "SB at offset " << offset << " didn't invalidate the IOP block";
}

} // namespace

TEST(EeRecIopHandoff, EeSbAtOffset1InvalidatesBlock)
{
	// Offset 1 = imm high byte of ADDIU. 0x24020064 → 0x24020164 gives
	// ADDIU v0, zero, 0x164 = 356.
	RunSbOffsetCase(/*offset=*/1, /*new_byte=*/0x01,
	                /*expected_word=*/0x2402'0164u,
	                reg::v0, /*expected=*/0x164u);
}

TEST(EeRecIopHandoff, EeSbAtOffset2InvalidatesBlock)
{
	// Offset 2 = low byte of {rt[4:0], imm[15:8] not-applicable here since
	// rt is high 5 bits of byte 2}. 0x24020064 → 0x24040064 rebinds rt
	// from v0 (reg 2) to a0 (reg 4): ADDIU a0, zero, 100. After re-run,
	// a0 = 100 and v0 is whatever iop.Run left it (iop.Run sets v0=100
	// and the rewritten block no longer writes v0, so it stays 100).
	// The assertion checks a0 — the register the new opcode actually targets.
	RunSbOffsetCase(/*offset=*/2, /*new_byte=*/0x04,
	                /*expected_word=*/0x2404'0064u,
	                reg::a0, /*expected=*/100u);
}

TEST(EeRecIopHandoff, EeSbAtOffset3InvalidatesBlock)
{
	// Offset 3 = opcode byte. 0x24020064 → 0x3C020064 flips the
	// instruction family from ADDIU to LUI: LUI v0, 0x0064 gives
	// v0 = 0x0064'0000 = 6,553,600. This is the byte furthest from
	// (addr & ~3); any off-by-one in the Clear's word-rounding would
	// miss this slot first.
	RunSbOffsetCase(/*offset=*/3, /*new_byte=*/0x3C,
	                /*expected_word=*/0x3C02'0064u,
	                reg::v0, /*expected=*/0x0064'0000u);
}

TEST(EeRecIopHandoff, EeByteCopyLoopPreservesExecutability)
{
	// BIOS-style byte-copy loop (sans null termination): EE runs
	//   loop: LBU v0,0(src); SB v0,0(dst); ADDIU src,+1; ADDIU cnt,-1;
	//         BNE cnt,zero,loop; ADDIU dst,+1 (delay slot)
	// copying 12 bytes (a 3-word IOP program) across the bus window.
	// Each SB fires iopMemWrite8 → Clear on its word-aligned slot — so
	// 12 individual Clears land, of which 4 hit the 1st word, 4 the 2nd,
	// 4 the 3rd (for a contiguous-byte copy).
	//
	// The assertion is that the freshly-copied 3-instruction program
	// actually executes — i.e., the JIT didn't leave any of the three
	// words pointing at a stale compiled block.
	JitTestHarness iop;
	iop.LoadProgram({
		ADDIU(reg::v0, reg::zero, 1),
		ADDIU(reg::v0, reg::zero, 2),
		ADDIU(reg::v0, reg::zero, 3),
	});
	iop.Run();
	ASSERT_EQ(iop.GetGprJit(reg::v0), 3u)
		<< "pre-compiled block should end with v0=3";

	// Build a replacement 3-instruction program as 12 raw bytes in EE RAM.
	const u32 new_words[] = {
		ADDIU(reg::v0, reg::zero, 777),
		ADDIU(reg::v1, reg::zero, 888),
		ADDU (reg::t0, reg::v0, reg::v1),
	};
	constexpr u32 kEeSrcBuffer = 0x0003'0000u;
	constexpr u32 kCopyBytes = sizeof(new_words);  // 12

	EeRecTestHarness ee;
	for (size_t i = 0; i < std::size(new_words); ++i)
		ee.WriteU32(kEeSrcBuffer + static_cast<u32>(i * 4), new_words[i]);

	ee.SetGpr(reg::a0, kEeSrcBuffer);                // src
	ee.SetGpr(reg::a1, EeToIop(kIopProgramPc));      // dst
	ee.SetGpr(reg::a2, kCopyBytes);                   // byte counter

	// Layout (word offsets from kProgramPc):
	//   0 : LBU   v0, 0(a0)    ; loop:
	//   4 : SB    v0, 0(a1)
	//   8 : ADDIU a0, a0, 1
	//  12 : ADDIU a2, a2, -1
	//  16 : BNE   a2, zero, -5 ; → word 0
	//  20 : ADDIU a1, a1, 1    ; delay slot (runs after each BNE incl. fall-thru)
	//  24 : JR ra / NOP        ; auto-appended terminator
	//
	// BNE at +16 targets +0. offset = (0 - (16+4))/4 = -5.
	ee.LoadProgram({
		LBU  (reg::v0, 0, reg::a0),
		SB   (reg::v0, 0, reg::a1),
		ADDIU(reg::a0, reg::a0, 1),
		ADDIU(reg::a2, reg::a2, -1),
		BNE  (reg::a2, reg::zero, -5),
		ADDIU(reg::a1, reg::a1, 1),
	});
	ee.Run();

	// Every byte of the 3-word program should have landed.
	for (size_t i = 0; i < std::size(new_words); ++i)
	{
		ASSERT_EQ(iop.ReadU32(kIopProgramPc + static_cast<u32>(i * 4)), new_words[i])
			<< "byte-copy loop didn't reassemble word " << i << " correctly";
	}

	iop.SetPc(kIopProgramPc);
	iop.RunResume();
	EXPECT_EQ(iop.GetGprJit(reg::v0), 777u);
	EXPECT_EQ(iop.GetGprJit(reg::v1), 888u);
	EXPECT_EQ(iop.GetGprJit(reg::t0), 1665u)
		<< "per-byte EE writes across a 3-word IOP block didn't fully "
		   "invalidate the JIT cache — one of the 12 Clears was dropped "
		   "or a word fell through to stale code";
}
