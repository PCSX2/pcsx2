// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "ReadbackSpinManager.h"

#include <algorithm>

static bool EventIsReadback(const ReadbackSpinManager::Event& event)
{
	return event.size < 0;
}

static bool EventIsDraw(const ReadbackSpinManager::Event& event)
{
	return !EventIsReadback(event);
}

static bool IsCompleted(const ReadbackSpinManager::Event& event)
{
	return event.begin != event.end;
}

static int Similarity(const std::vector<ReadbackSpinManager::Event>& a, std::vector<ReadbackSpinManager::Event>& b)
{
	u32 a_num_readbacks = std::count_if(a.begin(), a.end(), EventIsReadback);
	u32 b_num_readbacks = std::count_if(b.begin(), b.end(), EventIsReadback);

	int score = 0x10 - abs(static_cast<int>(a.size() - b.size()));

	if (a_num_readbacks == b_num_readbacks)
		score += 0x10000;

	auto a_idx = a.begin();
	auto b_idx = b.begin();
	while (a_idx != a.end() && b_idx != b.end())
	{
		if (EventIsReadback(*a_idx) && EventIsReadback(*b_idx))
		{
			// Same number of events between readbacks
			score += 0x1000;
		}
		// Try to match up on readbacks
		else if (EventIsReadback(*a_idx))
		{
			b_idx++;
			continue;
		}
		else if (EventIsReadback(*b_idx))
		{
			a_idx++;
			continue;
		}
		else if (a_idx->size == b_idx->size)
		{
			// Same size
			score += 0x100;
		}
		else if (a_idx->size / 2 <= b_idx->size && b_idx->size / 2 <= a_idx->size)
		{
			// Similar size
			score += 0x10;
		}
		a_idx++;
		b_idx++;
		continue;
	}
	// Both hit the end at the same time
	if (a_idx == a.end() && b_idx == b.end())
		score += 0x1000;

	return score;
}

static u32 PrevFrameNo(u32 frame, size_t total_frames)
{
	s32 prev_frame = frame - 1;
	if (prev_frame < 0)
		prev_frame = total_frames - 1;
	return prev_frame;
}

static u32 NextFrameNo(u32 frame, size_t total_frames)
{
	u32 next_frame = frame + 1;
	if (next_frame >= total_frames)
		next_frame = 0;
	return next_frame;
}

void ReadbackSpinManager::ReadbackRequested()
{
	Event ev = {};
	ev.size = -1;
	m_frames[m_current_frame].push_back(ev);

	// Advance reference frame idx to the next readback
	while (m_frames[m_reference_frame].size() > m_reference_frame_idx &&
	       !EventIsReadback(m_frames[m_reference_frame][m_reference_frame_idx]))
	{
		m_reference_frame_idx++;
	}
	// ...and past it
	if (m_frames[m_reference_frame].size() > m_reference_frame_idx)
		m_reference_frame_idx++;
}

void ReadbackSpinManager::NextFrame()
{
	u32 prev_frame_0 = PrevFrameNo(m_current_frame, std::size(m_frames));
	u32 prev_frame_1 = PrevFrameNo(prev_frame_0, std::size(m_frames));
	int similarity_0 = Similarity(m_frames[m_current_frame], m_frames[prev_frame_0]);
	int similarity_1 = Similarity(m_frames[m_current_frame], m_frames[prev_frame_1]);

	if (similarity_1 > similarity_0)
		m_reference_frame = prev_frame_0;
	else
		m_reference_frame = m_current_frame;
	m_reference_frame_idx = 0;

	m_current_frame = NextFrameNo(m_current_frame, std::size(m_frames));
	m_frames[m_current_frame].clear();
}

