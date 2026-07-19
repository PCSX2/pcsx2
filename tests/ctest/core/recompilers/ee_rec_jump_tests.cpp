// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Jump opcodes for the EE: J, JAL, JR, JALR.
// Architectural behaviors exercised here:
//   - J    absolute-in-256MB-region target, delay slot executes
//   - JAL  link register (r31) receives PC+8
//   - JR   register target; ra-style returns
//   - JALR link register is explicit (rd), can equal rs
//   - Delay slot executes before the control transfer lands

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kProgramPc = RecompilerTestEnvironment::kProgramPc;
constexpr u32 kPark      = RecompilerTestEnvironment::kParkingPc;
} // namespace

TEST(EeRecJump, JHitsAbsoluteTargetViaDelaySlot)
{
	// J park. Delay slot sets v0=1. Control transfers to park, ending the run.
	// Without `j park`, the fall-through would set v0=99.
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		J(kPark),
		ADDIU(reg::v0, reg::zero, 1),
		ADDIU(reg::v0, reg::zero, 99),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 1ull);
}

TEST(EeRecJump, JalLinkRegisterReceivesPcPlus8)
{
	// JAL park — link register (ra) is PC+8. Program is at kProgramPc.
	// So after JAL at kProgramPc: ra = kProgramPc + 8.
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		JAL(kPark),
		NOP,         // delay slot
	});
	h.Run();
	h.ExpectGpr64(reg::ra, static_cast<s64>(static_cast<s32>(kProgramPc + 8)));
}

TEST(EeRecJump, JrReturnsToAddressInRegister)
{
	// Manually set up "ra = park", then jr ra;nop. Should land at park.
	// This is the default exit path of every LoadProgram() test, but this
	// test asserts explicitly that JR alone (without any ADDIU side-effects)
	// works as a return.
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		JR(reg::ra),
		NOP,
		ADDIU(reg::v0, reg::zero, 99),   // not reached
	});
	h.Run();
	// v0 is zero-initialized; fall-through path would have set it to 99.
	h.ExpectGpr64(reg::v0, 0ull);
}

TEST(EeRecJump, JalrLinkRegisterExplicitSeparateFromJumpTarget)
{
	// JALR rd=v1, rs=t0. t0 is set to park. rd=v1 receives PC+8.
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, kPark);
	h.LoadProgramNoTerm({
		JALR(reg::v1, reg::t0),
		NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v1, static_cast<s64>(static_cast<s32>(kProgramPc + 8)));
}

TEST(EeRecJump, JalrCanAliasLinkAndTarget)
{
	// JALR rd=rs (same reg). Target is evaluated *before* link is written,
	// per MIPS-III: JALR with rd==rs is allowed, jump lands at old value of
	// the register, link receives PC+8.
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, kPark);
	h.LoadProgramNoTerm({
		JALR(reg::t0, reg::t0),
		NOP,
	});
	h.Run();
	// After the jump: t0 = PC+8 (the link).
	h.ExpectGpr64(reg::t0, static_cast<s64>(static_cast<s32>(kProgramPc + 8)));
}

TEST(EeRecJump, DelaySlotRunsBeforeJumpTargetExecutes)
{
	// Delay-slot ADDIU sets v0=42 *before* the jump lands at park. So the
	// post-state has v0=42, not whatever the fall-through would have produced.
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		J(kPark),
		ADDIU(reg::v0, reg::zero, 42),   // delay slot — runs
		ADDIU(reg::v0, reg::zero, 99),   // skipped
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 42ull);
}

TEST(EeRecJump, JalLinkUpperHalfClearedAndSignExtended)
{
	// recJAL writes UL[0]=pc+4 + UL[1]=0 (mirrors x86: link is plain u32 zero-
	// extended, not sign-extended). For a kProgramPc that fits in 31 bits, this
	// matches s64-sign-extension. The contract under test: UD[0] == pc+8 with
	// UD[1] left at its prior value (set nonzero pre-run to detect any
	// stray full-128 write).
	EeRecTestHarness h;
	h.SetGpr128(reg::ra, 0xDEADBEEFCAFEBABEull, 0x1122334455667788ull);
	h.LoadProgramNoTerm({
		JAL(kPark),
		NOP,
	});
	h.Run();
	h.ExpectGpr128(reg::ra,
		static_cast<u64>(kProgramPc + 8),
		0x1122334455667788ull);
}

