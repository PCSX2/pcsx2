// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/LibrashaderParams.h"

#include "common/FileSystem.h"
#include "common/Path.h"

#include "INISettingsInterface.h"
#include "Config.h"
#include "VMManager.h"

#include "common/Error.h"
#include "common/StringUtil.h"

#include "fmt/format.h"

#ifdef ENABLE_LIBRASHADER
#include <librashader/librashader.h>
#endif

namespace
{
	bool ContainsNoCase(std::string_view str, std::string_view search)
	{
		if (search.empty())
			return true;

		auto it = str.cbegin();
		const auto end = str.cend();
		while (it != end)
		{
			auto match = it;
			for (char c : search)
			{
				if (match == end || std::tolower(static_cast<unsigned char>(*match)) != std::tolower(static_cast<unsigned char>(c)))
				{
					match = end;
					break;
				}
				++match;
			}
			if (match != end)
				return true;
			++it;
		}

		return false;
	}

	std::string TrimQuotes(std::string_view value)
	{
		std::string out(StringUtil::StripWhitespace(value));
		if (out.size() >= 2 &&
			((out.front() == '"' && out.back() == '"') || (out.front() == '\'' && out.back() == '\'')))
		{
			return out.substr(1, out.size() - 2);
		}

		return out;
	}

	// #pragma parameter is float only, some packs fake headings/spacers with weird names.
	bool IsLayoutParamName(std::string_view name)
	{
		// grade.slang (0..1 but not a toggle)
		if (name == "g_analog" || name == "g_digital" || name == "g_sfixes")
			return true;

		// bogus*, dummy*
		if (name.starts_with("bogus") || name.starts_with("dummy"))
			return true;

		// mega bezel / uborder *_SETTINGS, *_nonono
		if (name.ends_with("_SETTINGS") || name.ends_with("_nonono"))
			return true;

		return false;
	}
} // namespace

std::vector<std::string> GetSlangpPassNames(const std::string& preset_path)
{
	std::vector<std::string> out;
	const std::string native_path = Path::ToNativePath(StringUtil::StripWhitespace(preset_path));
	if (native_path.empty() || !FileSystem::FileExists(native_path.c_str()))
		return out;

	const std::optional<std::string> contents = FileSystem::ReadFileToString(native_path.c_str());
	if (!contents.has_value())
		return out;

	std::map<int, std::string> by_index;
	size_t line_start = 0;
	while (line_start < contents->size())
	{
		size_t line_end = contents->find('\n', line_start);
		if (line_end == std::string::npos)
			line_end = contents->size();

		std::string_view line(contents->data() + line_start, line_end - line_start);
		if (!line.empty() && line.back() == '\r')
			line.remove_suffix(1);

		const size_t shader_pos = line.find("shader");
		if (shader_pos != std::string_view::npos)
		{
			size_t index_start = shader_pos + std::strlen("shader");
			size_t index_end = index_start;
			while (index_end < line.size() && std::isdigit(static_cast<unsigned char>(line[index_end])))
				++index_end;

			const size_t eq_pos = line.find('=', index_end);
			if (index_end > index_start && eq_pos != std::string_view::npos)
			{
				const int idx = std::atoi(std::string(line.substr(index_start, index_end - index_start)).c_str());
				std::string value = TrimQuotes(line.substr(eq_pos + 1));
				if (!value.empty() && value[0] != '#')
					by_index.emplace(idx, std::move(value));
			}
		}

		line_start = line_end + 1;
	}

	for (const auto& [_, path] : by_index)
		out.push_back(std::string(Path::GetFileName(path)));

	if (out.empty())
		out.emplace_back(Path::GetFileName(native_path));

	return out;
}

std::vector<size_t> FilterLibrashaderParams(
	const std::vector<LibrashaderParam>& parameters, const std::string& search_query)
{
	std::vector<size_t> visible_indices;
	visible_indices.reserve(parameters.size());

	for (size_t index = 0; index < parameters.size(); index++)
	{
		const LibrashaderParam& param = parameters[index];
		if (search_query.empty() || ContainsNoCase(param.name, search_query) || ContainsNoCase(param.description, search_query))
			visible_indices.push_back(index);
	}

	return visible_indices;
}

