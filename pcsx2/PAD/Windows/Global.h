/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include <wx/string.h>
#include <wx/tokenzr.h>
#include <wx/intl.h>
#include <wx/log.h>
#include <wx/filename.h>
#include "common/Console.h"
#include "common/RedtapeWindows.h"
#include <stdio.h>
#include <assert.h>

#include <array>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <mutex>
#include <queue>

#include "../Gamepad.h"

struct HostKeyEvent;

#ifdef __linux__
// Seriously why there is no standard
#include "stdint.h"
typedef uint32_t DWORD;
typedef uint16_t USHORT;
#ifndef __INTEL_COMPILER
typedef int64_t __int64;
#endif

#define MAX_PATH (256) // random value

#include <X11/keysym.h>

#define VK_SHIFT XK_Shift_L
#define VK_LSHIFT XK_Shift_L
#define VK_RSHIFT XK_Shift_R
#define VK_LMENU XK_Menu
#define VK_RMENU XK_Menu
#define VK_MENU XK_Menu
#define VK_CONTROL XK_Control_L
#define VK_TAB XK_Tab
#define VK_ESCAPE XK_Escape
#define VK_F4 XK_F4

#include <cwchar>
#include <cstdarg>

template <typename Array>
void wsprintfW(Array& buf, const wchar_t* format, ...)
{
	va_list a;
	va_start(a, format);

	vswprintf(buf, sizeof(buf) / sizeof(buf[0]), format, a);

	va_end(a);
}

template <typename Array>
void wsprintf(Array& buf, const wchar_t* format, ...)
{
	va_list a;
	va_start(a, format);

	vswprintf(buf, sizeof(buf) / sizeof(buf[0]), format, a);

	va_end(a);
}

static inline int wcsicmp(const wchar_t* w1, const wchar_t* w2)
{
	// I didn't find a way to put ignore case ...
	return wcscmp(w1, w2);
}

#include <sys/time.h>
static inline unsigned int timeGetTime()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	uint64_t ms = (now.tv_usec / 1000) + ((uint64_t)now.tv_sec * 1000);
	return (ms & 0xFFFFFFFF); // MS code is u32 ...
}

#include "Utilities/Dependencies.h"
#include "Utilities/StringHelpers.h"
#include "Utilities/Path.h"

#include <X11/Xutil.h>

extern Display* GSdsp;
extern Window GSwin;

#endif

#ifdef _MSC_VER
#include <commctrl.h>
#include <commdlg.h>
#include <Shlwapi.h>
#include <timeapi.h>
// Only needed for DBT_DEVNODES_CHANGED
#include <Dbt.h>
#endif

// Needed for config screen
void GetNameAndVersionString(wchar_t* out);
