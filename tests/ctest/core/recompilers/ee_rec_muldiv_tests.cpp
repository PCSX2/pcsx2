// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// MultDiv-family opcodes: MULT, MULTU, DIV, DIVU, MADD, MADDU. Verified under
// DiffJitVsInterp via EeRecTestHarness — every test exercises both the arm64
// JIT emitter and the interpreter and gtest-diffs their final architectural
// state.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

#include <cstdint>

using namespace recompiler_tests;
using namespace mips;

// ---- MULT ---------------------------------------------------------------

TEST(EeRecMulDiv, MultSignExtendsResult)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x12345);
	h.SetGpr64(reg::a1, 0x10);
	h.LoadProgram({MULT(reg::a0, reg::a1)});
	h.Run();
	// 0x12345 * 0x10 = 0x123450 — fits in 32 bits. Assert the FULL 64-bit
	// LO/HI (no mask) so the zero-extension this test is named for is checked.
	EXPECT_EQ(h.GetLo64Interp(), 0x123450ull);
	EXPECT_EQ(h.GetHi64Interp(), 0ull);
}

TEST(EeRecMulDiv, MultNegativeProduct)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, static_cast<s64>(-2));
	h.SetGpr64(reg::a1, 3);
	h.LoadProgram({MULT(reg::a0, reg::a1)});
	h.Run();
	// lo = -6 (as s32 → sign-extended). hi = all-ones (sign of -6).
	EXPECT_EQ(static_cast<s32>(h.GetLo64Interp()), -6);
	EXPECT_EQ(static_cast<s32>(h.GetHi64Interp()), -1);
}

TEST(EeRecMulDiv, MultLargeProductSpillsIntoHi)
{
	// 0x10000 * 0x10000 = 0x100000000 → LO = 0 (low 32), HI = 1.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x10000);
	h.SetGpr64(reg::a1, 0x10000);
	h.LoadProgram({MULT(reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(static_cast<s64>(h.GetLo64Interp()), 0);
	EXPECT_EQ(static_cast<s64>(h.GetHi64Interp()), 1);
}

TEST(EeRecMulDiv, MultIntMinByTwo)
{
	// 0x80000000 * 2 (signed) = 0xFFFFFFFF00000000 as s64.
	// LO = 0, HI = -1 (each sign-extended to 64).
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, static_cast<s64>(static_cast<s32>(0x80000000)));
	h.SetGpr64(reg::a1, 2);
	h.LoadProgram({MULT(reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 0ull);
	EXPECT_EQ(h.GetHi64Interp(), 0xFFFFFFFFFFFFFFFFull);
}

TEST(EeRecMulDiv, MultZeroLeavesHiLoZero)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x1234567);
	h.SetGpr64(reg::a1, 0);
	h.LoadProgram({MULT(reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 0ull);
	EXPECT_EQ(h.GetHi64Interp(), 0ull);
}

TEST(EeRecMulDiv, MultConstFold)
{
	// Pre-LUI/ADDIU both operands so const-prop fires; both sides fold
	// statically and the const-const emitter runs.
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::v0, 0x0000),  // v0 = 0
		ADDIU(reg::v0, reg::v0, 100),
		LUI(reg::v1, 0x0000),
		ADDIU(reg::v1, reg::v1, 200),
		MULT(reg::v0, reg::v1),
	});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 20000ull);
	EXPECT_EQ(h.GetHi64Interp(), 0ull);
}

// ---- MULTU --------------------------------------------------------------

