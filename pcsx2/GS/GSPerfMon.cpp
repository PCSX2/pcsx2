// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GSPerfMon.h"
#include "GS.h"
#include "GSUtil.h"

#include <cstring>
#include <inttypes.h>

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

	if (!frame_only)
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

GSPerfMon GSPerfMon::operator-(const GSPerfMon& other)
{
	GSPerfMon diff;
	for (std::size_t i = 0; i < std::size(diff.m_counters); i++)
	{
		diff.m_counters[i] = m_counters[i] - other.m_counters[i];
	}
	return diff;
}

void GSPerfMon::Dump(const std::string& filename, bool hw)
{
	FILE* fp = fopen(filename.c_str(), "w");
	if (!fp)
		return;

	std::size_t last = hw ? CounterLastHW : CounterLastSW;
	for (std::size_t i = 0; i < last; i++)
	{
		fprintf(fp, "%s: %" PRIu64 "\n", GSUtil::GetPerfMonCounterName(static_cast<counter_t>(i), hw), static_cast<u64>(m_counters[i]));
	}

	fclose(fp);
}
