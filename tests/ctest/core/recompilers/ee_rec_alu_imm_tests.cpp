// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Immediate-operand EE ALU coverage (ADDI/ADDIU, DADDI/DADDIU, ANDI/ORI/XORI,
// SLTI/SLTIU). Exercises the four dispatch paths of eeRecompileCodeRC1 for
// every AritImm handler — const-fold (GPR_IS_CONST1(rs)) vs runtime emit,
// with zero-immediate fast paths and sign/zero-extension edge cases.
//
// The const-fold path is provoked by preceding the target opcode with an
// immediate sequence (LUI + ORI) that the rec tracks as constant.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

// ---- ADDI / ADDIU (32-bit add + sign-extend) -------------------------------

TEST(EeRecAluImm, AddiPositiveImmediate)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x1000);
	h.LoadProgram({ADDI(reg::v0, reg::a0, 0x100)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x1100ull);
}

TEST(EeRecAluImm, AddiNegativeImmediateSignExtends)
{
	// Non-overflowing ADDI: 0x40000000 + (-1) stays in signed-32 range, so
	// the recADDI emitter runs without raising an Overflow exception. The
	// result is positive (0x3FFFFFFF), so the sxtw leaves the top 32 bits
	// zero.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x0000'0000'4000'0000ull);
	h.LoadProgram({ADDI(reg::v0, reg::a0, -1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'3FFF'FFFFull);
}

TEST(EeRecAluImm, AddiOverflowTrapsAndReturnsToParkingLot)
{
	// 0x80000000 + (-1) overflows the 32-bit signed range, so MIPS-III
	// raises an Overflow exception. With the harness's exception-vector
	// stubs installed (RecompilerTestEnvironment.cpp), the trap returns
	// cleanly via `jr ra` and rt is left at its pre-instruction value
	// (architecturally: trap fires before commit). v0 was zero on entry,
	// so zero is observed on exit — proof the trap was taken rather than
	// silently committing wraparound arithmetic.
	//
	// Interp-only: no EE recompiler backend emits the ADDI overflow trap
	// (titles do not rely on it; the per-op cost is not worth paying), so the
	// JIT commits the wrapped result (0x7fffffff) and this test does not diff
	// against the JIT.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x0000'0000'8000'0000ull);
	h.LoadProgram({ADDI(reg::v0, reg::a0, -1)});
	h.RunInterpOnly();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAluImm, AddiuZeroImmediateIsMoveAndSxtw)
{
	// Zero-imm fast path: hits the `Mov(Wt, Ws); Sxtw(Xt, Wt)` branch.
	// Start with UD[0] high bit set in the low-32 so the sxtw actually
	// propagates a 1.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x1234'5678'8000'0000ull);
	h.LoadProgram({ADDIU(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFF'FFFF'8000'0000ull);
}

TEST(EeRecAluImm, AddiuConstFoldThroughLui)
{
	// LUI+ORI produces a const rs that the rec tracks; the ADDIU below
	// should take the const-fold path (recADDI_const in RC1).
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::a0, 0x1234),
		ORI(reg::a0, reg::a0, 0x5678),
		ADDIU(reg::v0, reg::a0, 0x100),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'1234'5778ull);
}

// ---- DADDI / DADDIU (64-bit add, sign-extended imm) -----------------------

TEST(EeRecAluImm, DaddiLow16PositiveImm)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x1111'2222'3333'4444ull);
	h.LoadProgram({ee::DADDI(reg::v0, reg::a0, 0x100)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x1111'2222'3333'4544ull);
}

TEST(EeRecAluImm, DaddiuSignExtendedImmWrapsLow32)
{
	// -1 sign-extends to 0xFFFF...FFFF; add wraps the whole 64-bit reg.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x1000'0000'0000'0000ull);
	h.LoadProgram({ee::DADDIU(reg::v0, reg::a0, -1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0FFF'FFFF'FFFF'FFFFull);
}

TEST(EeRecAluImm, DaddiZeroImmediateIsCopy)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEAD'BEEF'CAFE'BABEull);
	h.LoadProgram({ee::DADDI(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xDEAD'BEEF'CAFE'BABEull);
}

// ---- ANDI / ORI / XORI (zero-extended 16-bit imm) -------------------------

TEST(EeRecAluImm, AndiMasksLow16AndZerosRest)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEAD'BEEF'CAFE'BABEull);
	h.LoadProgram({ANDI(reg::v0, reg::a0, 0xFF00)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'0000'BA00ull);
}

TEST(EeRecAluImm, AndiZeroImmediateIsMoveZero)
{
	// imm==0 hits the `Mov(Xt, 0)` fast path.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFF'FFFF'FFFF'FFFFull);
	h.LoadProgram({ANDI(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAluImm, AndiNonEncodableImm)
{
	// 0x1234 is not an encodable arm64 logical immediate — vixl MA must
	// synthesize it through a scratch register.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFF'FFFF'FFFF'FFFFull);
	h.LoadProgram({ANDI(reg::v0, reg::a0, 0x1234)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x1234ull);
}

TEST(EeRecAluImm, OriSetsLow16WithoutClobberingHigh)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x1234'5678'9000'0000ull);
	h.LoadProgram({ORI(reg::v0, reg::a0, 0xABCD)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x1234'5678'9000'ABCDull);
}

TEST(EeRecAluImm, OriZeroImmediateIsCopy)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xABCD'1234'5678'0000ull);
	h.LoadProgram({ORI(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xABCD'1234'5678'0000ull);
}

TEST(EeRecAluImm, XoriTogglesLow16)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x0000'0000'FFFF'FFFFull);
	h.LoadProgram({XORI(reg::v0, reg::a0, 0xF0F0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'FFFF'0F0Full);
}

TEST(EeRecAluImm, XoriZeroImmediateIsCopy)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x1111'2222'3333'4444ull);
	h.LoadProgram({XORI(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x1111'2222'3333'4444ull);
}

// ---- SLTI / SLTIU ---------------------------------------------------------

TEST(EeRecAluImm, SltiSignedLessThanZero)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFF'FFFF'FFFF'FFFFull);  // -1 signed
	h.LoadProgram({SLTI(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecAluImm, SltiSignedEqualIsZero)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 42);
	h.LoadProgram({SLTI(reg::v0, reg::a0, 42)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAluImm, SltiNegativeImmediateBoundary)
{
	// Large negative imm: -32768 sign-extended. rs = -32768 should yield
	// 0 (not less than), rs = -32769 (stored as wrap) should yield 1.
	{
		EeRecTestHarness h;
		h.SetGpr64(reg::a0, 0xFFFF'FFFF'FFFF'8000ull);  // -32768
		h.LoadProgram({SLTI(reg::v0, reg::a0, -32768)});
		h.Run();
		EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
	}
	{
		EeRecTestHarness h;
		h.SetGpr64(reg::a0, 0xFFFF'FFFF'FFFF'7FFFull);  // -32769
		h.LoadProgram({SLTI(reg::v0, reg::a0, -32768)});
		h.Run();
		EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
	}
}

TEST(EeRecAluImm, SltiuUnsignedBelowMaxThroughSignExtension)
{
	// imm=-1 sign-extends to 0xFFFF...FFFF; any u64 rs != ~0 is below it.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0);
	h.LoadProgram({SLTIU(reg::v0, reg::a0, -1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecAluImm, SltiuEqualToSignExtendedImmIsZero)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFF'FFFF'FFFF'FFFFull);
	h.LoadProgram({SLTIU(reg::v0, reg::a0, -1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAluImm, SltiuSmallPositiveImmediate)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 5);
	h.LoadProgram({SLTIU(reg::v0, reg::a0, 10)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

// ---- Writes to $zero are no-ops -------------------------------------------

TEST(EeRecAluImm, AddiToZeroIsNoOp)
{
	EeRecTestHarness h;
	h.LoadProgram({ADDI(reg::zero, reg::zero, 1234)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::zero), 0ull);
}

TEST(EeRecAluImm, XoriToZeroIsNoOp)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFFull);
	h.LoadProgram({XORI(reg::zero, reg::a0, 0xFFFF)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::zero), 0ull);
}