TEST(EeRecJump, JrTargetCapturedBeforeDelaySlotClobbersRs)
{
	// Delay slot writes t0=garbage AFTER the JR captured t0=kPark into
	// pcWriteback. Per MIPS, the captured target wins and execution lands at kPark.
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, kPark);
	h.LoadProgramNoTerm({
		JR(reg::t0),
		ADDIU(reg::t0, reg::zero, 0x1234), // delay slot clobbers t0
		ADDIU(reg::v0, reg::zero, 99),     // skipped
	});
	h.Run();
	// t0's final value is the delay-slot write (sign-extended); v0 untouched.
	h.ExpectGpr64(reg::t0, static_cast<s64>(static_cast<s32>(0x1234)));
	h.ExpectGpr64(reg::v0, 0ull);
}

TEST(EeRecJump, JalrWithRdZeroBehavesLikeJr)
{
	// JALR rd=0, rs=t0 — Rd==0 path skips the link write. Acts as plain jr.
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, kPark);
	h.LoadProgramNoTerm({
		JALR(reg::zero, reg::t0),
		NOP,
		ADDIU(reg::v0, reg::zero, 99), // skipped
	});
	h.Run();
	h.ExpectGpr64(reg::zero, 0ull);
	h.ExpectGpr64(reg::v0, 0ull);
}

TEST(EeRecJump, JalrLinkUpperBitsZero)
{
	// JALR writes UD[0] = pc+8 (full 64-bit; recJALR uses x14 and stores to
	// UD[0]). Distinct from JAL's UL[0]+UL[1] pair. Verify via UD[0]/UD[1]
	// observation that the upper 64 bits of rd are not zeroed.
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, kPark);
	h.SetGpr128(reg::v1, 0xDEADBEEF, 0xAA55AA55AA55AA55ull);
	h.LoadProgramNoTerm({
		JALR(reg::v1, reg::t0),
		NOP,
	});
	h.Run();
	h.ExpectGpr128(reg::v1,
		static_cast<u64>(kProgramPc + 8),
		0xAA55AA55AA55AA55ull);
}

// ===========================================================================
//  Delay-slot hoisting (TrySwapDelaySlot) on register jumps — the swap must
//  fire only when the slot can't observe the difference. The JIT-vs-interp
//  diff is the oracle; the swap conditions below are static, so each test
//  deterministically takes the intended path.
// ===========================================================================

TEST(EeRecJump, JrSwappableDelaySlotHoisted)
{
	// Slot writes v0 (unrelated to rs=t0) → TrySwapDelaySlot hoists it ahead
	// of the jump. Target and slot result must both be correct.
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, kPark);
	h.SetGpr64(reg::t1, 0x0000000000000700ull);
	h.LoadProgramNoTerm({
		JR(reg::t0),
		ADDIU(reg::v0, reg::t1, 0x42),  // swappable delay slot
		ADDIU(reg::v0, reg::zero, 99),  // skipped
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0x742ull);
}

TEST(EeRecJump, JalrSwappableDelaySlotHoisted)
{
	// Slot touches neither rs=t0 nor rd=v1 → hoisted. Link, target, and slot
	// result must all be correct.
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, kPark);
	h.SetGpr64(reg::t1, 0x0000000000000100ull);
	h.LoadProgramNoTerm({
		JALR(reg::v1, reg::t0),
		ADDIU(reg::v0, reg::t1, 0x23),  // swappable delay slot
		ADDIU(reg::v0, reg::zero, 99),  // skipped
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0x123ull);
	h.ExpectGpr64(reg::v1, static_cast<u64>(kProgramPc + 8));
}

TEST(EeRecJump, JalrDelaySlotReadingLinkSeesNewValue)
{
	// On hardware the link register is written BEFORE the delay slot runs, so
	// a slot reading rd must see pc+8 — TrySwapDelaySlot's rd argument
	// excludes this slot from hoisting (a hoisted slot would read the stale
	// value). v0 = v1 + 4 = (kProgramPc + 8) + 4.
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, kPark);
	h.SetGpr64(reg::v1, 0x0000000000001111ull); // stale value the slot must NOT see
	h.LoadProgramNoTerm({
		JALR(reg::v1, reg::t0),
		ADDIU(reg::v0, reg::v1, 4),     // reads the link — must not be hoisted
		ADDIU(reg::v0, reg::zero, 99),  // skipped
	});
	h.Run();
	h.ExpectGpr64(reg::v0, static_cast<u64>(kProgramPc + 8 + 4));
}
