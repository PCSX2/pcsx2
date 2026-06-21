// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Shifts. SLL/SRL/SRA sign-extend the 32-bit result into the 64-bit GPR.
// DSLL / DSRL / DSRA / DSLL32 / DSRL32 / DSRA32 are true 64-bit shifts.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

TEST(EeRecShift, SllSignExtendsHighBit)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 1);
	h.LoadProgram({SLL(reg::v0, reg::a0, 31)});
	h.Run();
	// Result 0x80000000 sign-extends to 0xFFFFFFFF80000000.
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFFFFFF80000000ull);
}

TEST(EeRecShift, SraSignFillsHigh64)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x80000000);
	h.LoadProgram({SRA(reg::v0, reg::a0, 1)});
	h.Run();
	// SRA on 0x80000000 by 1 = 0xC0000000 → sign-extends.
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFFFFFFC0000000ull);
}

TEST(EeRecShift, DsllFull64Bit)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 1);
	h.LoadProgram({ee::DSLL(reg::v0, reg::a0, 31)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000000080000000ull);
}

TEST(EeRecShift, Dsll32)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 1);
	h.LoadProgram({ee::DSLL32(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000000100000000ull);
}

TEST(EeRecShift, Dsll32With31Shifts63Total)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 1);
	h.LoadProgram({ee::DSLL32(reg::v0, reg::a0, 31)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x8000000000000000ull);
}

TEST(EeRecShift, DsrlKeepsZeroFill)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x8000000000000000ull);
	h.LoadProgram({ee::DSRL(reg::v0, reg::a0, 1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x4000000000000000ull);
}

TEST(EeRecShift, DsraSignFill)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x8000000000000000ull);
	h.LoadProgram({ee::DSRA(reg::v0, reg::a0, 4)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xF800000000000000ull);
}

