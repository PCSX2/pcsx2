// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "Pcsx2Defs.h"

/// Contains the information required to create a graphics context in a window.
struct WindowInfo
{
	enum class Type
	{
		Surfaceless,
		Win32,
		X11,
		Wayland,
		MacOS
	};

	/// The type of the surface. Surfaceless indicates it will not be displayed on screen at all.
	Type type = Type::Surfaceless;

	/// Connection to the display server. On most platforms except X11/Wayland, this is implicit and null.
	void* display_connection = nullptr;

	/// Abstract handle to the window. This depends on the surface type.
	void* window_handle = nullptr;

	/// For platforms where a separate surface/layer handle is needed, it is stored here (e.g. MacOS).
	void* surface_handle = nullptr;

	/// Width of the surface in pixels.
	u32 surface_width = 0;

	/// Height of the surface in pixels.
	u32 surface_height = 0;

	/// DPI scale for the surface.
	float surface_scale = 1.0f;

	/// Refresh rate of the surface, if available.
	float surface_refresh_rate = 0.0f;

	/// Returns the host's refresh rate for the given window, if available.
	static bool QueryRefreshRateForWindow(const WindowInfo& wi, float* refresh_rate);

	/// Enables or disables the screen saver from starting.
	static bool InhibitScreensaver(const WindowInfo& wi, bool inhibit);
};
