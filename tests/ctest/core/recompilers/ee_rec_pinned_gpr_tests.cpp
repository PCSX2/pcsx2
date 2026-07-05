// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Write-through pinned read-cache coverage ($sp → x22, $ra → x23; see
// REEPIN_* in arm64/iR5900-arm64.h). The pins mirror GPR.r[29/31].UD[0]
// while memory stays canonical, so a broken write-through path does NOT
// corrupt memory — it strands the mirror. These tests make that observable
// by writing a pinned reg through each write path and then READING IT BACK
// inside the same block: a stale mirror feeds the read-back a wrong value
// and the JIT-vs-interp diff goes red.
//
// The harness parks the machine via a trailing JR $ra (SeedEntryState sets
// $ra = kParkingPc), so every block that scribbles $ra saves it to a temp
// first and restores it last — the save/restore pair itself exercises
// pinned reads and writes.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;
using namespace mips::ee;

namespace {
constexpr u32 kScratch = RecompilerTestEnvironment::kScratchAddr;
}

// Pinned $sp/$ra as scalar sources: imm-ALU, 3-op ALU, shift, set-on-lt.
TEST(EeRecPinnedGpr, ScalarReadsOfPinnedRegs)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::sp, 0x0000000001F00010ull);
	h.SetGpr64(reg::t5, 0xFFFFFFFF80001234ull);
	h.LoadProgram({
		OR(reg::t6, reg::ra, reg::zero), // save parking $ra (pinned read)
		OR(reg::ra, reg::t5, reg::zero), // write pinned $ra
		ADDIU(reg::t0, reg::sp, -16),
		DADDU(reg::t1, reg::sp, reg::ra),
		SLTI(reg::t2, reg::ra, 0),
		DSLL(reg::t3, reg::ra, 4),
		OR(reg::t4, reg::sp, reg::ra),
		OR(reg::ra, reg::t6, reg::zero), // restore parking $ra
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), 0x0000000001F00000ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t1), 0xFFFFFFFF81F01244ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t2), 1ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t3), 0xFFFFFFF800012340ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t4), 0xFFFFFFFF81F01234ull);
}

// Scalar write-through: writes to $sp/$ra go through the memStore helpers'
// armStoreEERegPtr, which must refresh the mirror the read-backs consume.
TEST(EeRecPinnedGpr, ScalarWriteThroughThenReadBack)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0x0000000000010000ull);
	h.SetGpr64(reg::t1, 0x0000000000000230ull);
	h.LoadProgram({
		OR(reg::t6, reg::ra, reg::zero),  // save parking $ra
		DADDU(reg::sp, reg::t0, reg::t1), // write pinned $sp
		ADDIU(reg::t2, reg::sp, 8),       // read $sp back via the pin
		DADDU(reg::ra, reg::t2, reg::t0), // write pinned $ra
		DADDU(reg::t3, reg::ra, reg::t1), // read $ra back via the pin
		ADDIU(reg::sp, reg::sp, -32),     // pinned RMW: read + write $sp
		OR(reg::t4, reg::sp, reg::zero),  // read the RMW result back
		OR(reg::ra, reg::t6, reg::zero),  // restore parking $ra
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t2), 0x0000000000010238ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t3), 0x0000000000020468ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t4), 0x0000000000010210ull);
}

// 128-bit write-through: PADDW allocates $sp as a NEON dest; the scalar
// read-back forces the NEON→memory writeback (armStoreEEGPRQuad), whose
// lane-0 UMOV must refresh the mirror.
TEST(EeRecPinnedGpr, MmiQuadWriteThroughThenReadBack)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0x1111111122222222ull);
	h.SetGpr64(reg::t1, 0x0000000300000004ull);
	h.LoadProgram({
		PADDW(reg::sp, reg::t0, reg::t1),   // 128-bit write of pinned $sp
		DADDU(reg::t2, reg::sp, reg::zero), // scalar read-back via the pin
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t2), 0x1111111422222226ull);
}

