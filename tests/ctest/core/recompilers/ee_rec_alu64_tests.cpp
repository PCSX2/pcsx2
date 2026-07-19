// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
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

// ---- GE-05: const-operand folding paths (consts/constt) --------------------
// A const rs drives the *_consts emitters (reversed compare + flipped Cset for
// SLT/SLTU), a const rt the *_constt ones (direct immediate). The equality
// boundaries are the flip-sensitive cases; LUI-built constants exercise the
// non-add-imm-encodable materialization path.

TEST(EeRecAlu64, SltConstsEqualIsZero)
{
	// (const 0x12340000 < rt) with rt EQUAL — a botched reversed-compare
	// condition (ge instead of gt) would return 1 here.
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 0x0000'0000'1234'0000ull);
	h.LoadProgram({LUI(reg::a0, 0x1234), SLT(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAlu64, SltConstsStrictlyLessIsOne)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 0x0000'0000'1234'0001ull);
	h.LoadProgram({LUI(reg::a0, 0x1234), SLT(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecAlu64, SltConstsNegativeRtIsZero)
{
	// Signed compare: 0x12340000 < -5 is false.
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 0xFFFF'FFFF'FFFF'FFFBull);
	h.LoadProgram({LUI(reg::a0, 0x1234), SLT(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAlu64, SltConsttEqualIsZero)
{
	// constt with a NON-add-imm-encodable constant (0x12340000).
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x0000'0000'1234'0000ull);
	h.LoadProgram({LUI(reg::a1, 0x1234), SLT(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAlu64, SltConsttEncodableImmSigned)
{
	// constt with an encodable imm (0x10) and negative rs: -1 < 0x10 -> 1.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFF'FFFF'FFFF'FFFFull);
	h.LoadProgram({ORI(reg::a1, reg::zero, 0x10), SLT(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecAlu64, SltuConstsSignExtendedHugeConst)
{
	// LUI 0x8000 const-props to 0xFFFFFFFF80000000 — unsigned HUGE, so
	// (const <u small rt) must be 0.
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 1);
	h.LoadProgram({LUI(reg::a0, 0x8000), SLTU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAlu64, SltuConstsStrictlyAboveIsOne)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 0xFFFF'FFFF'8000'0001ull);
	h.LoadProgram({LUI(reg::a0, 0x8000), SLTU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecAlu64, SltuConstsEqualIsZero)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 0xFFFF'FFFF'8000'0000ull);
	h.LoadProgram({LUI(reg::a0, 0x8000), SLTU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAlu64, SltuConsttEqualIsZero)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x100);
	h.LoadProgram({ORI(reg::a1, reg::zero, 0x100), SLTU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecAlu64, SltuConsttBelowIsOne)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFF);
	h.LoadProgram({ORI(reg::a1, reg::zero, 0x100), SLTU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull);
}

TEST(EeRecAlu64, XorConstsNonEncodableImm)
{
	// 0x12340000 is not a valid AArch64 bitmask immediate — exercises the
	// materialization fallback inside the vixl macro.
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 0xF0F0'F0F0'FFFF'0000ull);
	h.LoadProgram({LUI(reg::a0, 0x1234), XOR(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xF0F0'F0F0'FFFF'0000ull ^ 0x0000'0000'1234'0000ull);
}

TEST(EeRecAlu64, XorConstsZeroIsCopy)
{
	// rs = $zero -> consts path with cval 0: must be a pure copy of rt,
	// including high bits.
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 0xDEAD'BEEF'CAFE'BABEull);
	h.LoadProgram({XOR(reg::v0, reg::zero, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xDEAD'BEEF'CAFE'BABEull);
}

TEST(EeRecAlu64, XorConsttZeroIsCopy)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEAD'BEEF'CAFE'BABEull);
	h.LoadProgram({XOR(reg::v0, reg::a0, reg::zero)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xDEAD'BEEF'CAFE'BABEull);
}

TEST(EeRecAlu64, DaddConstsEncodableImm)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 5);
	h.LoadProgram({ORI(reg::a0, reg::zero, 0x100), ee::DADD(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x105ull);
}

TEST(EeRecAlu64, DaddConstsNonEncodableImm)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 1);
	h.LoadProgram({LUI(reg::a0, 0x1234), ee::DADD(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'1234'0001ull);
}

TEST(EeRecAlu64, DsubConstsEncodableImmNegatedForm)
{
	// cval - rt via Sub(rt, imm) + Neg: 0x100 - 0x105 = -5.
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 0x105);
	h.LoadProgram({ORI(reg::a0, reg::zero, 0x100), ee::DSUB(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFF'FFFF'FFFF'FFFBull);
}

TEST(EeRecAlu64, DsubConstsNonEncodableImm)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 1);
	h.LoadProgram({LUI(reg::a0, 0x1234), ee::DSUB(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'1233'FFFFull);
}

TEST(EeRecAlu64, DsubConstsZeroIsNeg)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 5);
	h.LoadProgram({ee::DSUB(reg::v0, reg::zero, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFF'FFFF'FFFF'FFFBull);
}
