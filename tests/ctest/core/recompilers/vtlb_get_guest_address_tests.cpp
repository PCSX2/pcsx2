// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// vtlb_GetGuestAddress range-math hardening (DT-07).
//
// vtlb_GetGuestAddress maps a host fastmem pointer back to a 32-bit guest
// address. The original range check computed `fastmem_end = fastmem_start +
// 0xFFFFFFFF` and tested `host < start || host > end`. That addition OVERFLOWS
// a 64-bit uptr whenever the fastmem mapping lands within 4 GB of the top of the
// address space (exotic kernels / high-mmap allocators, e.g. FreeBSD/bhyve):
// fastmem_end wraps below fastmem_start, so the check then rejects every valid
// in-range address (silent wrong-behavior) — or, for other bases, could accept
// an out-of-range one.
//
// The hardened form is the explicit unsigned bound:
//   offset = host_addr - fastmem_start; reject if offset >= FASTMEM_AREA_SIZE.
// Unsigned wraparound subtraction makes it overflow-proof regardless of base:
// a below-base host wraps to a huge offset (rejected), an above-area host gives
// offset >= size (rejected), and the in-area case is exact. Shared (non-arm64)
// code; carried in our tree per Discord triage DT-07.

#include "vtlb.h"

#include "common/Pcsx2Types.h"

#include <gtest/gtest.h>

namespace {

constexpr u64 kFastmemAreaSize = 0x100000000ULL; // mirrors vtlb.cpp FASTMEM_AREA_SIZE

// Save/restore vtlbdata.fastmem_base around a test so we don't perturb the
// shared fastmem mapping the rest of the suite relies on.
struct ScopedFastmemBase
{
	uptr prev;
	explicit ScopedFastmemBase(uptr base) : prev(vtlb_private::vtlbdata.fastmem_base)
	{
		vtlb_private::vtlbdata.fastmem_base = base;
	}
	~ScopedFastmemBase() { vtlb_private::vtlbdata.fastmem_base = prev; }
};

} // namespace

// The regression: a fastmem base within 4 GB of 2^64 makes the old
// `start + 0xFFFFFFFF` wrap. A valid in-range host pointer must still resolve.
TEST(VtlbGetGuestAddress, HighBaseInRangeStillResolves)
{
	const uptr base = static_cast<uptr>(0xFFFFFFFF80000000ULL);
	ScopedFastmemBase guard(base);

	u32 guest = 0xDEADBEEF;
	// base + 0x1000 is clearly inside the 4 GB area; the old wrapping check
	// rejected it (false negative). The hardened check accepts it.
	EXPECT_TRUE(vtlb_GetGuestAddress(base + 0x1000, &guest));
	EXPECT_EQ(guest, 0x1000u);

	// Top of the area (offset 0xFFFFFFFF) must resolve.
	guest = 0;
	EXPECT_TRUE(vtlb_GetGuestAddress(base + 0xFFFFFFFFu, &guest));
	EXPECT_EQ(guest, 0xFFFFFFFFu);
}

// Overflow-proof rejection at a high base. NOTE: with a base this high the 4 GB
// area conceptually wraps past 2^64, so a *low* address is actually inside the
// wrapped tail and must be ACCEPTED — the out-of-range probes are therefore one
// past the end (offset == FASTMEM_AREA_SIZE) and one just below the base (offset
// wraps to ~2^64, i.e. into the gap between the wrapped tail and the base).
TEST(VtlbGetGuestAddress, HighBaseRejectsOutOfRange)
{
	const uptr base = static_cast<uptr>(0xFFFFFFFF80000000ULL);
	ScopedFastmemBase guard(base);

	u32 guest = 0;
	// One past the end of the 4 GB area → reject (offset == size).
	EXPECT_FALSE(vtlb_GetGuestAddress(base + kFastmemAreaSize, &guest));
	// Just below the base → reject (unsigned subtraction wraps to ~2^64).
	EXPECT_FALSE(vtlb_GetGuestAddress(base - 1, &guest));
}

// Sanity: an ordinary low base behaves exactly as before (no regression).
TEST(VtlbGetGuestAddress, NormalBaseRoundTrips)
{
	const uptr base = static_cast<uptr>(0x10000000ULL);
	ScopedFastmemBase guard(base);

	u32 guest = 0;
	EXPECT_TRUE(vtlb_GetGuestAddress(base + 0x2000, &guest));
	EXPECT_EQ(guest, 0x2000u);
	EXPECT_TRUE(vtlb_GetGuestAddress(base + 0xFFFFFFFFu, &guest));
	EXPECT_EQ(guest, 0xFFFFFFFFu);

	EXPECT_FALSE(vtlb_GetGuestAddress(base - 1, &guest));          // below base
	EXPECT_FALSE(vtlb_GetGuestAddress(base + kFastmemAreaSize, &guest)); // past end
}
