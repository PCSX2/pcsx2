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

#include "Config.h"

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

// S1b pinned-dest fast paths: the scalar templates compute their FINAL
// result directly into the pin (Add/Sxtw/Cset/Csel/Ldr with an x22/x23/x29
// dest) and the write-through then emits just the canonical STR. Exercises
// every emitter shape S1b rewrote with a pinned destination: 3-op ALU whose
// dest AND both sources are pins, Cset (SLTU), MTLO from a pinned source
// (plain STR of the mirror), MFLO into a pinned dest (Ldr straight into the
// pin), and MOVN/MOVZ's Csel with a pinned rd (the current-D "load" is a
// vixl-elided self-Mov). Each step reads the previous pin state, so a
// stranded mirror cascades into every subsequent EXPECT.
TEST(EeRecPinnedGpr, PinnedDestFastPathShapes)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::sp, 0x0000000000010000ull);
	h.SetGpr64(reg::t0, 0x0000000000000123ull);
	h.LoadProgram({
		OR(reg::t6, reg::ra, reg::zero),    // save parking $ra
		OR(reg::ra, reg::t0, reg::zero),    // $ra = 0x123 (pinned write)
		DADDU(reg::v0, reg::sp, reg::ra),   // pin dest <- pin + pin
		DADDU(reg::t1, reg::v0, reg::zero), // = 0x10123
		SLTU(reg::v0, reg::ra, reg::sp),    // Cset into pinned $v0 (0x123 < 0x10000)
		DADDU(reg::t2, reg::v0, reg::zero), // = 1
		MTLO(reg::ra),                      // LO = 0x123, STR of the $ra mirror
		MFLO(reg::v0),                      // Ldr LO straight into the $v0 pin
		DADDU(reg::t3, reg::v0, reg::zero), // = 0x123
		MOVN(reg::v0, reg::sp, reg::t0),    // t0 != 0 -> $v0 = $sp (Csel, pinned rd)
		DADDU(reg::t4, reg::v0, reg::zero), // = 0x10000
		MOVZ(reg::v0, reg::ra, reg::zero),  // rt == $zero -> unconditional $v0 = $ra
		DADDU(reg::t5, reg::v0, reg::zero), // = 0x123
		OR(reg::ra, reg::t6, reg::zero),    // restore parking $ra
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t1), 0x0000000000010123ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t2), 1ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t3), 0x0000000000000123ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t4), 0x0000000000010000ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t5), 0x0000000000000123ull);
}

// S1b aliasing corners: a pinned register as BOTH dest and source of the
// same op (the armEEDestForGPR contract's "final instruction reads its
// sources in the same instruction" clause), plus the vixl LogicalMacro
// temp paths for unencodable logical immediates — with rd != rn vixl may
// borrow the pinned rd itself to materialize the immediate (Mov pin, imm;
// And pin, rn, pin), and with rd == rn (both the same pin) it must fall
// back to x16/x17 since rn is excluded from the temp pool. 0x12345678 and
// 0x5A5A1234 are not valid AArch64 logical immediates, so both AND forms
// take the materialization path; the consts arrive via LUI+ORI const-prop.
TEST(EeRecPinnedGpr, PinnedDestAliasesPinnedSource)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::v0, 0x0000000000010000ull);
	h.SetGpr64(reg::sp, 0xFFFFFFFFFFFFFFFFull);
	h.LoadProgram({
		ADDIU(reg::v0, reg::v0, 0x111),     // $v0 = 0x10111 (RMW, Sxtw into pin)
		SLTU(reg::v0, reg::zero, reg::v0),  // $v0 = 1 (dest aliases source via Cset)
		DADDU(reg::t1, reg::v0, reg::zero),
		LUI(reg::t2, 0x1234),
		ORI(reg::t2, reg::t2, 0x5678),      // t2 = const 0x12345678 (unencodable)
		AND(reg::v0, reg::sp, reg::t2),     // rd-as-imm-temp: Mov x29,imm; And x29,x22,x29
		DADDU(reg::t3, reg::v0, reg::zero), // = 0x12345678
		LUI(reg::t4, 0x5A5A),
		ORI(reg::t4, reg::t4, 0x1234),      // t4 = const 0x5A5A1234 (unencodable)
		AND(reg::v0, reg::v0, reg::t4),     // rd==rn pin: imm goes via x16/x17
		DADDU(reg::t5, reg::v0, reg::zero), // = 0x12101230
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t1), 1ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t3), 0x0000000012345678ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t5), 0x0000000012101230ull);
}

