// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// 64-bit (MIPS-III) ALU additions that the EE has but the IOP does not.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

TEST(EeRecAlu64, DadduBasic)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x0000000100000000ull);
	h.SetGpr64(reg::a1, 0x0000000200000005ull);
	h.LoadProgram({ee::DADDU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000000300000005ull);
}

TEST(EeRecAlu64, DadduCarriesThroughBit32)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x00000000FFFFFFFFull);
	h.SetGpr64(reg::a1, 0x0000000000000001ull);
	h.LoadProgram({ee::DADDU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000000100000000ull);
}

TEST(EeRecAlu64, DaddiuNegativeImmediate)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x0000000100000000ull);
	h.LoadProgram({ee::DADDIU(reg::v0, reg::a0, -1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x00000000FFFFFFFFull);
}

TEST(EeRecAlu64, DsubuWraps)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0);
	h.SetGpr64(reg::a1, 1);
	h.LoadProgram({ee::DSUBU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFFFFFFFFFFFFFFull);
}
