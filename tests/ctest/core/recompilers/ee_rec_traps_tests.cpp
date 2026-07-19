// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Trap instructions on the EE: TEQ/TGE/TLT/TNE/TGEU/TLTU family +
// TEQI/TGEI/TLTI/TNEI/TGEIU/TLTIU immediate variants + SYSCALL + BREAK.
//
// Semantics per MIPS-III: when the condition is true, a trap exception
// fires and control transfers to the common exception vector (0x80000180
// with BEV=0). PC is saved in EPC, Cause is updated.
//
// For "trap taken" tests the BEV=0 exception vectors are pre-installed with
// `jr ra; nop` stubs by RecompilerTestEnvironment::SetUp() — the handler
// returns to the harness's parking lot via ra=kParkingPc, giving a
// well-defined post-state without per-test bootstrap.

#include "harness/EeRecTestHarness.h"

#include "Config.h"
#include "R5900.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kCauseTrapExCode = 0x34;  // ExcCode=0x0D (trap), shifted << 2
} // namespace

// ---------------- Register-register trap: not taken ----------------

TEST(EeRecTraps, TeqNotTakenFallsThrough)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 10);
	h.SetGpr64(reg::a1, 20);
	h.LoadProgram({
		ee::TEQ(reg::a0, reg::a1),                // 10 != 20 → not taken
		ADDIU(reg::v0, reg::zero, 7),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

TEST(EeRecTraps, TneNotTakenFallsThrough)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 10);
	h.SetGpr64(reg::a1, 10);
	h.LoadProgram({
		ee::TNE(reg::a0, reg::a1),                // 10 == 10 → not taken
		ADDIU(reg::v0, reg::zero, 7),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

TEST(EeRecTraps, TltNotTakenForGreaterOrEqual)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 20);
	h.SetGpr64(reg::a1, 10);                       // 20 < 10 → false → no trap
	h.LoadProgram({
		ee::TLT(reg::a0, reg::a1),
		ADDIU(reg::v0, reg::zero, 7),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

TEST(EeRecTraps, TgeNotTakenForLess)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 5);
	h.SetGpr64(reg::a1, 10);                       // 5 >= 10 → false → no trap
	h.LoadProgram({
		ee::TGE(reg::a0, reg::a1),
		ADDIU(reg::v0, reg::zero, 7),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

// ---------------- Immediate trap: not taken ----------------

TEST(EeRecTraps, TeqiNotTakenFallsThrough)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 42);
	h.LoadProgram({
		ee::TEQI(reg::a0, 99),                     // 42 != 99 → no trap
		ADDIU(reg::v0, reg::zero, 7),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

TEST(EeRecTraps, TltiNotTakenForGreaterOrEqual)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 100);
	h.LoadProgram({
		ee::TLTI(reg::a0, 50),                     // 100 < 50 → false → no trap
		ADDIU(reg::v0, reg::zero, 7),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

// ---------------- Register-register trap: taken ----------------

TEST(EeRecTraps, TeqTakenRaisesException)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 42);
	h.SetGpr64(reg::a1, 42);
	h.LoadProgram({
		ee::TEQ(reg::a0, reg::a1),                 // 42 == 42 → trap fires
		ADDIU(reg::v0, reg::zero, 99),             // should NOT execute
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);                  // fall-through did not run
	// Cause low byte has ExcCode<<2 = 0x34 (trap).
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, kCauseTrapExCode);
}

// ---------------- Trap in a branch delay slot: CAUSE.BD + EPC (AX-05) ----
//
// A trap raised while executing a branch delay slot must set CAUSE.BD and
// EPC = the BRANCH's address (the kernel re-executes the branch on ERET).
// The interp gets this from _doBranch_shared setting cpuRegs.branch = 1
// around the delay slot; the rec must emit the same bracket around delay-
// slot bodies or every exception helper (trap/syscall/overflow/TLB-miss)
// reads a stale 0 and produces BD=0 with a delay-slot resume address.

TEST(EeRecTraps, TeqTakenInDelaySlotSetsCauseBdAndBranchEpc)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 42);
	h.SetGpr64(reg::a1, 42);
	h.LoadProgram({
		BEQ(reg::zero, reg::zero, 2),    // +0x0: taken branch to +0xC
		ee::TEQ(reg::a0, reg::a1),       // +0x4: delay slot — trap fires
		ADDIU(reg::v0, reg::zero, 99),   // +0x8: skipped by the branch
		ADDIU(reg::v1, reg::zero, 77),   // +0xC: branch target — trap preempts it
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);
	h.ExpectGpr64(reg::v1, 0ull);
	// Interp oracle first.
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, kCauseTrapExCode);
	EXPECT_NE(h.GetCp0Interp(13) & 0x80000000u, 0u) << "interp CAUSE.BD";
	EXPECT_EQ(h.GetCp0Interp(14), RecompilerTestEnvironment::kProgramPc)
		<< "interp EPC = branch address";
	// JIT must match it.
	EXPECT_EQ(h.GetCp0Jit(13) & 0xFFu, kCauseTrapExCode);
	EXPECT_NE(h.GetCp0Jit(13) & 0x80000000u, 0u) << "JIT CAUSE.BD";
	EXPECT_EQ(h.GetCp0Jit(14), RecompilerTestEnvironment::kProgramPc)
		<< "JIT EPC must be the branch, not the delay slot";
}

// Delay-slot coverage for the OTHER bd-raisers (SYSCALL, BREAK, TLB miss on a
// load). Together with the TEQ test above these pin every exception class that
// can fire inside a delay slot on the arm64 rec — the classes for which the
// cpuRegs.branch bracket (and its exception-divert epilogue) must stay emitted
// when the bracket is gated to can-raise delay slots (EE-SRA 2, workstream A).

TEST(EeRecTraps, SyscallInDelaySlotSetsCauseBdAndBranchEpc)
{
	EeRecTestHarness h;
	h.LoadProgram({
		BEQ(reg::zero, reg::zero, 2),    // +0x0: taken branch to +0xC
		SYSCALL_(),                      // +0x4: delay slot — raises
		ADDIU(reg::v0, reg::zero, 99),   // +0x8: skipped by the branch
		ADDIU(reg::v1, reg::zero, 77),   // +0xC: target — exception preempts it
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);
	h.ExpectGpr64(reg::v1, 0ull);
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, 0x20u);
	EXPECT_NE(h.GetCp0Interp(13) & 0x80000000u, 0u) << "interp CAUSE.BD";
	EXPECT_EQ(h.GetCp0Interp(14), RecompilerTestEnvironment::kProgramPc)
		<< "interp EPC = branch address";
	EXPECT_EQ(h.GetCp0Jit(13) & 0xFFu, 0x20u);
	EXPECT_NE(h.GetCp0Jit(13) & 0x80000000u, 0u) << "JIT CAUSE.BD";
	EXPECT_EQ(h.GetCp0Jit(14), RecompilerTestEnvironment::kProgramPc)
		<< "JIT EPC must be the branch, not the delay slot";
}

TEST(EeRecTraps, BreakInDelaySlotSetsCauseBdAndBranchEpc)
{
	EeRecTestHarness h;
	h.LoadProgram({
		BEQ(reg::zero, reg::zero, 2),
		BREAK,                           // delay slot — raises
		ADDIU(reg::v0, reg::zero, 99),
		ADDIU(reg::v1, reg::zero, 77),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);
	h.ExpectGpr64(reg::v1, 0ull);
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, 0x24u);
	EXPECT_NE(h.GetCp0Interp(13) & 0x80000000u, 0u) << "interp CAUSE.BD";
	EXPECT_EQ(h.GetCp0Interp(14), RecompilerTestEnvironment::kProgramPc);
	EXPECT_EQ(h.GetCp0Jit(13) & 0xFFu, 0x24u);
	EXPECT_NE(h.GetCp0Jit(13) & 0x80000000u, 0u) << "JIT CAUSE.BD";
	EXPECT_EQ(h.GetCp0Jit(14), RecompilerTestEnvironment::kProgramPc);
}

TEST(EeRecTraps, LoadTlbMissInDelaySlotSetsCauseBdAndBranchEpc)
{
	// The memory-op raiser: a LW from an unmapped vaddr in a delay slot. The
	// inline load path has no recEmitInterpTlbMissCheck — the divert to the
	// exception vector rides the bracket epilogue, so this test also fails if
	// the epilogue is dropped for memory delay slots.
	// Softmem emit: the test binary installs no host SIGSEGV handler, so a
	// fastmem probe of the unmapped page would kill the process instead of
	// backpatching (same constraint as CallerSavedPinsSurviveVtlbSlowPath).
	// The softmem slow path reaches the identical vtlb_Miss → cpuTlbMiss →
	// cpuException(bd) machinery under test.
	EeRecTestHarness h;
	const bool old_fastmem = EmuConfig.Cpu.Recompiler.EnableFastmem;
	EmuConfig.Cpu.Recompiler.EnableFastmem = false;
	h.LoadProgram({
		LUI(reg::a0, 0x4000),            // +0x0: a0 = 0x40000000 (no TLB entry)
		BEQ(reg::zero, reg::zero, 2),    // +0x4: taken branch to +0x10
		LW(reg::v1, 0, reg::a0),         // +0x8: delay slot — TLB refill miss
		ADDIU(reg::v0, reg::zero, 99),   // +0xC: skipped by the branch
		ADDIU(reg::a1, reg::zero, 77),   // +0x10: target — exception preempts it
	});
	h.Run();
	EmuConfig.Cpu.Recompiler.EnableFastmem = old_fastmem;
	h.ExpectGpr64(reg::v0, 0ull);
	h.ExpectGpr64(reg::v1, 0ull);        // faulting load must not write rt
	h.ExpectGpr64(reg::a1, 0ull);
	// TLBL: ExcCode=2, Cause.ExcCode<<2 = 0x08. BadVAddr (CP0 r8) = the vaddr.
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, 0x08u);
	EXPECT_NE(h.GetCp0Interp(13) & 0x80000000u, 0u) << "interp CAUSE.BD";
	EXPECT_EQ(h.GetCp0Interp(14), RecompilerTestEnvironment::kProgramPc + 4)
		<< "interp EPC = branch address";
	EXPECT_EQ(h.GetCp0Interp(8), 0x40000000u);
	EXPECT_EQ(h.GetCp0Jit(13) & 0xFFu, 0x08u);
	EXPECT_NE(h.GetCp0Jit(13) & 0x80000000u, 0u) << "JIT CAUSE.BD";
	EXPECT_EQ(h.GetCp0Jit(14), RecompilerTestEnvironment::kProgramPc + 4)
		<< "JIT EPC must be the branch, not the delay slot";
	EXPECT_EQ(h.GetCp0Jit(8), 0x40000000u);
}

TEST(EeRecTraps, AluDelaySlotBranchSemanticsSurviveWithoutBracket)
{
	// Non-raising (ALU) delay slots lose the cpuRegs.branch bracket under the
	// workstream-A gate. Pin the plain branch semantics: slot executes, branch
	// is taken, the skipped instruction stays skipped, no exception fires.
	EeRecTestHarness h;
	h.LoadProgram({
		BEQ(reg::zero, reg::zero, 2),    // +0x0: taken branch to +0xC
		ADDIU(reg::a0, reg::zero, 5),    // +0x4: ALU delay slot — executes
		ADDIU(reg::v0, reg::zero, 99),   // +0x8: skipped
		ADDIU(reg::v1, reg::zero, 77),   // +0xC: target — executes
	});
	h.Run();
	h.ExpectGpr64(reg::a0, 5ull);
	h.ExpectGpr64(reg::v0, 0ull);
	h.ExpectGpr64(reg::v1, 77ull);
	EXPECT_EQ(h.GetCp0Jit(13) & 0xFFu, 0u) << "no exception on this path";
}

// ---------------- SYSCALL / BREAK (always taken) ----------------

TEST(EeRecTraps, SyscallAlwaysTaken)
{
	EeRecTestHarness h;
	h.LoadProgram({
		SYSCALL_(),
		ADDIU(reg::v0, reg::zero, 99),             // should NOT execute
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);
	// SYSCALL ExcCode=8, Cause.ExcCode<<2 = 0x20.
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, 0x20u);
}

TEST(EeRecTraps, BreakAlwaysTaken)
{
	EeRecTestHarness h;
	h.LoadProgram({
		BREAK,
		ADDIU(reg::v0, reg::zero, 99),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);
	// BREAK ExcCode=9, Cause.ExcCode<<2 = 0x24.
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, 0x24u);
}

// ---------------- Register-register trap: full taken coverage ----------------

namespace {
template <typename Encode>
void RunRegRegTrapTaken(Encode enc, u64 a0, u64 a1)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, a0);
	h.SetGpr64(reg::a1, a1);
	h.LoadProgram({
		enc(reg::a0, reg::a1),
		ADDIU(reg::v0, reg::zero, 99),  // should NOT execute
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, kCauseTrapExCode);
}

template <typename Encode>
void RunImmTrapTaken(Encode enc, u64 a0, s16 imm)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, a0);
	h.LoadProgram({
		enc(reg::a0, imm),
		ADDIU(reg::v0, reg::zero, 99),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, kCauseTrapExCode);
}
} // namespace

