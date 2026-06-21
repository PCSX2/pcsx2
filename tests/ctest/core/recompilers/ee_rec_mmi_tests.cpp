// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// MMI non-SIMD coverage — accumulator-mode arithmetic, pack/unpack, lane
// exchange, HI/LO transfer, and parallel-shift-immediate. Parallel-arith
// SIMD (PADDx/PSUBx/PCEQx/PCGTx) lives in ee_rec_mmi_simd_tests.cpp.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

TEST(EeRecMmi, MaddAccumulates32Bit)
{
	// MADD: (HI:LO) = (HI:LO) + rs * rt, with 32-bit sign-extended operands.
	// Also writes the new LO into rd.
	EeRecTestHarness h;
	h.SetLo64(100);                    // accumulator seed
	h.SetHi64(0);
	h.SetGpr64(reg::a0, 10);
	h.SetGpr64(reg::a1, 20);
	h.LoadProgram({ee::MADD(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetLo64Interp() & 0xFFFFFFFFull, 300ull);  // 100 + 10*20
	EXPECT_EQ(h.GetGpr64Interp(reg::v0) & 0xFFFFFFFFull, 300ull);
}

TEST(EeRecMmi, Plzcw)
{
	// PLZCW writes the sign-bit run-length of each 32-bit half of rs.UD[0]
	// into the two 32-bit halves of rd.UD[0].
	//   lo32 = 0x00000001 → sign bit 0, 30 leading zeros follow → count = 30
	//   hi32 = 0xFFFFFFFF → all-ones, 31 leading ones follow    → count = 31
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFFFFFF00000001ull);
	h.LoadProgram({ee::PLZCW(reg::v0, reg::a0)});
	h.Run();
	const u64 v = h.GetGpr64Interp(reg::v0);
	EXPECT_EQ(v & 0xFFFFFFFFull,        30ull);
	EXPECT_EQ((v >> 32) & 0xFFFFFFFFull, 31ull);
}

// ===========================================================================
//  HI/LO 128-bit transfer — PMTHI / PMFHI / PMTLO / PMFLO
//
//  PS2 HI/LO are 128-bit (used by MULT1 / DIV1 / parallel multiply pipeline
//  on the second 64-bit half). PMTHI/PMTLO move a full 128-bit GPR into
//  the HI/LO register; PMFHI/PMFLO read it back. Tested here as a
//  round-trip: GPR → HI/LO via PMTHIx → GPR via PMFHIx, ExpectGpr128.
// ===========================================================================

TEST(EeRecMmi, PmthiPmfhiRoundTrip128Bit)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);
	h.LoadProgram({
		ee::PMTHI(reg::a0),
		ee::PMFHI(reg::v0),
	});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);
}

TEST(EeRecMmi, PmtloPmfloRoundTrip128Bit)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xCAFEBABEDEADBEEFull, 0xBADC0FFEEDFACE99ull);
	h.LoadProgram({
		ee::PMTLO(reg::a0),
		ee::PMFLO(reg::v0),
	});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xCAFEBABEDEADBEEFull, 0xBADC0FFEEDFACE99ull);
}

// PMTHL.LW writes Rs's four words into the EVEN-indexed word
// lanes of LO and HI, preserving the odd-indexed lanes (the upper half of
// each 64-bit LO/HI word):
//   LO.UL[0] = Rs.UL[0]    LO.UL[2] = Rs.UL[2]
//   HI.UL[0] = Rs.UL[1]    HI.UL[2] = Rs.UL[3]
// The odd-indexed UL[1]/UL[3] of LO/HI must stay untouched.
TEST(EeRecMmi, PmthlWritesEvenWordsToLoAndHiPreservingOdds)
{
	EeRecTestHarness h;
	// Pre-seed LO/HI with sentinels in odd-indexed words (UL[1], UL[3]).
	// SetLoPair(lo_qw, hi_qw) writes LO.UD[0]=lo_qw, LO.UD[1]=hi_qw, so
	// 0xAAAA000000000000 puts 0xAAAA0000 in UL[1] and 0 in UL[0].
	h.SetLoPair(0xAAAA000000000000ull, 0xBBBB000000000000ull);
	h.SetHiPair(0xCCCC000000000000ull, 0xDDDD000000000000ull);
	// Rs words [0..3] = 0x1111_1111, 0x2222_2222, 0x3333_3333, 0x4444_4444.
	h.SetMmiPair(reg::a0, 0x2222222211111111ull, 0x4444444433333333ull);
	h.LoadProgram({
		ee::PMTHL(reg::a0),
		ee::PMFLO(reg::v0),
		ee::PMFHI(reg::v1),
	});
	h.Run();
	// LO = [Rs.UL[0], LO.UL[1]_kept, Rs.UL[2], LO.UL[3]_kept]
	//    = [0x11111111, 0xAAAA0000, 0x33333333, 0xBBBB0000]
	h.ExpectMmiPair(reg::v0, 0xAAAA000011111111ull, 0xBBBB000033333333ull);
	// HI = [Rs.UL[1], HI.UL[1]_kept, Rs.UL[3], HI.UL[3]_kept]
	//    = [0x22222222, 0xCCCC0000, 0x44444444, 0xDDDD0000]
	h.ExpectMmiPair(reg::v1, 0xCCCC000022222222ull, 0xDDDD000044444444ull);
}

// ===========================================================================
//  Halfword interleave — PINTH / PINTEH
//
//  PINTH (MMI.cpp:1121): rd[i*2]   = rt.US[i]; rd[i*2+1] = rs.US[i+4]
//                       — pairs lo halves of rt with hi halves of rs.
//  PINTEH (MMI.cpp:1498): rd[i*2]  = rt.US[i*2]; rd[i*2+1] = rs.US[i*2]
//                       — interleaves even halves of rt and rs.
// ===========================================================================

