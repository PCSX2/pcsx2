// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Common/GSDevice.h"
#include "GS/Renderers/Common/GSTexture.h"
#include "Achievements.h"
#include "GameList.h"
#include "Host.h"
#include "Host/AudioStream.h"
#include "INISettingsInterface.h"
#include "ImGui/FullscreenUI_Internal.h"
#include "ImGui/ImGuiFullscreen.h"
#include "ImGui/ImGuiManager.h"
#include "Input/InputManager.h"
#include "MTGS.h"
#include "Patch.h"
#include "USB/USB.h"
#include "VMManager.h"
#include "ps2/BiosTools.h"
#include "DEV9/ATA/HddCreate.h"
#include "DEV9/pcap_io.h"
#include "DEV9/sockets.h"
#ifdef _WIN32
#include "DEV9/Win32/tap.h"
#endif

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SettingsInterface.h"
#include "common/StringUtil.h"
#include "common/SmallString.h"
#include "common/Threading.h"

#include "SIO/Memcard/MemoryCardFile.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Sio.h"

#include "IconsFontAwesome.h"
#include "IconsPromptFont.h"
#include "imgui.h"
#include "imgui_internal.h"

#include "fmt/format.h"

#include <set>

namespace FullscreenUI
{
	class HddCreateInProgress : public HddCreate
	{
	private:
		std::string m_dialogId;
		int m_reqMiB = 0;

		static std::vector<std::shared_ptr<HddCreateInProgress>> s_activeOperations;
		static std::mutex s_operationsMutex;
		static std::atomic_int s_nextOperationId;

	public:
		HddCreateInProgress(const std::string& dialogId)
			: m_dialogId(dialogId)
		{
		}

		static bool StartCreation(const std::string& filePath, int sizeInGB, bool use48BitLBA)
		{
			if (filePath.empty() || sizeInGB <= 0)
				return false;

			std::string dialogId = fmt::format("hdd_create_{}", s_nextOperationId.fetch_add(1, std::memory_order_relaxed));

			std::shared_ptr<HddCreateInProgress> instance = std::make_shared<HddCreateInProgress>(dialogId);

			// Convert GB to bytes
			const u64 sizeBytes = static_cast<u64>(sizeInGB) * static_cast<u64>(_1gb);

			// Make sure the file doesn't already exist (or delete it if it does)
			if (FileSystem::FileExists(filePath.c_str()))
			{
				if (!FileSystem::DeleteFilePath(filePath.c_str()))
				{
					ShowToast(
						fmt::format("{} HDD Creation Failed", ICON_FA_TRIANGLE_EXCLAMATION),
						fmt::format("Failed to delete existing HDD image file '{}'. Please check file permissions and try again.", Path::GetFileName(filePath)),
						5.0f);
					return false;
				}
			}

			// Setup the creation parameters
			instance->filePath = filePath;
			instance->neededSize = sizeBytes;

			// Register the operation
			{
				std::lock_guard<std::mutex> lock(s_operationsMutex);
				s_activeOperations.push_back(instance);
			}

			// Start the HDD creation
			std::thread([instance = std::move(instance)]() {
				instance->Start();

				if (!instance->errored)
					MTGS::RunOnGSThread([size_gb = static_cast<int>(instance->neededSize / static_cast<u64>(_1gb))]() {
						ShowToast(
							ICON_FA_CIRCLE_CHECK,
							fmt::format("HDD image ({} GB) created successfully.", size_gb),
							3.0f);
					});
				else
					MTGS::RunOnGSThread([]() {
						ShowToast(
							ICON_FA_TRIANGLE_EXCLAMATION,
							"Failed to create HDD image.",
							3.0f);
					});

				std::lock_guard<std::mutex> lock(s_operationsMutex);
				for (auto it = s_activeOperations.begin(); it != s_activeOperations.end(); ++it)
				{
					if (it->get() == instance.get())
					{
						s_activeOperations.erase(it);
						break;
					}
				}
			}).detach();

			return true;
		}

		static void CancelAllOperations()
		{
			std::lock_guard<std::mutex> lock(s_operationsMutex);
			for (auto& operation : s_activeOperations)
				operation->SetCanceled();
			s_activeOperations.clear();
		}

	protected:
		virtual void Init() override
		{
			m_reqMiB = static_cast<int>((neededSize + ((1024 * 1024) - 1)) / (1024 * 1024));
			const std::string message = fmt::format("{} Creating HDD Image\n{} / {} MiB", ICON_FA_HARD_DRIVE, 0, m_reqMiB);
			ImGuiFullscreen::OpenProgressDialog(m_dialogId.c_str(), message, 0, m_reqMiB, 0);
		}

		virtual void SetFileProgress(u64 currentSize) override
		{
			const int writtenMiB = static_cast<int>((currentSize + ((1024 * 1024) - 1)) / (1024 * 1024));
			const std::string message = fmt::format("{} Creating HDD Image\n{} / {} MiB", ICON_FA_HARD_DRIVE, writtenMiB, m_reqMiB);
			ImGuiFullscreen::UpdateProgressDialog(m_dialogId.c_str(), message, 0, m_reqMiB, writtenMiB);
		}

		virtual void Cleanup() override
		{
			ImGuiFullscreen::CloseProgressDialog(m_dialogId.c_str());
		}
	};

	std::vector<std::shared_ptr<HddCreateInProgress>> HddCreateInProgress::s_activeOperations;
	std::mutex HddCreateInProgress::s_operationsMutex;
	std::atomic_int HddCreateInProgress::s_nextOperationId{0};

	bool CreateHardDriveWithProgress(const std::string& filePath, int sizeInGB, bool use48BitLBA)
	{
		// Validate size limits based on the LBA mode set
		const int min_size = use48BitLBA ? 100 : 40;
		const int max_size = use48BitLBA ? 2000 : 120;

		if (sizeInGB < min_size || sizeInGB > max_size)
		{
			ShowToast(std::string(), fmt::format("Invalid HDD size. Size must be between {} and {} GB.", min_size, max_size).c_str());
			return false;
		}

		return HddCreateInProgress::StartCreation(filePath, sizeInGB, use48BitLBA);
	}

	void CancelAllHddOperations()
	{
		HddCreateInProgress::CancelAllOperations();
	}
} // namespace FullscreenUI

bool FullscreenUI::IsEditingGameSettings(SettingsInterface* bsi)
{
	return (bsi == s_game_settings_interface.get());
}

SettingsInterface* FullscreenUI::GetEditingSettingsInterface()
{
	return s_game_settings_interface ? s_game_settings_interface.get() : Host::Internal::GetBaseSettingsLayer();
}

SettingsInterface* FullscreenUI::GetEditingSettingsInterface(bool game_settings)
{
	return (game_settings && s_game_settings_interface) ? s_game_settings_interface.get() : Host::Internal::GetBaseSettingsLayer();
}

bool FullscreenUI::ShouldShowAdvancedSettings(SettingsInterface* bsi)
{
	return IsEditingGameSettings(bsi) ? Host::GetBaseBoolSettingValue("UI", "ShowAdvancedSettings", false) :
	                                    bsi->GetBoolValue("UI", "ShowAdvancedSettings", false);
}

void FullscreenUI::SetSettingsChanged(SettingsInterface* bsi)
{
	if (bsi && bsi == s_game_settings_interface.get())
		s_game_settings_changed.store(true, std::memory_order_release);
	else
		s_settings_changed.store(true, std::memory_order_release);
}

bool FullscreenUI::GetEffectiveBoolSetting(SettingsInterface* bsi, const char* section, const char* key, bool default_value)
{
	if (IsEditingGameSettings(bsi))
	{
		std::optional<bool> value = bsi->GetOptionalBoolValue(section, key, std::nullopt);
		if (value.has_value())
			return value.value();
	}

	return Host::Internal::GetBaseSettingsLayer()->GetBoolValue(section, key, default_value);
}

s32 FullscreenUI::GetEffectiveIntSetting(SettingsInterface* bsi, const char* section, const char* key, s32 default_value)
{
	if (IsEditingGameSettings(bsi))
	{
		std::optional<s32> value = bsi->GetOptionalIntValue(section, key, std::nullopt);
		if (value.has_value())
			return value.value();
	}

	return Host::Internal::GetBaseSettingsLayer()->GetIntValue(section, key, default_value);
}

void FullscreenUI::DrawInputBindingButton(
	SettingsInterface* bsi, InputBindingInfo::Type type, const char* section, const char* name, const char* display_name, const char* icon_name, bool show_type)
{
	TinyString title;
	title.format("{}/{}", section, name);

	SmallString value = bsi->GetSmallStringValue(section, name);
	const bool oneline = (value.count('&') <= 1);

	ImRect bb;
	bool visible, hovered, clicked;
	clicked = MenuButtonFrame(title, true,
		oneline ? ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY :
				  ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT,
		&visible, &hovered, &bb.Min, &bb.Max);
	if (!visible)
		return;

	if (oneline)
		InputManager::PrettifyInputBinding(value, true);
	else
		InputManager::PrettifyInputBinding(value, false);

	if (show_type)
	{
		if (icon_name)
		{
			title.format("{} {}", icon_name, display_name);
		}
		else
		{
			switch (type)
			{
				case InputBindingInfo::Type::Button:
					title.format(ICON_FA_CIRCLE_DOT " {}", display_name);
					break;
				case InputBindingInfo::Type::Axis:
				case InputBindingInfo::Type::HalfAxis:
					title.format(ICON_FA_BULLSEYE " {}", display_name);
					break;
				case InputBindingInfo::Type::Motor:
					title.format(ICON_PF_CONTROLLER_VIBRATION " {}", display_name);
					break;
				case InputBindingInfo::Type::Macro:
					title.format(ICON_PF_THUNDERBOLT " {}", display_name);
					break;
				default:
					title = display_name;
					break;
			}
		}
	}

	const float midpoint = bb.Min.y + g_large_font.second + LayoutScale(4.0f);

	if (oneline)
	{
		ImGui::PushFont(g_large_font.first, g_large_font.second);

		const ImVec2 value_size(ImGui::CalcTextSize(value.empty() ? FSUI_CSTR("-") : value.c_str(), nullptr));
		const float text_end = bb.Max.x - value_size.x;
		const ImRect title_bb(bb.Min, ImVec2(text_end, midpoint));

		ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, show_type ? title.c_str() : display_name, nullptr, nullptr,
			ImVec2(0.0f, 0.0f), &title_bb);
		ImGui::RenderTextClipped(bb.Min, bb.Max, value.empty() ? FSUI_CSTR("-") : value.c_str(), nullptr, &value_size,
			ImVec2(1.0f, 0.5f), &bb);
		ImGui::PopFont();
	}
	else
	{
		const ImRect title_bb(bb.Min, ImVec2(bb.Max.x, midpoint));
		const ImRect summary_bb(ImVec2(bb.Min.x, midpoint), bb.Max);

		ImGui::PushFont(g_large_font.first, g_large_font.second);
		ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, show_type ? title.c_str() : display_name, nullptr, nullptr,
			ImVec2(0.0f, 0.0f), &title_bb);
		ImGui::PopFont();

		ImGui::PushFont(g_medium_font.first, g_medium_font.second);
		ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, value.empty() ? FSUI_CSTR("No Binding") : value.c_str(),
			nullptr, nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
		ImGui::PopFont();
	}

	if (clicked)
	{
		BeginInputBinding(bsi, type, section, name, display_name);
	}
	else if (hovered && (ImGui::IsItemClicked(ImGuiMouseButton_Right) || ImGui::Shortcut(ImGuiKey_NavGamepadMenu)))
	{
		bsi->DeleteValue(section, name);
		SetSettingsChanged(bsi);
	}
	else
	{
		if (hovered)
		{
			if (ImGuiFullscreen::IsGamepadInputSource())
			{
				const bool swapNorthWest = ImGuiManager::IsGamepadNorthWestSwapped();
				ImGuiFullscreen::QueueFooterHint(std::array{
					std::make_pair(swapNorthWest ? ICON_PF_BUTTON_TRIANGLE : ICON_PF_BUTTON_SQUARE, FSUI_VSTR("Clear Binding")),
				});
			}
			else
			{
				ImGuiFullscreen::QueueFooterHint(std::array{
					std::make_pair(ICON_PF_RIGHT_CLICK, FSUI_VSTR("Clear Binding")),
				});
			}
		}
	}
}

void FullscreenUI::ClearInputBindingVariables()
{
	s_input_binding_type = InputBindingInfo::Type::Unknown;
	s_input_binding_section = {};
	s_input_binding_key = {};
	s_input_binding_display_name = {};
	s_input_binding_new_bindings = {};
	s_input_binding_value_ranges = {};
}

void FullscreenUI::BeginInputBinding(SettingsInterface* bsi, InputBindingInfo::Type type, const std::string_view section,
	const std::string_view key, const std::string_view display_name)
{
	if (s_input_binding_type != InputBindingInfo::Type::Unknown)
	{
		InputManager::RemoveHook();
		ClearInputBindingVariables();
	}

	s_input_binding_type = type;
	s_input_binding_section = section;
	s_input_binding_key = key;
	s_input_binding_display_name = display_name;
	s_input_binding_new_bindings = {};
	s_input_binding_value_ranges = {};
	s_input_binding_timer.Reset();

	const bool game_settings = IsEditingGameSettings(bsi);

	InputManager::SetHook([game_settings](InputBindingKey key, float value) -> InputInterceptHook::CallbackResult {
		if (s_input_binding_type == InputBindingInfo::Type::Unknown)
			return InputInterceptHook::CallbackResult::StopProcessingEvent;

		// holding the settings lock here will protect the input binding list
		auto lock = Host::GetSettingsLock();

		float initial_value = value;
		float min_value = value;
		auto it = std::find_if(s_input_binding_value_ranges.begin(), s_input_binding_value_ranges.end(),
			[key](const auto& it) { return it.first.bits == key.bits; });
		if (it != s_input_binding_value_ranges.end())
		{
			initial_value = it->second.first;
			min_value = it->second.second = std::min(it->second.second, value);
		}
		else
		{
			s_input_binding_value_ranges.emplace_back(key, std::make_pair(initial_value, min_value));
		}

		const float abs_value = std::abs(value);
		const bool reverse_threshold = (key.source_subtype == InputSubclass::ControllerAxis && initial_value > 0.5f);

		for (InputBindingKey& other_key : s_input_binding_new_bindings)
		{
			// if this key is in our new binding list, it's a "release", and we're done
			if (other_key.MaskDirection() == key.MaskDirection())
			{
				// for pedals, we wait for it to go back to near its starting point to commit the binding
				if ((reverse_threshold ? ((initial_value - value) <= 0.25f) : (abs_value < 0.5f)))
				{
					// did we go the full range?
					if (reverse_threshold && initial_value > 0.5f && min_value <= -0.5f)
						other_key.modifier = InputModifier::FullAxis;

					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					const std::string new_binding(InputManager::ConvertInputBindingKeysToString(
						s_input_binding_type, s_input_binding_new_bindings.data(), s_input_binding_new_bindings.size()));
					bsi->SetStringValue(s_input_binding_section.c_str(), s_input_binding_key.c_str(), new_binding.c_str());
					SetSettingsChanged(bsi);
					ClearInputBindingVariables();
					return InputInterceptHook::CallbackResult::RemoveHookAndStopProcessingEvent;
				}

				// otherwise, keep waiting
				return InputInterceptHook::CallbackResult::StopProcessingEvent;
			}
		}

		// new binding, add it to the list, but wait for a decent distance first, and then wait for release
		if ((reverse_threshold ? (abs_value < 0.5f) : (abs_value >= 0.5f)))
		{
			InputBindingKey key_to_add = key;
			key_to_add.modifier = (value < 0.0f && !reverse_threshold) ? InputModifier::Negate : InputModifier::None;
			key_to_add.invert = reverse_threshold;
			s_input_binding_new_bindings.push_back(key_to_add);
		}

		return InputInterceptHook::CallbackResult::StopProcessingEvent;
	});
}

void FullscreenUI::DrawInputBindingWindow()
{
	pxAssert(s_input_binding_type != InputBindingInfo::Type::Unknown);

	const double time_remaining = INPUT_BINDING_TIMEOUT_SECONDS - s_input_binding_timer.GetTimeSeconds();
	if (time_remaining <= 0.0)
	{
		InputManager::RemoveHook();
		ClearInputBindingVariables();
		return;
	}

	const char* title = FSUI_ICONSTR(ICON_FA_GAMEPAD, "Set Input Binding");
	ImGui::SetNextWindowSize(LayoutScale(500.0f, 0.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::OpenPopup(title);

	ImGui::PushFont(g_large_font.first, g_large_font.second);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

	if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs))
	{
		ImGui::TextWrapped(FSUI_CSTR("Setting %s binding %s."), s_input_binding_section.c_str(), s_input_binding_display_name.c_str());
		ImGui::TextUnformatted(FSUI_CSTR("Push a controller button or axis now."));
		ImGui::NewLine();
		ImGui::Text(FSUI_CSTR("Timing out in %.0f seconds..."), time_remaining);
		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(4);
	ImGui::PopFont();
}

bool FullscreenUI::DrawToggleSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
	bool default_value, bool enabled, bool allow_tristate, float height, std::pair<ImFont*, float> font, std::pair<ImFont*, float> summary_font)
{
	if (!allow_tristate || !IsEditingGameSettings(bsi))
	{
		bool value = bsi->GetBoolValue(section, key, default_value);
		if (!ToggleButton(title, summary, &value, enabled, height, font, summary_font))
			return false;

		bsi->SetBoolValue(section, key, value);
	}
	else
	{
		std::optional<bool> value(false);
		if (!bsi->GetBoolValue(section, key, &value.value()))
			value.reset();
		if (!ThreeWayToggleButton(title, summary, &value, enabled, height, font, summary_font))
			return false;

		if (value.has_value())
			bsi->SetBoolValue(section, key, value.value());
		else
			bsi->DeleteValue(section, key);
	}

	SetSettingsChanged(bsi);
	return true;
}

void FullscreenUI::DrawIntListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
	int default_value, const char* const* options, size_t option_count, bool translate_options, int option_offset, bool enabled,
	float height, std::pair<ImFont*, float> font, std::pair<ImFont*, float> summary_font)
{
	if (options && option_count == 0)
	{
		while (options[option_count] != nullptr)
			option_count++;
	}

	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<int> value =
		bsi->GetOptionalIntValue(section, key, game_settings ? std::nullopt : std::optional<int>(default_value));
	const int index = value.has_value() ? (value.value() - option_offset) : std::numeric_limits<int>::min();
	const char* value_text = (value.has_value()) ?
	                             ((index < 0 || static_cast<size_t>(index) >= option_count) ?
										 FSUI_CSTR("Unknown") :
										 (translate_options ? Host::TranslateToCString(TR_CONTEXT, options[index]) : options[index])) :
	                             FSUI_CSTR("Use Global Setting");

	if (MenuButtonWithValue(title, summary, value_text, enabled, height, font, summary_font))
	{
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(option_count + 1);
		if (game_settings)
			cd_options.emplace_back(FSUI_STR("Use Global Setting"), !value.has_value());
		for (size_t i = 0; i < option_count; i++)
		{
			cd_options.emplace_back(translate_options ? Host::TranslateToString(TR_CONTEXT, options[i]) : std::string(options[i]),
				(i == static_cast<size_t>(index)));
		}
		OpenChoiceDialog(title, false, std::move(cd_options),
			[game_settings, section, key, option_offset](s32 index, const std::string& title, bool checked) {
				if (index >= 0)
				{
					auto lock = Host::GetSettingsLock();
					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					if (game_settings)
					{
						if (index == 0)
							bsi->DeleteValue(section, key);
						else
							bsi->SetIntValue(section, key, index - 1 + option_offset);
					}
					else
					{
						bsi->SetIntValue(section, key, index + option_offset);
					}

					SetSettingsChanged(bsi);
				}

				CloseChoiceDialog();
			});
	}
}

