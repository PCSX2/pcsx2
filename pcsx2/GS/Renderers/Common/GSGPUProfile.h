// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
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
	Unknown,
	Mali,
	Adreno,
	PowerVR,
};

enum class MobileGpuArchitecture : u8
{
	Unknown,
	Adreno2xx,
	Adreno3xx,
	Adreno4xx,
	Adreno5xx,
	Adreno6xx,
	Adreno7xx,
	Adreno8xx,
	AdrenoX,
	MaliUtgard,
	MaliMidgard,
	MaliBifrost,
	MaliValhall1,
	MaliValhall2,
	MaliValhall3,
	MaliFifthGen,
	MaliG1,
	PowerVR,
};

struct MobileGsTuning
{
	bool constrained = true;
	bool prefer_new_textures = false;
	bool force_partial_texture_preloading = true;
	u32 pooled_targets = 96;
	u32 target_age = 8;
	u32 pooled_textures = 96;
	u32 texture_age = 6;
};

struct MobileGpuIdentity
{
	MobileGpuArchitecture architecture = MobileGpuArchitecture::Unknown;
	u16 model_number = 0;
	u8 core_count = 0;
	bool recognized = false;
	std::string name = "Unknown";
};

struct GpuProfileSelection
{
	GpuProfileOverride override_mode = GpuProfileOverride::Auto;
	RuntimeGpuProfile runtime_profile = RuntimeGpuProfile::Unknown;
	bool is_mediatek_soc = false;
	MobileGpuIdentity gpu;
	MobileGsTuning gs_tuning;
	std::string hints;
};

class GpuProfileDetector
{
public:
	static GpuProfileOverride ParseOverride(std::string_view value);
	static const char* OverrideToConfigString(GpuProfileOverride value);
	static const char* OverrideToString(GpuProfileOverride value);
	static const char* RuntimeProfileToString(RuntimeGpuProfile value);
	static const char* ArchitectureToString(MobileGpuArchitecture value);

	static GpuProfileSelection Resolve(std::string_view override_value, std::string_view gpu_vendor,
		std::string_view gpu_renderer_or_name);
};