LibrashaderParam::Control LibrashaderParam::GetControl() const
{
	if (IsSpacer())
		return Control::Spacer;
	if (IsLabel())
		return Control::Label;
	if (IsBool())
		return Control::Bool;
	return Control::Float;
}

std::string LibrashaderParam::GetLabel() const
{
	return !description.empty() ? description : name;
}

bool LibrashaderParam::IsBool() const
{
	return !IsLabel() && minimum == 0.0f && maximum == 1.0f && step >= 1.0f - 1e-5f;
}

bool LibrashaderParam::GetBoolValue() const
{
	return value >= 0.5f;
}

void LibrashaderParam::SetBoolValue(bool on)
{
	value = on ? 1.0f : 0.0f;
}

bool LibrashaderParam::IsLabel() const
{
	if (std::abs(maximum - minimum) < 1e-5f)
		return true;

	if (step > 1e-5f)
	{
		const float step_count = (maximum - minimum) / step;
		// fake 0 0 0.0001 0.0001 spacers (hcrt_space*)
		if (step_count <= 1.0f + 1e-5f && maximum < 1.0f - 1e-5f)
			return true;
	}

	// params (0..1 step 1) that are section headers, not toggles
	if (minimum == 0.0f && maximum == 1.0f && step >= 1.0f - 1e-5f)
		return IsLayoutParamName(name);

	return false;
}

bool LibrashaderParam::IsSpacer() const
{
	if (name.ends_with("_EMPTY_LINE") || name.ends_with("_LINE"))
		return true;

	if (name.starts_with("hcrt_space"))
		return true;

	// non_nonono " " and similar
	if (name.ends_with("_nonono") && StringUtil::StripWhitespace(description).empty())
		return true;

	return false;
}

float LibrashaderParam::SnapValue(float v) const
{
	if (step > 0.0f)
		v = minimum + std::round((v - minimum) / step) * step;
	return std::clamp(v, minimum, maximum);
}

const char* LibrashaderParam::SliderFormat() const
{
	if (step >= 1.0f - 1e-6f)
		return "%.0f";
	if (step >= 0.1f - 1e-6f)
		return "%.1f";
	if (step >= 0.01f - 1e-6f)
		return "%.2f";
	return "%.3f";
}

std::string LibrashaderParam::FormatValue(float v) const
{
	v = SnapValue(v);
	char buf[64];
	std::snprintf(buf, sizeof(buf), SliderFormat(), v);
	return buf;
}

float LibrashaderParam::FromSlider(int slider) const
{
	const float t = static_cast<float>(slider) / static_cast<float>(LIBRASHADER_SLIDER_STEPS);
	return SnapValue(minimum + (maximum - minimum) * t);
}

int LibrashaderParam::ToSlider(float v) const
{
	if (std::abs(maximum - minimum) < 1e-8f)
		return 0;

	v = SnapValue(v);
	const float t = (v - minimum) / (maximum - minimum);
	return std::clamp(static_cast<int>(std::lround(t * static_cast<float>(LIBRASHADER_SLIDER_STEPS))), 0, LIBRASHADER_SLIDER_STEPS);
}

LibrashaderPage GetLibrashaderPage(size_t visible_count, size_t page_index)
{
	LibrashaderPage page;
	page.total_params = visible_count;
	if (visible_count == 0)
		return page;

	page.total_pages = (visible_count + LIBRASHADER_PARAMS_PER_PAGE - 1) / LIBRASHADER_PARAMS_PER_PAGE;
	page.page_index = std::min(page_index, page.total_pages - 1);
	page.start = page.page_index * LIBRASHADER_PARAMS_PER_PAGE;
	page.end = std::min(page.start + LIBRASHADER_PARAMS_PER_PAGE, visible_count);
	return page;
}

