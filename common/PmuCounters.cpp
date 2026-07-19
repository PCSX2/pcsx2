// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/PmuCounters.h"

#ifdef __linux__

#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace PmuCounters
{
	namespace
	{
		long PerfEventOpen(struct perf_event_attr* attr, pid_t pid, int cpu, int group_fd, unsigned long flags)
		{
			return ::syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
		}

		void FillAttrFor(Counter c, struct perf_event_attr& attr)
		{
			std::memset(&attr, 0, sizeof(attr));
			attr.size = sizeof(attr);
			attr.disabled = 1;
			attr.exclude_kernel = 1;
			attr.exclude_hv = 1;
			// PERF_FORMAT_GROUP returns all counters in the group from a
			// single read on the leader fd. PERF_FORMAT_ID is not requested
			// because counters are read in fixed order — the index is the
			// identity.
			attr.read_format = PERF_FORMAT_GROUP;

			switch (c)
			{
				case CpuCycles:
					attr.type = PERF_TYPE_HARDWARE;
					attr.config = PERF_COUNT_HW_CPU_CYCLES;
					break;
				case InstructionsRetired:
					attr.type = PERF_TYPE_HARDWARE;
					attr.config = PERF_COUNT_HW_INSTRUCTIONS;
					break;
				case BranchMisses:
					attr.type = PERF_TYPE_HARDWARE;
					attr.config = PERF_COUNT_HW_BRANCH_MISSES;
					break;
				case BranchInstructions:
					attr.type = PERF_TYPE_HARDWARE;
					attr.config = PERF_COUNT_HW_BRANCH_INSTRUCTIONS;
					break;
				case L1dCacheRefills:
					attr.type = PERF_TYPE_HW_CACHE;
					attr.config = (PERF_COUNT_HW_CACHE_L1D)
						| (PERF_COUNT_HW_CACHE_OP_READ << 8)
						| (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
					break;
				default:
					break;
			}
		}
	} // namespace

	Group::Group()
	{
		// Initialize every slot to the "not installed" sentinel (-1), matching
		// the documented invariant. Without this the arrays zero-initialize, so
		// a Group destroyed without a successful Open() would (a) report
		// IsAvailable()==true for slot 0 (0 >= 0), and (b) have Close() call
		// ::close(0) on the still-zero follower fds — closing stdin. Open()
		// repeats this reset before installing counters.
		for (int& fd : m_follower_fds)
			fd = -1;
		for (int& slot : m_read_slot)
			slot = -1;
	}

	Group::~Group()
	{
		Close();
	}

	bool Group::Open()
	{
		Close();

		for (int& fd : m_follower_fds)
			fd = -1;
		for (int& slot : m_read_slot)
			slot = -1;
		m_installed_count = 0;

		struct perf_event_attr leader_attr;
		FillAttrFor(CpuCycles, leader_attr);
		// Leader: pid=0 (calling thread), cpu=-1 (any), group_fd=-1.
		m_leader_fd = static_cast<int>(PerfEventOpen(&leader_attr, 0, -1, -1, 0));
		if (m_leader_fd < 0)
		{
			m_leader_fd = -1;
			return false;
		}
		m_read_slot[CpuCycles] = m_installed_count++;

		// Followers — failures are tolerated (e.g. Apple PMU under Asahi
		// returns ENOENT for the L1D cache event). Read() returns 0 for
		// any counter whose m_read_slot is -1.
		for (int i = 1; i < Counter::Count; ++i)
		{
			struct perf_event_attr attr;
			FillAttrFor(static_cast<Counter>(i), attr);
			const int fd = static_cast<int>(PerfEventOpen(&attr, 0, -1, m_leader_fd, 0));
			if (fd < 0)
				continue;
			m_follower_fds[i - 1] = fd;
			m_read_slot[i] = m_installed_count++;
		}

		Reset();
		return true;
	}

	bool Group::IsAvailable(Counter c) const
	{
		if (c < 0 || c >= Counter::Count)
			return false;
		return m_read_slot[c] >= 0;
	}

	void Group::Close()
	{
		for (int& fd : m_follower_fds)
		{
			if (fd >= 0)
			{
				::close(fd);
				fd = -1;
			}
		}
		if (m_leader_fd >= 0)
		{
			::close(m_leader_fd);
			m_leader_fd = -1;
		}
	}

	void Group::Reset()
	{
		if (m_leader_fd < 0)
			return;
		::ioctl(m_leader_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
	}

	void Group::Enable()
	{
		if (m_leader_fd < 0)
			return;
		::ioctl(m_leader_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
	}

	void Group::Disable()
	{
		if (m_leader_fd < 0)
			return;
		::ioctl(m_leader_fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
	}

	Values Group::Read() const
	{
		Values out{};
		if (m_leader_fd < 0)
			return out;

		// PERF_FORMAT_GROUP read layout. Assumes read_format is GROUP-ONLY — no
		// PERF_FORMAT_TOTAL_TIME_ENABLED/RUNNING/ID/LOST. Those flags insert extra
		// u64s around values[] and would shift every counter; if attr.read_format
		// ever gains one, this struct and the offset math below must change too.
		//   u64 nr;            // number of counters actually installed
		//   u64 values[nr];    // each installed counter's accumulated count
		struct ReadBuf
		{
			u64 nr;
			u64 values[Counter::Count];
		} buf{};
		const size_t expected_bytes = sizeof(u64) * (1 + m_installed_count);
		const ssize_t n = ::read(m_leader_fd, &buf, expected_bytes);
		if (n < static_cast<ssize_t>(expected_bytes))
			return out;
		if (static_cast<int>(buf.nr) != m_installed_count)
			return out;
		for (int i = 0; i < Counter::Count; ++i)
		{
			if (m_read_slot[i] >= 0)
				out[i] = buf.values[m_read_slot[i]];
		}
		return out;
	}

	const char* Name(Counter c)
	{
		switch (c)
		{
			case CpuCycles:            return "cycles";
			case InstructionsRetired:  return "instructions";
			case BranchMisses:         return "branch-misses";
			case BranchInstructions:   return "branches";
			case L1dCacheRefills:      return "L1-dcache-load-misses";
			default:                   return "?";
		}
	}
} // namespace PmuCounters

#else  // !__linux__

namespace PmuCounters
{
	Group::Group() = default;
	Group::~Group() = default;
	bool Group::Open() { return false; }
	bool Group::IsAvailable(Counter) const { return false; }
	void Group::Close() {}
	void Group::Reset() {}
	void Group::Enable() {}
	void Group::Disable() {}
	Values Group::Read() const { return {}; }
	const char* Name(Counter c)
	{
		switch (c)
		{
			case CpuCycles:            return "cycles";
			case InstructionsRetired:  return "instructions";
			case BranchMisses:         return "branch-misses";
			case BranchInstructions:   return "branches";
			case L1dCacheRefills:      return "L1-dcache-load-misses";
			default:                   return "?";
		}
	}
} // namespace PmuCounters

#endif
