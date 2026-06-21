// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
//
// Unit tests for MvuObservedEntries, the fixed-cap set of microMem
// byte offsets the dispatcher records for each microProgram. The
// persisted-JIT program cache uses the observed entry-PC set for INDEX
// serialization and preload prioritization, so its contract (idempotent
// insert, version bumps only on new PC, cap silently drops) stays
// pinned here.

#include "arm64/MvuObservedEntries.h"

#include <gtest/gtest.h>

namespace
{

TEST(MvuObservedEntries, default_constructed_is_empty)
{
	MvuObservedEntries e{};
	EXPECT_EQ(0u, e.count);
	EXPECT_EQ(0u, e.version);
}

TEST(MvuObservedEntries, record_first_inserts_and_bumps_version)
{
	MvuObservedEntries e{};
	EXPECT_TRUE(e.record(0x100u));
	EXPECT_EQ(1u, e.count);
	EXPECT_EQ(0x100u, e.pcs[0]);
	EXPECT_EQ(1u, e.version);
}

TEST(MvuObservedEntries, record_duplicate_is_no_op)
{
	MvuObservedEntries e{};
	ASSERT_TRUE(e.record(0x100u));
	EXPECT_FALSE(e.record(0x100u));
	EXPECT_EQ(1u, e.count);
	EXPECT_EQ(1u, e.version);
}

TEST(MvuObservedEntries, record_distinct_pcs_accumulate_in_order)
{
	MvuObservedEntries e{};
	ASSERT_TRUE(e.record(0x000u));
	ASSERT_TRUE(e.record(0x058u));
	ASSERT_TRUE(e.record(0x0A8u));
	ASSERT_TRUE(e.record(0x148u));
	EXPECT_EQ(4u, e.count);
	EXPECT_EQ(0x000u, e.pcs[0]);
	EXPECT_EQ(0x058u, e.pcs[1]);
	EXPECT_EQ(0x0A8u, e.pcs[2]);
	EXPECT_EQ(0x148u, e.pcs[3]);
	EXPECT_EQ(4u, e.version);
}

TEST(MvuObservedEntries, record_interleaved_duplicates_keep_version_stable)
{
	MvuObservedEntries e{};
	ASSERT_TRUE(e.record(0x000u));
	ASSERT_TRUE(e.record(0x058u));
	ASSERT_FALSE(e.record(0x000u));
	ASSERT_FALSE(e.record(0x058u));
	ASSERT_TRUE(e.record(0x0A8u));
	ASSERT_FALSE(e.record(0x058u));
	EXPECT_EQ(3u, e.count);
	EXPECT_EQ(3u, e.version);
}

TEST(MvuObservedEntries, record_at_cap_drops_overflow_silently)
{
	MvuObservedEntries e{};
	for (u32 i = 0; i < MvuObservedEntries::kMax; ++i)
		ASSERT_TRUE(e.record(i * 8u)) << "filling slot " << i;
	EXPECT_EQ(MvuObservedEntries::kMax, e.count);
	EXPECT_EQ(MvuObservedEntries::kMax, e.version);

	// One past the cap: silently dropped, count + version unchanged.
	EXPECT_FALSE(e.record(0xDEADu));
	EXPECT_EQ(MvuObservedEntries::kMax, e.count);
	EXPECT_EQ(MvuObservedEntries::kMax, e.version);

	// An already-observed PC at the cap is still detected as a
	// duplicate (the cap check fires only when the linear scan misses).
	EXPECT_FALSE(e.record(0u));
	EXPECT_EQ(MvuObservedEntries::kMax, e.count);
	EXPECT_EQ(MvuObservedEntries::kMax, e.version);
}

TEST(MvuObservedEntries, clear_resets_to_empty_state)
{
	MvuObservedEntries e{};
	ASSERT_TRUE(e.record(0x100u));
	ASSERT_TRUE(e.record(0x200u));
	ASSERT_TRUE(e.record(0x300u));
	e.clear();
	EXPECT_EQ(0u, e.count);
	EXPECT_EQ(0u, e.version);
	// Re-record after clear works exactly like a fresh struct.
	EXPECT_TRUE(e.record(0x100u));
	EXPECT_EQ(1u, e.count);
	EXPECT_EQ(1u, e.version);
}

TEST(MvuObservedEntries, version_bump_count_matches_distinct_inserts)
{
	// The "re-compile when version moves" heuristic depends on the version
	// reading exactly the number of distinct PCs the dispatcher has
	// observed. Pin that invariant here so a future refactor (e.g. "bump
	// on every record() call") doesn't break the re-compile threshold.
	MvuObservedEntries e{};
	// Mirrors the 9 distinct entry PCs observed on Katamari's vu0
	// program `7009298a…`, with deliberate duplicates sprinkled in to
	// exercise the idempotent path.
	const u32 inserts[] = {
		0x000, 0x058, 0x000, 0x0A8, 0x0C8, 0x058,
		0x0D8, 0x100, 0x118, 0x128, 0x148, 0x148, 0x148,
	};
	u32 expected_distinct = 0;
	for (u32 pc : inserts)
	{
		bool inserted = e.record(pc);
		if (inserted)
			++expected_distinct;
		EXPECT_EQ(expected_distinct, e.version);
		EXPECT_EQ(expected_distinct, e.count);
	}
	// 9 distinct PCs in the Katamari sequence.
	EXPECT_EQ(9u, e.count);
	EXPECT_EQ(9u, e.version);
}

} // namespace