TEST(EeRecMulDiv, MultuLargeUnsigned)
{
	// 0xFFFFFFFF * 0xFFFFFFFF = 0xFFFFFFFE00000001 (u64).
	// LO = 1, HI = 0xFFFFFFFE (sign-extended = 0xFFFFFFFFFFFFFFFE).
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFFFFFFull);
	h.SetGpr64(reg::a1, 0xFFFFFFFFull);
	h.LoadProgram({MULTU(reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 1ull);
	EXPECT_EQ(h.GetHi64Interp(), 0xFFFFFFFFFFFFFFFEull);
}

TEST(EeRecMulDiv, MultuTopBitSetLowBitSet)
{
	// 0x80000000 * 0x80000000 (unsigned) = 0x4000000000000000.
	// LO = 0, HI = 0x40000000.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x80000000ull);
	h.SetGpr64(reg::a1, 0x80000000ull);
	h.LoadProgram({MULTU(reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 0ull);
	EXPECT_EQ(h.GetHi64Interp(), 0x40000000ull);
}

TEST(EeRecMulDiv, MultuConstFold)
{
	EeRecTestHarness h;
	h.LoadProgram({
		ADDIU(reg::v0, reg::zero, -1),  // v0 = -1 (UL[0] = 0xFFFFFFFF), const
		ADDIU(reg::v1, reg::zero, 2),   // v1 = 2, const
		MULTU(reg::v0, reg::v1),
	});
	h.Run();
	// 0xFFFFFFFF * 2 = 0x1FFFFFFFE. LO = 0xFFFFFFFE (sign-ext = -2), HI = 1.
	EXPECT_EQ(static_cast<s64>(h.GetLo64Interp()), -2);
	EXPECT_EQ(h.GetHi64Interp(), 1ull);
}

// ---- DIV ----------------------------------------------------------------

TEST(EeRecMulDiv, DivSimple)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 100);
	h.SetGpr64(reg::a1, 7);
	h.LoadProgram({DIV(reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 14ull);  // 100/7 = 14
	EXPECT_EQ(h.GetHi64Interp(), 2ull);   // 100%7 = 2
}

TEST(EeRecMulDiv, DivNegativeQuotient)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, static_cast<s64>(-100));
	h.SetGpr64(reg::a1, 7);
	h.LoadProgram({DIV(reg::a0, reg::a1)});
	h.Run();
	// C++20 / MIPS truncate toward zero: -100 / 7 = -14, -100 % 7 = -2.
	EXPECT_EQ(static_cast<s64>(h.GetLo64Interp()), -14);
	EXPECT_EQ(static_cast<s64>(h.GetHi64Interp()), -2);
}

TEST(EeRecMulDiv, DivIntMinByMinusOneOverflow)
{
	// MIPS overflow case: 0x80000000 / -1 → LO = 0x80000000, HI = 0.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, static_cast<s64>(static_cast<s32>(0x80000000)));
	h.SetGpr64(reg::a1, static_cast<s64>(-1));
	h.LoadProgram({DIV(reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(static_cast<s64>(h.GetLo64Interp()),
	          static_cast<s64>(static_cast<s32>(0x80000000)));
	EXPECT_EQ(h.GetHi64Interp(), 0ull);
}

TEST(EeRecMulDiv, DivByZeroPositiveDividend)
{
	// MIPS: rs >= 0 / 0 → LO = -1, HI = rs.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x1234);
	h.SetGpr64(reg::a1, 0);
	h.LoadProgram({DIV(reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(static_cast<s64>(h.GetLo64Interp()), -1);
	EXPECT_EQ(h.GetHi64Interp(), 0x1234ull);
}

TEST(EeRecMulDiv, DivByZeroNegativeDividend)
{
	// MIPS: rs < 0 / 0 → LO = +1, HI = rs.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, static_cast<s64>(-0x1234));
	h.SetGpr64(reg::a1, 0);
	h.LoadProgram({DIV(reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 1ull);
	EXPECT_EQ(static_cast<s64>(h.GetHi64Interp()), -0x1234);
}

TEST(EeRecMulDiv, DivConstFold)
{
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::v0, 0x0000),
		ADDIU(reg::v0, reg::v0, 1000),
		LUI(reg::v1, 0x0000),
		ADDIU(reg::v1, reg::v1, 13),
		DIV(reg::v0, reg::v1),
	});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 76ull);    // 1000/13 = 76
	EXPECT_EQ(h.GetHi64Interp(), 12ull);    // 1000%13 = 12
}

TEST(EeRecMulDiv, DivConstFoldOverflow)
{
	// Const fold path of INT_MIN / -1.
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::v0, 0x8000),               // v0 UL[0] = 0x80000000 (INT_MIN)
		ADDIU(reg::v1, reg::zero, -1),      // v1 UL[0] = 0xFFFFFFFF (-1)
		DIV(reg::v0, reg::v1),
	});
	h.Run();
	EXPECT_EQ(static_cast<s64>(h.GetLo64Interp()),
	          static_cast<s64>(static_cast<s32>(0x80000000)));
	EXPECT_EQ(h.GetHi64Interp(), 0ull);
}

TEST(EeRecMulDiv, DivConstFoldByZero)
{
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::v0, 0x0000),
		ADDIU(reg::v0, reg::v0, -42),      // v0 = -42 (negative)
		LUI(reg::v1, 0x0000),              // v1 = 0
		DIV(reg::v0, reg::v1),
	});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 1ull);               // neg divby0 → q=+1
	EXPECT_EQ(static_cast<s64>(h.GetHi64Interp()), -42);
}

