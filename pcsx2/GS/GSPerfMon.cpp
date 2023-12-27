// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GSPerfMon.h"
#include "GS.h"

#include <cstring>

GSPerfMon g_perfmon;

GSPerfMon::GSPerfMon() = default;

void GSPerfMon::Reset()
{
	m_frame = 0;
	m_lastframe = 0;
	m_count = 0;
	std::memset(m_counters, 0, sizeof(m_counters));
	std::memset(m_stats, 0, sizeof(m_stats));
}

void GSPerfMon::EndFrame(bool frame_only)
{
	m_frame++;

	if(!frame_only)
		m_count++;
}

void GSPerfMon::Update()
{
	if (m_count > 0)
	{
		for (size_t i = 0; i < std::size(m_counters); i++)
		{
			m_stats[i] = m_counters[i] / m_count;
		}

		m_count = 0;
	}

	memset(m_counters, 0, sizeof(m_counters));
}
