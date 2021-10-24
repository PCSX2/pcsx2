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
#include "GS.h"

GSPerfMon g_perfmon;

GSPerfMon::GSPerfMon()
	: m_frame(0)
	, m_lastframe(0)
	, m_count(0)
{
	memset(m_counters, 0, sizeof(m_counters));
	memset(m_stats, 0, sizeof(m_stats));
	memset(m_timer_stats, 0, sizeof(m_timer_stats));
	memset(m_total, 0, sizeof(m_total));
	memset(m_begin, 0, sizeof(m_begin));
}

void GSPerfMon::EndFrame()
{
	m_frame++;
	m_count++;
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

		// Update CPU usage for SW renderer.
		if (GSConfig.Renderer == GSRendererType::SW)
		{
			const u64 current = __rdtsc();

			for (size_t i = WorkerDraw0; i < TimerLast; i++)
			{
				if (m_begin[i] == 0)
				{
					m_timer_stats[i] = 0.0f;
					continue;
				}

				m_timer_stats[i] =
					static_cast<float>(static_cast<double>(m_total[i]) / static_cast<double>(current - m_begin[i])
						* 100.0);

				m_begin[i] = 0;
				m_start[i] = 0;
				m_total[i] = 0;
			}
		}

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