// ---------------------------------------------------------------------------
// Rung 2 ($v1 → x12, $a0 → x13, kEEPinTable): the first CALLER-SAVED pins.
// Beyond the S1 coverage matrix, the rung-2-specific risk is a C call
// emitted mid-block clobbering the host registers even though the callee
// never touches guest state. Preservation is by preserve_most on the vtlb
// dispatchers (warm paths) and armReloadEEClobberedPins at every other
// emitted call site — see the REEPIN_* contract in iR5900-arm64.h.
// ---------------------------------------------------------------------------

// Core scalar matrix for both new pins: reads, 3-op writes, RMW, and a
// pin-dest ← pin-op-pin shape crossing $v1/$a0 with the S1 pins.
TEST(EeRecPinnedGpr, V1A0ScalarReadsAndWriteThrough)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::v1, 0x0000000001F00010ull);
	h.SetGpr64(reg::a0, 0x0000000000010000ull);
	h.LoadProgram({
		ADDIU(reg::t1, reg::v1, -16),       // pinned $v1 read (imm ALU)
		DADDU(reg::t2, reg::v1, reg::a0),   // both new pins as sources
		DSLL(reg::t3, reg::a0, 4),          // pinned $a0 read (shift)
		DADDU(reg::v1, reg::a0, reg::t2),   // write pinned $v1 from pinned $a0
		OR(reg::t4, reg::v1, reg::zero),    // read the write back via the pin
		ADDIU(reg::a0, reg::a0, 0x100),     // pinned $a0 RMW
		DADDU(reg::a0, reg::a0, reg::v1),   // $a0 ← $a0 + $v1 (all pins)
		DADDU(reg::t5, reg::a0, reg::zero), // read the result back
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t1), 0x0000000001F00000ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t2), 0x0000000001F10010ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t3), 0x0000000000100000ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t4), 0x0000000001F20010ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t5), 0x0000000001F30110ull);
}

// 128-bit write-through, vtlb loads with a pinned base, and the CFC2
// half-word Bfi path — the remaining write-path shapes, on the new pins.
TEST(EeRecPinnedGpr, V1A0QuadLoadAndHalfWordWriteThrough)
{
	EeRecTestHarness h;
	h.EnableVu0Capture();
	h.SeedVu0Vi(1, 0x8123);
	h.WriteU64(kScratch, 0xFFFFFFFF80332211ull);
	h.WriteU32(kScratch + 8, 0x00445566u);
	h.SetGpr64(reg::t0, 0x1111111122222222ull);
	h.SetGpr64(reg::t1, 0x0000000300000004ull);
	h.SetGpr64(reg::v1, kScratch);
	h.LoadProgram({
		LD(reg::a0, 0, reg::v1),            // pinned base, pinned dest
		DADDU(reg::t2, reg::a0, reg::zero),
		LW(reg::v1, 8, reg::v1),            // 32-bit load, pinned base == dest
		DADDU(reg::t3, reg::v1, reg::zero),
		PADDW(reg::a0, reg::t0, reg::t1),   // 128-bit write of pinned $a0
		DADDU(reg::t4, reg::a0, reg::zero), // lane-0 read-back via the pin
		CFC2(reg::v1, 1),                   // UL[0]+UL[1] Bfi halves into $v1
		DADDU(reg::t5, reg::v1, reg::zero),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t2), 0xFFFFFFFF80332211ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t3), 0x0000000000445566ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t4), 0x1111111422222226ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t5), 0x0000000000008123ull);
}