void FullscreenUI::DrawIntRangeSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
	int default_value, int min_value, int max_value, const char* format, bool enabled, float height, std::pair<ImFont*, float> font, std::pair<ImFont*, float> summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<int> value =
		bsi->GetOptionalIntValue(section, key, game_settings ? std::nullopt : std::optional<int>(default_value));
	const SmallString value_text =
		value.has_value() ? SmallString::from_sprintf(format, value.value()) : SmallString(FSUI_VSTR("Use Global Setting"));

	if (MenuButtonWithValue(title, summary, value_text.c_str(), enabled, height, font, summary_font))
		ImGui::OpenPopup(title);

	ImGui::SetNextWindowSize(LayoutScale(500.0f, 192.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushFont(g_large_font.first, g_large_font.second);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

	bool is_open = true;
	if (ImGui::BeginPopupModal(title, &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
	{
		BeginMenuButtons();

		const float end = ImGui::GetCurrentWindow()->WorkRect.GetWidth();
		ImGui::SetNextItemWidth(end);
		s32 dlg_value = static_cast<s32>(value.value_or(default_value));

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, LayoutScale(8.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, LayoutScale(1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, LayoutScale(8.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.45f, 0.65f, 0.95f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.55f, 0.75f, 1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

		if (ImGui::SliderInt("##value", &dlg_value, min_value, max_value, format, ImGuiSliderFlags_NoInput))
		{
			if (IsEditingGameSettings(bsi) && dlg_value == default_value)
				bsi->DeleteValue(section, key);
			else
				bsi->SetIntValue(section, key, dlg_value);

			SetSettingsChanged(bsi);
		}

		ImGui::PopStyleColor(7);
		ImGui::PopStyleVar(3);

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(10.0f));
		if (MenuButtonWithoutSummary(FSUI_CSTR("OK"), true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, g_large_font, ImVec2(0.5f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(4);
	ImGui::PopFont();
}

void FullscreenUI::DrawIntSpinBoxSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* key, int default_value, int min_value, int max_value, int step_value, const char* format, bool enabled, float height,
	std::pair<ImFont*, float> font, std::pair<ImFont*, float> summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<int> value =
		bsi->GetOptionalIntValue(section, key, game_settings ? std::nullopt : std::optional<int>(default_value));
	const SmallString value_text =
		value.has_value() ? SmallString::from_sprintf(format, value.value()) : SmallString(FSUI_VSTR("Use Global Setting"));

	static bool manual_input = false;

	if (MenuButtonWithValue(title, summary, value_text.c_str(), enabled, height, font, summary_font))
	{
		ImGui::OpenPopup(title);
		manual_input = false;
	}

	ImGui::SetNextWindowSize(LayoutScale(500.0f, 192.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushFont(g_large_font.first, g_large_font.second);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

	bool is_open = true;
	if (ImGui::BeginPopupModal(title, &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
	{
		BeginMenuButtons();

		s32 dlg_value = static_cast<s32>(value.value_or(default_value));
		bool dlg_value_changed = false;

		char str_value[32];
		std::snprintf(str_value, std::size(str_value), format, dlg_value);

		if (manual_input)
		{
			const float end = ImGui::GetCurrentWindow()->WorkRect.GetWidth();
			ImGui::SetNextItemWidth(end);

			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, LayoutScale(8.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(12.0f, 10.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, LayoutScale(1.0f));
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

			if (ImGui::InputText("##value", str_value, std::size(str_value), ImGuiInputTextFlags_CharsDecimal))
			{
				const s32 new_value = StringUtil::FromChars<s32>(str_value).value_or(dlg_value);
				dlg_value_changed = (dlg_value != new_value);
				dlg_value = new_value;
			}

			ImGui::PopStyleColor(5);
			ImGui::PopStyleVar(3);

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(10.0f));
		}
		else
		{
			const ImVec2& padding(ImGui::GetStyle().FramePadding);
			ImVec2 button_pos(ImGui::GetCursorPos());

			// Align value text in middle.
			ImGui::SetCursorPosY(
				button_pos.y + ((LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY) + padding.y * 2.0f) - g_large_font.second) * 0.5f);
			ImGui::TextUnformatted(str_value);

			s32 step = 0;
			if (FloatingButton(
					ICON_FA_CHEVRON_UP, padding.x, button_pos.y, -1.0f, -1.0f, 1.0f, 0.0f, true, g_large_font, &button_pos, true))
			{
				step = step_value;
			}
			if (FloatingButton(ICON_FA_CHEVRON_DOWN, button_pos.x - padding.x, button_pos.y, -1.0f, -1.0f, -1.0f, 0.0f, true, g_large_font,
					&button_pos, true))
			{
				step = -step_value;
			}
			if (FloatingButton(
					ICON_FA_KEYBOARD, button_pos.x - padding.x, button_pos.y, -1.0f, -1.0f, -1.0f, 0.0f, true, g_large_font, &button_pos))
			{
				manual_input = true;
			}
			if (FloatingButton(
					ICON_FA_TRASH, button_pos.x - padding.x, button_pos.y, -1.0f, -1.0f, -1.0f, 0.0f, true, g_large_font, &button_pos))
			{
				dlg_value = default_value;
				dlg_value_changed = true;
			}

			if (step != 0)
			{
				dlg_value += step;
				dlg_value_changed = true;
			}

			ImGui::SetCursorPosY(button_pos.y + (padding.y * 2.0f) + LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY + 10.0f));
		}

		if (dlg_value_changed)
		{
			dlg_value = std::clamp(dlg_value, min_value, max_value);
			if (IsEditingGameSettings(bsi) && dlg_value == default_value)
				bsi->DeleteValue(section, key);
			else
				bsi->SetIntValue(section, key, dlg_value);

			SetSettingsChanged(bsi);
		}

		if (MenuButtonWithoutSummary(FSUI_CSTR("OK"), true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, g_large_font, ImVec2(0.5f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(4);
	ImGui::PopFont();
}

void FullscreenUI::DrawFloatRangeSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* key, float default_value, float min_value, float max_value, const char* format, float multiplier, bool enabled,
	float height, std::pair<ImFont*, float> font, std::pair<ImFont*, float> summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<float> value =
		bsi->GetOptionalFloatValue(section, key, game_settings ? std::nullopt : std::optional<float>(default_value));
	const SmallString value_text =
		value.has_value() ? SmallString::from_sprintf(format, value.value() * multiplier) : SmallString(FSUI_VSTR("Use Global Setting"));

	if (MenuButtonWithValue(title, summary, value_text.c_str(), enabled, height, font, summary_font))
		ImGui::OpenPopup(title);

	ImGui::SetNextWindowSize(LayoutScale(500.0f, 190.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushFont(g_large_font.first, g_large_font.second);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

	bool is_open = true;
	if (ImGui::BeginPopupModal(title, &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
	{
		BeginMenuButtons();

		const float end = ImGui::GetCurrentWindow()->WorkRect.GetWidth();
		ImGui::SetNextItemWidth(end);
		float dlg_value = value.value_or(default_value) * multiplier;

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, LayoutScale(8.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, LayoutScale(1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, LayoutScale(8.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.45f, 0.65f, 0.95f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.55f, 0.75f, 1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

		if (ImGui::SliderFloat("##value", &dlg_value, min_value * multiplier, max_value * multiplier, format, ImGuiSliderFlags_NoInput))
		{
			dlg_value /= multiplier;

			if (IsEditingGameSettings(bsi) && dlg_value == default_value)
				bsi->DeleteValue(section, key);
			else
				bsi->SetFloatValue(section, key, dlg_value);

			SetSettingsChanged(bsi);
		}

		ImGui::PopStyleColor(7);
		ImGui::PopStyleVar(3);

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(10.0f));
		if (MenuButtonWithoutSummary(FSUI_CSTR("OK"), true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, g_large_font, ImVec2(0.5f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(4);
	ImGui::PopFont();
}

void FullscreenUI::DrawFloatSpinBoxSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* key, float default_value, float min_value, float max_value, float step_value, float multiplier, const char* format,
	bool enabled, float height, std::pair<ImFont*, float> font, std::pair<ImFont*, float> summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<float> value =
		bsi->GetOptionalFloatValue(section, key, game_settings ? std::nullopt : std::optional<int>(default_value));
	const SmallString value_text =
		value.has_value() ? SmallString::from_sprintf(format, value.value() * multiplier) : SmallString(FSUI_VSTR("Use Global Setting"));

	static bool manual_input = false;

	if (MenuButtonWithValue(title, summary, value_text.c_str(), enabled, height, font, summary_font))
	{
		ImGui::OpenPopup(title);
		manual_input = false;
	}

	ImGui::SetNextWindowSize(LayoutScale(500.0f, 192.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushFont(g_large_font.first, g_large_font.second);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

	bool is_open = true;
	if (ImGui::BeginPopupModal(title, &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		BeginMenuButtons();

		float dlg_value = value.value_or(default_value) * multiplier;
		bool dlg_value_changed = false;

		char str_value[32];
		std::snprintf(str_value, std::size(str_value), format, dlg_value);

		if (manual_input)
		{
			const float end = ImGui::GetCurrentWindow()->WorkRect.GetWidth();
			ImGui::SetNextItemWidth(end);

			// round trip to drop any suffixes (e.g. percent)
			if (auto tmp_value = StringUtil::FromChars<float>(str_value); tmp_value.has_value())
			{
				std::snprintf(str_value, std::size(str_value),
					((tmp_value.value() - std::floor(tmp_value.value())) < 0.01f) ? "%.0f" : "%f", tmp_value.value());
			}

			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, LayoutScale(8.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(12.0f, 10.0f));
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));			

			if (ImGui::InputText("##value", str_value, std::size(str_value), ImGuiInputTextFlags_CharsDecimal))
			{
				const float new_value = StringUtil::FromChars<float>(str_value).value_or(dlg_value);
				dlg_value_changed = (dlg_value != new_value);
				dlg_value = new_value;
			}

			ImGui::PopStyleColor(4);
			ImGui::PopStyleVar(2);

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(10.0f));
		}
		else
		{
			const ImVec2& padding(ImGui::GetStyle().FramePadding);
			ImVec2 button_pos(ImGui::GetCursorPos());

			// Align value text in middle.
			ImGui::SetCursorPosY(
				button_pos.y + ((LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY) + padding.y * 2.0f) - g_large_font.second) * 0.5f);
			ImGui::TextUnformatted(str_value);

			float step = 0;
			if (FloatingButton(
					ICON_FA_CHEVRON_UP, padding.x, button_pos.y, -1.0f, -1.0f, 1.0f, 0.0f, true, g_large_font, &button_pos, true))
			{
				step = step_value;
			}
			if (FloatingButton(ICON_FA_CHEVRON_DOWN, button_pos.x - padding.x, button_pos.y, -1.0f, -1.0f, -1.0f, 0.0f, true, g_large_font,
					&button_pos, true))
			{
				step = -step_value;
			}
			if (FloatingButton(
					ICON_FA_KEYBOARD, button_pos.x - padding.x, button_pos.y, -1.0f, -1.0f, -1.0f, 0.0f, true, g_large_font, &button_pos))
			{
				manual_input = true;
			}
			if (FloatingButton(
					ICON_FA_TRASH, button_pos.x - padding.x, button_pos.y, -1.0f, -1.0f, -1.0f, 0.0f, true, g_large_font, &button_pos))
			{
				dlg_value = default_value * multiplier;
				dlg_value_changed = true;
			}

			if (step != 0)
			{
				dlg_value += step * multiplier;
				dlg_value_changed = true;
			}

			ImGui::SetCursorPosY(button_pos.y + (padding.y * 2.0f) + LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY + 10.0f));
		}

		if (dlg_value_changed)
		{
			dlg_value = std::clamp(dlg_value / multiplier, min_value, max_value);
			if (IsEditingGameSettings(bsi) && dlg_value == default_value)
				bsi->DeleteValue(section, key);
			else
				bsi->SetFloatValue(section, key, dlg_value);

			SetSettingsChanged(bsi);
		}

		if (MenuButtonWithoutSummary(FSUI_CSTR("OK"), true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, g_large_font, ImVec2(0.5f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(4);
	ImGui::PopFont();
}

void FullscreenUI::DrawIntRectSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* left_key, int default_left, const char* top_key, int default_top, const char* right_key, int default_right,
	const char* bottom_key, int default_bottom, int min_value, int max_value, int step_value, const char* format, bool enabled,
	float height, std::pair<ImFont*, float> font, std::pair<ImFont*, float> summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<int> left_value =
		bsi->GetOptionalIntValue(section, left_key, game_settings ? std::nullopt : std::optional<int>(default_left));
	const std::optional<int> top_value =
		bsi->GetOptionalIntValue(section, top_key, game_settings ? std::nullopt : std::optional<int>(default_top));
	const std::optional<int> right_value =
		bsi->GetOptionalIntValue(section, right_key, game_settings ? std::nullopt : std::optional<int>(default_right));
	const std::optional<int> bottom_value =
		bsi->GetOptionalIntValue(section, bottom_key, game_settings ? std::nullopt : std::optional<int>(default_bottom));
	const SmallString value_text = SmallString::from_format(FSUI_FSTR("{0}/{1}/{2}/{3}"),
		left_value.has_value() ? TinyString::from_sprintf(format, left_value.value()) : TinyString(FSUI_VSTR("Default")),
		top_value.has_value() ? TinyString::from_sprintf(format, top_value.value()) : TinyString(FSUI_VSTR("Default")),
		right_value.has_value() ? TinyString::from_sprintf(format, right_value.value()) : TinyString(FSUI_VSTR("Default")),
		bottom_value.has_value() ? TinyString::from_sprintf(format, bottom_value.value()) : TinyString(FSUI_VSTR("Default")));

	static bool manual_input = false;

	if (MenuButtonWithValue(title, summary, value_text.c_str(), enabled, height, font, summary_font))
	{
		ImGui::OpenPopup(title);
		manual_input = false;
	}

	ImGui::SetNextWindowSize(LayoutScale(550.0f, 370.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushFont(g_large_font.first, g_large_font.second);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

	bool is_open = true;
	if (ImGui::BeginPopupModal(title, &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
	{
		static constexpr const char* labels[4] = {
			FSUI_NSTR("Left: "),
			FSUI_NSTR("Top: "),
			FSUI_NSTR("Right: "),
			FSUI_NSTR("Bottom: "),
		};
		const char* keys[4] = {
			left_key,
			top_key,
			right_key,
			bottom_key,
		};
		int defaults[4] = {
			default_left,
			default_top,
			default_right,
			default_bottom,
		};
		s32 values[4] = {
			static_cast<s32>(left_value.value_or(default_left)),
			static_cast<s32>(top_value.value_or(default_top)),
			static_cast<s32>(right_value.value_or(default_right)),
			static_cast<s32>(bottom_value.value_or(default_bottom)),
		};

		BeginMenuButtons();

		const ImVec2& padding(ImGui::GetStyle().FramePadding);

		for (u32 i = 0; i < std::size(labels); i++)
		{
			s32 dlg_value = values[i];
			bool dlg_value_changed = false;

			char str_value[32];
			std::snprintf(str_value, std::size(str_value), format, dlg_value);

			ImGui::PushID(i);

			const float midpoint = LayoutScale(125.0f);
			const float end = (ImGui::GetCurrentWindow()->WorkRect.GetWidth() - midpoint) + ImGui::GetStyle().WindowPadding.x;
			ImVec2 button_pos(ImGui::GetCursorPos());

			// Align value text in middle.
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
								 ((LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY) + padding.y * 2.0f) - g_large_font.second) * 0.5f);
			ImGui::TextUnformatted(Host::TranslateToCString(TR_CONTEXT, labels[i]));
			ImGui::SameLine(midpoint);
			ImGui::SetNextItemWidth(end);
			button_pos.x = ImGui::GetCursorPosX();

			if (manual_input)
			{
				ImGui::SetNextItemWidth(end);
				ImGui::SetCursorPosY(button_pos.y);

				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, LayoutScale(8.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(12.0f, 10.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, LayoutScale(1.0f));
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

				if (ImGui::InputText("##value", str_value, std::size(str_value), ImGuiInputTextFlags_CharsDecimal))
				{
					const s32 new_value = StringUtil::FromChars<s32>(str_value).value_or(dlg_value);
					dlg_value_changed = (dlg_value != new_value);
					dlg_value = new_value;
				}

				ImGui::PopStyleColor(5);
				ImGui::PopStyleVar(3);

				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(10.0f));
			}
			else
			{
				ImGui::TextUnformatted(str_value);

				s32 step = 0;
				if (FloatingButton(
						ICON_FA_CHEVRON_UP, padding.x, button_pos.y, -1.0f, -1.0f, 1.0f, 0.0f, true, g_large_font, &button_pos, true))
				{
					step = step_value;
				}
				if (FloatingButton(ICON_FA_CHEVRON_DOWN, button_pos.x - padding.x, button_pos.y, -1.0f, -1.0f, -1.0f, 0.0f, true,
						g_large_font, &button_pos, true))
				{
					step = -step_value;
				}
				if (FloatingButton(ICON_FA_KEYBOARD, button_pos.x - padding.x, button_pos.y, -1.0f, -1.0f, -1.0f, 0.0f, true, g_large_font,
						&button_pos))
				{
					manual_input = true;
				}
				if (FloatingButton(
						ICON_FA_TRASH, button_pos.x - padding.x, button_pos.y, -1.0f, -1.0f, -1.0f, 0.0f, true, g_large_font, &button_pos))
				{
					dlg_value = defaults[i];
					dlg_value_changed = true;
				}

				if (step != 0)
				{
					dlg_value += step;
					dlg_value_changed = true;
				}

				ImGui::SetCursorPosY(button_pos.y + (padding.y * 2.0f) + LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY + 10.0f));
			}

			if (dlg_value_changed)
			{
				dlg_value = std::clamp(dlg_value, min_value, max_value);
				if (IsEditingGameSettings(bsi) && dlg_value == defaults[i])
					bsi->DeleteValue(section, keys[i]);
				else
					bsi->SetIntValue(section, keys[i], dlg_value);

				SetSettingsChanged(bsi);
			}

			ImGui::PopID();
		}

		if (MenuButtonWithoutSummary(FSUI_CSTR("OK"), true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, g_large_font, ImVec2(0.5f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(4);
	ImGui::PopFont();
}

void FullscreenUI::DrawStringListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* key, const char* default_value, const char* const* options, const char* const* option_values, size_t option_count,
	bool translate_options, bool enabled, float height, std::pair<ImFont*, float> font, std::pair<ImFont*, float> summary_font, const char* translation_ctx)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<SmallString> value(
		bsi->GetOptionalSmallStringValue(section, key, game_settings ? std::nullopt : std::optional<const char*>(default_value)));

	if (option_count == 0)
	{
		// select from null entry
		while (options && options[option_count] != nullptr)
			option_count++;
	}

	size_t index = option_count;
	if (value.has_value())
	{
		for (size_t i = 0; i < option_count; i++)
		{
			if (value == option_values[i])
			{
				index = i;
				break;
			}
		}
	}

	if (MenuButtonWithValue(title, summary,
			value.has_value() ?
				((index < option_count) ? (translate_options ? Host::TranslateToCString(translation_ctx, options[index]) : options[index]) :
										  FSUI_CSTR("Unknown")) :
				FSUI_CSTR("Use Global Setting"),
			enabled, height, font, summary_font))
	{
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(option_count + 1);
		if (game_settings)
			cd_options.emplace_back(FSUI_STR("Use Global Setting"), !value.has_value());
		for (size_t i = 0; i < option_count; i++)
		{
			cd_options.emplace_back(translate_options ? Host::TranslateToString(translation_ctx, options[i]) : std::string(options[i]),
				(value.has_value() && i == static_cast<size_t>(index)));
		}
		OpenChoiceDialog(title, false, std::move(cd_options),
			[game_settings, section, key, option_values](s32 index, const std::string& title, bool checked) {
				if (index >= 0)
				{
					auto lock = Host::GetSettingsLock();
					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					if (game_settings)
					{
						if (index == 0)
							bsi->DeleteValue(section, key);
						else
							bsi->SetStringValue(section, key, option_values[index - 1]);
					}
					else
					{
						bsi->SetStringValue(section, key, option_values[index]);
					}

					SetSettingsChanged(bsi);
				}

				CloseChoiceDialog();
			});
	}
}

void FullscreenUI::DrawStringListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* key, const char* default_value, SettingInfo::GetOptionsCallback option_callback, bool enabled, float height, std::pair<ImFont*, float> font,
	std::pair<ImFont*, float> summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<SmallString> value(
		bsi->GetOptionalSmallStringValue(section, key, game_settings ? std::nullopt : std::optional<const char*>(default_value)));

	if (MenuButtonWithValue(
			title, summary, value.has_value() ? value->c_str() : FSUI_CSTR("Use Global Setting"), enabled, height, font, summary_font))
	{
		std::vector<std::pair<std::string, std::string>> raw_options(option_callback());
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(raw_options.size() + 1);
		if (game_settings)
			cd_options.emplace_back(FSUI_STR("Use Global Setting"), !value.has_value());
		for (size_t i = 0; i < raw_options.size(); i++)
			cd_options.emplace_back(raw_options[i].second, (value.has_value() && value.value() == raw_options[i].first));
		OpenChoiceDialog(title, false, std::move(cd_options),
			[game_settings, section, key, raw_options = std::move(raw_options)](s32 index, const std::string& title, bool checked) {
				if (index >= 0)
				{
					auto lock = Host::GetSettingsLock();
					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					if (game_settings)
					{
						if (index == 0)
							bsi->DeleteValue(section, key);
						else
							bsi->SetStringValue(section, key, raw_options[index - 1].first.c_str());
					}
					else
					{
						bsi->SetStringValue(section, key, raw_options[index].first.c_str());
					}

					SetSettingsChanged(bsi);
				}

				CloseChoiceDialog();
			});
	}
}

void FullscreenUI::DrawFloatListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* key, float default_value, const char* const* options, const float* option_values, size_t option_count,
	bool translate_options, bool enabled, float height, std::pair<ImFont*, float> font, std::pair<ImFont*, float> summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<float> value(
		bsi->GetOptionalFloatValue(section, key, game_settings ? std::nullopt : std::optional<float>(default_value)));

	if (option_count == 0)
	{
		// select from null entry
		while (options && options[option_count] != nullptr)
			option_count++;
	}

	size_t index = option_count;
	if (value.has_value())
	{
		for (size_t i = 0; i < option_count; i++)
		{
			if (value == option_values[i])
			{
				index = i;
				break;
			}
		}
	}

	if (MenuButtonWithValue(title, summary,
			value.has_value() ?
				((index < option_count) ? (translate_options ? Host::TranslateToCString(TR_CONTEXT, options[index]) : options[index]) :
										  FSUI_CSTR("Unknown")) :
				FSUI_CSTR("Use Global Setting"),
			enabled, height, font, summary_font))
	{
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(option_count + 1);
		if (game_settings)
			cd_options.emplace_back(FSUI_STR("Use Global Setting"), !value.has_value());
		for (size_t i = 0; i < option_count; i++)
		{
			cd_options.emplace_back(translate_options ? Host::TranslateToString(TR_CONTEXT, options[i]) : std::string(options[i]),
				(value.has_value() && i == static_cast<size_t>(index)));
		}
		OpenChoiceDialog(title, false, std::move(cd_options),
			[game_settings, section, key, option_values](s32 index, const std::string& title, bool checked) {
				if (index >= 0)
				{
					auto lock = Host::GetSettingsLock();
					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					if (game_settings)
					{
						if (index == 0)
							bsi->DeleteValue(section, key);
						else
							bsi->SetFloatValue(section, key, option_values[index - 1]);
					}
					else
					{
						bsi->SetFloatValue(section, key, option_values[index]);
					}

					SetSettingsChanged(bsi);
				}

				CloseChoiceDialog();
			});
	}
}

template <typename DataType, typename SizeType>
void FullscreenUI::DrawEnumSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* key, DataType default_value, std::optional<DataType> (*from_string_function)(const char* str),
	const char* (*to_string_function)(DataType value), const char* (*to_display_string_function)(DataType value), SizeType option_count,
	bool enabled, float height, std::pair<ImFont*, float> font, std::pair<ImFont*, float> summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<SmallString> value(bsi->GetOptionalSmallStringValue(
		section, key, game_settings ? std::nullopt : std::optional<const char*>(to_string_function(default_value))));

	const std::optional<DataType> typed_value(value.has_value() ? from_string_function(value->c_str()) : std::nullopt);

	if (MenuButtonWithValue(title, summary,
			typed_value.has_value() ? to_display_string_function(typed_value.value()) :
									  FSUI_CSTR("Use Global Setting"),
			enabled, height, font, summary_font))
	{
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(static_cast<u32>(option_count) + 1);
		if (game_settings)
			cd_options.emplace_back(FSUI_CSTR("Use Global Setting"), !value.has_value());
		for (u32 i = 0; i < static_cast<u32>(option_count); i++)
			cd_options.emplace_back(to_display_string_function(static_cast<DataType>(i)),
				(typed_value.has_value() && i == static_cast<u32>(typed_value.value())));
		OpenChoiceDialog(
			title, false, std::move(cd_options),
			[section, key, to_string_function, game_settings](s32 index, const std::string& title, bool checked) {
				if (index >= 0)
				{
					auto lock = Host::GetSettingsLock();
					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					if (game_settings)
					{
						if (index == 0)
							bsi->DeleteValue(section, key);
						else
							bsi->SetStringValue(section, key, to_string_function(static_cast<DataType>(index - 1)));
					}
					else
					{
						bsi->SetStringValue(section, key, to_string_function(static_cast<DataType>(index)));
					}

					SetSettingsChanged(bsi);
				}

				CloseChoiceDialog();
			});
	}
}

void FullscreenUI::DrawFolderSetting(SettingsInterface* bsi, const char* title, const char* section, const char* key,
	const std::string& runtime_var, float height /* = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT */, std::pair<ImFont*, float> font /* = g_large_font */,
	std::pair<ImFont*, float> summary_font /* = g_medium_font */)
{
	if (MenuButton(title, runtime_var.c_str()))
	{
		OpenFileSelector(title, true,
			[game_settings = IsEditingGameSettings(bsi), section = std::string(section), key = std::string(key)](const std::string& dir) {
				if (dir.empty())
					return;

				auto lock = Host::GetSettingsLock();
				SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
				std::string relative_path(Path::MakeRelative(dir, EmuFolders::DataRoot));
				bsi->SetStringValue(section.c_str(), key.c_str(), relative_path.c_str());
				SetSettingsChanged(bsi);

				Host::RunOnCPUThread(&VMManager::Internal::UpdateEmuFolders);
				s_cover_image_map.clear();

				CloseFileSelector();
			});
	}
}

void FullscreenUI::DrawPathSetting(SettingsInterface* bsi, const char* title, const char* section, const char* key,
	const char* default_value, bool enabled /* = true */, float height /* = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT */,
	std::pair<ImFont*, float> font /* = g_large_font */, std::pair<ImFont*, float> summary_font /* = g_medium_font */)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<SmallString> value(
		bsi->GetOptionalSmallStringValue(section, key, game_settings ? std::nullopt : std::optional<const char*>(default_value)));

	if (MenuButton(title, value.has_value() ? value->c_str() : FSUI_CSTR("Use Global Setting")))
	{
		auto callback = [game_settings = IsEditingGameSettings(bsi), section = std::string(section), key = std::string(key)](
							const std::string& dir) {
			if (dir.empty())
				return;

			auto lock = Host::GetSettingsLock();
			SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
			std::string relative_path(Path::MakeRelative(dir, EmuFolders::DataRoot));
			bsi->SetStringValue(section.c_str(), key.c_str(), relative_path.c_str());
			SetSettingsChanged(bsi);

			Host::RunOnCPUThread(&VMManager::Internal::UpdateEmuFolders);
			s_cover_image_map.clear();

			CloseFileSelector();
		};

		std::string initial_path;
		if (value.has_value())
			initial_path = Path::GetDirectory(value.value());

		OpenFileSelector(title, false, std::move(callback), {"*"}, std::move(initial_path));
	}
}

void FullscreenUI::DrawIPAddressSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* key, const char* default_value, bool enabled, float height, std::pair<ImFont*, float> font, std::pair<ImFont*, float> summary_font, IPAddressType ip_type)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<SmallString> value(
		bsi->GetOptionalSmallStringValue(section, key, game_settings ? std::nullopt : std::optional<const char*>(default_value)));

	const SmallString value_text = value.has_value() ? value.value() : SmallString(FSUI_VSTR("Use Global Setting"));

	static std::array<int, 4> ip_octets = {0, 0, 0, 0};

	if (MenuButtonWithValue(title, summary, value_text.c_str(), enabled, height, font, summary_font))
	{
		const std::string current_ip = value.has_value() ? std::string(value->c_str()) : std::string(default_value);
		std::istringstream iss(current_ip);
		std::string segment;
		int i = 0;
		while (std::getline(iss, segment, '.') && i < 4)
		{
			ip_octets[i] = std::clamp(std::atoi(segment.c_str()), 0, 255);
			i++;
		}
		for (; i < 4; i++)
			ip_octets[i] = 0;

		char ip_str[16];
		std::snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip_octets[0], ip_octets[1], ip_octets[2], ip_octets[3]);

		const char* message;
		switch (ip_type)
		{
			case IPAddressType::DNS1:
			case IPAddressType::DNS2:
				message = FSUI_CSTR("Enter the DNS server address");
				break;
			case IPAddressType::Gateway:
				message = FSUI_CSTR("Enter the Gateway address");
				break;
			case IPAddressType::SubnetMask:
				message = FSUI_CSTR("Enter the Subnet Mask");
				break;
			case IPAddressType::PS2IP:
				message = FSUI_CSTR("Enter the PS2 IP address");
				break;
			case IPAddressType::Other:
			default:
				message = FSUI_CSTR("Enter the IP address");
				break;
		}

		ImGuiFullscreen::CloseInputDialog();

		std::string ip_str_value(ip_str);

		ImGuiFullscreen::OpenInputStringDialog(
			title,
			message,
			"",
			std::string(FSUI_ICONSTR(ICON_FA_CHECK, "OK")),
			[bsi, section, key, default_value](std::string text) {
				// Validate and clean up the IP address
				std::array<int, 4> new_octets = {0, 0, 0, 0};
				std::istringstream iss(text);
				std::string segment;
				int i = 0;
				while (std::getline(iss, segment, '.') && i < 4)
				{
					new_octets[i] = std::clamp(std::atoi(segment.c_str()), 0, 255);
					i++;
				}

				char ip_str[16];
				std::snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", new_octets[0], new_octets[1], new_octets[2], new_octets[3]);

				if (IsEditingGameSettings(bsi) && strcmp(ip_str, default_value) == 0)
					bsi->DeleteValue(section, key);
				else
					bsi->SetStringValue(section, key, ip_str);
				SetSettingsChanged(bsi);
			},
			ip_str_value,
			ImGuiFullscreen::InputFilterType::IPAddress);
	}
}

void FullscreenUI::StartAutomaticBinding(u32 port)
{
	// messy because the enumeration has to happen on the input thread
	Host::RunOnCPUThread([port]() {
		std::vector<std::pair<std::string, std::string>> devices(InputManager::EnumerateDevices());
		MTGS::RunOnGSThread([port, devices = std::move(devices)]() {
			if (devices.empty())
			{
				ShowToast({}, FSUI_STR("Automatic binding failed, no devices are available."));
				return;
			}

			std::vector<std::string> names;
			ImGuiFullscreen::ChoiceDialogOptions options;
			options.reserve(devices.size());
			names.reserve(devices.size());
			for (auto& [name, display_name] : devices)
			{
				if (!StringUtil::compareNoCase(name, display_name))
					options.emplace_back(fmt::format("{}: {}", name, display_name), false);
				else
					options.emplace_back(std::move(display_name), false);
				names.push_back(std::move(name));
			}
			OpenChoiceDialog(FSUI_CSTR("Select Device"), false, std::move(options),
				[port, names = std::move(names)](s32 index, const std::string& title, bool checked) {
					if (index < 0)
						return;

					// since this is working with the device, it has to happen on the input thread too
					Host::RunOnCPUThread([port, name = std::move(names[index])]() {
						auto lock = Host::GetSettingsLock();
						SettingsInterface* bsi = GetEditingSettingsInterface();
						const bool result = Pad::MapController(*bsi, port, InputManager::GetGenericBindingMapping(name));
						SetSettingsChanged(bsi);


						// and the toast needs to happen on the UI thread.
						MTGS::RunOnGSThread([result, name = std::move(name)]() {
							ShowToast({}, result ? fmt::format(FSUI_FSTR("Automatic mapping completed for {}."), name) :
												   fmt::format(FSUI_FSTR("Automatic mapping failed for {}."), name));
						});
					});
					CloseChoiceDialog();
				});
		});
	});
}

void FullscreenUI::DrawSettingInfoSetting(SettingsInterface* bsi, const char* section, const char* key, const SettingInfo& si,
	const char* translation_ctx)
{
	SmallString title;
	title.format(ICON_FA_GEAR " {}", Host::TranslateToStringView(translation_ctx, si.display_name));
	switch (si.type)
	{
		case SettingInfo::Type::Boolean:
			DrawToggleSetting(bsi, title.c_str(), si.description, section, key, si.BooleanDefaultValue(), true, false);
			break;

		case SettingInfo::Type::Integer:
			DrawIntRangeSetting(bsi, title.c_str(), si.description, section, key, si.IntegerDefaultValue(), si.IntegerMinValue(),
				si.IntegerMaxValue(), si.format, true);
			break;

		case SettingInfo::Type::IntegerList:
			DrawIntListSetting(
				bsi, title.c_str(), si.description, section, key, si.IntegerDefaultValue(), si.options, 0, true, si.IntegerMinValue());
			break;

		case SettingInfo::Type::Float:
			DrawFloatSpinBoxSetting(bsi, title.c_str(), si.description, section, key, si.FloatDefaultValue(), si.FloatMinValue(),
				si.FloatMaxValue(), si.FloatStepValue(), si.multiplier, si.format, true);
			break;

		case SettingInfo::Type::StringList:
		{
			if (si.get_options)
			{
				DrawStringListSetting(bsi, title.c_str(), si.description, section, key, si.StringDefaultValue(), si.get_options, true);
			}
			else
			{
				DrawStringListSetting(
					bsi, title.c_str(), si.description, section, key, si.StringDefaultValue(), si.options, si.options, 0, false, true,
					LAYOUT_MENU_BUTTON_HEIGHT, g_large_font, g_medium_font, translation_ctx);
			}
		}
		break;

		case SettingInfo::Type::Path:
			DrawPathSetting(bsi, title.c_str(), section, key, si.StringDefaultValue(), true);
			break;

		default:
			break;
	}
}

void FullscreenUI::SwitchToSettings()
{
	s_game_settings_entry.reset();
	s_game_settings_interface.reset();
	s_game_patch_list = {};
	s_enabled_game_patch_cache = {};
	s_game_cheats_list = {};
	s_enabled_game_cheat_cache = {};
	PopulateGraphicsAdapterList();

	s_current_main_window = MainWindowType::Settings;
	s_settings_page = SettingsPage::Interface;
}

void FullscreenUI::SwitchToGameSettings(const std::string_view serial, u32 crc)
{
	s_game_settings_entry.reset();
	s_game_settings_interface = std::make_unique<INISettingsInterface>(VMManager::GetGameSettingsPath(serial, crc));
	s_game_settings_interface->Load();
	PopulatePatchesAndCheatsList(serial, crc);
	s_current_main_window = MainWindowType::Settings;
	s_settings_page = SettingsPage::Summary;
	QueueResetFocus(FocusResetType::WindowChanged);
}

void FullscreenUI::SwitchToGameSettings()
{
	if (s_current_disc_serial.empty() || s_current_disc_crc == 0)
		return;

	auto lock = GameList::GetLock();
	const GameList::Entry* entry = GameList::GetEntryForPath(s_current_disc_path.c_str());
	if (!entry)
		entry = GameList::GetEntryBySerialAndCRC(s_current_disc_serial.c_str(), s_current_disc_crc);

	if (entry)
		SwitchToGameSettings(entry);
}

void FullscreenUI::SwitchToGameSettings(const std::string& path)
{
	auto lock = GameList::GetLock();
	const GameList::Entry* entry = GameList::GetEntryForPath(path.c_str());
	if (entry)
		SwitchToGameSettings(entry);
}

void FullscreenUI::SwitchToGameSettings(const GameList::Entry* entry)
{
	SwitchToGameSettings((entry->type != GameList::EntryType::ELF) ? std::string_view(entry->serial) : std::string_view(), entry->crc);
	s_game_settings_entry = std::make_unique<GameList::Entry>(*entry);
}

void FullscreenUI::PopulateGraphicsAdapterList()
{
	s_graphics_adapter_list_cache = GSGetAdapterInfo(GSConfig.Renderer);
}

void FullscreenUI::PopulateGameListDirectoryCache(SettingsInterface* si)
{
	s_game_list_directories_cache.clear();
	for (std::string& dir : si->GetStringList("GameList", "Paths"))
		s_game_list_directories_cache.emplace_back(std::move(dir), false);
	for (std::string& dir : si->GetStringList("GameList", "RecursivePaths"))
		s_game_list_directories_cache.emplace_back(std::move(dir), true);
}

void FullscreenUI::PopulatePatchesAndCheatsList(const std::string_view serial, u32 crc)
{
	constexpr auto sort_patches = [](std::vector<Patch::PatchInfo>& list) {
		std::sort(list.begin(), list.end(), [](const Patch::PatchInfo& lhs, const Patch::PatchInfo& rhs) { return lhs.name < rhs.name; });
	};

	s_game_patch_list = Patch::GetPatchInfo(serial, crc, false, true, nullptr);
	sort_patches(s_game_patch_list);
	s_game_cheats_list = Patch::GetPatchInfo(serial, crc, true, true, &s_game_cheat_unlabelled_count);
	sort_patches(s_game_cheats_list);

	pxAssert(s_game_settings_interface);
	s_enabled_game_patch_cache = s_game_settings_interface->GetStringList(Patch::PATCHES_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
	s_enabled_game_cheat_cache = s_game_settings_interface->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
}

void FullscreenUI::DoCopyGameSettings()
{
	if (!s_game_settings_interface)
		return;

	Pcsx2Config::CopyConfiguration(s_game_settings_interface.get(), *GetEditingSettingsInterface(false));
	Pcsx2Config::ClearInvalidPerGameConfiguration(s_game_settings_interface.get());

	SetSettingsChanged(s_game_settings_interface.get());

	ShowToast(std::string(), fmt::format(FSUI_FSTR("Game settings initialized with global settings for '{}'."),
								 Path::GetFileTitle(s_game_settings_interface->GetFileName())));
}

void FullscreenUI::DoClearGameSettings()
{
	if (!s_game_settings_interface)
		return;

	Pcsx2Config::ClearConfiguration(s_game_settings_interface.get());

	SetSettingsChanged(s_game_settings_interface.get());

	ShowToast(std::string(),
		fmt::format(FSUI_FSTR("Game settings have been cleared for '{}'."), Path::GetFileTitle(s_game_settings_interface->GetFileName())));
}

void FullscreenUI::DrawSettingsWindow()
{
	ImGuiIO& io = ImGui::GetIO();
	const ImVec2 heading_size =
		ImVec2(io.DisplaySize.x, LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY) +
									 (LayoutScale(LAYOUT_MENU_BUTTON_Y_PADDING) * 2.0f) + LayoutScale(2.0f));

	const bool using_custom_bg = !VMManager::HasValidVM() && s_custom_background_enabled && s_custom_background_texture;
	const float header_bg_alpha = VMManager::HasValidVM() ? 0.90f : 1.0f;
	const float content_bg_alpha = using_custom_bg ? 0.0f : (VMManager::HasValidVM() ? 0.90f : 1.0f);
	SettingsInterface* bsi = GetEditingSettingsInterface();
	const bool game_settings = IsEditingGameSettings(bsi);
	const bool show_advanced_settings = ShouldShowAdvancedSettings(bsi);

	if (BeginFullscreenWindow(
			ImVec2(0.0f, 0.0f), heading_size, "settings_category", ImVec4(UIPrimaryColor.x, UIPrimaryColor.y, UIPrimaryColor.z, header_bg_alpha)))
	{
		static constexpr float ITEM_WIDTH = 25.0f;

		static constexpr const char* global_icons[] = {
			ICON_FA_TV,
			ICON_PF_MICROCHIP,
			ICON_PF_GEARS_OPTIONS_SETTINGS,
			ICON_PF_PICTURE,
			ICON_PF_SOUND,
			ICON_PF_MEMORY_CARD,
			ICON_FA_NETWORK_WIRED,
			ICON_FA_FOLDER_OPEN,
			ICON_FA_TROPHY,
			ICON_PF_GAMEPAD_ALT,
			ICON_PF_KEYBOARD_ALT,
			ICON_FA_TRIANGLE_EXCLAMATION,
		};
		static constexpr const char* per_game_icons[] = {
			ICON_FA_INFO,
			ICON_PF_GEARS_OPTIONS_SETTINGS,
			ICON_FA_BANDAGE,
			ICON_PF_INFINITY,
			ICON_PF_PICTURE,
			ICON_PF_SOUND,
			ICON_PF_MEMORY_CARD,
			ICON_FA_TRIANGLE_EXCLAMATION,
		};
		static constexpr SettingsPage global_pages[] = {
			SettingsPage::Interface,
			SettingsPage::BIOS,
			SettingsPage::Emulation,
			SettingsPage::Graphics,
			SettingsPage::Audio,
			SettingsPage::MemoryCard,
			SettingsPage::NetworkHDD,
			SettingsPage::Folders,
			SettingsPage::Achievements,
			SettingsPage::Controller,
			SettingsPage::Hotkey,
			SettingsPage::Advanced,
		};
		static constexpr SettingsPage per_game_pages[] = {
			SettingsPage::Summary,
			SettingsPage::Emulation,
			SettingsPage::Patches,
			SettingsPage::Cheats,
			SettingsPage::Graphics,
			SettingsPage::Audio,
			SettingsPage::MemoryCard,
			SettingsPage::GameFixes,
		};
		static constexpr const char* titles[] = {
			FSUI_NSTR("Summary"),
			FSUI_NSTR("Interface Settings"),
			FSUI_NSTR("BIOS Settings"),
			FSUI_NSTR("Emulation Settings"),
			FSUI_NSTR("Graphics Settings"),
			FSUI_NSTR("Audio Settings"),
			FSUI_NSTR("Memory Card Settings"),
			FSUI_NSTR("Network & HDD Settings"),
			FSUI_NSTR("Folder Settings"),
			FSUI_NSTR("Achievements Settings"),
			FSUI_NSTR("Controller Settings"),
			FSUI_NSTR("Hotkey Settings"),
			FSUI_NSTR("Advanced Settings"),
			FSUI_NSTR("Patches"),
			FSUI_NSTR("Cheats"),
			FSUI_NSTR("Game Fixes"),
		};

		const u32 count = game_settings ? (show_advanced_settings ? std::size(per_game_pages) : (std::size(per_game_pages) - 1)) : std::size(global_pages);
		const char* const* icons = game_settings ? per_game_icons : global_icons;
		const SettingsPage* pages = game_settings ? per_game_pages : global_pages;
		u32 index = 0;
		for (u32 i = 0; i < count; i++)
		{
			if (pages[i] == s_settings_page)
			{
				index = i;
				break;
			}
		}

		BeginNavBar();

		if (!ImGui::IsPopupOpen(0u, ImGuiPopupFlags_AnyPopup))
		{
			if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft, true) ||
				ImGui::IsKeyPressed(ImGuiKey_NavGamepadTweakSlow, true) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
			{
				index = (index == 0) ? (count - 1) : (index - 1);
				s_settings_page = pages[index];
				QueueResetFocus(FocusResetType::WindowChanged);
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight, true) ||
					 ImGui::IsKeyPressed(ImGuiKey_NavGamepadTweakFast, true) ||
					 ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
			{
				index = (index + 1) % count;
				s_settings_page = pages[index];
				QueueResetFocus(FocusResetType::WindowChanged);
			}
		}

		if (NavButton(ICON_PF_BACKWARD, true, true))
		{
			if (VMManager::HasValidVM())
				ReturnToPreviousWindow();
			else
				SwitchToLanding();
		}

		if (s_game_settings_entry)
		{
			NavTitle(SmallString::from_format(
				"{} ({})", Host::TranslateToCString(TR_CONTEXT, titles[static_cast<u32>(pages[index])]), s_game_settings_entry->GetTitle(true)));
		}
		else
		{
			NavTitle(Host::TranslateToCString(TR_CONTEXT, titles[static_cast<u32>(pages[index])]));
		}

		RightAlignNavButtons(count, ITEM_WIDTH, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

		for (u32 i = 0; i < count; i++)
		{
			if (NavButton(icons[i], i == index, true, ITEM_WIDTH, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
			{
				s_settings_page = pages[i];
				QueueResetFocus(FocusResetType::WindowChanged);
			}
		}

		EndNavBar();
	}

	EndFullscreenWindow();

	// we have to do this here, because otherwise it uses target, and jumps a frame later.
	if (IsFocusResetQueued())
		if (FocusResetType focus_reset = GetQueuedFocusResetType(); focus_reset != FocusResetType::None &&
																	focus_reset != FocusResetType::PopupOpened &&
																	focus_reset != FocusResetType::PopupClosed)
		{
			ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
		}

	if (BeginFullscreenWindow(
			ImVec2(0.0f, heading_size.y),
			ImVec2(io.DisplaySize.x, io.DisplaySize.y - heading_size.y - LayoutScale(LAYOUT_FOOTER_HEIGHT)),
			TinyString::from_format("settings_page_{}", static_cast<u32>(s_settings_page)).c_str(),
			ImVec4(UIBackgroundColor.x, UIBackgroundColor.y, UIBackgroundColor.z, content_bg_alpha), 0.0f,
			ImVec2(ImGuiFullscreen::LAYOUT_MENU_WINDOW_X_PADDING, 0.0f)))
	{
		ResetFocusHere();

		if (ImGui::IsWindowFocused() && WantsToCloseMenu())
			ReturnToPreviousWindow();

		auto lock = Host::GetSettingsLock();

		switch (s_settings_page)
		{
			case SettingsPage::Summary:
				DrawSummarySettingsPage();
				break;

			case SettingsPage::Interface:
				DrawInterfaceSettingsPage();
				break;

			case SettingsPage::BIOS:
				DrawBIOSSettingsPage();
				break;

			case SettingsPage::Emulation:
				DrawEmulationSettingsPage();
				break;

			case SettingsPage::Graphics:
				DrawGraphicsSettingsPage(bsi, show_advanced_settings);
				break;

			case SettingsPage::Audio:
				DrawAudioSettingsPage();
				break;

			case SettingsPage::MemoryCard:
				DrawMemoryCardSettingsPage();
				break;

			case SettingsPage::NetworkHDD:
				DrawNetworkHDDSettingsPage();
				break;

			case SettingsPage::Folders:
				DrawFoldersSettingsPage();
				break;

			case SettingsPage::Achievements:
				DrawAchievementsSettingsPage(lock);
				break;

			case SettingsPage::Controller:
				DrawControllerSettingsPage();
				break;

			case SettingsPage::Hotkey:
				DrawHotkeySettingsPage();
				break;

			case SettingsPage::Patches:
				DrawPatchesOrCheatsSettingsPage(false);
				break;

			case SettingsPage::Cheats:
				DrawPatchesOrCheatsSettingsPage(true);
				break;

			case SettingsPage::Advanced:
				DrawAdvancedSettingsPage();
				break;

			case SettingsPage::GameFixes:
				DrawGameFixesSettingsPage();
				break;

			default:
				break;
		}
	}

	EndFullscreenWindow();

	if (IsGamepadInputSource())
	{
		const bool circleOK = ImGui::GetIO().ConfigNavSwapGamepadButtons;
		SetFullscreenFooterText(std::array{
			std::make_pair(ICON_PF_DPAD_LEFT_RIGHT, FSUI_VSTR("Change Page")),
			std::make_pair(ICON_PF_DPAD_UP_DOWN, FSUI_VSTR("Navigate")),
			std::make_pair(circleOK ? ICON_PF_BUTTON_CIRCLE : ICON_PF_BUTTON_CROSS, FSUI_VSTR("Select")),
			std::make_pair(circleOK ? ICON_PF_BUTTON_CROSS : ICON_PF_BUTTON_CIRCLE, FSUI_VSTR("Back")),
		});
	}
	else
	{
		SetFullscreenFooterText(std::array{
			std::make_pair(ICON_PF_ARROW_LEFT ICON_PF_ARROW_RIGHT, FSUI_VSTR("Change Page")),
			std::make_pair(ICON_PF_ARROW_UP ICON_PF_ARROW_DOWN, FSUI_VSTR("Navigate")),
			std::make_pair(ICON_PF_ENTER, FSUI_VSTR("Select")),
			std::make_pair(ICON_PF_ESC, FSUI_VSTR("Back")),
		});
	}
}

void FullscreenUI::DrawSummarySettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading(FSUI_CSTR("Details"));

	if (s_game_settings_entry)
	{
		if (MenuButton(FSUI_ICONSTR(ICON_FA_TAG, "Title"), s_game_settings_entry->GetTitle(true).c_str(), true))
			CopyTextToClipboard(FSUI_STR("Game title copied to clipboard."), s_game_settings_entry->GetTitle(true));
		if (MenuButton(FSUI_ICONSTR(ICON_FA_PAGER, "Serial"), s_game_settings_entry->serial.c_str(), true))
			CopyTextToClipboard(FSUI_STR("Game serial copied to clipboard."), s_game_settings_entry->serial);
		if (MenuButton(FSUI_ICONSTR(ICON_FA_CODE, "CRC"), fmt::format("{:08X}", s_game_settings_entry->crc).c_str(), true))
			CopyTextToClipboard(FSUI_STR("Game CRC copied to clipboard."), fmt::format("{:08X}", s_game_settings_entry->crc));
		if (MenuButton(FSUI_ICONSTR(ICON_FA_BOX, "Type"), GameList::EntryTypeToString(s_game_settings_entry->type, true), true))
			CopyTextToClipboard(FSUI_STR("Game type copied to clipboard."), GameList::EntryTypeToString(s_game_settings_entry->type, true));
		if (MenuButton(FSUI_ICONSTR(ICON_FA_GLOBE, "Region"), GameList::RegionToString(s_game_settings_entry->region, true), true))
			CopyTextToClipboard(FSUI_STR("Game region copied to clipboard."), GameList::RegionToString(s_game_settings_entry->region, true));
		if (MenuButton(FSUI_ICONSTR(ICON_FA_STAR, "Compatibility Rating"),
				GameList::EntryCompatibilityRatingToString(s_game_settings_entry->compatibility_rating, true), true))
		{
			CopyTextToClipboard(FSUI_STR("Game compatibility copied to clipboard."),
				GameList::EntryCompatibilityRatingToString(s_game_settings_entry->compatibility_rating, true));
		}
		if (MenuButton(FSUI_ICONSTR(ICON_FA_FOLDER_OPEN, "Path"), s_game_settings_entry->path.c_str(), true))
			CopyTextToClipboard(FSUI_STR("Game path copied to clipboard."), s_game_settings_entry->path);

		if (s_game_settings_entry->type == GameList::EntryType::ELF)
		{
			const SmallString iso_path = bsi->GetSmallStringValue("EmuCore", "DiscPath");
			if (MenuButton(FSUI_ICONSTR(ICON_FA_COMPACT_DISC, "Disc Path"), iso_path.empty() ? "No Disc" : iso_path.c_str()))
			{
				auto callback = [](const std::string& path) {
					if (!path.empty())
					{
						{
							auto lock = Host::GetSettingsLock();
							if (s_game_settings_interface)
							{
								s_game_settings_interface->SetStringValue("EmuCore", "DiscPath", path.c_str());
								s_game_settings_interface->Save();
							}
						}

						if (s_game_settings_entry)
						{
							// re-scan the entry to update its serial.
							if (GameList::RescanPath(s_game_settings_entry->path))
							{
								auto lock = GameList::GetLock();
								const GameList::Entry* entry = GameList::GetEntryForPath(s_game_settings_entry->path.c_str());
								if (entry)
									*s_game_settings_entry = *entry;
							}
						}
					}

					QueueResetFocus(FocusResetType::PopupClosed);
					CloseFileSelector();
				};

				OpenFileSelector(FSUI_ICONSTR(ICON_FA_COMPACT_DISC, "Select Disc Path"), false, std::move(callback), GetDiscImageFilters());
			}
		}

		const std::optional<SmallString> value = bsi->GetOptionalSmallStringValue("EmuCore", "InputProfileName", "Shared");

		if (MenuButtonWithValue(FSUI_ICONSTR_S(ICON_PF_GAMEPAD_ALT, "Input Profile", "input_profile"),
				FSUI_CSTR("The selected input profile will be used for this game."),
				value.has_value() ? value->c_str() : FSUI_CSTR("Shared"), true))
		{
			ImGuiFullscreen::ChoiceDialogOptions options;
			std::vector<std::string> names;

			options.emplace_back(fmt::format(FSUI_FSTR("Shared")), (value.has_value() && !value->empty() && value == "Shared") ? true : false);
			names.emplace_back("Shared");

			for (const std::string& name : Pad::GetInputProfileNames())
			{
				options.emplace_back(name, (value.has_value() && !value->empty() && value == name) ? true : false);
				names.push_back(std::move(name));
			}

			OpenChoiceDialog(FSUI_CSTR("Input Profile"), false, options,
				[game_settings = IsEditingGameSettings(bsi), names = std::move(names)](s32 index, const std::string& title, bool checked) {
					if (index < 0)
						return;

					auto lock = Host::GetSettingsLock();
					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					bsi->SetStringValue("EmuCore", "InputProfileName", names[index].c_str());
					SetSettingsChanged(bsi);
					CloseChoiceDialog();
				});
		}
	}
	else
	{
		MenuButton(FSUI_ICONSTR(ICON_FA_BAN, "Cannot show details for games which were not scanned in the game list."), "");
	}

	MenuHeading(FSUI_CSTR("Options"));

	if (MenuButton(FSUI_ICONSTR(ICON_FA_COPY, "Copy Settings"), FSUI_CSTR("Copies the current global settings to this game.")))
		DoCopyGameSettings();
	if (MenuButton(FSUI_ICONSTR(ICON_FA_TRASH, "Clear Settings"), FSUI_CSTR("Clears all settings set for this game.")))
		DoClearGameSettings();

	EndMenuButtons();
}

void FullscreenUI::DrawInterfaceSettingsPage()
{
	static constexpr const char* s_theme_name[] = {
		FSUI_NSTR("Dark"),
		FSUI_NSTR("Light"),
		FSUI_NSTR("Grey Matter"),
		FSUI_NSTR("Untouched Lagoon"),
		FSUI_NSTR("Baby Pastel"),
		FSUI_NSTR("Pizza Time!"),
		FSUI_NSTR("PCSX2 Blue"),
		FSUI_NSTR("Scarlet Devil"),
		FSUI_NSTR("Violet Angel"),
		FSUI_NSTR("Cobalt Sky"),
		FSUI_NSTR("AMOLED"),
	};

	static constexpr const char* s_theme_value[] = {
		"Dark",
		"Light",
		"GreyMatter",
		"UntouchedLagoon",
		"BabyPastel",
		"PizzaBrown",
		"PCSX2Blue",
		"ScarletDevil",
		"VioletAngel",
		"CobaltSky",
		"AMOLED",
	};

	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading(FSUI_CSTR("Appearance"));
	DrawStringListSetting(bsi, FSUI_ICONSTR(ICON_FA_PAINTBRUSH, "Theme"),
		FSUI_CSTR("Selects the color style to be used for Big Picture Mode."),
		"UI", "FullscreenUITheme", "Dark", s_theme_name, s_theme_value, std::size(s_theme_name), true);
	DrawToggleSetting(
		bsi, FSUI_ICONSTR(ICON_FA_LIST, "Default To Game List"), FSUI_CSTR("When Big Picture mode is started, the game list will be displayed instead of the main menu."), "UI", "FullscreenUIDefaultToGameList", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_CIRCLE_INFO, "Use Save State Selector"),
		FSUI_CSTR("Show a save state selector UI when switching slots instead of showing a notification bubble."),
		"EmuCore", "UseSavestateSelector", true);

	MenuHeading(FSUI_CSTR("Background"));

	std::string background_path = bsi->GetStringValue("UI", "FSUIBackgroundPath", "");

	std::string background_display = FSUI_STR("None");
	if (!background_path.empty())
	{
		background_display = Path::GetFileName(background_path);
	}

	if (MenuButtonWithValue(FSUI_ICONSTR(ICON_FA_IMAGE, "Background Image"),
			FSUI_CSTR("Select a custom background image to use in Big Picture Mode menus.\n\nSupported formats: PNG, JPG, JPEG, BMP."),
			background_display.c_str()))
	{
		OpenFileSelector(FSUI_ICONSTR(ICON_FA_IMAGE, "Select Background Image"), false, [](const std::string& path) {
				if (!path.empty())
				{
					{
						auto lock = Host::GetSettingsLock();
						SettingsInterface* bsi = GetEditingSettingsInterface(false);

						std::string relative_path = Path::MakeRelative(path, EmuFolders::DataRoot);
						bsi->SetStringValue("UI", "FSUIBackgroundPath", relative_path.c_str());
						bsi->SetBoolValue("UI", "FSUIBackgroundEnabled", true);
						SetSettingsChanged(bsi);
					}

					LoadCustomBackground();
				}
				CloseFileSelector(); }, GetImageFileFilters());
	}

	if (MenuButton(FSUI_ICONSTR(ICON_FA_XMARK, "Clear Background Image"),
			FSUI_CSTR("Removes the custom background image.")))
	{
		bsi->DeleteValue("UI", "FSUIBackgroundPath");
		SetSettingsChanged(bsi);

		s_custom_background_texture.reset();
		s_custom_background_path.clear();
		s_custom_background_enabled = false;
	}

	DrawIntRangeSetting(bsi, FSUI_ICONSTR(ICON_FA_DROPLET, "Background Opacity"),
		FSUI_CSTR("Sets the transparency of the custom background image."),
		"UI", "FSUIBackgroundOpacity", 100, 0, 100, "%d%%");

	static constexpr const char* s_background_mode_names[] = {
		FSUI_NSTR("Fit"),
		FSUI_NSTR("Fill"),
		FSUI_NSTR("Stretch"),
		FSUI_NSTR("Center"),
		FSUI_NSTR("Tile"),
	};
	static constexpr const char* s_background_mode_values[] = {
		"fit",
		"fill",
		"stretch",
		"center",
		"tile",
	};
	DrawStringListSetting(bsi, FSUI_ICONSTR(ICON_FA_EXPAND, "Background Mode"),
		FSUI_CSTR("Select how to display the background image."),
		"UI", "FSUIBackgroundMode", "fit", s_background_mode_names, s_background_mode_values, std::size(s_background_mode_names), true);

	MenuHeading(FSUI_CSTR("Behaviour"));
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_PF_SNOOZE, "Inhibit Screensaver"),
		FSUI_CSTR("Prevents the screen saver from activating and the host from sleeping while emulation is running."), "EmuCore",
		"InhibitScreensaver", true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_PAUSE, "Pause On Start"), FSUI_CSTR("Pauses the emulator when a game is started."), "UI",
		"StartPaused", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_EYE, "Pause On Focus Loss"),
		FSUI_CSTR("Pauses the emulator when you minimize the window or switch to another application, and unpauses when you switch back."),
		"UI", "PauseOnFocusLoss", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_GAMEPAD, "Pause On Controller Disconnection"),
		FSUI_CSTR("Pauses the emulator when a controller with bindings is disconnected."), "UI", "PauseOnControllerDisconnection", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_RECTANGLE_LIST, "Pause On Menu"),
		FSUI_CSTR("Pauses the emulator when you open the quick menu, and unpauses when you close it."), "UI", "PauseOnMenu", true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_FLOPPY_DISK, "Prompt On State Load/Save Failure"),
		FSUI_CSTR("Display a modal dialog when a save state load/save operation fails."), "UI", "PromptOnStateLoadSaveFailure", true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_POWER_OFF, "Confirm Shutdown"),
		FSUI_CSTR("Determines whether a prompt will be displayed to confirm shutting down the emulator/game when the hotkey is pressed."),
		"UI", "ConfirmShutdown", true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_FLOPPY_DISK, "Save State On Shutdown"),
		FSUI_CSTR("Automatically saves the emulator state when powering down or exiting. You can then resume directly from where you left "
				  "off next time."),
		"EmuCore", "SaveStateOnShutdown", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_BOX_ARCHIVE, "Create Save State Backups"),
		FSUI_CSTR("Creates a backup copy of a save state if it already exists when the save is created. The backup copy has a .backup suffix"),
		"EmuCore", "BackupSavestate", true);
	// DrawStringListSetting dosn't have a callback for applying settings
	const SmallString swap_mode = bsi->GetSmallStringValue("UI", "SwapOKFullscreenUI", "auto");
	static constexpr const char* swap_names[] = {
		FSUI_NSTR("Automatic"),
		FSUI_NSTR("Enabled"),
		FSUI_NSTR("Disabled"),
	};
	static constexpr const char* swap_values[] = {
		"auto",
		"true",
		"false",
	};
	size_t swap_index = std::size(swap_values);
	for (size_t i = 0; i < std::size(swap_values); i++)
	{
		if (swap_mode == swap_values[i])
		{
			swap_index = i;
			break;
		}
	}

	SmallStackString<256> swap_summery;
	swap_summery.format(FSUI_FSTR("Uses {} as confirm when using a controller."), ICON_PF_BUTTON_CIRCLE);
	if (MenuButtonWithValue(FSUI_ICONSTR(ICON_FA_GAMEPAD, "Swap OK/Cancel in Big Picture Mode"), swap_summery.c_str(),
			(swap_index < std::size(swap_values)) ? Host::TranslateToCString(TR_CONTEXT, swap_names[swap_index]) : FSUI_CSTR("Unknown")))
	{
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(std::size(swap_values));
		for (size_t i = 0; i < std::size(swap_values); i++)
			cd_options.emplace_back(Host::TranslateToString(TR_CONTEXT, swap_names[i]), i == static_cast<size_t>(swap_index));

		OpenChoiceDialog(FSUI_ICONSTR(ICON_FA_GAMEPAD, "Swap OK/Cancel in Big Picture Mode"), false, std::move(cd_options), [](s32 index, const std::string& title, bool checked) {
			if (index >= 0)
			{
				auto lock = Host::GetSettingsLock();
				SettingsInterface* bsi = GetEditingSettingsInterface(false);
				bsi->SetStringValue("UI", "SwapOKFullscreenUI", swap_values[index]);
				SetSettingsChanged(bsi);
				ApplyLayoutSettings(bsi);
			}

			CloseChoiceDialog();
		});
	}

	const SmallString nintendo_mode = bsi->GetSmallStringValue("UI", "SDL2NintendoLayout", "false");
	size_t nintendo_index = std::size(swap_values);
	for (size_t i = 0; i < std::size(swap_values); i++)
	{
		if (nintendo_mode == swap_values[i])
		{
			nintendo_index = i;
			break;
		}
	}
	swap_summery.format(FSUI_FSTR("Swaps both {}/{} (When Swap OK/Cancel is set to automatic) and {}/{} buttons"), ICON_PF_BUTTON_CROSS, ICON_PF_BUTTON_CIRCLE, ICON_PF_BUTTON_SQUARE, ICON_PF_BUTTON_TRIANGLE);
	if (MenuButtonWithValue(FSUI_ICONSTR(ICON_FA_GAMEPAD, "Use Legacy Nintendo Layout in Big Picture Mode"), swap_summery.c_str(),
			(nintendo_index < std::size(swap_values)) ? Host::TranslateToCString(TR_CONTEXT, swap_names[nintendo_index]) : FSUI_CSTR("Unknown")))
	{
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(std::size(swap_values));
		for (size_t i = 0; i < std::size(swap_values); i++)
			cd_options.emplace_back(Host::TranslateToString(TR_CONTEXT, swap_names[i]), i == static_cast<size_t>(nintendo_index));

		OpenChoiceDialog(FSUI_ICONSTR(ICON_FA_GAMEPAD, "Use Legacy Nintendo Layout in Big Picture Mode"), false, std::move(cd_options), [](s32 index, const std::string& title, bool checked) {
			if (index >= 0)
			{
				auto lock = Host::GetSettingsLock();
				SettingsInterface* bsi = GetEditingSettingsInterface(false);
				bsi->SetStringValue("UI", "SDL2NintendoLayout", swap_values[index]);
				SetSettingsChanged(bsi);
				ApplyLayoutSettings(bsi);
			}

			CloseChoiceDialog();
		});
	}

	MenuHeading(FSUI_CSTR("Integration"));
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_CIRCLE_USER, "Enable Discord Presence"),
		FSUI_CSTR("Shows the game you are currently playing as part of your profile on Discord."), "EmuCore", "EnableDiscordPresence", false);

	MenuHeading(FSUI_CSTR("Game Display"));
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_TV, "Start Fullscreen"),
		FSUI_CSTR("Automatically switches to fullscreen mode when a game is started."), "UI", "StartFullscreen", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_COMPUTER_MOUSE, "Double-Click Toggles Fullscreen"),
		FSUI_CSTR("Switches between full screen and windowed when the window is double-clicked."), "UI", "DoubleClickTogglesFullscreen",
		true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROW_POINTER, "Hide Cursor In Fullscreen"),
		FSUI_CSTR("Hides the mouse pointer/cursor when the emulator is in fullscreen mode."), "UI", "HideMouseCursor", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_TABLET_SCREEN_BUTTON, "Start Big Picture UI"),
		FSUI_CSTR("Automatically starts Big Picture Mode instead of the regular Qt interface when PCSX2 launches."), "UI", "StartBigPictureMode", false);

	MenuHeading(FSUI_CSTR("On-Screen Display"));
	DrawIntSpinBoxSetting(bsi, FSUI_ICONSTR(ICON_FA_MAGNIFYING_GLASS, "OSD Scale"),
		FSUI_CSTR("Determines how large the on-screen messages and monitors are."), "EmuCore/GS", "OsdScale", 100, 25, 500, 1, FSUI_CSTR("%d%%"));

	// OSD Positioning Options
	static constexpr const char* s_osd_position_options[] = {
		FSUI_NSTR("None"),
		FSUI_NSTR("Top Left"),
		FSUI_NSTR("Top Center"),
		FSUI_NSTR("Top Right"),
		FSUI_NSTR("Center Left"),
		FSUI_NSTR("Center"),
		FSUI_NSTR("Center Right"),
		FSUI_NSTR("Bottom Left"),
		FSUI_NSTR("Bottom Center"),
		FSUI_NSTR("Bottom Right"),
	};
	static constexpr const char* s_osd_position_values[] = {
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

	DrawStringListSetting(bsi, FSUI_ICONSTR(ICON_FA_COMMENT, "OSD Messages Position"),
		FSUI_CSTR("Determines where on-screen display messages are positioned."), "EmuCore/GS", "OsdMessagesPos", "1",
		s_osd_position_options, s_osd_position_values, std::size(s_osd_position_options), true);
	DrawStringListSetting(bsi, FSUI_ICONSTR(ICON_FA_CHART_BAR, "OSD Performance Position"),
		FSUI_CSTR("Determines where performance statistics are positioned."), "EmuCore/GS", "OsdPerformancePos", "3",
		s_osd_position_options, s_osd_position_values, std::size(s_osd_position_options), true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_CODE_MERGE, "Show PCSX2 Version"),
		FSUI_CSTR("Shows the current PCSX2 version."), "EmuCore/GS",
		"OsdShowVersion", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_GAUGE_SIMPLE_HIGH, "Show Speed"),
		FSUI_CSTR("Shows the current emulation speed of the system as a percentage."), "EmuCore/GS",
		"OsdShowSpeed", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_FILM, "Show FPS"),
		FSUI_CSTR("Shows the number of internal video frames displayed per second by the system."),
		"EmuCore/GS", "OsdShowFPS", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_CLAPPERBOARD, "Show VPS"),
		FSUI_CSTR("Shows the number of Vsyncs performed per second by the system."), "EmuCore/GS", "OsdShowVPS", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_PF_MONITOR_CODE, "Show Resolution"),
		FSUI_CSTR("Shows the internal resolution of the game."), "EmuCore/GS", "OsdShowResolution", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_COMPUTER, "Show Hardware Info"),
		FSUI_CSTR("Shows the current system CPU and GPU information."), "EmuCore/GS", "OsdShowHardwareInfo", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_CHART_PIE, "Show GS Statistics"),
		FSUI_CSTR("Shows statistics about the emulated GS such as primitives and draw calls."),
		"EmuCore/GS", "OsdShowGSStats", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_PF_MICROCHIP_ALT, "Show CPU Usage"),
		FSUI_CSTR("Shows the host's CPU utilization based on threads."), "EmuCore/GS", "OsdShowCPU", false);
	// TODO: Change this to a GPU icon when FA gets one or PromptFont fixes their codepoints.
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_IMAGE, "Show GPU Usage"),
		FSUI_CSTR("Shows the host's GPU utilization."), "EmuCore/GS", "OsdShowGPU", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_PLAY, "Show Status Indicators"),
		FSUI_CSTR("Shows indicators when fast forwarding, pausing, and other abnormal states are active."), "EmuCore/GS",
		"OsdShowIndicators", true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_PF_HEARTBEAT_ALT, "Show Frame Times"),
		FSUI_CSTR("Shows a visual history of frame times."), "EmuCore/GS", "OsdShowFrameTimes", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_SLIDERS, "Show Settings"),
		FSUI_CSTR("Shows the current configuration in the bottom-right corner of the display."),
		"EmuCore/GS", "OsdShowSettings", false);
	bool show_settings = (bsi->GetBoolValue("EmuCore/GS", "OsdShowSettings", false) == false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_HAMMER, "Show Patches"),
		FSUI_CSTR("Shows the amount of currently active patches/cheats on the bottom-right corner of the display."), "EmuCore/GS",
		"OsdshowPatches", false, !show_settings);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_PF_GAMEPAD_ALT, "Show Inputs"),
		FSUI_CSTR("Shows the current controller state of the system in the bottom-left corner of the display."), "EmuCore/GS",
		"OsdShowInputs", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_VIDEO, "Show Video Capture Status"),
		FSUI_CSTR("Shows the status of the currently active video capture."), "EmuCore/GS",
		"OsdShowVideoCapture", true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_KEYBOARD, "Show Input Recording Status"),
		FSUI_CSTR("Shows the status of the currently active input recording."), "EmuCore/GS",
		"OsdShowInputRec", true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_IMAGES, "Show Texture Replacement Status"),
		FSUI_CSTR("Shows the number of dumped and loaded texture replacements on the OSD."), "EmuCore/GS",
		"OsdShowTextureReplacements", true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_TRIANGLE_EXCLAMATION, "Warn About Unsafe Settings"),
		FSUI_CSTR("Displays warnings when settings are enabled which may break games."), "EmuCore", "WarnAboutUnsafeSettings", true);

	MenuHeading(FSUI_CSTR("Operations"));
	if (MenuButton(FSUI_ICONSTR(u8"🔥", "Reset Settings"),
			FSUI_CSTR("Resets configuration to defaults (excluding controller settings)."), !IsEditingGameSettings(bsi)))
	{
		DoResetSettings();
	}

	EndMenuButtons();
}

