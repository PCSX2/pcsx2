// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Smoke tests for PmuCounters. The tests SUCCEED-and-skip-assertions when
// perf_event_open is restricted (perf_event_paranoid >= 2 without
// CAP_PERFMON), to avoid CI failures on locked-down hosts.

#include "common/PmuCounters.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

TEST(PmuCounters, OpenSucceedsOrSkipsCleanly)
{
	PmuCounters::Group g;
	const bool opened = g.Open();
	if (!opened)
	{
		GTEST_SKIP() << "perf_event_open denied — set kernel.perf_event_paranoid=1 "
		                "to enable this test on a development box.";
	}
	EXPECT_TRUE(g.IsOpen());
	// Leader is always installed if Open() returned true.
	EXPECT_TRUE(g.IsAvailable(PmuCounters::CpuCycles));
}

TEST(PmuCounters, MeasureRecordsNonZeroCyclesAndInstructions)
{
	PmuCounters::Group g;
	if (!g.Open())
		GTEST_SKIP() << "perf_event_open denied";

	// Busy work the optimizer can't prove dead — accumulator escapes via the
	// gtest assertion below.
	volatile u64 sink = 0;
	const auto values = g.Measure([&]() {
		for (u64 i = 0; i < 1'000'000; ++i)
			sink += i * 7u;
	});

	EXPECT_GT(values[PmuCounters::CpuCycles], 0u);
	EXPECT_GT(values[PmuCounters::InstructionsRetired], 0u);
	// Instructions ≈ same order as cycles for this loop. Loose bound so the
	// test does not need to reason about IPC on whichever host is running.
	EXPECT_GT(values[PmuCounters::InstructionsRetired],
		values[PmuCounters::CpuCycles] / 100u);

	(void)sink;  // placate the optimizer; the volatile load is what matters
}

TEST(PmuCounters, ResetClearsCounts)
{
	PmuCounters::Group g;
	if (!g.Open())
		GTEST_SKIP() << "perf_event_open denied";

	volatile u64 sink = 0;
	g.Enable();
	for (u64 i = 0; i < 10'000; ++i)
		sink += i;
	g.Disable();
	const auto first = g.Read();
	EXPECT_GT(first[PmuCounters::CpuCycles], 0u);

	// Reset() reads back exactly zero only because the group is Disabled here:
	// PERF_EVENT_IOC_RESET on a still-enabled leader can race in-flight counting
	// and return a small non-zero value. Don't copy this == 0 assertion into a
	// context where the counters are still running (Read() itself is allowed
	// while running, but a reset-then-read-zero is not).
	g.Reset();
	const auto after_reset = g.Read();
	EXPECT_EQ(after_reset[PmuCounters::CpuCycles], 0u);
	EXPECT_EQ(after_reset[PmuCounters::InstructionsRetired], 0u);

	(void)sink;
}

TEST(PmuCounters, NameReturnsStableLabels)
{
	EXPECT_STREQ("cycles", PmuCounters::Name(PmuCounters::CpuCycles));
	EXPECT_STREQ("instructions", PmuCounters::Name(PmuCounters::InstructionsRetired));
	EXPECT_STREQ("branch-misses", PmuCounters::Name(PmuCounters::BranchMisses));
	EXPECT_STREQ("branches", PmuCounters::Name(PmuCounters::BranchInstructions));
	EXPECT_STREQ("L1-dcache-load-misses", PmuCounters::Name(PmuCounters::L1dCacheRefills));
}

TEST(PmuCounters, ReadReturnsZeroForUnavailableCounter)
{
	PmuCounters::Group g;
	if (!g.Open())
		GTEST_SKIP() << "perf_event_open denied";

	const auto values = g.Read();
	for (int i = 0; i < PmuCounters::Count; ++i)
	{
		const auto c = static_cast<PmuCounters::Counter>(i);
		if (!g.IsAvailable(c))
			EXPECT_EQ(values[i], 0u) << PmuCounters::Name(c);
	}
}