// Interp-fallback write-back into a caller-saved pin, with the OTHER
// caller-saved pin holding a live sentinel ACROSS the C call: Interp::MFC0
// both writes GPR[rt] in memory AND clobbers x12/x13 per AAPCS — the full
// armReloadEEGPRPins after it must restore both pins.
TEST(EeRecPinnedGpr, V1A0Mfc0PerfCounterAcrossInterpFallback)
{
	EeRecTestHarness h;
	h.EnableCop0();
	h.SetGpr64(reg::t5, 0x0000000000C0FFEEull); // PCCR seed (CTE clear)
	h.SetGpr64(reg::v1, 0x0000000001F00010ull); // stale-mirror sentinel
	h.SetGpr64(reg::a0, 0x7EDCBA9876543210ull); // live across the call
	h.LoadProgram({
		MTC0(reg::t5, 25),                  // MTPS: PERF.pccr.val = t5
		MFC0(reg::v1, 25),                  // MFPS via interp fallback
		DADDU(reg::t0, reg::v1, reg::zero), // read $v1 back via the pin
		DADDU(reg::t1, reg::a0, reg::zero), // $a0 must have survived the call
	});
	h.Run();
	h.ExpectGpr64(reg::t0, 0x0000000000C0FFEEull);
	h.ExpectGpr64(reg::t1, 0x7EDCBA9876543210ull);
}

// ---------------------------------------------------------------------------
// Rung 3 ($k0 → x4, $a1 → x5, $s0 → x6, $at → x7): four more caller-saved
// pins, in argument-register territory — preserve_most never spares x0-x8,
// so the vtlb softmem/thunk slow paths carry an explicit reload (the seam
// test below runs with ALL six caller-saved pins live).
// ---------------------------------------------------------------------------

// Core scalar matrix for the rung-3 pins: reads, writes, RMW, cross-pin ops.
TEST(EeRecPinnedGpr, RungThreeScalarReadsAndWriteThrough)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::k0, 0x0000000001F00010ull);
	h.SetGpr64(reg::a1, 0x0000000000010000ull);
	h.SetGpr64(reg::s0, 0x0000000000000123ull);
	h.LoadProgram({
		ADDIU(reg::t1, reg::k0, -16),       // pinned $k0 read
		DADDU(reg::at, reg::a1, reg::s0),   // pinned $at dest <- $a1 + $s0
		DADDU(reg::t2, reg::at, reg::zero), // read $at back via the pin
		ADDIU(reg::s0, reg::s0, 0x100),     // pinned $s0 RMW
		DADDU(reg::t3, reg::s0, reg::zero),
		DSLL(reg::t4, reg::a1, 4),          // pinned $a1 read (shift)
		DADDU(reg::k0, reg::at, reg::a1),   // pin <- pin + pin
		DADDU(reg::t5, reg::k0, reg::zero),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t1), 0x0000000001F00000ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t2), 0x0000000000010123ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t3), 0x0000000000000223ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t4), 0x0000000000100000ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t5), 0x0000000000020123ull);
}

