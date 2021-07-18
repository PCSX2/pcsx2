/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"

#include "WindowInfo.h"
#include "Console.h"

#if defined(_WIN32) && !defined(_UWP)

#include "RedtapeWindows.h"
#include <dwmapi.h>

static bool GetRefreshRateFromDWM(HWND hwnd, float* refresh_rate)
{
	static HMODULE dwm_module = nullptr;
	static HRESULT(STDAPICALLTYPE * is_composition_enabled)(BOOL * pfEnabled) = nullptr;
	static HRESULT(STDAPICALLTYPE * get_timing_info)(HWND hwnd, DWM_TIMING_INFO * pTimingInfo) = nullptr;
	static bool load_tried = false;
	if (!load_tried)
	{
		load_tried = true;
		dwm_module = LoadLibraryW(L"dwmapi.dll");
		if (dwm_module)
		{
			std::atexit([]() {
				FreeLibrary(dwm_module);
				dwm_module = nullptr;
			});
			is_composition_enabled =
				reinterpret_cast<decltype(is_composition_enabled)>(GetProcAddress(dwm_module, "DwmIsCompositionEnabled"));
			get_timing_info =
				reinterpret_cast<decltype(get_timing_info)>(GetProcAddress(dwm_module, "DwmGetCompositionTimingInfo"));
		}
	}

	BOOL composition_enabled;
	if (!is_composition_enabled || FAILED(is_composition_enabled(&composition_enabled) || !get_timing_info))
		return false;

	DWM_TIMING_INFO ti = {};
	ti.cbSize = sizeof(ti);
	HRESULT hr = get_timing_info(nullptr, &ti);
	if (SUCCEEDED(hr))
	{
		if (ti.rateRefresh.uiNumerator == 0 || ti.rateRefresh.uiDenominator == 0)
			return false;

		*refresh_rate = static_cast<float>(ti.rateRefresh.uiNumerator) / static_cast<float>(ti.rateRefresh.uiDenominator);
		return true;
	}

	return false;
}

static bool GetRefreshRateFromMonitor(HWND hwnd, float* refresh_rate)
{
	HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	if (!mon)
		return false;

	MONITORINFOEXW mi = {};
	mi.cbSize = sizeof(mi);
	if (GetMonitorInfoW(mon, &mi))
	{
		DEVMODEW dm = {};
		dm.dmSize = sizeof(dm);

		// 0/1 are reserved for "defaults".
		if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1)
		{
			*refresh_rate = static_cast<float>(dm.dmDisplayFrequency);
			return true;
		}
	}

	return false;
}

bool WindowInfo::QueryRefreshRateForWindow(const WindowInfo& wi, float* refresh_rate)
{
	if (wi.type != Type::Win32 || !wi.window_handle)
		return false;

	// Try DWM first, then fall back to integer values.
	const HWND hwnd = static_cast<HWND>(wi.window_handle);
	return GetRefreshRateFromDWM(hwnd, refresh_rate) || GetRefreshRateFromMonitor(hwnd, refresh_rate);
}

#else

#if defined(X11_API) && defined(HAS_XRANDR)

#include "common/ScopedGuard.h"
#include <X11/extensions/Xrandr.h>
#include <X11/Xlib.h>

static bool GetRefreshRateFromXRandR(const WindowInfo& wi, float* refresh_rate)
{
	Display* display = static_cast<Display*>(wi.display_connection);
	Window window = static_cast<Window>(reinterpret_cast<uintptr_t>(wi.window_handle));
	if (!display || !window)
		return false;

	XRRScreenResources* res = XRRGetScreenResources(display, window);
	if (!res)
	{
		Console.Error("(GetRefreshRateFromXRandR) XRRGetScreenResources() failed");
		return false;
	}

	ScopedGuard res_guard([res]() { XRRFreeScreenResources(res); });

	int num_monitors;
	XRRMonitorInfo* mi = XRRGetMonitors(display, window, True, &num_monitors);
	if (num_monitors < 0)
	{
		Console.Error("(GetRefreshRateFromXRandR) XRRGetMonitors() failed");
		return false;
	}
	else if (num_monitors > 1)
	{
		Console.Warning("(GetRefreshRateFromXRandR) XRRGetMonitors() returned %d monitors, using first", num_monitors);
	}

	ScopedGuard mi_guard([mi]() { XRRFreeMonitors(mi); });
	if (mi->noutput <= 0)
	{
		Console.Error("(GetRefreshRateFromXRandR) Monitor has no outputs");
		return false;
	}
	else if (mi->noutput > 1)
	{
		Console.Warning("(GetRefreshRateFromXRandR) Monitor has %d outputs, using first", mi->noutput);
	}

	XRROutputInfo* oi = XRRGetOutputInfo(display, res, mi->outputs[0]);
	if (!oi)
	{
		Console.Error("(GetRefreshRateFromXRandR) XRRGetOutputInfo() failed");
		return false;
	}

	ScopedGuard oi_guard([oi]() { XRRFreeOutputInfo(oi); });

	XRRCrtcInfo* ci = XRRGetCrtcInfo(display, res, oi->crtc);
	if (!ci)
	{
		Console.Error("(GetRefreshRateFromXRandR) XRRGetCrtcInfo() failed");
		return false;
	}

	ScopedGuard ci_guard([ci]() { XRRFreeCrtcInfo(ci); });

	XRRModeInfo* mode = nullptr;
	for (int i = 0; i < res->nmode; i++)
	{
		if (res->modes[i].id == ci->mode)
		{
			mode = &res->modes[i];
			break;
		}
	}
	if (!mode)
	{
		Console.Error("(GetRefreshRateFromXRandR) Failed to look up mode %d (of %d)", static_cast<int>(ci->mode), res->nmode);
		return false;
	}

	if (mode->dotClock == 0 || mode->hTotal == 0 || mode->vTotal == 0)
	{
		Console.Error("(GetRefreshRateFromXRandR) Modeline is invalid: %ld/%d/%d", mode->dotClock, mode->hTotal, mode->vTotal);
		return false;
	}

	*refresh_rate =
		static_cast<double>(mode->dotClock) / (static_cast<double>(mode->hTotal) * static_cast<double>(mode->vTotal));
	return true;
}

#endif // X11_API && defined(HAS_XRANDR)

bool WindowInfo::QueryRefreshRateForWindow(const WindowInfo& wi, float* refresh_rate)
{
#if defined(X11_API) && defined(HAS_XRANDR)
	if (wi.type == WindowInfo::Type::X11)
		return GetRefreshRateFromXRandR(wi, refresh_rate);
#endif

	return false;
}

#endif
