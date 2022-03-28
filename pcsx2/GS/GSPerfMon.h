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

#pragma once

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
		Quad,
		SyncPoint,
		Barriers,
		CounterLast,

		// Reused counters for HW.
		TextureCopies = Fillrate,
		TextureUploads = SyncPoint,
	};

protected:
	double m_counters[CounterLast];
	double m_stats[CounterLast];
	u64 m_frame;
	clock_t m_lastframe;
	int m_count;
	int m_disp_fb_sprite_blits;

public:
	GSPerfMon();

	void SetFrame(u64 frame) { m_frame = frame; }
	u64 GetFrame() { return m_frame; }
	void EndFrame();

	void Put(counter_t c, double val = 0) { m_counters[c] += val; }
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