/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022 PCSX2 Dev Team
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

#ifndef __OBJC__
	#error "This header is for use with Objective-C++ only.
#endif

#ifdef __APPLE__

#include "PCSX2Base.h"
#include "common/MRCHelpers.h"
#include <Metal/Metal.h>

struct GSMTLDevice
{
	enum class MetalVersion : u8
	{
		Metal20, ///< Metal 2.0 (macOS 10.13, iOS 11)
		Metal21, ///< Metal 2.1 (macOS 10.14, iOS 12)
		Metal22, ///< Metal 2.2 (macOS 10.15, iOS 13)
		Metal23, ///< Metal 2.3 (macOS 11, iOS 14)
	};

	struct Features
	{
		bool unified_memory;
		bool texture_swizzle;
		bool framebuffer_fetch;
		bool primid;
		bool slow_color_compression; ///< Color compression seems to slow down rt read on AMD
		MetalVersion shader_version;
		int max_texsize;
	};

	MRCOwned<id<MTLDevice>> dev;
	MRCOwned<id<MTLLibrary>> shaders;
	Features features;

	GSMTLDevice() = default;
	explicit GSMTLDevice(MRCOwned<id<MTLDevice>> dev);

	bool IsOk() const { return dev && shaders; }
	void Reset()
	{
		dev = nullptr;
		shaders = nullptr;
	}
};

const char* to_string(GSMTLDevice::MetalVersion ver);

#endif // __APPLE__
