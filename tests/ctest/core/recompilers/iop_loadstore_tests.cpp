// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "harness/JitTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kDataAddr = RecompilerTestEnvironment::kScratchAddr;
}

TEST(IopLoadStore, LwBasic)
{
	JitTestHarness h;
	h.WriteU32(kDataAddr, 0x12345678u);
	h.SetGpr(reg::a0, kDataAddr);
	h.LoadProgram({LW(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x12345678u);
}

TEST(IopLoadStore, LwPositiveOffset)
{
	JitTestHarness h;
	h.WriteU32(kDataAddr + 8, 0xDEADBEEFu);
	h.SetGpr(reg::a0, kDataAddr);
	h.LoadProgram({LW(reg::v0, 8, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xDEADBEEFu);
}

TEST(IopLoadStore, LwNegativeOffset)
{
	JitTestHarness h;
	h.WriteU32(kDataAddr, 0xCAFEF00Du);
	h.SetGpr(reg::a0, kDataAddr + 16);
	h.LoadProgram({LW(reg::v0, -16, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xCAFEF00Du);
}

TEST(IopLoadStore, LbSignExtendsNegative)
{
	JitTestHarness h;
	h.WriteU8(kDataAddr, 0xFFu);
	h.SetGpr(reg::a0, kDataAddr);
	h.LoadProgram({LB(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xFFFFFFFFu);
}

TEST(IopLoadStore, LbuZeroExtends)
{
	JitTestHarness h;
	h.WriteU8(kDataAddr, 0xFFu);
	h.SetGpr(reg::a0, kDataAddr);
	h.LoadProgram({LBU(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x000000FFu);
}

TEST(IopLoadStore, LhSignExtends)
{
	JitTestHarness h;
	h.WriteU16(kDataAddr, 0x8001u);
	h.SetGpr(reg::a0, kDataAddr);
	h.LoadProgram({LH(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xFFFF8001u);
}

TEST(IopLoadStore, LhuZeroExtends)
{
	JitTestHarness h;
	h.WriteU16(kDataAddr, 0x8001u);
	h.SetGpr(reg::a0, kDataAddr);
	h.LoadProgram({LHU(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x00008001u);
}

TEST(IopLoadStore, SwBasic)
{
	JitTestHarness h;
	h.TrackMemWindow(kDataAddr, 4);
	h.SetGpr(reg::a0, kDataAddr);
	h.SetGpr(reg::a1, 0xFEEDFACEu);
	h.LoadProgram({SW(reg::a1, 0, reg::a0)});
	h.Run();
	// The diff between pre and post mem state is what asserts correctness;
	// the stored value can also be read back directly.
	EXPECT_EQ(h.ReadU32(kDataAddr), 0xFEEDFACEu);
}

TEST(IopLoadStore, SbLowByteOnly)
{
	JitTestHarness h;
	h.WriteU32(kDataAddr, 0xAABBCCDDu);
	h.TrackMemWindow(kDataAddr, 4);
	h.SetGpr(reg::a0, kDataAddr);
	h.SetGpr(reg::a1, 0x55u);
	h.LoadProgram({SB(reg::a1, 0, reg::a0)});
	h.Run();
	// Stored only the low byte; upper three bytes are unaffected.
	EXPECT_EQ(h.ReadU32(kDataAddr), 0xAABBCC55u);
}

TEST(IopLoadStore, ShLowHalf)
{
	JitTestHarness h;
	h.WriteU32(kDataAddr, 0xAABBCCDDu);
	h.TrackMemWindow(kDataAddr, 4);
	h.SetGpr(reg::a0, kDataAddr);
	h.SetGpr(reg::a1, 0x1234u);
	h.LoadProgram({SH(reg::a1, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.ReadU32(kDataAddr), 0xAABB1234u);
}

TEST(IopLoadStore, StoreThenLoadRoundtrip)
{
	JitTestHarness h;
	h.TrackMemWindow(kDataAddr, 4);
	h.SetGpr(reg::a0, kDataAddr);
	h.SetGpr(reg::a1, 0x01020304u);
	h.LoadProgram({
		SW(reg::a1, 0, reg::a0),
		LW(reg::v0, 0, reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x01020304u);
}

// ===========================================================================
//  IOP unaligned word loads/stores — LWL / LWR / SWL / SWR
//
//  Same LE merge semantics as the EE 32-bit ops (see
//  R3000AOpcodeTables.cpp:214-288). IOP recompiler uses REC_FUNC fallback
//  to the interpreter for these — tests lock in the architectural result
//  and the JIT call-sequence around the fallback.
// ===========================================================================

TEST(IopLoadStore, LwlShift0PartialLoad)
{
	JitTestHarness h;
	h.WriteU32(kDataAddr, 0x44332211u);
	h.SetGpr(reg::a0, kDataAddr);
	h.SetGpr(reg::v0, 0xCAFEBABEu);
	h.LoadProgram({LWL(reg::v0, 0, reg::a0)});
	h.Run();
	// (rt & 0x00FFFFFF) | (mem << 24) = 0x00FEBABE | 0x11000000
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x11FEBABEu);
}

TEST(IopLoadStore, LwlShift3FullWord)
{
	JitTestHarness h;
	h.WriteU32(kDataAddr, 0xFFEEDDCCu);
	h.SetGpr(reg::a0, kDataAddr);
	h.SetGpr(reg::v0, 0xCAFEBABEu);
	h.LoadProgram({LWL(reg::v0, 3, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xFFEEDDCCu);
}

TEST(IopLoadStore, LwrShift0FullWord)
{
	JitTestHarness h;
	h.WriteU32(kDataAddr, 0x44332211u);
	h.SetGpr(reg::a0, kDataAddr);
	h.SetGpr(reg::v0, 0xCAFEBABEu);
	h.LoadProgram({LWR(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x44332211u);
}

TEST(IopLoadStore, LwrShift3SingleByte)
{
	JitTestHarness h;
	h.WriteU32(kDataAddr, 0x44332211u);
	h.SetGpr(reg::a0, kDataAddr);
	h.SetGpr(reg::v0, 0xCAFEBABEu);
	h.LoadProgram({LWR(reg::v0, 3, reg::a0)});
	h.Run();
	// (rt & 0xFFFFFF00) | (mem >> 24) = 0xCAFEBA00 | 0x44
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xCAFEBA44u);
}

TEST(IopLoadStore, LwrLwlPairUnalignedWordLoad)
{
	// Bytes [0..7] = 99 11 22 33 44 AA BB CC. Unaligned word at byte 1
	// = LE 0x44332211. Canonical IOP unaligned-load pair.
	JitTestHarness h;
	h.WriteU32(kDataAddr + 0, 0x33221199u);
	h.WriteU32(kDataAddr + 4, 0xCCBBAA44u);
	h.SetGpr(reg::a0, kDataAddr);
	h.SetGpr(reg::v0, 0xCAFEBABEu);
	h.LoadProgram({
		LWR(reg::v0, 1, reg::a0),
		LWL(reg::v0, 4, reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x44332211u);
}

TEST(IopLoadStore, SwlShift0SingleByte)
{
	JitTestHarness h;
	h.WriteU32(kDataAddr, 0xAABBCCDDu);
	h.SetGpr(reg::a0, kDataAddr);
	h.SetGpr(reg::a1, 0x12345678u);
	h.TrackMemWindow(kDataAddr, 4);
	h.LoadProgram({SWL(reg::a1, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.ReadU32(kDataAddr), 0xAABBCC12u);
}

TEST(IopLoadStore, SwrShift3SingleByte)
{
	JitTestHarness h;
	h.WriteU32(kDataAddr, 0xAABBCCDDu);
	h.SetGpr(reg::a0, kDataAddr);
	h.SetGpr(reg::a1, 0x12345678u);
	h.TrackMemWindow(kDataAddr, 4);
	h.LoadProgram({SWR(reg::a1, 3, reg::a0)});
	h.Run();
	EXPECT_EQ(h.ReadU32(kDataAddr), 0x78BBCCDDu);
}

TEST(IopLoadStore, SwrSwlPairUnalignedWordStore)
{
	JitTestHarness h;
	h.WriteU32(kDataAddr + 0, 0xAAAAAAAAu);
	h.WriteU32(kDataAddr + 4, 0xBBBBBBBBu);
	h.SetGpr(reg::a0, kDataAddr);
	h.SetGpr(reg::a1, 0x12345678u);
	h.TrackMemWindow(kDataAddr, 8);
	h.LoadProgram({
		SWR(reg::a1, 1, reg::a0),
		SWL(reg::a1, 4, reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.ReadU32(kDataAddr + 0), 0x345678AAu);
	EXPECT_EQ(h.ReadU32(kDataAddr + 4), 0xBBBBBB12u);
}
