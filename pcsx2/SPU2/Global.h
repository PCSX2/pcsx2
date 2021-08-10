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

#include "PrecompiledHeader.h"

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

extern void SysMessage(const char* fmt, ...);
extern void SysMessage(const wchar_t* fmt, ...);

// Uncomment to enable debug keys on numpad (0 to 5)
//#define DEBUG_KEYS
#ifdef PCSX2_DEVBUILD
#define SPU2_LOG
#endif

#include "defs.h"
#include "regs.h"

#include "Config.h"
#include "Debug.h"
#include "Mixer.h"
#include "SndOut.h"
