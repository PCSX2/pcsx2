// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// MMI paired-word SIMD coverage. Complements the existing ee_rec_mmi_tests.cpp
// (MADD + PLZCW) with representative ops from every MMI SIMD sub-family.
//
// Full 128-bit paired-word coverage exercising SetGpr128/ExpectGpr128. Not
// every MMI0/1/2/3 sub-op is exhausted; this file proves the harness +
// encoders work for each family and provides a regression base for further
// MMI ports.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

// ---------------- MMI0: parallel arithmetic ----------------

TEST(EeRecMmiSimd, PaddwPacked32BitLanes)
{
	// Two 32-bit lanes per 64-bit half of the 128-bit reg; four lanes total.
	// a0 =  {1, 2, 3, 4}, a1 = {10, 20, 30, 40}, expected = {11, 22, 33, 44}.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0000000200000001ull, 0x0000000400000003ull);
	h.SetMmiPair(reg::a1, 0x000000140000000Aull, 0x000000280000001Eull);
	h.LoadProgram({ee::PADDW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x000000160000000Bull, 0x0000002C00000021ull);
}

TEST(EeRecMmiSimd, PaddhPacked16BitLanes)
{
	// Eight 16-bit lanes. a0 = 8×0x0001, a1 = 8×0x0002 → expect 8×0x0003.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0001000100010001ull, 0x0001000100010001ull);
	h.SetMmiPair(reg::a1, 0x0002000200020002ull, 0x0002000200020002ull);
	h.LoadProgram({ee::PADDH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x0003000300030003ull, 0x0003000300030003ull);
}

TEST(EeRecMmiSimd, PaddbPacked8BitLanesWrapOnOverflow)
{
	// 16 × 8-bit lanes. 0xFF + 0x01 = 0x00 (wrap on u8 overflow, PADDB is
	// non-saturating).
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull);
	h.SetMmiPair(reg::a1, 0x0101010101010101ull, 0x0101010101010101ull);
	h.LoadProgram({ee::PADDB(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0ull, 0ull);
}

TEST(EeRecMmiSimd, PsubwPacked32)
{
	// a0 = {50, 100, 150, 300}, a1 = {5, 10, 20, 30}, expect {45, 90, 130, 270}.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0000006400000032ull, 0x0000012C00000096ull);
	h.SetMmiPair(reg::a1, 0x0000000A00000005ull, 0x0000001E00000014ull);
	h.LoadProgram({ee::PSUBW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x0000005A0000002Dull, 0x0000010E00000082ull);
}

// ---------------- MMI0: parallel compare-greater-than (signed) ----------------

TEST(EeRecMmiSimd, PcgtwSignedLanewise)
{
	// Lane result: 0xFFFFFFFF when rs > rt (signed), 0 otherwise. Test both.
	// a0 = {5, -1}, a1 = {3, 0} → expect {all-ones, 0}.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xFFFFFFFF00000005ull, 0);
	h.SetMmiPair(reg::a1, 0x0000000000000003ull, 0);
	h.LoadProgram({ee::PCGTW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x00000000FFFFFFFFull, 0);
}

// ---------------- MMI1: parallel compare-equal ----------------

TEST(EeRecMmiSimd, PceqwLaneEqualSetsAllOnes)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0000001200000042ull, 0);
	h.SetMmiPair(reg::a1, 0x0000001200000043ull, 0);
	h.LoadProgram({ee::PCEQW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// UD[0]: lane 0 (0x42 != 0x43) → 0, lane 1 (0x12 == 0x12) → 0xFFFFFFFF.
	// UD[1]: both zero-initialized, both lanes equal → all-ones.
	h.ExpectMmiPair(reg::v0, 0xFFFFFFFF00000000ull, 0xFFFFFFFFFFFFFFFFull);
}

TEST(EeRecMmiSimd, PceqbByteLanewise)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x1122334455667788ull, 0);
	h.SetMmiPair(reg::a1, 0x1022334400667700ull, 0);
	h.LoadProgram({ee::PCEQB(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// Byte indices are LE within UD[0]: UC[0]=LSB, UC[7]=MSB.
	//   a0.UC[0..7] = 88 77 66 55 44 33 22 11
	//   a1.UC[0..7] = 00 77 66 00 44 33 22 10
	//   equal?        N  Y  Y  N  Y  Y  Y  N
	//   rd.UC[0..7] = 00 FF FF 00 FF FF FF 00
	// Reassembled as UD[0] (MSB first when written as hex):
	//   byte7=00 byte6=FF byte5=FF byte4=FF byte3=00 byte2=FF byte1=FF byte0=00
	h.ExpectMmiPair(reg::v0, 0x00FFFFFF00FFFF00ull, 0xFFFFFFFFFFFFFFFFull);
}

// ---------------- MMI2: logical AND, XOR ----------------

TEST(EeRecMmiSimd, PandBitwise128)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xF0F0F0F0F0F0F0F0ull, 0xAAAAAAAAAAAAAAAAull);
	h.SetMmiPair(reg::a1, 0xFFFF0000FFFF0000ull, 0x5555555555555555ull);
	h.LoadProgram({ee::PAND(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xF0F00000F0F00000ull, 0ull);
}

TEST(EeRecMmiSimd, PxorBitwise128)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xDEADBEEFDEADBEEFull, 0xCAFEBABECAFEBABEull);
	h.SetMmiPair(reg::a1, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull);
	h.LoadProgram({ee::PXOR(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, ~0xDEADBEEFDEADBEEFull, ~0xCAFEBABECAFEBABEull);
}

// ---------------- MMI3: logical OR, NOR ----------------

TEST(EeRecMmiSimd, PorBitwise128)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xF0F0000000000000ull, 0x0000000000000001ull);
	h.SetMmiPair(reg::a1, 0x00000F0F00000000ull, 0x8000000000000000ull);
	h.LoadProgram({ee::POR(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xF0F00F0F00000000ull, 0x8000000000000001ull);
}

// `por rd, r0, rt` — the canonical PS2 128-bit register-move idiom. Exercises
// recPOR's s_zero special-case (register copy, no allocated r0 / Movi+Orr).
TEST(EeRecMmiSimd, PorR0SourceIsRegisterMove)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a1, 0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);
	h.LoadProgram({ee::POR(reg::v0, reg::zero, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);
}

// `por rd, rs, r0` — t_zero special-case (register copy from rs).
TEST(EeRecMmiSimd, PorR0TargetIsRegisterMove)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xAABBCCDDEEFF0011ull, 0x2233445566778899ull);
	h.LoadProgram({ee::POR(reg::v0, reg::a0, reg::zero)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xAABBCCDDEEFF0011ull, 0x2233445566778899ull);
}

// `por rd, r0, r0` — both r0, materializes a 128-bit zero.
TEST(EeRecMmiSimd, PorBothR0IsZero)
{
	EeRecTestHarness h;
	h.LoadProgram({ee::POR(reg::v0, reg::zero, reg::zero)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0ull, 0ull);
}

TEST(EeRecMmiSimd, PnorIsBitwiseComplementOfOr)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x00FF00FF00FF00FFull, 0);
	h.SetMmiPair(reg::a1, 0xFF00FF00FF00FF00ull, 0);
	h.LoadProgram({ee::PNOR(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// a0|a1 = 0xFFFFFFFFFFFFFFFF → NOR = 0.
	h.ExpectMmiPair(reg::v0, 0ull, ~0ull);
}

// ---------------- MMI3: copy-halves and paired copy ----------------

TEST(EeRecMmiSimd, PcpyhReplicatesLowHalfwordsAcrossAllLanes)
{
	// PCPYH: each 16-bit halfword of rd takes the low halfword of the
	// corresponding 64-bit source half. So rt.UD[0] low 16 bits → rd.UD[0]
	// all four halves; rt.UD[1] low 16 bits → rd.UD[1] all four halves.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x1122334455667788ull, 0xAABBCCDDEEFF0011ull);
	h.LoadProgram({ee::PCPYH(reg::v0, reg::a0)});
	h.Run();
	// Low halfword of a0.UD[0] = 0x7788; replicated across 4 halves = 0x7788778877887788
	// Low halfword of a0.UD[1] = 0x0011; replicated = 0x0011001100110011
	h.ExpectMmiPair(reg::v0, 0x7788778877887788ull, 0x0011001100110011ull);
}

TEST(EeRecMmiSimd, PcpyhAliasedRdEqualsRt)
{
	// Register-resident rewrite must stay correct when the allocator
	// hands back qd == qt — the qd write happens after rt.H[4] is broadcast to
	// scratch, so the source half is not clobbered mid-shuffle.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x1122334455667788ull, 0xAABBCCDDEEFF0011ull);
	h.LoadProgram({ee::PCPYH(reg::a0, reg::a0)});
	h.Run();
	h.ExpectMmiPair(reg::a0, 0x7788778877887788ull, 0x0011001100110011ull);
}

TEST(EeRecMmiSimd, PcpyldAssemblesLowHalvesFromRsAndRt)
{
	// PCPYLD: rd.UD[0] = rt.UD[0], rd.UD[1] = rs.UD[0].
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xAAAAAAAAAAAAAAAAull, 0xBBBBBBBBBBBBBBBBull);
	h.SetMmiPair(reg::a1, 0xCCCCCCCCCCCCCCCCull, 0xDDDDDDDDDDDDDDDDull);
	h.LoadProgram({ee::PCPYLD(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xCCCCCCCCCCCCCCCCull, 0xAAAAAAAAAAAAAAAAull);
}

TEST(EeRecMmiSimd, PcpyudAssemblesHighHalvesFromRsAndRt)
{
	// PCPYUD: rd.UD[0] = rs.UD[1], rd.UD[1] = rt.UD[1].
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xAAAAAAAAAAAAAAAAull, 0xBBBBBBBBBBBBBBBBull);
	h.SetMmiPair(reg::a1, 0xCCCCCCCCCCCCCCCCull, 0xDDDDDDDDDDDDDDDDull);
	h.LoadProgram({ee::PCPYUD(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xBBBBBBBBBBBBBBBBull, 0xDDDDDDDDDDDDDDDDull);
}

// ---------------- MMI0: saturating signed add/sub ----------------

TEST(EeRecMmiSimd, PaddsbClampsSignedByteOverflow)
{
	// Per byte: s8 add, clamped to [-128, +127]. Mix: (127 + 1) → +127,
	// (-128 + -1) → -128, (10 + 20) → 30, (-5 + 3) → -2.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x7F800AFB0102FFFFull, 0);
	h.SetMmiPair(reg::a1, 0x01FF14030001FFFFull, 0);
	//   Bytes (MSB→LSB):  7F+01=80→7F,  80+FF=7F→80, 0A+14=1E, FB+03=FE,
	//                     01+00=01,    02+01=03,    FF+FF=FE, FF+FF=FE
	h.LoadProgram({ee::PADDSB(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x7F801EFE0103FEFEull, 0);
}

TEST(EeRecMmiSimd, PaddshClampsSignedHalfwordOverflow)
{
	// s16 lanes: (0x7FFF + 1) → 0x7FFF (+max clamp),
	//            (0x8000 + -1) → 0x8000 (-max clamp),
	//            (0x0001 + 0x0002) → 0x0003 (no clamp).
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x7FFF80000001FFFFull, 0);
	h.SetMmiPair(reg::a1, 0x0001FFFF00020001ull, 0);
	h.LoadProgram({ee::PADDSH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x7FFF800000030000ull, 0);
}

TEST(EeRecMmiSimd, PaddswClampsSignedWordOverflow)
{
	// s32 lanes: (INT_MAX + 1) → INT_MAX, (INT_MIN + -1) → INT_MIN,
	//            (100 + 200) → 300, (-5 + 3) → -2.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x7FFFFFFF00000064ull, 0xFFFFFFFB80000000ull);
	// UD[0] lanes: lo=0x00000064 (100), hi=0x7FFFFFFF (INT_MAX).
	// UD[1] lanes: lo=0x80000000 (INT_MIN), hi=0xFFFFFFFB (-5).
	h.SetMmiPair(reg::a1, 0x00000001000000C8ull, 0x00000003FFFFFFFFull);
	// UD[0] lanes: lo=0x000000C8 (200), hi=0x00000001.
	// UD[1] lanes: lo=0xFFFFFFFF (-1), hi=0x00000003.
	h.LoadProgram({ee::PADDSW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// UD[0]: lo=100+200=300=0x12C, hi=INT_MAX+1 clamps to INT_MAX.
	// UD[1]: lo=INT_MIN+-1 clamps to INT_MIN, hi=-5+3=-2=0xFFFFFFFE.
	h.ExpectMmiPair(reg::v0, 0x7FFFFFFF0000012Cull, 0xFFFFFFFE80000000ull);
}

TEST(EeRecMmiSimd, PsubsbClampsSignedByteUnderflow)
{
	// (-128 - 1) → -128 (clamp), (127 - -1) → 127 (clamp).
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x807F00FF00000000ull, 0);
	h.SetMmiPair(reg::a1, 0x01FFFF0100000000ull, 0);
	//  80-01 = -128-1 = -129 → clamp -128 = 80
	//  7F-FF = 127-(-1) = 128 → clamp 127 = 7F
	//  00-FF = 0-(-1)   = 1
	//  FF-01 = -1-1     = -2 = FE
	h.LoadProgram({ee::PSUBSB(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x807F01FE00000000ull, 0);
}

TEST(EeRecMmiSimd, PsubshClampsSignedHalfwordUnderflow)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x80007FFF00000000ull, 0);
	h.SetMmiPair(reg::a1, 0x0001FFFF00000000ull, 0);
	// 8000 - 0001 = -32768-1 = clamp -32768 = 8000
	// 7FFF - FFFF = 32767-(-1) = clamp 32767 = 7FFF
	h.LoadProgram({ee::PSUBSH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x80007FFF00000000ull, 0);
}

TEST(EeRecMmiSimd, PsubswClampsSignedWordUnderflow)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x7FFFFFFF80000000ull, 0);
	h.SetMmiPair(reg::a1, 0xFFFFFFFF00000001ull, 0);
	// UD[0] lo: 80000000 - 00000001 = INT_MIN-1 → clamp INT_MIN = 0x80000000
	// UD[0] hi: 7FFFFFFF - FFFFFFFF = INT_MAX-(-1) → clamp INT_MAX = 0x7FFFFFFF
	h.LoadProgram({ee::PSUBSW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x7FFFFFFF80000000ull, 0);
}

// ---------------- MMI1: saturating unsigned add/sub ----------------

TEST(EeRecMmiSimd, PaddubClampsUnsignedByteOverflow)
{
	// (0xFF + 0x01) → 0xFF (clamp), (0x80 + 0x80) → 0xFF (clamp),
	// (0x10 + 0x20) → 0x30 (no clamp).
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xFF801000000000FFull, 0);
	h.SetMmiPair(reg::a1, 0x0180200000000001ull, 0);
	h.LoadProgram({ee::PADDUB(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xFFFF3000000000FFull, 0);
}

TEST(EeRecMmiSimd, PadduhClampsUnsignedHalfwordOverflow)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xFFFF8000000000FFull, 0);
	h.SetMmiPair(reg::a1, 0x00018000FFFF0001ull, 0);
	// FFFF+0001=FFFF clamp, 8000+8000=FFFF clamp, 0000+FFFF=FFFF, 00FF+0001=0100.
	h.LoadProgram({ee::PADDUH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xFFFFFFFFFFFF0100ull, 0);
}

TEST(EeRecMmiSimd, PadduwClampsUnsignedWordOverflow)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x00000000FFFFFFFFull, 0x80000000FFFFFFFFull);
	h.SetMmiPair(reg::a1, 0x8000000000000001ull, 0x8000000000000001ull);
	// UD[0] lo: FFFFFFFF + 00000001 = clamp FFFFFFFF
	// UD[0] hi: 00000000 + 80000000 = 80000000
	// UD[1] lo: FFFFFFFF + 00000001 = clamp FFFFFFFF
	// UD[1] hi: 80000000 + 80000000 = clamp FFFFFFFF
	h.LoadProgram({ee::PADDUW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x80000000FFFFFFFFull, 0xFFFFFFFFFFFFFFFFull);
}

TEST(EeRecMmiSimd, PsububClampsUnsignedByteUnderflow)
{
	// (0x00 - 0x01) → 0x00 (clamp), (0x80 - 0x80) → 0x00, (0xFF - 0x01) → 0xFE.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x008010FF00000000ull, 0);
	h.SetMmiPair(reg::a1, 0x0180010000000000ull, 0);
	h.LoadProgram({ee::PSUBUB(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x00000FFF00000000ull, 0);
}

TEST(EeRecMmiSimd, PsubuhClampsUnsignedHalfwordUnderflow)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0000800000FFFFFFull, 0);
	h.SetMmiPair(reg::a1, 0x000180000000FFFEull, 0);
	// 0000-0001=clamp 0000, 8000-8000=0000, 00FF-0000=00FF, FFFF-FFFE=0001.
	h.LoadProgram({ee::PSUBUH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x0000000000FF0001ull, 0);
}

TEST(EeRecMmiSimd, PsubuwClampsUnsignedWordUnderflow)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x00000000FFFFFFFFull, 0x7FFFFFFF80000000ull);
	h.SetMmiPair(reg::a1, 0x00000001FFFFFFFFull, 0x80000000FFFFFFFFull);
	// UD[0] lo: FFFFFFFF - FFFFFFFF = 00000000
	// UD[0] hi: 00000000 - 00000001 = clamp 0
	// UD[1] lo: 80000000 - FFFFFFFF = clamp 0
	// UD[1] hi: 7FFFFFFF - 80000000 = clamp 0
	h.LoadProgram({ee::PSUBUW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x0000000000000000ull, 0);
}

// ---------------- MMI0: missing parallel sub/compare fills ----------------

TEST(EeRecMmiSimd, PsubhPacked16BitLanes)
{
	// 8 × u16 lanes, wrap subtraction (PSUBH is non-saturating).
	// rs = {0x0014, 0x0028, 0x003C, 0x0050, 0x0064, 0x0078, 0x008C, 0x00A0}
	// rt = {0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008}
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0050003C00280014ull, 0x00A0008C00780064ull);
	h.SetMmiPair(reg::a1, 0x0004000300020001ull, 0x0008000700060005ull);
	h.LoadProgram({ee::PSUBH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// {19, 38, 57, 76} / {95, 114, 133, 152}
	h.ExpectMmiPair(reg::v0, 0x004C003900260013ull, 0x009800850072005Full);
}

TEST(EeRecMmiSimd, PsubhWrapsOnUnderflow)
{
	// 0 - 1 = 0xFFFF on every lane (wrap, not saturate).
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0, 0);
	h.SetMmiPair(reg::a1, 0x0001000100010001ull, 0x0001000100010001ull);
	h.LoadProgram({ee::PSUBH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull);
}

TEST(EeRecMmiSimd, PsubbPacked8BitLanesWrap)
{
	// 16 × u8 lanes, wrap subtraction.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x1010101010101010ull, 0x2020202020202020ull);
	h.SetMmiPair(reg::a1, 0x0202020202020202ull, 0x0505050505050505ull);
	h.LoadProgram({ee::PSUBB(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x0E0E0E0E0E0E0E0Eull, 0x1B1B1B1B1B1B1B1Bull);
}

TEST(EeRecMmiSimd, PcgthSignedHalfword)
{
	// 8 × s16 lanes; 0xFFFF if rs > rt signed, else 0. rs=1 in every lane;
	// rt mixes signed values around the compare boundary.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0001000100010001ull, 0x0001000100010001ull);
	h.SetMmiPair(reg::a1, 0xFFFF000000010000ull, 0x00010001FFFFFFFFull);
	// rt lanes (LE order): lo US[0..3] = {0, 1, 0, -1}; hi US[4..7] = {-1, -1, 1, 1}.
	// PCGTH rs(=1) > rt:  lo {T, F, T, T}, hi {T, T, F, F}.
	h.LoadProgram({ee::PCGTH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// lo (US[3..0] LE): {T, T, F, T} = {0xFFFF, 0xFFFF, 0x0000, 0xFFFF}
	// hi (US[7..4] LE): {F, F, T, T} = {0x0000, 0x0000, 0xFFFF, 0xFFFF}
	h.ExpectMmiPair(reg::v0, 0xFFFFFFFF0000FFFFull, 0x00000000FFFFFFFFull);
}

TEST(EeRecMmiSimd, PcgtbSignedByte)
{
	// 16 × s8 lanes. rs=1 in every lane; rt with mixed signed values.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0101010101010101ull, 0x0101010101010101ull);
	h.SetMmiPair(reg::a1, 0xFF00010002FF0100ull, 0x00FF010001FF00FFull);
	// LE: lo UC[0..7] = {0, 1, FF, 2, 0, 1, 0, FF}     → signed {0, 1, -1, 2, 0, 1, 0, -1}
	//     hi UC[8..15] = {FF, 0, FF, 1, 0, 1, FF, 0}    → signed {-1, 0, -1, 1, 0, 1, -1, 0}
	// 1>x signed:
	//   lo: {T, F, T, F, T, F, T, T} → bytes {FF, 00, FF, 00, FF, 00, FF, FF}
	//   hi: {T, T, T, F, T, F, T, T} → bytes {FF, FF, FF, 00, FF, 00, FF, FF}
	h.LoadProgram({ee::PCGTB(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// LE u64 lo (UC[7]<<56 ... | UC[0]) = 0xFF_FF_00_FF_00_FF_00_FF
	// LE u64 hi (UC[15]<<56 ... | UC[8]) = 0xFF_FF_00_FF_00_FF_FF_FF
	h.ExpectMmiPair(reg::v0, 0xFFFF00FF00FF00FFull, 0xFFFF00FF00FFFFFFull);
}

TEST(EeRecMmiSimd, PceqhHalfwordEqual)
{
	// 8 × u16 lanes, 0xFFFF if equal, else 0.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x000A0009000B000Aull, 0x000C000B000A000Aull);
	h.SetMmiPair(reg::a1, 0x000A000A000A000Aull, 0x000A000A000A000Aull);
	// equal pattern (lo): {T, F, F, T} = lane0..3 → 0xFFFF, 0x0000, 0x0000, 0xFFFF
	// LE u64 (lane3..lane0) = 0xFFFF 0000 0000 FFFF
	// Hi: {T, T, F, F} → 0x0000 0000 FFFF FFFF
	h.LoadProgram({ee::PCEQH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xFFFF00000000FFFFull, 0x00000000FFFFFFFFull);
}
