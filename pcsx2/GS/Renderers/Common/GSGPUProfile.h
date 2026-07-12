// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <string>
#include <string_view>

enum class GpuProfileOverride : u8
{
	Auto,
	Mali,
	Adreno,
	PowerVR,
};

enum class RuntimeGpuProfile : u8
{
	Mali,
	Adreno,
	PowerVR,
};

struct GpuProfileSelection
{
	GpuProfileOverride override_mode = GpuProfileOverride::Auto;
	RuntimeGpuProfile runtime_profile = RuntimeGpuProfile::Adreno;
	// True when the SoC hints look like a MediaTek chipset (Dimensity/Helio). Used
	// to disable the Vulkan framebuffer-fetch/ROAA path on MediaTek Mali stacks,
	// whose driver returns zero/stale destination color (black or missing textures)
	// across GPU generations. Ported from sashkinbro/EmuCoreX.
	bool is_mediatek_soc = false;
	std::string hints;
};

class GpuProfileDetector
{
public:
	static GpuProfileOverride ParseOverride(std::string_view value);
	static const char* OverrideToConfigString(GpuProfileOverride value);
	static const char* OverrideToString(GpuProfileOverride value);
	static const char* RuntimeProfileToString(RuntimeGpuProfile value);

	static GpuProfileSelection Resolve(std::string_view override_value, std::string_view gpu_vendor,
		std::string_view gpu_renderer_or_name);
};