TEST(EeRecMmi, PinthLoLanesOfRtPairedWithHiLanesOfRs)
{
	// rt halves [t0..t7] = 0x10..0x17, rs halves [s0..s7] = 0x80..0x87.
	// rd = {t0, s4, t1, s5, t2, s6, t3, s7} = {10,84, 11,85, 12,86, 13,87}.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0013001200110010ull, 0x0017001600150014ull);  // rs (a0)
	h.SetMmiPair(reg::a1, 0x0083008200810080ull, 0x0087008600850084ull);  // rt (a1)
	h.LoadProgram({ee::PINTH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// lo qword: {t0, s4, t1, s5} = {0x80, 0x14, 0x81, 0x15} → 0x0015 0081 0014 0080
	// hi qword: {t2, s6, t3, s7} = {0x82, 0x16, 0x83, 0x17} → 0x0017 0083 0016 0082
	h.ExpectMmiPair(reg::v0, 0x0015008100140080ull, 0x0017008300160082ull);
}

TEST(EeRecMmi, PintehEvenHalvesInterleaved)
{
	// PINTEH: rd.US[2k]   = rt.US[2k]; rd.US[2k+1] = rs.US[2k] for k=0..3.
	// Drops the odd-indexed halves of both inputs.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0xAAAA0001AAAA0002ull, 0xAAAA0003AAAA0004ull);  // rs even = {2,1,4,3}
	h.SetMmiPair(reg::a1, 0xBBBB0010BBBB0020ull, 0xBBBB0030BBBB0040ull);  // rt even = {0x20,0x10,0x40,0x30}
	h.LoadProgram({ee::PINTEH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// rd.US[0]=rt.US[0]=0x20, rd.US[1]=rs.US[0]=0x02, rd.US[2]=rt.US[2]=0x10,
	// rd.US[3]=rs.US[2]=0x01 → lo qword 0x0001 0010 0002 0020
	// rd.US[4]=rt.US[4]=0x40, rd.US[5]=rs.US[4]=0x04, rd.US[6]=rt.US[6]=0x30,
	// rd.US[7]=rs.US[6]=0x03 → hi qword 0x0003 0030 0004 0040
	h.ExpectMmiPair(reg::v0, 0x0001001000020020ull, 0x0003003000040040ull);
}

// ===========================================================================
//  Halfword/word lane shuffles — PEXEH, PEXEW, PEXCH, PEXCW, PROT3W, PREVH
//
//  Pure permutations of rt's lanes into rd; rs is unused (encoded as 0).
//  Semantics per MMI.cpp.
// ===========================================================================

TEST(EeRecMmi, PexehSwapsHalfwordLanes0and2)
{
	// rd.US[0]<->rt.US[2]; rd.US[1]=rt.US[1]; rd.US[3]=rt.US[3];
	// rd.US[4]<->rt.US[6]; rd.US[5]=rt.US[5]; rd.US[7]=rt.US[7];
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0003000200010000ull, 0x0007000600050004ull);
	h.LoadProgram({ee::PEXEH(reg::v0, reg::a0)});
	h.Run();
	// lo: {US[2], US[1], US[0], US[3]} = {0x2, 0x1, 0x0, 0x3} → 0x0003000000010002
	// hi: {US[6], US[5], US[4], US[7]} = {0x6, 0x5, 0x4, 0x7} → 0x0007000400050006
	h.ExpectMmiPair(reg::v0, 0x0003000000010002ull, 0x0007000400050006ull);
}

TEST(EeRecMmi, PexewSwapsWordLanes0and2)
{
	// rd.UL[0]<->rt.UL[2]; rd.UL[1]=rt.UL[1]; rd.UL[3]=rt.UL[3].
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x1111111100000000ull, 0x3333333322222222ull);
	h.LoadProgram({ee::PEXEW(reg::v0, reg::a0)});
	h.Run();
	// rd.UL[0]=UL[2]=0x22222222, rd.UL[1]=UL[1]=0x11111111
	//   → lo qword (UL[1]<<32)|UL[0] = 0x1111111122222222
	// rd.UL[2]=UL[0]=0x00000000, rd.UL[3]=UL[3]=0x33333333
	//   → hi qword = 0x3333333300000000
	h.ExpectMmiPair(reg::v0, 0x1111111122222222ull, 0x3333333300000000ull);
}

TEST(EeRecMmi, Prot3wRotatesLow3Words)
{
	// rd.UL[0]=rt.UL[1]; rd.UL[1]=rt.UL[2]; rd.UL[2]=rt.UL[0]; rd.UL[3]=rt.UL[3].
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x22222222 | (0x11111111ull << 32), 0x44444444 | (0x33333333ull << 32));
	// rt.UL = {0x22222222, 0x11111111, 0x44444444, 0x33333333}
	h.LoadProgram({ee::PROT3W(reg::v0, reg::a0)});
	h.Run();
	// rd.UL = {0x11111111, 0x44444444, 0x22222222, 0x33333333}
	h.ExpectMmiPair(reg::v0, 0x4444444411111111ull, 0x3333333322222222ull);
}

TEST(EeRecMmi, PexchSwapsHalfwordsWithinEachWord)
{
	// rd.US[0]=rt.US[0]; rd.US[1]<->rt.US[2]; rd.US[3]=rt.US[3];
	// rd.US[4]=rt.US[4]; rd.US[5]<->rt.US[6]; rd.US[7]=rt.US[7];
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0003000200010000ull, 0x0007000600050004ull);
	h.LoadProgram({ee::PEXCH(reg::v0, reg::a0)});
	h.Run();
	// lo: {US[0], US[2], US[1], US[3]} = {0, 2, 1, 3}
	// hi: {US[4], US[6], US[5], US[7]} = {4, 6, 5, 7}
	h.ExpectMmiPair(reg::v0, 0x0003000100020000ull, 0x0007000500060004ull);
}