LibrashaderVisiblePage GetVisibleLibrashaderPage(
	const std::vector<LibrashaderParam>& parameters, const std::string& search_query, size_t page_index)
{
	LibrashaderVisiblePage result;
	result.indices = FilterLibrashaderParams(parameters, search_query);
	result.page = GetLibrashaderPage(result.indices.size(), page_index);
	return result;
}

#ifdef ENABLE_LIBRASHADER
namespace
{

	std::unordered_map<std::string, float> LoadParamSection(const std::string& file_path, const std::string& section)
	{
		std::unordered_map<std::string, float> out;
		INISettingsInterface params_si(file_path);
		if (!params_si.Load())
			return out;

		for (const auto& [key, val] : params_si.GetKeyValueList(section.c_str()))
		{
			char* end;
			const float f = std::strtof(val.c_str(), &end);
			if (end != val.c_str())
				out.emplace(key, f);
		}

		return out;
	}

	bool FloatsEqual(float a, float b)
	{
		return std::abs(a - b) < 1e-5f;
	}

	float GetInheritedParamValue(const LibrashaderParam& param, const std::unordered_map<std::string, float>& global_values)
	{
		const auto it = global_values.find(param.name);
		return (it != global_values.end()) ? it->second : param.default_value;
	}
} // namespace

std::string GetLibrashaderParamsSectionName(const std::string& preset_path)
{
	uint32_t h = 2166136261u;
	for (unsigned char c : preset_path)
		h = (h ^ c) * 16777619u;

	char buf[16];
	std::snprintf(buf, sizeof(buf), "%08x", h);
	return buf;
}

std::string GetLibrashaderParamsFilePath()
{
	return Path::Combine(EmuFolders::Settings, "librashader_params.ini");
}

std::unordered_map<std::string, float> LoadLibrashaderSavedParams(
	const std::string& preset_path, std::string_view game_serial, u32 game_crc)
{
	std::unordered_map<std::string, float> saved_values;
	if (preset_path.empty())
		return saved_values;

	const std::string section = GetLibrashaderParamsSectionName(preset_path);
	saved_values = LoadParamSection(GetLibrashaderParamsFilePath(), section);

	if (game_crc != 0)
	{
		const std::unordered_map<std::string, float> game_values =
			LoadParamSection(GetLibrashaderGameParamsPath(game_serial, game_crc), section);
		for (const auto& [key, value] : game_values)
			saved_values[key] = value;
	}

	return saved_values;
}

std::string GetLibrashaderGameParamsPath(std::string_view game_serial, u32 game_crc)
{
	return fmt::format("{}_librashader_params.ini", Path::StripExtension(VMManager::GetGameSettingsPath(game_serial, game_crc)));
}

LibrashaderPresetLoad LoadLibrashaderPreset(const std::string& preset_path, const LibrashaderParamsContext& context)
{
	LibrashaderPresetLoad result;
	const std::string native_path = Path::ToNativePath(StringUtil::StripWhitespace(preset_path));
	result.pass_names = GetSlangpPassNames(preset_path);

	libra_shader_preset_t preset = nullptr;
	libra_error_t err = libra_preset_create(native_path.c_str(), &preset);
	if (err)
	{
		libra_error_free(&err);
		result.error = LibrashaderLoadError::PresetFailed;
		return result;
	}

	libra_preset_param_list_t list = {};
	err = libra_preset_get_runtime_params(&preset, &list);
	if (err)
	{
		libra_error_free(&err);
		libra_preset_free(&preset);
		result.error = LibrashaderLoadError::ParamsFailed;
		return result;
	}

	const std::unordered_map<std::string, float> saved_values = LoadLibrashaderSavedParams(
		native_path, context.game_serial, context.IsPerGame() ? context.game_crc : 0);

	result.parameters.reserve(static_cast<size_t>(list.length));
	for (uint64_t i = 0; i < list.length; ++i)
	{
		const libra_preset_param_t& p = list.parameters[i];
		const std::string name = p.name ? std::string(StringUtil::StripWhitespace(p.name)) : std::string();
		if (name.empty())
			continue;

		LibrashaderParam param;
		param.name = name;
		param.description = p.description ? p.description : "";
		param.minimum = p.minimum;
		param.maximum = p.maximum;
		param.step = p.step;
		param.default_value = p.initial;

		const auto saved_it = saved_values.find(name);
		param.value = (saved_it != saved_values.end()) ? saved_it->second : param.default_value;
		result.parameters.push_back(std::move(param));
	}

	libra_preset_free_runtime_params(list);
	libra_preset_free(&preset);
	result.success = true;
	return result;
}

