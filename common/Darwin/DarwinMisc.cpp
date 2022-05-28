/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
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

#if defined(__APPLE__)

#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach_time.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include "common/Pcsx2Types.h"
#include "common/General.h"

// Darwin (OSX) is a bit different from Linux when requesting properties of
// the OS because of its BSD/Mach heritage. Helpfully, most of this code
// should translate pretty well to other *BSD systems. (e.g.: the sysctl(3)
// interface).
//
// For an overview of all of Darwin's sysctls, check:
// https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/sysctl.3.html

// Return the total physical memory on the machine, in bytes. Returns 0 on
// failure (not supported by the operating system).
u64 GetPhysicalMemory()
{
	u64 getmem = 0;
	size_t len = sizeof(getmem);
	int mib[] = {CTL_HW, HW_MEMSIZE};
	if (sysctl(mib, std::size(mib), &getmem, &len, NULL, 0) < 0)
		perror("sysctl:");
	return getmem;
}

static u64 tickfreq;

void InitCPUTicks()
{
	mach_timebase_info_data_t info;
	if (mach_timebase_info(&info) != KERN_SUCCESS)
		abort();
	tickfreq = (u64)1e9 * (u64)info.denom / (u64)info.numer;
}

// returns the performance-counter frequency: ticks per second (Hz)
//
// usage:
//   u64 seconds_passed = GetCPUTicks() / GetTickFrequency();
//   u64 millis_passed = (GetCPUTicks() * 1000) / GetTickFrequency();
//
// NOTE: multiply, subtract, ... your ticks before dividing by
// GetTickFrequency() to maintain good precision.
u64 GetTickFrequency()
{
	return tickfreq;
}

// return the number of "ticks" since some arbitrary, fixed time in the
// past. On OSX x86(-64), this is actually the number of nanoseconds passed,
// because mach_timebase_info.numer == denom == 1. So "ticks" ==
// nanoseconds.
u64 GetCPUTicks()
{
	return mach_absolute_time();
}

static std::string sysctl_str(int category, int name)
{
	char buf[32];
	size_t len = sizeof(buf);
	int mib[] = {category, name};
	sysctl(mib, std::size(mib), buf, &len, nullptr, 0);
	return std::string(buf, len > 0 ? len - 1 : 0);
}

std::string GetOSVersionString()
{
	std::string type    = sysctl_str(CTL_KERN, KERN_OSTYPE);
	std::string release = sysctl_str(CTL_KERN, KERN_OSRELEASE);
	std::string arch    = sysctl_str(CTL_HW, HW_MACHINE);
	return type + " " + release + " " + arch;
}

static IOPMAssertionID s_pm_assertion;

void ScreensaverAllow(bool allow)
{
	if (s_pm_assertion)
	{
		IOPMAssertionRelease(s_pm_assertion);
		s_pm_assertion = 0;
	}

	if (!allow)
		IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn, CFSTR("Playing a game"), &s_pm_assertion);
}
#endif