void FullscreenUI::DrawBIOSSettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading(FSUI_CSTR("BIOS Configuration"));

	DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_FA_FOLDER_OPEN, "Change Search Directory"), "Folders", "Bios", EmuFolders::Bios);

	const SmallString bios_selection = GetEditingSettingsInterface()->GetSmallStringValue("Filenames", "BIOS", "");
	if (MenuButtonWithValue(FSUI_ICONSTR(ICON_PF_MICROCHIP, "BIOS Selection"),
			FSUI_CSTR("Changes the BIOS image used to start future sessions."),
			bios_selection.empty() ? FSUI_CSTR("Automatic") : bios_selection.c_str()))
	{
		ImGuiFullscreen::ChoiceDialogOptions choices;
		choices.emplace_back(FSUI_STR("Automatic"), bios_selection.empty());

		std::vector<std::string> values;
		values.push_back("");

		FileSystem::FindResultsArray results;
		FileSystem::FindFiles(EmuFolders::Bios.c_str(), "*", FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES, &results);
		for (const FILESYSTEM_FIND_DATA& fd : results)
		{
			u32 version, region;
			std::string description, zone;
			if (!IsBIOS(fd.FileName.c_str(), version, description, region, zone))
				continue;

			const std::string_view filename(Path::GetFileName(fd.FileName));
			choices.emplace_back(fmt::format("{} ({})", description, filename), bios_selection == filename);
			values.emplace_back(filename);
		}

		OpenChoiceDialog(FSUI_CSTR("BIOS Selection"), false, std::move(choices),
			[game_settings = IsEditingGameSettings(bsi), values = std::move(values)](s32 index, const std::string& title, bool checked) {
				if (index < 0)
					return;

				auto lock = Host::GetSettingsLock();
				SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
				bsi->SetStringValue("Filenames", "BIOS", values[index].c_str());
				SetSettingsChanged(bsi);
				ApplyLayoutSettings(bsi);
				CloseChoiceDialog();
			});
	}

	MenuHeading(FSUI_CSTR("Fast Boot Options"));
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_FORWARD_FAST, "Fast Boot"), FSUI_CSTR("Skips the intro screen, and bypasses region checks."),
		"EmuCore", "EnableFastBoot", true);

	EndMenuButtons();
}

