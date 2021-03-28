/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2021 PCSX2 Dev Team
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

#include "General.h"

static u32 PAUSE_TIME = 0;

static void MultiPause()
{
	_mm_pause();
	_mm_pause();
	_mm_pause();
	_mm_pause();
	_mm_pause();
	_mm_pause();
	_mm_pause();
	_mm_pause();
}

__noinline static void UpdatePauseTime()
{
	u64 start = GetCPUTicks();
	for (int i = 0; i < 64; i++)
	{
		MultiPause();
	}
	u64 time = GetCPUTicks() - start;
	u64 nanos = (time * 1000000000) / GetTickFrequency();
	PAUSE_TIME = (nanos / 64) + 1;
}

u32 ShortSpin()
{
	u32 inc = PAUSE_TIME;
	if (unlikely(inc == 0))
	{
		UpdatePauseTime();
		inc = PAUSE_TIME;
	}

	u32 time = 0;
	// Sleep for approximately 500ns
	for (; time < 500; time += inc)
		MultiPause();

	return time;
}

static u32 GetSpinTime()
{
	if (char* req = getenv("WAIT_SPIN_MICROSECONDS"))
	{
		return 1000 * atoi(req);
	}
	else
	{
		return 50 * 1000; // 50µs
	}
}

const u32 SPIN_TIME_NS = GetSpinTime();