TEST(EeRecMmi, PexcwSwapsMiddleWordLanes)
{
	// rd.UL = {rt.UL[0], rt.UL[2], rt.UL[1], rt.UL[3]}.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x1111111100000000ull, 0x3333333322222222ull);
	// UL[] = {0, 0x11111111, 0x22222222, 0x33333333}
	h.LoadProgram({ee::PEXCW(reg::v0, reg::a0)});
	h.Run();
	// {UL[0], UL[2], UL[1], UL[3]} = {0, 0x22222222, 0x11111111, 0x33333333}
	h.ExpectMmiPair(reg::v0, 0x2222222200000000ull, 0x3333333311111111ull);
}

TEST(EeRecMmi, PrevhReversesHalfwordsWithinEachQword)
{
	// rd.US[0]=rt.US[3]; rd.US[1]=rt.US[2]; rd.US[2]=rt.US[1]; rd.US[3]=rt.US[0]
	// (and same shape for upper qword: US[4..7] reversed).
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0003000200010000ull, 0x0007000600050004ull);
	h.LoadProgram({ee::PREVH(reg::v0, reg::a0)});
	h.Run();
	// lo: {US[3], US[2], US[1], US[0]} = {3, 2, 1, 0} → 0x0000 0001 0002 0003
	// hi: {US[7], US[6], US[5], US[4]} = {7, 6, 5, 4} → 0x0004 0005 0006 0007
	h.ExpectMmiPair(reg::v0, 0x0000000100020003ull, 0x0004000500060007ull);
}

// ---------------------------------------------------------------------------
//  Aliased rd == rt for the shuffle rewrites.
//  The 2-/3-op NEON idioms must read rt fully before writing rd; if the
//  allocator hands back the same Q-reg for both, a non-alias-safe sequence
//  would corrupt the source mid-shuffle. These pin the aliased path the
//  rd != rt tests above don't exercise.
// ---------------------------------------------------------------------------

TEST(EeRecMmi, PexewAliasedRdEqualsRt)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x1111111100000000ull, 0x3333333322222222ull);
	h.LoadProgram({ee::PEXEW(reg::a0, reg::a0)});
	h.Run();
	// rd.UL = {UL[2],UL[1],UL[0],UL[3]} = {0x22,0x11,0x00,0x33}
	h.ExpectMmiPair(reg::a0, 0x1111111122222222ull, 0x3333333300000000ull);
}

TEST(EeRecMmi, Prot3wAliasedRdEqualsRt)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x22222222 | (0x11111111ull << 32), 0x44444444 | (0x33333333ull << 32));
	// rt.UL = {0x22222222, 0x11111111, 0x44444444, 0x33333333}
	h.LoadProgram({ee::PROT3W(reg::a0, reg::a0)});
	h.Run();
	// rd.UL = {UL[1],UL[2],UL[0],UL[3]} = {0x11,0x44,0x22,0x33}
	h.ExpectMmiPair(reg::a0, 0x4444444411111111ull, 0x3333333322222222ull);
}

TEST(EeRecMmi, PexcwAliasedRdEqualsRt)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x1111111100000000ull, 0x3333333322222222ull);
	// UL[] = {0, 0x11111111, 0x22222222, 0x33333333}
	h.LoadProgram({ee::PEXCW(reg::a0, reg::a0)});
	h.Run();
	// rd.UL = {UL[0],UL[2],UL[1],UL[3]} = {0, 0x22, 0x11, 0x33}
	h.ExpectMmiPair(reg::a0, 0x2222222200000000ull, 0x3333333311111111ull);
}

// ===========================================================================
//  QFSRV — funnel-shift {Rs:Rt} right by cpuRegs.sa bytes.
//  sa is set via MTSAB(rs,imm) -> sa = (GPR[rs].UL[0] & 0xF) ^ (imm & 0xF).
//  Run() auto-diffs JIT vs interp; ExpectMmiPair pins the concrete result.
// ===========================================================================

TEST(EeRecMmi, QfsrvAdjacentSourceContiguous)
{
	// Rs == Rt+1 (a1 == a0+1) hits the contiguous-memory path that reads the
	// two source registers directly and skips the temp-buffer stores. sa = 4 bytes.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x1122334455667788ull, 0x99AABBCCDDEEFF00ull); // Rt
	h.SetMmiPair(reg::a1, 0xAABBCCDD11223344ull, 0x5566778899AABBCCull); // Rs
	h.LoadProgram({ee::MTSAB(reg::zero, 4), ee::QFSRV(reg::v0, reg::a1, reg::a0)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xDDEEFF0011223344ull, 0x1122334499AABBCCull);
}

TEST(EeRecMmi, QfsrvNonAdjacentSource)
{
	// Rs != Rt+1 (a2 != a0+1) takes the temp-buffer path. Same Rt/Rs values,
	// sa = 7 bytes.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x1122334455667788ull, 0x99AABBCCDDEEFF00ull); // Rt
	h.SetMmiPair(reg::a2, 0xAABBCCDD11223344ull, 0x5566778899AABBCCull); // Rs
	h.LoadProgram({ee::MTSAB(reg::zero, 7), ee::QFSRV(reg::v0, reg::a2, reg::a0)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xAABBCCDDEEFF0011ull, 0xBBCCDD1122334499ull);
}

TEST(EeRecMmi, QfsrvAdjacentRtZeroUsesTempBuffer)
{
	// Rt == r0 with Rs == Rt+1 (at == zero+1): the contiguous path is gated off
	// (it would depend on GPR.r[0] memory holding zero), so this must fall to
	// the temp-buffer path which zero-fills r0 explicitly. sa = 4 bytes.
	EeRecTestHarness h;
	h.SetMmiPair(reg::at, 0xAABBCCDD11223344ull, 0x5566778899AABBCCull); // Rs (at == reg 1)
	h.LoadProgram({ee::MTSAB(reg::zero, 4), ee::QFSRV(reg::v0, reg::at, reg::zero)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x0ull, 0x1122334400000000ull);
}

// ===========================================================================
//  Parallel shift immediate — PSLLH / PSRLH / PSRAH (8 × u16) +
//                             PSLLW / PSRLW / PSRAW (4 × u32)
//
//  Top-level MMI table (funct = 0x34..0x3F) with shift amount in sa.
//  PSxxxH masks sa with 0xF (only 4 bits used since lane is 16-bit).
// ===========================================================================

TEST(EeRecMmi, PsllhShiftsEachHalfwordLeft)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0080000800040002ull, 0x000F00200040FFFFull);
	h.LoadProgram({ee::PSLLH(reg::v0, reg::a0, 4)});
	h.Run();
	// Each halfword << 4 (truncated to 16 bits).
	// lo: {2<<4, 4<<4, 8<<4, 0x80<<4} = {0x20, 0x40, 0x80, 0x800}
	// hi: {0xFFFF<<4 & 0xFFFF=0xFFF0, 0x40<<4=0x400, 0x20<<4=0x200, 0xF<<4=0xF0}
	h.ExpectMmiPair(reg::v0, 0x0800008000400020ull, 0x00F002000400FFF0ull);
}

TEST(EeRecMmi, PsrlhLogicalShiftRight)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0080004000200010ull, 0xF000800040002000ull);
	h.LoadProgram({ee::PSRLH(reg::v0, reg::a0, 4)});
	h.Run();
	// lo: {0x10>>4, 0x20>>4, 0x40>>4, 0x80>>4} = {0x1, 0x2, 0x4, 0x8}
	// hi: {0x2000>>4, 0x4000>>4, 0x8000>>4, 0xF000>>4} = {0x200, 0x400, 0x800, 0xF00}
	h.ExpectMmiPair(reg::v0, 0x0008000400020001ull, 0x0F00080004000200ull);
}