void FullscreenUI::DrawEmulationSettingsPage()
{
	static constexpr int DEFAULT_FRAME_LATENCY = 2;

	static constexpr const char* speed_entries[] = {
		FSUI_NSTR("2% [1 FPS (NTSC) / 1 FPS (PAL)]"),
		FSUI_NSTR("10% [6 FPS (NTSC) / 5 FPS (PAL)]"),
		FSUI_NSTR("25% [15 FPS (NTSC) / 12 FPS (PAL)]"),
		FSUI_NSTR("50% [30 FPS (NTSC) / 25 FPS (PAL)]"),
		FSUI_NSTR("75% [45 FPS (NTSC) / 37 FPS (PAL)]"),
		FSUI_NSTR("90% [54 FPS (NTSC) / 45 FPS (PAL)]"),
		FSUI_NSTR("100% [60 FPS (NTSC) / 50 FPS (PAL)]"),
		FSUI_NSTR("110% [66 FPS (NTSC) / 55 FPS (PAL)]"),
		FSUI_NSTR("120% [72 FPS (NTSC) / 60 FPS (PAL)]"),
		FSUI_NSTR("150% [90 FPS (NTSC) / 75 FPS (PAL)]"),
		FSUI_NSTR("175% [105 FPS (NTSC) / 87 FPS (PAL)]"),
		FSUI_NSTR("200% [120 FPS (NTSC) / 100 FPS (PAL)]"),
		FSUI_NSTR("300% [180 FPS (NTSC) / 150 FPS (PAL)]"),
		FSUI_NSTR("400% [240 FPS (NTSC) / 200 FPS (PAL)]"),
		FSUI_NSTR("500% [300 FPS (NTSC) / 250 FPS (PAL)]"),
		FSUI_NSTR("1000% [600 FPS (NTSC) / 500 FPS (PAL)]"),
	};
	static constexpr const float speed_values[] = {
		0.02f,
		0.10f,
		0.25f,
		0.50f,
		0.75f,
		0.90f,
		1.00f,
		1.10f,
		1.20f,
		1.50f,
		1.75f,
		2.00f,
		3.00f,
		4.00f,
		5.00f,
		10.00f,
	};
	static constexpr const char* ee_cycle_rate_settings[] = {
		FSUI_NSTR("50% Speed"),
		FSUI_NSTR("60% Speed"),
		FSUI_NSTR("75% Speed"),
		FSUI_NSTR("100% Speed (Default)"),
		FSUI_NSTR("130% Speed"),
		FSUI_NSTR("180% Speed"),
		FSUI_NSTR("300% Speed"),
	};
	static constexpr const char* ee_cycle_skip_settings[] = {
		FSUI_NSTR("Normal (Default)"),
		FSUI_NSTR("Mild Underclock"),
		FSUI_NSTR("Moderate Underclock"),
		FSUI_NSTR("Maximum Underclock"),
	};
	static constexpr const char* queue_entries[] = {
		FSUI_NSTR("0 Frames (Hard Sync)"),
		FSUI_NSTR("1 Frame"),
		FSUI_NSTR("2 Frames"),
		FSUI_NSTR("3 Frames"),
	};

	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading(FSUI_CSTR("Speed Control"));

	DrawFloatListSetting(bsi, FSUI_ICONSTR(ICON_FA_PLAY, "Normal Speed"), FSUI_CSTR("Sets the speed when running without fast forwarding."), "Framerate",
		"NominalScalar", 1.00f, speed_entries, speed_values, std::size(speed_entries), true);
	DrawFloatListSetting(bsi, FSUI_ICONSTR(ICON_FA_FORWARD_FAST, "Fast Forward Speed"), FSUI_CSTR("Sets the speed when using the fast forward hotkey."), "Framerate",
		"TurboScalar", 2.00f, speed_entries, speed_values, std::size(speed_entries), true);
	DrawFloatListSetting(bsi, FSUI_ICONSTR(ICON_PF_SLOW_MOTION, "Slow Motion Speed"), FSUI_CSTR("Sets the speed when using the slow motion hotkey."), "Framerate",
		"SlomoScalar", 0.50f, speed_entries, speed_values, std::size(speed_entries), true);

	MenuHeading(FSUI_CSTR("System Settings"));

	DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_GAUGE_HIGH, "EE Cycle Rate"), FSUI_CSTR("Underclocks or overclocks the emulated Emotion Engine CPU."),
		"EmuCore/Speedhacks", "EECycleRate", 0, ee_cycle_rate_settings, std::size(ee_cycle_rate_settings), true, -3);
	DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROW_TREND_DOWN, "EE Cycle Skipping"),
		FSUI_CSTR("Makes the emulated Emotion Engine skip cycles. Helps a small subset of games like SOTC. Most of the time it's harmful to performance."), "EmuCore/Speedhacks", "EECycleSkip", 0,
		ee_cycle_skip_settings, std::size(ee_cycle_skip_settings), true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_USERS, "Enable MTVU (Multi-Threaded VU1)"),
		FSUI_CSTR("Generally a speedup on CPUs with 4 or more cores. Safe for most games, but a few are incompatible and may hang."), "EmuCore/Speedhacks", "vuThread", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_LOCATION_PIN_LOCK, "Thread Pinning"),
		FSUI_CSTR("Pins emulation threads to CPU cores to potentially improve performance/frame time variance."), "EmuCore",
		"EnableThreadPinning", false);
	DrawToggleSetting(
		bsi, FSUI_ICONSTR(ICON_FA_FACE_ROLLING_EYES, "Enable Cheats"), FSUI_CSTR("Enables loading cheats from pnach files."), "EmuCore", "EnableCheats", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_HARD_DRIVE, "Enable Host Filesystem"),
		FSUI_CSTR("Enables access to files from the host: namespace in the virtual machine."), "EmuCore", "HostFs", false);

	if (IsEditingGameSettings(bsi))
	{
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_COMPACT_DISC, "Enable Fast CDVD"), FSUI_CSTR("Fast disc access, less loading times. Not recommended."),
			"EmuCore/Speedhacks", "fastCDVD", false);
	}

	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_COMPACT_DISC, "Enable CDVD Precaching"), FSUI_CSTR("Loads the disc image into RAM before starting the virtual machine."),
		"EmuCore", "CdvdPrecache", false);

	MenuHeading(FSUI_CSTR("Frame Pacing/Latency Control"));

	bool optimal_frame_pacing = (bsi->GetIntValue("EmuCore/GS", "VsyncQueueSize", DEFAULT_FRAME_LATENCY) == 0);

	DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_CLOCK_ROTATE_LEFT, "Maximum Frame Latency"), FSUI_CSTR("Sets the number of frames which can be queued."), "EmuCore/GS",
		"VsyncQueueSize", DEFAULT_FRAME_LATENCY, queue_entries, std::size(queue_entries), true, 0, !optimal_frame_pacing);

	if (ToggleButton(FSUI_ICONSTR(ICON_PF_HEARTBEAT_ALT, "Optimal Frame Pacing"),
			FSUI_CSTR("Synchronize EE and GS threads after each frame. Lowest input latency, but increases system requirements."),
			&optimal_frame_pacing))
	{
		bsi->SetIntValue("EmuCore/GS", "VsyncQueueSize", optimal_frame_pacing ? 0 : DEFAULT_FRAME_LATENCY);
		SetSettingsChanged(bsi);
	}

	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROWS_SPIN, "Vertical Sync (VSync)"), FSUI_CSTR("Synchronizes frame presentation with host refresh."),
		"EmuCore/GS", "VsyncEnable", false);

	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_PF_MONITOR_CODE, "Sync to Host Refresh Rate"),
		FSUI_CSTR("Speeds up emulation so that the guest refresh rate matches the host."), "EmuCore/GS", "SyncToHostRefreshRate", false);

	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_PF_HEARTBEAT_CIRCLE, "Use Host VSync Timing"),
		FSUI_CSTR("Disables PCSX2's internal frame timing, and uses host vsync instead."), "EmuCore/GS", "UseVSyncForTiming", false,
		GetEffectiveBoolSetting(bsi, "EmuCore/GS", "VsyncEnable", false) && GetEffectiveBoolSetting(bsi, "EmuCore/GS", "SyncToHostRefreshRate", false));

	EndMenuButtons();
}

void FullscreenUI::DrawClampingModeSetting(SettingsInterface* bsi, const char* title, const char* summary, int vunum)
{
	// This is so messy... maybe we should just make the mode an int in the settings too...
	const bool base = IsEditingGameSettings(bsi) ? 1 : 0;
	std::optional<bool> default_false = IsEditingGameSettings(bsi) ? std::nullopt : std::optional<bool>(false);
	std::optional<bool> default_true = IsEditingGameSettings(bsi) ? std::nullopt : std::optional<bool>(true);

	std::optional<bool> third = bsi->GetOptionalBoolValue(
		"EmuCore/CPU/Recompiler", (vunum >= 0 ? ((vunum == 0) ? "vu0SignOverflow" : "vu1SignOverflow") : "fpuFullMode"), default_false);
	std::optional<bool> second = bsi->GetOptionalBoolValue("EmuCore/CPU/Recompiler",
		(vunum >= 0 ? ((vunum == 0) ? "vu0ExtraOverflow" : "vu1ExtraOverflow") : "fpuExtraOverflow"), default_false);
	std::optional<bool> first = bsi->GetOptionalBoolValue(
		"EmuCore/CPU/Recompiler", (vunum >= 0 ? ((vunum == 0) ? "vu0Overflow" : "vu1Overflow") : "fpuOverflow"), default_true);

	int index;
	if (third.has_value() && third.value())
		index = base + 3;
	else if (second.has_value() && second.value())
		index = base + 2;
	else if (first.has_value() && first.value())
		index = base + 1;
	else if (first.has_value())
		index = base + 0; // none
	else
		index = 0; // no per game override

	static constexpr const char* ee_clamping_mode_settings[] = {
		FSUI_NSTR("Use Global Setting"),
		FSUI_NSTR("None"),
		FSUI_NSTR("Normal (Default)"),
		FSUI_NSTR("Extra + Preserve Sign"),
		FSUI_NSTR("Full"),
	};
	static constexpr const char* vu_clamping_mode_settings[] = {
		FSUI_NSTR("Use Global Setting"),
		FSUI_NSTR("None"),
		FSUI_NSTR("Normal (Default)"),
		FSUI_NSTR("Extra"),
		FSUI_NSTR("Extra + Preserve Sign"),
	};
	const char* const* options = (vunum >= 0) ? vu_clamping_mode_settings : ee_clamping_mode_settings;
	const int setting_offset = IsEditingGameSettings(bsi) ? 0 : 1;

	if (MenuButtonWithValue(title, summary, Host::TranslateToCString(TR_CONTEXT, options[index + setting_offset])))
	{
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(std::size(ee_clamping_mode_settings));
		for (int i = setting_offset; i < static_cast<int>(std::size(ee_clamping_mode_settings)); i++)
			cd_options.emplace_back(Host::TranslateToString(TR_CONTEXT, options[i]), (i == (index + setting_offset)));
		OpenChoiceDialog(title, false, std::move(cd_options),
			[game_settings = IsEditingGameSettings(bsi), vunum](s32 index, const std::string& title, bool checked) {
				if (index >= 0)
				{
					auto lock = Host::GetSettingsLock();

					std::optional<bool> first, second, third;

					if (!game_settings || index > 0)
					{
						const bool base = game_settings ? 1 : 0;
						third = (index >= (base + 3));
						second = (index >= (base + 2));
						first = (index >= (base + 1));
					}

					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					bsi->SetOptionalBoolValue("EmuCore/CPU/Recompiler",
						(vunum >= 0 ? ((vunum == 0) ? "vu0SignOverflow" : "vu1SignOverflow") : "fpuFullMode"), third);
					bsi->SetOptionalBoolValue("EmuCore/CPU/Recompiler",
						(vunum >= 0 ? ((vunum == 0) ? "vu0ExtraOverflow" : "vu1ExtraOverflow") : "fpuExtraOverflow"), second);
					bsi->SetOptionalBoolValue(
						"EmuCore/CPU/Recompiler", (vunum >= 0 ? ((vunum == 0) ? "vu0Overflow" : "vu1Overflow") : "fpuOverflow"), first);
					SetSettingsChanged(bsi);
				}

				CloseChoiceDialog();
			});
	}
}

