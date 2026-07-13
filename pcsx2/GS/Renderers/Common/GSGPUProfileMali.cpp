// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Common/GSGPUProfilePrivate.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace GpuProfileDetail
{
namespace
{
struct MaliSpec
{
	char series;
	u16 model;
	MobileGpuArchitecture architecture;
	MobileGsTuning tuning;
};

constexpr MobileGsTuning T(u32 pool, u32 target_age, u32 texture_age, bool prefer_new = false)
{
	return MobileGsTuning{pool < 128, prefer_new, pool < 128, pool, target_age, pool, texture_age};
}

// Arm's public product families. Performance still depends heavily on MC/MP core count, which is applied below.
static constexpr std::array<MaliSpec, 40> s_mali_specs = {{
	{'U', 200, MobileGpuArchitecture::MaliUtgard, T(40, 4, 4)},
	{'U', 300, MobileGpuArchitecture::MaliUtgard, T(40, 4, 4)},
	{'U', 400, MobileGpuArchitecture::MaliUtgard, T(44, 4, 4)},
	{'U', 450, MobileGpuArchitecture::MaliUtgard, T(48, 4, 4)},
	{'U', 470, MobileGpuArchitecture::MaliUtgard, T(48, 4, 4)},
	{'T', 600, MobileGpuArchitecture::MaliMidgard, T(48, 4, 4)},
	{'T', 604, MobileGpuArchitecture::MaliMidgard, T(52, 4, 4)},
	{'T', 620, MobileGpuArchitecture::MaliMidgard, T(52, 4, 4)},
	{'T', 624, MobileGpuArchitecture::MaliMidgard, T(56, 5, 4)},
	{'T', 628, MobileGpuArchitecture::MaliMidgard, T(60, 5, 4)},
	{'T', 658, MobileGpuArchitecture::MaliMidgard, T(64, 5, 5)},
	{'T', 678, MobileGpuArchitecture::MaliMidgard, T(68, 5, 5)},
	{'T', 720, MobileGpuArchitecture::MaliMidgard, T(52, 4, 4)},
	{'T', 760, MobileGpuArchitecture::MaliMidgard, T(64, 5, 5)},
	{'T', 820, MobileGpuArchitecture::MaliMidgard, T(52, 4, 4)},
	{'T', 830, MobileGpuArchitecture::MaliMidgard, T(56, 5, 4)},
	{'T', 860, MobileGpuArchitecture::MaliMidgard, T(64, 5, 5)},
	{'T', 880, MobileGpuArchitecture::MaliMidgard, T(72, 6, 5)},
	{'G', 31, MobileGpuArchitecture::MaliBifrost, T(56, 5, 4)},
	{'G', 51, MobileGpuArchitecture::MaliBifrost, T(60, 5, 5)},
	{'G', 52, MobileGpuArchitecture::MaliBifrost, T(68, 5, 5)},
	{'G', 71, MobileGpuArchitecture::MaliBifrost, T(72, 6, 5)},
	{'G', 72, MobileGpuArchitecture::MaliBifrost, T(76, 6, 5)},
	{'G', 76, MobileGpuArchitecture::MaliBifrost, T(84, 7, 6)},
	{'G', 57, MobileGpuArchitecture::MaliValhall1, T(72, 6, 5)},
	{'G', 68, MobileGpuArchitecture::MaliValhall1, T(84, 7, 6)},
	{'G', 77, MobileGpuArchitecture::MaliValhall1, T(92, 7, 6)},
	{'G', 78, MobileGpuArchitecture::MaliValhall1, T(104, 8, 7)}, // G78AE shares the GS policy.
	{'G', 310, MobileGpuArchitecture::MaliValhall2, T(64, 5, 5)},
	{'G', 510, MobileGpuArchitecture::MaliValhall2, T(80, 6, 5)},
	{'G', 610, MobileGpuArchitecture::MaliValhall2, T(104, 8, 7)},
	{'G', 710, MobileGpuArchitecture::MaliValhall2, T(136, 10, 8, true)},
	{'G', 615, MobileGpuArchitecture::MaliValhall3, T(112, 8, 7)},
	{'G', 715, MobileGpuArchitecture::MaliValhall3, T(144, 10, 8, true)},
	{'G', 620, MobileGpuArchitecture::MaliFifthGen, T(104, 8, 7)},
	{'G', 720, MobileGpuArchitecture::MaliFifthGen, T(140, 10, 8, true)},
	{'G', 625, MobileGpuArchitecture::MaliFifthGen, T(112, 8, 7)},
	{'G', 725, MobileGpuArchitecture::MaliFifthGen, T(144, 10, 8, true)},
	{'G', 925, MobileGpuArchitecture::MaliFifthGen, T(160, 12, 8, true)},
	{'G', 1, MobileGpuArchitecture::MaliG1, T(144, 10, 8, true)},
}};

static bool ParseUnsigned(std::string_view text, size_t pos, u16* value, size_t* end)
{
	if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos])))
		return false;

	u32 parsed = 0;
	while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])))
	{
		parsed = parsed * 10 + static_cast<u32>(text[pos++] - '0');
		if (parsed > 9999)
			return false;
	}
	*value = static_cast<u16>(parsed);
	*end = pos;
	return true;
}