TEST(EeRecMmi, PsrahArithmeticShiftRightSignExtends)
{
	// PSRAH preserves the sign bit (16-bit signed shift right).
	// Lanes with high bit set become 0xFFFF (sign-extended).
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x80008000FFFF8000ull, 0x000180007FFF0001ull);
	h.LoadProgram({ee::PSRAH(reg::v0, reg::a0, 8)});
	h.Run();
	// lane: shift right 8, signed.
	// lo: {0x8000>>8 sign ext = 0xFF80, 0xFFFF>>8 = 0xFFFF, 0x8000 = 0xFF80, 0x8000 = 0xFF80}
	// hi: {0x0001>>8 = 0x0000, 0x7FFF>>8 = 0x007F, 0x8000 = 0xFF80, 0x0001 = 0x0000}
	h.ExpectMmiPair(reg::v0, 0xFF80FF80FFFFFF80ull, 0x0000FF80007F0000ull);
}

// PSxxxH masks the shift amount with 0xF before shifting each 16-bit lane
// (the interpreter does (_Sa_ & 0xf), and a halfword shift is only defined for
// counts in [0,15]). Verify sa ≥ 16 wraps to sa & 0xf.

TEST(EeRecMmi, PsllhMasksSaToFourBits)
{
	// sa=18 → effective shift = 2.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0080000800040002ull, 0x000F00200040FFFFull);
	h.LoadProgram({ee::PSLLH(reg::v0, reg::a0, 18)});
	h.Run();
	// Each halfword << 2 (truncated to 16 bits).
	// lo: {2<<2, 4<<2, 8<<2, 0x80<<2} = {0x8, 0x10, 0x20, 0x200}
	// hi: {0xFFFF<<2 & 0xFFFF=0xFFFC, 0x40<<2=0x100, 0x20<<2=0x80, 0xF<<2=0x3C}
	h.ExpectMmiPair(reg::v0, 0x0200002000100008ull, 0x003C00800100FFFCull);
}

TEST(EeRecMmi, PsrlhMasksSaToFourBits)
{
	// sa=20 → effective shift = 4.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0080004000200010ull, 0xF000800040002000ull);
	h.LoadProgram({ee::PSRLH(reg::v0, reg::a0, 20)});
	h.Run();
	// lo: {0x10>>4, 0x20>>4, 0x40>>4, 0x80>>4} = {0x1, 0x2, 0x4, 0x8}
	// hi: {0x2000>>4, 0x4000>>4, 0x8000>>4, 0xF000>>4} = {0x200, 0x400, 0x800, 0xF00}
	h.ExpectMmiPair(reg::v0, 0x0008000400020001ull, 0x0F00080004000200ull);
}

TEST(EeRecMmi, PsrahMasksSaToFourBits)
{
	// sa=24 → effective shift = 8.
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x80008000FFFF8000ull, 0x000180007FFF0001ull);
	h.LoadProgram({ee::PSRAH(reg::v0, reg::a0, 24)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0xFF80FF80FFFFFF80ull, 0x0000FF80007F0000ull);
}

TEST(EeRecMmi, PsllwShiftsEachWordLeft)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0000000200000001ull, 0x0000000400000003ull);
	h.LoadProgram({ee::PSLLW(reg::v0, reg::a0, 4)});
	h.Run();
	// Each 32-bit word << 4. {1, 2, 3, 4} → {0x10, 0x20, 0x30, 0x40}.
	h.ExpectMmiPair(reg::v0, 0x0000002000000010ull, 0x0000004000000030ull);
}

TEST(EeRecMmi, PsrlwLogicalShiftRight)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x80000000FFFFFFFFull, 0x000000FF00000010ull);
	h.LoadProgram({ee::PSRLW(reg::v0, reg::a0, 4)});
	h.Run();
	// {0xFFFFFFFF >> 4, 0x80000000 >> 4, 0x10 >> 4, 0xFF >> 4}
	// = {0x0FFFFFFF, 0x08000000, 0x1, 0xF}
	h.ExpectMmiPair(reg::v0, 0x080000000FFFFFFFull, 0x0000000F00000001ull);
}

