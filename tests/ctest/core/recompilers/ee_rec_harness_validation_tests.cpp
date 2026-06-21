// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Harness self-validation. These aren't tests of the EE rec — they're smoke
// tests of the EeRecTestHarness itself: that SetGpr/Run/GetGpr round-trips,
// that each instance starts from clean state, and that the JIT and interp
// paths agree (both ultimately delegate to the interpreter).
//
// Note: EeRecTestHarness's ctor calls ZeroCpuRegs() (a memset of cpuRegs), so
// every test starts from a zeroed cpuRegs regardless of what ran before it.
// The tests are therefore order-independent — there is no cross-test
// contamination to guard against, and no required ordering with other harness
// files.

#include "harness/EeRecTestHarness.h"

#include "R5900.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

TEST(EeRecHarnessValidation, TestOne_SetsRegisterToKnownPoison)
{
	// Set r9 (t1) to a recognizable value and confirm it round-trips: the
	// harness seeds it, Run() executes a no-op program, and the interp
	// snapshot reports the same value back.
	EeRecTestHarness h;
	h.SetGpr64(reg::t1, 0xDEADBEEFDEADBEEFull);
	h.LoadProgram({NOP});          // no-op, just exercises Run()
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t1), 0xDEADBEEFDEADBEEFull);
}

TEST(EeRecHarnessValidation, TestTwo_DoesNotSeePreviousTestResidue)
{
	// New harness, no SetGpr. The ctor's ZeroCpuRegs scrub means r9 (t1)
	// starts at 0 even though the previous test set it to a poison value —
	// confirming each test gets clean cpuRegs — and a simple ADDIU computes
	// correctly. (If the ctor scrub ever regressed, r9 would still hold the
	// poison and the assertion below would fire.)
	EeRecTestHarness h;
	h.LoadProgram({
		ADDIU(reg::t0, reg::zero, 42),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t1), 0ull)
		<< "r9 (t1) leaked from previous test's poison";
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), 42ull);
}

TEST(EeRecHarnessValidation, MultipleRunsInSameTestAreIdempotent)
{
	// Same harness, same program, called twice. Second Run() must produce
	// identical results — proves that Run()'s internal state management
	// doesn't accumulate over repeated invocations.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 5);
	h.SetGpr64(reg::a1, 7);
	h.LoadProgram({ADDU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 12ull);

	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 12ull);
}

TEST(EeRecHarnessValidation, DiffJitVsInterpIsTautologicalUnderDelegatingHarness)
{
	// Sanity: the JIT path emits a real block per guest pc (cached in
	// recBlocks/recLUT), but each block's body is a sequence of
	// `bl <per-insn interp helper>` calls, so both paths ultimately run
	// intCpu.Step(). Diff should be empty on any deterministic program.
	// This catches regressions where real JIT opcode emission is
	// accidentally enabled while the harness still delegates to interp.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 100);
	h.SetGpr64(reg::a1, 200);
	h.LoadProgram({
		ADDU(reg::v0, reg::a0, reg::a1),
		AND (reg::v1, reg::a0, reg::a1),
		ee::DADDU(reg::a2, reg::a0, reg::a1),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), h.GetGpr64Jit(reg::v0));
	EXPECT_EQ(h.GetGpr64Interp(reg::v1), h.GetGpr64Jit(reg::v1));
	EXPECT_EQ(h.GetGpr64Interp(reg::a2), h.GetGpr64Jit(reg::a2));
}
