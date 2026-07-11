// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// EE call-ret shadow-stack ring (P2-2, EE_CALLRET_STACK). Guest JAL /
// JALR-rd31 tails push {return PC, host landing} frames and transfer via
// BL; guest JR-$ra tails pop, compare, and RET on hit or miss. These tests
// pin the ABI-visible correctness of both paths (roundtrips, nesting,
// clobbered $ra misses, ring wrap, sentinel misses) through the standard
// JIT-vs-interp diff. Ring-state assertions are gated on the compile flag
// so the -DEE_CALLRET_STACK=0 A/B baseline build still passes this file.

#include "harness/EeRecTestHarness.h"

#include "R5900.h"

#include <gtest/gtest.h>

// Mirror of the iR5900-arm64.h default; the real value arrives via the
// global compile flag on -DEE_CALLRET_STACK=0 builds.
#ifndef EE_CALLRET_STACK
#define EE_CALLRET_STACK 1
#endif

using namespace recompiler_tests;
using namespace mips;

namespace
{
constexpr u32 kProgramPc = RecompilerTestEnvironment::kProgramPc;
constexpr u32 kPark = RecompilerTestEnvironment::kParkingPc;

constexpr u32 kFuncA = kProgramPc + 0x100;
constexpr u32 kFuncB = kProgramPc + 0x200;

#if EE_CALLRET_STACK
constexpr u64 kRingBytes = 0x10000;
constexpr u64 kOffMask = kRingBytes - 16;

struct RingFrame
{
	u64 guest_ra;
	u64 landing;
};

RingFrame ReadFrame(u64 off)
{
	RingFrame f;
	std::memcpy(&f, reinterpret_cast<const void*>(_cpuRegistersPack.eeCallRetBase + (off & kOffMask)),
		sizeof(f));
	return f;
}
#endif
} // namespace

TEST(EeRecCallRet, JalSubroutineRoundtrip)
{
	// main: v0=1; JAL A; (delay nop); v1=9; j park.
	// A:    v0+=10; JR ra; nop.
	// The return is the call-ret hit path (frame RA == $ra).
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		ADDIU(reg::v0, reg::zero, 1),
		JAL(kFuncA),
		NOP,
		ADDIU(reg::v1, reg::zero, 9),
		J(kPark),
		NOP,
	});
	h.WriteU32(kFuncA + 0, ADDIU(reg::v0, reg::v0, 10));
	h.WriteU32(kFuncA + 4, JR(reg::ra));
	h.WriteU32(kFuncA + 8, NOP);
	h.Run();
	h.ExpectGpr64(reg::v0, 11ull);
	h.ExpectGpr64(reg::v1, 9ull);
}

TEST(EeRecCallRet, NestedCallsPopInOrder)
{
	// main JALs A, A saves $ra and JALs B, B returns, A restores and
	// returns. Two stacked frames pop LIFO; both returns are hit-path.
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		ADDIU(reg::v0, reg::zero, 1),
		JAL(kFuncA),
		NOP,
		ADDIU(reg::v1, reg::zero, 9),
		J(kPark),
		NOP,
	});
	h.WriteU32(kFuncA + 0x0, OR(reg::t3, reg::ra, reg::zero)); // save ra
	h.WriteU32(kFuncA + 0x4, JAL(kFuncB));
	h.WriteU32(kFuncA + 0x8, NOP);
	h.WriteU32(kFuncA + 0xc, OR(reg::ra, reg::t3, reg::zero)); // restore ra
	h.WriteU32(kFuncA + 0x10, JR(reg::ra));
	h.WriteU32(kFuncA + 0x14, ADDIU(reg::v0, reg::v0, 10)); // delay slot
	h.WriteU32(kFuncB + 0x0, JR(reg::ra));
	h.WriteU32(kFuncB + 0x4, ADDIU(reg::v0, reg::v0, 100)); // delay slot
	h.Run();
	h.ExpectGpr64(reg::v0, 111ull);
	h.ExpectGpr64(reg::v1, 9ull);
}

TEST(EeRecCallRet, ClobberedRaTakesMissPathCorrectly)
{
	// A overwrites $ra with the parking address before returning: the popped
	// frame's RA (main+12) no longer matches, so the JR-$ra tail takes the
	// miss path (RET into the dispatcher) and must still land at park —
	// main's post-call code must NOT run.
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		JAL(kFuncA),
		NOP,
		ADDIU(reg::v1, reg::zero, 7), // reached only if A "returned" here
		J(kPark),
		NOP,
	});
	h.WriteU32(kFuncA + 0x0, LUI(reg::ra, static_cast<u16>(kPark >> 16)));
	h.WriteU32(kFuncA + 0x4, ORI(reg::ra, reg::ra, static_cast<u16>(kPark & 0xFFFF)));
	h.WriteU32(kFuncA + 0x8, JR(reg::ra));
	h.WriteU32(kFuncA + 0xc, NOP);
	h.Run();
	h.ExpectGpr64(reg::v1, 0ull);
}