bool SaveLibrashaderParams(const std::string& preset_path, std::span<const LibrashaderParam> parameters,
	const LibrashaderParamsContext& context)
{
	if (preset_path.empty())
		return false;

	const std::string section = GetLibrashaderParamsSectionName(preset_path);
	const std::string file_path = context.IsPerGame() ?
	                                  GetLibrashaderGameParamsPath(context.game_serial, context.game_crc) :
	                                  GetLibrashaderParamsFilePath();
	INISettingsInterface params_si(file_path);
	params_si.Load();
	params_si.ClearSection(section.c_str());

	const std::unordered_map<std::string, float> global_values = context.IsPerGame() ?
	                                                                 LoadParamSection(GetLibrashaderParamsFilePath(), section) :
	                                                                 std::unordered_map<std::string, float>{};

	for (const LibrashaderParam& param : parameters)
	{
		const LibrashaderParam::Control control = param.GetControl();
		if (control != LibrashaderParam::Control::Bool && control != LibrashaderParam::Control::Float)
			continue;

		if (context.IsPerGame() && FloatsEqual(param.value, GetInheritedParamValue(param, global_values)))
			continue;

		params_si.SetFloatValue(section.c_str(), param.name.c_str(), param.value);
	}

	Error err;
	return params_si.Save(&err);
}

bool ResetLibrashaderParams(const std::string& preset_path, std::span<LibrashaderParam> parameters,
	const LibrashaderParamsContext& context)
{
	if (preset_path.empty())
		return false;

	for (LibrashaderParam& param : parameters)
		param.value = param.default_value;

	return SaveLibrashaderParams(preset_path, parameters, context);
}

bool HasLibrashaderGameOverrides(const std::string& preset_path, const LibrashaderParamsContext& context)
{
	if (!context.IsPerGame() || preset_path.empty())
		return false;

	const std::string section = GetLibrashaderParamsSectionName(preset_path);
	INISettingsInterface params_si(GetLibrashaderGameParamsPath(context.game_serial, context.game_crc));
	if (!params_si.Load())
		return false;

	return !params_si.GetKeyValueList(section.c_str()).empty();
}

bool ClearLibrashaderGameOverrides(const std::string& preset_path, const LibrashaderParamsContext& context)
{
	if (!context.IsPerGame() || preset_path.empty())
		return false;

	const std::string section = GetLibrashaderParamsSectionName(preset_path);
	const std::string file_path = GetLibrashaderGameParamsPath(context.game_serial, context.game_crc);
	INISettingsInterface params_si(file_path);
	if (!params_si.Load())
		return true;

	params_si.ClearSection(section.c_str());
	Error err;
	return params_si.Save(&err);
}

std::string GetLibrashaderError(libra_error_t err)
{
	if (!err)
		return {};

	char* error_message = nullptr;
	const LIBRA_ERRNO error_code = libra_error_errno(err);
	std::string result;
	if (libra_error_write(err, &error_message) == 0 && error_message)
	{
		result = fmt::format("{} (errno={})", error_message, static_cast<int>(error_code));
		libra_error_free_string(&error_message);
	}
	else
	{
		result = fmt::format("unknown error (errno={})", static_cast<int>(error_code));
	}
	libra_error_free(&err);
	return result;
}
#endif