// ---- DIVU ---------------------------------------------------------------

TEST(EeRecMulDiv, Divu)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 100);
	h.SetGpr64(reg::a1, 7);
	h.LoadProgram({DIVU(reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 14ull);  // 100/7 = 14
	EXPECT_EQ(h.GetHi64Interp(), 2ull);   // 100%7 = 2
}

TEST(EeRecMulDiv, DivuLargeDividend)
{
	// Dividend with top bit set — would be negative for DIV, positive for DIVU.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFFFFFFull);    // u32 = 4294967295
	h.SetGpr64(reg::a1, 2);
	h.LoadProgram({DIVU(reg::a0, reg::a1)});
	h.Run();
	// 4294967295 / 2 = 2147483647 (0x7FFFFFFF), remainder 1.
	EXPECT_EQ(h.GetLo64Interp(), 0x7FFFFFFFull);
	EXPECT_EQ(h.GetHi64Interp(), 1ull);
}

TEST(EeRecMulDiv, DivuByZero)
{
	// MIPS DIVU / 0 → LO = 0xFFFFFFFF, HI = rs. LO sign-extends to -1 s64.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x1234);
	h.SetGpr64(reg::a1, 0);
	h.LoadProgram({DIVU(reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(static_cast<s64>(h.GetLo64Interp()), -1);
	EXPECT_EQ(h.GetHi64Interp(), 0x1234ull);
}

TEST(EeRecMulDiv, DivuConstFold)
{
	EeRecTestHarness h;
	h.LoadProgram({
		ADDIU(reg::v0, reg::zero, -1),      // UL[0] = 0xFFFFFFFF, const
		ADDIU(reg::v1, reg::zero, 2),       // v1 = 2, const
		DIVU(reg::v0, reg::v1),
	});
	h.Run();
	// 0xFFFFFFFF / 2 = 0x7FFFFFFF, rem 1.
	EXPECT_EQ(h.GetLo64Interp(), 0x7FFFFFFFull);
	EXPECT_EQ(h.GetHi64Interp(), 1ull);
}

TEST(EeRecMulDiv, DivuConstFoldByZero)
{
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::v0, 0x0000),
		ADDIU(reg::v0, reg::v0, 0x7FFF),   // v0 = 0x7FFF
		LUI(reg::v1, 0x0000),              // v1 = 0
		DIVU(reg::v0, reg::v1),
	});
	h.Run();
	EXPECT_EQ(static_cast<s64>(h.GetLo64Interp()), -1);
	EXPECT_EQ(h.GetHi64Interp(), 0x7FFFull);
}

// ---- Pipeline-1 ---------------------------------------------------------
//
// MULT1 — MMI pipeline-1 signed multiply (writes HI1:LO1).

TEST(EeRecMulDiv, Mult1UsesSecondPipeline)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 6);
	h.SetGpr64(reg::a1, 7);
	h.LoadProgram({ee::MULT1(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 42ull);
}

// ---- MADD ---------------------------------------------------------------

TEST(EeRecMulDiv, MaddAccumulatesIntoHiLo)
{
	// HI:LO starts at (0, 10). MADD adds (3 * 5) = 15 → new LO = 25.
	EeRecTestHarness h;
	h.SetLo64(10);
	h.SetHi64(0);
	h.SetGpr64(reg::a0, 3);
	h.SetGpr64(reg::a1, 5);
	h.LoadProgram({ee::MADD(reg::zero, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 25ull);
	EXPECT_EQ(h.GetHi64Interp(), 0ull);
}

TEST(EeRecMulDiv, MaddNegativeProductDecrementsAccum)
{
	// HI:LO = 100. MADD adds (-3 * 7) = -21. New HI:LO = 79.
	EeRecTestHarness h;
	h.SetLo64(100);
	h.SetHi64(0);
	h.SetGpr64(reg::a0, static_cast<s64>(-3));
	h.SetGpr64(reg::a1, 7);
	h.LoadProgram({ee::MADD(reg::zero, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 79ull);
	EXPECT_EQ(h.GetHi64Interp(), 0ull);
}

TEST(EeRecMulDiv, MaddCarryFromLoToHi)
{
	// Force carry across the LO/HI boundary of the 64-bit accumulator.
	// HI:LO starts at (0, 0xFFFFFFFE) == 0xFFFFFFFE.
	// MADD adds (4 * 2) = 8 → new accumulator = 0x100000006.
	// Expected: LO = 6 (sign-ext 0x6), HI = 1.
	EeRecTestHarness h;
	h.SetLo64(static_cast<s64>(static_cast<s32>(0xFFFFFFFE)));  // sign-ext -2
	h.SetHi64(0);
	h.SetGpr64(reg::a0, 4);
	h.SetGpr64(reg::a1, 2);
	h.LoadProgram({ee::MADD(reg::zero, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 6ull);
	EXPECT_EQ(h.GetHi64Interp(), 1ull);
}

TEST(EeRecMulDiv, MaddWritesRdWithLo)
{
	// rd gets the sign-extended LO as well as the HI:LO pair.
	EeRecTestHarness h;
	h.SetLo64(0);
	h.SetHi64(0);
	h.SetGpr64(reg::a0, 6);
	h.SetGpr64(reg::a1, 7);
	h.LoadProgram({ee::MADD(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 42ull);
	EXPECT_EQ(h.GetLo64Interp(), 42ull);
	EXPECT_EQ(h.GetHi64Interp(), 0ull);
}

TEST(EeRecMulDiv, MaddConstFold)
{
	// Const-fold path: both rs/rt flagged const, no actual multiply emitted
	// — handler loads accumulator and adds the precomputed product.
	EeRecTestHarness h;
	h.SetLo64(1000);
	h.SetHi64(0);
	h.LoadProgram({
		LUI(reg::v0, 0x0000),
		ADDIU(reg::v0, reg::v0, 11),
		LUI(reg::v1, 0x0000),
		ADDIU(reg::v1, reg::v1, 13),
		ee::MADD(reg::a0, reg::v0, reg::v1),
	});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 1143ull);   // 1000 + 11*13
	EXPECT_EQ(h.GetHi64Interp(), 0ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::a0), 1143ull);
}

// ---- MADDU --------------------------------------------------------------

TEST(EeRecMulDiv, MadduSimple)
{
	EeRecTestHarness h;
	h.SetLo64(10);
	h.SetHi64(0);
	h.SetGpr64(reg::a0, 3);
	h.SetGpr64(reg::a1, 4);
	h.LoadProgram({ee::MADDU(reg::zero, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 22ull);   // 10 + 3*4
	EXPECT_EQ(h.GetHi64Interp(), 0ull);
}

TEST(EeRecMulDiv, MadduUnsignedOperands)
{
	// rs=rt=0xFFFFFFFF; unsigned product = 0xFFFFFFFE00000001. LO=1, HI=0xFFFFFFFE.
	// HI:LO accumulator starts at 0.
	EeRecTestHarness h;
	h.SetLo64(0);
	h.SetHi64(0);
	h.SetGpr64(reg::a0, 0xFFFFFFFFull);
	h.SetGpr64(reg::a1, 0xFFFFFFFFull);
	h.LoadProgram({ee::MADDU(reg::zero, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 1ull);
	EXPECT_EQ(h.GetHi64Interp(), 0xFFFFFFFFFFFFFFFEull);
}

TEST(EeRecMulDiv, MadduCarryFromLoToHi)
{
	// Same carry test as MADD but unsigned path.
	EeRecTestHarness h;
	h.SetLo64(static_cast<s64>(static_cast<s32>(0xFFFFFFFE)));
	h.SetHi64(0);
	h.SetGpr64(reg::a0, 4);
	h.SetGpr64(reg::a1, 2);
	h.LoadProgram({ee::MADDU(reg::zero, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 6ull);
	EXPECT_EQ(h.GetHi64Interp(), 1ull);
}

TEST(EeRecMulDiv, MadduConstFold)
{
	EeRecTestHarness h;
	h.SetLo64(500);
	h.SetHi64(0);
	h.LoadProgram({
		LUI(reg::v0, 0x0000),
		ADDIU(reg::v0, reg::v0, 20),
		LUI(reg::v1, 0x0000),
		ADDIU(reg::v1, reg::v1, 25),
		ee::MADDU(reg::a0, reg::v0, reg::v1),
	});
	h.Run();
	// 500 + 20*25 = 1000; LO sign-extended into a0.
	EXPECT_EQ(h.GetLo64Interp(), 1000ull);
	EXPECT_EQ(h.GetHi64Interp(), 0ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::a0), 1000ull);
}