TEST(EeRecTraps, TgeTakenRaisesException)
{
	RunRegRegTrapTaken(ee::TGE, 100, 50);  // 100 >= 50 (signed) → trap
}

TEST(EeRecTraps, TgeuTakenRaisesException)
{
	// TGEU compares unsigned; -1 (= 0xFF..) >= 1 unsigned → trap.
	RunRegRegTrapTaken(ee::TGEU, static_cast<u64>(-1), 1);
}

TEST(EeRecTraps, TltTakenRaisesException)
{
	RunRegRegTrapTaken(ee::TLT, static_cast<u64>(-5), 5);  // -5 < 5 (signed) → trap
}

TEST(EeRecTraps, TltuTakenRaisesException)
{
	RunRegRegTrapTaken(ee::TLTU, 1, static_cast<u64>(-1));  // 1 < ~0 unsigned → trap
}

TEST(EeRecTraps, TneTakenRaisesException)
{
	RunRegRegTrapTaken(ee::TNE, 7, 9);
}

// ---------------- Immediate trap: full taken coverage ----------------

TEST(EeRecTraps, TgeiTakenRaisesException)
{
	RunImmTrapTaken(ee::TGEI, 100, 50);  // 100 >= 50 → trap
}

TEST(EeRecTraps, TgeiuTakenRaisesException)
{
	// _Imm_ is sign-extended to 64-bit before the unsigned compare per MIPS-III,
	// so an imm of -1 produces 0xFFFF_FFFF_FFFF_FFFF; rs=0xFF..F is >= that → trap.
	RunImmTrapTaken(ee::TGEIU, static_cast<u64>(-1), -1);
}

