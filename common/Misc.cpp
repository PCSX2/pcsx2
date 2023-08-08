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
#include "Console.h"

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

static u32 MeasurePauseTime()
{
	// GetCPUTicks may have resolution as low as 1µs
	// One call to MultiPause could take anywhere from 20ns (fast Haswell) to 400ns (slow Skylake)
	// We want a measurement of reasonable resolution, but don't want to take too long
	// So start at a fairly small number and increase it if it's too fast
	for (int testcnt = 64; true; testcnt *= 2)
	{
		u64 start = GetCPUTicks();
		for (int i = 0; i < testcnt; i++)
		{
			MultiPause();
		}
		u64 time = GetCPUTicks() - start;
		if (time > 100)
		{
			u64 nanos = (time * 1000000000) / GetTickFrequency();
			return (nanos / testcnt) + 1;
		}
	}
}

__noinline static void UpdatePauseTime()
{
	u64 wait = GetCPUTicks() + GetTickFrequency() / 100; // Wake up processor (spin for 10ms)
	while (GetCPUTicks() < wait)
		;
	u32 pause = MeasurePauseTime();
	// Take a few measurements in case something weird happens during one
	// (e.g. OS interrupt)
	for (int i = 0; i < 4; i++)
		pause = std::min(pause, MeasurePauseTime());
	PAUSE_TIME = pause;
	DevCon.WriteLn("MultiPause time: %uns", pause);
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

#ifdef __APPLE__
// https://alastairs-place.net/blog/2013/01/10/interesting-os-x-crash-report-tidbits/
// https://opensource.apple.com/source/WebKit2/WebKit2-7608.3.10.0.3/Platform/spi/Cocoa/CrashReporterClientSPI.h.auto.html
struct crash_info_t
{
	u64 version;
	u64 message;
	u64 signature;
	u64 backtrace;
	u64 message2;
	u64 reserved;
	u64 reserved2;
};
#define CRASH_ANNOTATION __attribute__((used, section("__DATA,__crash_info")))
#define CRASH_VERSION 4
extern "C" crash_info_t gCRAnnotations CRASH_ANNOTATION = { CRASH_VERSION };
#endif

void AbortWithMessage(const char* msg)
{
#ifdef __APPLE__
	gCRAnnotations.message = reinterpret_cast<size_t>(msg);
	// Some macOS's seem to have issues displaying non-static `message`s, so throw it in here too
	gCRAnnotations.backtrace = gCRAnnotations.message;
#endif
	abort();
}
