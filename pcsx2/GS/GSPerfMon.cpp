/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "GSPerfMon.h"

GSPerfMon::GSPerfMon()
	: m_frame(0)
	, m_lastframe(0)
	, m_count(0)
{
	memset(m_counters, 0, sizeof(m_counters));
	memset(m_stats, 0, sizeof(m_stats));
	memset(m_total, 0, sizeof(m_total));
	memset(m_begin, 0, sizeof(m_begin));
}

void GSPerfMon::Put(counter_t c, double val)
{
#ifndef DISABLE_PERF_MON
	if (c == Frame)
	{
#if defined(__unix__) || defined(__APPLE__)
		struct timespec ts;
# ifdef CLOCK_MONOTONIC_RAW
		clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
# else
		clock_gettime(CLOCK_MONOTONIC, &ts);
# endif
		u64 now = (u64)ts.tv_sec * (u64)1e6 + (u64)ts.tv_nsec / (u64)1e3;
#else
		clock_t now = clock();
#endif

		if (m_lastframe != 0)
		{
			m_counters[c] += (now - m_lastframe) * 1000 / CLOCKS_PER_SEC;
		}

		m_lastframe = now;
		m_frame++;
		m_count++;
	}
	else
	{
		m_counters[c] += val;
	}
#endif
}

void GSPerfMon::Update()
{
#ifndef DISABLE_PERF_MON
	if (m_count > 0)
	{
		for (size_t i = 0; i < std::size(m_counters); i++)
		{
			m_stats[i] = m_counters[i] / m_count;
		}

		m_count = 0;
	}

	memset(m_counters, 0, sizeof(m_counters));
#endif
}

void GSPerfMon::Start(int timer)
{
#ifndef DISABLE_PERF_MON
	m_start[timer] = __rdtsc();

	if (m_begin[timer] == 0)
	{
		m_begin[timer] = m_start[timer];
	}
#endif
}

void GSPerfMon::Stop(int timer)
{
#ifndef DISABLE_PERF_MON
	if (m_start[timer] > 0)
	{
		m_total[timer] += __rdtsc() - m_start[timer];
		m_start[timer] = 0;
	}
#endif
}

int GSPerfMon::CPU(int timer, bool reset)
{
	int percent = (int)(100 * m_total[timer] / (__rdtsc() - m_begin[timer]));

	if (reset)
	{
		m_begin[timer] = 0;
		m_start[timer] = 0;
		m_total[timer] = 0;
	}

	return percent;
}
