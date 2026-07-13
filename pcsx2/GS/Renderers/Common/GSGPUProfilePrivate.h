// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/Renderers/Common/GSGPUProfile.h"

#include <initializer_list>
#include <string>
#include <string_view>

namespace GpuProfileDetail
{
struct ResolvedGpuProfile
{
	MobileGpuIdentity gpu;
	MobileGsTuning tuning;
};

std::string ToLowerASCII(std::string_view value);
bool ContainsAny(std::string_view haystack, std::initializer_list<const char*> needles);

MobileGsTuning MakeMobileGsTuning(u32 pooled_targets, u32 target_age, u32 pooled_textures, u32 texture_age,
	bool prefer_new_textures = false);
MobileGsTuning MakeConservativeMobileGsTuning();

bool LooksLikeAdreno(std::string_view lowered_hints);
ResolvedGpuProfile ResolveAdrenoProfile(std::string_view lowered_hints);

bool LooksLikeMali(std::string_view lowered_hints);
ResolvedGpuProfile ResolveMaliProfile(std::string_view lowered_hints);

bool LooksLikePowerVR(std::string_view lowered_hints);
ResolvedGpuProfile ResolvePowerVRProfile(std::string_view lowered_hints);
} // namespace GpuProfileDetail