// vtlb-load write-through: LW/LD land in the guest reg via
// recStoreLoadResult's armStoreEERegPtr; the read-back consumes the mirror.
// (LD $ra, off($sp) is the ubiquitous epilogue stack-restore idiom.)
TEST(EeRecPinnedGpr, LoadIntoPinnedThenReadBack)
{
	EeRecTestHarness h;
	h.WriteU64(kScratch, 0xFFFFFFFF80332211ull);
	h.WriteU32(kScratch + 8, 0x00445566u);
	h.SetGpr64(reg::sp, kScratch);
	h.LoadProgram({
		OR(reg::t6, reg::ra, reg::zero), // save parking $ra
		LD(reg::ra, 0, reg::sp),         // pinned base, pinned dest
		DADDU(reg::t0, reg::ra, reg::zero),
		LW(reg::sp, 8, reg::sp),         // pinned base and dest, 32-bit
		DADDU(reg::t1, reg::sp, reg::zero),
		OR(reg::ra, reg::t6, reg::zero), // restore parking $ra
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), 0xFFFFFFFF80332211ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t1), 0x0000000000445566ull);
}

// Interp-fallback write-back: MFC0 rd=25 (performance counters) drops to
// iFlushCall(FLUSH_INTERPRETER) + Interp::MFC0, which writes cpuRegs.GPR[rt]
// in MEMORY behind the JIT's back. When rt is pinned, the mirror must be
// re-read after the call or the in-block read-back consumes the stale pin.
// (MTC0 rd=25 / MTPS seeds PCCR through the same fallback mechanism — it
// writes no GPRs so it needs no reload. The seed keeps CTE (pccr bit 31)
// clear, so COP0_UpdatePCCR is inert and the value is pass-deterministic.)
TEST(EeRecPinnedGpr, Mfc0PerfCounterIntoPinnedThenReadBack)
{
	EeRecTestHarness h;
	h.EnableCop0();
	h.SetGpr64(reg::t5, 0x0000000000C0FFEEull); // PCCR seed (CTE clear)
	h.SetGpr64(reg::sp, 0x0000000001F00010ull); // stale-mirror sentinel
	h.LoadProgram({
		OR(reg::t6, reg::ra, reg::zero),    // save parking $ra
		MTC0(reg::t5, 25),                  // MTPS: PERF.pccr.val = t5
		MFC0(reg::sp, 25),                  // MFPS: GPR[29] = (s32)pccr via interp fallback
		DADDU(reg::t0, reg::sp, reg::zero), // read $sp back via the pin
		MFC0(reg::ra, 25),                  // same hole into pinned $ra
		DADDU(reg::t1, reg::ra, reg::zero), // read $ra back via the pin
		OR(reg::ra, reg::t6, reg::zero),    // restore parking $ra
	});
	h.Run();
	h.ExpectGpr64(reg::t0, 0x0000000000C0FFEEull);
	h.ExpectGpr64(reg::t1, 0x0000000000C0FFEEull);
}

// 32-bit-half write-through (Bfi path): CFC2 writes UL[0] and UL[1] of the
// pinned reg separately; the read-back consumes the mirror.
TEST(EeRecPinnedGpr, HalfWordWriteThroughViaCfc2)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.SeedVu0Vi(1, 0x8123);
	h.LoadProgram({
		OR(reg::t6, reg::ra, reg::zero),    // save parking $ra
		CFC2(reg::ra, 1),                   // UL[0]+UL[1] stores into pinned $ra
		DADDU(reg::t0, reg::ra, reg::zero), // read $ra back via the pin
		OR(reg::ra, reg::t6, reg::zero),    // restore parking $ra
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), 0x0000000000008123ull);
}

// ---------------------------------------------------------------------------
// Rung 1 ($v0 → x29, kEEPinTable). Same coverage matrix as $sp/$ra above.
// $v0 is the hottest EE register in real games (20.3% of dynamic refs in the
// SotC SD865 capture, tools/perf/sotc-regheat-2026-07-05.md) and its host reg
// doubles as the AAPCS frame pointer — any C-reachable path that failed to
// preserve x29 (or an emitter that treated it as scratch) strands the mirror,
// which the in-block read-backs surface as a JIT-vs-interp diff.
// ---------------------------------------------------------------------------