void FullscreenUI::DrawGraphicsSettingsPage(SettingsInterface* bsi, bool show_advanced_settings)
{
	static constexpr const char* s_renderer_names[] = {
		FSUI_NSTR("Automatic (Default)"),
#ifdef _WIN32
		FSUI_NSTR("Direct3D 11 (Legacy)"),
		FSUI_NSTR("Direct3D 12"),
#endif
#ifdef ENABLE_OPENGL
		FSUI_NSTR("OpenGL"),
#endif
#ifdef ENABLE_VULKAN
		FSUI_NSTR("Vulkan"),
#endif
#ifdef __APPLE__
		FSUI_NSTR("Metal"),
#endif
		FSUI_NSTR("Software Renderer"),
		FSUI_NSTR("Null"),
	};
	static constexpr const char* s_renderer_values[] = {
		"-1", //GSRendererType::Auto,
#ifdef _WIN32
		"3", //GSRendererType::DX11,
		"15", //GSRendererType::DX12,
#endif
#ifdef ENABLE_OPENGL
		"12", //GSRendererType::OGL,
#endif
#ifdef ENABLE_VULKAN
		"14", //GSRendererType::VK,
#endif
#ifdef __APPLE__
		"17", //GSRendererType::Metal,
#endif
		"13", //GSRendererType::SW,
		"11", //GSRendererType::Null
	};
	static constexpr const char* s_bilinear_present_options[] = {
		FSUI_NSTR("Off"),
		FSUI_NSTR("Bilinear (Smooth)"),
		FSUI_NSTR("Bilinear (Sharp)"),
	};
	static constexpr const char* s_deinterlacing_options[] = {
		FSUI_NSTR("Automatic (Default)"),
		FSUI_NSTR("No Deinterlacing"),
		FSUI_NSTR("Weave (Top Field First, Sawtooth)"),
		FSUI_NSTR("Weave (Bottom Field First, Sawtooth)"),
		FSUI_NSTR("Bob (Top Field First)"),
		FSUI_NSTR("Bob (Bottom Field First)"),
		FSUI_NSTR("Blend (Top Field First, Half FPS)"),
		FSUI_NSTR("Blend (Bottom Field First, Half FPS)"),
		FSUI_NSTR("Adaptive (Top Field First)"),
		FSUI_NSTR("Adaptive (Bottom Field First)"),
	};
	static const char* s_resolution_options[] = {
		FSUI_NSTR("Native (PS2)"),
		FSUI_NSTR("2x Native (~720px/HD)"),
		FSUI_NSTR("3x Native (~1080px/FHD)"),
		FSUI_NSTR("4x Native (~1440px/QHD)"),
		FSUI_NSTR("5x Native (~1800px/QHD+)"),
		FSUI_NSTR("6x Native (~2160px/4K UHD)"),
		FSUI_NSTR("7x Native (~2520px)"),
		FSUI_NSTR("8x Native (~2880px/5K UHD)"),
		FSUI_NSTR("9x Native (~3240px)"),
		FSUI_NSTR("10x Native (~3600px/6K UHD)"),
		FSUI_NSTR("11x Native (~3960px)"),
		FSUI_NSTR("12x Native (~4320px/8K UHD)"),
	};
	static const char* s_resolution_values[] = {
		"1",
		"2",
		"3",
		"4",
		"5",
		"6",
		"7",
		"8",
		"9",
		"10",
		"11",
		"12",
	};
	static constexpr const char* s_bilinear_options[] = {
		FSUI_NSTR("Nearest"),
		FSUI_NSTR("Bilinear (Forced)"),
		FSUI_NSTR("Bilinear (PS2)"),
		FSUI_NSTR("Bilinear (Forced excluding sprite)"),
	};
	static constexpr const char* s_trilinear_options[] = {
		FSUI_NSTR("Automatic (Default)"),
		FSUI_NSTR("Off (None)"),
		FSUI_NSTR("Trilinear (PS2)"),
		FSUI_NSTR("Trilinear (Forced)"),
	};
	static constexpr const char* s_dithering_options[] = {
		FSUI_NSTR("Off"),
		FSUI_NSTR("Scaled"),
		FSUI_NSTR("Unscaled (Default)"),
		FSUI_NSTR("Force 32bit"),
	};
	static constexpr const char* s_blending_options[] = {
		FSUI_NSTR("Minimum"),
		FSUI_NSTR("Basic (Recommended)"),
		FSUI_NSTR("Medium"),
		FSUI_NSTR("High"),
		FSUI_NSTR("Full (Slow)"),
		FSUI_NSTR("Maximum (Very Slow)"),
	};
	static constexpr const char* s_anisotropic_filtering_entries[] = {
		FSUI_NSTR("Off (Default)"),
		FSUI_NSTR("2x"),
		FSUI_NSTR("4x"),
		FSUI_NSTR("8x"),
		FSUI_NSTR("16x"),
	};
	static constexpr const char* s_anisotropic_filtering_values[] = {
		"0",
		"2",
		"4",
		"8",
		"16",
	};
	static constexpr const char* s_preloading_options[] = {
		FSUI_NSTR("None"),
		FSUI_NSTR("Partial"),
		FSUI_NSTR("Full (Hash Cache)"),
	};
	static constexpr const char* s_generic_options[] = {
		FSUI_NSTR("Automatic (Default)"),
		FSUI_NSTR("Force Disabled"),
		FSUI_NSTR("Force Enabled"),
	};
	static constexpr const char* s_hw_download[] = {
		FSUI_NSTR("Accurate (Recommended)"),
		FSUI_NSTR("Disable Readbacks (Synchronize GS Thread)"),
		FSUI_NSTR("Unsynchronized (Non-Deterministic)"),
		FSUI_NSTR("Disabled (Ignore Transfers)"),
	};
	static constexpr const char* s_screenshot_sizes[] = {
		FSUI_NSTR("Display Resolution (Aspect Corrected)"),
		FSUI_NSTR("Internal Resolution (Aspect Corrected)"),
		FSUI_NSTR("Internal Resolution (No Aspect Correction)"),
	};
	static constexpr const char* s_screenshot_formats[] = {
		FSUI_NSTR("PNG"),
		FSUI_NSTR("JPEG"),
		FSUI_NSTR("WebP"),
	};

	const GSRendererType renderer =
		static_cast<GSRendererType>(GetEffectiveIntSetting(bsi, "EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::Auto)));
	const bool is_hardware = (renderer == GSRendererType::Auto || renderer == GSRendererType::DX11 || renderer == GSRendererType::DX12 ||
							  renderer == GSRendererType::OGL || renderer == GSRendererType::VK || renderer == GSRendererType::Metal);
	//const bool is_software = (renderer == GSRendererType::SW);

#ifndef PCSX2_DEVBUILD
	const bool hw_fixes_visible = is_hardware && IsEditingGameSettings(bsi);
#else
	const bool hw_fixes_visible = is_hardware;
#endif

	BeginMenuButtons();

	MenuHeading(FSUI_CSTR("Graphics API"));
	DrawStringListSetting(bsi, FSUI_ICONSTR(ICON_FA_PAINTBRUSH, "Graphics API"), FSUI_CSTR("Selects the API used to render the emulated GS."), "EmuCore/GS",
		"Renderer", "-1", s_renderer_names, s_renderer_values, std::size(s_renderer_names), true);

	MenuHeading(FSUI_CSTR("Display"));
	DrawStringListSetting(bsi, FSUI_ICONSTR(ICON_FA_COMPRESS, "Aspect Ratio"), FSUI_CSTR("Selects the aspect ratio to display the game content at."),
		"EmuCore/GS", "AspectRatio", "Auto 4:3/3:2", Pcsx2Config::GSOptions::AspectRatioNames, Pcsx2Config::GSOptions::AspectRatioNames, 0,
		false);
	DrawStringListSetting(bsi, FSUI_ICONSTR(ICON_FA_VIDEO, "FMV Aspect Ratio Override"),
		FSUI_CSTR("Selects the aspect ratio for display when a FMV is detected as playing."), "EmuCore/GS", "FMVAspectRatioSwitch",
		"Auto 4:3/3:2", Pcsx2Config::GSOptions::FMVAspectRatioSwitchNames, Pcsx2Config::GSOptions::FMVAspectRatioSwitchNames, 0, false);
	DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_TV, "Deinterlacing"),
		FSUI_CSTR("Selects the algorithm used to convert the PS2's interlaced output to progressive for display."), "EmuCore/GS",
		"deinterlace_mode", static_cast<int>(GSInterlaceMode::Automatic), s_deinterlacing_options, std::size(s_deinterlacing_options),
		true);
	DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, "Screenshot Size"), FSUI_CSTR("Determines the resolution at which screenshots will be saved."),
		"EmuCore/GS", "ScreenshotSize", static_cast<int>(GSScreenshotSize::WindowResolution), s_screenshot_sizes,
		std::size(s_screenshot_sizes), true);
	DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_PHOTO_FILM, "Screenshot Format"), FSUI_CSTR("Selects the format which will be used to save screenshots."),
		"EmuCore/GS", "ScreenshotFormat", static_cast<int>(GSScreenshotFormat::PNG), s_screenshot_formats, std::size(s_screenshot_formats),
		true);
	DrawIntRangeSetting(bsi, FSUI_ICONSTR(ICON_FA_GAUGE, "Screenshot Quality"), FSUI_CSTR("Selects the quality at which screenshots will be compressed."),
		"EmuCore/GS", "ScreenshotQuality", 90, 1, 100, FSUI_CSTR("%d%%"));
	DrawIntRangeSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROW_RIGHT_ARROW_LEFT, "Vertical Stretch"), FSUI_CSTR("Increases or decreases the virtual picture size vertically."),
		"EmuCore/GS", "StretchY", 100, 10, 300, FSUI_CSTR("%d%%"));
	DrawIntRectSetting(bsi, FSUI_ICONSTR(ICON_FA_CROP, "Crop"), FSUI_CSTR("Crops the image, while respecting aspect ratio."), "EmuCore/GS", "CropLeft", 0,
		"CropTop", 0, "CropRight", 0, "CropBottom", 0, 0, 720, 1, FSUI_CSTR("%dpx"));

	if (!IsEditingGameSettings(bsi))
	{
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_TV, "Enable Widescreen Patches"), FSUI_CSTR("Enables loading widescreen patches from pnach files."),
			"EmuCore", "EnableWideScreenPatches", false);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_TV, "Enable No-Interlacing Patches"),
			FSUI_CSTR("Enables loading no-interlacing patches from pnach files."), "EmuCore", "EnableNoInterlacingPatches", false);
	}

	DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_TABLE_CELLS, "Bilinear Upscaling"), FSUI_CSTR("Smooths out the image when upscaling the console to the screen."),
		"EmuCore/GS", "linear_present_mode", static_cast<int>(GSPostBilinearMode::BilinearSharp), s_bilinear_present_options,
		std::size(s_bilinear_present_options), true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_SQUARE_ARROW_UP_RIGHT, "Integer Upscaling"),
		FSUI_CSTR("Adds padding to the display area to ensure that the ratio between pixels on the host to pixels in the console is an "
				  "integer number. May result in a sharper image in some 2D games."),
		"EmuCore/GS", "IntegerScaling", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_SQUARE_UP_RIGHT, "Screen Offsets"), FSUI_CSTR("Enables PCRTC Offsets which position the screen as the game requests."),
		"EmuCore/GS", "pcrtc_offsets", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_MAXIMIZE, "Show Overscan"),
		FSUI_CSTR("Enables the option to show the overscan area on games which draw more than the safe area of the screen."), "EmuCore/GS",
		"pcrtc_overscan", false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_GLASSES, "Anti-Blur"),
		FSUI_CSTR("Enables internal Anti-Blur hacks. Less accurate to PS2 rendering but will make a lot of games look less blurry."),
		"EmuCore/GS", "pcrtc_antiblur", true);

	MenuHeading(FSUI_CSTR("Rendering"));
	if (is_hardware)
	{
		DrawStringListSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE, "Internal Resolution"),
			FSUI_CSTR("Multiplies the render resolution by the specified factor (upscaling)."), "EmuCore/GS", "upscale_multiplier",
			"1.000000", s_resolution_options, s_resolution_values, std::size(s_resolution_options), true);
		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_TABLE_CELLS_LARGE, "Bilinear Filtering"),
			FSUI_CSTR("Selects where bilinear filtering is utilized when rendering textures."), "EmuCore/GS", "filter",
			static_cast<int>(BiFiltering::PS2), s_bilinear_options, std::size(s_bilinear_options), true);
		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_TABLE_CELLS_LARGE, "Trilinear Filtering"),
			FSUI_CSTR("Selects where trilinear filtering is utilized when rendering textures."), "EmuCore/GS", "TriFilter",
			static_cast<int>(TriFiltering::Automatic), s_trilinear_options, std::size(s_trilinear_options), true, -1);
		DrawStringListSetting(bsi, FSUI_ICONSTR(ICON_FA_EYE_LOW_VISION, "Anisotropic Filtering"),
			FSUI_CSTR("Selects where anisotropic filtering is utilized when rendering textures."), "EmuCore/GS", "MaxAnisotropy", "0",
			s_anisotropic_filtering_entries, s_anisotropic_filtering_values, std::size(s_anisotropic_filtering_entries), true);
		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_DROPLET_SLASH, "Dithering"), FSUI_CSTR("Selects the type of dithering applies when the game requests it."),
			"EmuCore/GS", "dithering_ps2", 2, s_dithering_options, std::size(s_dithering_options), true);
		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_SPLOTCH, "Blending Accuracy"),
			FSUI_CSTR("Determines the level of accuracy when emulating blend modes not supported by the host graphics API."), "EmuCore/GS",
			"accurate_blending_unit", static_cast<int>(AccBlendLevel::Basic), s_blending_options, std::size(s_blending_options), true);
		DrawToggleSetting(
			bsi, FSUI_ICONSTR(ICON_FA_BULLSEYE, "Mipmapping"), FSUI_CSTR("Enables emulation of the GS's texture mipmapping."), "EmuCore/GS", "hw_mipmap", true);
	}
	else
	{
		DrawIntRangeSetting(bsi, FSUI_ICONSTR(ICON_FA_USERS, "Software Rendering Threads"),
			FSUI_CSTR("Number of threads to use in addition to the main GS thread for rasterization."), "EmuCore/GS", "extrathreads", 2, 0,
			10);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_TOILET, "Auto Flush (Software)"),
			FSUI_CSTR("Force a primitive flush when a framebuffer is also an input texture."), "EmuCore/GS", "autoflush_sw", true);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_EYE_DROPPER, "Edge AA (AA1)"), FSUI_CSTR("Enables emulation of the GS's edge anti-aliasing (AA1)."),
			"EmuCore/GS", "aa1", true);
		DrawToggleSetting(
			bsi, FSUI_ICONSTR(ICON_FA_BULLSEYE, "Mipmapping"), FSUI_CSTR("Enables emulation of the GS's texture mipmapping."), "EmuCore/GS", "mipmap", true);
	}

	if (hw_fixes_visible)
	{
		MenuHeading(FSUI_CSTR("Hardware Fixes"));
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_TOOLBOX, "Manual Hardware Fixes"),
			FSUI_CSTR("Disables automatic hardware fixes, allowing you to set fixes manually."), "EmuCore/GS", "UserHacks", false);

		const bool manual_hw_fixes = GetEffectiveBoolSetting(bsi, "EmuCore/GS", "UserHacks", false);
		if (manual_hw_fixes)
		{
			static constexpr const char* s_cpu_sprite_render_bw_options[] = {
				FSUI_NSTR("0 (Disabled)"),
				FSUI_NSTR("1 (64 Max Width)"),
				FSUI_NSTR("2 (128 Max Width)"),
				FSUI_NSTR("3 (192 Max Width)"),
				FSUI_NSTR("4 (256 Max Width)"),
				FSUI_NSTR("5 (320 Max Width)"),
				FSUI_NSTR("6 (384 Max Width)"),
				FSUI_NSTR("7 (448 Max Width)"),
				FSUI_NSTR("8 (512 Max Width)"),
				FSUI_NSTR("9 (576 Max Width)"),
				FSUI_NSTR("10 (640 Max Width)"),
			};
			static constexpr const char* s_cpu_sprite_render_level_options[] = {
				FSUI_NSTR("Sprites Only"),
				FSUI_NSTR("Sprites/Triangles"),
				FSUI_NSTR("Blended Sprites/Triangles"),
			};
			static constexpr const char* s_cpu_clut_render_options[] = {
				FSUI_NSTR("0 (Disabled)"),
				FSUI_NSTR("1 (Normal)"),
				FSUI_NSTR("2 (Aggressive)"),
			};
			static constexpr const char* s_texture_inside_rt_options[] = {
				FSUI_NSTR("Disabled"),
				FSUI_NSTR("Inside Target"),
				FSUI_NSTR("Merge Targets"),
			};
			static constexpr const char* s_half_pixel_offset_options[] = {
				FSUI_NSTR("Off (Default)"),
				FSUI_NSTR("Normal (Vertex)"),
				FSUI_NSTR("Special (Texture)"),
				FSUI_NSTR("Special (Texture - Aggressive)"),
				FSUI_NSTR("Align to Native"),
				FSUI_NSTR("Align to Native - with Texture Offset"),
			};
			static constexpr const char* s_native_scaling_options[] = {
				FSUI_NSTR("Off (Default)"),
				FSUI_NSTR("Normal"),
				FSUI_NSTR("Aggressive"),
				FSUI_NSTR("Normal (Maintain Upscale)"),
				FSUI_NSTR("Aggressive (Maintain Upscale)"),
			};
			static constexpr const char* s_round_sprite_options[] = {
				FSUI_NSTR("Off (Default)"),
				FSUI_NSTR("Half"),
				FSUI_NSTR("Full"),
			};
			static constexpr const char* s_bilinear_dirty_options[] = {
				FSUI_NSTR("Automatic (Default)"),
				FSUI_NSTR("Force Bilinear"),
				FSUI_NSTR("Force Nearest"),
			};
			static constexpr const char* s_auto_flush_options[] = {
				FSUI_NSTR("Disabled (Default)"),
				FSUI_NSTR("Enabled (Sprites Only)"),
				FSUI_NSTR("Enabled (All Primitives)"),
			};

			static constexpr const char* s_gpu_clut_options[] = {
				FSUI_NSTR("Disabled (Default)"),
				FSUI_NSTR("Enabled (Exact Match)"),
				FSUI_NSTR("Enabled (Check Inside Target)"),
			};

			DrawIntListSetting(bsi, FSUI_CSTR("CPU Sprite Render Size"),
				FSUI_CSTR("Uses software renderer to draw texture decompression-like sprites."), "EmuCore/GS",
				"UserHacks_CPUSpriteRenderBW", 0, s_cpu_sprite_render_bw_options, std::size(s_cpu_sprite_render_bw_options), true);
			DrawIntListSetting(bsi, FSUI_CSTR("CPU Sprite Render Level"), FSUI_CSTR("Determines filter level for CPU sprite render."),
				"EmuCore/GS", "UserHacks_CPUSpriteRenderLevel", 0, s_cpu_sprite_render_level_options,
				std::size(s_cpu_sprite_render_level_options), true);
			DrawIntListSetting(bsi, FSUI_CSTR("Software CLUT Render"),
				FSUI_CSTR("Uses software renderer to draw texture CLUT points/sprites."), "EmuCore/GS", "UserHacks_CPUCLUTRender", 0,
				s_cpu_clut_render_options, std::size(s_cpu_clut_render_options), true);
			DrawIntListSetting(bsi, FSUI_CSTR("GPU Target CLUT"),
				FSUI_CSTR("Try to detect when a game is drawing its own color palette and then renders it on the GPU with special handling."), "EmuCore/GS", "UserHacks_GPUTargetCLUTMode",
				0, s_gpu_clut_options, std::size(s_gpu_clut_options), true, 0, manual_hw_fixes);
			DrawIntSpinBoxSetting(bsi, FSUI_CSTR("Skip Draw Start"), FSUI_CSTR("Object range to skip drawing."), "EmuCore/GS",
				"UserHacks_SkipDraw_Start", 0, 0, 5000, 1);
			DrawIntSpinBoxSetting(bsi, FSUI_CSTR("Skip Draw End"), FSUI_CSTR("Object range to skip drawing."), "EmuCore/GS",
				"UserHacks_SkipDraw_End", 0, 0, 5000, 1);
			DrawIntListSetting(bsi, FSUI_CSTR("Auto Flush (Hardware)"),
				FSUI_CSTR("Force a primitive flush when a framebuffer is also an input texture."), "EmuCore/GS", "UserHacks_AutoFlushLevel",
				0, s_auto_flush_options, std::size(s_auto_flush_options), true, 0, manual_hw_fixes);
			DrawToggleSetting(bsi, FSUI_CSTR("CPU Framebuffer Conversion"),
				FSUI_CSTR("Convert 4-bit and 8-bit framebuffer on the CPU instead of the GPU."), "EmuCore/GS",
				"UserHacks_CPU_FB_Conversion", false, manual_hw_fixes);
			DrawToggleSetting(bsi, FSUI_CSTR("Disable Depth Conversion"),
				FSUI_CSTR("Disable the support of depth buffers in the texture cache."), "EmuCore/GS", "UserHacks_DisableDepthSupport",
				false, manual_hw_fixes);
			DrawToggleSetting(bsi, FSUI_CSTR("Disable Safe Features"), FSUI_CSTR("This option disables multiple safe features."),
				"EmuCore/GS", "UserHacks_Disable_Safe_Features", false, manual_hw_fixes);
			DrawToggleSetting(bsi, FSUI_CSTR("Disable Render Fixes"), FSUI_CSTR("This option disables game-specific render fixes."),
				"EmuCore/GS", "UserHacks_DisableRenderFixes", false, manual_hw_fixes);
			DrawToggleSetting(bsi, FSUI_CSTR("Preload Frame Data"),
				FSUI_CSTR("Uploads GS data when rendering a new frame to reproduce some effects accurately."), "EmuCore/GS",
				"preload_frame_with_gs_data", false, manual_hw_fixes);
			DrawToggleSetting(bsi, FSUI_CSTR("Disable Partial Invalidation"),
				FSUI_CSTR("Removes texture cache entries when there is any intersection, rather than only the intersected areas."),
				"EmuCore/GS", "UserHacks_DisablePartialInvalidation", false, manual_hw_fixes);
			DrawIntListSetting(bsi, FSUI_CSTR("Texture Inside RT"),
				FSUI_CSTR("Allows the texture cache to reuse as an input texture the inner portion of a previous framebuffer."),
				"EmuCore/GS", "UserHacks_TextureInsideRt", 0, s_texture_inside_rt_options, std::size(s_texture_inside_rt_options), true, 0,
				manual_hw_fixes);
			DrawToggleSetting(bsi, FSUI_CSTR("Read Targets When Closing"),
				FSUI_CSTR("Flushes all targets in the texture cache back to local memory when shutting down."), "EmuCore/GS",
				"UserHacks_ReadTCOnClose", false, manual_hw_fixes);
			DrawToggleSetting(bsi, FSUI_CSTR("Estimate Texture Region"),
				FSUI_CSTR("Attempts to reduce the texture size when games do not set it themselves (e.g. Snowblind games)."), "EmuCore/GS",
				"UserHacks_EstimateTextureRegion", false, manual_hw_fixes);
			DrawToggleSetting(bsi, FSUI_CSTR("GPU Palette Conversion"),
				FSUI_CSTR("When enabled GPU converts colormap-textures, otherwise the CPU will. It is a trade-off between GPU and CPU."),
				"EmuCore/GS", "paltex", false, manual_hw_fixes);

			MenuHeading(FSUI_CSTR("Upscaling Fixes"));
			DrawIntListSetting(bsi, FSUI_CSTR("Half Pixel Offset"), FSUI_CSTR("Adjusts vertices relative to upscaling."), "EmuCore/GS",
				"UserHacks_HalfPixelOffset", 0, s_half_pixel_offset_options, std::size(s_half_pixel_offset_options), true);
			DrawIntListSetting(bsi, FSUI_CSTR("Native Scaling"), FSUI_CSTR("Attempt to do rescaling at native resolution."), "EmuCore/GS",
				"UserHacks_native_scaling", 0, s_native_scaling_options, std::size(s_native_scaling_options), true);
			DrawIntListSetting(bsi, FSUI_CSTR("Round Sprite"), FSUI_CSTR("Adjusts sprite coordinates."), "EmuCore/GS",
				"UserHacks_round_sprite_offset", 0, s_round_sprite_options, std::size(s_round_sprite_options), true);
			DrawIntListSetting(bsi, FSUI_CSTR("Bilinear Dirty Upscale"),
				FSUI_CSTR("Can smooth out textures due to be bilinear filtered when upscaling. E.g. Brave sun glare."), "EmuCore/GS",
				"UserHacks_BilinearHack", static_cast<int>(GSBilinearDirtyMode::Automatic), s_bilinear_dirty_options,
				std::size(s_bilinear_dirty_options), true);
			DrawIntSpinBoxSetting(bsi, FSUI_CSTR("Texture Offset X"), FSUI_CSTR("Adjusts target texture offsets."), "EmuCore/GS",
				"UserHacks_TCOffsetX", 0, -4096, 4096, 1);
			DrawIntSpinBoxSetting(bsi, FSUI_CSTR("Texture Offset Y"), FSUI_CSTR("Adjusts target texture offsets."), "EmuCore/GS",
				"UserHacks_TCOffsetY", 0, -4096, 4096, 1);
			DrawToggleSetting(bsi, FSUI_CSTR("Align Sprite"), FSUI_CSTR("Fixes issues with upscaling (vertical lines) in some games."),
				"EmuCore/GS", "UserHacks_align_sprite_X", false, manual_hw_fixes);
			DrawToggleSetting(bsi, FSUI_CSTR("Merge Sprite"),
				FSUI_CSTR("Replaces multiple post-processing sprites with a larger single sprite."), "EmuCore/GS",
				"UserHacks_merge_pp_sprite", false, manual_hw_fixes);
			DrawToggleSetting(bsi, FSUI_CSTR("Force Even Sprite Position"),
				FSUI_CSTR("Lowers the GS precision to avoid gaps between pixels when upscaling. Fixes the text on Wild Arms games."),
				"EmuCore/GS", "UserHacks_ForceEvenSpritePosition", false, manual_hw_fixes);
			DrawToggleSetting(bsi, FSUI_CSTR("Unscaled Palette Texture Draws"),
				FSUI_CSTR("Can fix some broken effects which rely on pixel perfect precision."), "EmuCore/GS",
				"UserHacks_NativePaletteDraw", false, manual_hw_fixes);
		}
	}

	if (is_hardware)
	{
		const bool dumping_active = GetEffectiveBoolSetting(bsi, "EmuCore/GS", "DumpReplaceableTextures", false);
		const bool replacement_active = GetEffectiveBoolSetting(bsi, "EmuCore/GS", "LoadTextureReplacements", false);

		MenuHeading(FSUI_CSTR("Texture Replacement"));
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_IMAGES, "Load Textures"), FSUI_CSTR("Loads replacement textures where available and user-provided."),
			"EmuCore/GS", "LoadTextureReplacements", false);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_SPINNER, "Asynchronous Texture Loading"),
			FSUI_CSTR("Loads replacement textures on a worker thread, reducing microstutter when replacements are enabled."), "EmuCore/GS",
			"LoadTextureReplacementsAsync", true, replacement_active);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_DATABASE, "Precache Replacements"),
			FSUI_CSTR("Preloads all replacement textures to memory. Not necessary with asynchronous loading."), "EmuCore/GS",
			"PrecacheTextureReplacements", false, replacement_active);

		if (!IsEditingGameSettings(bsi))
		{
			DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_FA_FOLDER_OPEN, "Replacements Directory"), FSUI_CSTR("Folders"), "Textures", EmuFolders::Textures);
		}

		MenuHeading(FSUI_CSTR("Texture Dumping"));
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_DOWNLOAD, "Dump Textures"), FSUI_CSTR("Dumps replaceable textures to disk. Will reduce performance."),
			"EmuCore/GS", "DumpReplaceableTextures", false);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_IMAGES, "Dump Mipmaps"), FSUI_CSTR("Includes mipmaps when dumping textures."), "EmuCore/GS",
			"DumpReplaceableMipmaps", false, dumping_active);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_VIDEO, "Dump FMV Textures"),
			FSUI_CSTR("Allows texture dumping when FMVs are active. You should not enable this."), "EmuCore/GS",
			"DumpTexturesWithFMVActive", false, dumping_active);
	}

	MenuHeading(FSUI_CSTR("Post-Processing"));
	{
		static constexpr const char* s_cas_options[] = {
			FSUI_NSTR("None (Default)"),
			FSUI_NSTR("Sharpen Only (Internal Resolution)"),
			FSUI_NSTR("Sharpen and Resize (Display Resolution)"),
		};
		const bool cas_active = (GetEffectiveIntSetting(bsi, "EmuCore/GS", "CASMode", 0) != static_cast<int>(GSCASMode::Disabled));

		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_EYE, "FXAA"), FSUI_CSTR("Enables FXAA post-processing shader."), "EmuCore/GS", "fxaa", false);
		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_SUN, "Contrast Adaptive Sharpening"), FSUI_CSTR("Enables FidelityFX Contrast Adaptive Sharpening."),
			"EmuCore/GS", "CASMode", static_cast<int>(GSCASMode::Disabled), s_cas_options, std::size(s_cas_options), true);
		DrawIntSpinBoxSetting(bsi, FSUI_ICONSTR(ICON_FA_PENCIL, "CAS Sharpness"),
			FSUI_CSTR("Determines the intensity the sharpening effect in CAS post-processing."), "EmuCore/GS", "CASSharpness", 50, 0, 100,
			1, FSUI_CSTR("%d%%"), cas_active);
	}

	MenuHeading(FSUI_CSTR("Filters"));
	{
		const bool shadeboost_active = GetEffectiveBoolSetting(bsi, "EmuCore/GS", "ShadeBoost", false);

		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_GEM, "Shade Boost"), FSUI_CSTR("Enables brightness/contrast/gamma/saturation adjustment."), "EmuCore/GS",
			"ShadeBoost", false);
		DrawIntRangeSetting(bsi, FSUI_ICONSTR(ICON_FA_SUN, "Shade Boost Brightness"), FSUI_CSTR("Adjusts brightness. 50 is normal."), "EmuCore/GS",
			"ShadeBoost_Brightness", 50, 1, 100, "%d", shadeboost_active);
		DrawIntRangeSetting(bsi, FSUI_ICONSTR(ICON_FA_LIGHTBULB, "Shade Boost Contrast"), FSUI_CSTR("Adjusts contrast. 50 is normal."), "EmuCore/GS",
			"ShadeBoost_Contrast", 50, 1, 100, "%d", shadeboost_active);
		DrawIntRangeSetting(bsi, FSUI_CSTR("Shade Boost Gamma"), FSUI_CSTR("Adjusts gamma. 50 is normal."), "EmuCore/GS",
			"ShadeBoost_Gamma", 50, 1, 100, "%d", shadeboost_active);
		DrawIntRangeSetting(bsi, FSUI_ICONSTR(ICON_FA_DROPLET, "Shade Boost Saturation"), FSUI_CSTR("Adjusts saturation. 50 is normal."), "EmuCore/GS",
			"ShadeBoost_Saturation", 50, 1, 100, "%d", shadeboost_active);

		static constexpr const char* s_tv_shaders[] = {
			FSUI_NSTR("None (Default)"),
			FSUI_NSTR("Scanline Filter"),
			FSUI_NSTR("Diagonal Filter"),
			FSUI_NSTR("Triangular Filter"),
			FSUI_NSTR("Wave Filter"),
			FSUI_NSTR("Lottes CRT"),
			FSUI_NSTR("4xRGSS"),
			FSUI_NSTR("NxAGSS"),
		};
		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_TV, "TV Shaders"), FSUI_CSTR("Applies a shader which replicates the visual effects of different styles of television set."), "EmuCore/GS", "TVShader", 0,
			s_tv_shaders, std::size(s_tv_shaders), true);
	}

	static constexpr const char* s_gsdump_compression[] = {
		FSUI_NSTR("Uncompressed"),
		FSUI_NSTR("LZMA (xz)"),
		FSUI_NSTR("Zstandard (zst)"),
	};

	if (show_advanced_settings)
	{
		MenuHeading(FSUI_CSTR("Advanced"));
		DrawToggleSetting(bsi, FSUI_CSTR("Skip Presenting Duplicate Frames"),
			FSUI_CSTR("Skips displaying frames that don't change in 25/30fps games. Can improve speed, but increase input lag/make frame pacing "
					  "worse."),
			"EmuCore/GS", "SkipDuplicateFrames", false);
		DrawToggleSetting(bsi, FSUI_CSTR("Disable Mailbox Presentation"),
			FSUI_CSTR("Forces the use of FIFO over Mailbox presentation, i.e. double buffering instead of triple buffering. "
					  "Usually results in worse frame pacing."),
			"EmuCore/GS", "DisableMailboxPresentation", false);
		/* DrawToggleSetting(bsi, FSUI_CSTR("Extended Upscaling Multipliers"),
			FSUI_CSTR("Displays additional, very high upscaling multipliers dependent on GPU capability."),
			"EmuCore/GS", "ExtendedUpscalingMultipliers", false); */
		// TODO: Immplement this button properly
		if (IsEditingGameSettings(bsi))
		{
			DrawIntListSetting(bsi, FSUI_CSTR("Hardware Download Mode"), FSUI_CSTR("Changes synchronization behavior for GS downloads."),
				"EmuCore/GS", "HWDownloadMode", static_cast<int>(GSHardwareDownloadMode::Enabled), s_hw_download, std::size(s_hw_download),
				true);
		}
		DrawIntListSetting(bsi, FSUI_CSTR("Allow Exclusive Fullscreen"),
			FSUI_CSTR("Overrides the driver's heuristics for enabling exclusive fullscreen, or direct flip/scanout."), "EmuCore/GS",
			"ExclusiveFullscreenControl", -1, s_generic_options, std::size(s_generic_options), true, -1,
			(renderer == GSRendererType::Auto || renderer == GSRendererType::VK));
		DrawIntListSetting(bsi, FSUI_CSTR("Override Texture Barriers"),
			FSUI_CSTR("Forces texture barrier functionality to the specified value."), "EmuCore/GS", "OverrideTextureBarriers", -1,
			s_generic_options, std::size(s_generic_options), true, -1);
		DrawIntListSetting(bsi, FSUI_CSTR("GS Dump Compression"), FSUI_CSTR("Sets the compression algorithm for GS dumps."), "EmuCore/GS",
			"GSDumpCompression", static_cast<int>(GSDumpCompressionMethod::LZMA), s_gsdump_compression, std::size(s_gsdump_compression), true);
		DrawToggleSetting(bsi, FSUI_CSTR("Disable Framebuffer Fetch"),
			FSUI_CSTR("Prevents the usage of framebuffer fetch when supported by host GPU."), "EmuCore/GS", "DisableFramebufferFetch", false);
		DrawToggleSetting(bsi, FSUI_CSTR("Disable Shader Cache"), FSUI_CSTR("Prevents the loading and saving of shaders/pipelines to disk."),
			"EmuCore/GS", "DisableShaderCache", false);
		DrawToggleSetting(bsi, FSUI_CSTR("Disable Vertex Shader Expand"), FSUI_CSTR("Falls back to the CPU for expanding sprites/lines."),
			"EmuCore/GS", "DisableVertexShaderExpand", false);
		DrawIntListSetting(bsi, FSUI_CSTR("Texture Preloading"),
			FSUI_CSTR(
				"Uploads full textures to the GPU on use, rather than only the utilized regions. Can improve performance in some games."),
			"EmuCore/GS", "texture_preloading", static_cast<int>(TexturePreloadingLevel::Off), s_preloading_options,
			std::size(s_preloading_options), true);
		DrawFloatRangeSetting(bsi, FSUI_CSTR("NTSC Frame Rate"), FSUI_CSTR("Determines what frame rate NTSC games run at."),
			"EmuCore/GS", "FrameRateNTSC", 59.94f, 10.0f, 300.0f, "%.2f Hz");
		DrawFloatRangeSetting(bsi, FSUI_CSTR("PAL Frame Rate"), FSUI_CSTR("Determines what frame rate PAL games run at."),
			"EmuCore/GS", "FrameRatePAL", 50.0f, 10.0f, 300.0f, "%.2f Hz");
	}

	EndMenuButtons();
}

