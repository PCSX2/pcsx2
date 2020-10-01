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


#define NOMINMAX

extern bool psxmode;

struct StereoOut16;
struct StereoOut32;
struct StereoOutFloat;

struct V_Core;

namespace soundtouch
{
	class SoundTouch;
}

#include <assert.h>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <ctime>
#include <stdexcept>

#include "Utilities/Dependencies.h"
#include "Pcsx2Defs.h"
#include "Pcsx2Types.h"

//////////////////////////////////////////////////////////////////////////
// Override Win32 min/max macros with the STL's type safe and macro
// free varieties (much safer!)

#undef min
#undef max

template <typename T>
static __forceinline void Clampify(T& src, T min, T max)
{
	src = std::min(std::max(src, min), max);
}

template <typename T>
static __forceinline T GetClamped(T src, T min, T max)
{
	return std::min(std::max(src, min), max);
}

#ifdef __WXMAC__
#else
extern void SysMessage(const char* fmt, ...);
#endif
extern void SysMessage(const wchar_t* fmt, ...);

//////////////////////////////////////////////////////////////
// Dev / Debug conditionals --
//   Consts for using if() statements instead of uglier #ifdef macros.
//   Abbreviated macros for dev/debug only consoles and msgboxes.

#ifdef PCSX2_DEVBUILD
#define DevMsg MsgBox
#else
#define DevMsg
#endif

#ifdef PCSX2_DEVBUILD
#define SPU2_LOG
#endif

// Uncomment to enable debug keys on numpad (0 to 5)
//#define DEBUG_KEYS

#include "Utilities/Exceptions.h"
#include "Utilities/SafeArray.h"

#include "defs.h"
#include "regs.h"

#include "Config.h"
#include "Debug.h"
#include "Mixer.h"
#include "SndOut.h"