TEST(EeRecMmi, PsrawArithmeticShiftRightSignExtends)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x800000007FFFFFFFull, 0xFFFFFFF000000010ull);
	h.LoadProgram({ee::PSRAW(reg::v0, reg::a0, 4)});
	h.Run();
	// {0x7FFFFFFF >> 4 = 0x07FFFFFF, 0x80000000 sign>>4 = 0xF8000000,
	//  0x10 >> 4 = 0x1, 0xFFFFFFF0 sign>>4 = 0xFFFFFFFF}
	h.ExpectMmiPair(reg::v0, 0xF800000007FFFFFFull, 0xFFFFFFFF00000001ull);
}

// ===========================================================================
//  Parallel multiply (16-bit lanes) — PMULTH / PMADDH / PMSUBH
//
//  Eight 16-bit signed multiplies: r[i] = rs.SH[i] * rt.SH[i], i = 0..7.
//  Result distribution (MMI.cpp:1156-1184):
//    LO.UL[0..3] (re-)receive { r0, r1, r4, r5 }
//    HI.UL[0..3] (re-)receive { r2, r3, r6, r7 }
//    Rd.UL[0..3] = { LO[0], HI[0], LO[2], HI[2] } (post-update)
//  PMADDH / PMSUBH read+modify the existing LO/HI; PMULTH overwrites.
// ===========================================================================

