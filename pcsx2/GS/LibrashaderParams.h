// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Types.h"

#include <span>
#include <string>
#include <vector>
#include <string_view>
#include <unordered_map>

static constexpr size_t LIBRASHADER_PARAMS_PER_PAGE = 250;
static constexpr int LIBRASHADER_SLIDER_STEPS = 10000;

struct LibrashaderParam
{
	std::string name;
	std::string description;

	float minimum = 0.0f;
	float maximum = 1.0f;
	float step = 0.0f;

	float default_value = 0.0f;
	float value = 0.0f;

	enum class Control : u8
	{
		Spacer,
		Label,
		Bool,
		Float,
	};

	Control GetControl() const;

	std::string GetLabel() const;
	bool GetBoolValue() const;
	void SetBoolValue(bool on);

	float SnapValue(float v) const;
	std::string FormatValue(float v) const;
	const char* SliderFormat() const;
	int ToSlider(float v) const;
	float FromSlider(int slider) const;

private:
	bool IsLabel() const;
	bool IsSpacer() const;
	bool IsBool() const;
};

enum class LibrashaderLoadError : u8
{
	None,
	PresetFailed,
	ParamsFailed,
};

struct LibrashaderPresetLoad
{
	std::vector<std::string> pass_names;
	LibrashaderLoadError error = LibrashaderLoadError::None;
	std::vector<LibrashaderParam> parameters;
	bool success = false;
};

struct LibrashaderParamsContext
{
	std::string game_serial;
	u32 game_crc = 0;
	bool per_game = false;

	bool IsPerGame() const { return per_game && game_crc != 0; }
};

std::vector<std::string> GetSlangpPassNames(const std::string& preset_path);

std::vector<size_t> FilterLibrashaderParams(
	const std::vector<LibrashaderParam>& parameters, const std::string& search_query);

struct LibrashaderPage
{
	size_t start = 0;
	size_t end = 0;
	size_t total_params = 0;
	size_t total_pages = 1;
	size_t page_index = 0;
};

struct LibrashaderVisiblePage
{
	std::vector<size_t> indices;
	LibrashaderPage page;
};

LibrashaderPage GetLibrashaderPage(size_t visible_count, size_t page_index);
LibrashaderVisiblePage GetVisibleLibrashaderPage(
	const std::vector<LibrashaderParam>& parameters, const std::string& search_query, size_t page_index);

#ifdef ENABLE_LIBRASHADER
struct _libra_error;
typedef struct _libra_error* libra_error_t;

std::string GetLibrashaderParamsSectionName(const std::string& preset_path);
std::string GetLibrashaderParamsFilePath();
std::string GetLibrashaderGameParamsPath(std::string_view game_serial, u32 game_crc);

LibrashaderPresetLoad LoadLibrashaderPreset(
	const std::string& preset_path, const LibrashaderParamsContext& context = {});
bool SaveLibrashaderParams(const std::string& preset_path, std::span<const LibrashaderParam> parameters,
	const LibrashaderParamsContext& context = {});
bool ResetLibrashaderParams(const std::string& preset_path, std::span<LibrashaderParam> parameters,
	const LibrashaderParamsContext& context = {});
std::unordered_map<std::string, float> LoadLibrashaderSavedParams(
	const std::string& preset_path, std::string_view game_serial, u32 game_crc);
bool HasLibrashaderGameOverrides(const std::string& preset_path, const LibrashaderParamsContext& context);
bool ClearLibrashaderGameOverrides(const std::string& preset_path, const LibrashaderParamsContext& context);
std::string GetLibrashaderError(libra_error_t err);
#endif
