// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// EE Timer T_COUNT read vs. overflow/target event atomicity.
//
// The counter value returned by a T_COUNT read is derived from the live
// cpuRegs.cycle, but the overflow/target side effects (INTC raise + OVFF/EQUF
// mode flags + the guest ISR running) only happen when the event system fires
// at a block boundary. Under the JITs the guest could therefore observe a
// *wrapped* count while the corresponding interrupt hadn't even been raised —
// a state impossible on hardware, where the wrap and the interrupt are the
// same edge (and with IE=1 the ISR preempts before any later read).
//
// NFL 2K5 (SLUS-20919) trips exactly this: its 64-bit clock is
// (overflow-ISR-maintained wrap accumulator + T0_COUNT), read lock-free with
// a double-read reconcile that is airtight on hardware. A wrapped count with
// a stale accumulator makes time go backwards by one wrap period, and its
// divide-by-repeated-subtraction then runs ~2^48 iterations — a permanent
// hang at the boot logo.
//
// The fix: a T_COUNT read must never expose a boundary crossing whose event
// hasn't fired yet — the read clamps to just-before-the-boundary (frozen for
// the few dozen cycles until the event test), and the scheduled event then
// makes the wrap and the interrupt visible together. These tests pin the
// read-side clamp.

#include "Counters.h"
#include "Hw.h"
#include "Memory.h"
#include "R5900.h"

#include <gtest/gtest.h>

namespace
{
// NFL 2K5's Timer0 shape: CLKS=bus/16, CUE counting, OVFE overflow interrupt.
constexpr u32 kMode_Bus16_Ovfe = 0x281;
// CLKS=bus/16, CUE, target interrupt + zero-return.
constexpr u32 kMode_Bus16_TargetZret = 0x1c1;
constexpr u32 kEeCyclesPerTick = 32; // bus/16 == 32 EE cycles per tick
constexpr u32 kTim0IntcBit = 1u << 9;

void ProgramTimer0(u32 mode)
{
	rcntInit();
	psHu32(INTC_STAT) = 0;
	mem32_t v = 0;
	rcntWrite32<0x00>(RCNT0_COUNT, v);
	v = mode;
	rcntWrite32<0x00>(RCNT0_MODE, v);
}
} // namespace

TEST(EeTimerCountReadRace, WrappedCountNotVisibleBeforeOverflowEvent)
{
	ProgramTimer0(kMode_Bus16_Ovfe);

	// Advance time one full wrap plus 100 ticks WITHOUT running rcntUpdate —
	// models the JIT running ahead of the scheduled overflow event inside a
	// block. The wrapped value must not be observable yet (the guest's
	// overflow ISR has not run); the read clamps to just-before-the-wrap.
	cpuRegs.cycle += kEeCyclesPerTick * 0x10000 + kEeCyclesPerTick * 100;

	EXPECT_EQ(rcntRead32<0x00>(RCNT0_COUNT), 0xFFFFu)
		<< "wrapped T0_COUNT was observable before its overflow event fired";
	// A second read in the same window must stay frozen (monotonic, no wrap).
	EXPECT_EQ(rcntRead32<0x00>(RCNT0_COUNT), 0xFFFFu);
	// The overflow side effects still belong to the event, not the read.
	EXPECT_EQ(psHu32(INTC_STAT) & kTim0IntcBit, 0u);
	EXPECT_EQ(counters[0].modeval & 0x800, 0u);
}

TEST(EeTimerCountReadRace, PreWrapReadIsExact)
{
	ProgramTimer0(kMode_Bus16_Ovfe);

	cpuRegs.cycle += kEeCyclesPerTick * 0xF000; // near, but before, the wrap

	EXPECT_EQ(rcntRead32<0x00>(RCNT0_COUNT), 0xF000u);
	EXPECT_EQ(psHu32(INTC_STAT) & kTim0IntcBit, 0u);
	EXPECT_EQ(counters[0].modeval & 0x800, 0u);
}