TEST(EeRecTraps, TltiTakenRaisesException)
{
	RunImmTrapTaken(ee::TLTI, static_cast<u64>(-5), 5);  // -5 < 5 → trap
}

TEST(EeRecTraps, TltiuTakenRaisesException)
{
	// 1 < (sign-extended) -1 = 0xFF..F unsigned → trap.
	RunImmTrapTaken(ee::TLTIU, 1, -1);
}

TEST(EeRecTraps, TeqiTakenRaisesException)
{
	RunImmTrapTaken(ee::TEQI, static_cast<u64>(-1), -1);
}

TEST(EeRecTraps, TneiTakenRaisesException)
{
	RunImmTrapTaken(ee::TNEI, 5, 9);
}

// ---------------- MFSA / MTSA / MTSAB / MTSAH ----------------
//
// SA is u32 in cpuRegs.sa; MFSA zero-extends to 64. Test JIT vs interp via
// the snapshot machinery (DiffEe asserts both paths match) — sa is included
// in the EeSnapshot so any divergence fails the test.

TEST(EeRecTraps, MtsaCopiesFullRegister)
{
	// PS2 spec (R5900OpcodeImpl.cpp:1265): cpuRegs.sa = (u32)rs[lo]. No mask.
	// Only MTSAB/MTSAH narrow the value (low 4 / low 3 bits respectively).
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFFFFF7u);
	h.LoadProgram({
		ee::MTSA(reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0xFFFFFFF7u);
}

TEST(EeRecTraps, MtsabXorsLow4BitsWithImmediate)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x5u);
	h.LoadProgram({
		ee::MTSAB(reg::a0, 0x3),  // (5 & 0xF) ^ (3 & 0xF) = 6
	});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0x6u);
}

