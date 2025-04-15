// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/HostSys.h"
#include "common/RedtapeWindows.h"
#include "common/StringUtil.h"
#include "common/Threading.h"
#include "common/WindowInfo.h"

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

u64 GetAvailablePhysicalMemory()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return status.ullAvailPhys;
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

bool Common::InhibitScreensaver(bool inhibit)
{
	EXECUTION_STATE flags = ES_CONTINUOUS;
	if (inhibit)
		flags |= ES_DISPLAY_REQUIRED;
	SetThreadExecutionState(flags);
	return true;
}

void Common::SetMousePosition(int x, int y)
{
	SetCursorPos(x, y);
}

/*
static HHOOK mouseHook = nullptr;
static std::function<void(int, int)> fnMouseMoveCb;
LRESULT CALLBACK Mousecb(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0 && wParam == WM_MOUSEMOVE)
	{
		MSLLHOOKSTRUCT* mouse = (MSLLHOOKSTRUCT*)lParam;
		fnMouseMoveCb(mouse->pt.x, mouse->pt.y);
	}
	return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}
*/

// This (and the above) works, but is not recommended on Windows and is only here for consistency.
// Defer to using raw input instead.
bool Common::AttachMousePositionCb(std::function<void(int, int)> cb)
{
	/*
		if (mouseHook)
			Common::DetachMousePositionCb();

		fnMouseMoveCb = cb;
		mouseHook = SetWindowsHookEx(WH_MOUSE_LL, Mousecb, GetModuleHandle(NULL), 0);
		if (!mouseHook)
		{
			Console.Warning("Failed to set mouse hook: %d", GetLastError());
			return false;
		}

		#if defined(PCSX2_DEBUG) || defined(PCSX2_DEVBUILD)
			static bool warned = false;
			if (!warned)
			{
				Console.Warning("Mouse hooks are enabled, and this isn't a release build! Using a debugger, or loading symbols, _will_ stall the hook and cause global mouse lag.");
				warned = true;
			}
		#endif
	*/
	return true;
}

void Common::DetachMousePositionCb()
{
	/*
		UnhookWindowsHookEx(mouseHook);
		mouseHook = nullptr;
	*/
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

