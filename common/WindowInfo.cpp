// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "WindowInfo.h"
#include "Console.h"

#if defined(_WIN32)

#include "RedtapeWindows.h"
#include <dwmapi.h>

static bool GetRefreshRateFromDWM(HWND hwnd, float* refresh_rate)
{
	BOOL composition_enabled;
	if (FAILED(DwmIsCompositionEnabled(&composition_enabled)))
		return false;

	DWM_TIMING_INFO ti = {};
	ti.cbSize = sizeof(ti);
	HRESULT hr = DwmGetCompositionTimingInfo(nullptr, &ti);
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

#if defined(X11_API)

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

#endif // X11_API

bool WindowInfo::QueryRefreshRateForWindow(const WindowInfo& wi, float* refresh_rate)
{
#if defined(X11_API)
	if (wi.type == WindowInfo::Type::X11)
		return GetRefreshRateFromXRandR(wi, refresh_rate);
#endif

	return false;
}

#endif
