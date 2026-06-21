// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "harness/JitTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

TEST(IopMulDiv, MultPositive)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 7);
	h.SetGpr(reg::a1, 11);
	h.LoadProgram({
		MULT(reg::a0, reg::a1),
		MFHI(reg::v0),
		MFLO(reg::v1),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0u);  // hi = 0 for small product
	EXPECT_EQ(h.GetGprInterp(reg::v1), 77u); // lo = 77
}

TEST(IopMulDiv, MultLargeGoesToHi)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x10000);
	h.SetGpr(reg::a1, 0x10000);
	h.LoadProgram({
		MULT(reg::a0, reg::a1),
		MFHI(reg::v0),
		MFLO(reg::v1),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x00000001u);
	EXPECT_EQ(h.GetGprInterp(reg::v1), 0x00000000u);
}

TEST(IopMulDiv, MultSigned)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, static_cast<u32>(-2));
	h.SetGpr(reg::a1, 3);
	h.LoadProgram({
		MULT(reg::a0, reg::a1),
		MFHI(reg::v0),
		MFLO(reg::v1),
	});
	h.Run();
	// -2 * 3 = -6: 64-bit signed = 0xFFFF_FFFF_FFFF_FFFA
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xFFFFFFFFu);
	EXPECT_EQ(h.GetGprInterp(reg::v1), 0xFFFFFFFAu);
}

TEST(IopMulDiv, MultuUnsignedMax)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xFFFFFFFFu);
	h.SetGpr(reg::a1, 2);
	h.LoadProgram({
		MULTU(reg::a0, reg::a1),
		MFHI(reg::v0),
		MFLO(reg::v1),
	});
	h.Run();
	// 0xFFFFFFFF * 2 = 0x1_FFFFFFFE
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x00000001u);
	EXPECT_EQ(h.GetGprInterp(reg::v1), 0xFFFFFFFEu);
}

TEST(IopMulDiv, DivSigned)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 23);
	h.SetGpr(reg::a1, 4);
	h.LoadProgram({
		DIV(reg::a0, reg::a1),
		MFHI(reg::v0),  // remainder
		MFLO(reg::v1),  // quotient
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 3u);
	EXPECT_EQ(h.GetGprInterp(reg::v1), 5u);
}

TEST(IopMulDiv, DivuUnsigned)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 100);
	h.SetGpr(reg::a1, 7);
	h.LoadProgram({
		DIVU(reg::a0, reg::a1),
		MFHI(reg::v0),
		MFLO(reg::v1),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 2u);   // 100 % 7 = 2
	EXPECT_EQ(h.GetGprInterp(reg::v1), 14u);  // 100 / 7 = 14
}

// Divide-by-zero canonical results — psxDIV / psxDIVU at
// R3000AOpcodeTables.cpp:63-92 specify:
//   psxDIV  Rt==0: LO = (i32(Rs) < 0) ? 1 : 0xFFFFFFFF;  HI = Rs
//   psxDIVU Rt==0: LO = 0xFFFFFFFF;                       HI = Rs
// Run() auto-diffs JIT vs interp post-state, so reaching an interp value
// that matches the spec also confirms the JIT side.

TEST(IopMulDiv, DivByZeroPositiveRs)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x12345678u);
	h.SetGpr(reg::a1, 0u);
	h.LoadProgram({
		DIV(reg::a0, reg::a1),
		MFHI(reg::v0),
		MFLO(reg::v1),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x12345678u); // HI = Rs
	EXPECT_EQ(h.GetGprInterp(reg::v1), 0xFFFFFFFFu); // LO = -1 (Rs>=0)
}

TEST(IopMulDiv, DivByZeroNegativeRs)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x80000001u); // sign bit set
	h.SetGpr(reg::a1, 0u);
	h.LoadProgram({
		DIV(reg::a0, reg::a1),
		MFHI(reg::v0),
		MFLO(reg::v1),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x80000001u); // HI = Rs
	EXPECT_EQ(h.GetGprInterp(reg::v1), 1u);          // LO = 1 (Rs<0)
}

TEST(IopMulDiv, DivuByZero)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xCAFEBABEu);
	h.SetGpr(reg::a1, 0u);
	h.LoadProgram({
		DIVU(reg::a0, reg::a1),
		MFHI(reg::v0),
		MFLO(reg::v1),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xCAFEBABEu); // HI = Rs
	EXPECT_EQ(h.GetGprInterp(reg::v1), 0xFFFFFFFFu); // LO = 0xFFFFFFFF
}

TEST(IopMulDiv, DivOverflowMinIntByMinusOne)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x80000000u);
	h.SetGpr(reg::a1, 0xFFFFFFFFu);
	h.LoadProgram({
		DIV(reg::a0, reg::a1),
		MFHI(reg::v0),
		MFLO(reg::v1),
	});
	h.Run();
	// psxDIV() at R3000AOpcodeTables.cpp:69 hardcodes LO=INT_MIN, HI=0 for
	// this overflow case; aarch64 SDIV produces the same values natively,
	// so the JIT needs no explicit overflow branch.
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0u);
	EXPECT_EQ(h.GetGprInterp(reg::v1), 0x80000000u);
}

TEST(IopMulDiv, MthiMtloMfhiMflo)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x12345678u);
	h.SetGpr(reg::a1, 0xABCDEF01u);
	h.LoadProgram({
		MTHI(reg::a0),
		MTLO(reg::a1),
		MFHI(reg::v0),
		MFLO(reg::v1),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x12345678u);
	EXPECT_EQ(h.GetGprInterp(reg::v1), 0xABCDEF01u);
}
