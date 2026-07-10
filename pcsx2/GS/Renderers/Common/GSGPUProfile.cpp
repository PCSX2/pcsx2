// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Common/GSGPUProfile.h"

#include <array>
#include <cctype>
#include <initializer_list>

#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

namespace
{
static std::string ToLowerASCII(std::string_view value)
{
	std::string lowered;
	lowered.reserve(value.size());

	for (const char ch : value)
		lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));

	return lowered;
}

static bool Contains(std::string_view haystack, std::string_view needle)
{
	return (haystack.find(needle) != std::string_view::npos);
}

static bool ContainsAny(std::string_view haystack, std::initializer_list<const char*> needles)
{
	for (const char* needle : needles)
	{
		if (Contains(haystack, needle))
			return true;
	}

	return false;
}

static void AppendHint(std::string& hints, std::string_view key, std::string_view value)
{
	if (value.empty())
		return;

	if (!hints.empty())
		hints.append(" | ");

	if (!key.empty())
	{
		hints.append(key);
		hints.push_back('=');
	}

	hints.append(value);
}

#if defined(__ANDROID__)
static std::string GetAndroidProperty(const char* name)
{
	std::array<char, PROP_VALUE_MAX> value = {};
	const int length = __system_property_get(name, value.data());
	return (length > 0) ? std::string(value.data(), static_cast<size_t>(length)) : std::string();
}
#endif

static std::string BuildHints(std::string_view gpu_vendor, std::string_view gpu_renderer_or_name)
{
	std::string hints;
	AppendHint(hints, "gpu_vendor", gpu_vendor);
	AppendHint(hints, "gpu", gpu_renderer_or_name);

#if defined(__ANDROID__)
	static constexpr const char* property_names[] = {
		"ro.soc.manufacturer",
		"ro.soc.model",
		"ro.soc.platform",
		"ro.board.platform",
		"ro.hardware",
		"ro.product.board",
		"ro.product.cpu.abi",
		"ro.vendor.product.cpu.abilist",
	};

	for (const char* property_name : property_names)
		AppendHint(hints, property_name, GetAndroidProperty(property_name));
#endif

	return hints;
}

static bool LooksLikeAdreno(std::string_view lowered_hints)
{
	const bool has_adreno = ContainsAny(lowered_hints, {"adreno"});
	const bool has_qualcomm = ContainsAny(lowered_hints, {"qualcomm", "qcom", "snapdragon"});
	return (has_adreno || has_qualcomm);
}

static bool LooksLikePowerVR(std::string_view lowered_hints)
{
	// "imagination" is unambiguous. "powervr" appears in PowerVR renderer strings
	// (e.g. "PowerVR B-Series BXM-8-256"). "img" is a common Imagination prefix
	// in SoC manifests (Mediatek MT68xx/MT69xx use "ro.soc.manufacturer=Mediatek"
	// with "img" markers on some boards). We deliberately do NOT match bare "vr"
	// to avoid false positives.
	return ContainsAny(lowered_hints, {"imagination", "powervr", "img"});
}

static bool LooksLikeMali(std::string_view lowered_hints)
{
	// "arm" alone is too broad (it matches the CPU ABI string "arm64-v8a") — gate
	// on Mali-specific markers. "valhall"/"bifrost"/"midgard" are Mali GPU arches.
	return ContainsAny(lowered_hints, {"mali", "valhall", "bifrost", "midgard"});
}
} // namespace

GpuProfileOverride GpuProfileDetector::ParseOverride(std::string_view value)
{
	const std::string lowered = ToLowerASCII(value);
	if (lowered == "mali")
		return GpuProfileOverride::Mali;
	if (lowered == "adreno")
		return GpuProfileOverride::Adreno;
	if (lowered == "powervr")
		return GpuProfileOverride::PowerVR;

	return GpuProfileOverride::Auto;
}

const char* GpuProfileDetector::OverrideToConfigString(GpuProfileOverride value)
{
	switch (value)
	{
		case GpuProfileOverride::Mali:
			return "mali";
		case GpuProfileOverride::Adreno:
			return "adreno";
		case GpuProfileOverride::PowerVR:
			return "powervr";
		case GpuProfileOverride::Auto:
		default:
			return "auto";
	}
}

const char* GpuProfileDetector::OverrideToString(GpuProfileOverride value)
{
	switch (value)
	{
		case GpuProfileOverride::Mali:
			return "Force Mali";
		case GpuProfileOverride::Adreno:
			return "Force Adreno";
		case GpuProfileOverride::PowerVR:
			return "Force PowerVR";
		case GpuProfileOverride::Auto:
		default:
			return "Auto";
	}
}

const char* GpuProfileDetector::RuntimeProfileToString(RuntimeGpuProfile value)
{
	switch (value)
	{
		case RuntimeGpuProfile::Mali:
			return "Mali";
		case RuntimeGpuProfile::PowerVR:
			return "PowerVR";
		case RuntimeGpuProfile::Adreno:
		default:
			return "Adreno";
	}
}

GpuProfileSelection GpuProfileDetector::Resolve(std::string_view override_value, std::string_view gpu_vendor,
	std::string_view gpu_renderer_or_name)
{
	GpuProfileSelection selection;
	selection.override_mode = ParseOverride(override_value);
	selection.hints = BuildHints(gpu_vendor, gpu_renderer_or_name);

	if (selection.override_mode == GpuProfileOverride::Mali)
	{
		selection.runtime_profile = RuntimeGpuProfile::Mali;
		return selection;
	}

	if (selection.override_mode == GpuProfileOverride::Adreno)
	{
		selection.runtime_profile = RuntimeGpuProfile::Adreno;
		return selection;
	}

	if (selection.override_mode == GpuProfileOverride::PowerVR)
	{
		selection.runtime_profile = RuntimeGpuProfile::PowerVR;
		return selection;
	}

	const std::string lowered_hints = ToLowerASCII(selection.hints);

#if defined(__ANDROID__)
	if (LooksLikeAdreno(lowered_hints))
	{
		selection.runtime_profile = RuntimeGpuProfile::Adreno;
	}
	else if (LooksLikePowerVR(lowered_hints))
	{
		selection.runtime_profile = RuntimeGpuProfile::PowerVR;
	}
	else if (LooksLikeMali(lowered_hints))
	{
		selection.runtime_profile = RuntimeGpuProfile::Mali;
	}
	else
	{
		// No vendor/renderer string matched. Mali used to be the catch-all on this
		// fork but that classified Imagination/PowerVR — which only has EXT fbfetch,
		// not ARM — as Mali and broke device init. EXT-style fbfetch is the de-facto
		// standard on every modern non-Mali mobile GPU, so default unknowns to
		// Adreno profile instead.
		selection.runtime_profile = RuntimeGpuProfile::Adreno;
	}
#else
	selection.runtime_profile = RuntimeGpuProfile::Adreno;
#endif

	return selection;
}
