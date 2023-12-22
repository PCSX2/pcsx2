// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <ctime>

class GSPerfMon
{
public:
	enum counter_t
	{
		Prim,
		Draw,
		DrawCalls,
		Readbacks,
		Swizzle,
		Unswizzle,
		Fillrate,
		SyncPoint,
		Barriers,
		RenderPasses,
		CounterLast,

		// Reused counters for HW.
		TextureCopies = Fillrate,
		TextureUploads = SyncPoint,
	};

protected:
	double m_counters[CounterLast] = {};
	double m_stats[CounterLast] = {};
	u64 m_frame = 0;
	clock_t m_lastframe = 0;
	int m_count = 0;
	int m_disp_fb_sprite_blits = 0;

public:
	GSPerfMon();

	void Reset();

	void SetFrame(u64 frame) { m_frame = frame; }
	u64 GetFrame() { return m_frame; }
	void EndFrame(bool frame_only);

	void Put(counter_t c, double val) { m_counters[c] += val; }
	double GetCounter(counter_t c) { return m_counters[c]; }
	double Get(counter_t c) { return m_stats[c]; }
	void Update();

	__fi void AddDisplayFramebufferSpriteBlit() { m_disp_fb_sprite_blits++; }
	__fi int GetDisplayFramebufferSpriteBlits()
	{
		const int blits = m_disp_fb_sprite_blits;
		m_disp_fb_sprite_blits = 0;
		return blits;
	}
};

extern GSPerfMon g_perfmon;