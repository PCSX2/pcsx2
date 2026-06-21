// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Coverage expansion for the IOP load/store JIT path. Basic width /
// sign-extension coverage lives in iop_loadstore_tests.cpp; this file picks up
// what that file doesn't:
//
//   * RAM mirror aliasing (physical 0x00000000 / kseg0 0x80000000 / kseg1
//     0xa0000000 all land in the same 2MB iopMem->Main through the fast
//     path's 21-bit mask).
//   * Helper-path dispatch — any effective address with bit 28 set bypasses
//     the fast path and calls iopMemRead*/iopMemWrite* directly.
//   * Unaligned load/store (LWL/LWR/SWL/SWR) via the REC_FUNC interpreter
//     fallback — a different JIT code path from aligned loads/stores.
//   * `lw $0, ...` short-circuit (the _Rt_==0 branch in rpsxLW).

#include "harness/JitTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kPhysBase = RecompilerTestEnvironment::kScratchAddr;  // 0x00020000
constexpr u32 kKseg0Base = 0x80000000u | kPhysBase;                 // 0x80020000
constexpr u32 kKseg1Base = 0xA0000000u | kPhysBase;                 // 0xA0020000

// A bit-28-set address that routes through the helper path. Chosen in the
// IOP hardware register window (0x1F801xxx); iopHw is zero-filled by the
// test env, so reads from unassigned registers return whatever the helper
// dispatches to. No values are asserted — the implicit JIT-vs-interp diff
// in Run() locks the two paths to the same result.
constexpr u32 kHwHelperAddr = 0x1F801078u;
} // namespace

// ---------------------------------------------------------------------------
// RAM mirror aliasing — physical / kseg0 / kseg1 all address the same byte.
// ---------------------------------------------------------------------------

