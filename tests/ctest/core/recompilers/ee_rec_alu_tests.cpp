// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// 32-bit EE ALU tests. The EE sign-extends every 32-bit result into the
// low 64 bits of the target GPR, so every test checks the full 64-bit
// representation — this is the whole point of the distinction from IOP.
//
// All tests run through Run() which executes both JIT and interpreter
// paths and diffs architectural state, so a JIT/interp divergence on any
// covered opcode fails the test.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

TEST(EeRecAlu, AddiuSignExtend)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 100);
	h.LoadProgram({ADDIU(reg::v0, reg::a0, -7)});
	h.Run();
	// 93 sign-extended to 64 bits — top half is 0.
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'0000'005Dull);
}

TEST(EeRecAlu, AddiuWrapsIntoNegative)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x7FFFFFFF);
	h.LoadProgram({ADDIU(reg::v0, reg::a0, 1)});
	h.Run();
	// 0x80000000 sign-extends to 0xFFFFFFFF80000000 in the 64-bit GPR.
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFFFFFF80000000ull);
}

TEST(EeRecAlu, AddiuOnlyLow32Matters)
{
	// High 32 bits of rs are ignored by ADDIU (32-bit add + sign-extend).
	EeRecTestHarness h;
	h.SetGpr128(reg::a0, /*lo=*/5, /*hi=*/0xDEADBEEFCAFEBABEull);
	h.LoadProgram({ADDIU(reg::v0, reg::a0, 7)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 12ull);
}

TEST(EeRecAlu, AdduBasic)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 10);
	h.SetGpr64(reg::a1, 20);
	h.LoadProgram({ADDU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 30ull);
}

TEST(EeRecAlu, SubuSignExtendsNegativeResult)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 5);
	h.SetGpr64(reg::a1, 10);
	h.LoadProgram({SUBU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFFFFFFFFFFFFFBull);  // -5
}