// The warm-path seam: a vtlb SLOW-PATH C call (handler-page access) with
// both caller-saved pins live. Fastmem is disabled for this block so
// recLoad/recStore emit the inline softmem lookup whose slow path BLs
// vtlb_memRead/Write<mem32_t> — the preserve_most dispatchers. A non-const
// base defeats the const-paddr MMIO shortcut, so this is exactly the
// emitted-call shape the fastmem backpatch thunk also takes.
// Coverage honesty: this wires the seam end-to-end, but whether a BROKEN
// seam (annotation stripped) actually corrupts x12/x13 depends on the
// dispatcher's register allocation on the dynamic path taken — the INTC
// read path happens not to touch them in current clang builds, so this
// test alone cannot prove the annotation. That proof is object-level (the
// vtlb_memRead prologue saves x9-x15 iff preserve_most applied) plus the
// stepdiff/corpus gates, where real games hit fat MMIO paths constantly.
TEST(EeRecPinnedGpr, CallerSavedPinsSurviveVtlbSlowPath)
{
	EeRecTestHarness h;
	const bool old_fastmem = EmuConfig.Cpu.Recompiler.EnableFastmem;
	const bool old_intc = EmuConfig.Speedhacks.IntcStat;
	EmuConfig.Cpu.Recompiler.EnableFastmem = false; // force softmem emit
	EmuConfig.Speedhacks.IntcStat = false;          // plain pure INTC read handler
	h.SetGpr64(reg::t1, 0x1000f010ull);             // INTC_STAT vaddr (non-const)
	h.SetGpr64(reg::v1, 0x0123456789ABCDEFull);
	h.SetGpr64(reg::a0, 0x7EDCBA9876543210ull);
	// Rung-3 pins: preserve_most does NOT spare x4-x7, so these four ride
	// the explicit armReloadEEClobberedPins after the slow-path call.
	h.SetGpr64(reg::k0, 0x1111222233334444ull);
	h.SetGpr64(reg::a1, 0x5555666677778888ull);
	h.SetGpr64(reg::s0, 0x0102030405060708ull);
	h.SetGpr64(reg::at, 0x69ABCDEF01234567ull);
	h.LoadProgram({
		LW(reg::t0, 0, reg::t1),            // handler page → slow-path C read
		DADDU(reg::t2, reg::v1, reg::zero), // pins survived the read call?
		DADDU(reg::t3, reg::a0, reg::zero),
		DADDU(reg::t6, reg::k0, reg::zero),
		DADDU(reg::t7, reg::a1, reg::zero),
		SW(reg::zero, 0, reg::t1),          // slow-path C write (0 clears no bits)
		DADDU(reg::t4, reg::v1, reg::zero), // pins survived the write call?
		DADDU(reg::t5, reg::a0, reg::zero),
		DADDU(reg::t8, reg::s0, reg::zero),
		DADDU(reg::t9, reg::at, reg::zero),
	});
	h.Run();
	EmuConfig.Cpu.Recompiler.EnableFastmem = old_fastmem;
	EmuConfig.Speedhacks.IntcStat = old_intc;
	EXPECT_EQ(h.GetGpr64Interp(reg::t2), 0x0123456789ABCDEFull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t3), 0x7EDCBA9876543210ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t6), 0x1111222233334444ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t7), 0x5555666677778888ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t4), 0x0123456789ABCDEFull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t5), 0x7EDCBA9876543210ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t8), 0x0102030405060708ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t9), 0x69ABCDEF01234567ull);
}

// The cold-path seam: const-paddr MMIO shortcut — a CONST base resolving to
// a handler page emits a direct BL to the raw registered handler (plain
// AAPCS, not preserve_most) followed by armReloadEEClobberedPins. INTC_MASK
// keeps it side-effect-free: reads are pure and writing 0 toggles nothing.
TEST(EeRecPinnedGpr, CallerSavedPinsSurviveConstPaddrMMIOShortcut)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::v1, 0x0123456789ABCDEFull);
	h.SetGpr64(reg::a0, 0x7EDCBA9876543210ull);
	h.LoadProgram({
		LUI(reg::t1, 0x1000),
		ORI(reg::t1, reg::t1, 0xf020),      // t1 = const 0x1000f020 (INTC_MASK)
		LW(reg::t0, 0, reg::t1),            // shortcut: BL hwRead32 + pin reload
		DADDU(reg::t2, reg::v1, reg::zero),
		DADDU(reg::t3, reg::a0, reg::zero),
		SW(reg::zero, 0, reg::t1),          // shortcut: BL hwWrite32 + pin reload
		DADDU(reg::t4, reg::v1, reg::zero),
		DADDU(reg::t5, reg::a0, reg::zero),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::t2), 0x0123456789ABCDEFull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t3), 0x7EDCBA9876543210ull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t4), 0x0123456789ABCDEFull);
	EXPECT_EQ(h.GetGpr64Interp(reg::t5), 0x7EDCBA9876543210ull);
}