TEST(EeTimerCountReadRace, ZeroReturnCountClampsBelowTargetBeforeEvent)
{
	ProgramTimer0(kMode_Bus16_TargetZret);
	mem32_t target = 0x4000;
	rcntWrite32<0x00>(RCNT0_TARGET, target);

	// Cross the target without running rcntUpdate. With ZeroReturn the
	// visible count must never reach the target before the event performs the
	// reset (on hardware the counter never holds a value >= target).
	cpuRegs.cycle += kEeCyclesPerTick * 0x4000 + kEeCyclesPerTick * 5;

	EXPECT_EQ(rcntRead32<0x00>(RCNT0_COUNT), 0x3FFFu)
		<< "ZeroReturn T0_COUNT reached its target before the target event fired";
}

TEST(EeTimerCountReadRace, WrappedCountNotVisibleBeforeInterruptDelivery)
{
	// Phase 2 of the window: rcntUpdate has processed the overflow (count
	// wrapped, OVFF set, INTC bit raised) but the exception hasn't been
	// dispatched to the guest yet. With interrupts enabled, the wrapped count
	// must stay hidden — on hardware the ISR would already have preempted us.
	ProgramTimer0(kMode_Bus16_Ovfe);
	cpuRegs.cycle += kEeCyclesPerTick * 100;

	counters[0].modeval |= 0x800; // OVFF (overflow reached, unacked)
	psHu32(INTC_STAT) |= kTim0IntcBit;
	psHu32(INTC_MASK) |= kTim0IntcBit;
	cpuRegs.CP0.n.Status.val = 0x70030c11; // EIE+IE+IM2, no EXL/ERL (live NFL 2K5 value)

	EXPECT_EQ(rcntRead32<0x00>(RCNT0_COUNT), 0xFFFFu)
		<< "wrapped T0_COUNT was observable while its overflow interrupt was pending delivery";

	// Once inside the handler (EXL=1) the interrupt is no longer deliverable
	// and the wrapped count is legitimately observable — the ISR must see it.
	cpuRegs.CP0.n.Status.val |= 0x2; // EXL
	EXPECT_EQ(rcntRead32<0x00>(RCNT0_COUNT), 100u);
	cpuRegs.CP0.n.Status.val = 0x70030c11;

	// After the ISR acks (INTC_STAT cleared), the count is visible again.
	psHu32(INTC_STAT) &= ~kTim0IntcBit;
	EXPECT_EQ(rcntRead32<0x00>(RCNT0_COUNT), 100u);
	cpuRegs.CP0.n.Status.val = 0;
}

TEST(EeTimerCountReadRace, MaskedPendingOverflowDoesNotClamp)
{
	// A game that masks the TIM0 INTC and polls count + flags manually must
	// see the wrapped count immediately (hardware allows it: no delivery).
	ProgramTimer0(kMode_Bus16_Ovfe);
	cpuRegs.cycle += kEeCyclesPerTick * 100;

	counters[0].modeval |= 0x800;
	psHu32(INTC_STAT) |= kTim0IntcBit;
	psHu32(INTC_MASK) &= ~kTim0IntcBit; // masked
	cpuRegs.CP0.n.Status.val = 0x70030c11;

	EXPECT_EQ(rcntRead32<0x00>(RCNT0_COUNT), 100u);
	cpuRegs.CP0.n.Status.val = 0;
}

TEST(EeTimerCountReadRace, StoppedCounterDoesNotMove)
{
	// CUE off: counter holds; advancing cycles must not move the count.
	ProgramTimer0(0x201); // OVFE set but IsCounting clear

	cpuRegs.cycle += kEeCyclesPerTick * 0x20000;

	EXPECT_EQ(rcntRead32<0x00>(RCNT0_COUNT), 0u);
	EXPECT_EQ(psHu32(INTC_STAT) & kTim0IntcBit, 0u);
}
