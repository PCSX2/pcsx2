// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#ifndef __OBJC__
	#error "This header is for use with Objective-C++ only.
#endif

#ifdef __APPLE__

#include "common/MRCHelpers.h"
#include "common/Pcsx2Types.h"
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
		bool has_fast_half;
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
