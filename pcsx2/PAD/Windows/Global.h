/*  LilyPad - Pad plugin for PS2 Emulator
 *  Copyright (C) 2002-2014  PCSX2 Dev Team/ChickenLiver
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the
 *  terms of the GNU Lesser General Public License as published by the Free
 *  Software Found- ation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with PCSX2.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

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
void wsprintfW(Array &buf, const wchar_t *format, ...)
{
    va_list a;
    va_start(a, format);

    vswprintf(buf, sizeof(buf) / sizeof(buf[0]), format, a);

    va_end(a);
}

template <typename Array>
void wsprintf(Array &buf, const wchar_t *format, ...)
{
    va_list a;
    va_start(a, format);

    vswprintf(buf, sizeof(buf) / sizeof(buf[0]), format, a);

    va_end(a);
}

static inline int wcsicmp(const wchar_t *w1, const wchar_t *w2)
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

extern Display *GSdsp;
extern Window GSwin;

#endif

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifdef _MSC_VER
#define EXPORT_C_(type) extern "C" type CALLBACK
#else
#define EXPORT_C_(type) extern "C" __attribute__((stdcall, externally_visible, visibility("default"))) type CALLBACK
#endif

#ifdef _MSC_VER
#define _WIN32_WINNT 0x0600
#define NOMINMAX
#include <algorithm>
#include <windows.h>

#ifdef PCSX2_DEBUG
#define _CRTDBG_MAPALLOC
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#else
#include <stdlib.h>
#endif

#else

#include <stdlib.h>
#include <mutex>

#endif

#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef _MSC_VER
#include <commctrl.h>
// Only needed for DBT_DEVNODES_CHANGED
#include <Dbt.h>
#endif

#include "PS2Edefs.h"

#ifdef _MSC_VER
extern HINSTANCE hInst;
#endif
// Needed for config screen
void GetNameAndVersionString(wchar_t *out);

typedef struct
{
    unsigned char controllerType;
    unsigned short buttonStatus;
    unsigned char rightJoyX, rightJoyY, leftJoyX, leftJoyY;
    unsigned char moveX, moveY;
    unsigned char reserved[91];
} PadDataS;

EXPORT_C_(void)
PADupdate(int pad);
EXPORT_C_(u32)
PS2EgetLibType(void);
EXPORT_C_(u32)
PS2EgetLibVersion2(u32 type);
EXPORT_C_(char *)
PS2EgetLibName(void);
EXPORT_C_(void)
PADshutdown();
EXPORT_C_(s32)
PADinit(u32 flags);
EXPORT_C_(s32)
PADopen(void *pDsp);
EXPORT_C_(void)
PADclose();
EXPORT_C_(u8)
PADstartPoll(int pad);
EXPORT_C_(u8)
PADpoll(u8 value);
EXPORT_C_(u32)
PADquery();
EXPORT_C_(void)
PADabout();
EXPORT_C_(s32)
PADtest();
EXPORT_C_(keyEvent *)
PADkeyEvent();
EXPORT_C_(u32)
PADreadPort1(PadDataS *pads);
EXPORT_C_(u32)
PADreadPort2(PadDataS *pads);
EXPORT_C_(void)
PADconfigure();
EXPORT_C_(s32)
PADfreeze(int mode, freezeData *data);
EXPORT_C_(s32)
PADsetSlot(u8 port, u8 slot);
EXPORT_C_(s32)
PADqueryMtap(u8 port);
EXPORT_C_(void)
PADsetSettingsDir(const char *dir);