// Pinned $v0 as scalar source and dest: reads, write, RMW, read-backs.
TEST(EeRecPinnedGpr, V0ScalarReadsAndWriteThrough)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::v0, 0x0000000001F00010ull);
	h.SetGpr64(reg::t0, 0x0000000000010000ull);
	h.LoadProgram({
		ADDIU(reg::t1, reg::v0, -16),       // pinned read (32-bit imm ALU)
		DADDU(reg::t2, reg::v0, reg::t0),   // pinned read (64-bit 3-op)
		DSLL(reg::t3, reg::v0, 4),          // pinned read (shift)
		DADDU(reg::v0, reg::t0, reg::t2),   // write pinned $v0
		OR(reg::t4, reg::v0, reg::zero),    // read the write back via the pin
		ADDIU(reg::v0, reg::v0, 0x100),     // pinned RMW: read + write $v0
		DADDU(reg::t5, reg::v0, reg::zero), // read the RMW result back
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t1), 0x0000000001F00000ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t2), 0x0000000001F10010ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t3), 0x000000001F000100ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t4), 0x0000000001F20010ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t5), 0x0000000001F20110ull);
}

// 128-bit write-through into $v0 (armStoreEEGPRQuad lane-0 refresh).
TEST(EeRecPinnedGpr, V0MmiQuadWriteThroughThenReadBack)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, 0x1111111122222222ull);
	h.SetGpr64(reg::t1, 0x0000000300000004ull);
	h.LoadProgram({
		PADDW(reg::v0, reg::t0, reg::t1),   // 128-bit write of pinned $v0
		DADDU(reg::t2, reg::v0, reg::zero), // scalar read-back via the pin
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t2), 0x1111111422222226ull);
}

// vtlb-load write-through into $v0 (LD 64-bit and LW 32-bit sign-extending),
// with the pinned $sp as base — one op exercising two pins at once.
TEST(EeRecPinnedGpr, V0LoadIntoPinnedThenReadBack)
{
	EeRecTestHarness h;
	h.WriteU64(kScratch, 0xFFFFFFFF80332211ull);
	h.WriteU32(kScratch + 8, 0x00445566u);
	h.SetGpr64(reg::sp, kScratch);
	h.LoadProgram({
		LD(reg::v0, 0, reg::sp),            // pinned base, pinned dest
		DADDU(reg::t0, reg::v0, reg::zero),
		LW(reg::v0, 8, reg::sp),            // 32-bit load into pinned dest
		DADDU(reg::t1, reg::v0, reg::zero),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), 0xFFFFFFFF80332211ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t1), 0x0000000000445566ull);
}

// Interp-fallback write-back into $v0 (the MFC0 rd=25 reload-if-pinned path
// must serve every kEEPinTable entry, not just the original $sp/$ra pair).
TEST(EeRecPinnedGpr, V0Mfc0PerfCounterIntoPinnedThenReadBack)
{
	EeRecTestHarness h;
	h.EnableCop0();
	h.SetGpr64(reg::t5, 0x0000000000C0FFEEull); // PCCR seed (CTE clear)
	h.SetGpr64(reg::v0, 0x0000000001F00010ull); // stale-mirror sentinel
	h.LoadProgram({
		MTC0(reg::t5, 25),                  // MTPS: PERF.pccr.val = t5
		MFC0(reg::v0, 25),                  // MFPS via interp fallback
		DADDU(reg::t0, reg::v0, reg::zero), // read $v0 back via the pin
	});
	h.Run();
	h.ExpectGpr64(reg::t0, 0x0000000000C0FFEEull);
}

// 32-bit-half write-through into $v0 (CFC2 Bfi path).
TEST(EeRecPinnedGpr, V0HalfWordWriteThroughViaCfc2)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.SeedVu0Vi(1, 0x8123);
	h.LoadProgram({
		CFC2(reg::v0, 1),                   // UL[0]+UL[1] stores into pinned $v0
		DADDU(reg::t0, reg::v0, reg::zero), // read $v0 back via the pin
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t0), 0x0000000000008123ull);
}
