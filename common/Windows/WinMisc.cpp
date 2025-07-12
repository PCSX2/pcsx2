// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/HostSys.h"
#include "common/Path.h"
#include "common/RedtapeWindows.h"
#include "common/StringUtil.h"
#include "common/Threading.h"
#include "common/WindowInfo.h"

#include "pcsx2/Host.h"
#include "fmt/format.h"

#include <Windows.h>
#include <shlobj.h>
#include <winnls.h>
#include <shobjidl.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <comdef.h>
#include <mmsystem.h>
#include <timeapi.h>
#include <VersionHelpers.h>

#include <wrl/client.h>

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

void Common::CreateShortcut(const std::string name, const std::string game_path, const std::string passed_cli_args, bool is_desktop)
{
	if (name.empty())
	{
		Console.Error("Cannot create shortcuts without a name.");
		return;
	}

	// Sanitize filename
	const std::string clean_name = Path::SanitizeFileName(name).c_str();
	if (!Path::IsValidFileName(clean_name))
	{
		Host::ReportErrorAsync(TRANSLATE_SV("WinMisc", "Failed to create shortcut"), TRANSLATE_SV("WinMisc", "Filename contains illegal character."));
		return;
	}
	
	// Locate home directory
	std::string link_file;
	if (const char* home = getenv("USERPROFILE"))
	{
		if (is_desktop)
		{
			link_file = Path::ToNativePath(fmt::format("{}/Desktop/{}.lnk", home, clean_name));
		}
		else
		{
			const std::string start_menu_dir = Path::ToNativePath(fmt::format("{}/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/PCSX2", home));
			if (!FileSystem::EnsureDirectoryExists(start_menu_dir.c_str(), false))
			{
				Host::ReportErrorAsync(TRANSLATE_SV("WinMisc", "Failed to create shortcut"), TRANSLATE_SV("WinMisc", "Could not create start menu directory."));
				return;
			}

			link_file = Path::ToNativePath(fmt::format("{}/{}.lnk", start_menu_dir, clean_name));
		}
	}
	else
	{
		Host::ReportErrorAsync(TRANSLATE_SV("WinMisc", "Failed to create shortcut"), TRANSLATE_SV("WinMisc", "Home path is empty."));
		return;
	}

	// Check if the same shortcut already exists
	if (FileSystem::FileExists(link_file.c_str()))
	{
		Host::ReportErrorAsync(TRANSLATE_SV("WinMisc", "Failed to create shortcut"), TRANSLATE_SV("WinMisc", "A shortcut with the same name already exist."));
		return;
	}

	const std::string final_args = fmt::format(" {} -- \"{}\"", StringUtil::StripWhitespace(passed_cli_args), game_path);
	Console.WriteLnFmt("Creating a shortcut '{}' with arguments '{}'", link_file, final_args);
	const auto str_error = [](HRESULT hr) -> std::string {
		_com_error err(hr);
		const TCHAR* errMsg = err.ErrorMessage();
		return fmt::format("{} [{}]", StringUtil::WideStringToUTF8String(errMsg), hr);
	};

	// Construct the shortcut
	// https://stackoverflow.com/questions/3906974/how-to-programmatically-create-a-shortcut-using-win32
	HRESULT res = CoInitialize(NULL);
	if (FAILED(res))
	{
		Console.ErrorFmt("Failed to create shortcut: CoInitialize failed ({})", str_error(res));
		return;
	}

	Microsoft::WRL::ComPtr<IShellLink> pShellLink;
	Microsoft::WRL::ComPtr<IPersistFile> pPersistFile;

	const auto cleanup = [&](bool return_value, const std::string& fail_reason) -> bool {
		if (!return_value)
			Console.ErrorFmt("Failed to create shortcut: {}", fail_reason);
		CoUninitialize();
		return return_value;
	};

	res = CoCreateInstance(__uuidof(ShellLink), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pShellLink));
	if (FAILED(res))
	{
		cleanup(false, "CoCreateInstance failed");
		return;
	}

	// Set path to the executable
	const std::wstring target_file = StringUtil::UTF8StringToWideString(FileSystem::GetProgramPath());
	res = pShellLink->SetPath(target_file.c_str());
	if (FAILED(res))
	{
		cleanup(false, fmt::format("SetPath failed ({})", str_error(res)));
		return;
	}

	// Set the working directory
	const std::wstring working_dir = StringUtil::UTF8StringToWideString(FileSystem::GetWorkingDirectory());
	res = pShellLink->SetWorkingDirectory(working_dir.c_str());
	if (FAILED(res))
	{
		cleanup(false, fmt::format("SetWorkingDirectory failed ({})", str_error(res)));
		return;
	}

	// Set the launch arguments
	if (!final_args.empty())
	{
		const std::wstring target_cli_args = StringUtil::UTF8StringToWideString(final_args);
		res = pShellLink->SetArguments(target_cli_args.c_str());
		if (FAILED(res))
		{
			cleanup(false, fmt::format("SetArguments failed ({})", str_error(res)));
			return;
		}
	}
	
	// Set the icon
	std::string icon_path = Path::ToNativePath(Path::Combine(Path::GetDirectory(FileSystem::GetProgramPath()), "resources/icons/AppIconLarge.ico"));
	const std::wstring w_icon_path = StringUtil::UTF8StringToWideString(icon_path);
	res = pShellLink->SetIconLocation(w_icon_path.c_str(), 0);
	if (FAILED(res))
	{
		cleanup(false, fmt::format("SetIconLocation failed ({})", str_error(res)));
		return;
	}

	// Use the IPersistFile object to save the shell link
	res = pShellLink.As(&pPersistFile);
	if (FAILED(res))
	{
		cleanup(false, fmt::format("QueryInterface failed ({})", str_error(res)));
		return;
	}

	// Save shortcut link to disk
	const std::wstring w_link_file = StringUtil::UTF8StringToWideString(link_file);
	res = pPersistFile->Save(w_link_file.c_str(), TRUE);
	if (FAILED(res))
	{
		cleanup(false, fmt::format("Failed to save the shortcut ({})", str_error(res)));
		return;
	}

	Console.WriteLnFmt(Color_StrongGreen, "{} shortcut for {} has been created succesfully.", is_desktop ? "Desktop" : "Start Menu", clean_name);
	cleanup(true, {});
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

