// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Types.h"

#include <array>

// Thin wrapper around perf_event_open(2) for measuring hardware perf counters
// on the calling thread, user-mode only. Designed for codegen-iteration loops
// that need cycles/instructions/branch-misses/L1D-refills per iteration with
// sub-microsecond overhead.
//
// Linux-only — on other platforms Open() returns false and Read() returns
// zeros, so callers should treat "open failed" as "skip the bench" rather
// than as an error.
//
// Counter access can be restricted by /proc/sys/kernel/perf_event_paranoid:
//   2 (default on many distros) → user-only counters require CAP_PERFMON,
//                                  Open() will fail without it.
//   1                            → user-only counters work for unprivileged
//                                  users.
//   0                            → all counters available.
// To enable on a development box: `sysctl kernel.perf_event_paranoid=1`.
//
// Per-counter availability also varies by host. Apple Silicon under Asahi
// exposes cycles / instructions / branch-misses / branch-instructions but
// not the generic L1D cache events. Open() tolerates per-counter failures:
// the leader (CPU_CYCLES) is required, followers that fail are simply not
// installed and will read as 0. Use IsAvailable(c) to distinguish "0 events"
// from "this PMU doesn't have that counter."

namespace PmuCounters
{
	// Counters we read in a single perf_event_open group. Order matches the
	// order of values returned by Read() and Measure().
	enum Counter : int
	{
		CpuCycles = 0,         // PERF_COUNT_HW_CPU_CYCLES — required (group leader)
		InstructionsRetired,   // PERF_COUNT_HW_INSTRUCTIONS
		BranchMisses,          // PERF_COUNT_HW_BRANCH_MISSES
		BranchInstructions,    // PERF_COUNT_HW_BRANCH_INSTRUCTIONS
		L1dCacheRefills,       // L1D read miss — unavailable on Apple PMU
		Count
	};

	using Values = std::array<u64, Counter::Count>;

	class Group
	{
	public:
		Group();
		~Group();

		Group(const Group&) = delete;
		Group& operator=(const Group&) = delete;

		// Opens the hardware perf counters as one group on the calling thread,
		// user-mode-only, initially disabled. Returns false on syscall
		// failure (most commonly EACCES from perf_event_paranoid).
		bool Open();

		// Reset accumulated counts to zero.
		void Reset();

		// Toggle counting. Group leader uses PERF_FORMAT_GROUP; a single
		// PERF_EVENT_IOC_ENABLE/DISABLE ioctl with PERF_IOC_FLAG_GROUP flips
		// every counter in the group atomically.
		void Enable();
		void Disable();

		// Read current counter values. Safe to call any time after Open();
		// returns zeros if the group isn't open. Read is allowed while
		// counters are running — the values are an instantaneous snapshot.
		Values Read() const;

		// Convenience: enable, run callable, disable, read. The Reset()
		// before the run means the returned values are deltas, not absolute.
		template <typename F>
		Values Measure(F&& fn)
		{
			Reset();
			Enable();
			fn();
			Disable();
			return Read();
		}

		// True iff Open() succeeded and the group hasn't been closed.
		bool IsOpen() const { return m_leader_fd >= 0; }

		// True iff this counter was actually installed by Open(). Followers
		// that returned ENOENT (PMU doesn't support the event) read as 0
		// from Read(); IsAvailable lets callers distinguish "really 0" from
		// "not measured."
		bool IsAvailable(Counter c) const;

	private:
		void Close();

		// fds[0] is the group leader; fds[1..] are followers attached via
		// the leader's group_fd. -1 marks slots that failed to open.
		int m_leader_fd = -1;
		int m_follower_fds[Counter::Count - 1] = {};

		// Slot i in the PERF_FORMAT_GROUP read buffer that corresponds to
		// Counter i. -1 means the counter wasn't installed.
		int m_read_slot[Counter::Count] = {};
		int m_installed_count = 0;
	};

	// Static text label for a counter — useful for printing.
	const char* Name(Counter c);
} // namespace PmuCounters
