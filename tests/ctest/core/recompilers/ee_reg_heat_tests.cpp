// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// EE register-heat collector (EE-SRA S0) — pins the counting semantics
// through the REAL recompiler: the backprop macros must report every
// 64-bit and 128-bit GPR reference into the block record, and the emitted
// block-entry counter must tick once per execution. Also (implicitly, via
// Run()'s auto-diff) proves the instrumented block still computes
// bit-identical results.

#include "harness/EeRecTestHarness.h"

#include "arm64/EERegHeat.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;
using namespace mips::ee;

namespace {

// Re-derive the env-driven state (disabled, in tests) on scope exit and
// drop this test's records (DumpAndReset while disabled clears silently),
// so later tests see an empty arena.
struct HeatScope
{
	explicit HeatScope(const char* dir) { EERegHeat::OverrideDirForTesting(dir); }
	~HeatScope()
	{
		EERegHeat::OverrideDirForTesting(nullptr);
		EERegHeat::DumpAndReset("test-teardown");
	}
};

} // namespace

TEST(EeRegHeat, CountsStaticRefsAndExecutions)
{
	HeatScope scope("."); // enabled; nothing is dumped inside this test

	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 5);
	h.SetGpr64(reg::t1, 7);
	h.LoadProgram({
		ADDIU(reg::t2, reg::t0, 1),       // r64 t0, w64 t2
		DADDU(reg::t3, reg::t0, reg::t1), // r64 t0+t1, w64 t3
		PADDW(reg::t4, reg::t0, reg::t1), // r128 t0+t1, w128 t4
		MFHI(reg::t5),                    // w64 t5; HI ref must NOT alias $zero/$at
	});
	h.Run();

	EERegHeat::RecordView v{};
	ASSERT_TRUE(EERegHeat::FindRecordForTesting(RecompilerTestEnvironment::kProgramPc, &v));

	// 4 program insns + the harness terminator (JR $ra; NOP).
	EXPECT_EQ(v.insns, 6u);
	// The JIT pass executed the block exactly once. (The interp pass never
	// touches JIT-side counters.)
	EXPECT_EQ(v.exec, 1u);

	EXPECT_EQ(v.r64[reg::t0], 2u); // ADDIU + DADDU sources
	EXPECT_EQ(v.r64[reg::t1], 1u); // DADDU source
	EXPECT_EQ(v.w64[reg::t2], 1u);
	EXPECT_EQ(v.w64[reg::t3], 1u);
	EXPECT_EQ(v.w64[reg::t5], 1u); // MFHI dest
	EXPECT_EQ(v.r64[reg::ra], 1u); // harness JR $ra terminator

	EXPECT_EQ(v.r128[reg::t0], 1u); // PADDW sources
	EXPECT_EQ(v.r128[reg::t1], 1u);
	EXPECT_EQ(v.w128[reg::t4], 1u);
	// 128-bit refs must not leak into the 64-bit tallies (pins can only
	// serve 64-bit accesses).
	EXPECT_EQ(v.r64[reg::t4], 0u);
	EXPECT_EQ(v.w64[reg::t4], 0u);

	// $zero never reaches the collector (macro-guarded), and the MFHI HI
	// ref (XMMGPR_HI = 32) must be excluded, not aliased onto $zero/$at.
	EXPECT_EQ(v.r64[0], 0u);
	EXPECT_EQ(v.w64[0], 0u);
	EXPECT_EQ(v.r64[1], 0u);
	EXPECT_EQ(v.w64[1], 0u);
}

TEST(EeRegHeat, ExecCounterTicksPerEntry)
{
	HeatScope scope(".");

	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 1);
	h.LoadProgram({
		ADDIU(reg::t1, reg::t0, 3),
	});
	h.Run();
	// Second dispatch of the SAME compiled block (no recompile): the entry
	// counter must tick again on the cached-block path.
	h.RunJitNoDiff(EeRecTestHarness::RunMode::PreserveCache);

	EERegHeat::RecordView v{};
	ASSERT_TRUE(EERegHeat::FindRecordForTesting(RecompilerTestEnvironment::kProgramPc, &v));
	EXPECT_EQ(v.exec, 2u);
}

TEST(EeRegHeat, DisabledCollectsNothing)
{
	// No override: env-derived state, which is disabled in the test runner.
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 9);
	h.LoadProgram({
		ADDIU(reg::t1, reg::t0, 1),
	});
	h.Run();

	EERegHeat::RecordView v{};
	EXPECT_FALSE(EERegHeat::FindRecordForTesting(RecompilerTestEnvironment::kProgramPc, &v));
}
