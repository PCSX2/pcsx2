// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Common/GSGPUProfilePrivate.h"

#include <array>
#include <cctype>

#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

namespace GpuProfileDetail
{
std::string ToLowerASCII(std::string_view value)
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

bool ContainsAny(std::string_view haystack, std::initializer_list<const char*> needles)
{
	for (const char* needle : needles)
	{
		if (Contains(haystack, needle))
			return true;
	}

	return false;
}

MobileGsTuning MakeMobileGsTuning(u32 pooled_targets, u32 target_age, u32 pooled_textures, u32 texture_age,
	bool prefer_new_textures)
{
	MobileGsTuning tuning;
	tuning.pooled_targets = pooled_targets;
	tuning.target_age = target_age;
	tuning.pooled_textures = pooled_textures;
	tuning.texture_age = texture_age;
	tuning.constrained = (pooled_targets < 128 || pooled_textures < 128);
	tuning.prefer_new_textures = prefer_new_textures;
	tuning.force_partial_texture_preloading = tuning.constrained;
	return tuning;
}

MobileGsTuning MakeConservativeMobileGsTuning()
{
	return MakeMobileGsTuning(96, 8, 96, 6, false);
}
} // namespace GpuProfileDetail

namespace
{
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
		"ro.hardware.chipname",
		"ro.chipname",
		"ro.product.board",
		"ro.product.manufacturer",
		"ro.product.model",
		"ro.vendor.product.manufacturer",
		"ro.vendor.product.model",
		"ro.mediatek.platform",
		"ro.vendor.mediatek.platform",
		"ro.product.cpu.abi",
		"ro.vendor.product.cpu.abilist",
	};

	for (const char* property_name : property_names)
		AppendHint(hints, property_name, GetAndroidProperty(property_name));
#endif

	return hints;
}

static bool LooksLikeMediaTekSoc(std::string_view lowered_hints)
{
	if (GpuProfileDetail::ContainsAny(lowered_hints, {"mediatek", "dimensity", "helio"}))
		return true;

	// MediaTek board/platform properties commonly use compact part numbers such as mt6877 or
	// mt6989z without spelling out the vendor. Require a token boundary and four digits to avoid
	// treating an unrelated occurrence of "mt" as a chipset identifier.
	for (size_t i = 0; i + 6 <= lowered_hints.size(); i++)
	{
		if (lowered_hints[i] != 'm' || lowered_hints[i + 1] != 't' ||
			(i > 0 && std::isalnum(static_cast<unsigned char>(lowered_hints[i - 1]))))
		{
			continue;
		}

		bool has_four_digits = true;
		for (size_t digit = i + 2; digit < i + 6; digit++)
			has_four_digits &= (std::isdigit(static_cast<unsigned char>(lowered_hints[digit])) != 0);

		if (has_four_digits)
			return true;
	}

	return false;
}
} // namespace

GpuProfileOverride GpuProfileDetector::ParseOverride(std::string_view value)
{
	const std::string lowered = GpuProfileDetail::ToLowerASCII(value);
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
			return "Adreno";
		case RuntimeGpuProfile::Unknown:
		default:
			return "Unknown";
	}
}

const char* GpuProfileDetector::ArchitectureToString(MobileGpuArchitecture value)
{
	switch (value)
	{
		case MobileGpuArchitecture::Adreno2xx: return "Adreno 2xx";
		case MobileGpuArchitecture::Adreno3xx: return "Adreno 3xx";
		case MobileGpuArchitecture::Adreno4xx: return "Adreno 4xx";
		case MobileGpuArchitecture::Adreno5xx: return "Adreno 5xx";
		case MobileGpuArchitecture::Adreno6xx: return "Adreno 6xx";
		case MobileGpuArchitecture::Adreno7xx: return "Adreno 7xx";
		case MobileGpuArchitecture::Adreno8xx: return "Adreno 8xx";
		case MobileGpuArchitecture::AdrenoX: return "Adreno X";
		case MobileGpuArchitecture::MaliUtgard: return "Mali Utgard";
		case MobileGpuArchitecture::MaliMidgard: return "Mali Midgard";
		case MobileGpuArchitecture::MaliBifrost: return "Mali Bifrost";
		case MobileGpuArchitecture::MaliValhall1: return "Mali Valhall (1st Gen)";
		case MobileGpuArchitecture::MaliValhall2: return "Mali Valhall (2nd Gen)";
		case MobileGpuArchitecture::MaliValhall3: return "Mali Valhall (3rd Gen)";
		case MobileGpuArchitecture::MaliFifthGen: return "Arm 5th Gen";
		case MobileGpuArchitecture::MaliG1: return "Arm Mali G1";
		case MobileGpuArchitecture::PowerVR: return "PowerVR";
		case MobileGpuArchitecture::Unknown:
		default:
			return "Unknown";
	}
}

static void ApplyResolvedProfile(GpuProfileSelection& selection, RuntimeGpuProfile runtime_profile,
	GpuProfileDetail::ResolvedGpuProfile&& resolved)
{
	selection.runtime_profile = runtime_profile;
	selection.gpu = std::move(resolved.gpu);
	selection.gs_tuning = resolved.tuning;
}

GpuProfileSelection GpuProfileDetector::Resolve(std::string_view override_value, std::string_view gpu_vendor,
	std::string_view gpu_renderer_or_name)
{
	GpuProfileSelection selection;
	selection.override_mode = ParseOverride(override_value);
	selection.hints = BuildHints(gpu_vendor, gpu_renderer_or_name);
	const std::string lowered_hints = GpuProfileDetail::ToLowerASCII(selection.hints);
	const std::string lowered_override = GpuProfileDetail::ToLowerASCII(override_value);
	selection.is_mediatek_soc = (lowered_override == "mediatek") || LooksLikeMediaTekSoc(lowered_hints);
	selection.gs_tuning = GpuProfileDetail::MakeConservativeMobileGsTuning();

	if (selection.override_mode == GpuProfileOverride::Mali)
	{
		ApplyResolvedProfile(selection, RuntimeGpuProfile::Mali, GpuProfileDetail::ResolveMaliProfile(lowered_hints));
		return selection;
	}

	if (selection.override_mode == GpuProfileOverride::Adreno)
	{
		ApplyResolvedProfile(selection, RuntimeGpuProfile::Adreno, GpuProfileDetail::ResolveAdrenoProfile(lowered_hints));
		return selection;
	}

	if (selection.override_mode == GpuProfileOverride::PowerVR)
	{
		ApplyResolvedProfile(selection, RuntimeGpuProfile::PowerVR, GpuProfileDetail::ResolvePowerVRProfile(lowered_hints));
		return selection;
	}

	if (GpuProfileDetail::LooksLikeAdreno(lowered_hints))
	{
		ApplyResolvedProfile(selection, RuntimeGpuProfile::Adreno, GpuProfileDetail::ResolveAdrenoProfile(lowered_hints));
	}
	else if (GpuProfileDetail::LooksLikePowerVR(lowered_hints))
	{
		ApplyResolvedProfile(selection, RuntimeGpuProfile::PowerVR, GpuProfileDetail::ResolvePowerVRProfile(lowered_hints));
	}
	else if (GpuProfileDetail::LooksLikeMali(lowered_hints))
	{
		ApplyResolvedProfile(selection, RuntimeGpuProfile::Mali, GpuProfileDetail::ResolveMaliProfile(lowered_hints));
	}

	return selection;
}
