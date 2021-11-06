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
};