TEST(EeRecMmi, PmulthSignedHwordMultiplyDistributesAcrossHiLoRd)
{
	EeRecTestHarness h;
	// rs.SH[0..7] = { 1, 2, 3, 4, 5, 6, 7, 8 }
	h.SetMmiPair(reg::a0, 0x0004000300020001ull, 0x0008000700060005ull);
	// rt.SH[0..7] = { 0x0A, 0x14, 0x1E, 0x28, 0x32, 0x3C, 0x46, 0x50 }
	h.SetMmiPair(reg::a1, 0x0028001E0014000Aull, 0x00500046003C0032ull);
	h.LoadProgram({ee::PMULTH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// Products r[i] = rs.SH[i] * rt.SH[i]:
	//   r0=0x0A, r1=0x28, r2=0x5A, r3=0xA0, r4=0xFA, r5=0x168, r6=0x1EA, r7=0x280
	// Rd = { r0, r2, r4, r6 } as 4×32 = { 0x0A, 0x5A, 0xFA, 0x1EA }
	//   Rd lo qw [UL1 UL0] = 0x0000005A 0000000A
	//   Rd hi qw [UL3 UL2] = 0x000001EA 000000FA
	h.ExpectMmiPair(reg::v0, 0x0000005A0000000Aull, 0x000001EA000000FAull);
}

TEST(EeRecMmi, PmaddhAccumulatesIntoHiLoAndWritesRdEvenLanes)
{
	EeRecTestHarness h;
	// rs.SH = { 1, 1, 1, 1, 2, 2, 2, 2 }, rt.SH = { 3, 3, 3, 3, 4, 4, 4, 4 }
	// Products r[0..3] = 3, r[4..7] = 8.
	// LO seed UL[0..1] only (harness limit) = { 100, 200 }; LO.UL[2..3] left at 0.
	h.SetMmiPair(reg::a0, 0x0001000100010001ull, 0x0002000200020002ull);
	h.SetMmiPair(reg::a1, 0x0003000300030003ull, 0x0004000400040004ull);
	h.SetLo64(0x00000064000000C8ull); // LO.UL[0]=200(0xC8), LO.UL[1]=100(0x64). UL[2..3]=0.
	h.SetHi64(0x0000003200000019ull); // HI.UL[0]=25(0x19),  HI.UL[1]=50(0x32). UL[2..3]=0.
	h.LoadProgram({ee::PMADDH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// new_LO = LO + {r0,r1,r4,r5} = {200,100,0,0} + {3,3,8,8} = {203,103,8,8}
	// new_HI = HI + {r2,r3,r6,r7} = {25,50,0,0}  + {3,3,8,8} = {28,53,8,8}
	// Rd = { new_LO[0], new_HI[0], new_LO[2], new_HI[2] } = { 203, 28, 8, 8 }
	//     = { 0xCB, 0x1C, 0x08, 0x08 }
	h.ExpectMmiPair(reg::v0, 0x0000001C000000CBull, 0x0000000800000008ull);
}

TEST(EeRecMmi, PmsubhSubtractsFromHiLoAndWritesRdEvenLanes)
{
	EeRecTestHarness h;
	h.SetMmiPair(reg::a0, 0x0001000100010001ull, 0x0002000200020002ull);
	h.SetMmiPair(reg::a1, 0x0003000300030003ull, 0x0004000400040004ull);
	h.SetLo64(0x00000064000000C8ull); // LO.UL[0]=200, LO.UL[1]=100
	h.SetHi64(0x0000003200000019ull); // HI.UL[0]=25,  HI.UL[1]=50
	h.LoadProgram({ee::PMSUBH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// new_LO = LO - {r0,r1,r4,r5} = {200,100,0,0} - {3,3,8,8} = {197,97,-8,-8}
	// new_HI = HI - {r2,r3,r6,r7} = {25,50,0,0}  - {3,3,8,8} = {22,47,-8,-8}
	// Rd = { 197, 22, -8, -8 } = { 0xC5, 0x16, 0xFFFFFFF8, 0xFFFFFFF8 }
	h.ExpectMmiPair(reg::v0, 0x00000016000000C5ull, 0xFFFFFFF8FFFFFFF8ull);
}

// ===========================================================================
// PMULTW / PMULTUW — signed/unsigned 32x32->64 multiply on even-indexed lanes
//   prod[0] = Rs.SL[0] * Rt.SL[0]  (SL for PMULTW, UL for PMULTUW)
//   prod[1] = Rs.SL[2] * Rt.SL[2]
//   LO.UD[k] = sign-extended low32 of prod[k]
//   HI.UD[k] = sign-extended high32 of prod[k]
//   Rd.UD[k] = full 64-bit product
// ===========================================================================

TEST(EeRecMmi, PmultwSignedMultiplyDistributesAcrossHiLoRd)
{
	EeRecTestHarness h;
	// Rs.SL[0]=0x40000000, Rs.SL[2]=-2; SL[1]/SL[3] don't matter.
	h.SetMmiPair(reg::a0, 0x0000000040000000ull, 0x00000000FFFFFFFEull);
	// Rt.SL[0]=0x10, Rt.SL[2]=3
	h.SetMmiPair(reg::a1, 0x0000000000000010ull, 0x0000000000000003ull);
	h.LoadProgram({ee::PMULTW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// prod[0] = 0x40000000 * 0x10 = 0x4_00000000
	//   Rd.UD[0] = 0x0000000400000000
	// prod[1] = (-2) * 3 = -6 = 0xFFFFFFFFFFFFFFFA (signed 64)
	//   Rd.UD[1] = 0xFFFFFFFFFFFFFFFA
	h.ExpectMmiPair(reg::v0, 0x0000000400000000ull, 0xFFFFFFFFFFFFFFFAull);
}

TEST(EeRecMmi, PmultuwUnsignedMultiplyDistributesAcrossHiLoRd)
{
	EeRecTestHarness h;
	// Rs.UL[0]=0xFFFFFFFF, Rs.UL[2]=0x100
	h.SetMmiPair(reg::a0, 0x00000000FFFFFFFFull, 0x0000000000000100ull);
	// Rt.UL[0]=2,           Rt.UL[2]=0x200
	h.SetMmiPair(reg::a1, 0x0000000000000002ull, 0x0000000000000200ull);
	h.LoadProgram({ee::PMULTUW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// prod[0] = 0xFFFFFFFF * 2 = 0x1FFFFFFFE (unsigned 64)
	//   Rd.UD[0] = 0x00000001FFFFFFFE
	// prod[1] = 0x100 * 0x200 = 0x20000
	//   Rd.UD[1] = 0x0000000000020000
	h.ExpectMmiPair(reg::v0, 0x00000001FFFFFFFEull, 0x0000000000020000ull);
}

// ===========================================================================
// PHMADH / PHMSBH — horizontal signed-16x16 multiply with paired (sum / diff)
//
//   Eight products r[i] = Rs.SH[i] * Rt.SH[i], paired into k=0..3 (i = 2k, 2k+1).
//   PHMADH: pair_sum[k] = r[2k] + r[2k+1]
//   PHMSBH: pair_diff[k] = r[2k+1] - r[2k]
//   "firsttemp[k]" = r[2k+1] (the second product in each pair).
//   LO = { pair[0], firsttemp[0]   , pair[2], firsttemp[2]    }   (PHMADH)
//        { pair[0], ~firsttemp[0]  , pair[2], ~firsttemp[2]   }   (PHMSBH)
//   HI = { pair[1], firsttemp[1]   , pair[3], firsttemp[3]    }   (PHMADH)
//        { pair[1], ~firsttemp[1]  , pair[3], ~firsttemp[3]   }   (PHMSBH)
//   Rd = { pair[0], pair[1], pair[2], pair[3] }
// ===========================================================================

TEST(EeRecMmi, PhmadhHorizontalSignedHwordMultiplyAddsPairsAcrossHiLoRd)
{
	EeRecTestHarness h;
	// Rs.SH = { 1, 2, 3, 4, 5, 6, 7, 8 }
	h.SetMmiPair(reg::a0, 0x0004000300020001ull, 0x0008000700060005ull);
	// Rt.SH = { 10, 20, 30, 40, 50, 60, 70, 80 }
	h.SetMmiPair(reg::a1, 0x0028001E0014000Aull, 0x00500046003C0032ull);
	h.LoadProgram({ee::PHMADH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// Products: r0=10, r1=40, r2=90, r3=160, r4=250, r5=360, r6=490, r7=640
	// pair_sums: 50, 250, 610, 1130
	// Rd = { 50, 250, 610, 1130 } = { 0x32, 0xFA, 0x262, 0x46A }
	h.ExpectMmiPair(reg::v0, 0x000000FA00000032ull, 0x0000046A00000262ull);
}

TEST(EeRecMmi, PhmsbhHorizontalSignedHwordMultiplySubtractsPairsAcrossHiLoRd)
{
	EeRecTestHarness h;
	// Same operands as PHMADH for easy comparison.
	h.SetMmiPair(reg::a0, 0x0004000300020001ull, 0x0008000700060005ull);
	h.SetMmiPair(reg::a1, 0x0028001E0014000Aull, 0x00500046003C0032ull);
	h.LoadProgram({ee::PHMSBH(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// pair_diffs: r1-r0=30, r3-r2=70, r5-r4=110, r7-r6=150
	// Rd = { 30, 70, 110, 150 } = { 0x1E, 0x46, 0x6E, 0x96 }
	h.ExpectMmiPair(reg::v0, 0x000000460000001Eull, 0x000000960000006Eull);
}

// ===========================================================================
// PEXT5 / PPAC5 — RGB1555 <-> BGRA8 lane-wise pack / unpack.
//
//   PEXT5 (expand per 32-bit lane):
//     rd = ((rt & 0x001F) << 3) | ((rt & 0x03E0) << 6)
//        | ((rt & 0x7C00) << 9) | ((rt & 0x8000) << 16);
//   PPAC5 (inverse):
//     rd = ((rt >>  3) & 0x001F) | ((rt >>  6) & 0x03E0)
//        | ((rt >>  9) & 0x7C00) | ((rt >> 16) & 0x8000);
// ===========================================================================

TEST(EeRecMmi, Pext5ExpandsRgb1555ToBgra8PerLane)
{
	EeRecTestHarness h;
	// UL[0] = 0xFFFF       (all R/G/B/A bits set → 0x80F8F8F8)
	// UL[1] = 0            (zero  → zero)
	// UL[2] = 0x5555       (alt bits → 0x00A850A8)
	// UL[3] = 0xAAAA       (alt bits with A=1 → 0x8050A850)
	h.SetMmiPair(reg::a0, 0x000000000000FFFFull, 0x0000AAAA00005555ull);
	h.LoadProgram({ee::PEXT5(reg::v0, reg::a0)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x0000000080F8F8F8ull, 0x8050A85000A850A8ull);
}

TEST(EeRecMmi, Ppac5PacksBgra8BackToRgb1555PerLane)
{
	EeRecTestHarness h;
	// Inverse of the Pext5 test — feed the expanded (BGRA8) values back in.
	h.SetMmiPair(reg::a0, 0x0000000080F8F8F8ull, 0x8050A85000A850A8ull);
	h.LoadProgram({ee::PPAC5(reg::v0, reg::a0)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x000000000000FFFFull, 0x0000AAAA00005555ull);
}

// ===========================================================================
// PMFHL — move from HI/LO with one of five lane patterns selected by sa:
//   sa=0 LW : Rd = { LO[0], HI[0], LO[2], HI[2] }           (even words)
//   sa=1 UW : Rd = { LO[1], HI[1], LO[3], HI[3] }           (odd words)
//   sa=2 SLW: Rd.UD[k] = sat(s64(HI.UL[2k]:LO.UL[2k])) → sign-extended s32
//   sa=3 LH : Rd.US = { LO[0], LO[2], HI[0], HI[2],
//                       LO[4], LO[6], HI[4], HI[6] }        (even halfwords)
//   sa=4 SH : Rd.US lanes = PMFHL_CLAMP(LO.UL[0..3] and HI.UL[0..3]) interleaved
//             at 32-bit-pair granularity.
// ===========================================================================

TEST(EeRecMmi, PmfhlLwInterleavesEvenWordsOfLoAndHi)
{
	EeRecTestHarness h;
	// LO.UL = {0x10000001, 0x20000002, 0x30000003, 0x40000004}
	h.SetLoPair(0x2000000210000001ull, 0x4000000430000003ull);
	// HI.UL = {0xA000000A, 0xB000000B, 0xC000000C, 0xD000000D}
	h.SetHiPair(0xB000000BA000000Aull, 0xD000000DC000000Cull);
	h.LoadProgram({ee::PMFHL(reg::v0, 0x00)});
	h.Run();
	// Rd.UL = {LO[0]=0x10000001, HI[0]=0xA000000A, LO[2]=0x30000003, HI[2]=0xC000000C}
	h.ExpectMmiPair(reg::v0, 0xA000000A10000001ull, 0xC000000C30000003ull);
}

TEST(EeRecMmi, PmfhlUwInterleavesOddWordsOfLoAndHi)
{
	EeRecTestHarness h;
	h.SetLoPair(0x2000000210000001ull, 0x4000000430000003ull);
	h.SetHiPair(0xB000000BA000000Aull, 0xD000000DC000000Cull);
	h.LoadProgram({ee::PMFHL(reg::v0, 0x01)});
	h.Run();
	// Rd.UL = {LO[1]=0x20000002, HI[1]=0xB000000B, LO[3]=0x40000004, HI[3]=0xD000000D}
	h.ExpectMmiPair(reg::v0, 0xB000000B20000002ull, 0xD000000D40000004ull);
}

TEST(EeRecMmi, PmfhlSlwSaturatesComposedS64InRange)
{
	EeRecTestHarness h;
	// Pair 0: HI[0]:LO[0] = 0x00000000:0x12345678 = 0x12345678 (in range, positive)
	//   Rd.UD[0] = (s64)(s32)0x12345678 = 0x0000000012345678
	// Pair 1: HI[2]:LO[2] = 0xFFFFFFFF:0xFEDCBA98 = 0xFFFFFFFFFEDCBA98 (= -19088744, in range)
	//   Rd.UD[1] = (s64)(s32)0xFEDCBA98 = 0xFFFFFFFFFEDCBA98
	h.SetLoPair(0x0000000012345678ull, 0x00000000FEDCBA98ull);
	h.SetHiPair(0x0000000000000000ull, 0x00000000FFFFFFFFull);
	h.LoadProgram({ee::PMFHL(reg::v0, 0x02)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x0000000012345678ull, 0xFFFFFFFFFEDCBA98ull);
}

TEST(EeRecMmi, PmfhlSlwSaturatesComposedS64OutOfRange)
{
	EeRecTestHarness h;
	// Pair 0: HI[0]:LO[0] = 0x00000001:0x00000000 = 0x100000000 (= 4294967296 > INT_MAX)
	//   Rd.UD[0] = (s64)INT32_MAX = 0x000000007FFFFFFF
	// Pair 1: HI[2]:LO[2] = 0xFFFFFFFE:0xFFFFFFFF = 0xFFFFFFFEFFFFFFFF (= -4294967297 < INT_MIN)
	//   Rd.UD[1] = (s64)INT32_MIN = 0xFFFFFFFF80000000
	h.SetLoPair(0x0000000000000000ull, 0x00000000FFFFFFFFull);
	h.SetHiPair(0x0000000000000001ull, 0x00000000FFFFFFFEull);
	h.LoadProgram({ee::PMFHL(reg::v0, 0x02)});
	h.Run();
	h.ExpectMmiPair(reg::v0, 0x000000007FFFFFFFull, 0xFFFFFFFF80000000ull);
}

TEST(EeRecMmi, PmfhlLhInterleavesEvenHalfwordsOfLoAndHi)
{
	EeRecTestHarness h;
	// LO.US = {0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x8888}
	h.SetLoPair(0x4444333322221111ull, 0x8888777766665555ull);
	// HI.US = {0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD, 0xEEEE, 0xFFFF, 0x9999, 0x1010}
	h.SetHiPair(0xDDDDCCCCBBBBAAAAull, 0x10109999FFFFEEEEull);
	h.LoadProgram({ee::PMFHL(reg::v0, 0x03)});
	h.Run();
	// Rd.US = {LO[0]=0x1111, LO[2]=0x3333, HI[0]=0xAAAA, HI[2]=0xCCCC,
	//          LO[4]=0x5555, LO[6]=0x7777, HI[4]=0xEEEE, HI[6]=0x9999}
	h.ExpectMmiPair(reg::v0, 0xCCCCAAAA33331111ull, 0x9999EEEE77775555ull);
}

TEST(EeRecMmi, PmfhlShSignedSaturates32To16AndInterleaves)
{
	EeRecTestHarness h;
	// LO.UL = {0x00001234, 0x12345678, 0x80000000, 0xFFFFFFFE}
	//   PMFHL_CLAMP → {0x1234, 0x7FFF, 0x8000, 0xFFFE}
	h.SetLoPair(0x1234567800001234ull, 0xFFFFFFFE80000000ull);
	// HI.UL = {0xFFFF1000, 0x00007FFF, 0xFFFF8000, 0x00000ABC}
	//   sign-interpreted: {-61440, 32767, -32768, 2748}
	//   PMFHL_CLAMP →     {0x8000, 0x7FFF, 0x8000, 0x0ABC}
	h.SetHiPair(0x00007FFFFFFF1000ull, 0x00000ABCFFFF8000ull);
	h.LoadProgram({ee::PMFHL(reg::v0, 0x04)});
	h.Run();
	// Rd.US = {sat(LO[0]), sat(LO[1]), sat(HI[0]), sat(HI[1]),
	//          sat(LO[2]), sat(LO[3]), sat(HI[2]), sat(HI[3])}
	//       = {0x1234, 0x7FFF, 0x8000, 0x7FFF, 0x8000, 0xFFFE, 0x8000, 0x0ABC}
	h.ExpectMmiPair(reg::v0, 0x7FFF80007FFF1234ull, 0x0ABC8000FFFE8000ull);
}

// ============================================================================
// PMADDUW — 2-lane unsigned 32x32+64 multiply-accumulate
//   tempu[k] = (LO.UL[2k] | HI.UL[2k]<<32) + Rs.UL[2k]*Rt.UL[2k]   (u64)
//   LO.UD[k] = sign-ext s32 of tempu[k] low32
//   HI.UD[k] = sign-ext s32 of tempu[k] high32
//   Rd.UD[k] = tempu[k]
// Run() asserts LO/HI also match between JIT and interp; ExpectMmiPair
// covers Rd explicitly.
// ============================================================================

TEST(EeRecMmi, PmadduwAccumulatesAcrossBothLanesNoOverflow)
{
	EeRecTestHarness h;
	// Rs.UL = {2, _, 3, _}; Rt.UL = {10, _, 20, _}
	h.SetMmiPair(reg::a0, 0x0000000000000002ull, 0x0000000000000003ull);
	h.SetMmiPair(reg::a1, 0x000000000000000Aull, 0x0000000000000014ull);
	// LO.UL[0]=100=0x64,   LO.UL[2]=200=0xC8
	h.SetLoPair(0x0000000000000064ull, 0x00000000000000C8ull);
	// HI.UL[0]=1,          HI.UL[2]=2
	h.SetHiPair(0x0000000000000001ull, 0x0000000000000002ull);
	h.LoadProgram({ee::PMADDUW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// tempu[0] = 0x100000064 + 20  = 0x100000078
	// tempu[1] = 0x2000000C8 + 60  = 0x200000104
	h.ExpectMmiPair(reg::v0, 0x0000000100000078ull, 0x0000000200000104ull);
}

TEST(EeRecMmi, PmadduwOverflowsProductIntoHighHalf)
{
	EeRecTestHarness h;
	// Rs.UL = {0xFFFFFFFF, _, 0x00010000, _}
	h.SetMmiPair(reg::a0, 0x00000000FFFFFFFFull, 0x0000000000010000ull);
	// Rt.UL = {0x00000002, _, 0x00020000, _}
	h.SetMmiPair(reg::a1, 0x0000000000000002ull, 0x0000000000020000ull);
	h.SetLo64(0);
	h.SetHi64(0);
	h.LoadProgram({ee::PMADDUW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// product[0] = 0xFFFFFFFF*2 = 0x1FFFFFFFE
	// product[1] = 0x10000*0x20000 = 0x200000000
	h.ExpectMmiPair(reg::v0, 0x00000001FFFFFFFEull, 0x0000000200000000ull);
}

TEST(EeRecMmi, PmadduwCarriesAcrossWord32Boundary)
{
	EeRecTestHarness h;
	// Lane 0: composed = 0xAB_FFFFFFFF, product = 2 → sum = 0xAC_00000001
	//         (validates that the 64-bit accumulate carries across bit 32, i.e.
	//          the codegen really does a 64-bit-lane add rather than two
	//          independent 32-bit adds.)
	h.SetMmiPair(reg::a0, 0x0000000000000002ull, 0x0000000000000005ull);
	h.SetMmiPair(reg::a1, 0x0000000000000001ull, 0x0000000000000007ull);
	// LO.UL[0]=0xFFFFFFFF, LO.UL[2]=10
	h.SetLoPair(0x00000000FFFFFFFFull, 0x000000000000000Aull);
	// HI.UL[0]=0xAB,       HI.UL[2]=0
	h.SetHiPair(0x00000000000000ABull, 0x0000000000000000ull);
	h.LoadProgram({ee::PMADDUW(reg::v0, reg::a0, reg::a1)});
	h.Run();
	// tempu[0] = 0xAB_FFFFFFFF + 2          = 0xAC_00000001
	// tempu[1] = 0x00_0000000A + 5*7 = 0x2D = 0x00000000_0000002D
	h.ExpectMmiPair(reg::v0, 0x000000AC00000001ull, 0x000000000000002Dull);
}
