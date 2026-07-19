// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Move-family opcodes: LUI, MFHI/MFLO, MTHI/MTLO,
// MOVZ, MOVN. Verified under DiffJitVsInterp via EeRecTestHarness.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

// ---- LUI ----------------------------------------------------------------

TEST(EeRecMove, LuiPositiveImmediate)
{
	EeRecTestHarness h;
	h.LoadProgram({LUI(reg::v0, 0x1234)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'1234'0000ull);
}

TEST(EeRecMove, LuiNegativeImmediateSignExtends)
{
	// Bit 15 of imm set → result has bit 31 set → sign-extends to 64.
	EeRecTestHarness h;
	h.LoadProgram({LUI(reg::v0, 0x8000)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFF'FFFF'8000'0000ull);
}

TEST(EeRecMove, LuiZeroImmediateClearsLow64)
{
	// Even with rt holding garbage, LUI rt, 0 must zero the lower 64.
	EeRecTestHarness h;
	h.SetGpr64(reg::v0, 0xDEAD'BEEF'CAFE'BABEull);
	h.LoadProgram({LUI(reg::v0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0ull);
}

TEST(EeRecMove, LuiThenAddiBuildsConstant)
{
	// Common MIPS idiom: LUI hi; ADDIU low. Tests const-prop chain.
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::v0, 0x1234),
		ADDIU(reg::v0, reg::v0, 0x5678),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0000'0000'1234'5678ull);
}

// ---- MFHI / MFLO --------------------------------------------------------

TEST(EeRecMove, MfhiCopiesHi)
{
	EeRecTestHarness h;
	h.SetHi64(0xABCD'EF01'2345'6789ull);
	h.LoadProgram({MFHI(reg::v0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xABCD'EF01'2345'6789ull);
}

TEST(EeRecMove, MfloCopiesLo)
{
	EeRecTestHarness h;
	h.SetLo64(0x1122'3344'5566'7788ull);
	h.LoadProgram({MFLO(reg::v0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x1122'3344'5566'7788ull);
}

TEST(EeRecMove, MfhiToZeroIsNoOp)
{
	// MFHI to r0 must have no architectural effect — r0 stays zero.
	EeRecTestHarness h;
	h.SetHi64(0xDEAD'BEEFull);
	h.LoadProgram({MFHI(reg::zero)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::zero), 0ull);
}

TEST(EeRecMove, MfhiPreservesIndependentLo)
{
	EeRecTestHarness h;
	h.SetHi64(0x1111'1111'1111'1111ull);
	h.SetLo64(0x2222'2222'2222'2222ull);
	h.LoadProgram({MFHI(reg::v0)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x1111'1111'1111'1111ull);
	EXPECT_EQ(h.GetLo64Interp(),         0x2222'2222'2222'2222ull);
}

// ---- MTHI / MTLO --------------------------------------------------------

TEST(EeRecMove, MthiWritesHi)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xCAFE'BABE'1234'5678ull);
	h.LoadProgram({MTHI(reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetHi64Interp(), 0xCAFE'BABE'1234'5678ull);
}

TEST(EeRecMove, MtloWritesLo)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFEED'FACE'8765'4321ull);
	h.LoadProgram({MTLO(reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp(), 0xFEED'FACE'8765'4321ull);
}

TEST(EeRecMove, MthiFromZeroClearsHi)
{
	EeRecTestHarness h;
	h.SetHi64(0xDEAD'BEEFull);
	h.LoadProgram({MTHI(reg::zero)});
	h.Run();
	EXPECT_EQ(h.GetHi64Interp(), 0ull);
}

TEST(EeRecMove, MthiFromConstPropSource)
{
	// LUI const-folds rs, then MTHI must still propagate the value to HI.
	EeRecTestHarness h;
	h.LoadProgram({
		LUI (reg::a0, 0xABCD),
		MTHI(reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.GetHi64Interp(), 0xFFFF'FFFF'ABCD'0000ull);
}

TEST(EeRecMove, MfhiAfterMthiRoundTrip)
{
	// MTHI then MFHI on the same value — exercises HI memory readback in
	// the same block.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x0123'4567'89AB'CDEFull);
	h.LoadProgram({
		MTHI(reg::a0),
		MFHI(reg::v0),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x0123'4567'89AB'CDEFull);
}

// ---- MOVZ / MOVN --------------------------------------------------------

TEST(EeRecMove, MovzMovesWhenRtZero)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xCAFE'BABEull);          // rs
	h.SetGpr64(reg::a1, 0);                        // rt — triggers move
	h.SetGpr64(reg::v0, 0xDEAD'BEEFull);          // rd pre-state
	h.LoadProgram({ee::MOVZ(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xCAFE'BABEull);
}

TEST(EeRecMove, MovzPreservesRdWhenRtNonzero)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xCAFE'BABEull);
	h.SetGpr64(reg::a1, 1);                        // non-zero — preserve rd
	h.SetGpr64(reg::v0, 0xDEAD'BEEFull);
	h.LoadProgram({ee::MOVZ(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xDEAD'BEEFull);
}

TEST(EeRecMove, MovnMovesWhenRtNonzero)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xCAFE'BABEull);
	h.SetGpr64(reg::a1, 1);                        // non-zero — triggers move
	h.SetGpr64(reg::v0, 0xDEAD'BEEFull);
	h.LoadProgram({ee::MOVN(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xCAFE'BABEull);
}

TEST(EeRecMove, MovnPreservesRdWhenRtZero)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xCAFE'BABEull);
	h.SetGpr64(reg::a1, 0);                        // zero — preserve rd
	h.SetGpr64(reg::v0, 0xDEAD'BEEFull);
	h.LoadProgram({ee::MOVN(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xDEAD'BEEFull);
}

TEST(EeRecMove, MovzMoves64BitFullValue)
{
	// Verify the lower-64 path moves all 64 bits, not just the low 32.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xABCD'EF01'2345'6789ull);
	h.SetGpr64(reg::a1, 0);
	h.LoadProgram({ee::MOVZ(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xABCD'EF01'2345'6789ull);
}

TEST(EeRecMove, MovzWithRsEqualsRdIsNoOp)
{
	// `MOVZ rd, rd, rt` is the trivially-useless form; recMOVZ short-circuits.
	EeRecTestHarness h;
	h.SetGpr64(reg::v0, 0x1111'2222'3333'4444ull);
	h.SetGpr64(reg::a1, 1);                        // would-block, but short-circuit returns first
	h.LoadProgram({ee::MOVZ(reg::v0, reg::v0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0x1111'2222'3333'4444ull);
}

TEST(EeRecMove, MovzConstRtNonzeroStaticallyDeadIsNoOp)
{
	// rt is loaded from a LUI-fold (const-prop), value != 0 → MOVZ never
	// fires. rd must keep its pre-state.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xCAFE'BABEull);
	h.SetGpr64(reg::v0, 0xDEAD'BEEFull);
	h.LoadProgram({
		LUI (reg::a1, 0x0001),                     // rt = 0x0001'0000 (≠ 0)
		ee::MOVZ(reg::v0, reg::a0, reg::a1),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xDEAD'BEEFull);
}

TEST(EeRecMove, MovnConstRtZeroStaticallyDeadIsNoOp)
{
	// rt const-folds to 0, MOVN never fires. rd preserved.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xCAFE'BABEull);
	h.SetGpr64(reg::v0, 0xDEAD'BEEFull);
	h.LoadProgram({
		LUI (reg::a1, 0x0000),                     // rt = 0
		ee::MOVN(reg::v0, reg::a0, reg::a1),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xDEAD'BEEFull);
}

TEST(EeRecMove, MovzConstRtZeroAlwaysFires)
{
	// rt const-folds to 0 → MOVZ unconditionally moves rs to rd.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xCAFE'BABE'1234'5678ull);
	h.SetGpr64(reg::v0, 0xDEAD'BEEFull);
	h.LoadProgram({
		LUI (reg::a1, 0x0000),                     // rt = 0
		ee::MOVZ(reg::v0, reg::a0, reg::a1),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xCAFE'BABE'1234'5678ull);
}

TEST(EeRecMove, MovzConstRsNonzeroRtRuntimeZeroFires)
{
	// rs is const (const-fold via LUI+ORI), rt is runtime value 0 →
	// dispatcher takes the consts-path (rs constant), move fires.
	EeRecTestHarness h;
	h.SetGpr64(reg::v0, 0xDEAD'BEEFull);
	h.SetGpr64(reg::a1, 0);                        // runtime zero
	h.LoadProgram({
		LUI (reg::a0, 0xABCD),
		ORI (reg::a0, reg::a0, 0x1234),            // rs = 0xFFFF'FFFF'ABCD'1234
		ee::MOVZ(reg::v0, reg::a0, reg::a1),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 0xFFFF'FFFF'ABCD'1234ull);
}

// ── Const-flagged PINNED dest read by the condition-false path ───────────────
//
// Regression tests for the 2026-07-17 fuzzer find (seed 194): a MOVZ/MOVN dest
// that is (a) pinned and (b) compile-time const was read for its old value
// AFTER _eeGetGPRDestReg dropped the const flag — but a pinned const dest is
// never materialized into the pin (no allocator fill runs for pins), so the
// condition-false path kept the stale pre-const pin value instead of the
// LUI-produced constant.

TEST(EeRecMove, MovzCondFalseKeepsConstPinnedDest)
{
	// rt != 0 → move does NOT fire; rd must keep the const 0x430000.
	EeRecTestHarness h;
	h.SetGpr64(reg::v0, 0xDEAD'BEEF'0000'0001ull); // stale pre-const pin value
	h.SetGpr64(reg::t9, 0x1111'2222'3333'4444ull);
	h.SetGpr64(reg::s0, 5);                        // condition false
	h.LoadProgram({
		LUI (reg::v0, 0x0043),
		ee::MOVZ(reg::v0, reg::t9, reg::s0),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0x0043'0000ull);
}

TEST(EeRecMove, MovnCondFalseKeepsConstPinnedDest)
{
	// rt == 0 → MOVN does NOT fire; rd must keep the const.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEAD'BEEF'0000'0002ull);
	h.SetGpr64(reg::t9, 0x1111'2222'3333'4444ull);
	h.SetGpr64(reg::s1, 0);                        // condition false
	h.LoadProgram({
		LUI (reg::a0, 0x0055),
		ee::MOVN(reg::a0, reg::t9, reg::s1),
	});
	h.Run();
	h.ExpectGpr64(reg::a0, 0x0055'0000ull);
}

TEST(EeRecMove, MovzConstRsCondFalseKeepsConstPinnedDest)
{
	// consts leaf (rs const) with a const pinned dest and condition false.
	EeRecTestHarness h;
	h.SetGpr64(reg::v1, 0xDEAD'BEEF'0000'0003ull);
	h.SetGpr64(reg::s2, 7);                        // condition false
	h.LoadProgram({
		LUI (reg::t8, 0xABCD),
		LUI (reg::v1, 0x0066),
		ee::MOVZ(reg::v1, reg::t8, reg::s2),
	});
	h.Run();
	h.ExpectGpr64(reg::v1, 0x0066'0000ull);
}