void FullscreenUI::DrawAudioSettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading(FSUI_CSTR("Audio Control"));

	DrawIntRangeSetting(bsi, FSUI_ICONSTR(ICON_FA_VOLUME_HIGH, "Standard Volume"),
		FSUI_CSTR("Controls the volume of the audio played on the host at normal speed."), "SPU2/Output", "StandardVolume", 100,
		0, 100, "%d%%");
	DrawIntRangeSetting(bsi, FSUI_ICONSTR(ICON_FA_FORWARD_FAST, "Fast Forward Volume"),
		FSUI_CSTR("Controls the volume of the audio played on the host when fast forwarding."), "SPU2/Output",
		"FastForwardVolume", 100, 0, 100, "%d%%");
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_VOLUME_XMARK, "Mute All Sound"),
		FSUI_CSTR("Prevents the emulator from producing any audible sound."), "SPU2/Output", "OutputMuted",
		false);

	MenuHeading(FSUI_CSTR("Backend Settings"));

	DrawEnumSetting(
		bsi, FSUI_ICONSTR(ICON_FA_VOLUME_OFF, "Audio Backend"),
		FSUI_CSTR("Determines how audio frames produced by the emulator are submitted to the host."), "SPU2/Output",
		"Backend", Pcsx2Config::SPU2Options::DEFAULT_BACKEND, &AudioStream::ParseBackendName, &AudioStream::GetBackendName,
		&AudioStream::GetBackendDisplayName, AudioBackend::Count);
	DrawEnumSetting(bsi, FSUI_ICONSTR(ICON_PF_SPEAKER_ALT, "Expansion"),
		FSUI_CSTR("Determines how audio is expanded from stereo to surround for supported games."), "SPU2/Output",
		"ExpansionMode", AudioStreamParameters::DEFAULT_EXPANSION_MODE, &AudioStream::ParseExpansionMode,
		&AudioStream::GetExpansionModeName, &AudioStream::GetExpansionModeDisplayName,
		AudioExpansionMode::Count);
	DrawEnumSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROWS_SPIN, "Synchronization"),
		FSUI_CSTR("Changes when SPU samples are generated relative to system emulation."),
		"SPU2/Output", "SyncMode", Pcsx2Config::SPU2Options::DEFAULT_SYNC_MODE,
		&Pcsx2Config::SPU2Options::ParseSyncMode, &Pcsx2Config::SPU2Options::GetSyncModeName,
		&Pcsx2Config::SPU2Options::GetSyncModeDisplayName, Pcsx2Config::SPU2Options::SPU2SyncMode::Count);
	DrawIntRangeSetting(bsi, FSUI_ICONSTR(ICON_FA_BUCKET, "Buffer Size"),
		FSUI_CSTR("Determines the amount of audio buffered before being pulled by the host API."),
		"SPU2/Output", "BufferMS", AudioStreamParameters::DEFAULT_BUFFER_MS, 10, 500, FSUI_CSTR("%d ms"));
	if (!GetEffectiveBoolSetting(bsi, "Audio", "OutputLatencyMinimal", AudioStreamParameters::DEFAULT_OUTPUT_LATENCY_MINIMAL))
	{
		DrawIntRangeSetting(
			bsi, FSUI_ICONSTR(ICON_FA_STOPWATCH_20, "Output Latency"),
			FSUI_CSTR("Determines how much latency there is between the audio being picked up by the host API, and "
					  "played through speakers."),
			"SPU2/Output", "OutputLatencyMS", AudioStreamParameters::DEFAULT_OUTPUT_LATENCY_MS, 1, 500, FSUI_CSTR("%d ms"));
	}
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_STOPWATCH, "Minimal Output Latency"),
		FSUI_CSTR("When enabled, the minimum supported output latency will be used for the host API."),
		"SPU2/Output", "OutputLatencyMinimal", AudioStreamParameters::DEFAULT_OUTPUT_LATENCY_MINIMAL);

	EndMenuButtons();
}

void FullscreenUI::DrawMemoryCardSettingsPage()
{
	BeginMenuButtons();

	SettingsInterface* bsi = GetEditingSettingsInterface();

	MenuHeading(FSUI_CSTR("Settings and Operations"));
	if (MenuButton(FSUI_ICONSTR(ICON_FA_FILE_CIRCLE_PLUS, "Create Memory Card"), FSUI_CSTR("Creates a new memory card file or folder.")))
		OpenMemoryCardCreateDialog();

	DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_FA_FOLDER_OPEN, "Memory Card Directory"), "Folders", "MemoryCards", EmuFolders::MemoryCards);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_MAGNIFYING_GLASS, "Folder Memory Card Filter"),
		FSUI_CSTR("Simulates a larger memory card by filtering saves only to the current game."), "EmuCore", "McdFolderAutoManage", true);

	for (u32 port = 0; port < NUM_MEMORY_CARD_PORTS; port++)
	{
		SmallString str;
		str.format(FSUI_FSTR("Slot {}"), port + 1);
		MenuHeading(str.c_str());

		std::string enable_key(fmt::format("Slot{}_Enable", port + 1));
		std::string file_key(fmt::format("Slot{}_Filename", port + 1));

		DrawToggleSetting(bsi,
			SmallString::from_format(fmt::runtime(FSUI_ICONSTR_S(ICON_PF_MEMORY_CARD, "Memory Card Enabled", "##card_enabled_{}")), port),
			FSUI_CSTR("If not set, this card will be considered unplugged."), "MemoryCards", enable_key.c_str(), true);

		const bool enabled = GetEffectiveBoolSetting(bsi, "MemoryCards", enable_key.c_str(), true);

		const std::optional<SmallString> value = bsi->GetOptionalSmallStringValue("MemoryCards", file_key.c_str(),
			IsEditingGameSettings(bsi) ? std::nullopt : std::optional<const char*>(FileMcd_GetDefaultName(port).c_str()));

		if (MenuButtonWithValue(SmallString::from_format(fmt::runtime(FSUI_ICONSTR_S(ICON_FA_FILE, "Card Name", "##card_name_{}")), port),
				FSUI_CSTR("The selected memory card image will be used for this slot."),
				value.has_value() ? value->c_str() : FSUI_CSTR("Use Global Setting"), enabled))
		{
			ImGuiFullscreen::ChoiceDialogOptions options;
			std::vector<std::string> names;
			if (IsEditingGameSettings(bsi))
				options.emplace_back(FSUI_STR("Use Global Setting"), !value.has_value());
			if (value.has_value() && !value->empty())
			{
				options.emplace_back(fmt::format(FSUI_FSTR("{} (Current)"), value.value()), true);
				names.emplace_back(value->view());
			}
			for (AvailableMcdInfo& mci : FileMcd_GetAvailableCards(IsEditingGameSettings(bsi)))
			{
				if (mci.type == MemoryCardType::Folder)
				{
					options.emplace_back(fmt::format(FSUI_FSTR("{} (Folder)"), mci.name), false);
				}
				else
				{
					static constexpr const char* file_type_names[] = {
						FSUI_NSTR("Unknown"),
						FSUI_NSTR("PS2 (8MB)"),
						FSUI_NSTR("PS2 (16MB)"),
						FSUI_NSTR("PS2 (32MB)"),
						FSUI_NSTR("PS2 (64MB)"),
						FSUI_NSTR("PS1"),
					};
					options.emplace_back(fmt::format("{} ({})", mci.name,
											 Host::TranslateToStringView(TR_CONTEXT, file_type_names[static_cast<u32>(mci.file_type)])),
						false);
				}
				names.push_back(std::move(mci.name));
			}
			OpenChoiceDialog(str.c_str(), false, std::move(options),
				[game_settings = IsEditingGameSettings(bsi), names = std::move(names), file_key = std::move(file_key)](
					s32 index, const std::string& title, bool checked) {
					if (index < 0)
						return;

					auto lock = Host::GetSettingsLock();
					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					if (game_settings && index == 0)
					{
						bsi->DeleteValue("MemoryCards", file_key.c_str());
					}
					else
					{
						if (game_settings)
							index--;
						bsi->SetStringValue("MemoryCards", file_key.c_str(), names[index].c_str());
					}
					SetSettingsChanged(bsi);
					CloseChoiceDialog();
				});
		}

		if (MenuButton(SmallString::from_format(fmt::runtime(FSUI_ICONSTR_S(ICON_FA_EJECT, "Eject Card", "##eject_card_{}")), port),
				FSUI_CSTR("Removes the current card from the slot."), enabled))
		{
			bsi->SetStringValue("MemoryCards", file_key.c_str(), "");
			SetSettingsChanged(bsi);
		}
	}

	EndMenuButtons();
}

void FullscreenUI::DrawNetworkHDDSettingsPage()
{

	static constexpr const char* dns_options[] = {
		FSUI_NSTR("Manual"),
		FSUI_NSTR("Auto"),
		FSUI_NSTR("Internal"),
	};

	static constexpr const char* dns_values[] = {
		"Manual",
		"Auto",
		"Internal",
	};

	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading(FSUI_CSTR("Network Adapter"));

	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_NETWORK_WIRED, "Enable Network Adapter"),
		FSUI_CSTR("Enables the network adapter for online functionality and LAN play."), "DEV9/Eth", "EthEnable", false);

	const bool network_enabled = GetEffectiveBoolSetting(bsi, "DEV9/Eth", "EthEnable", false);

	const std::string current_api = bsi->GetStringValue("DEV9/Eth", "EthApi", "Unset");

	static std::vector<std::vector<AdapterEntry>> adapter_lists;
	static std::vector<Pcsx2Config::DEV9Options::NetApi> api_types;
	static std::vector<std::string> api_display_names;
	static bool adapters_loaded = false;

	if (!adapters_loaded && network_enabled)
	{
		adapter_lists.clear();
		api_types.clear();
		api_display_names.clear();

		adapter_lists.emplace_back();
		api_types.emplace_back(Pcsx2Config::DEV9Options::NetApi::Unset);
		api_display_names.emplace_back("Unset");

		std::vector<AdapterEntry> pcap_adapters = PCAPAdapter::GetAdapters();
		if (!pcap_adapters.empty())
		{
			std::vector<AdapterEntry> pcap_bridged_adapters;
			std::vector<AdapterEntry> pcap_switched_adapters;
			std::set<std::string> seen_bridged_guids;
			std::set<std::string> seen_switched_guids;

			for (const auto& adapter : pcap_adapters)
			{
				if (adapter.type == Pcsx2Config::DEV9Options::NetApi::PCAP_Bridged)
				{
					if (seen_bridged_guids.find(adapter.guid) == seen_bridged_guids.end())
					{
						seen_bridged_guids.insert(adapter.guid);
						pcap_bridged_adapters.push_back(adapter);
					}
				}
				else if (adapter.type == Pcsx2Config::DEV9Options::NetApi::PCAP_Switched)
				{
					if (seen_switched_guids.find(adapter.guid) == seen_switched_guids.end())
					{
						seen_switched_guids.insert(adapter.guid);
						pcap_switched_adapters.push_back(adapter);
					}
				}
			}

			// Sort adapters alphabetically by name
			std::sort(pcap_bridged_adapters.begin(), pcap_bridged_adapters.end(),
				[](const AdapterEntry& a, const AdapterEntry& b) { return a.name < b.name; });
			std::sort(pcap_switched_adapters.begin(), pcap_switched_adapters.end(),
				[](const AdapterEntry& a, const AdapterEntry& b) { return a.name < b.name; });

			if (!pcap_bridged_adapters.empty())
			{
				adapter_lists.emplace_back(pcap_bridged_adapters);
				api_types.emplace_back(Pcsx2Config::DEV9Options::NetApi::PCAP_Bridged);
				api_display_names.emplace_back("PCAP Bridged");
			}

			if (!pcap_switched_adapters.empty())
			{
				adapter_lists.emplace_back(pcap_switched_adapters);
				api_types.emplace_back(Pcsx2Config::DEV9Options::NetApi::PCAP_Switched);
				api_display_names.emplace_back("PCAP Switched");
			}
		}

#ifdef _WIN32
		std::vector<AdapterEntry> tap_adapters = TAPAdapter::GetAdapters();
		if (!tap_adapters.empty())
		{
			// Sort adapters alphabetically by name
			std::sort(tap_adapters.begin(), tap_adapters.end(),
				[](const AdapterEntry& a, const AdapterEntry& b) { return a.name < b.name; });

			adapter_lists.emplace_back(tap_adapters);
			api_types.emplace_back(Pcsx2Config::DEV9Options::NetApi::TAP);
			api_display_names.emplace_back("TAP");
		}
#endif

		std::vector<AdapterEntry> socket_adapters = SocketAdapter::GetAdapters();
		if (!socket_adapters.empty())
		{
			// Sort adapters alphabetically by name
			std::sort(socket_adapters.begin(), socket_adapters.end(),
				[](const AdapterEntry& a, const AdapterEntry& b) { return a.name < b.name; });

			adapter_lists.emplace_back(socket_adapters);
			api_types.emplace_back(Pcsx2Config::DEV9Options::NetApi::Sockets);
			api_display_names.emplace_back("Sockets");
		}

		adapters_loaded = true;
	}

	size_t current_api_index = 0;
	for (size_t i = 0; i < api_types.size(); i++)
	{
		if (current_api == Pcsx2Config::DEV9Options::NetApiNames[static_cast<int>(api_types[i])])
		{
			current_api_index = i;
			break;
		}
	}

	if (MenuButtonWithValue(FSUI_ICONSTR(ICON_FA_PLUG, "Ethernet Device Type"),
			FSUI_CSTR("Determines the simulated Ethernet adapter type."),
			current_api_index < api_display_names.size() ? api_display_names[current_api_index].c_str() : "Unset",
			network_enabled))
	{
		ImGuiFullscreen::ChoiceDialogOptions options;

		for (size_t i = 0; i < api_display_names.size(); i++)
		{
			options.emplace_back(api_display_names[i], i == current_api_index);
		}

		std::vector<Pcsx2Config::DEV9Options::NetApi> current_api_types = api_types;
		std::vector<std::vector<AdapterEntry>> current_adapter_lists = adapter_lists;

		OpenChoiceDialog(FSUI_ICONSTR(ICON_FA_PLUG, "Ethernet Device Type"), false, std::move(options),
			[bsi, current_api_types, current_adapter_lists](s32 index, const std::string& title, bool checked) {
				if (index < 0 || index >= static_cast<s32>(current_api_types.size()))
					return;

				auto lock = Host::GetSettingsLock();
				const std::string selected_api = Pcsx2Config::DEV9Options::NetApiNames[static_cast<int>(current_api_types[index])];
				const std::string previous_api = bsi->GetStringValue("DEV9/Eth", "EthApi", "Unset");
				const std::string previous_device = bsi->GetStringValue("DEV9/Eth", "EthDevice", "");

				bsi->SetStringValue("DEV9/Eth", "EthApi", selected_api.c_str());

				std::string new_device = "";
				if (index < static_cast<s32>(current_adapter_lists.size()))
				{
					const auto& new_adapter_list = current_adapter_lists[index];

					// Try to find the same GUID in the new adapter list
					if (!previous_device.empty())
					{
						for (const auto& adapter : new_adapter_list)
						{
							if (adapter.guid == previous_device)
							{
								new_device = adapter.guid;
								break;
							}
						}
					}

					// If no matching device found, use the first available device
					if (new_device.empty() && !new_adapter_list.empty())
					{
						new_device = new_adapter_list[0].guid;
					}
				}

				bsi->SetStringValue("DEV9/Eth", "EthDevice", new_device.c_str());
				SetSettingsChanged(bsi);

				CloseChoiceDialog();
			});
	}

	const std::string current_device = bsi->GetStringValue("DEV9/Eth", "EthDevice", "");
	const bool show_device_setting = (current_api_index > 0 && current_api_index < api_types.size());

	std::string device_display = "";
	if (show_device_setting && !current_device.empty())
	{
		if (current_api_index < adapter_lists.size())
		{
			const auto& adapter_list = adapter_lists[current_api_index];
			for (const auto& adapter : adapter_list)
			{
				if (adapter.guid == current_device)
				{
					device_display = adapter.name;
					break;
				}
			}
		}

		if (device_display.empty())
			device_display = current_device;
	}
	else if (show_device_setting && current_device.empty())
	{
		device_display = "Not Selected";
	}

	if (MenuButtonWithValue(FSUI_ICONSTR(ICON_FA_ETHERNET, "Ethernet Device"),
			FSUI_CSTR("Network adapter to use for PS2 network emulation."),
			device_display.c_str(),
			network_enabled && show_device_setting))
	{
		ImGuiFullscreen::ChoiceDialogOptions options;

		if (current_api_index > 0 && current_api_index < adapter_lists.size())
		{
			const auto& adapter_list = adapter_lists[current_api_index];
			for (size_t i = 0; i < adapter_list.size(); i++)
			{
				const auto& adapter = adapter_list[i];
				options.emplace_back(adapter.name, adapter.guid == current_device);
			}
		}

		if (options.empty())
		{
			options.emplace_back("No adapters found", false);
		}

		std::vector<AdapterEntry> current_adapter_list;
		if (current_api_index > 0 && current_api_index < adapter_lists.size())
		{
			current_adapter_list = adapter_lists[current_api_index];
		}

		std::string current_api_choice = bsi->GetStringValue("DEV9/Eth", "EthApi", "Unset");

		OpenChoiceDialog(FSUI_ICONSTR(ICON_FA_ETHERNET, "Ethernet Device"), false, std::move(options),
			[bsi, current_adapter_list, current_api_choice](s32 index, const std::string& title, bool checked) {
				if (index < 0 || title == "No adapters found")
					return;

				if (index < static_cast<s32>(current_adapter_list.size()))
				{
					const auto& selected_adapter = current_adapter_list[index];

					auto lock = Host::GetSettingsLock();
					bsi->SetStringValue("DEV9/Eth", "EthApi", current_api_choice.c_str());
					bsi->SetStringValue("DEV9/Eth", "EthDevice", selected_adapter.guid.c_str());
					SetSettingsChanged(bsi);
				}

				CloseChoiceDialog();
			});
	}

	AdapterOptions adapter_options = AdapterOptions::None;
	const std::string final_api = bsi->GetStringValue("DEV9/Eth", "EthApi", "Unset");
	if (final_api == "PCAP Bridged" || final_api == "PCAP Switched")
		adapter_options = PCAPAdapter::GetAdapterOptions();
#ifdef _WIN32
	else if (final_api == "TAP")
		adapter_options = TAPAdapter::GetAdapterOptions();