TEST(EeRecShift, Dsra32)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFFFFFF00000000ull);
	h.LoadProgram({ee::DSRA32(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFFFFFFFFFFFFFFull);
}

TEST(EeRecShift, DsllvMasksShiftBy63)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 1);
	h.SetGpr64(reg::a1, 65);                // low 6 bits = 1
	h.LoadProgram({ee::DSLLV(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
}

TEST(EeRecShift, SllvMasksShiftBy31)
{
	// 32-bit variable shifts use only the low 5 bits, not low 6.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 1);
	h.SetGpr64(reg::a1, 33);                // low 5 bits = 1
	h.LoadProgram({SLLV(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull);
}

// ---- Additional coverage extensions ----------------------------------------
// Extensions below exercise the remaining dispatcher paths and edge cases.

// SLL with sa=0 is the canonical MIPS sign-extending move.
TEST(EeRecShift, SllByZeroIsLow32SignExtend)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEAD'BEEF'8000'0001ull);
	h.LoadProgram({SLL(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFF'FFFF'8000'0001ull);
}

TEST(EeRecShift, SrlByImmZeroExtendsLow32)
{
	// 0xFFFF'FFFF >> 4 = 0x0FFF'FFFF; positive 32-bit so high zero.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x1234'5678'FFFF'FFFFull);
	h.LoadProgram({SRL(reg::v0, reg::a0, 4)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'0FFF'FFFFull);
}

TEST(EeRecShift, SrlByZeroIsSignExtendingMove)
{
	// SRL with sa=0 still discards high 32 bits and sign-extends low 32.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEAD'BEEF'8000'0000ull);
	h.LoadProgram({SRL(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFF'FFFF'8000'0000ull);
}

TEST(EeRecShift, DsllByZeroIsCopy)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEAD'BEEF'CAFE'BABEull);
	h.LoadProgram({ee::DSLL(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xDEAD'BEEF'CAFE'BABEull);
}

TEST(EeRecShift, Dsrl32WithExtraSa)
{
	// sa=4 → effective shift = 36.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x1000'0000'0000'0000ull);
	h.LoadProgram({ee::DSRL32(reg::v0, reg::a0, 4)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'0100'0000ull);
}

// SRLV / SRAV — variable counterparts of SRL / SRA.
TEST(EeRecShift, SrlvByRegisterZeroExtends)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x1234'5678'F000'0000ull);
	h.SetGpr64(reg::a1, 0x0000'0000'0000'0004ull);
	h.LoadProgram({SRLV(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'0F00'0000ull);
}

TEST(EeRecShift, SravByRegisterSignExtends)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x0000'0000'F000'0000ull);
	h.SetGpr64(reg::a1, 0x0000'0000'0000'0004ull);
	h.LoadProgram({SRAV(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// 0xF000'0000 >> 4 (arith) = 0xFF00'0000; sign-extend.
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFF'FFFF'FF00'0000ull);
}

TEST(EeRecShift, DsrlvAcrossWordBoundary)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFF'FFFF'0000'0000ull);
	h.SetGpr64(reg::a1, 0x0000'0000'0000'0010ull);
	h.LoadProgram({ee::DSRLV(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'FFFF'FFFF'0000ull);
}

TEST(EeRecShift, DsravArithRightAcrossWordBoundary)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x8000'0000'0000'0000ull);
	h.SetGpr64(reg::a1, 0x0000'0000'0000'0008ull);
	h.LoadProgram({ee::DSRAV(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFF80'0000'0000'0000ull);
}

// Const-fold paths — provoked by preceding LUI/ORI sequences that the rec
// tracks as constant.

TEST(EeRecShift, SllConstFoldThroughLui)
{
	// LUI tracks a0 as const; SLL hits recSLL_const.
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::a0, 0x0001),                  // a0 = 0x0001'0000
		SLL(reg::v0, reg::a0, 4),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'0010'0000ull);
}

TEST(EeRecShift, SllvConstSPath)
{
	// rs (a1) const, rt (a0) runtime → recSLLV_consts.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x0000'0000'0000'00FFull);
	h.LoadProgram({
		ORI(reg::a1, reg::zero, 0x0010),
		SLLV(reg::v0, reg::a0, reg::a1),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'00FF'0000ull);
}

TEST(EeRecShift, SllvConstTPath)
{
	// rt (a0) const, rs (a1) runtime → recSLLV_constt.
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 0x0000'0000'0000'0008ull);
	h.LoadProgram({
		LUI(reg::a0, 0x0001),                  // a0 = 0x0001'0000
		SLLV(reg::v0, reg::a0, reg::a1),
	});
	h.Run();
	// 0x0001'0000 << 8 = 0x0100'0000.
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'0100'0000ull);
}

TEST(EeRecShift, SllvBothConst)
{
	// Both operands const → recSLLV_const compile-time fold.
	EeRecTestHarness h;
	h.LoadProgram({
		ORI(reg::a0, reg::zero, 0x0001),
		ORI(reg::a1, reg::zero, 0x0004),
		SLLV(reg::v0, reg::a0, reg::a1),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'0000'0010ull);
}

TEST(EeRecShift, DsllvConstTPath)
{
	// rt const, rs runtime → recDSLLV_constt.
	EeRecTestHarness h;
	h.SetGpr64(reg::a1, 0x0000'0000'0000'0010ull);
	h.LoadProgram({
		LUI(reg::a0, 0x1234),
		ORI(reg::a0, reg::a0, 0x5678),         // a0 = 0x0000'0000'1234'5678
		ee::DSLLV(reg::v0, reg::a0, reg::a1),
	});
	h.Run();
	// 0x1234'5678 << 16 = 0x1234'5678'0000.
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'1234'5678'0000ull);
}

// Writes to $zero are no-ops.
TEST(EeRecShift, SllToZeroIsNoOp)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFF'FFFFull);
	h.LoadProgram({SLL(reg::zero, reg::a0, 4)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::zero), 0ull);
}

TEST(EeRecShift, DsllvToZeroIsNoOp)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 1);
	h.SetGpr64(reg::a1, 4);
	h.LoadProgram({ee::DSLLV(reg::zero, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::zero), 0ull);
}