ReadbackSpinManager::DrawSubmittedReturn ReadbackSpinManager::DrawSubmitted(u64 size)
{
	DrawSubmittedReturn out = {};
	u32 idx = m_frames[m_current_frame].size();
	out.id = idx | m_current_frame << 28;
	Event ev = {};
	ev.size = size;
	m_frames[m_current_frame].push_back(ev);

	if (m_reference_frame != m_current_frame &&
	    m_frames[m_reference_frame].size() > m_reference_frame_idx &&
	    EventIsDraw(m_frames[m_reference_frame][m_reference_frame_idx]))
	{
		auto find_next_draw = [this](u32 frame) -> Event* {
			auto next = std::find_if(m_frames[frame].begin() + m_reference_frame_idx + 1,
			                         m_frames[frame].end(),
			                         EventIsDraw);
			bool found = next != m_frames[frame].end();
			if (!found)
			{
				u32 next_frame = NextFrameNo(frame, std::size(m_frames));
				next = std::find_if(m_frames[next_frame].begin(), m_frames[next_frame].end(), EventIsDraw);
				found = next != m_frames[next_frame].end();
			}
			return found ? &*next : nullptr;
		};
		Event* cur_draw = &m_frames[m_reference_frame][m_reference_frame_idx];
		Event* next_draw = find_next_draw(m_reference_frame);
		const bool is_one_frame_back = m_reference_frame == PrevFrameNo(m_current_frame, std::size(m_frames));
		if ((!next_draw || !IsCompleted(*cur_draw) || !IsCompleted(*next_draw)) && is_one_frame_back)
		{
			// Last frame's timing data hasn't arrived, try the same spot in the frame before
			u32 two_back = PrevFrameNo(m_reference_frame, std::size(m_frames));
			if (m_frames[two_back].size() > m_reference_frame_idx &&
			    EventIsDraw(m_frames[two_back][m_reference_frame_idx]))
			{
				cur_draw = &m_frames[two_back][m_reference_frame_idx];
				next_draw = find_next_draw(two_back);
			}
		}
		if (next_draw && IsCompleted(*cur_draw) && IsCompleted(*next_draw) && m_spins_per_unit_time != 0)
		{
			u64 cur_size = cur_draw->size;
			bool is_similar = cur_size / 2 <= size && size / 2 <= cur_size;
			if (is_similar) // Only recommend spins if we're somewhat confident in what's going on
			{
				s32 current_draw_time = cur_draw->end - cur_draw->begin;
				s32 gap = next_draw->begin - cur_draw->end;
				// Give an extra bit of space for the draw to take a bit longer (we'll go with 1/8 longer)
				s32 fill = gap - (current_draw_time >> 3);
				if (fill > 0)
					out.recommended_spin = static_cast<u32>(static_cast<double>(fill) * m_spins_per_unit_time);
			}
		}

		m_reference_frame_idx++;
	}

	if (m_spins_per_unit_time == 0)
	{
		// Recommend some spinning so that we can get timing data
		out.recommended_spin = 128;
	}

	return out;
}

void ReadbackSpinManager::DrawCompleted(u32 id, u32 begin_time, u32 end_time)
{
	u32 frame_id = id >> 28;
	u32 frame_off = id & ((1 << 28) - 1);
	if (frame_id < std::size(m_frames) && frame_off < m_frames[frame_id].size())
	{
		Event& ev = m_frames[frame_id][frame_off];
		ev.begin = begin_time;
		ev.end = end_time;
	}
}

void ReadbackSpinManager::SpinCompleted(u32 cycles, u32 begin_time, u32 end_time)
{
	double elapsed = static_cast<double>(end_time - begin_time);
	constexpr double decay = 15.0 / 16.0;

	// Obviously it'll vary from GPU to GPU, but in my testing,
	// both a Radeon Pro 5600M and Intel UHD 630 spin at about 100ns/cycle

	// Note: We assume spin time is some constant times the number of cycles
	// Obviously as the number of cycles gets really low, a constant offset may start being noticeable
	// But this is not the case as low as 512 cycles (~50µs) on the GPUs listed above

	m_total_spin_cycles = m_total_spin_cycles * decay + cycles;
	m_total_spin_time = m_total_spin_time * decay + elapsed;
	m_spins_per_unit_time = m_total_spin_cycles / m_total_spin_time;
}