TEST(IopMemoryAccess, LwThroughKseg0Mirror)
{
	JitTestHarness h;
	h.WriteU32(kPhysBase, 0xABCD1234u);
	h.SetGpr(reg::a0, kKseg0Base);
	h.LoadProgram({LW(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xABCD1234u);
}

TEST(IopMemoryAccess, LwThroughKseg1Mirror)
{
	JitTestHarness h;
	h.WriteU32(kPhysBase, 0xFEEDFACEu);
	h.SetGpr(reg::a0, kKseg1Base);
	h.LoadProgram({LW(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xFEEDFACEu);
}

TEST(IopMemoryAccess, SwViaKseg0ReadableViaPhysical)
{
	JitTestHarness h;
	h.TrackMemWindow(kPhysBase, 4);
	h.SetGpr(reg::a0, kKseg0Base);
	h.SetGpr(reg::a1, 0xDEADBEEFu);
	h.LoadProgram({SW(reg::a1, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.ReadU32(kPhysBase), 0xDEADBEEFu);
}

TEST(IopMemoryAccess, SwViaKseg1ReadableViaPhysical)
{
	JitTestHarness h;
	h.TrackMemWindow(kPhysBase, 4);
	h.SetGpr(reg::a0, kKseg1Base);
	h.SetGpr(reg::a1, 0x55AA55AAu);
	h.LoadProgram({SW(reg::a1, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.ReadU32(kPhysBase), 0x55AA55AAu);
}

TEST(IopMemoryAccess, LbuThroughKseg1Mirror)
{
	// Byte load through the uncached-mirror address.
	JitTestHarness h;
	h.WriteU8(kPhysBase, 0x42u);
	h.SetGpr(reg::a0, kKseg1Base);
	h.LoadProgram({LBU(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x42u);
}

TEST(IopMemoryAccess, LhuThroughKseg0Mirror)
{
	JitTestHarness h;
	h.WriteU16(kPhysBase, 0xBEEFu);
	h.SetGpr(reg::a0, kKseg0Base);
	h.LoadProgram({LHU(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xBEEFu);
}

TEST(IopMemoryAccess, MirroredWritesAliasSamePhysicalWord)
{
	// Sequence that bounces through all three mirrors.
	//   SW  $a1, 0($a0)   ; a0 = phys, a1 = 0x11111111 → phys[0] = 0x11111111
	//   LW  $v0, 0($a2)   ; a2 = kseg0, should see 0x11111111
	//   SW  $a1, 0($a3)   ; a3 = kseg1, overwrite with 0x22222222
	//   LW  $v1, 0($a0)   ; phys, should see 0x22222222
	JitTestHarness h;
	h.TrackMemWindow(kPhysBase, 4);
	h.SetGpr(reg::a0, kPhysBase);
	h.SetGpr(reg::a1, 0x11111111u);
	h.SetGpr(reg::a2, kKseg0Base);
	h.SetGpr(reg::a3, kKseg1Base);
	h.LoadProgram({
		SW(reg::a1, 0, reg::a0),
		LW(reg::v0, 0, reg::a2),
		ORI(reg::a1, reg::zero, 0x2222u),        // a1 = 0x00002222
		LUI(reg::t0, 0x2222),                    // t0 = 0x22220000
		ADDU(reg::a1, reg::a1, reg::t0),         // a1 = 0x22222222
		SW(reg::a1, 0, reg::a3),
		LW(reg::v1, 0, reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x11111111u);
	EXPECT_EQ(h.GetGprInterp(reg::v1), 0x22222222u);
	EXPECT_EQ(h.ReadU32(kPhysBase), 0x22222222u);
}

// ---------------------------------------------------------------------------
// Helper-path dispatch — bit 28 of the effective address routes to the
// iopMemRead*/iopMemWrite* C helpers instead of the RAM fast path.
// ---------------------------------------------------------------------------

TEST(IopMemoryAccess, LwViaHelperPathMatchesInterp)
{
	// Read from an IOP HW register window address. No concrete assertion —
	// the test relies on the harness's implicit JIT-vs-interp diff in Run() to lock
	// both paths to the same value. If the JIT's helper call emits a
	// different ABI or clobbers a reg the interp didn't, the diff surfaces.
	JitTestHarness h;
	h.SetGpr(reg::a0, kHwHelperAddr);
	h.LoadProgram({LW(reg::v0, 0, reg::a0)});
	h.Run();
	// Lock the observed value as spec (whatever interp returned).
	EXPECT_EQ(h.GetGprJit(reg::v0), h.GetGprInterp(reg::v0));
}

TEST(IopMemoryAccess, LbuViaHelperPathMatchesInterp)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, kHwHelperAddr);
	h.LoadProgram({LBU(reg::v0, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprJit(reg::v0), h.GetGprInterp(reg::v0));
}

// ---------------------------------------------------------------------------
// _Rt_==0 short-circuit in rpsxLoad. The fast path body early-exits
// without writing a result, but the helper path still runs so device
// side-effects fire.
// ---------------------------------------------------------------------------

TEST(IopMemoryAccess, LoadIntoZeroDoesNotDisturbR0)
{
	// r0 is hardwired zero and must stay that way even when named as the
	// destination of a load. Both JIT and interp uphold this; the diff
	// locks them together, and the direct check pins the architectural
	// guarantee.
	JitTestHarness h;
	h.WriteU32(kPhysBase, 0xAAAAAAAAu);
	h.SetGpr(reg::a0, kPhysBase);
	h.LoadProgram({LW(reg::zero, 0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::zero), 0u);
	EXPECT_EQ(h.GetGprJit(reg::zero), 0u);
}

// ---------------------------------------------------------------------------
// LWL/LWR/SWL/SWR — REC_FUNC interpreter fallback. These spill the opcode,
// flush live regs, and dispatch to the interpreter. Different JIT code path
// from aligned loads/stores; worth smoke-testing.
// ---------------------------------------------------------------------------

TEST(IopMemoryAccess, LwlLwrAssembleAlignedWord)
{
	// `lwl rt, 3(base); lwr rt, 0(base)` — the canonical unaligned-load
	// pattern, here used with naturally-aligned base = 0x20000. Expected
	// result: the word at 0x20000 (little-endian).
	JitTestHarness h;
	h.WriteU32(kPhysBase, 0x44332211u);
	h.SetGpr(reg::a0, kPhysBase);
	h.LoadProgram({
		LWL(reg::v0, 3, reg::a0),
		LWR(reg::v0, 0, reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x44332211u);
}

TEST(IopMemoryAccess, LwlLwrAssembleUnalignedWord)
{
	// Load 4 bytes starting at an unaligned boundary (offset 1). Seed two
	// adjacent words so the unaligned read crosses the boundary. Lock
	// whatever the interpreter produces as spec.
	JitTestHarness h;
	h.WriteU32(kPhysBase + 0, 0x44332211u);
	h.WriteU32(kPhysBase + 4, 0x88776655u);
	h.SetGpr(reg::a0, kPhysBase);
	h.LoadProgram({
		// lwl rt, 4(a0)  -> loads high bytes from word at 0x20004
		// lwr rt, 1(a0)  -> loads low bytes starting at 0x20001
		LWL(reg::v0, 4, reg::a0),
		LWR(reg::v0, 1, reg::a0),
	});
	h.Run();
	// Don't pin a concrete value; surface only if JIT diverges from interp.
	EXPECT_EQ(h.GetGprJit(reg::v0), h.GetGprInterp(reg::v0));
}

TEST(IopMemoryAccess, SwlSwrWriteAlignedWord)
{
	// Mirror of the LWL/LWR round-trip for stores.
	JitTestHarness h;
	h.WriteU32(kPhysBase, 0u);
	h.TrackMemWindow(kPhysBase, 4);
	h.SetGpr(reg::a0, kPhysBase);
	h.SetGpr(reg::a1, 0x44332211u);
	h.LoadProgram({
		SWL(reg::a1, 3, reg::a0),
		SWR(reg::a1, 0, reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.ReadU32(kPhysBase), 0x44332211u);
}

TEST(IopMemoryAccess, LwlAfterSwSeesFlushedData)
{
	// Sequence: pre-state word → SW overwrites it → LWL+LWR reads it back.
	// Exercises the flush between rec opcodes: the SW leaves its value in
	// memory (helper-dispatched), and the subsequent LWL — which falls back to
	// the interpreter after flushing all live guest registers to memory — must
	// see the updated word.
	JitTestHarness h;
	h.WriteU32(kPhysBase, 0xDEADDEADu);
	h.TrackMemWindow(kPhysBase, 4);
	h.SetGpr(reg::a0, kPhysBase);
	h.SetGpr(reg::a1, 0xCAFEF00Du);
	h.LoadProgram({
		SW(reg::a1, 0, reg::a0),
		LWL(reg::v0, 3, reg::a0),
		LWR(reg::v0, 0, reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xCAFEF00Du);
}

// ---------------------------------------------------------------------------
// Flush semantics under pressure — a helper-dispatched store must flush
// all live guest regs so the post-helper state is coherent. Existing ALU
// tests don't stress this because none follow an SW with a read-from-
// register sequence long enough to see a bad flush.
// ---------------------------------------------------------------------------

TEST(IopMemoryAccess, ManyLiveRegsSurviveStoreFlush)
{
	// Load several regs, issue an SW (which calls _psxFlushCall), then
	// consume those regs afterward. A botched flush would show as one of
	// the post-SW reads returning a stale value.
	JitTestHarness h;
	h.SetGpr(reg::a0, kPhysBase);
	h.SetGpr(reg::t0, 0x11111111u);
	h.SetGpr(reg::t1, 0x22222222u);
	h.SetGpr(reg::t2, 0x33333333u);
	h.SetGpr(reg::t3, 0x44444444u);
	h.LoadProgram({
		SW(reg::t0, 0, reg::a0),                // flush happens here
		ADDU(reg::v0, reg::t1, reg::t2),        // uses t1+t2 post-flush
		ADDU(reg::v1, reg::t2, reg::t3),        // uses t2+t3 post-flush
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x55555555u);
	EXPECT_EQ(h.GetGprInterp(reg::v1), 0x77777777u);
	EXPECT_EQ(h.ReadU32(kPhysBase), 0x11111111u);
}