TEST(EeRecAlu, AndIsLow64Only)
{
	// The EE interpreter implements AND/OR/XOR/NOR as 64-bit ops that only
	// write `.UD[0]` (low 64 bits) of the destination — `UD[1]` is left
	// alone. Spec-lock this behavior; whether it's what the hardware does
	// is a separate question, but what we care about is interp/JIT agree.
	EeRecTestHarness h;
	h.SetGpr128(reg::a0, 0xAAAA'AAAA'AAAA'AAAAull, 0xFFFF'FFFF'FFFF'FFFFull);
	h.SetGpr128(reg::a1, 0x0F0F'0F0F'0F0F'0F0Full, 0x0F0F'0F0F'0F0F'0F0Full);
	h.SetGpr128(reg::v0, 0ull, 0xDEAD'BEEF'CAFE'BABEull);  // pre-state for UD[1]
	h.LoadProgram({AND(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0),                    0x0A0A'0A0A'0A0A'0A0Aull);
	EXPECT_EQ(h.InterpSnapshot().regs.GPR.r[reg::v0].UD[1], 0xDEAD'BEEF'CAFE'BABEull);
}

TEST(EeRecAlu, OrIsLow64Only)
{
	EeRecTestHarness h;
	h.SetGpr128(reg::a0, 0x00FF'00FF'00FF'00FFull, 0ull);
	h.SetGpr128(reg::a1, 0xFF00'FF00'FF00'FF00ull, 0ull);
	h.LoadProgram({OR(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFF'FFFF'FFFF'FFFFull);
}

TEST(EeRecAlu, XorSelfIsZero)
{
	EeRecTestHarness h;
	h.SetGpr128(reg::a0, 0x1234'5678'9ABC'DEF0ull, 0x1111'2222'3333'4444ull);
	h.LoadProgram({XOR(reg::v0, reg::a0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
	EXPECT_EQ(h.InterpSnapshot().regs.GPR.r[reg::v0].UD[1], 0ull);
}

TEST(EeRecAlu, NorLow64Only)
{
	EeRecTestHarness h;
	h.SetGpr128(reg::a0, 0ull, 0ull);
	h.SetGpr128(reg::a1, 0ull, 0ull);
	h.LoadProgram({NOR(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFF'FFFF'FFFF'FFFFull);
}

TEST(EeRecAlu, LuiShiftsAndSignExtends)
{
	EeRecTestHarness h;
	h.LoadProgram({LUI(reg::v0, 0x8000)});
	h.Run();
	// 0x80000000 sign-extends into the 64-bit result.
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFFFFFF80000000ull);
}

TEST(EeRecAlu, SltSigned64Bit)
{
	// EE SLT is the full-width signed compare (MIPS-III).
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFFFFFFFFFFFFFBull);  // -5
	h.SetGpr64(reg::a1, 3);
	h.LoadProgram({SLT(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecAlu, SltuUnsigned64Bit)
{
	// Upper half of rs all-ones → unsigned max > 3.
	EeRecTestHarness h;
	h.SetGpr128(reg::a0, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull);
	h.SetGpr64(reg::a1, 3);
	h.LoadProgram({SLTU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAlu, AndiZeroExtends)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFFFFFFFFFFFFFFull);
	h.LoadProgram({ANDI(reg::v0, reg::a0, 0x1234)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x1234ull);
}

TEST(EeRecAlu, OriOperatesOn64BitLowSource)
{
	// ORI immediate is zero-extended and OR'd against rs.UD[0]; result
	// lands in rt.UD[0]. rt.UD[1] is unchanged by the interpreter.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFFFFFF00000000ull);
	h.LoadProgram({ORI(reg::v0, reg::a0, 0xABCD)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFFFFFF0000ABCDull);
}

TEST(EeRecAlu, SltiNegativeImmediate)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFFFFFFFFFFFFFFull);  // -1
	h.LoadProgram({SLTI(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecAlu, SltiuSignExtendsImmediate)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0);
	// imm=-1 sign-extends to 0xFFFFFFFFFFFFFFFF; 0 <u max → 1
	h.LoadProgram({SLTIU(reg::v0, reg::a0, -1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecAlu, WritesToZeroAreNoOp)
{
	EeRecTestHarness h;
	h.LoadProgram({ADDIU(reg::zero, reg::zero, 1234)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::zero), 0ull);
}

TEST(EeRecAlu, MovzTakes)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEADBEEF);
	h.SetGpr64(reg::a1, 0);                   // condition == 0 → move
	h.SetGpr64(reg::v0, 0x11112222);
	h.LoadProgram({ee::MOVZ(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xDEADBEEFull);
}

TEST(EeRecAlu, MovzSkips)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEADBEEF);
	h.SetGpr64(reg::a1, 1);                   // condition != 0 → no move
	h.SetGpr64(reg::v0, 0x11112222);
	h.LoadProgram({ee::MOVZ(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x11112222ull);
}

TEST(EeRecAlu, MovnTakes)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEADBEEF);
	h.SetGpr64(reg::a1, 1);                   // condition != 0 → move
	h.SetGpr64(reg::v0, 0x11112222);
	h.LoadProgram({ee::MOVN(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xDEADBEEFull);
}

TEST(EeRecAlu, AddSignedNoOverflow)
{
	// ADD traps on overflow; this case doesn't overflow.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 100);
	h.SetGpr64(reg::a1, 200);
	h.LoadProgram({ADD(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 300ull);
}

// Regression coverage for code paths in the EE recompiler's Arit handlers.

TEST(EeRecAlu, SubSelfIsZero)
{
	// Exercises recSUB_'s early `_Rs_ == _Rt_` shortcut (emits Mov 0 instead
	// of Sub). The interpreter also produces 0 — diff check validates the
	// shortcut matches semantics.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x12345678ull);
	h.LoadProgram({SUB(reg::v0, reg::a0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAlu, XorSelfIsZeroWithNonZeroSource)
{
	// Exercises recLogicalOp's `op == XOR && rs == rt` shortcut.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEADBEEFCAFEBABEull);
	h.LoadProgram({XOR(reg::v0, reg::a0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAlu, WriteToZeroIsNoOp)
{
	// eeRecompileCodeRC0 early-exits when `!rd && (xmminfo & WRITED)`.
	// The harness seeds r0 before Run() — if the shortcut fired cleanly,
	// nothing should happen to r0 (hardwired zero by the EE anyway).
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x12345);
	h.SetGpr64(reg::a1, 0x6789A);
	h.LoadProgram({ADD(reg::zero, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::zero), 0ull);
}

TEST(EeRecAlu, ChainedAdduAcrossInsns)
{
	// Two ADDUs in a row where the second reads the first's destination —
	// exercises the register allocator's Rd→Rs handoff within a block. If the
	// allocator mis-tracks it, the second ADDU sees stale Rs.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 10);
	h.SetGpr64(reg::a1, 20);
	h.SetGpr64(reg::a2, 5);
	h.LoadProgram({
		ADDU(reg::v0, reg::a0, reg::a1),   // v0 = 30
		ADDU(reg::v1, reg::v0, reg::a2),   // v1 = 35
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 30ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::v1), 35ull);
}

TEST(EeRecAlu, NorRegRegFullWidth)
{
	// NOR of two registers whose union is all-ones must yield 0 — verifies the
	// full 64-bit inversion, not a 32-bit-truncated one.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFFFFFF'00000000ull);
	h.SetGpr64(reg::a1, 0x00000000'FFFFFFFFull);
	h.LoadProgram({NOR(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);   // ~(all ones) = 0
}

TEST(EeRecAlu, DaddAdds64Bit)
{
	// DADD is the 64-bit add (no sign-extend truncation).
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x00000000'FFFFFFFFull);
	h.SetGpr64(reg::a1, 0x00000000'00000001ull);
	h.LoadProgram({ee::DADD(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// 0xFFFFFFFF + 1 = 0x1'00000000 — carried into bit 32.
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x00000001'00000000ull);
}

TEST(EeRecAlu, DsubSelfIsZero)
{
	// recDSUB_ shortcut for `_Rs_ == _Rt_`.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xABCDEF0123456789ull);
	h.LoadProgram({ee::DSUB(reg::v0, reg::a0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAlu, SltEqualReturnsZero)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 42);
	h.SetGpr64(reg::a1, 42);
	h.LoadProgram({SLT(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAlu, SltuUnsignedWrap)
{
	// 0xFFFFFFFF'80000000 vs 5: signed, lhs < rhs (-(2^31) < 5). Unsigned,
	// lhs > rhs. SLTU should give 0.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFFFFFF'80000000ull);
	h.SetGpr64(reg::a1, 5);
	h.LoadProgram({SLTU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

// ---------------------------------------------------------------------------
//  Harness plumbing tests — these live in the EeRecHarness suite rather than
//  a per-family suite because they test the harness itself, not any opcode.
// ---------------------------------------------------------------------------

// Confirms Run() captures both JIT and interp post-states cleanly and that
// they agree on the trivial path. If pre_snapshot.Restore() failed between
// JIT and interp (e.g. cpuRegs not reset, memory not rewound), the interp
// result would differ from the JIT result even on this simple program —
// and the divergence would fire ADD_FAILURE inside Run() itself. The
// explicit GetGpr*Jit / GetGpr*Interp equality here catches a subtler
// regression: if the harness ever silently swapped the snapshots
// (copy-paste bug, field confusion), the diff would still be empty but
// the accessor returns would be wrong. Belt and suspenders.
TEST(EeRecHarness, JitAndInterpSnapshotsAgreeOnTrivialProgram)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEAD'BEEFull);
	h.SetGpr64(reg::a1, 0xCAFE'BABEull);
	h.LoadProgram({
		ADDU(reg::v0, reg::a0, reg::a1),
		ADDU(reg::v1, reg::a0, reg::a1),
	});
	h.Run();

	EXPECT_EQ(h.GetGpr64Jit(reg::v0), h.GetGpr64Interp(reg::v0));
	EXPECT_EQ(h.GetGpr64Jit(reg::v1), h.GetGpr64Interp(reg::v1));

	// Both snapshots must have the same program counter and register
	// file — if the harness failed to re-seed before the interp run,
	// interp would start from JIT's post-state and land somewhere else.
	EXPECT_EQ(h.JitSnapshot().regs.pc, h.InterpSnapshot().regs.pc);
}

// Cross-talk check: two tests in sequence, where the second reads cpuRegs
// fields that the first wrote. If the harness doesn't zero cpuRegs in its
// constructor (or the test framework's per-test teardown is incomplete),
// the second test sees leftover state and produces wrong results. The
// ctor's ZeroCpuRegs() is the thing under test here.
TEST(EeRecHarness, CrossTalkZZ_SeedsForContamination)
{
	// Defined first in the file; gtest runs tests in source-definition order,
	// so this runs before the follow-up. Do not reorder/alphabetize these two
	// tests. Intentionally leave a0 with a non-zero value that the next
	// test's a0 default-of-zero depends on.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEAD'BEEF'CAFE'BABEull);
	h.SetGpr64(reg::a1, 0xFFFF'FFFF'FFFF'FFFFull);
	h.LoadProgram({ADDU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// The result here is not the focus — cpuRegs must be non-zero
	// after this test runs so the next test can verify the harness
	// resets state properly.
	EXPECT_EQ(h.GetGpr64Jit(reg::v0), h.GetGpr64Interp(reg::v0));
}

TEST(EeRecHarness, CrossTalkZZ_FollowupSeesCleanState)
{
	// If the prior test's state leaked, a0 would still be nonzero and
	// this test's ADDU reg::a0+reg::a0 would compute 2*DEADBEEFCAFEBABE
	// instead of 0. The harness ctor must zero cpuRegs.
	EeRecTestHarness h;
	// Do NOT call SetGpr64 — rely on ctor's ZeroCpuRegs() for a0.
	h.LoadProgram({ADDU(reg::v0, reg::a0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::a0), 0ull) << "cpuRegs.a0 leaked from prior test";
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
	EXPECT_EQ(h.GetGpr64Jit(reg::v0),    0ull);
}
