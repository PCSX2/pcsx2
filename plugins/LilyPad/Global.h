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

// Includes Windows.h, has inlined versions of memory allocation and
// string comparison functions needed to avoid using CRT.  This reduces
// dll size by over 100k while avoiding any dependencies on updated CRT dlls.
#pragma once

#ifdef __linux__
// Seriously why there is no standard
#include "stdint.h"
typedef uint32_t DWORD;
typedef uint16_t USHORT;
typedef int64_t  __int64;

#define MAX_PATH (256) // random value

#include <X11/keysym.h>

#define   VK_SHIFT     XK_Shift_L
#define   VK_LSHIFT    XK_Shift_L
#define   VK_RSHIFT    XK_Shift_R
#define   VK_LMENU     XK_Menu
#define   VK_RMENU     XK_Menu
#define   VK_MENU      XK_Menu
#define   VK_CONTROL   XK_Control_L
#define   VK_TAB       XK_Tab
#define   VK_ESCAPE    XK_Escape
#define   VK_F4        XK_F4

#include <cwchar>
#include <cstdarg>

template <typename Array>
void wsprintfW(Array& buf, const wchar_t *format, ...) {
	va_list a;
	va_start(a, format);

	vswprintf(buf, sizeof(buf)/sizeof(buf[0]), format, a);

	va_end(a);
}

template <typename Array>
void wsprintf(Array& buf, const wchar_t *format, ...) {
	va_list a;
	va_start(a, format);

	vswprintf(buf, sizeof(buf)/sizeof(buf[0]), format, a);

	va_end(a);
}

static inline int wcsicmp(const wchar_t* w1, const wchar_t* w2) {
	// I didn't find a way to put ignore case ...
	return wcscmp(w1, w2);
}

#include <sys/time.h>
static inline unsigned int timeGetTime() {
	struct timeval now;
	gettimeofday(&now, NULL);
	uint64_t ms = (now.tv_usec/1000) + (now.tv_sec * 1000);
	return (ms & 0xFFFFFFFF); // MS code is u32 ...
}

#include "Utilities/Dependencies.h"
#include "Utilities/StringHelpers.h"
#include "Utilities/Path.h"

#include <X11/Xutil.h>

extern Display *GSdsp;
extern Window  GSwin;

#endif


#define DIRECTINPUT_VERSION 0x0800

#ifdef NO_CRT
#define _CRT_ALLOCATION_DEFINED

inline void * malloc(size_t size);
inline void * calloc(size_t num, size_t size);
inline void free(void * mem);
inline void * realloc(void *mem, size_t size);
#endif

#define UNICODE

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifdef _MSC_VER
#define EXPORT_C_(type) extern "C" __declspec(dllexport) type CALLBACK
#else
#define EXPORT_C_(type) extern "C" __attribute__((externally_visible,visibility("default"))) type CALLBACK
#endif

#ifdef _MSC_VER
// Actually works with 0x0400, but need 0x500 to get XBUTTON defines,
// 0x501 to get raw input structures, and 0x0600 to get WM_MOUSEHWHEEL.
#define WINVER 0x0600
#define _WIN32_WINNT WINVER
#define __MSCW32__


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

typedef struct {
	unsigned char controllerType;
	unsigned short buttonStatus;
	unsigned char rightJoyX, rightJoyY, leftJoyX, leftJoyY;
	unsigned char moveX, moveY;
	unsigned char reserved[91];
} PadDataS;

EXPORT_C_(void) PADupdate(int pad);
EXPORT_C_(u32) PS2EgetLibType(void);
EXPORT_C_(u32) PS2EgetLibVersion2(u32 type);
EXPORT_C_(char*) PSEgetLibName();
EXPORT_C_(char*) PS2EgetLibName(void);
EXPORT_C_(void) PADshutdown();
EXPORT_C_(s32) PADinit(u32 flags);
EXPORT_C_(s32) PADopen(void *pDsp);
EXPORT_C_(void) PADclose();
EXPORT_C_(u8) PADstartPoll(int pad);
EXPORT_C_(u8) PADpoll(u8 value);
EXPORT_C_(u32) PADquery();
EXPORT_C_(void) PADabout();
EXPORT_C_(s32) PADtest();
EXPORT_C_(keyEvent*) PADkeyEvent();
EXPORT_C_(u32) PADreadPort1 (PadDataS* pads);
EXPORT_C_(u32) PADreadPort2 (PadDataS* pads);
EXPORT_C_(u32) PSEgetLibType();
EXPORT_C_(u32) PSEgetLibVersion();
EXPORT_C_(void) PADconfigure();
EXPORT_C_(s32) PADfreeze(int mode, freezeData *data);
EXPORT_C_(s32) PADsetSlot(u8 port, u8 slot);
EXPORT_C_(s32) PADqueryMtap(u8 port);
EXPORT_C_(void) PADsetSettingsDir(const char *dir);

#ifdef NO_CRT

#define wcsdup MyWcsdup
#define wcsicmp MyWcsicmp

inline void * malloc(size_t size) {
	return HeapAlloc(GetProcessHeap(), 0, size);
}

inline void * calloc(size_t num, size_t size) {
	size *= num;
	void *out = malloc(size);
	if (out) memset(out, 0, size);
	return out;
}

inline void free(void * mem) {
	if (mem) HeapFree(GetProcessHeap(), 0, mem);
}

inline void * realloc(void *mem, size_t size) {
	if (!mem) {
		return malloc(size);
	}

	if (!size) {
		free(mem);
		return 0;
	}
	return HeapReAlloc(GetProcessHeap(), 0, mem, size);
}

inline void * __cdecl operator new(size_t lSize) {
	return HeapAlloc(GetProcessHeap(), 0, lSize);
}

inline void __cdecl operator delete(void *pBlock) {
	HeapFree(GetProcessHeap(), 0, pBlock);
}

inline wchar_t * __cdecl wcsdup(const wchar_t *in) {
	size_t size = sizeof(wchar_t) * (1+wcslen(in));
	wchar_t *out = (wchar_t*) malloc(size);
	if (out)
		memcpy(out, in, size);
	return out;
}

inline int __cdecl wcsicmp(const wchar_t *s1, const wchar_t *s2) {
	int res = CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE, s1, -1, s2, -1);
	if (res) return res-2;
	return res;
}

#endif