TEST(EeRecTraps, MtsahShiftsLeftByOne)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x5u);
	h.LoadProgram({
		ee::MTSAH(reg::a0, 0x3),  // ((5 & 7) ^ (3 & 7)) << 1 = 6 << 1 = 0xC
	});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0xCu);
}

TEST(EeRecTraps, MfsaReadsSaZeroExtended)
{
	EeRecTestHarness h;
	h.LoadProgram({
		ee::MTSA(reg::a0),
		ee::MFSA(reg::v0),
	});
	h.SetGpr64(reg::a0, 0xCu);
	h.Run();
	// rd should hold the value of sa, zero-extended to 64-bit.
	h.ExpectGpr64(reg::v0, 0xCull);
}

TEST(EeRecTraps, MfsaToZeroIsNoOp)
{
	// rd=r0 should not modify state; the JIT path early-returns.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x5u);
	h.LoadProgram({
		ee::MTSA(reg::a0),
		ee::MFSA(reg::zero),
	});
	h.Run();
	h.ExpectGpr64(reg::zero, 0ull);
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0x5u);
}

// ---------------- SYNC (no-op) ----------------

TEST(EeRecTraps, SyncIsNoOp)
{
	EeRecTestHarness h;
	h.LoadProgram({
		ADDIU(reg::v0, reg::zero, 7),
		ee::SYNC,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

// ---------------- Const-prop fold paths for MTSA family ----------------

TEST(EeRecTraps, MtsaConstFoldsAtCompile)
{
	// Exercise the GPR_IS_CONST1 fast path: rs is set via a LUI+ORI sequence
	// that the const-prop tracker captures. Same end-state as the runtime
	// path (full 32-bit copy, no mask); a divergence here would point at a
	// const-fold bug.
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::a0, 0),
		ORI(reg::a0, reg::a0, 0xF7u),    // a0 = 0xF7 (const-tracked)
		ee::MTSA(reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0xF7u);
}

TEST(EeRecTraps, MtsabConstFoldsAtCompile)
{
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::a0, 0),
		ORI(reg::a0, reg::a0, 0xAu),
		ee::MTSAB(reg::a0, 0x5),  // (0xA & 0xF) ^ (0x5 & 0xF) = 0xF
	});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0xFu);
}

TEST(EeRecTraps, MtsahConstFoldsAtCompile)
{
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::a0, 0),
		ORI(reg::a0, reg::a0, 0x6u),
		ee::MTSAH(reg::a0, 0x3),  // ((6 & 7) ^ (3 & 7)) << 1 = 5 << 1 = 0xA
	});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0xAu);
}