static bool ParseMaliModel(std::string_view hints, char* series, u16* model, bool* immortalis)
{
	const size_t immortalis_pos = hints.find("immortalis");
	*immortalis = (immortalis_pos != std::string_view::npos);

	const auto try_token = [&](size_t name_pos, size_t name_length) {
		size_t pos = name_pos + name_length;
		const size_t token_end = hints.find('|', pos);
		const size_t end_limit = (token_end == std::string_view::npos) ? hints.size() : token_end;

		while (pos < end_limit && hints[pos] != 'g' && hints[pos] != 't' &&
			!std::isdigit(static_cast<unsigned char>(hints[pos])))
		{
			pos++;
		}
		if (pos == end_limit)
			return false;

		char parsed_series = 'U';
		if (hints[pos] == 'g' || hints[pos] == 't')
			parsed_series = static_cast<char>(std::toupper(static_cast<unsigned char>(hints[pos++])));

		u16 parsed_model = 0;
		size_t parsed_end = pos;
		if (!ParseUnsigned(hints.substr(0, end_limit), pos, &parsed_model, &parsed_end))
			return false;

		*series = parsed_series;
		*model = parsed_model;
		return true;
	};

	// A vendor hint such as "ARM Mali" can appear before the actual renderer token. Do not
	// scan across the " | " separator, otherwise the 'g' in the next "gpu=" key is mistaken
	// for the model series and Mali-G57 becomes Unknown Mali.
	for (size_t pos = hints.find("mali"); pos != std::string_view::npos; pos = hints.find("mali", pos + 4))
	{
		if (try_token(pos, 4))
			return true;
	}

	return immortalis_pos != std::string_view::npos && try_token(immortalis_pos, 10);
}

static u8 ParseCoreCount(std::string_view hints)
{
	for (size_t pos = 0; pos + 2 < hints.size(); pos++)
	{
		if ((hints[pos] != 'm' || (hints[pos + 1] != 'c' && hints[pos + 1] != 'p')) ||
			(pos > 0 && std::isalnum(static_cast<unsigned char>(hints[pos - 1]))))
		{
			continue;
		}

		u16 value = 0;
		size_t end = pos + 2;
		if (ParseUnsigned(hints, pos + 2, &value, &end) && value <= 255)
			return static_cast<u8>(value);
	}
	return 0;
}

static const MaliSpec* FindMaliSpec(char series, u16 model)
{
	for (const MaliSpec& spec : s_mali_specs)
	{
		if (spec.series == series && spec.model == model)
			return &spec;
	}
	return nullptr;
}

static MobileGpuArchitecture ArchitectureForUnknownMali(char series, u16 model)
{
	if (series == 'U')
		return MobileGpuArchitecture::MaliUtgard;
	if (series == 'T')
		return MobileGpuArchitecture::MaliMidgard;
	if (series != 'G')
		return MobileGpuArchitecture::Unknown;
	if (model < 100)
		return (model == 57 || model == 68 || model == 77 || model == 78) ?
			MobileGpuArchitecture::MaliValhall1 : MobileGpuArchitecture::MaliBifrost;
	if (model < 600)
		return MobileGpuArchitecture::MaliValhall2;
	if (model == 615 || model == 715)
		return MobileGpuArchitecture::MaliValhall3;
	if (model >= 620)
		return MobileGpuArchitecture::MaliFifthGen;
	return MobileGpuArchitecture::Unknown;
}