#endif
	else if (final_api == "Sockets")
		adapter_options = SocketAdapter::GetAdapterOptions();

	const bool dhcp_can_be_disabled = (adapter_options & AdapterOptions::DHCP_ForcedOn) == AdapterOptions::None;

	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_SHIELD_HALVED, "Intercept DHCP"),
		FSUI_CSTR("When enabled, DHCP packets will be intercepted and replaced with internal responses."), "DEV9/Eth", "InterceptDHCP", false, network_enabled && dhcp_can_be_disabled);

	MenuHeading(FSUI_CSTR("Network Configuration"));

	const bool intercept_dhcp = GetEffectiveBoolSetting(bsi, "DEV9/Eth", "InterceptDHCP", false);
	const bool dhcp_forced_on = (adapter_options & AdapterOptions::DHCP_ForcedOn) == AdapterOptions::DHCP_ForcedOn;
	const bool ip_settings_enabled = network_enabled && (intercept_dhcp || dhcp_forced_on);

	const bool ip_can_be_edited = (adapter_options & AdapterOptions::DHCP_OverrideIP) == AdapterOptions::None;
	const bool subnet_can_be_edited = (adapter_options & AdapterOptions::DHCP_OverideSubnet) == AdapterOptions::None;
	const bool gateway_can_be_edited = (adapter_options & AdapterOptions::DHCP_OverideGateway) == AdapterOptions::None;

	DrawIPAddressSetting(bsi, FSUI_ICONSTR(ICON_FA_NETWORK_WIRED, "Address"),
		FSUI_CSTR("IP address for the PS2 virtual network adapter."), "DEV9/Eth", "PS2IP", "0.0.0.0",
		ip_settings_enabled && ip_can_be_edited, LAYOUT_MENU_BUTTON_HEIGHT, g_large_font, g_medium_font, IPAddressType::PS2IP);

	const bool mask_auto = GetEffectiveBoolSetting(bsi, "DEV9/Eth", "AutoMask", true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_WAND_MAGIC, "Auto Subnet Mask"),
		FSUI_CSTR("Automatically determine the subnet mask based on the IP address class."),
		"DEV9/Eth", "AutoMask", true, ip_settings_enabled && subnet_can_be_edited);
	DrawIPAddressSetting(bsi, FSUI_ICONSTR(ICON_FA_NETWORK_WIRED, "Subnet Mask"),
		FSUI_CSTR("Subnet mask for the PS2 virtual network adapter."), "DEV9/Eth", "Mask", "0.0.0.0",
		ip_settings_enabled && subnet_can_be_edited && !mask_auto, LAYOUT_MENU_BUTTON_HEIGHT, g_large_font, g_medium_font, IPAddressType::SubnetMask);

	const bool gateway_auto = GetEffectiveBoolSetting(bsi, "DEV9/Eth", "AutoGateway", true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_WAND_MAGIC, "Auto Gateway"),
		FSUI_CSTR("Automatically determine the gateway address based on the IP address."),
		"DEV9/Eth", "AutoGateway", true, ip_settings_enabled && gateway_can_be_edited);
	DrawIPAddressSetting(bsi, FSUI_ICONSTR(ICON_FA_NETWORK_WIRED, "Gateway Address"),
		FSUI_CSTR("Gateway address for the PS2 virtual network adapter."), "DEV9/Eth", "Gateway", "0.0.0.0",
		ip_settings_enabled && gateway_can_be_edited && !gateway_auto, LAYOUT_MENU_BUTTON_HEIGHT, g_large_font, g_medium_font, IPAddressType::Gateway);

	// DNS Configuration
	const std::string dns1_mode = bsi->GetStringValue("DEV9/Eth", "ModeDNS1", "Auto");
	const std::string dns2_mode = bsi->GetStringValue("DEV9/Eth", "ModeDNS2", "Auto");
	const bool dns1_editable = dns1_mode == "Manual" && ip_settings_enabled;
	const bool dns2_editable = dns2_mode == "Manual" && ip_settings_enabled;

	DrawStringListSetting(bsi, FSUI_ICONSTR(ICON_FA_SERVER, "DNS1 Mode"),
		FSUI_CSTR("Determines how primary DNS requests are handled."), "DEV9/Eth", "ModeDNS1", "Auto",
		dns_options, dns_values, std::size(dns_options), true, ip_settings_enabled);

	DrawIPAddressSetting(bsi, FSUI_ICONSTR(ICON_FA_SERVER, "DNS1 Address"),
		FSUI_CSTR("Primary DNS server address for the PS2 virtual network adapter."), "DEV9/Eth", "DNS1", "0.0.0.0",
		dns1_editable, LAYOUT_MENU_BUTTON_HEIGHT, g_large_font, g_medium_font, IPAddressType::DNS1);

	DrawStringListSetting(bsi, FSUI_ICONSTR(ICON_FA_SERVER, "DNS2 Mode"),
		FSUI_CSTR("Determines how secondary DNS requests are handled."), "DEV9/Eth", "ModeDNS2", "Auto",
		dns_options, dns_values, std::size(dns_options), true, ip_settings_enabled);

	DrawIPAddressSetting(bsi, FSUI_ICONSTR(ICON_FA_SERVER, "DNS2 Address"),
		FSUI_CSTR("Secondary DNS server address for the PS2 virtual network adapter."), "DEV9/Eth", "DNS2", "0.0.0.0",
		dns2_editable, LAYOUT_MENU_BUTTON_HEIGHT, g_large_font, g_medium_font, IPAddressType::DNS2);

	MenuHeading(FSUI_CSTR("Internal HDD"));

	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_FLOPPY_DISK, "Enable HDD"),
		FSUI_CSTR("Enables the internal Hard Disk Drive for expanded storage."), "DEV9/Hdd", "HddEnable", false);

	const bool hdd_enabled = GetEffectiveBoolSetting(bsi, "DEV9/Hdd", "HddEnable", false);

	const SmallString hdd_selection = GetEditingSettingsInterface()->GetSmallStringValue("DEV9/Hdd", "HddFile", "");
	const std::string current_display = hdd_selection.empty() ? std::string(FSUI_CSTR("None")) : std::string(Path::GetFileName(hdd_selection.c_str()));
	if (MenuButtonWithValue(FSUI_ICONSTR(ICON_FA_HARD_DRIVE, "HDD Image Selection"),
			FSUI_CSTR("Changes the HDD image used for PS2 internal storage."),
			current_display.c_str(), hdd_enabled))
	{
		ImGuiFullscreen::ChoiceDialogOptions choices;
		choices.emplace_back(FSUI_STR("None"), hdd_selection.empty());

		std::vector<std::string> values;
		values.push_back("");

		FileSystem::FindResultsArray results;
		FileSystem::FindFiles(EmuFolders::DataRoot.c_str(), "*.raw", FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES, &results);
		for (const FILESYSTEM_FIND_DATA& fd : results)
		{
			const std::string full_path = fd.FileName;
			const std::string filename = std::string(Path::GetFileName(full_path));

			// Get file size and determine LBA mode
			const s64 file_size = FileSystem::GetPathFileSize(full_path.c_str());
			if (file_size > 0)
			{
				const int size_gb = static_cast<int>(file_size / _1gb);
				const bool uses_lba48 = (file_size > static_cast<s64>(120) * _1gb);
				const std::string lba_mode = uses_lba48 ? "LBA48" : "LBA28";

				choices.emplace_back(fmt::format("{} ({} GB, {})", filename, size_gb, lba_mode),
					hdd_selection == full_path);
				values.emplace_back(full_path);
			}
		}

		choices.emplace_back(FSUI_STR("Browse..."), false);
		values.emplace_back("__browse__");

		choices.emplace_back(FSUI_STR("Create New..."), false);
		values.emplace_back("__create__");

		OpenChoiceDialog(FSUI_CSTR("HDD Image Selection"), false, std::move(choices),
			[game_settings = IsEditingGameSettings(bsi), values = std::move(values)](s32 index, const std::string& title, bool checked) {
				if (index < 0)
					return;

				if (values[index] == "__browse__")
				{
					CloseChoiceDialog();

					OpenFileSelector(FSUI_ICONSTR(ICON_FA_HARD_DRIVE, "Select HDD Image File"), false, [game_settings](const std::string& path) {
							if (path.empty())
								return;

							auto lock = Host::GetSettingsLock();
							SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
							bsi->SetStringValue("DEV9/Hdd", "HddFile", path.c_str());
							SetSettingsChanged(bsi);
							ShowToast(std::string(), fmt::format(FSUI_FSTR("Selected HDD image: {}"), Path::GetFileName(path))); }, {"*.raw", "*"}, EmuFolders::DataRoot);
				}
				else if (values[index] == "__create__")
				{
					CloseChoiceDialog();

					std::vector<std::pair<std::string, int>> size_options = {
						{"40 GB (Recommended)", 40},
						{"80 GB", 80},
						{"120 GB (Max LBA28)", 120},
						{"200 GB", 200},
						{"Custom...", -1}};

					ImGuiFullscreen::ChoiceDialogOptions size_choices;
					std::vector<int> size_values;
					for (const auto& [label, size] : size_options)
					{
						size_choices.emplace_back(label, false);
						size_values.push_back(size);
					}

					OpenChoiceDialog(FSUI_ICONSTR(ICON_FA_PLUS, "Select HDD Size"), false, std::move(size_choices),
						[game_settings, size_values = std::move(size_values)](s32 size_index, const std::string& size_title, bool size_checked) {
							if (size_index < 0)
								return;

							if (size_values[size_index] == -1)
							{
								CloseChoiceDialog();

								OpenInputStringDialog(
									FSUI_ICONSTR(ICON_FA_PEN_TO_SQUARE, "Custom HDD Size"),
									FSUI_STR("Enter custom HDD size in gigabytes (40–2000):"),
									std::string(),
									FSUI_ICONSTR(ICON_FA_CHECK, "Create"),
									[game_settings](std::string input) {
										if (input.empty())
											return;

										std::optional<int> custom_size_opt = StringUtil::FromChars<int>(input);
										if (!custom_size_opt.has_value())
										{
											ShowToast(std::string(), FSUI_STR("Invalid size. Please enter a number between 40 and 2000."));
											return;
										}
										int custom_size_gb = custom_size_opt.value();

										if (custom_size_gb < 40 || custom_size_gb > 2000)
										{
											ShowToast(std::string(), FSUI_STR("HDD size must be between 40 GB and 2000 GB."));
											return;
										}

										const bool lba48 = (custom_size_gb > 120);
										const std::string filename = fmt::format("DEV9hdd_{}GB_{}.raw", custom_size_gb, lba48 ? "LBA48" : "LBA28");
										const std::string filepath = Path::Combine(EmuFolders::DataRoot, filename);

										if (FileSystem::FileExists(filepath.c_str()))
										{
											OpenConfirmMessageDialog(
												FSUI_ICONSTR(ICON_FA_TRIANGLE_EXCLAMATION, "File Already Exists"),
												fmt::format(FSUI_FSTR("HDD image '{}' already exists. Do you want to overwrite it?"), filename),
												[filepath, custom_size_gb, lba48, game_settings](bool confirmed) {
													if (confirmed)
													{
														auto lock = Host::GetSettingsLock();
														SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
														bsi->SetStringValue("DEV9/Hdd", "HddFile", filepath.c_str());
														SetSettingsChanged(bsi);
														FullscreenUI::CreateHardDriveWithProgress(filepath, custom_size_gb, lba48);
													}
												});
										}
										else
										{
											auto lock = Host::GetSettingsLock();
											SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
											bsi->SetStringValue("DEV9/Hdd", "HddFile", filepath.c_str());
											SetSettingsChanged(bsi);
											FullscreenUI::CreateHardDriveWithProgress(filepath, custom_size_gb, lba48);
										}
									},
									"40",
									InputFilterType::Numeric);
								return;
							}

							const int size_gb = size_values[size_index];
							const bool lba48 = (size_gb > 120);

							const std::string filename = fmt::format("DEV9hdd_{}GB_{}.raw", size_gb, lba48 ? "LBA48" : "LBA28");
							const std::string filepath = Path::Combine(EmuFolders::DataRoot, filename);

							if (FileSystem::FileExists(filepath.c_str()))
							{
								OpenConfirmMessageDialog(
									FSUI_ICONSTR(ICON_FA_TRIANGLE_EXCLAMATION, "File Already Exists"),
									fmt::format(FSUI_FSTR("HDD image '{}' already exists. Do you want to overwrite it?"), filename),
									[filepath, size_gb, lba48, game_settings](bool confirmed) {
										if (confirmed)
										{
											auto lock = Host::GetSettingsLock();
											SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
											bsi->SetStringValue("DEV9/Hdd", "HddFile", filepath.c_str());
											SetSettingsChanged(bsi);
											FullscreenUI::CreateHardDriveWithProgress(filepath, size_gb, lba48);
										}
									});
							}
							else
							{
								auto lock = Host::GetSettingsLock();
								SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
								bsi->SetStringValue("DEV9/Hdd", "HddFile", filepath.c_str());
								SetSettingsChanged(bsi);
								FullscreenUI::CreateHardDriveWithProgress(filepath, size_gb, lba48);
							}

							CloseChoiceDialog();
						});
				}
				else
				{
					auto lock = Host::GetSettingsLock();
					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					bsi->SetStringValue("DEV9/Hdd", "HddFile", values[index].c_str());
					SetSettingsChanged(bsi);
					CloseChoiceDialog();
				}
			});
	}

	EndMenuButtons();
}

void FullscreenUI::OpenMemoryCardCreateDialog()
{
	OpenInputStringDialog(FSUI_ICONSTR(ICON_FA_PLUS, "Create Memory Card"),
		FSUI_STR("Enter the name for the new memory card."), std::string(),
		FSUI_ICONSTR(ICON_FA_CHECK, "Create"), [](std::string name) {
			if (name.empty())
				return;

			name.erase(std::remove(name.begin(), name.end(), '.'), name.end());
			if (name.empty())
			{
				ShowToast(std::string(), FSUI_STR("Memory card name cannot be empty."));
				return;
			}

			// Show memory card selection dialog
			ImGuiFullscreen::ChoiceDialogOptions options;
			options.emplace_back(FSUI_STR("PS2 (8MB)"), true);
			options.emplace_back(FSUI_STR("PS2 (16MB)"), false);
			options.emplace_back(FSUI_STR("PS2 (32MB)"), false);
			options.emplace_back(FSUI_STR("PS2 (64MB)"), false);
			options.emplace_back(FSUI_STR("PS1 (128KB)"), false);
			options.emplace_back(FSUI_STR("Folder"), false);

			OpenChoiceDialog(FSUI_ICONSTR(ICON_FA_FLOPPY_DISK, "Memory Card Type"), false, std::move(options),
				[name](s32 index, const std::string& title, bool checked) {
					if (index < 0)
						return;

					MemoryCardType type;
					MemoryCardFileType file_type;

					switch (index)
					{
						case 0: // PS2 (8MB)
							type = MemoryCardType::File;
							file_type = MemoryCardFileType::PS2_8MB;
							break;
						case 1: // PS2 (16MB)
							type = MemoryCardType::File;
							file_type = MemoryCardFileType::PS2_16MB;
							break;
						case 2: // PS2 (32MB)
							type = MemoryCardType::File;
							file_type = MemoryCardFileType::PS2_32MB;
							break;
						case 3: // PS2 (64MB)
							type = MemoryCardType::File;
							file_type = MemoryCardFileType::PS2_64MB;
							break;
						case 4: // PS1 (128KB)
							type = MemoryCardType::File;
							file_type = MemoryCardFileType::PS1;
							break;
						case 5: // Folder
							type = MemoryCardType::Folder;
							file_type = MemoryCardFileType::Unknown;
							break;
						default:
							return;
					}

#ifdef _WIN32
					// On Windows, show NTFS compression option for only file options (not folder)
					if (type == MemoryCardType::File)
					{
						ImGuiFullscreen::ChoiceDialogOptions compression_options;
						compression_options.emplace_back(FSUI_STR("Yes - Enable NTFS compression"), true);
						compression_options.emplace_back(FSUI_STR("No - Disable NTFS compression"), false);

						OpenChoiceDialog(FSUI_ICONSTR(ICON_FA_BOX_ARCHIVE, "Use NTFS Compression?"),
							false, std::move(compression_options),
							[name, type, file_type](s32 compression_index, const std::string& compression_title, bool compression_checked) {
								if (compression_index < 0)
									return;

								const bool use_compression = (compression_index == 0); // 0 = Yes, 1 = No
								DoCreateMemoryCard(name, type, file_type, use_compression);
								CloseChoiceDialog();
							});
						return;
					}
					else
					{
						DoCreateMemoryCard(name, type, file_type, false);
						CloseChoiceDialog();
					}
#else
					DoCreateMemoryCard(name, type, file_type, false);
					CloseChoiceDialog();
#endif
				});
		});
}

void FullscreenUI::DoCreateMemoryCard(std::string name, MemoryCardType type, MemoryCardFileType file_type, bool use_ntfs_compression)
{
	// Build the filename with the extension
	const std::string name_str = fmt::format("{}.{}", name,
		(file_type == MemoryCardFileType::PS1) ? "mcr" : "ps2");

	// check the filename
	if (!Path::IsValidFileName(name_str, false))
	{
		ShowToast(std::string(), fmt::format(FSUI_FSTR("Failed to create the Memory Card, because the name '{}' contains one or more invalid characters."), name));
		return;
	}

	// Check if a memory card with this name already exists
	if (FileMcd_GetCardInfo(name_str).has_value())
	{
		ShowToast(std::string(), fmt::format(FSUI_FSTR("Failed to create the Memory Card, because another card with the name '{}' already exists."), name));
		return;
	}

	// Create the memory card
	if (!FileMcd_CreateNewCard(name_str, type, file_type))
	{
		ShowToast(std::string(), FSUI_STR("Failed to create the Memory Card, the log may contain more information."));
		return;
	}

#ifdef _WIN32
	if (type == MemoryCardType::File && use_ntfs_compression)
	{
		const std::string full_path = Path::Combine(EmuFolders::MemoryCards, name_str);
		FileSystem::SetPathCompression(full_path.c_str(), true);
	}
#endif

	ShowToast(std::string(), fmt::format(FSUI_FSTR("Memory Card '{}' created."), name));
}

void FullscreenUI::ResetControllerSettings()
{
	OpenConfirmMessageDialog(FSUI_ICONSTR(u8"🔥", "Reset Controller Settings"),
		FSUI_STR("Are you sure you want to restore the default controller configuration?\n\n"
				 "All shared bindings and configuration will be lost, but your input profiles will remain.\n\n"
				 "You cannot undo this action."),
		[](bool result) {
			if (result)
			{
				SettingsInterface* dsi = GetEditingSettingsInterface();

				Pad::SetDefaultControllerConfig(*dsi);
				Pad::SetDefaultHotkeyConfig(*dsi);
				USB::SetDefaultConfiguration(dsi);
				ShowToast(std::string(), FSUI_STR("Controller settings reset to default."));
			}
		});
}

void FullscreenUI::DoLoadInputProfile()
{
	std::vector<std::string> profiles = Pad::GetInputProfileNames();
	if (profiles.empty())
	{
		ShowToast(std::string(), FSUI_STR("No input profiles available."));
		return;
	}

	ImGuiFullscreen::ChoiceDialogOptions coptions;
	coptions.reserve(profiles.size());
	for (std::string& name : profiles)
		coptions.emplace_back(std::move(name), false);
	OpenChoiceDialog(FSUI_ICONSTR(ICON_FA_FOLDER_OPEN, "Load Profile"), false, std::move(coptions),
		[](s32 index, const std::string& title, bool checked) {
			if (index < 0)
				return;

			INISettingsInterface ssi(VMManager::GetInputProfilePath(title));
			if (!ssi.Load())
			{
				ShowToast(std::string(), fmt::format(FSUI_FSTR("Failed to load '{}'."), title));
				CloseChoiceDialog();
				return;
			}

			auto lock = Host::GetSettingsLock();
			SettingsInterface* dsi = GetEditingSettingsInterface();
			Pad::CopyConfiguration(dsi, ssi, true, true, IsEditingGameSettings(dsi));
			USB::CopyConfiguration(dsi, ssi, true, true);
			SetSettingsChanged(dsi);
			ShowToast(std::string(), fmt::format(FSUI_FSTR("Input profile '{}' loaded."), title));
			CloseChoiceDialog();
		});
}

void FullscreenUI::DoSaveInputProfile(const std::string& name)
{
	INISettingsInterface dsi(VMManager::GetInputProfilePath(name));

	auto lock = Host::GetSettingsLock();
	SettingsInterface* ssi = GetEditingSettingsInterface();
	Pad::CopyConfiguration(&dsi, *ssi, true, true, IsEditingGameSettings(ssi));
	USB::CopyConfiguration(&dsi, *ssi, true, true);
	if (dsi.Save())
		ShowToast(std::string(), fmt::format(FSUI_FSTR("Input profile '{}' saved."), name));
	else
		ShowToast(std::string(), fmt::format(FSUI_FSTR("Failed to save input profile '{}'."), name));
}

void FullscreenUI::DoSaveInputProfile()
{
	std::vector<std::string> profiles = Pad::GetInputProfileNames();

	ImGuiFullscreen::ChoiceDialogOptions coptions;
	coptions.reserve(profiles.size() + 1);
	coptions.emplace_back(FSUI_STR("Create New..."), false);
	for (std::string& name : profiles)
		coptions.emplace_back(std::move(name), false);
	OpenChoiceDialog(
		FSUI_ICONSTR(ICON_FA_FLOPPY_DISK, "Save Profile"), false, std::move(coptions), [](s32 index, const std::string& title, bool checked) {
			if (index < 0)
				return;

			if (index > 0)
			{
				DoSaveInputProfile(title);
				CloseChoiceDialog();
				return;
			}

			CloseChoiceDialog();

			OpenInputStringDialog(FSUI_ICONSTR(ICON_FA_FLOPPY_DISK, "Save Profile"),
				FSUI_STR("Custom input profiles are used to override the Shared input profile for specific games.\n\n"
						 "To apply a custom input profile to a game, go to its Game Properties, then change the 'Input Profile' on the Summary tab.\n\n"
						 "Enter the name for the new input profile:"),
				std::string(),
				FSUI_ICONSTR(ICON_FA_CHECK, "Create"), [](std::string title) {
					if (!title.empty())
						DoSaveInputProfile(title);
				});
		});
}

void FullscreenUI::DoResetSettings()
{
	OpenConfirmMessageDialog(FSUI_ICONSTR(u8"🔥", "Reset Settings"),
		FSUI_STR("Are you sure you want to restore the default settings? Any preferences will be lost."), [](bool result) {
			if (result)
			{
				Host::RunOnCPUThread([]() { Host::RequestResetSettings(false, true, false, false, false); });
				ShowToast(std::string(), FSUI_STR("Settings reset to defaults."));
			}
		});
}

void FullscreenUI::DrawControllerSettingsPage()
{
	BeginMenuButtons();

	SettingsInterface* bsi = GetEditingSettingsInterface();

	MenuHeading(FSUI_CSTR("Configuration"));

	if (MenuButton(
			FSUI_ICONSTR(ICON_FA_FOLDER_OPEN, "Load Profile"), FSUI_CSTR("Replaces these settings with a previously saved input profile.")))
	{
		DoLoadInputProfile();
	}
	if (MenuButton(FSUI_ICONSTR(ICON_FA_FLOPPY_DISK, "Save Profile"), FSUI_CSTR("Stores the current settings to an input profile.")))
	{
		DoSaveInputProfile();
	}

	if (MenuButton(FSUI_ICONSTR(u8"🔥", "Reset Settings"),
			FSUI_CSTR("Resets all configuration to defaults (including bindings).")))
	{
		ResetControllerSettings();
	}

	MenuHeading(FSUI_CSTR("Input Sources"));

	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_GEAR, "Enable SDL Input Source"),
		FSUI_CSTR("The SDL input source supports most controllers."), "InputSources", "SDL", true, true, false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_WIFI, "SDL DualShock 4 / DualSense Enhanced Mode"),
		FSUI_CSTR("Provides vibration and LED control support over Bluetooth."), "InputSources", "SDLControllerEnhancedMode", true,
		bsi->GetBoolValue("InputSources", "SDL", true), false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_LIGHTBULB, "SDL DualSense Player LED"),
		FSUI_CSTR("Enable/Disable the Player LED on DualSense controllers."), "InputSources", "SDLPS5PlayerLED", true,
		bsi->GetBoolValue("InputSources", "SDLControllerEnhancedMode", true), true);
#ifdef _WIN32
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_GEAR, "SDL Raw Input"), FSUI_CSTR("Allow SDL to use raw access to input devices."),
		"InputSources", "SDLRawInput", false, bsi->GetBoolValue("InputSources", "SDL", true), false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_GEAR, "Enable XInput Input Source"),
		FSUI_CSTR("The XInput source provides support for XBox 360/XBox One/XBox Series controllers."), "InputSources", "XInput", false,
		true, false);
