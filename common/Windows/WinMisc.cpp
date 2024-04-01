// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#if defined(_WIN32)

#include "common/FileSystem.h"
#include "common/HostSys.h"
#include "common/RedtapeWindows.h"
#include "common/StringUtil.h"
#include "common/Threading.h"
#include "common/WindowInfo.h"

#include "fmt/core.h"

#include <mmsystem.h>
#include <timeapi.h>
#include <VersionHelpers.h>

// If anything tries to read this as an initializer, we're in trouble.
static const LARGE_INTEGER lfreq = []() {
	LARGE_INTEGER ret = {};
	QueryPerformanceFrequency(&ret);
	return ret;
}();

// This gets leaked... oh well.
static thread_local HANDLE s_sleep_timer;
static thread_local bool s_sleep_timer_created = false;

static HANDLE GetSleepTimer()
{
	if (s_sleep_timer_created)
		return s_sleep_timer;

	s_sleep_timer_created = true;
	s_sleep_timer = CreateWaitableTimerEx(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	if (!s_sleep_timer)
		s_sleep_timer = CreateWaitableTimer(nullptr, TRUE, nullptr);

	return s_sleep_timer;
}

u64 GetTickFrequency()
{
	return lfreq.QuadPart;
}

u64 GetCPUTicks()
{
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	return count.QuadPart;
}

u64 GetPhysicalMemory()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return status.ullTotalPhys;
}

// Calculates the Windows OS Version and processor architecture, and returns it as a
// human-readable string. :)
std::string GetOSVersionString()
{
	std::string retval;

	SYSTEM_INFO si;
	GetNativeSystemInfo(&si);

	if (IsWindows10OrGreater())
	{
		retval = "Microsoft ";
		retval += IsWindowsServer() ? "Windows Server 2016+" : "Windows 10+";
		
	}
	else
		retval = "Unsupported Operating System!";

	return retval;
}

bool WindowInfo::InhibitScreensaver(const WindowInfo& wi, bool inhibit)
{
	EXECUTION_STATE flags = ES_CONTINUOUS;
	if (inhibit)
		flags |= ES_DISPLAY_REQUIRED;
	SetThreadExecutionState(flags);
	return true;
}

bool Common::PlaySoundAsync(const char* path)
{
	const std::wstring wpath = FileSystem::GetWin32Path(path);
	return PlaySoundW(wpath.c_str(), NULL, SND_ASYNC | SND_NODEFAULT);
}

void Threading::Sleep(int ms)
{
	::Sleep(ms);
}

void Threading::SleepUntil(u64 ticks)
{
	// This is definitely sub-optimal, but there's no way to sleep until a QPC timestamp on Win32.
	const s64 diff = static_cast<s64>(ticks - GetCPUTicks());
	if (diff <= 0)
		return;

	const HANDLE hTimer = GetSleepTimer();
	if (!hTimer)
		return;

	const u64 one_hundred_nanos_diff = (static_cast<u64>(diff) * 10000000ULL) / GetTickFrequency();
	if (one_hundred_nanos_diff == 0)
		return;

	LARGE_INTEGER fti;
	fti.QuadPart = -static_cast<s64>(one_hundred_nanos_diff);

	if (SetWaitableTimer(hTimer, &fti, 0, nullptr, nullptr, FALSE))
	{
		WaitForSingleObject(hTimer, INFINITE);
		return;
	}
}

#endif