static MobileGsTuning FallbackTuningForMali(MobileGpuArchitecture architecture)
{
	switch (architecture)
	{
		case MobileGpuArchitecture::MaliUtgard: return T(44, 4, 4);
		case MobileGpuArchitecture::MaliMidgard: return T(60, 5, 5);
		case MobileGpuArchitecture::MaliBifrost: return T(72, 6, 5);
		case MobileGpuArchitecture::MaliValhall1: return T(88, 7, 6);
		case MobileGpuArchitecture::MaliValhall2: return T(104, 8, 7);
		case MobileGpuArchitecture::MaliValhall3: return T(128, 9, 7, true);
		case MobileGpuArchitecture::MaliFifthGen: return T(136, 10, 8, true);
		case MobileGpuArchitecture::MaliG1: return T(144, 10, 8, true);
		default: return MakeConservativeMobileGsTuning();
	}
}

static void ApplyCoreCountLimits(MobileGsTuning* tuning, u8 core_count)
{
	if (core_count == 0)
		return;

	u32 pool_cap = 160;
	u32 target_age_cap = 12;
	u32 texture_age_cap = 8;
	if (core_count <= 2)
	{
		pool_cap = 64;
		target_age_cap = 5;
		texture_age_cap = 5;
	}
	else if (core_count <= 4)
	{
		pool_cap = 88;
		target_age_cap = 7;
		texture_age_cap = 6;
	}
	else if (core_count <= 6)
	{
		pool_cap = 112;
		target_age_cap = 8;
		texture_age_cap = 7;
	}
	else if (core_count <= 8)
	{
		pool_cap = 136;
		target_age_cap = 10;
		texture_age_cap = 8;
	}

	tuning->pooled_targets = std::min(tuning->pooled_targets, pool_cap);
	tuning->pooled_textures = std::min(tuning->pooled_textures, pool_cap);
	tuning->target_age = std::min(tuning->target_age, target_age_cap);
	tuning->texture_age = std::min(tuning->texture_age, texture_age_cap);
	tuning->constrained = (tuning->pooled_targets < 128 || tuning->pooled_textures < 128);
	tuning->prefer_new_textures &= !tuning->constrained;
	tuning->force_partial_texture_preloading = tuning->constrained;
}
} // namespace

bool LooksLikeMali(std::string_view lowered_hints)
{
	// Do not equate MediaTek with Mali: older MediaTek parts shipped PowerVR, and the GL/Vulkan renderer
	// string is a more authoritative signal than the SoC vendor.
	return ContainsAny(lowered_hints, {"mali", "immortalis", "arm mali"});
}

ResolvedGpuProfile ResolveMaliProfile(std::string_view lowered_hints)
{
	ResolvedGpuProfile resolved;
	resolved.gpu.name = "Unknown Mali";
	resolved.tuning = MakeConservativeMobileGsTuning();

	char series = 0;
	u16 model = 0;
	bool immortalis = false;
	if (!ParseMaliModel(lowered_hints, &series, &model, &immortalis))
		return resolved;

	const MaliSpec* spec = FindMaliSpec(series, model);
	resolved.gpu.architecture = spec ? spec->architecture : ArchitectureForUnknownMali(series, model);
	resolved.gpu.model_number = model;
	resolved.gpu.core_count = ParseCoreCount(lowered_hints);
	resolved.gpu.recognized = (spec != nullptr);

	if (immortalis)
		resolved.gpu.name = "Immortalis-G" + std::to_string(model);
	else if (series == 'U')
		resolved.gpu.name = "Mali-" + std::to_string(model);
	else
		resolved.gpu.name = "Mali-" + std::string(1, series) + std::to_string(model);
	if (resolved.gpu.core_count != 0)
		resolved.gpu.name += " MC" + std::to_string(resolved.gpu.core_count);

	resolved.tuning = spec ? spec->tuning : FallbackTuningForMali(resolved.gpu.architecture);
	ApplyCoreCountLimits(&resolved.tuning, resolved.gpu.core_count);
	return resolved;
}
} // namespace GpuProfileDetail