#endif

	MenuHeading(FSUI_CSTR("Multitap"));
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_SQUARE_PLUS, "Enable Console Port 1 Multitap"),
		FSUI_CSTR("Enables an additional three controller slots. Not supported in all games."), "Pad", "MultitapPort1", false, true, false);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_SQUARE_PLUS, "Enable Console Port 2 Multitap"),
		FSUI_CSTR("Enables an additional three controller slots. Not supported in all games."), "Pad", "MultitapPort2", false, true, false);

	const std::array<bool, 2> mtap_enabled = {
		{bsi->GetBoolValue("Pad", "MultitapPort1", false), bsi->GetBoolValue("Pad", "MultitapPort2", false)}};

	// we reorder things a little to make it look less silly for mtap
	static constexpr const std::array<char, 4> mtap_slot_names = {{'A', 'B', 'C', 'D'}};
	static constexpr const std::array<u32, Pad::NUM_CONTROLLER_PORTS> mtap_port_order = {{0, 2, 3, 4, 1, 5, 6, 7}};
	static constexpr const std::array<const char*, Pad::NUM_CONTROLLER_PORTS> sections = {
		{"Pad1", "Pad2", "Pad3", "Pad4", "Pad5", "Pad6", "Pad7", "Pad8"}};

	// create the ports
	for (u32 global_slot : mtap_port_order)
	{
		const bool is_mtap_port = sioPadIsMultitapSlot(global_slot);
		const auto [mtap_port, mtap_slot] = sioConvertPadToPortAndSlot(global_slot);
		if (is_mtap_port && !mtap_enabled[mtap_port])
			continue;

		ImGui::PushID(global_slot);
		if (mtap_enabled[mtap_port])
		{
			MenuHeading(SmallString::from_format(
				fmt::runtime(FSUI_ICONSTR(ICON_FA_PLUG, "Controller Port {}{}")), mtap_port + 1, mtap_slot_names[mtap_slot]));
		}
		else
		{
			MenuHeading(SmallString::from_format(fmt::runtime(FSUI_ICONSTR(ICON_FA_PLUG, "Controller Port {}")), mtap_port + 1));
		}

		const char* section = sections[global_slot];
		const Pad::ControllerInfo* ci = Pad::GetConfigControllerType(*bsi, section, global_slot);
		if (MenuButton(FSUI_ICONSTR(ICON_PF_GAMEPAD_ALT, "Controller Type"), ci ? ci->GetLocalizedName() : FSUI_CSTR("Unknown")))
		{
			const std::vector<std::pair<const char*, const char*>> raw_options = Pad::GetControllerTypeNames();
			ImGuiFullscreen::ChoiceDialogOptions options;
			options.reserve(raw_options.size());
			for (auto& it : raw_options)
				options.emplace_back(it.second, (ci && ci->name == it.first));
			OpenChoiceDialog(fmt::format(FSUI_FSTR("Port {} Controller Type"), global_slot + 1).c_str(), false, std::move(options),
				[game_settings = IsEditingGameSettings(bsi), section, raw_options = std::move(raw_options)](
					s32 index, const std::string& title, bool checked) {
					if (index < 0)
						return;

					auto lock = Host::GetSettingsLock();
					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					bsi->SetStringValue(section, "Type", raw_options[index].first);
					SetSettingsChanged(bsi);
					CloseChoiceDialog();
				});
		}

		if (!ci || ci->bindings.empty())
		{
			ImGui::PopID();
			continue;
		}

		if (MenuButton(
				FSUI_ICONSTR(ICON_FA_WAND_MAGIC_SPARKLES, "Automatic Mapping"), FSUI_CSTR("Attempts to map the selected port to a chosen controller.")))
			StartAutomaticBinding(global_slot);

		for (const InputBindingInfo& bi : ci->bindings)
			if (bi.name) [[likely]]
				DrawInputBindingButton(bsi, bi.bind_type, section, bi.name, Host::TranslateToCString("Pad", bi.display_name), bi.icon_name, true);

		if (mtap_enabled[mtap_port])
		{
			MenuHeading(SmallString::from_format(
				fmt::runtime(FSUI_ICONSTR(ICON_PF_EMPTY_KEYCAP, "Controller Port {}{} Macros")), mtap_port + 1, mtap_slot_names[mtap_slot]));
		}
		else
		{
			MenuHeading(SmallString::from_format(fmt::runtime(FSUI_ICONSTR(ICON_PF_EMPTY_KEYCAP, "Controller Port {} Macros")), mtap_port + 1));
		}

		static bool macro_button_expanded[Pad::NUM_CONTROLLER_PORTS][Pad::NUM_MACRO_BUTTONS_PER_CONTROLLER] = {};

		for (u32 macro_index = 0; macro_index < Pad::NUM_MACRO_BUTTONS_PER_CONTROLLER; macro_index++)
		{
			bool& expanded = macro_button_expanded[global_slot][macro_index];
			expanded ^=
				MenuHeadingButton(SmallString::from_format(fmt::runtime(FSUI_ICONSTR(ICON_PF_EMPTY_KEYCAP, "Macro Button {}")), macro_index + 1),
					macro_button_expanded[global_slot][macro_index] ? ICON_FA_CHEVRON_UP : ICON_FA_CHEVRON_DOWN);
			if (!expanded)
				continue;

			ImGui::PushID(&expanded);

			DrawInputBindingButton(
				bsi, InputBindingInfo::Type::Macro, section, TinyString::from_format("Macro{}", macro_index + 1), FSUI_CSTR("Trigger"), nullptr);

			SmallString binds_string = bsi->GetSmallStringValue(section, fmt::format("Macro{}Binds", macro_index + 1).c_str());
			TinyString pretty_binds_string;
			if (!binds_string.empty())
			{
				for (const std::string_view& bind : StringUtil::SplitString(binds_string, '&', true))
				{
					const char* dispname = nullptr;
					for (const InputBindingInfo& bi : ci->bindings)
					{
						if (bind == bi.name)
						{
							dispname = bi.icon_name ? bi.icon_name : Host::TranslateToCString("Pad", bi.display_name);
							break;
						}
					}
					pretty_binds_string.append_format("{}{}", pretty_binds_string.empty() ? "" : " ", dispname);
				}
			}
			if (MenuButtonWithValue(FSUI_ICONSTR(ICON_FA_KEYBOARD, "Buttons"), nullptr, pretty_binds_string.empty() ? FSUI_CSTR("-") : pretty_binds_string.c_str(), true,
					LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
			{
				std::vector<std::string_view> buttons_split(StringUtil::SplitString(binds_string, '&', true));
				ImGuiFullscreen::ChoiceDialogOptions options;
				for (const InputBindingInfo& bi : ci->bindings)
				{
					if (bi.bind_type != InputBindingInfo::Type::Button && bi.bind_type != InputBindingInfo::Type::Axis &&
						bi.bind_type != InputBindingInfo::Type::HalfAxis)
					{
						continue;
					}
					options.emplace_back(Host::TranslateToCString("Pad", bi.display_name),
						std::any_of(
							buttons_split.begin(), buttons_split.end(), [bi](const std::string_view& it) { return (it == bi.name); }));
				}

				OpenChoiceDialog(fmt::format(FSUI_FSTR("Select Macro {} Binds"), macro_index + 1).c_str(), true, std::move(options),
					[section, macro_index, ci](s32 index, const std::string& title, bool checked) {
						// convert display name back to bind name
						std::string_view to_modify;
						for (const InputBindingInfo& bi : ci->bindings)
						{
							if (bi.display_name == title)
							{
								to_modify = bi.name;
								break;
							}
						}
						if (to_modify.empty())
						{
							// wtf?
							return;
						}

						auto lock = Host::GetSettingsLock();
						SettingsInterface* bsi = GetEditingSettingsInterface();
						const std::string key(fmt::format("Macro{}Binds", macro_index + 1));

						std::string binds_string(bsi->GetStringValue(section, key.c_str()));
						std::vector<std::string_view> buttons_split(StringUtil::SplitString(binds_string, '&', true));
						auto it = std::find(buttons_split.begin(), buttons_split.end(), to_modify);
						if (checked)
						{
							if (it == buttons_split.end())
								buttons_split.push_back(to_modify);
						}
						else
						{
							if (it != buttons_split.end())
								buttons_split.erase(it);
						}

						binds_string = StringUtil::JoinString(buttons_split.begin(), buttons_split.end(), " & ");
						if (binds_string.empty())
							bsi->DeleteValue(section, key.c_str());
						else
							bsi->SetStringValue(section, key.c_str(), binds_string.c_str());

						SetSettingsChanged(bsi);
					});
			}

			DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_GAMEPAD, "Press To Toggle"),
				FSUI_CSTR("Toggles the macro when the button is pressed, instead of held."), section,
				TinyString::from_format("Macro{}Toggle", macro_index + 1), false, true, false);

			const TinyString freq_key = TinyString::from_format("Macro{}Frequency", macro_index + 1);
			const TinyString freq_label = TinyString::from_format(ICON_FA_CLOCK " {}##macro_{}_frequency", FSUI_VSTR("Frequency"), macro_index + 1);
			s32 frequency = bsi->GetIntValue(section, freq_key.c_str(), 0);
			const SmallString freq_summary =
				((frequency == 0) ? TinyString(FSUI_VSTR("Disabled")) :
									TinyString::from_format(FSUI_FSTR("{} Frames"), frequency));
			if (MenuButtonWithValue(freq_label, FSUI_CSTR("Determines the frequency at which the macro will toggle the buttons on and off (aka auto fire)."), freq_summary, true))
				ImGui::OpenPopup(freq_label.c_str());

			const std::string pressure_key(fmt::format("Macro{}Pressure", macro_index + 1));
			DrawFloatSpinBoxSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROW_DOWN, "Pressure"),
				FSUI_CSTR("Determines how much pressure is simulated when macro is active."), section, pressure_key.c_str(), 1.0f, 0.01f,
				1.0f, 0.01f, 100.0f, "%.0f%%");

			const std::string deadzone_key(fmt::format("Macro{}Deadzone", macro_index + 1));
			DrawFloatSpinBoxSetting(bsi, FSUI_ICONSTR(ICON_FA_SKULL, "Deadzone"),
				FSUI_CSTR("Determines the pressure required to activate the macro."), section, deadzone_key.c_str(), 0.0f, 0.00f, 1.0f,
				0.01f, 100.0f, "%.0f%%");

			ImGui::SetNextWindowSize(LayoutScale(500.0f, 180.0f));
			ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

			ImGui::PushFont(g_large_font.first, g_large_font.second);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
				LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

			if (ImGui::BeginPopupModal(
					freq_label.c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
			{
				ImGui::SetNextItemWidth(LayoutScale(450.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, LayoutScale(8.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, LayoutScale(1.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, LayoutScale(8.0f));
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.45f, 0.65f, 0.95f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.55f, 0.75f, 1.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
				if (ImGui::SliderInt("##value", &frequency, 0, 60, FSUI_CSTR("Toggle every %d frames"), ImGuiSliderFlags_NoInput))
				{
					if (frequency == 0)
						bsi->DeleteValue(section, freq_key.c_str());
					else
						bsi->SetIntValue(section, freq_key.c_str(), frequency);

					SetSettingsChanged(bsi);
				}

				ImGui::PopStyleColor(7);
				ImGui::PopStyleVar(3);

				BeginMenuButtons();
				if (MenuButton("OK", nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
					ImGui::CloseCurrentPopup();
				EndMenuButtons();

				ImGui::EndPopup();
			}

			ImGui::PopStyleVar(4);
			ImGui::PopFont();

			ImGui::PopID();
		}

		if (!ci->settings.empty())
		{
			if (mtap_enabled[mtap_port])
			{
				MenuHeading(SmallString::from_format(fmt::runtime(FSUI_ICONSTR(ICON_FA_SLIDERS, "Controller Port {}{} Settings")),
					mtap_port + 1, mtap_slot_names[mtap_slot]));
			}
			else
			{
				MenuHeading(
					SmallString::from_format(fmt::runtime(FSUI_ICONSTR(ICON_FA_SLIDERS, "Controller Port {} Settings")), mtap_port + 1));
			}

			for (const SettingInfo& si : ci->settings)
				DrawSettingInfoSetting(bsi, section, Host::TranslateToCString("Pad", si.name), si, "Pad");
		}

		ImGui::PopID();
	}

	for (u32 port = 0; port < USB::NUM_PORTS; port++)
	{
		ImGui::PushID(port);
		MenuHeading(TinyString::from_format(fmt::runtime(FSUI_ICONSTR(ICON_FA_PLUG, "USB Port {}")), port + 1));

		const std::string type(USB::GetConfigDevice(*bsi, port));
		if (MenuButton(FSUI_ICONSTR(ICON_PF_USB, "Device Type"), USB::GetDeviceName(type)))
		{
			const std::vector<std::pair<const char*, const char*>> raw_options = USB::GetDeviceTypes();
			ImGuiFullscreen::ChoiceDialogOptions options;
			options.reserve(raw_options.size());
			for (auto& it : raw_options)
			{
				options.emplace_back(it.second, type == it.first);
			}
			OpenChoiceDialog(fmt::format(FSUI_FSTR("Port {} Device"), port + 1).c_str(), false, std::move(options),
				[game_settings = IsEditingGameSettings(bsi), raw_options = std::move(raw_options), port](
					s32 index, const std::string& title, bool checked) {
					if (index < 0)
						return;

					auto lock = Host::GetSettingsLock();
					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					USB::SetConfigDevice(*bsi, port, raw_options[static_cast<u32>(index)].first);
					SetSettingsChanged(bsi);
					CloseChoiceDialog();
				});
		}

		if (type.empty() || type == "None")
		{
			ImGui::PopID();
			continue;
		}

		const u32 subtype = USB::GetConfigSubType(*bsi, port, type);
		const std::span<const char*> subtypes(USB::GetDeviceSubtypes(type));
		if (!subtypes.empty())
		{
			const char* subtype_name = USB::GetDeviceSubtypeName(type, subtype);
			if (MenuButton(FSUI_ICONSTR(ICON_FA_GEAR, "Device Subtype"), subtype_name))
			{
				ImGuiFullscreen::ChoiceDialogOptions options;
				options.reserve(subtypes.size());
				for (u32 i = 0; i < subtypes.size(); i++)
					options.emplace_back(subtypes[i], i == subtype);

				OpenChoiceDialog(fmt::format(FSUI_FSTR("Port {} Subtype"), port + 1).c_str(), false, std::move(options),
					[game_settings = IsEditingGameSettings(bsi), port, type](s32 index, const std::string& title, bool checked) {
						if (index < 0)
							return;

						auto lock = Host::GetSettingsLock();
						SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
						USB::SetConfigSubType(*bsi, port, type.c_str(), static_cast<u32>(index));
						SetSettingsChanged(bsi);
						CloseChoiceDialog();
					});
			}
		}

		const std::span<const InputBindingInfo> bindings(USB::GetDeviceBindings(type, subtype));
		if (!bindings.empty())
		{
			MenuHeading(TinyString::from_format(fmt::runtime(FSUI_ICONSTR(ICON_FA_KEYBOARD, "{} Bindings")), USB::GetDeviceName(type)));

			if (MenuButton(FSUI_ICONSTR(ICON_FA_TRASH, "Clear Bindings"), FSUI_CSTR("Clears all bindings for this USB controller.")))
			{
				USB::ClearPortBindings(*bsi, port);
				SetSettingsChanged(bsi);
			}

			const std::string section(USB::GetConfigSection(port));
			for (const InputBindingInfo& bi : bindings)
			{
				DrawInputBindingButton(bsi, bi.bind_type, section.c_str(), USB::GetConfigSubKey(type, bi.name).c_str(),
					Host::TranslateToCString("USB", bi.display_name), bi.icon_name);
			}
		}

		const std::span<const SettingInfo> settings(USB::GetDeviceSettings(type, subtype));
		if (!settings.empty())
		{
			MenuHeading(TinyString::from_format(fmt::runtime(FSUI_ICONSTR(ICON_FA_SLIDERS, "{} Settings")), USB::GetDeviceName(type)));

			const std::string section(USB::GetConfigSection(port));
			for (const SettingInfo& si : settings)
				DrawSettingInfoSetting(bsi, section.c_str(), USB::GetConfigSubKey(type, si.name).c_str(), si, "USB");
		}
		ImGui::PopID();
	}

	EndMenuButtons();
}

void FullscreenUI::DrawHotkeySettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	const HotkeyInfo* last_category = nullptr;
	for (const HotkeyInfo* hotkey : s_hotkey_list_cache)
	{
		if (!last_category || std::strcmp(hotkey->category, last_category->category) != 0)
		{
			MenuHeading(Host::TranslateToCString("Hotkeys", hotkey->category));
			last_category = hotkey;
		}

		DrawInputBindingButton(
			bsi, InputBindingInfo::Type::Button, "Hotkeys", hotkey->name, Host::TranslateToCString("Hotkeys", hotkey->display_name), nullptr, false);
	}

	EndMenuButtons();
}

void FullscreenUI::DrawFoldersSettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading(FSUI_CSTR("Data Save Locations"));

	DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_FA_CUBES, "Cache Directory"), "Folders", "Cache", EmuFolders::Cache);
	DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_FA_IMAGES, "Covers Directory"), "Folders", "Covers", EmuFolders::Covers);
	DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_FA_CAMERA, "Snapshots Directory"), "Folders", "Snapshots", EmuFolders::Snapshots);
	DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_FA_FLOPPY_DISK, "Save States Directory"), "Folders", "Savestates", EmuFolders::Savestates);
	DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_FA_WRENCH, "Game Settings Directory"), "Folders", "GameSettings", EmuFolders::GameSettings);
	DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_PF_GAMEPAD_ALT, "Input Profile Directory"), "Folders", "InputProfiles", EmuFolders::InputProfiles);
	DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_PF_INFINITY, "Cheats Directory"), "Folders", "Cheats", EmuFolders::Cheats);
	DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_FA_BANDAGE, "Patches Directory"), "Folders", "Patches", EmuFolders::Patches);
	DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_FA_SHIRT, "Texture Replacements Directory"), "Folders", "Textures", EmuFolders::Textures);
	DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_FA_VIDEO, "Video Dumping Directory"), "Folders", "Videos", EmuFolders::Videos);

	EndMenuButtons();
}

void FullscreenUI::DrawAdvancedSettingsPage()
{
	static constexpr const char* ee_rounding_mode_settings[] = {
		FSUI_NSTR("Nearest"),
		FSUI_NSTR("Negative"),
		FSUI_NSTR("Positive"),
		FSUI_NSTR("Chop/Zero (Default)"),
	};

	SettingsInterface* bsi = GetEditingSettingsInterface();

	const bool show_advanced_settings = ShouldShowAdvancedSettings(bsi);

	BeginMenuButtons();

	if (!IsEditingGameSettings(bsi))
	{
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_BIOHAZARD, "Show Advanced Settings"),
			FSUI_CSTR("Changing these options may cause games to become non-functional. Modify at your own risk, the PCSX2 team will not "
					  "provide support for configurations with these settings changed."),
			"UI", "ShowAdvancedSettings", false);
	}

	MenuHeading(FSUI_CSTR("Logging"));

	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_TERMINAL, "System Console"),
		FSUI_CSTR("Writes log messages to the system console (console window/standard output)."), "Logging", "EnableSystemConsole", false);
	DrawToggleSetting(
		bsi, FSUI_ICONSTR(ICON_FA_SCROLL, "File Logging"), FSUI_CSTR("Writes log messages to emulog.txt."), "Logging", "EnableFileLogging", true);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_SCROLL, "Verbose Logging"), FSUI_CSTR("Writes dev log messages to log sinks."), "Logging", "EnableVerbose",
		false, !IsDevBuild);

	if (show_advanced_settings)
	{
		DrawToggleSetting(
			bsi, FSUI_ICONSTR(ICON_FA_CLOCK, "Log Timestamps"), FSUI_CSTR("Writes timestamps alongside log messages."), "Logging", "EnableTimestamps", true);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_MICROCHIP, "EE Console"), FSUI_CSTR("Writes debug messages from the game's EE code to the console."),
			"Logging", "EnableEEConsole", true);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_MICROCHIP, "IOP Console"), FSUI_CSTR("Writes debug messages from the game's IOP code to the console."),
			"Logging", "EnableIOPConsole", true);
		DrawToggleSetting(
			bsi, FSUI_ICONSTR(ICON_FA_COMPACT_DISC, "CDVD Verbose Reads"), FSUI_CSTR("Logs disc reads from games."), "EmuCore", "CdvdVerboseReads", false);
	}

	static constexpr const char* s_savestate_compression_type[] = {
		FSUI_NSTR("Uncompressed"),
		FSUI_NSTR("Deflate64"),
		FSUI_NSTR("Zstandard"),
		FSUI_NSTR("LZMA2"),
	};

	static constexpr const char* s_savestate_compression_ratio[] = {
		FSUI_NSTR("Low (Fast)"),
		FSUI_NSTR("Medium (Recommended)"),
		FSUI_NSTR("High"),
		FSUI_NSTR("Very High (Slow, Not Recommended)"),
	};

	if (show_advanced_settings)
	{
		MenuHeading(FSUI_CSTR("Emotion Engine"));

		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROW_TREND_DOWN, "Rounding Mode"),
			FSUI_CSTR("Determines how the results of floating-point operations are rounded. Some games need specific settings."),
			"EmuCore/CPU", "FPU.Roundmode", static_cast<int>(FPRoundMode::ChopZero), ee_rounding_mode_settings,
			std::size(ee_rounding_mode_settings), true);
		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_DIVIDE, "Division Rounding Mode"),
			FSUI_CSTR("Determines how the results of floating-point division is rounded. Some games need specific settings."),
			"EmuCore/CPU", "FPUDiv.Roundmode", static_cast<int>(FPRoundMode::Nearest),
			ee_rounding_mode_settings, std::size(ee_rounding_mode_settings), true);
		DrawClampingModeSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROW_TURN_DOWN, "Clamping Mode"),
			FSUI_CSTR("Determines how out-of-range floating point numbers are handled. Some games need specific settings."), -1);

		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_MICROCHIP, "Enable EE Recompiler"),
			FSUI_CSTR("Performs just-in-time binary translation of 64-bit MIPS-IV machine code to native code."), "EmuCore/CPU/Recompiler",
			"EnableEE", true);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_BUCKET, "Enable EE Cache"), FSUI_CSTR("Enables simulation of the EE's cache. Slow."),
			"EmuCore/CPU/Recompiler", "EnableEECache", false);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROWS_SPIN, "Enable INTC Spin Detection"),
			FSUI_CSTR("Huge speedup for some games, with almost no compatibility side effects."), "EmuCore/Speedhacks", "IntcStat", true);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROWS_SPIN, "Enable Wait Loop Detection"),
			FSUI_CSTR("Moderate speedup for some games, with no known side effects."), "EmuCore/Speedhacks", "WaitLoop", true);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_MEMORY, "Enable Fast Memory Access"),
			FSUI_CSTR("Uses backpatching to avoid register flushing on every memory access."), "EmuCore/CPU/Recompiler", "EnableFastmem",
			true);

		MenuHeading(FSUI_CSTR("Vector Units"));
		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROW_TREND_DOWN, "VU0 Rounding Mode"),
			FSUI_CSTR("Determines how the results of floating-point operations are rounded. Some games need specific settings."),
			"EmuCore/CPU", "VU0.Roundmode", static_cast<int>(FPRoundMode::ChopZero),
			ee_rounding_mode_settings, std::size(ee_rounding_mode_settings), true);
		DrawClampingModeSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROW_TURN_DOWN, "VU0 Clamping Mode"),
			FSUI_CSTR("Determines how out-of-range floating point numbers are handled. Some games need specific settings."), 0);
		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROW_TREND_DOWN, "VU1 Rounding Mode"),
			FSUI_CSTR("Determines how the results of floating-point operations are rounded. Some games need specific settings."),
			"EmuCore/CPU", "VU1.Roundmode", static_cast<int>(FPRoundMode::ChopZero),
			ee_rounding_mode_settings, std::size(ee_rounding_mode_settings), true);
		DrawClampingModeSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROW_TURN_DOWN, "VU1 Clamping Mode"),
			FSUI_CSTR("Determines how out-of-range floating point numbers are handled. Some games need specific settings."), 1);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_MICROCHIP, "Enable VU0 Recompiler (Micro Mode)"),
			FSUI_CSTR("New Vector Unit recompiler with much improved compatibility. Recommended."), "EmuCore/CPU/Recompiler", "EnableVU0",
			true);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_MICROCHIP, "Enable VU1 Recompiler"),
			FSUI_CSTR("New Vector Unit recompiler with much improved compatibility. Recommended."), "EmuCore/CPU/Recompiler", "EnableVU1",
			true);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_FLAG, "Enable VU Flag Optimization"),
			FSUI_CSTR("Good speedup and high compatibility, may cause graphical errors."), "EmuCore/Speedhacks", "vuFlagHack", true);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_CLOCK, "Enable Instant VU1"),
			FSUI_CSTR("Runs VU1 instantly. Provides a modest speed improvement in most games. Safe for most games, but a few games may exhibit graphical errors."),
			"EmuCore/Speedhacks", "vu1Instant", true);

		MenuHeading(FSUI_CSTR("I/O Processor"));
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_MICROCHIP, "Enable IOP Recompiler"),
			FSUI_CSTR("Performs just-in-time binary translation of 32-bit MIPS-I machine code to native code."), "EmuCore/CPU/Recompiler",
			"EnableIOP", true);

		MenuHeading(FSUI_CSTR("Save State Management"));
		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_BOX_OPEN, "Compression Method"), FSUI_CSTR("Sets the compression algorithm for savestate."), "EmuCore",
			"SavestateCompressionType", static_cast<int>(SavestateCompressionMethod::Zstandard), s_savestate_compression_type, std::size(s_savestate_compression_type), true);
		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_COMPRESS, "Compression Level"), FSUI_CSTR("Sets the compression level for savestate."), "EmuCore",
			"SavestateCompressionRatio", static_cast<int>(SavestateCompressionLevel::Medium), s_savestate_compression_ratio, std::size(s_savestate_compression_ratio), true);

		MenuHeading(FSUI_CSTR("Graphics"));
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_BUG, "Use Debug Device"), FSUI_CSTR("Enables API-level validation of graphics commands."), "EmuCore/GS",
			"UseDebugDevice", false);
	}

	EndMenuButtons();
}

void FullscreenUI::DrawPatchesOrCheatsSettingsPage(bool cheats)
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	const std::vector<Patch::PatchInfo>& patch_list = cheats ? s_game_cheats_list : s_game_patch_list;
	std::vector<std::string>& enable_list = cheats ? s_enabled_game_cheat_cache : s_enabled_game_patch_cache;
	const char* section = cheats ? Patch::CHEATS_CONFIG_SECTION : Patch::PATCHES_CONFIG_SECTION;
	const bool master_enable = cheats ? GetEffectiveBoolSetting(bsi, "EmuCore", "EnableCheats", false) : true;

	BeginMenuButtons();

	if (cheats)
	{
		MenuHeading(FSUI_CSTR("Settings"));
		DrawToggleSetting(
			bsi, FSUI_CSTR("Enable Cheats"), FSUI_CSTR("Enables loading cheats from pnach files."), "EmuCore", "EnableCheats", false);

		if (patch_list.empty())
		{
			ActiveButton(
				FSUI_CSTR("No cheats are available for this game."), false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		}
		else
		{
			MenuHeading(FSUI_CSTR("Cheat Codes"));
		}
	}
	else
	{
		if (patch_list.empty())
		{
			ActiveButton(
				FSUI_CSTR("No patches are available for this game."), false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		}
		else
		{
			MenuHeading(FSUI_CSTR("Game Patches"));
		}
	}

	for (const Patch::PatchInfo& pi : patch_list)
	{
		const auto enable_it = std::find(enable_list.begin(), enable_list.end(), pi.name);

		bool state = (enable_it != enable_list.end());
		if (ToggleButton(pi.name.c_str(), pi.description.c_str(), &state, master_enable))
		{
			if (state)
			{
				bsi->AddToStringList(section, Patch::PATCH_ENABLE_CONFIG_KEY, pi.name.c_str());
				enable_list.push_back(pi.name);
			}
			else
			{
				bsi->RemoveFromStringList(section, Patch::PATCH_ENABLE_CONFIG_KEY, pi.name.c_str());
				enable_list.erase(enable_it);
			}

			SetSettingsChanged(bsi);
		}
	}

	if (cheats && s_game_cheat_unlabelled_count > 0)
	{
		ActiveButton(SmallString::from_format(master_enable ? FSUI_FSTR("{} unlabelled patch codes will automatically activate.") :
															  FSUI_FSTR("{} unlabelled patch codes found but not enabled."),
						 s_game_cheat_unlabelled_count),
			false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
	}

	if (!patch_list.empty() || (cheats && s_game_cheat_unlabelled_count > 0))
	{
		ActiveButton(
			cheats ? FSUI_CSTR("Activating cheats can cause unpredictable behavior, crashing, soft-locks, or broken saved games.") :
					 FSUI_CSTR("Activating game patches can cause unpredictable behavior, crashing, soft-locks, or broken saved games."),
			false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		ActiveButton(
			FSUI_CSTR("Use patches at your own risk, the PCSX2 team will provide no support for users who have enabled game patches."),
			false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
	}

	EndMenuButtons();
}

void FullscreenUI::DrawGameFixesSettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading(FSUI_CSTR("Game Fixes"));
	ActiveButton(
		FSUI_CSTR("Game fixes should not be modified unless you are aware of what each option does and the implications of doing so."),
		false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

	DrawToggleSetting(bsi, FSUI_CSTR("FPU Multiply Hack"), FSUI_CSTR("For Tales of Destiny."), "EmuCore/Gamefixes", "FpuMulHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("Use Software Renderer For FMVs"),
		FSUI_CSTR("Needed for some games with complex FMV rendering."), "EmuCore/Gamefixes", "SoftwareRendererFMVHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("Skip MPEG Hack"), FSUI_CSTR("Skips videos/FMVs in games to avoid game hanging/freezes."),
		"EmuCore/Gamefixes", "SkipMPEGHack", false);
	DrawToggleSetting(
		bsi, FSUI_CSTR("Preload TLB Hack"), FSUI_CSTR("To avoid TLB miss on Goemon."), "EmuCore/Gamefixes", "GoemonTlbHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("EE Timing Hack"),
		FSUI_CSTR("General-purpose timing hack. Known to affect following games: Digital Devil Saga, SSX."),
		"EmuCore/Gamefixes", "EETimingHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("Instant DMA Hack"),
		FSUI_CSTR("Good for cache emulation problems. Known to affect following games: Fire Pro Wrestling Z."), "EmuCore/Gamefixes", "InstantDMAHack",
		false);
	DrawToggleSetting(bsi, FSUI_CSTR("OPH Flag Hack"),
		FSUI_CSTR("Known to affect following games: Bleach Blade Battlers, Growlanser II and III, Wizardry."), "EmuCore/Gamefixes",
		"OPHFlagHack", false);
	DrawToggleSetting(
		bsi, FSUI_CSTR("Emulate GIF FIFO"), FSUI_CSTR("Correct but slower. Known to affect the following games: FIFA Street 2."), "EmuCore/Gamefixes", "GIFFIFOHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("DMA Busy Hack"),
		FSUI_CSTR("Known to affect following games: Mana Khemia 1, Metal Saga, Pilot Down Behind Enemy Lines."), "EmuCore/Gamefixes",
		"DMABusyHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("Delay VIF1 Stalls"), FSUI_CSTR("For SOCOM 2 HUD and Spy Hunter loading hang."),
		"EmuCore/Gamefixes", "VIF1StallHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("Emulate VIF FIFO"),
		FSUI_CSTR("Simulate VIF1 FIFO read ahead. Known to affect following games: Test Drive Unlimited, Transformers."), "EmuCore/Gamefixes", "VIFFIFOHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("Full VU0 Synchronization"), FSUI_CSTR("Forces tight VU0 sync on every COP2 instruction."),
		"EmuCore/Gamefixes", "FullVU0SyncHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("VU I Bit Hack"),
		FSUI_CSTR("Avoids constant recompilation in some games. Known to affect the following games: Scarface The World is Yours, Crash Tag Team Racing."), "EmuCore/Gamefixes", "IbitHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("VU Add Hack"),
		FSUI_CSTR("For Tri-Ace Games: Star Ocean 3, Radiata Stories, Valkyrie Profile 2."), "EmuCore/Gamefixes",
		"VuAddSubHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("VU Overflow Hack"), FSUI_CSTR("To check for possible float overflows (Superman Returns)."),
		"EmuCore/Gamefixes", "VUOverflowHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("VU Sync"), FSUI_CSTR("Run behind. To avoid sync problems when reading or writing VU registers."),
		"EmuCore/Gamefixes", "VUSyncHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("VU XGKick Sync"), FSUI_CSTR("Use accurate timing for VU XGKicks (slower)."), "EmuCore/Gamefixes",
		"XgKickHack", false);
	DrawToggleSetting(bsi, FSUI_CSTR("Force Blit Internal FPS Detection"),
		FSUI_CSTR("Use alternative method to calculate internal FPS to avoid false readings in some games."), "EmuCore/Gamefixes",
		"BlitInternalFPSHack", false);

	EndMenuButtons();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Translation String Area
// To avoid having to type T_RANSLATE("FullscreenUI", ...) everywhere, we use the shorter macros in the internal
// header file, then preprocess and generate a bunch of noops here to define the strings. Sadly that means
// the view in Linguist is gonna suck, but you can search the file for the string for more context.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
// TRANSLATION-STRING-AREA-BEGIN

// TRANSLATION-STRING-AREA-END
#endif
