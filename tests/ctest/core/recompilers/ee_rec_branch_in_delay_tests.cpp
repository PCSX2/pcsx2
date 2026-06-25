// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Branch/jump in a delay slot — the EE recompiler squashes the inner branch
// (DT-05). Verbatim port of x86's check_branch_delay (iR5900.cpp:1742-1803,
// "new code by FlatOut"): when a branch's delay slot is ITSELF a branch, the
// inner branch is treated as a nop — the recompiler emits NOTHING for it and
// returns before cycle counting, matching x86 exactly.
//
// This is an undefined-behavior corner (ps2autotests: HW "returns 2", and both
// the JIT and the interpreter are imperfect). The arm64 EE rec previously
// INTERPRETED the inner branch (recCall(opcode.interpret)), a third behavior
// that diverged from x86. Per the project's "match x86" rule we now squash it.
//
// IMPORTANT: this deliberately diverges from the EE *interpreter*, which fully
// executes the inner branch (_doBranch_shared -> execI), so it CANNOT be
// validated by the usual jit-vs-interp auto-diff. It is JIT-only: assert the
// JIT's squash behavior directly via RunJitNoDiff().
//
// Witness: an inner JAL writes $ra (= its own PC + 8) iff it executes. The
// harness seeds $ra = kParkingPc; the squash leaves it untouched, so $ra stays
// kParkingPc. The old interpret path (and any regression) writes JAL_pc+8
// instead.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kProgramPc = RecompilerTestEnvironment::kProgramPc;
constexpr u32 kPark      = RecompilerTestEnvironment::kParkingPc;
} // namespace

// Outer branch = JR $t0 (-> parking lot); its delay slot is an inner JAL.
// The inner JAL must be squashed (no $ra write), so $ra remains the seeded
// parking-lot value and control leaves via the outer JR only.
TEST(EeRecBranchInDelay, InnerJalInDelaySlotIsSquashed)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, static_cast<s64>(static_cast<s32>(kPark))); // JR target

	h.LoadProgramNoTerm({
		JR(reg::t0),     // outer branch — ends the block, target = parking lot
		JAL(kPark),      // delay slot: inner branch → squashed (emits nothing)
	});
	h.RunJitNoDiff();

	// $ra untouched by the squashed JAL (had it executed, $ra would be
	// JAL_pc + 8 = kProgramPc + 12, not the seeded parking-lot value).
	EXPECT_EQ(h.GetGpr64Jit(reg::ra), static_cast<u64>(static_cast<s64>(static_cast<s32>(kPark))));
	EXPECT_NE(h.GetGpr64Jit(reg::ra), static_cast<u64>(kProgramPc + 12));
}