TEST(EeRecCallRet, JalrRd31CallReturnRoundtrip)
{
	// JALR ra, t0 — the indirect-call idiom. The call pushes a frame and
	// transfers via BL through the dispatcher; the return is hit-path.
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, kFuncA);
	h.LoadProgramNoTerm({
		JALR(reg::ra, reg::t0),
		NOP,
		ADDIU(reg::v1, reg::zero, 9),
		J(kPark),
		NOP,
	});
	h.WriteU32(kFuncA + 0, ADDIU(reg::v0, reg::v0, 10));
	h.WriteU32(kFuncA + 4, JR(reg::ra));
	h.WriteU32(kFuncA + 8, NOP);
	h.Run();
	h.ExpectGpr64(reg::v0, 10ull);
	h.ExpectGpr64(reg::v1, 9ull);
}

TEST(EeRecCallRet, JalrNonRaLinkTakesPlainPath)
{
	// JALR t1, t0 — links into t1, not $ra: no frame is pushed, and the
	// callee returns via JR t1 (plain dispatcher path). Ensures the rd!=31
	// gate doesn't disturb semantics.
	EeRecTestHarness h;
	h.SetGpr64(reg::t0, kFuncA);
	h.LoadProgramNoTerm({
		JALR(reg::t1, reg::t0),
		NOP,
		ADDIU(reg::v1, reg::zero, 9),
		J(kPark),
		NOP,
	});
	h.WriteU32(kFuncA + 0, ADDIU(reg::v0, reg::v0, 10));
	h.WriteU32(kFuncA + 4, JR(reg::t1));
	h.WriteU32(kFuncA + 8, NOP);
	h.Run();
	h.ExpectGpr64(reg::v0, 10ull);
	h.ExpectGpr64(reg::v1, 9ull);
}

TEST(EeRecCallRet, ReturnWithEmptyRingMissesSafely)
{
	// A bare JR $ra with no prior call: the pop reads whatever frame sits at
	// the ring head. Pre-fill the whole ring with sentinel frames (the exact
	// post-recResetRaw state) — the compare must miss (sentinel RA=1 can
	// never equal an alignment-checked target) and the return must resolve
	// through the dispatcher.
#if EE_CALLRET_STACK
	ASSERT_NE(_cpuRegistersPack.eeCallRetBase, 0ull);
	u64* ring = reinterpret_cast<u64*>(_cpuRegistersPack.eeCallRetBase);
	for (u64 i = 0; i < kRingBytes / 8; i += 2)
	{
		ring[i] = 1; // sentinel guestRA
		ring[i + 1] = 0;
	}
	_cpuRegistersPack.eeCallRetOff = 0;
#endif
	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		JR(reg::ra), // harness convention: ra = kParkingPc
		NOP,
		ADDIU(reg::v0, reg::zero, 99), // not reached
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);
}

#if EE_CALLRET_STACK
TEST(EeRecCallRet, RingWrapsAcrossZeroAndStaysBalanced)
{
	// Start the ring offset at 16 so the two nested pushes cross the zero
	// boundary (16 -> 0 -> wrap to kRingBytes-16) and the two pops walk
	// back. Behavior must be identical, and the net offset must return to
	// its starting value (balanced push/pop through the wrap).
	_cpuRegistersPack.eeCallRetOff = 16;

	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		ADDIU(reg::v0, reg::zero, 1),
		JAL(kFuncA),
		NOP,
		ADDIU(reg::v1, reg::zero, 9),
		J(kPark),
		NOP,
	});
	h.WriteU32(kFuncA + 0x0, OR(reg::t3, reg::ra, reg::zero));
	h.WriteU32(kFuncA + 0x4, JAL(kFuncB));
	h.WriteU32(kFuncA + 0x8, NOP);
	h.WriteU32(kFuncA + 0xc, OR(reg::ra, reg::t3, reg::zero));
	h.WriteU32(kFuncA + 0x10, JR(reg::ra));
	h.WriteU32(kFuncA + 0x14, ADDIU(reg::v0, reg::v0, 10));
	h.WriteU32(kFuncB + 0x0, JR(reg::ra));
	h.WriteU32(kFuncB + 0x4, ADDIU(reg::v0, reg::v0, 100));
	h.Run();
	h.ExpectGpr64(reg::v0, 111ull);
	h.ExpectGpr64(reg::v1, 9ull);

	EXPECT_EQ(_cpuRegistersPack.eeCallRetOff, 16ull);
	// The outer JAL's frame landed at offset 0, the inner (wrapped) at
	// kRingBytes-16. Both record their guest return PCs.
	EXPECT_EQ(ReadFrame(0).guest_ra, static_cast<u64>(kProgramPc + 12));
	EXPECT_EQ(ReadFrame(kRingBytes - 16).guest_ra, static_cast<u64>(kFuncA + 12));
}

TEST(EeRecCallRet, PushRecordsFrameAndPopRebalances)
{
	// Single roundtrip: the frame below the starting offset carries the JAL
	// return PC and a non-null host landing; the balanced pop restores the
	// starting offset.
	_cpuRegistersPack.eeCallRetOff = 0x80;

	EeRecTestHarness h;
	h.LoadProgramNoTerm({
		JAL(kFuncA),
		NOP,
		J(kPark),
		NOP,
	});
	h.WriteU32(kFuncA + 0, JR(reg::ra));
	h.WriteU32(kFuncA + 4, NOP);
	h.Run();

	EXPECT_EQ(_cpuRegistersPack.eeCallRetOff, 0x80ull);
	const RingFrame f = ReadFrame(0x80 - 16);
	EXPECT_EQ(f.guest_ra, static_cast<u64>(kProgramPc + 8));
	EXPECT_NE(f.landing, 0ull);
}
#endif // EE_CALLRET_STACK
