// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "BuildVersion.h"
#include "CDVD/CDVDcommon.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "GS/Renderers/Common/GSTexture.h"
#include "Achievements.h"
#include "CDVD/CDVDdiscReader.h"
#include "GameList.h"
#include "Host.h"
#include "Host/AudioStream.h"
#include "INISettingsInterface.h"
#include "ImGui/FullscreenUI.h"
#include "ImGui/FullscreenUI_Internal.h"
#include "ImGui/ImGuiFullscreen.h"
#include "ImGui/ImGuiManager.h"
#include "ImGui/ImGuiOverlays.h"
#include "Input/InputManager.h"
#include "MTGS.h"
#include "Patch.h"
#include "SupportURLs.h"
#include "USB/USB.h"
#include "VMManager.h"
#include "ps2/BiosTools.h"

#include "common/Console.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SettingsInterface.h"
#include "common/SmallString.h"
#include "common/StringUtil.h"
#include "common/Threading.h"

#include "SIO/Memcard/MemoryCardFile.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Sio.h"

#include "IconsFontAwesome.h"
#include "IconsPromptFont.h"
#include "imgui.h"
#include "imgui_internal.h"

#include "fmt/chrono.h"

//////////////////////////////////////////////////////////////////////////
// Utility
//////////////////////////////////////////////////////////////////////////

TinyString FullscreenUI::TimeToPrintableString(time_t t)
{
	struct tm lt = {};
#ifdef _MSC_VER
	localtime_s(&lt, &t);
#else
	localtime_r(&t, &lt);
#endif

	TinyString ret;
	std::strftime(ret.data(), ret.buffer_size(), "%c", &lt);
	ret.update_size();
	return ret;
}

void FullscreenUI::GetStandardSelectionFooterText(SmallStringBase& dest, bool back_instead_of_cancel)
{
	if (IsGamepadInputSource())
	{
		const bool circleOK = ImGui::GetIO().ConfigNavSwapGamepadButtons;
		ImGuiFullscreen::CreateFooterTextString(
			dest,
			std::array{
				std::make_pair(ICON_PF_DPAD_UP_DOWN, FSUI_VSTR("Change Selection")),
				std::make_pair(circleOK ? ICON_PF_BUTTON_CIRCLE : ICON_PF_BUTTON_CROSS, FSUI_VSTR("Select")),
				std::make_pair(circleOK ? ICON_PF_BUTTON_CROSS : ICON_PF_BUTTON_CIRCLE, back_instead_of_cancel ? FSUI_VSTR("Back") : FSUI_VSTR("Cancel")),
			});
	}
	else
	{
		ImGuiFullscreen::CreateFooterTextString(
			dest, std::array{
					  std::make_pair(ICON_PF_ARROW_UP ICON_PF_ARROW_DOWN, FSUI_VSTR("Change Selection")),
					  std::make_pair(ICON_PF_ENTER, FSUI_VSTR("Select")),
					  std::make_pair(ICON_PF_ESC, back_instead_of_cancel ? FSUI_VSTR("Back") : FSUI_VSTR("Cancel")),
				  });
	}
}

void FullscreenUI::SetStandardSelectionFooterText(bool back_instead_of_cancel)
{
	SmallString text;
	GetStandardSelectionFooterText(text, back_instead_of_cancel);
	ImGuiFullscreen::SetFullscreenFooterText(text);
}

void ImGuiFullscreen::GetChoiceDialogHelpText(SmallStringBase& dest)
{
	FullscreenUI::GetStandardSelectionFooterText(dest, false);
}

void ImGuiFullscreen::GetFileSelectorHelpText(SmallStringBase& dest)
{
	if (IsGamepadInputSource())
	{
		const bool circleOK = ImGui::GetIO().ConfigNavSwapGamepadButtons;
		const bool swapNorthWest = ImGuiManager::IsGamepadNorthWestSwapped();
		ImGuiFullscreen::CreateFooterTextString(
			dest, std::array{
					  std::make_pair(ICON_PF_DPAD_UP_DOWN, FSUI_VSTR("Change Selection")),
					  std::make_pair(swapNorthWest ? ICON_PF_BUTTON_SQUARE : ICON_PF_BUTTON_TRIANGLE, FSUI_VSTR("Parent Directory")),
					  std::make_pair(circleOK ? ICON_PF_BUTTON_CIRCLE : ICON_PF_BUTTON_CROSS, FSUI_VSTR("Select")),
					  std::make_pair(circleOK ? ICON_PF_BUTTON_CROSS : ICON_PF_BUTTON_CIRCLE, FSUI_VSTR("Cancel")),
				  });
	}
	else
	{
		ImGuiFullscreen::CreateFooterTextString(
			dest,
			std::array{
				std::make_pair(ICON_PF_ARROW_UP ICON_PF_ARROW_DOWN, FSUI_VSTR("Change Selection")),
				std::make_pair(ICON_PF_BACKSPACE, FSUI_VSTR("Parent Directory")),
				std::make_pair(ICON_PF_ENTER, FSUI_VSTR("Select")),
				std::make_pair(ICON_PF_ESC, FSUI_VSTR("Cancel")),
			});
	}
}

void ImGuiFullscreen::GetInputDialogHelpText(SmallStringBase& dest)
{
	if (IsGamepadInputSource())
	{
		const bool circleOK = ImGui::GetIO().ConfigNavSwapGamepadButtons;
		CreateFooterTextString(dest, std::array{
										 std::make_pair(ICON_PF_KEYBOARD, FSUI_VSTR("Enter Value")),
										 std::make_pair(circleOK ? ICON_PF_BUTTON_CIRCLE : ICON_PF_BUTTON_CROSS, FSUI_VSTR("Select")),
										 std::make_pair(circleOK ? ICON_PF_BUTTON_CROSS : ICON_PF_BUTTON_CIRCLE, FSUI_VSTR("Cancel")),
									 });
	}
	else
	{
		CreateFooterTextString(dest, std::array{
										 std::make_pair(ICON_PF_KEYBOARD, FSUI_VSTR("Enter Value")),
										 std::make_pair(ICON_PF_ENTER, FSUI_VSTR("Select")),
										 std::make_pair(ICON_PF_ESC, FSUI_VSTR("Cancel")),
									 });
	}
}

void FullscreenUI::ApplyLayoutSettings(const SettingsInterface* bsi)
{
	ImGuiIO& io = ImGui::GetIO();
	SmallString swap_mode;
	if (bsi)
		swap_mode = bsi->GetSmallStringValue("UI", "SwapOKFullscreenUI", "auto");
	else
		swap_mode = Host::GetBaseSmallStringSettingValue("UI", "SwapOKFullscreenUI", "auto");

	// Check Nintendo Setting
	SmallString sdl2_nintendo_mode;
	if (bsi)
		sdl2_nintendo_mode = bsi->GetSmallStringValue("UI", "SDL2NintendoLayout", "false");
	else
		sdl2_nintendo_mode = Host::GetBaseSmallStringSettingValue("UI", "SDL2NintendoLayout", "false");

	const InputLayout layout = ImGuiFullscreen::GetGamepadLayout();

	if ((sdl2_nintendo_mode == "true" || sdl2_nintendo_mode == "auto") && layout == InputLayout::Nintendo)
	{
		// Apply
		ImGuiManager::SwapGamepadNorthWest(true);

		// Check swap_mode if A/B should also be swapped
		if (swap_mode == "auto")
		{
			io.ConfigNavSwapGamepadButtons = true;
			return;
		}
	}
	else
		ImGuiManager::SwapGamepadNorthWest(false);

	if (swap_mode == "true")
		io.ConfigNavSwapGamepadButtons = true;
	else if (swap_mode == "false")
		io.ConfigNavSwapGamepadButtons = false;
	else if (swap_mode == "auto")
	{
		// Check gamepad
		if (layout == InputLayout::Nintendo)
		{
			io.ConfigNavSwapGamepadButtons = true;
			return;
		}

		// Check language
		if (Host::LocaleCircleConfirm())
		{
			io.ConfigNavSwapGamepadButtons = true;
			return;
		}

		// Check BIOS
		SmallString bios_selection;
		if (bsi)
			bios_selection = bsi->GetSmallStringValue("Filenames", "BIOS", "");
		else
			bios_selection = Host::GetBaseSmallStringSettingValue("Filenames", "BIOS", "");

		if (bios_selection != "")
		{
			u32 bios_version, bios_region;
			std::string bios_description, bios_zone;
			if (IsBIOS(Path::Combine(EmuFolders::Bios, bios_selection).c_str(), bios_version, bios_description, bios_region, bios_zone))
			{
				// Japan, Asia, China
				if (bios_region == 0 || bios_region == 4 || bios_region == 6)
				{
					io.ConfigNavSwapGamepadButtons = true;
					return;
				}
			}
		}

		// X is confirm
		io.ConfigNavSwapGamepadButtons = false;
		return;
	}
	// Invalid setting
	else
		io.ConfigNavSwapGamepadButtons = false;
}

void FullscreenUI::LocaleChanged()
{
	ApplyLayoutSettings();
}

void FullscreenUI::GamepadLayoutChanged()
{
	ApplyLayoutSettings();
}

// When drawing an svg to a non-integer size, we get a padded texture.
// This function crops off this padding by setting the image UV for the draw.
// We currently only use integer sizes for images, but I wrote this before checking that.
void FullscreenUI::DrawSvgTexture(GSTexture* padded_texture, ImVec2 unpadded_size)
{
	if (padded_texture != GetPlaceholderTexture().get())
	{
		const ImVec2 padded_size(padded_texture->GetWidth(), padded_texture->GetHeight());
		const ImVec2 uv1 = unpadded_size / padded_size;
		ImGui::Image(reinterpret_cast<ImTextureID>(padded_texture->GetNativeHandle()), unpadded_size, ImVec2(0.0f, 0.0f), uv1);
	}
	else
	{
		// Placeholder is a png file and should be scaled by ImGui
		ImGui::Image(reinterpret_cast<ImTextureID>(padded_texture->GetNativeHandle()), unpadded_size);
	}
}

void FullscreenUI::DrawCachedSvgTexture(const std::string& path, ImVec2 size, SvgScaling mode)
{
	DrawSvgTexture(GetCachedSvgTexture(path, size, mode), size);
}

void FullscreenUI::DrawCachedSvgTextureAsync(const std::string& path, ImVec2 size, SvgScaling mode)
{
	DrawSvgTexture(GetCachedSvgTextureAsync(path, size, mode), size);
}

// p_unpadded_max should be equal to p_min + unpadded_size
void FullscreenUI::DrawListSvgTexture(ImDrawList* drawList, GSTexture* padded_texture, const ImVec2& p_min, const ImVec2& p_unpadded_max)
{
	const ImVec2 unpadded_size = p_unpadded_max - p_min;
	if (padded_texture != GetPlaceholderTexture().get())
	{
		const ImVec2 padded_size(padded_texture->GetWidth(), padded_texture->GetHeight());
		const ImVec2 uv1 = unpadded_size / padded_size;
		drawList->AddImage(reinterpret_cast<ImTextureID>(padded_texture->GetNativeHandle()), p_min, p_unpadded_max, ImVec2(0.0f, 0.0f), uv1);
	}
	else
	{
		// Placeholder is a png file and should be scaled by ImGui
		drawList->AddImage(reinterpret_cast<ImTextureID>(padded_texture->GetNativeHandle()), p_min, p_unpadded_max);
	}
}

//////////////////////////////////////////////////////////////////////////
// Main
//////////////////////////////////////////////////////////////////////////

bool FullscreenUI::Initialize()
{
	if (s_initialized)
		return true;

	if (s_tried_to_initialize)
		return false;

	ImGuiFullscreen::SetTheme(Host::GetBaseStringSettingValue("UI", "FullscreenUITheme", "Dark"));
	ImGuiFullscreen::UpdateLayoutScale();
	ImGuiFullscreen::UpdateFontScale();
	ApplyLayoutSettings();

	if (!ImGuiFullscreen::Initialize("fullscreenui/placeholder.png") || !LoadResources())
	{
		DestroyResources();
		ImGuiFullscreen::Shutdown(true);
		s_tried_to_initialize = true;
		return false;
	}

	s_initialized = true;
	s_hotkey_list_cache = InputManager::GetHotkeyList();
	MTGS::SetRunIdle(true);

	LoadCustomBackground();

	if (VMManager::HasValidVM())
	{
		UpdateGameDetails(VMManager::GetDiscPath(), VMManager::GetDiscSerial(), VMManager::GetTitle(true), VMManager::GetDiscCRC(),
			VMManager::GetCurrentCRC());
	}
	else
	{
		const bool open_main_window = s_current_main_window == MainWindowType::None;
		if (open_main_window)
			ReturnToMainWindow();
	}

	ForceKeyNavEnabled();
	return true;
}

bool FullscreenUI::IsInitialized()
{
	return s_initialized;
}

void FullscreenUI::ReloadSvgResources()
{
	LoadSvgResources();
}

bool FullscreenUI::HasActiveWindow()
{
	return s_initialized && (s_current_main_window != MainWindowType::None || AreAnyDialogsOpen());
}

bool FullscreenUI::AreAnyDialogsOpen()
{
	return (s_save_state_selector_open || s_about_window_open ||
			s_input_binding_type != InputBindingInfo::Type::Unknown || ImGuiFullscreen::IsChoiceDialogOpen() ||
			ImGuiFullscreen::IsFileSelectorOpen());
}

void FullscreenUI::CheckForConfigChanges(const Pcsx2Config& old_config)
{
	if (!IsInitialized())
		return;

	ImGuiFullscreen::SetTheme(Host::GetBaseStringSettingValue("UI", "FullscreenUITheme", "Dark"));

	MTGS::RunOnGSThread([]() {
		LoadCustomBackground();
	});

	// If achievements got disabled, we might have the menu open...
	// That means we're going to be reaching achievement state.
	if (old_config.Achievements.Enabled && !EmuConfig.Achievements.Enabled)
	{
		// So, wait just in case.
		MTGS::RunOnGSThread([]() {
			if (s_current_main_window == MainWindowType::Achievements || s_current_main_window == MainWindowType::Leaderboards)
			{
				ReturnToPreviousWindow();
			}
		});
		MTGS::WaitGS(false, false, false);
	}

	if (old_config.FullpathToBios() != EmuConfig.FullpathToBios())
	{
		MTGS::RunOnGSThread([]() {
			ApplyLayoutSettings();
		});
	}
}

void FullscreenUI::OnVMStarted()
{
	if (!IsInitialized())
		return;

	MTGS::RunOnGSThread([]() {
		if (!IsInitialized())
			return;

		s_current_main_window = MainWindowType::None;
		QueueResetFocus(FocusResetType::WindowChanged);
	});
}

void FullscreenUI::OnVMDestroyed()
{
	if (!IsInitialized())
		return;

	MTGS::RunOnGSThread([]() {
		if (!IsInitialized())
			return;

		s_pause_menu_was_open = false;
		s_was_paused_on_quick_menu_open = false;
		s_current_pause_submenu = PauseSubMenu::None;
		ReturnToMainWindow();
	});
}

void FullscreenUI::GameChanged(std::string path, std::string serial, std::string title, u32 disc_crc, u32 crc)
{
	if (!IsInitialized())
		return;

	MTGS::RunOnGSThread([path = std::move(path), serial = std::move(serial), title = std::move(title), disc_crc, crc]() {
		if (!IsInitialized())
			return;

		UpdateGameDetails(std::move(path), std::move(serial), std::move(title), disc_crc, crc);
	});
}

void FullscreenUI::UpdateGameDetails(std::string path, std::string serial, std::string title, u32 disc_crc, u32 crc)
{
	if (!serial.empty())
		s_current_game_subtitle = fmt::format("{} / {:08X}", serial, crc);
	else
		s_current_game_subtitle = {};

	s_current_game_title = std::move(title);
	s_current_disc_serial = std::move(serial);
	s_current_disc_path = std::move(path);
	s_current_disc_crc = disc_crc;
}

void FullscreenUI::PauseForMenuOpen(bool set_pause_menu_open)
{
	s_was_paused_on_quick_menu_open = (VMManager::GetState() == VMState::Paused);
	if (Host::GetBoolSettingValue("UI", "PauseOnMenu", true) && !s_was_paused_on_quick_menu_open)
		Host::RunOnCPUThread([]() { VMManager::SetPaused(true); });

	s_pause_menu_was_open |= set_pause_menu_open;
}

void FullscreenUI::OpenPauseMenu()
{
	if (!VMManager::HasValidVM())
		return;

	if (SaveStateSelectorUI::IsOpen())
	{
		SaveStateSelectorUI::Close();
		return;
	}

	MTGS::RunOnGSThread([]() {
		if (!ImGuiManager::InitializeFullscreenUI() || s_current_main_window != MainWindowType::None)
			return;

		PauseForMenuOpen(true);
		ForceKeyNavEnabled();
		s_current_main_window = MainWindowType::PauseMenu;
		s_current_pause_submenu = PauseSubMenu::None;
		QueueResetFocus(FocusResetType::WindowChanged);
	});
}

void FullscreenUI::ClosePauseMenu()
{
	if (!IsInitialized() || !VMManager::HasValidVM())
		return;

	if (VMManager::GetState() == VMState::Paused && !s_was_paused_on_quick_menu_open)
		Host::RunOnCPUThread([]() { VMManager::SetPaused(false); });

	s_current_main_window = MainWindowType::None;
	s_current_pause_submenu = PauseSubMenu::None;
	s_pause_menu_was_open = false;
	QueueResetFocus(FocusResetType::WindowChanged);
}

void FullscreenUI::OpenPauseSubMenu(PauseSubMenu submenu)
{
	s_current_main_window = MainWindowType::PauseMenu;
	s_current_pause_submenu = submenu;
	QueueResetFocus(FocusResetType::WindowChanged);
}

void FullscreenUI::Shutdown(bool clear_state)
{
	if (clear_state)
	{
		CancelAllHddOperations();
		CloseSaveStateSelector();
		s_cover_image_map.clear();
		s_game_list_sorted_entries = {};
		s_game_list_directories_cache = {};
		s_game_cheat_unlabelled_count = 0;
		s_enabled_game_cheat_cache = {};
		s_game_cheats_list = {};
		s_enabled_game_patch_cache = {};
		s_game_patch_list = {};
		s_graphics_adapter_list_cache = {};
		s_current_game_title = {};
		s_current_game_subtitle = {};
		s_current_disc_serial = {};
		s_current_disc_path = {};
		s_current_disc_crc = 0;

		s_current_main_window = MainWindowType::None;
		s_current_pause_submenu = PauseSubMenu::None;
		s_pause_menu_was_open = false;
		s_was_paused_on_quick_menu_open = false;
		s_about_window_open = false;
	}
	s_hotkey_list_cache = {};

	s_custom_background_texture.reset();
	s_custom_background_path.clear();
	s_custom_background_enabled = false;

	DestroyResources();
	ImGuiFullscreen::Shutdown(clear_state);
	s_initialized = false;
	s_tried_to_initialize = false;
}

void FullscreenUI::Render()
{
	if (!s_initialized)
		return;

	// see if background setting changed
	static std::string s_last_background_path;
	std::string current_path = Host::GetBaseStringSettingValue("UI", "FSUIBackgroundPath");
	if (s_last_background_path != current_path)
	{
		s_last_background_path = current_path;
		LoadCustomBackground();
	}

	for (std::unique_ptr<GSTexture>& tex : s_cleanup_textures)
		g_gs_device->Recycle(tex.release());
	s_cleanup_textures.clear();
	ImGuiFullscreen::UploadAsyncTextures();

	ImGuiFullscreen::BeginLayout();

	const bool should_draw_background = (s_current_main_window == MainWindowType::Landing ||
		s_current_main_window == MainWindowType::StartGame ||
		s_current_main_window == MainWindowType::Exit ||
		s_current_main_window == MainWindowType::GameList ||
		s_current_main_window == MainWindowType::GameListSettings ||
		s_current_main_window == MainWindowType::Settings) &&
			!VMManager::HasValidVM() && s_custom_background_enabled && s_custom_background_texture;

	ImVec4 original_background_color;
	if (should_draw_background)
	{
		original_background_color = ImGuiFullscreen::UIBackgroundColor;
		DrawCustomBackground();
	}

	// Primed achievements must come first, because we don't want the pause screen to be behind them.
	if (s_current_main_window == MainWindowType::None && (EmuConfig.Achievements.Overlays || EmuConfig.Achievements.LBOverlays))
		Achievements::DrawGameOverlays();

	switch (s_current_main_window)
	{
		case MainWindowType::Landing:
			DrawLandingWindow();
			break;
		case MainWindowType::StartGame:
			DrawStartGameWindow();
			break;
		case MainWindowType::Exit:
			DrawExitWindow();
			break;
		case MainWindowType::GameList:
			DrawGameListWindow();
			break;
		case MainWindowType::GameListSettings:
			DrawGameListSettingsWindow();
			break;
		case MainWindowType::Settings:
			DrawSettingsWindow();
			break;
		case MainWindowType::PauseMenu:
			DrawPauseMenu(s_current_main_window);
			break;
		case MainWindowType::Achievements:
			Achievements::DrawAchievementsWindow();
			break;
		case MainWindowType::Leaderboards:
			Achievements::DrawLeaderboardsWindow();
			break;
		default:
			break;
	}

	if (s_save_state_selector_open)
	{
		if (s_save_state_selector_resuming)
			DrawResumeStateSelector();
		else
			DrawSaveStateSelector(s_save_state_selector_loading);
	}

	if (s_about_window_open)
		DrawAboutWindow();

	if (s_achievements_login_open)
		DrawAchievementsLoginWindow();

	if (s_input_binding_type != InputBindingInfo::Type::Unknown)
		DrawInputBindingWindow();

	ImGuiFullscreen::EndLayout();

	if (s_settings_changed.load(std::memory_order_relaxed))
	{
		Host::CommitBaseSettingChanges();
		Host::RunOnCPUThread([]() { VMManager::ApplySettings(); });
		s_settings_changed.store(false, std::memory_order_release);
	}
	if (s_game_settings_changed.load(std::memory_order_relaxed))
	{
		if (s_game_settings_interface)
		{
			Error error;
			s_game_settings_interface->RemoveEmptySections();

			if (s_game_settings_interface->IsEmpty())
			{
				if (FileSystem::FileExists(s_game_settings_interface->GetFileName().c_str()) &&
					!FileSystem::DeleteFilePath(s_game_settings_interface->GetFileName().c_str(), &error))
				{
					ImGuiFullscreen::OpenInfoMessageDialog(
						FSUI_STR("Error"), fmt::format(FSUI_FSTR("An error occurred while deleting empty game settings:\n{}"),
											   error.GetDescription()));
				}
			}
			else
			{
				if (!s_game_settings_interface->Save(&error))
				{
					ImGuiFullscreen::OpenInfoMessageDialog(
						FSUI_STR("Error"),
						fmt::format(FSUI_FSTR("An error occurred while saving game settings:\n{}"), error.GetDescription()));
				}
			}

			if (VMManager::HasValidVM())
				Host::RunOnCPUThread([]() { VMManager::ReloadGameSettings(); });
		}
		s_game_settings_changed.store(false, std::memory_order_release);
	}

	if (should_draw_background)
		ImGuiFullscreen::UIBackgroundColor = original_background_color;

	ImGuiFullscreen::ResetCloseMenuIfNeeded();
}

void FullscreenUI::InvalidateCoverCache()
{
	if (!IsInitialized())
		return;

	MTGS::RunOnGSThread([]() { s_cover_image_map.clear(); });
}

void FullscreenUI::ReturnToPreviousWindow()
{
	if (VMManager::HasValidVM() && s_pause_menu_was_open)
	{
		s_current_main_window = MainWindowType::PauseMenu;
		QueueResetFocus(FocusResetType::WindowChanged);
	}
	else
	{
		ReturnToMainWindow();
	}
}

void FullscreenUI::ReturnToMainWindow()
{
	ClosePauseMenu();

	if (VMManager::HasValidVM())
	{
		s_current_main_window = MainWindowType::None;
		return;
	}

	if (ShouldDefaultToGameList())
		SwitchToGameList();
	else
		SwitchToLanding();
}

bool FullscreenUI::LoadResources()
{
	return LoadSvgResources();
}

bool FullscreenUI::LoadSvgResources()
{
	s_banner_texture = LoadSvgTexture("icons/AppBanner.svg", LayoutScale(500.0f, 76.0f), SvgScaling::Fit);

	for (u32 i = static_cast<u32>(GameDatabaseSchema::Compatibility::Nothing);
		 i <= static_cast<u32>(GameDatabaseSchema::Compatibility::Perfect); i++)
	{
		s_game_compatibility_textures[i - 1] = LoadSvgTexture(fmt::format("icons/star-{}.svg", i - 1).c_str(), LayoutScale(64.0f, 16.0f), SvgScaling::Fit);
	}

	return true;
}

void FullscreenUI::DestroyResources()
{
	s_banner_texture.reset();
	for (auto& tex : s_game_compatibility_textures)
		tex.reset();
	for (auto& tex : s_cleanup_textures)
		g_gs_device->Recycle(tex.release());
}

//////////////////////////////////////////////////////////////////////////
// Utility
//////////////////////////////////////////////////////////////////////////

ImGuiFullscreen::FileSelectorFilters FullscreenUI::GetOpenFileFilters()
{
	return {"*.bin", "*.iso", "*.cue", "*.mdf", "*.chd", "*.cso", "*.zso", "*.gz", "*.elf", "*.irx", "*.gs", "*.gs.xz", "*.gs.zst", "*.dump"};
}

ImGuiFullscreen::FileSelectorFilters FullscreenUI::GetDiscImageFilters()
{
	return {"*.bin", "*.iso", "*.cue", "*.mdf", "*.chd", "*.cso", "*.zso", "*.gz"};
}

ImGuiFullscreen::FileSelectorFilters FullscreenUI::GetAudioFileFilters()
{
	return {"*.wav"};
}

ImGuiFullscreen::FileSelectorFilters FullscreenUI::GetImageFileFilters()
{
	return {"*.png", "*.jpg", "*.jpeg", "*.bmp"};
}

void FullscreenUI::DoVMInitialize(const VMBootParameters& boot_params, bool switch_to_landing_on_failure)
{
	auto hardcore_disable_callback = [switch_to_landing_on_failure](
											   std::string reason, VMBootRestartCallback restart_callback) {
		MTGS::RunOnGSThread([reason = std::move(reason),
								restart_callback = std::move(restart_callback),
								switch_to_landing_on_failure]() {
			const auto callback = [restart_callback = std::move(restart_callback),
									  switch_to_landing_on_failure](bool confirmed) {
				if (confirmed)
					Host::RunOnCPUThread(restart_callback);
				else if (switch_to_landing_on_failure)
					SwitchToLanding();
			};

			ImGuiFullscreen::OpenConfirmMessageDialog(
				Achievements::GetHardcoreModeDisableTitle(),
				Achievements::GetHardcoreModeDisableText(reason.c_str()),
				std::move(callback), true,
				fmt::format(ICON_FA_CHECK " {}", TRANSLATE_SV("Achievements", "Yes")),
				fmt::format(ICON_FA_XMARK " {}", TRANSLATE_SV("Achievements", "No")));
		});
	};

	auto done_callback = [switch_to_landing_on_failure](VMBootResult result, const Error& error) {
		if (result != VMBootResult::StartupSuccess)
		{
			ImGuiFullscreen::OpenInfoMessageDialog(
				FSUI_ICONSTR(ICON_FA_TRIANGLE_EXCLAMATION, "Startup Error"), error.GetDescription());

			if (switch_to_landing_on_failure)
				MTGS::RunOnGSThread(SwitchToLanding);

			return;
		}

		VMManager::SetState(VMState::Running);
	};

	VMManager::InitializeAsync(boot_params, std::move(hardcore_disable_callback), std::move(done_callback));
}

void FullscreenUI::DoStartPath(const std::string& path, std::optional<s32> state_index, std::optional<bool> fast_boot)
{
	VMBootParameters params;
	params.filename = path;
	params.state_index = state_index;
	params.fast_boot = fast_boot;

	// switch to nothing, we'll get brought back if init fails
	Host::RunOnCPUThread([params = std::move(params)]() {
		DoVMInitialize(std::move(params), false);
	});
}

void FullscreenUI::DoStartFile()
{
	auto callback = [](const std::string& path) {
		if (!path.empty())
			DoStartPath(path);

		CloseFileSelector();
	};

	OpenFileSelector(FSUI_ICONSTR(ICON_FA_FOLDER_OPEN, "Select Disc Image"), false, std::move(callback), GetOpenFileFilters());
}

void FullscreenUI::DoStartBIOS()
{
	Host::RunOnCPUThread([]() {
		if (VMManager::HasValidVM())
			return;

		VMBootParameters params;
		DoVMInitialize(std::move(params), true);
	});

	// switch to nothing, we'll get brought back if init fails
	s_current_main_window = MainWindowType::None;
}

void FullscreenUI::DoStartDisc(const std::string& drive)
{
	Host::RunOnCPUThread([drive]() {
		if (VMManager::HasValidVM())
			return;

		VMBootParameters params;
		params.filename = std::move(drive);
		params.source_type = CDVD_SourceType::Disc;
		DoVMInitialize(std::move(params), true);
	});
}

void FullscreenUI::DoStartDisc()
{
	std::vector<std::string> devices(GetOpticalDriveList());
	if (devices.empty())
	{
		ShowToast(std::string(), FSUI_STR("Could not find any CD/DVD-ROM devices. Please ensure you have a drive connected and sufficient "
										  "permissions to access it."));
		return;
	}

	// if there's only one, select it automatically
	if (devices.size() == 1)
	{
		DoStartDisc(devices.front());
		return;
	}

	ImGuiFullscreen::ChoiceDialogOptions options;
	for (std::string& drive : devices)
		options.emplace_back(std::move(drive), false);
	OpenChoiceDialog(
		FSUI_ICONSTR(ICON_FA_COMPACT_DISC, "Select Disc Drive"), false, std::move(options), [](s32, const std::string& path, bool) {
			DoStartDisc(path);
			CloseChoiceDialog();
		});
}

void FullscreenUI::DoToggleFrameLimit()
{
	Host::RunOnCPUThread([]() {
		if (!VMManager::HasValidVM())
			return;

		VMManager::SetLimiterMode(
			(VMManager::GetLimiterMode() != LimiterModeType::Unlimited) ? LimiterModeType::Unlimited : LimiterModeType::Nominal);
	});
}

void FullscreenUI::DoToggleSoftwareRenderer()
{
	Host::RunOnCPUThread([]() {
		if (!VMManager::HasValidVM())
			return;

		MTGS::ToggleSoftwareRendering();
	});
}

void FullscreenUI::RequestShutdown(bool save_state)
{
	ConfirmShutdownIfMemcardBusy([save_state](bool result) {
		if (result)
			DoShutdown(save_state);

		ClosePauseMenu();
	});
}

void FullscreenUI::DoShutdown(bool save_state)
{
	Host::RunOnCPUThread([save_state]() { Host::RequestVMShutdown(false, save_state, save_state); });
}

void FullscreenUI::RequestReset()
{
	ConfirmShutdownIfMemcardBusy([](bool result) {
		if (result)
			DoReset();

		ClosePauseMenu();
	});
}

void FullscreenUI::DoReset()
{
	Host::RunOnCPUThread([]() {
		if (!VMManager::HasValidVM())
			return;

		VMManager::Reset();
	});
}

void FullscreenUI::DoChangeDiscFromFile()
{
	auto callback = [](const std::string& path) {
		if (!path.empty())
		{
			if (!VMManager::IsDiscFileName(path))
			{
				ShowToast({}, fmt::format(FSUI_FSTR("{} is not a valid disc image."), Path::GetFileName(path)));
			}
			else
			{
				Host::RunOnCPUThread([path]() { VMManager::ChangeDisc(CDVD_SourceType::Iso, std::move(path)); });
			}
		}

		CloseFileSelector();
		ReturnToPreviousWindow();
		ClosePauseMenu();
	};

	OpenFileSelector(FSUI_ICONSTR(ICON_FA_COMPACT_DISC, "Select Disc Image"), false, std::move(callback), GetDiscImageFilters(),
		std::string(Path::GetDirectory(s_current_disc_path)));
}

void FullscreenUI::RequestChangeDisc()
{
	ConfirmShutdownIfMemcardBusy([](bool result) {
		if (result)
			DoChangeDiscFromFile();
		else
			ClosePauseMenu();
	});
}

void FullscreenUI::DoRequestExit()
{
	Host::RunOnCPUThread([]() { Host::RequestExitApplication(true); });
}

void FullscreenUI::DoDesktopMode()
{
	Host::RunOnCPUThread([]() { Host::RequestExitBigPicture(); });
}

void FullscreenUI::DoToggleFullscreen()
{
	Host::RunOnCPUThread([]() { Host::SetFullscreen(!Host::IsFullscreen()); });
}

void FullscreenUI::ConfirmShutdownIfMemcardBusy(std::function<void(bool)> callback)
{
	if (!MemcardBusy::IsBusy())
	{
		callback(true);
		return;
	}

	OpenConfirmMessageDialog(FSUI_ICONSTR(ICON_PF_MEMORY_CARD, "WARNING: Memory Card Busy"),
		FSUI_STR("Your memory card is still saving data.\n\n"
			"WARNING: Shutting down now can IRREVERSIBLY CORRUPT YOUR MEMORY CARD.\n\n"
			"You are strongly advised to select 'No' and let the save finish.\n\n"
			"Do you want to shutdown anyway and IRREVERSIBLY CORRUPT YOUR MEMORY CARD?"),
		std::move(callback), false);
}

bool FullscreenUI::ShouldDefaultToGameList()
{
	return Host::GetBaseBoolSettingValue("UI", "FullscreenUIDefaultToGameList", false);
}

//////////////////////////////////////////////////////////////////////////
// Custom Background
//////////////////////////////////////////////////////////////////////////

void FullscreenUI::LoadCustomBackground()
{
	std::string path = Host::GetBaseStringSettingValue("UI", "FSUIBackgroundPath");

	if (path.empty())
	{
		s_custom_background_texture.reset();
		s_custom_background_path.clear();
		s_custom_background_enabled = false;
		return;
	}

	if (s_custom_background_path == path && s_custom_background_texture)
	{
		s_custom_background_enabled = true;
		return;
	}

	if (!Path::IsAbsolute(path))
		path = Path::Combine(EmuFolders::DataRoot, path);

	if (!FileSystem::FileExists(path.c_str()))
	{
		Console.Warning("Custom background file not found: %s", path.c_str());
		s_custom_background_texture.reset();
		s_custom_background_path.clear();
		s_custom_background_enabled = false;
		return;
	}

	if (StringUtil::EndsWithNoCase(path, ".gif"))
	{
		Console.Warning("GIF files aren't supported as backgrounds: %s", path.c_str());
		s_custom_background_texture.reset();
		s_custom_background_path.clear();
		s_custom_background_enabled = false;
		return;
	}

	if (StringUtil::EndsWithNoCase(path, ".webp"))
	{
		Console.Warning("WebP files aren't supported as backgrounds: %s", path.c_str());
		s_custom_background_texture.reset();
		s_custom_background_path.clear();
		s_custom_background_enabled = false;
		return;
	}

	s_custom_background_texture = LoadTexture(path.c_str());
	if (s_custom_background_texture)
	{
		s_custom_background_path = std::move(path);
		s_custom_background_enabled = true;
	}
	else
	{
		Console.Error("Failed to load custom background: %s", path.c_str());
		s_custom_background_path.clear();
		s_custom_background_enabled = false;
	}
}

void FullscreenUI::DrawCustomBackground()
{
	if (!s_custom_background_enabled || !s_custom_background_texture)
		return;

	const ImGuiIO& io = ImGui::GetIO();
	const ImVec2 display_size = io.DisplaySize;

	const u8 alpha = static_cast<u8>(Host::GetBaseFloatSettingValue("UI", "FSUIBackgroundOpacity", 100.0f) * 2.55f);
	const std::string mode = Host::GetBaseStringSettingValue("UI", "FSUIBackgroundMode", "fit");

	const float tex_width = static_cast<float>(s_custom_background_texture->GetWidth());
	const float tex_height = static_cast<float>(s_custom_background_texture->GetHeight());

	// Override the UIBackgroundColor that windows use
	// We need to make windows transparent so our background image shows through
	const ImVec4 transparent_bg = ImVec4(UIBackgroundColor.x, UIBackgroundColor.y, UIBackgroundColor.z, 0.0f);
	ImGuiFullscreen::UIBackgroundColor = transparent_bg;

	ImDrawList* bg_draw_list = ImGui::GetBackgroundDrawList();
	const ImU32 col = IM_COL32(255, 255, 255, alpha);
	const ImTextureID tex_id = reinterpret_cast<ImTextureID>(s_custom_background_texture->GetNativeHandle());

	if (mode == "stretch")
	{
		// stretch to fill entire display (ignores aspect ratio)
		bg_draw_list->AddImage(tex_id, ImVec2(0.0f, 0.0f), display_size, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), col);
	}
	else if (mode == "fill")
	{
		// Fill display while preserving aspect ratio (could crop edges)
		const float display_aspect = display_size.x / display_size.y;
		const float tex_aspect = tex_width / tex_height;

		float scale;
		if (tex_aspect > display_aspect)
		{
			// Image is wider scale to height and crop sides
			scale = display_size.y / tex_height;
		}
		else
		{
			// Image is taller scale to width and crop top/bottom
			scale = display_size.x / tex_width;
		}

		const float scaled_width = tex_width * scale;
		const float scaled_height = tex_height * scale;
		const float offset_x = (display_size.x - scaled_width) * 0.5f;
		const float offset_y = (display_size.y - scaled_height) * 0.5f;

		bg_draw_list->AddImage(tex_id,
			ImVec2(offset_x, offset_y),
			ImVec2(offset_x + scaled_width, offset_y + scaled_height),
			ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), col);
	}
	else if (mode == "center")
	{
		// Center image at original size
		const float offset_x = (display_size.x - tex_width) * 0.5f;
		const float offset_y = (display_size.y - tex_height) * 0.5f;

		bg_draw_list->AddImage(tex_id,
			ImVec2(offset_x, offset_y),
			ImVec2(offset_x + tex_width, offset_y + tex_height),
			ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), col);
	}
	else if (mode == "tile")
	{
		// Tile image across entire display
		// If the image is extremely small, this approach can generate millions of quads
		// and overflow the backend stream buffer (e.g. Vulkan assertion in VKStreamBuffer).
		// Since we cannot switch ImGui's sampler to wrap (yet), clamp the maximum number of quads
		constexpr int MAX_TILE_QUADS = 16384;

		float tile_width = tex_width;
		float tile_height = tex_height;
		int tiles_x = static_cast<int>(std::ceil(display_size.x / tile_width));
		int tiles_y = static_cast<int>(std::ceil(display_size.y / tile_height));

		const int total_tiles = tiles_x * tiles_y;
		if (total_tiles > MAX_TILE_QUADS)
		{
			const float scale = std::sqrt(static_cast<float>(total_tiles) / static_cast<float>(MAX_TILE_QUADS));
			tile_width *= scale;
			tile_height *= scale;
			tiles_x = static_cast<int>(std::ceil(display_size.x / tile_width));
			tiles_y = static_cast<int>(std::ceil(display_size.y / tile_height));
		}

		for (int y = 0; y < tiles_y; y++)
		{
			for (int x = 0; x < tiles_x; x++)
			{
				const float tile_x = static_cast<float>(x) * tile_width;
				const float tile_y = static_cast<float>(y) * tile_height;
				const float tile_max_x = std::min(tile_x + tile_width, display_size.x);
				const float tile_max_y = std::min(tile_y + tile_height, display_size.y);

				// get uvs for partial tiles at edges
				const float uv_max_x = (tile_max_x - tile_x) / tile_width;
				const float uv_max_y = (tile_max_y - tile_y) / tile_height;

				bg_draw_list->AddImage(tex_id,
					ImVec2(tile_x, tile_y),
					ImVec2(tile_max_x, tile_max_y),
					ImVec2(0.0f, 0.0f), ImVec2(uv_max_x, uv_max_y), col);
			}
		}
	}
	else // "fit" or default
	{
		// Fit on screen while preserving aspect ratio (no cropping)
		const float display_aspect = display_size.x / display_size.y;
		const float tex_aspect = tex_width / tex_height;

		float scale;
		if (tex_aspect > display_aspect)
		{
			// Image is wider than display
			scale = display_size.x / tex_width;
		}
		else
		{
			// Image is taller than display
			scale = display_size.y / tex_height;
		}

		const float scaled_width = tex_width * scale;
		const float scaled_height = tex_height * scale;
		const float offset_x = (display_size.x - scaled_width) * 0.5f;
		const float offset_y = (display_size.y - scaled_height) * 0.5f;

		bg_draw_list->AddImage(tex_id,
			ImVec2(offset_x, offset_y),
			ImVec2(offset_x + scaled_width, offset_y + scaled_height),
			ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), col);
	}
}

//////////////////////////////////////////////////////////////////////////
// Landing Window
//////////////////////////////////////////////////////////////////////////

void FullscreenUI::SwitchToLanding()
{
	s_current_main_window = MainWindowType::Landing;
	QueueResetFocus(FocusResetType::WindowChanged);
}

void FullscreenUI::DrawLandingTemplate(ImVec2* menu_pos, ImVec2* menu_size)
{
	const ImGuiIO& io = ImGui::GetIO();
	const ImVec2 heading_size =
		ImVec2(io.DisplaySize.x, LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY) +
									 (LayoutScale(LAYOUT_MENU_BUTTON_Y_PADDING) * 2.0f) + LayoutScale(2.0f));
	*menu_pos = ImVec2(0.0f, heading_size.y);
	*menu_size = ImVec2(io.DisplaySize.x, io.DisplaySize.y - heading_size.y - LayoutScale(LAYOUT_FOOTER_HEIGHT));

	if (BeginFullscreenWindow(ImVec2(0.0f, 0.0f), heading_size, "landing_heading", UIPrimaryColor))
	{
		const std::pair<ImFont*, float> heading_font = g_large_font;
		ImDrawList* const dl = ImGui::GetWindowDrawList();
		SmallString heading_str;

		ImGui::PushFont(heading_font.first, heading_font.second);
		ImGui::PushStyleColor(ImGuiCol_Text, UIPrimaryTextColor);

		// draw branding
		{
			const ImVec2 logo_pos = LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING, LAYOUT_MENU_BUTTON_Y_PADDING);
			const ImVec2 logo_size = LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
			dl->AddImage(reinterpret_cast<ImTextureID>(GetCachedTexture("icons/AppIconLarge.png")->GetNativeHandle()),
				logo_pos, logo_pos + logo_size);
			dl->AddText(heading_font.first, heading_font.second,
				ImVec2(logo_pos.x + logo_size.x + LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING), logo_pos.y),
				ImGui::GetColorU32(ImGuiCol_Text), "PCSX2");
		}

		// draw time
		ImVec2 time_pos;
		{
			// Waiting on (apple)clang support for P0355R7
			// Migrate to std::chrono::current_zone and zoned_time then
			const auto utc_now = std::chrono::system_clock::now();
			const auto utc_time_t = std::chrono::system_clock::to_time_t(utc_now);
			std::tm tm_local = {};
#ifdef _MSC_VER
			localtime_s(&tm_local, &utc_time_t);
#else
			localtime_r(&utc_time_t, &tm_local);
#endif
			heading_str.format(FSUI_FSTR("{:%H:%M}"), tm_local);

			const ImVec2 time_size = heading_font.first->CalcTextSizeA(heading_font.second, FLT_MAX, 0.0f, "00:00");
			time_pos = ImVec2(heading_size.x - LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING) - time_size.x,
				LayoutScale(LAYOUT_MENU_BUTTON_Y_PADDING));
			ImGui::RenderTextClipped(time_pos, time_pos + time_size, heading_str.c_str(), heading_str.end_ptr(), &time_size);
		}

		// draw achievements info
		if (Achievements::IsActive())
		{
			const auto lock = Achievements::GetLock();
			const char* username = Achievements::GetLoggedInUserName();
			if (username)
			{
				const ImVec2 name_size = heading_font.first->CalcTextSizeA(heading_font.second, FLT_MAX, 0.0f, username);
				const ImVec2 name_pos =
					ImVec2(time_pos.x - name_size.x - LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING), time_pos.y);
				ImGui::RenderTextClipped(name_pos, name_pos + name_size, username, nullptr, &name_size);

				// TODO: should we cache this? heap allocations bad...
				std::string badge_path = Achievements::GetLoggedInUserBadgePath();
				if (!badge_path.empty()) [[likely]]
				{
					const ImVec2 badge_size =
						LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
					const ImVec2 badge_pos =
						ImVec2(name_pos.x - badge_size.x - LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING), time_pos.y);

					dl->AddImage(reinterpret_cast<ImTextureID>(GetCachedTextureAsync(badge_path)->GetNativeHandle()),
						badge_pos, badge_pos + badge_size);
				}
			}
		}

		ImGui::PopStyleColor();
		ImGui::PopFont();
	}
	EndFullscreenWindow();
}

void FullscreenUI::DrawLandingWindow()
{
	ImVec2 menu_pos, menu_size;
	DrawLandingTemplate(&menu_pos, &menu_size);

	ImGui::PushStyleColor(ImGuiCol_Text, UIBackgroundTextColor);

	if (BeginHorizontalMenu("landing_window", menu_pos, menu_size, 4))
	{
		ResetFocusHere();

		if (HorizontalMenuSvgItem("fullscreenui/media-cdrom.svg", FSUI_CSTR("Game List"),
				FSUI_CSTR("Launch a game from images scanned from your game directories.")))
		{
			SwitchToGameList();
		}

		if (HorizontalMenuSvgItem("fullscreenui/start-game.svg", FSUI_CSTR("Start Game"),
				FSUI_CSTR("Launch a game from a file, disc, or starts the console without any disc inserted.")))
		{
			s_current_main_window = MainWindowType::StartGame;
			QueueResetFocus(FocusResetType::WindowChanged);
		}

		if (HorizontalMenuSvgItem("fullscreenui/applications-system.svg", FSUI_CSTR("Settings"),
				FSUI_CSTR("Changes settings for the application.")))
		{
			SwitchToSettings();
		}

		if (HorizontalMenuSvgItem("fullscreenui/exit.svg", FSUI_CSTR("Exit"),
				FSUI_CSTR("Return to desktop mode, or exit the application.")) ||
			(!AreAnyDialogsOpen() && WantsToCloseMenu()))
		{
			s_current_main_window = MainWindowType::Exit;
			QueueResetFocus(FocusResetType::WindowChanged);
		}
	}
	ImGui::PopStyleColor();

	if (ImGui::Shortcut(ImGuiKey_GamepadBack) || ImGui::Shortcut(ImGuiKey_F1))
		OpenAboutWindow();
	if (ImGui::Shortcut(ImGuiKey_NavGamepadInput) || ImGui::Shortcut(ImGuiKey_Space))
		SwitchToGameList();
	else if (ImGui::Shortcut(ImGuiKey_NavGamepadMenu) || ImGui::Shortcut(ImGuiKey_F11))
		DoToggleFullscreen();

	EndHorizontalMenu();

	if (IsGamepadInputSource())
	{
		const bool circleOK = ImGui::GetIO().ConfigNavSwapGamepadButtons;
		const bool swapNorthWest = ImGuiManager::IsGamepadNorthWestSwapped();
		SetFullscreenFooterText(std::array{
			std::make_pair(ICON_PF_SELECT_SHARE, FSUI_VSTR("About")),
			std::make_pair(ICON_PF_DPAD_LEFT_RIGHT, FSUI_VSTR("Navigate")),
			std::make_pair(swapNorthWest ? ICON_PF_BUTTON_SQUARE : ICON_PF_BUTTON_TRIANGLE, FSUI_VSTR("Game List")),
			std::make_pair(swapNorthWest ? ICON_PF_BUTTON_TRIANGLE : ICON_PF_BUTTON_SQUARE, FSUI_VSTR("Toggle Fullscreen")),
			std::make_pair(circleOK ? ICON_PF_BUTTON_CIRCLE : ICON_PF_BUTTON_CROSS, FSUI_VSTR("Select")),
			std::make_pair(circleOK ? ICON_PF_BUTTON_CROSS : ICON_PF_BUTTON_CIRCLE, FSUI_VSTR("Exit")),
		});
	}
	else
	{
		SetFullscreenFooterText(std::array{
			std::make_pair(ICON_PF_F1, FSUI_VSTR("About")),
			std::make_pair(ICON_PF_F11, FSUI_VSTR("Toggle Fullscreen")),
			std::make_pair(ICON_PF_ARROW_LEFT ICON_PF_ARROW_RIGHT, FSUI_VSTR("Navigate")),
			std::make_pair(ICON_PF_SPACE, FSUI_VSTR("Game List")),
			std::make_pair(ICON_PF_ENTER, FSUI_VSTR("Select")),
			std::make_pair(ICON_PF_ESC, FSUI_VSTR("Exit")),
		});
	}
}

void FullscreenUI::DrawStartGameWindow()
{
	ImVec2 menu_pos, menu_size;
	DrawLandingTemplate(&menu_pos, &menu_size);

	ImGui::PushStyleColor(ImGuiCol_Text, UIBackgroundTextColor);

	if (BeginHorizontalMenu("start_game_window", menu_pos, menu_size, 4))
	{
		ResetFocusHere();

		if (HorizontalMenuSvgItem("fullscreenui/start-file.svg", FSUI_CSTR("Start File"),
				FSUI_CSTR("Launch a game by selecting a file/disc image.")))
		{
			DoStartFile();
		}

		if (HorizontalMenuSvgItem("fullscreenui/drive-cdrom.svg", FSUI_CSTR("Start Disc"),
				FSUI_CSTR("Start a game from a disc in your PC's DVD drive.")))
		{
			DoStartDisc();
		}

		if (HorizontalMenuSvgItem("fullscreenui/start-bios.svg", FSUI_CSTR("Start BIOS"),
				FSUI_CSTR("Start the console without any disc inserted.")))
		{
			DoStartBIOS();
		}

		if (HorizontalMenuSvgItem("fullscreenui/back-icon.svg", FSUI_CSTR("Back"),
				FSUI_CSTR("Return to the previous menu.")) ||
			(!AreAnyDialogsOpen() && WantsToCloseMenu()))
		{
			s_current_main_window = MainWindowType::Landing;
			QueueResetFocus(FocusResetType::WindowChanged);
		}
	}

	ImGui::PopStyleColor();

	if (ImGui::Shortcut(ImGuiKey_NavGamepadMenu) || ImGui::Shortcut(ImGuiKey_F1))
		OpenSaveStateSelector(true);

	EndHorizontalMenu();

	if (IsGamepadInputSource())
	{
		const bool circleOK = ImGui::GetIO().ConfigNavSwapGamepadButtons;
		const bool swapNorthWest = ImGuiManager::IsGamepadNorthWestSwapped();
		SetFullscreenFooterText(std::array{
			std::make_pair(ICON_PF_DPAD_LEFT_RIGHT, FSUI_VSTR("Navigate")),
			std::make_pair(swapNorthWest ? ICON_PF_BUTTON_TRIANGLE : ICON_PF_BUTTON_SQUARE, FSUI_VSTR("Load Global State")),
			std::make_pair(circleOK ? ICON_PF_BUTTON_CIRCLE : ICON_PF_BUTTON_CROSS, FSUI_VSTR("Select")),
			std::make_pair(circleOK ? ICON_PF_BUTTON_CROSS : ICON_PF_BUTTON_CIRCLE, FSUI_VSTR("Back")),
		});
	}
	else
	{
		SetFullscreenFooterText(std::array{
			std::make_pair(ICON_PF_ARROW_LEFT ICON_PF_ARROW_RIGHT, FSUI_VSTR("Navigate")),
			std::make_pair(ICON_PF_F1, FSUI_VSTR("Load Global State")),
			std::make_pair(ICON_PF_ENTER, FSUI_VSTR("Select")),
			std::make_pair(ICON_PF_ESC, FSUI_VSTR("Back")),
		});
	}
}

void FullscreenUI::DrawExitWindow()
{
	ImVec2 menu_pos, menu_size;
	DrawLandingTemplate(&menu_pos, &menu_size);

	ImGui::PushStyleColor(ImGuiCol_Text, UIBackgroundTextColor);

	if (BeginHorizontalMenu("exit_window", menu_pos, menu_size, (Host::InNoGUIMode()) ? 2 : 3))
	{
		ResetFocusHere();

		if (HorizontalMenuSvgItem("fullscreenui/back-icon.svg", FSUI_CSTR("Back"),
				FSUI_CSTR("Return to the previous menu.")) ||
			WantsToCloseMenu())
		{
			s_current_main_window = MainWindowType::Landing;
			QueueResetFocus(FocusResetType::WindowChanged);
		}

		if (HorizontalMenuSvgItem("fullscreenui/exit.svg", FSUI_CSTR("Exit PCSX2"),
				FSUI_CSTR("Completely exits the application, returning you to your desktop.")))
		{
			DoRequestExit();
		}

		if (!Host::InNoGUIMode())
		{
			if (HorizontalMenuSvgItem("fullscreenui/desktop-mode.svg", FSUI_CSTR("Desktop Mode"),
					FSUI_CSTR("Exits Big Picture mode, returning to the desktop interface.")))
			{
				DoDesktopMode();
			}
		}
	}
	EndHorizontalMenu();

	ImGui::PopStyleColor();

	if (IsGamepadInputSource())
	{
		const bool circleOK = ImGui::GetIO().ConfigNavSwapGamepadButtons;
		SetFullscreenFooterText(std::array{
			std::make_pair(ICON_PF_DPAD_LEFT_RIGHT, FSUI_VSTR("Navigate")),
			std::make_pair(circleOK ? ICON_PF_BUTTON_CIRCLE : ICON_PF_BUTTON_CROSS, FSUI_VSTR("Select")),
			std::make_pair(circleOK ? ICON_PF_BUTTON_CROSS : ICON_PF_BUTTON_CIRCLE, FSUI_VSTR("Back")),
		});
	}
	else
	{
		SetFullscreenFooterText(std::array{
			std::make_pair(ICON_PF_ARROW_LEFT ICON_PF_ARROW_RIGHT, FSUI_VSTR("Navigate")),
			std::make_pair(ICON_PF_ENTER, FSUI_VSTR("Select")),
			std::make_pair(ICON_PF_ESC, FSUI_VSTR("Back")),
		});
	}
}

static void DrawShadowedText(
	ImDrawList* dl, std::pair<ImFont*, float> font, const ImVec2& pos, u32 col, const char* text, const char* text_end = nullptr, float wrap_width = 0.0f)
{
	dl->AddText(font.first, font.second, pos + LayoutScale(1.0f, 1.0f), IM_COL32(0, 0, 0, 100), text, text_end, wrap_width);
	dl->AddText(font.first, font.second, pos, col, text, text_end, wrap_width);
}

void FullscreenUI::DrawPauseMenu(MainWindowType type)
{
	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	const ImVec2 display_size(ImGui::GetIO().DisplaySize);
	const ImU32 text_color = IM_COL32(UIBackgroundTextColor.x * 255, UIBackgroundTextColor.y * 255, UIBackgroundTextColor.z * 255, 255);
	dl->AddRectFilled(
		ImVec2(0.0f, 0.0f), display_size, IM_COL32(UIBackgroundColor.x * 255, UIBackgroundColor.y * 255, UIBackgroundColor.z * 255, 200));

	// title info
	{
		const float image_width = 60.0f;
		const float image_height = 90.0f;
		const std::string_view path_string(Path::GetFileName(s_current_disc_path));
		const ImVec2 title_size(
			g_large_font.first->CalcTextSizeA(g_large_font.second, std::numeric_limits<float>::max(), -1.0f, s_current_game_title.c_str()));
		const ImVec2 path_size(path_string.empty() ?
								   ImVec2(0.0f, 0.0f) :
								   g_medium_font.first->CalcTextSizeA(g_medium_font.second, std::numeric_limits<float>::max(), -1.0f,
									   path_string.data(), path_string.data() + path_string.length()));
		const ImVec2 subtitle_size(g_medium_font.first->CalcTextSizeA(
			g_medium_font.second, std::numeric_limits<float>::max(), -1.0f, s_current_game_subtitle.c_str()));

		ImVec2 title_pos(display_size.x - LayoutScale(10.0f + image_width + 20.0f) - title_size.x,
			display_size.y - LayoutScale(LAYOUT_FOOTER_HEIGHT) - LayoutScale(10.0f + image_height));
		ImVec2 path_pos(display_size.x - LayoutScale(10.0f + image_width + 20.0f) - path_size.x,
			title_pos.y + g_large_font.second + LayoutScale(4.0f));
		ImVec2 subtitle_pos(display_size.x - LayoutScale(10.0f + image_width + 20.0f) - subtitle_size.x,
			(path_string.empty() ? title_pos.y + g_large_font.second : path_pos.y + g_medium_font.second) + LayoutScale(4.0f));

		float rp_height = 0.0f;
		{
			const auto lock = Achievements::GetLock();
			const std::string& rp = Achievements::IsActive() ? Achievements::GetRichPresenceString() : std::string();

			if (!rp.empty())
			{
				const float wrap_width = LayoutScale(350.0f);
				const ImVec2 rp_size = g_medium_font.first->CalcTextSizeA(
					g_medium_font.second, std::numeric_limits<float>::max(), wrap_width, rp.data(), rp.data() + rp.length());

				// Add a small extra gap if any Rich Presence is displayed
				rp_height = rp_size.y - g_medium_font.second + LayoutScale(2.0f);

				const ImVec2 rp_pos(display_size.x - LayoutScale(20.0f + 50.0f + 20.0f) - rp_size.x,
					subtitle_pos.y + g_medium_font.second + LayoutScale(4.0f) - rp_height);

				title_pos.y -= rp_height;
				path_pos.y -= rp_height;
				subtitle_pos.y -= rp_height;

				DrawShadowedText(dl, g_medium_font, rp_pos, text_color, rp.data(), rp.data() + rp.length(), wrap_width);
			}
		}

		DrawShadowedText(dl, g_large_font, title_pos, text_color, s_current_game_title.c_str());
		if (!path_string.empty())
		{
			DrawShadowedText(dl, g_medium_font, path_pos, text_color, path_string.data(), path_string.data() + path_string.length());
		}
		DrawShadowedText(dl, g_medium_font, subtitle_pos, text_color, s_current_game_subtitle.c_str());

		const ImVec2 image_min(display_size.x - LayoutScale(10.0f + image_width),
			display_size.y - LayoutScale(LAYOUT_FOOTER_HEIGHT) - LayoutScale(10.0f + image_height) - rp_height);
		const ImVec2 image_max(image_min.x + LayoutScale(image_width), image_min.y + LayoutScale(image_height) + rp_height);
		{
			auto lock = GameList::GetLock();

			const GameList::Entry* entry = GameList::GetEntryForPath(s_current_disc_path.c_str());
			if (entry)
				DrawGameCover(entry, dl, image_min, image_max);
			else
				DrawFallbackCover(dl, image_min, image_max);
		}
	}

	// current time / play time
	{
		char buf[256];
		struct tm ltime;
		const std::time_t ctime(std::time(nullptr));
#ifdef _MSC_VER
		localtime_s(&ltime, &ctime);
#else
		localtime_r(&ctime, &ltime);
#endif
		std::strftime(buf, sizeof(buf), "%X", &ltime);

		const ImVec2 time_size(g_large_font.first->CalcTextSizeA(g_large_font.second, std::numeric_limits<float>::max(), -1.0f, buf));
		const ImVec2 time_pos(display_size.x - LayoutScale(10.0f) - time_size.x, LayoutScale(10.0f));
		DrawShadowedText(dl, g_large_font, time_pos, text_color, buf);

		if (!s_current_disc_serial.empty())
		{
			const std::time_t cached_played_time = GameList::GetCachedPlayedTimeForSerial(s_current_disc_serial);
			const std::time_t session_time = static_cast<std::time_t>(VMManager::GetSessionPlayedTime());
			const std::string played_time_str(GameList::FormatTimespan(cached_played_time + session_time, true));
			const std::string session_time_str(GameList::FormatTimespan(session_time, true));

			SmallString buf;
			buf.format(FSUI_FSTR("This Session: {}"), session_time_str);
			const ImVec2 session_size(g_medium_font.first->CalcTextSizeA(g_medium_font.second, std::numeric_limits<float>::max(), -1.0f, buf));
			const ImVec2 session_pos(
				display_size.x - LayoutScale(10.0f) - session_size.x, time_pos.y + g_large_font.second + LayoutScale(4.0f));
			DrawShadowedText(dl, g_medium_font, session_pos, text_color, buf);

			buf.format(FSUI_FSTR("All Time: {}"), played_time_str);
			const ImVec2 total_size(g_medium_font.first->CalcTextSizeA(g_medium_font.second, std::numeric_limits<float>::max(), -1.0f, buf));
			const ImVec2 total_pos(
				display_size.x - LayoutScale(10.0f) - total_size.x, session_pos.y + g_medium_font.second + LayoutScale(4.0f));
			DrawShadowedText(dl, g_medium_font, total_pos, text_color, buf);
		}
	}

	const ImVec2 window_size(LayoutScale(500.0f, LAYOUT_SCREEN_HEIGHT));
	const ImVec2 window_pos(0.0f, display_size.y - LayoutScale(LAYOUT_FOOTER_HEIGHT) - window_size.y);

	if (BeginFullscreenWindow(window_pos, window_size, "pause_menu", ImVec4(0.0f, 0.0f, 0.0f, 0.0f), 0.0f,
			ImVec2(10.0f, 10.0f), ImGuiWindowFlags_NoBackground))
	{
		static constexpr u32 submenu_item_count[] = {
			11, // None
			4, // Exit
			3, // Achievements
		};

		const bool just_focused = ResetFocusHere();
		BeginMenuButtons(submenu_item_count[static_cast<u32>(s_current_pause_submenu)], 1.0f, ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING,
			ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

		if (!ImGui::IsPopupOpen(0u, ImGuiPopupFlags_AnyPopup))
		{
			const bool up_pressed = ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp, ImGuiInputFlags_Repeat, ImGuiKeyOwner_NoOwner) ||
									ImGui::IsKeyPressed(ImGuiKey_UpArrow, ImGuiInputFlags_Repeat, ImGuiKeyOwner_NoOwner);
			const bool down_pressed = ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown, ImGuiInputFlags_Repeat, ImGuiKeyOwner_NoOwner) ||
									  ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_Repeat, ImGuiKeyOwner_NoOwner);

			if (up_pressed || down_pressed)
			{
				const ImGuiID current_focus_id = ImGui::GetFocusID();
				ImGuiWindow* window = ImGui::GetCurrentWindow();
				ImGuiID first_id = 0;
				ImGuiID last_id = 0;

				switch (s_current_pause_submenu)
				{
					case PauseSubMenu::None:
						first_id = ImGui::GetID(FSUI_ICONSTR(ICON_FA_PLAY, "Resume Game"));
						last_id = ImGui::GetID(FSUI_ICONSTR(ICON_FA_POWER_OFF, "Close Game"));
						break;
					case PauseSubMenu::Exit:
						first_id = ImGui::GetID(FSUI_ICONSTR(ICON_PF_BACKWARD, "Back To Pause Menu"));
						last_id = ImGui::GetID(FSUI_ICONSTR(ICON_FA_POWER_OFF, "Exit Without Saving"));
						break;
					case PauseSubMenu::Achievements:
						first_id = ImGui::GetID(FSUI_ICONSTR(ICON_PF_BACKWARD, "Back To Pause Menu"));
						last_id = ImGui::GetID(FSUI_ICONSTR(ICON_FA_STOPWATCH, "Leaderboards"));
						break;
				}

				if (first_id != 0 && last_id != 0)
				{
					if (up_pressed && current_focus_id == first_id)
						ImGui::SetFocusID(last_id, window);
					else if (down_pressed && current_focus_id == last_id)
						ImGui::SetFocusID(first_id, window);
				}
			}
		}

		switch (s_current_pause_submenu)
		{
			case PauseSubMenu::None:
			{
				// NOTE: Menu close must come first, because otherwise VM destruction options will race.
				const bool can_load_state = s_current_disc_crc != 0 && !Achievements::IsHardcoreModeActive();
				const bool can_save_state = s_current_disc_crc != 0;

				if (just_focused)
					ImGui::SetFocusID(ImGui::GetID(FSUI_ICONSTR(ICON_FA_PLAY, "Resume Game")), ImGui::GetCurrentWindow());

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_PLAY, "Resume Game"), false) || WantsToCloseMenu())
					ClosePauseMenu();

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_FORWARD_FAST, "Toggle Frame Limit"), false))
				{
					ClosePauseMenu();
					DoToggleFrameLimit();
				}

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_ARROW_ROTATE_LEFT, "Load State"), false, can_load_state))
				{
					if (OpenSaveStateSelector(true))
						s_current_main_window = MainWindowType::None;
				}

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_DOWNLOAD, "Save State"), false, can_save_state))
				{
					if (OpenSaveStateSelector(false))
						s_current_main_window = MainWindowType::None;
				}

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_WRENCH, "Game Properties"), false, can_save_state))
				{
					SwitchToGameSettings();
				}

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_TROPHY, "Achievements"), false, Achievements::HasAchievementsOrLeaderboards()))
				{
					// skip second menu and go straight to cheevos if there's no lbs
					if (!Achievements::HasLeaderboards())
						OpenAchievementsWindow();
					else
						OpenPauseSubMenu(PauseSubMenu::Achievements);
				}

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_CAMERA, "Save Screenshot"), false))
				{
					GSQueueSnapshot(std::string());
					ClosePauseMenu();
				}

				if (ActiveButton(GSIsHardwareRenderer() ? (FSUI_ICONSTR(ICON_FA_PAINTBRUSH, "Switch To Software Renderer")) :
														  (FSUI_ICONSTR(ICON_FA_PAINTBRUSH, "Switch To Hardware Renderer")),
						false))
				{
					ClosePauseMenu();
					DoToggleSoftwareRenderer();
				}

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_COMPACT_DISC, "Change Disc"), false))
				{
					s_current_main_window = MainWindowType::None;
					RequestChangeDisc();
				}

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_SLIDERS, "Settings"), false))
					SwitchToSettings();

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_POWER_OFF, "Close Game"), false))
				{
					// skip submenu when we can't save anyway
					if (!can_save_state)
						RequestShutdown(false);
					else
						OpenPauseSubMenu(PauseSubMenu::Exit);
				}
			}
			break;

			case PauseSubMenu::Exit:
			{
				if (just_focused)
				{
					ImGui::SetFocusID(ImGui::GetID(FSUI_ICONSTR(ICON_FA_POWER_OFF, "Exit Without Saving")), ImGui::GetCurrentWindow());
				}

				if (ActiveButton(FSUI_ICONSTR(ICON_PF_BACKWARD, "Back To Pause Menu"), false) || WantsToCloseMenu())
					OpenPauseSubMenu(PauseSubMenu::None);

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_ARROWS_SPIN, "Reset System"), false))
				{
					RequestReset();
				}

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_FLOPPY_DISK, "Exit And Save State"), false))
					RequestShutdown(true);

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_POWER_OFF, "Exit Without Saving"), false))
					RequestShutdown(false);
			}
			break;

			case PauseSubMenu::Achievements:
			{
				if (just_focused)
					ImGui::SetFocusID(ImGui::GetID(FSUI_ICONSTR(ICON_PF_BACKWARD, "Back To Pause Menu")), ImGui::GetCurrentWindow());

				if (ActiveButton(FSUI_ICONSTR(ICON_PF_BACKWARD, "Back To Pause Menu"), false) || WantsToCloseMenu())
					OpenPauseSubMenu(PauseSubMenu::None);

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_TROPHY, "Achievements"), false))
					OpenAchievementsWindow();

				if (ActiveButton(FSUI_ICONSTR(ICON_FA_STOPWATCH, "Leaderboards"), false))
					OpenLeaderboardsWindow();
			}
			break;
		}

		EndMenuButtons();

		EndFullscreenWindow();
	}

	// Primed achievements must come first, because we don't want the pause screen to be behind them.
	if (Achievements::HasAchievementsOrLeaderboards())
		Achievements::DrawPauseMenuOverlays();

	if (IsGamepadInputSource())
	{
		const bool circleOK = ImGui::GetIO().ConfigNavSwapGamepadButtons;
		SetFullscreenFooterText(std::array{
			std::make_pair(ICON_PF_DPAD_UP_DOWN, FSUI_VSTR("Change Selection")),
			std::make_pair(circleOK ? ICON_PF_BUTTON_CIRCLE : ICON_PF_BUTTON_CROSS, FSUI_VSTR("Select")),
			std::make_pair(circleOK ? ICON_PF_BUTTON_CROSS : ICON_PF_BUTTON_CIRCLE, FSUI_VSTR("Return To Game")),
		});
	}
	else
	{
		SetFullscreenFooterText(std::array{
			std::make_pair(ICON_PF_ARROW_UP ICON_PF_ARROW_DOWN, FSUI_VSTR("Change Selection")),
			std::make_pair(ICON_PF_ENTER, FSUI_VSTR("Select")),
			std::make_pair(ICON_PF_ESC, FSUI_VSTR("Return To Game")),
		});
	}
}

void FullscreenUI::InitializePlaceholderSaveStateListEntry(SaveStateListEntry* li, s32 slot)
{
	li->title = fmt::format("{}##game_slot_{}", TinyString::from_format(FSUI_FSTR("Save Slot {0}"), slot), slot);
	li->summary = FSUI_STR("No save present in this slot.");
	li->path = {};
	li->timestamp = 0;
	li->slot = slot;
	li->preview_texture = {};
}

bool FullscreenUI::InitializeSaveStateListEntry(
	SaveStateListEntry* li, const std::string& title, const std::string& serial, u32 crc, s32 slot, bool backup)
{
	std::string filename(VMManager::GetSaveStateFileName(serial.c_str(), crc, slot, backup));
	FILESYSTEM_STAT_DATA sd;
	if (filename.empty() || !FileSystem::StatFile(filename.c_str(), &sd))
	{
		InitializePlaceholderSaveStateListEntry(li, slot);
		return false;
	}

	li->title = fmt::format("{}##game_slot_{}", TinyString::from_format(FSUI_FSTR("{0} Slot {1}"), backup ? "Backup Save" : "Save", slot), slot);
	li->summary = fmt::format(FSUI_FSTR("Saved {}"), TimeToPrintableString(sd.ModificationTime));
	li->slot = slot;
	li->timestamp = sd.ModificationTime;
	li->path = std::move(filename);

	li->preview_texture.reset();

	u32 screenshot_width, screenshot_height;
	std::vector<u32> screenshot_pixels;
	if (SaveState_ReadScreenshot(li->path, &screenshot_width, &screenshot_height, &screenshot_pixels))
	{
		li->preview_texture =
			std::unique_ptr<GSTexture>(g_gs_device->CreateTexture(screenshot_width, screenshot_height, 1, GSTexture::Format::Color));
		if (!li->preview_texture || !li->preview_texture->Update(GSVector4i(0, 0, screenshot_width, screenshot_height),
										screenshot_pixels.data(), sizeof(u32) * screenshot_width))
		{
			Console.Error("Failed to upload save state image to GPU");
			if (li->preview_texture)
				g_gs_device->Recycle(li->preview_texture.release());
		}
	}

	return true;
}

void FullscreenUI::ClearSaveStateEntryList()
{
	for (SaveStateListEntry& entry : s_save_state_selector_slots)
	{
		if (entry.preview_texture)
			s_cleanup_textures.push_back(std::move(entry.preview_texture));
	}
	s_save_state_selector_slots.clear();
}

u32 FullscreenUI::PopulateSaveStateListEntries(const std::string& title, const std::string& serial, u32 crc)
{
	ClearSaveStateEntryList();

	for (s32 i = 1; i <= VMManager::NUM_SAVE_STATE_SLOTS; i++)
	{
		SaveStateListEntry li;
		if (InitializeSaveStateListEntry(&li, title, serial, crc, i) || !s_save_state_selector_loading)
			s_save_state_selector_slots.push_back(std::move(li));

		if (s_save_state_selector_loading)
		{
			SaveStateListEntry bli;
			if (InitializeSaveStateListEntry(&bli, title, serial, crc, i, true))
				s_save_state_selector_slots.push_back(std::move(bli));
		}
	}

	return static_cast<u32>(s_save_state_selector_slots.size());
}

bool FullscreenUI::OpenLoadStateSelectorForGame(const std::string& game_path)
{
	auto lock = GameList::GetLock();
	const GameList::Entry* entry = GameList::GetEntryForPath(game_path.c_str());
	if (entry)
	{
		s_save_state_selector_loading = true;
		if (PopulateSaveStateListEntries(entry->title.c_str(), entry->serial.c_str(), entry->crc) > 0)
		{
			s_save_state_selector_open = true;
			s_save_state_selector_resuming = false;
			s_save_state_selector_game_path = game_path;
			return true;
		}
	}

	ShowToast({}, FSUI_STR("No save states found."), 5.0f);
	return false;
}

bool FullscreenUI::OpenSaveStateSelector(bool is_loading)
{
	s_save_state_selector_game_path = {};
	s_save_state_selector_loading = is_loading;
	s_save_state_selector_resuming = false;
	if (PopulateSaveStateListEntries(s_current_game_title.c_str(), s_current_disc_serial.c_str(), s_current_disc_crc) > 0)
	{
		s_save_state_selector_open = true;
		return true;
	}

	ShowToast({}, FSUI_STR("No save states found."), 5.0f);
	return false;
}

void FullscreenUI::CloseSaveStateSelector()
{
	ClearSaveStateEntryList();
	s_save_state_selector_open = false;
	s_save_state_selector_submenu_index = -1;
	s_save_state_selector_loading = false;
	s_save_state_selector_resuming = false;
	s_save_state_selector_game_path = {};
}

void FullscreenUI::DrawSaveStateSelector(bool is_loading)
{
	ImGuiIO& io = ImGui::GetIO();

	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(io.DisplaySize - LayoutScale(0.0f, LAYOUT_FOOTER_HEIGHT));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);

	const char* window_title = is_loading ? FSUI_CSTR("Load State") : FSUI_CSTR("Save State");
	ImGui::OpenPopup(window_title);

	bool is_open = true;
	const bool valid = ImGui::BeginPopupModal(window_title, &is_open,
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoBackground);
	if (!valid || !is_open)
	{
		if (valid)
			ImGui::EndPopup();

		ImGui::PopStyleVar(5);
		if (!is_open)
		{
			CloseSaveStateSelector();
			ReturnToPreviousWindow();
		}
		return;
	}

	const ImVec2 heading_size =
		ImVec2(io.DisplaySize.x, LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY) +
									 (LayoutScale(LAYOUT_MENU_BUTTON_Y_PADDING) * 2.0f) + LayoutScale(2.0f));

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ModAlpha(UIPrimaryColor, 0.9f));

	if (ImGui::BeginChild("state_titlebar", heading_size, ImGuiChildFlags_NavFlattened, 0))
	{
		BeginNavBar();
		if (NavButton(ICON_PF_BACKWARD, true, true))
		{
			CloseSaveStateSelector();
			ReturnToPreviousWindow();
		}

		NavTitle(is_loading ? FSUI_CSTR("Load State") : FSUI_CSTR("Save State"));
		EndNavBar();
		ImGui::EndChild();
	}

	ImGui::PopStyleColor();
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ModAlpha(UIBackgroundColor, 0.9f));
	ImGui::SetCursorPos(ImVec2(0.0f, heading_size.y));

	bool close_handled = false;
	if (s_save_state_selector_open &&
		ImGui::BeginChild("state_list", ImVec2(io.DisplaySize.x, io.DisplaySize.y - LayoutScale(LAYOUT_FOOTER_HEIGHT) - heading_size.y),
			ImGuiChildFlags_NavFlattened, 0))
	{
		BeginMenuButtons();

		const ImGuiStyle& style = ImGui::GetStyle();

		const float title_spacing = LayoutScale(10.0f);
		const float summary_spacing = LayoutScale(4.0f);
		const float item_spacing = LayoutScale(20.0f);
		const float item_width_with_spacing = std::floor(LayoutScale(LAYOUT_SCREEN_WIDTH / 4.0f));
		const float item_width = item_width_with_spacing - item_spacing;
		const float image_width = item_width - (style.FramePadding.x * 2.0f);
		const float image_height = image_width / 1.33f;
		const ImVec2 image_size(image_width, image_height);
		const float item_height = (style.FramePadding.y * 2.0f) + image_height + title_spacing + g_large_font.second + summary_spacing +
								  g_medium_font.second;
		const ImVec2 item_size(item_width, item_height);
		const u32 grid_count_x = std::floor(ImGui::GetWindowWidth() / item_width_with_spacing);
		const float start_x =
			(static_cast<float>(ImGui::GetWindowWidth()) - (item_width_with_spacing * static_cast<float>(grid_count_x))) * 0.5f;

		u32 grid_x = 0;
		for (u32 i = 0; i < s_save_state_selector_slots.size();)
		{
			if (i == 0)
				ResetFocusHere();

			if (static_cast<s32>(i) == s_save_state_selector_submenu_index)
			{
				SaveStateListEntry& entry = s_save_state_selector_slots[i];

				// can't use a choice dialog here, because we're already in a modal...
				ImGuiFullscreen::PushResetLayout();
				ImGui::PushFont(g_large_font.first, g_large_font.second);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING, LAYOUT_MENU_BUTTON_Y_PADDING));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
				ImGui::PushStyleColor(ImGuiCol_Text, UIPrimaryTextColor);
				ImGui::PushStyleColor(ImGuiCol_TitleBg, UIPrimaryDarkColor);
				ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UIPrimaryColor);
				ImGui::PushStyleColor(ImGuiCol_PopupBg, UIPopupBackgroundColor);

				const float width = LayoutScale(600.0f);
				const float title_height =
					g_large_font.second + ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetStyle().WindowPadding.y * 2.0f;
				const float height =
					title_height + LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY + (LAYOUT_MENU_BUTTON_Y_PADDING * 2.0f)) * 3.0f;
				ImGui::SetNextWindowSize(ImVec2(width, height));
				ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
				ImGui::OpenPopup(entry.title.c_str());

				// don't let the back button flow through to the main window
				bool submenu_open = !WantsToCloseMenu();
				close_handled ^= submenu_open;

				bool closed = false;
				if (ImGui::BeginPopupModal(
						entry.title.c_str(), &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
				{
					ImGui::PushStyleColor(ImGuiCol_Text, UIBackgroundTextColor);

					BeginMenuButtons();

					if (ActiveButton(
							is_loading ? FSUI_ICONSTR(ICON_FA_FOLDER_OPEN, "Load State") : FSUI_ICONSTR(ICON_FA_FOLDER_OPEN, "Save State"),
							false, is_loading ? !Achievements::IsHardcoreModeActive() : true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
					{
						if (is_loading)
							DoLoadState(std::move(entry.path), entry.slot, false);
						else
							DoSaveState(entry.slot);

						CloseSaveStateSelector();
						ReturnToMainWindow();
						closed = true;
					}

					if (ActiveButton(FSUI_ICONSTR(ICON_FA_TRASH, "Delete Save"), false, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
					{
						if (!FileSystem::FileExists(entry.path.c_str()))
						{
							ShowToast({}, fmt::format(FSUI_FSTR("{} does not exist."), ImGuiFullscreen::RemoveHash(entry.title)));
							is_open = true;
						}
						else if (FileSystem::DeleteFilePath(entry.path.c_str()))
						{
							ShowToast({}, fmt::format(FSUI_FSTR("{} deleted."), ImGuiFullscreen::RemoveHash(entry.title)));
							if (s_save_state_selector_loading)
								s_save_state_selector_slots.erase(s_save_state_selector_slots.begin() + i);
							else
								InitializePlaceholderSaveStateListEntry(&entry, entry.slot);

							// Close if this was the last state.
							if (s_save_state_selector_slots.empty())
							{
								CloseSaveStateSelector();
								ReturnToMainWindow();
								closed = true;
							}
							else
							{
								is_open = false;
							}
						}
						else
						{
							ShowToast({}, fmt::format(FSUI_FSTR("Failed to delete {}."), ImGuiFullscreen::RemoveHash(entry.title)));
							is_open = false;
						}
					}

					if (ActiveButton(FSUI_ICONSTR(ICON_FA_SQUARE_XMARK, "Close Menu"), false, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
					{
						is_open = false;
					}

					EndMenuButtons();

					ImGui::PopStyleColor();
					ImGui::EndPopup();
				}
				if (!is_open)
				{
					s_save_state_selector_submenu_index = -1;
					if (!closed)
						QueueResetFocus(FocusResetType::WindowChanged);
				}

				ImGui::PopStyleColor(4);
				ImGui::PopStyleVar(3);
				ImGui::PopFont();
				ImGuiFullscreen::PopResetLayout();

				if (closed || i >= s_save_state_selector_slots.size())
					break;
			}

			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (window->SkipItems)
			{
				i++;
				continue;
			}

			if (grid_x == grid_count_x)
			{
				grid_x = 0;
				ImGui::SetCursorPosX(start_x);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + item_spacing);
			}
			else
			{
				ImGui::SameLine(start_x + static_cast<float>(grid_x) * (item_width + item_spacing));
			}

			const SaveStateListEntry& entry = s_save_state_selector_slots[i];
			const ImGuiID id = window->GetID(static_cast<int>(i));
			const ImVec2 pos(window->DC.CursorPos);
			ImRect bb(pos, pos + item_size);
			ImGui::ItemSize(item_size);
			if (ImGui::ItemAdd(bb, id))
			{
				bool held;
				bool hovered;
				bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);
				if (hovered)
				{
					const ImU32 col = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered, 1.0f);

					const float t = std::min<float>(std::abs(std::sin(ImGui::GetTime() * 0.75) * 1.1), 1.0f);
					ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_Border, t));

					ImGuiFullscreen::DrawMenuButtonFrame(bb.Min, bb.Max, col, true, 0.0f);

					ImGui::PopStyleColor();
				}

				bb.Min += style.FramePadding;
				bb.Max -= style.FramePadding;

				const GSTexture* const screenshot = entry.preview_texture ? entry.preview_texture.get() : GetPlaceholderTexture().get();
				const ImRect image_rect(CenterImage(ImRect(bb.Min, bb.Min + image_size),
					ImVec2(static_cast<float>(screenshot->GetWidth()), static_cast<float>(screenshot->GetHeight()))));

				ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(screenshot->GetNativeHandle()),
					image_rect.Min, image_rect.Max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), IM_COL32(255, 255, 255, 255));

				const ImVec2 title_pos(bb.Min.x, bb.Min.y + image_height + title_spacing);
				const ImRect title_bb(title_pos, ImVec2(bb.Max.x, title_pos.y + g_large_font.second));
				ImGui::PushFont(g_large_font.first, g_large_font.second);
				ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, entry.title.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
				ImGui::PopFont();

				if (!entry.summary.empty())
				{
					const ImVec2 summary_pos(bb.Min.x, title_pos.y + g_large_font.second + summary_spacing);
					const ImRect summary_bb(summary_pos, ImVec2(bb.Max.x, summary_pos.y + g_medium_font.second));
					ImGui::PushFont(g_medium_font.first, g_medium_font.second);
					ImGui::RenderTextClipped(
						summary_bb.Min, summary_bb.Max, entry.summary.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
					ImGui::PopFont();
				}

				if (pressed)
				{
					if (is_loading)
						DoLoadState(entry.path, entry.slot, false);
					else
						DoSaveState(entry.slot);

					CloseSaveStateSelector();
					ReturnToMainWindow();
					break;
				}
				else if (hovered && (ImGui::IsItemClicked(ImGuiMouseButton_Right) || ImGui::Shortcut(ImGuiKey_NavGamepadMenu) ||
										ImGui::Shortcut(ImGuiKey_F1)))
				{
					s_save_state_selector_submenu_index = static_cast<s32>(i);
				}
			}

			grid_x++;
			i++;
		}

		EndMenuButtons();
		ImGui::EndChild();
	}

	ImGui::PopStyleColor();

	ImGui::EndPopup();
	ImGui::PopStyleVar(5);

	if (!close_handled && WantsToCloseMenu())
	{
		CloseSaveStateSelector();
		ReturnToPreviousWindow();
	}
	else
	{
		if (IsGamepadInputSource())
		{
			const bool circleOK = ImGui::GetIO().ConfigNavSwapGamepadButtons;
			const bool swapNorthWest = ImGuiManager::IsGamepadNorthWestSwapped();
			SetFullscreenFooterText(std::array{
				std::make_pair(ICON_PF_DPAD, FSUI_VSTR("Select State")),
				std::make_pair(swapNorthWest ? ICON_PF_BUTTON_TRIANGLE : ICON_PF_BUTTON_SQUARE, FSUI_VSTR("Options")),
				std::make_pair(circleOK ? ICON_PF_BUTTON_CIRCLE : ICON_PF_BUTTON_CROSS, FSUI_VSTR("Load/Save State")),
				std::make_pair(circleOK ? ICON_PF_BUTTON_CROSS : ICON_PF_BUTTON_CIRCLE, FSUI_VSTR("Cancel")),
			});
		}
		else
		{
			SetFullscreenFooterText(std::array{
				std::make_pair(ICON_PF_ARROW_UP ICON_PF_ARROW_DOWN ICON_PF_ARROW_LEFT ICON_PF_ARROW_RIGHT, FSUI_VSTR("Select State")),
				std::make_pair(ICON_PF_F1, FSUI_VSTR("Options")),
				std::make_pair(ICON_PF_ENTER, FSUI_VSTR("Load/Save State")),
				std::make_pair(ICON_PF_ESC, FSUI_VSTR("Cancel")),
			});
		}
	}
}

bool FullscreenUI::OpenLoadStateSelectorForGameResume(const GameList::Entry* entry)
{
	SaveStateListEntry slentry;
	if (!InitializeSaveStateListEntry(&slentry, entry->title, entry->serial, entry->crc, -1))
		return false;

	CloseSaveStateSelector();
	s_save_state_selector_slots.push_back(std::move(slentry));
	s_save_state_selector_game_path = entry->path;
	s_save_state_selector_loading = true;
	s_save_state_selector_open = true;
	s_save_state_selector_resuming = true;
	return true;
}

void FullscreenUI::DrawResumeStateSelector()
{
	ImGui::SetNextWindowSize(LayoutScale(800.0f, 600.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::OpenPopup(FSUI_CSTR("Load Resume State"));

	ImGui::PushFont(g_large_font.first, g_large_font.second);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));

	bool is_open = true;
	if (ImGui::BeginPopupModal(FSUI_CSTR("Load Resume State"), &is_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
	{
		const SaveStateListEntry& entry = s_save_state_selector_slots.front();
		ImGui::TextWrapped(FSUI_CSTR("A resume save state created at %s was found.\n\nDo you want to load this save and continue?"),
			TimeToPrintableString(entry.timestamp).c_str());

		const GSTexture* image = entry.preview_texture ? entry.preview_texture.get() : GetPlaceholderTexture().get();
		const float image_height = LayoutScale(250.0f);
		const float image_width = image_height * (static_cast<float>(image->GetWidth()) / static_cast<float>(image->GetHeight()));
		const ImVec2 pos(ImGui::GetCursorScreenPos() +
						 ImVec2((ImGui::GetCurrentWindow()->WorkRect.GetWidth() - image_width) * 0.5f, LayoutScale(20.0f)));
		const ImRect image_bb(pos, pos + ImVec2(image_width, image_height));
		ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(entry.preview_texture ? entry.preview_texture->GetNativeHandle() :
																								   GetPlaceholderTexture()->GetNativeHandle()),
			image_bb.Min, image_bb.Max);

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + image_height + LayoutScale(40.0f));

		BeginMenuButtons();

		if (ActiveButton(FSUI_ICONSTR(ICON_FA_PLAY, "Load State"), false))
		{
			DoStartPath(s_save_state_selector_game_path, -1);
			is_open = false;
		}

		if (ActiveButton(FSUI_ICONSTR(ICON_FA_COMPACT_DISC, "Default Boot"), false))
		{
			DoStartPath(s_save_state_selector_game_path);
			is_open = false;
		}

		if (ActiveButton(FSUI_ICONSTR(ICON_FA_TRASH, "Delete State"), false))
		{
			if (FileSystem::DeleteFilePath(entry.path.c_str()))
			{
				DoStartPath(s_save_state_selector_game_path);
				is_open = false;
			}
			else
			{
				ShowToast(std::string(), FSUI_STR("Failed to delete save state."));
			}
		}

		if (ActiveButton(FSUI_ICONSTR(ICON_FA_SQUARE_XMARK, "Cancel"), false) || WantsToCloseMenu())
		{
			ImGui::CloseCurrentPopup();
			is_open = false;
		}
		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(2);
	ImGui::PopFont();

	if (!is_open)
	{
		ClearSaveStateEntryList();
		s_save_state_selector_open = false;
		s_save_state_selector_loading = false;
		s_save_state_selector_resuming = false;
		s_save_state_selector_game_path = {};
	}
	else
	{
		SetStandardSelectionFooterText(false);
	}
}

void FullscreenUI::DoLoadState(std::string path, std::optional<s32> slot, bool backup)
{
	std::string boot_path = s_save_state_selector_game_path;
	Host::RunOnCPUThread([boot_path = std::move(boot_path), path = std::move(path), slot, backup]() {
		if (VMManager::HasValidVM())
		{
			Error error;
			if (!VMManager::LoadState(path.c_str(), &error))
			{
				ReportStateLoadError(error.GetDescription(), slot, backup);
				return;
			}

			if (!boot_path.empty() && VMManager::GetDiscPath() != boot_path)
				VMManager::ChangeDisc(CDVD_SourceType::Iso, std::move(boot_path));
		}
		else
		{
			VMBootParameters params;
			params.filename = std::move(boot_path);
			params.save_state = std::move(path);
			DoVMInitialize(params, false);
		}
	});
}

void FullscreenUI::DoSaveState(s32 slot)
{
	Host::RunOnCPUThread([slot]() {
		VMManager::SaveStateToSlot(slot, true, [slot](const std::string& error) {
			ReportStateSaveError(error, slot);
		});
	});
}

void FullscreenUI::PopulateGameListEntryList()
{
	const int sort = Host::GetBaseIntSettingValue("UI", "FullscreenUIGameSort", 0);
	const bool reverse = Host::GetBaseBoolSettingValue("UI", "FullscreenUIGameSortReverse", false);

	const u32 count = GameList::GetEntryCount();
	s_game_list_sorted_entries.resize(count);
	for (u32 i = 0; i < count; i++)
		s_game_list_sorted_entries[i] = GameList::GetEntryByIndex(i);

	std::sort(s_game_list_sorted_entries.begin(), s_game_list_sorted_entries.end(),
		[sort, reverse](const GameList::Entry* lhs, const GameList::Entry* rhs) {
			switch (sort)
			{
				case 0: // Type
				{
					if (lhs->type != rhs->type)
						return reverse ? (lhs->type > rhs->type) : (lhs->type < rhs->type);
				}
				break;

				case 1: // Serial
				{
					if (lhs->serial != rhs->serial)
						return reverse ? (lhs->serial > rhs->serial) : (lhs->serial < rhs->serial);
				}
				break;

				case 2: // Title
					break;

				case 3: // File Title
				{
					const std::string_view lhs_title(Path::GetFileTitle(lhs->path));
					const std::string_view rhs_title(Path::GetFileTitle(rhs->path));
					const int res =
						StringUtil::Strncasecmp(lhs_title.data(), rhs_title.data(), std::min(lhs_title.size(), rhs_title.size()));
					if (res != 0)
						return reverse ? (res > 0) : (res < 0);
				}
				break;

				case 4: // CRC
				{
					if (lhs->crc != rhs->crc)
						return reverse ? (lhs->crc > rhs->crc) : (lhs->crc < rhs->crc);
				}
				break;

				case 5: // Time Played
				{
					if (lhs->total_played_time != rhs->total_played_time)
					{
						return reverse ? (lhs->total_played_time > rhs->total_played_time) :
										 (lhs->total_played_time < rhs->total_played_time);
					}
				}
				break;

				case 6: // Last Played (reversed by default)
				{
					if (lhs->last_played_time != rhs->last_played_time)
					{
						return reverse ? (lhs->last_played_time < rhs->last_played_time) : (lhs->last_played_time > rhs->last_played_time);
					}
				}
				break;

				case 7: // Size
				{
					if (lhs->total_size != rhs->total_size)
					{
						return reverse ? (lhs->total_size > rhs->total_size) : (lhs->total_size < rhs->total_size);
					}
				}
				break;
			}

			// fallback to title when all else is equal
			const int res = StringUtil::Strcasecmp(lhs->GetTitleSort(true).c_str(), rhs->GetTitleSort(true).c_str());
			return reverse ? (res > 0) : (res < 0);
		});
}

void FullscreenUI::DrawGameListWindow()
{
	auto game_list_lock = GameList::GetLock();
	PopulateGameListEntryList();

	ImGuiIO& io = ImGui::GetIO();
	const ImVec2 heading_size =
		ImVec2(io.DisplaySize.x, LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY) +
									 (LayoutScale(LAYOUT_MENU_BUTTON_Y_PADDING) * 2.0f) + LayoutScale(2.0f));

	const float bg_alpha = VMManager::HasValidVM() ? 0.90f : 1.0f;

	if (BeginFullscreenWindow(ImVec2(0.0f, 0.0f), heading_size, "gamelist_view", MulAlpha(UIPrimaryColor, bg_alpha)))
	{
		static constexpr float ITEM_WIDTH = 25.0f;
		static constexpr const char* icons[] = {ICON_FA_BORDER_ALL, ICON_FA_LIST};
		static constexpr const char* titles[] = {FSUI_NSTR("Game Grid"), FSUI_NSTR("Game List")};
		static constexpr u32 count = std::size(titles);

		BeginNavBar();

		if (NavButton(ICON_PF_BACKWARD, true, true))
			SwitchToLanding();

		NavTitle(Host::TranslateToCString(TR_CONTEXT, titles[static_cast<u32>(s_game_list_view)]));
		RightAlignNavButtons(count, ITEM_WIDTH, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

		for (u32 i = 0; i < count; i++)
		{
			if (NavButton(icons[i], static_cast<GameListView>(i) == s_game_list_view, true, ITEM_WIDTH,
					LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
			{
				s_game_list_view = static_cast<GameListView>(i);
			}
		}

		EndNavBar();
	}

	EndFullscreenWindow();

	if (ImGui::IsKeyPressed(ImGuiKey_NavGamepadInput, false) || ImGui::IsKeyPressed(ImGuiKey_F1, false))
	{
		s_game_list_view = (s_game_list_view == GameListView::Grid) ? GameListView::List : GameListView::Grid;
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false) || ImGui::IsKeyPressed(ImGuiKey_F2))
	{
		s_current_main_window = MainWindowType::GameListSettings;
		QueueResetFocus(FocusResetType::WindowChanged);
	}

	switch (s_game_list_view)
	{
		case GameListView::Grid:
			DrawGameGrid(heading_size);
			break;
		case GameListView::List:
			DrawGameList(heading_size);
			break;
		default:
			break;
	}

	if (VMManager::GetState() != VMState::Shutdown)
	{
		// Dummy window to prevent interacting with the game list while loading.
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowBgAlpha(0.25f);
		ImGui::Begin("##dummy", nullptr, ImGuiWindowFlags_NoDecoration);
		ImGui::End();
		ImGui::PopStyleColor();
	}

	if (IsGamepadInputSource())
	{
		const bool circleOK = ImGui::GetIO().ConfigNavSwapGamepadButtons;
		const bool swapNorthWest = ImGuiManager::IsGamepadNorthWestSwapped();
		SetFullscreenFooterText(std::array{
			std::make_pair(ICON_PF_DPAD, FSUI_VSTR("Select Game")),
			std::make_pair(ICON_PF_START, FSUI_VSTR("Settings")),
			std::make_pair(swapNorthWest ? ICON_PF_BUTTON_SQUARE : ICON_PF_BUTTON_TRIANGLE, FSUI_VSTR("Change View")),
			std::make_pair(swapNorthWest ? ICON_PF_BUTTON_TRIANGLE : ICON_PF_BUTTON_SQUARE, FSUI_VSTR("Launch Options")),
			std::make_pair(circleOK ? ICON_PF_BUTTON_CIRCLE : ICON_PF_BUTTON_CROSS, FSUI_VSTR("Start Game")),
			std::make_pair(circleOK ? ICON_PF_BUTTON_CROSS : ICON_PF_BUTTON_CIRCLE, FSUI_VSTR("Back")),
		});
	}
	else
	{
		SetFullscreenFooterText(std::array{
			std::make_pair(ICON_PF_ARROW_UP ICON_PF_ARROW_DOWN ICON_PF_ARROW_LEFT ICON_PF_ARROW_RIGHT, FSUI_VSTR("Select Game")),
			std::make_pair(ICON_PF_F1, FSUI_VSTR("Change View")),
			std::make_pair(ICON_PF_F2, FSUI_VSTR("Settings")),
			std::make_pair(ICON_PF_F3, FSUI_VSTR("Launch Options")),
			std::make_pair(ICON_PF_ENTER, FSUI_VSTR("Start Game")),
			std::make_pair(ICON_PF_ESC, FSUI_VSTR("Back")),
		});
	}
}

void FullscreenUI::DrawGameList(const ImVec2& heading_size)
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, UIBackgroundColor);

	if (!BeginFullscreenColumns(nullptr, heading_size.y, true, true))
	{
		EndFullscreenColumns();
		ImGui::PopStyleColor();
		return;
	}

	if (!AreAnyDialogsOpen() && WantsToCloseMenu())
		SwitchToLanding();

	const GameList::Entry* selected_entry = nullptr;

	if (BeginFullscreenColumnWindow(0.0f, -530.0f, "game_list_entries"))
	{
		const ImVec2 image_size(LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT * 0.68f, LAYOUT_MENU_BUTTON_HEIGHT));

		ResetFocusHere();

		BeginMenuButtons();

		// TODO: replace with something not heap allocating
		std::string summary;

		for (const GameList::Entry* entry : s_game_list_sorted_entries)
		{
			ImRect bb;
			bool visible, hovered;
			bool pressed = MenuButtonFrame(entry->path.c_str(), true, LAYOUT_MENU_BUTTON_HEIGHT, &visible, &hovered, &bb.Min, &bb.Max);
			if (!visible)
				continue;

			summary.clear();
			if (entry->serial.empty())
				fmt::format_to(std::back_inserter(summary), "{} - ", GameList::RegionToString(entry->region, true));
			else
				fmt::format_to(std::back_inserter(summary), "{} - {} - ", entry->serial, GameList::RegionToString(entry->region, true));

			const std::string_view filename(Path::GetFileName(entry->path));
			summary.append(filename);

			DrawGameCover(entry, ImGui::GetWindowDrawList(), bb.Min, bb.Min + image_size);

			const float midpoint = bb.Min.y + g_large_font.second + LayoutScale(4.0f);
			const float text_start_x = bb.Min.x + image_size.x + LayoutScale(15.0f);
			const ImRect title_bb(ImVec2(text_start_x, bb.Min.y), ImVec2(bb.Max.x, midpoint));
			const ImRect summary_bb(ImVec2(text_start_x, midpoint), bb.Max);

			ImGui::PushFont(g_large_font.first, g_large_font.second);
			// TODO: Fix font fallback issues and enable native-language titles
			ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, entry->GetTitle(true).c_str(), entry->GetTitle(true).c_str() + entry->GetTitle(true).size(), nullptr,
				ImVec2(0.0f, 0.0f), &title_bb);
			ImGui::PopFont();

			if (!summary.empty())
			{
				ImGui::PushFont(g_medium_font.first, g_medium_font.second);
				ImGui::RenderTextClipped(
					summary_bb.Min, summary_bb.Max, summary.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
				ImGui::PopFont();
			}

			if (pressed)
				HandleGameListActivate(entry);

			if (hovered)
				selected_entry = entry;

			if (selected_entry &&
				(ImGui::IsItemClicked(ImGuiMouseButton_Right) || ImGui::Shortcut(ImGuiKey_NavGamepadMenu) ||
					ImGui::Shortcut(ImGuiKey_F3)))
			{
				HandleGameListOptions(selected_entry);
			}
		}

		EndMenuButtons();
	}
	EndFullscreenColumnWindow();

	if (BeginFullscreenColumnWindow(-530.0f, 0.0f, "game_list_info", UIPrimaryDarkColor))
	{
		const float img_padding_y = LayoutScale(20.0f);
		// Spacing between each text item
		const float text_spacing_y = LayoutScale(8.0f);
		// Space between title/serial and details, is in addition to text_spacing_y
		const float title_padding_below_y = LayoutScale(12.0f);

		// Estimate how much space is needed for text
		// Do this even when nothing is selected, to ensure cover/icon is in a consistant size/position
		const float title_detail_height =
			LayoutScale(LAYOUT_LARGE_FONT_SIZE) + text_spacing_y + // Title
			LayoutScale(LAYOUT_MEDIUM_FONT_SIZE) + text_spacing_y + // Serial
			title_padding_below_y +
			7.0f * (LayoutScale(LAYOUT_MEDIUM_FONT_SIZE) + text_spacing_y) + // File, CRC, Region, Compat, Time/Last Played, Size
			LayoutScale(12.0f); // Extra padding

		// Limit cover height to avoid pushing text off the screen
		const ImGuiWindow* window = ImGui::GetCurrentWindow();
		// Based on ImGui code for WorkRect, with scrolling logic removed
		const float window_height = std::trunc(window->InnerRect.GetHeight() - 2.0f * std::max(window->WindowPadding.y, window->WindowBorderSize));

		const float free_height = window_height - title_detail_height;
		const float img_height = std::min(free_height - 2.0f * img_padding_y, LayoutScale(400.0f));

		const ImVec2 image_size = ImVec2(LayoutScale(275.0f), img_height);
		ImGui::SetCursorPos(ImVec2(LayoutScale(128.0f), img_padding_y));

		if (selected_entry)
			DrawGameCover(selected_entry, image_size);
		else
			DrawFallbackCover(image_size);

		const float work_width = window->WorkRect.GetWidth();
		const float start_x = LayoutScale(50.0f);
		const float text_y = img_height + 2.0f * img_padding_y;
		float text_width;

		PushPrimaryColor();
		ImGui::SetCursorPos(ImVec2(start_x, text_y));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, text_spacing_y));
		ImGui::PushTextWrapPos(LayoutScale(490.0f));
		ImGui::BeginGroup();

		if (selected_entry)
		{
			// title
			ImGui::PushFont(g_large_font.first, g_large_font.second);
			const std::string_view title(std::string_view(selected_entry->GetTitle(true)).substr(0, 37));
			text_width = ImGui::CalcTextSize(title.data(), title.data() + title.length(), false, work_width).x;
			if (title.length() != selected_entry->GetTitle(true).length())
				text_width += ImGui::CalcTextSize("...", nullptr, false, -1.0f).x;
			ImGui::SetCursorPosX((work_width - text_width) / 2.0f);
			ImGui::TextWrapped(
				"%.*s%s", static_cast<int>(title.size()), title.data(), (title.length() == selected_entry->GetTitle(true).length()) ? "" : "...");
			ImGui::PopFont();

			ImGui::PushFont(g_medium_font.first, g_medium_font.second);

			// code
			text_width = ImGui::CalcTextSize(selected_entry->serial.c_str(), nullptr, false, work_width).x;
			ImGui::SetCursorPosX((work_width - text_width) / 2.0f);
			ImGui::TextWrapped("%s", selected_entry->serial.c_str());
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + title_padding_below_y);

			// file tile
			ImGui::TextWrapped("%s", SmallString::from_format(FSUI_FSTR("File: {}"), Path::GetFileName(selected_entry->path)).c_str());

			// crc
			ImGui::TextUnformatted(TinyString::from_format(FSUI_FSTR("CRC: {:08X}"), selected_entry->crc));

			// region
			{
				std::string flag_texture(fmt::format("icons/flags/{}.svg", GameList::RegionToFlagFilename(selected_entry->region)));
				ImGui::TextUnformatted(FSUI_CSTR("Region: "));
				ImGui::SameLine();
				DrawCachedSvgTextureAsync(flag_texture, LayoutScale(23.0f, 16.0f), SvgScaling::Fit);
				ImGui::SameLine();
				ImGui::Text(" (%s)", GameList::RegionToString(selected_entry->region, true));
			}

			// compatibility
			ImGui::TextUnformatted(FSUI_CSTR("Compatibility: "));
			ImGui::SameLine();
			if (selected_entry->compatibility_rating != GameDatabaseSchema::Compatibility::Unknown)
			{
				DrawSvgTexture(s_game_compatibility_textures[static_cast<u32>(selected_entry->compatibility_rating) - 1].get(), LayoutScale(64.0f, 16.0f));
				ImGui::SameLine();
			}
			ImGui::Text(" (%s)", GameList::EntryCompatibilityRatingToString(selected_entry->compatibility_rating, true));

			// play time
			ImGui::TextUnformatted(
				SmallString::from_format(FSUI_FSTR("Time Played: {}"), GameList::FormatTimespan(selected_entry->total_played_time)));
			ImGui::TextUnformatted(
				SmallString::from_format(FSUI_FSTR("Last Played: {}"), GameList::FormatTimestamp(selected_entry->last_played_time)));

			// size
			ImGui::TextUnformatted(
				SmallString::from_format(FSUI_FSTR("Size: {:.2f} MB"), static_cast<float>(selected_entry->total_size) / 1048576.0f));

			ImGui::PopFont();
		}
		else
		{
			// title
			const char* title = FSUI_CSTR("No Game Selected");
			ImGui::PushFont(g_large_font.first, g_large_font.second);
			text_width = ImGui::CalcTextSize(title, nullptr, false, work_width).x;
			ImGui::SetCursorPosX((work_width - text_width) / 2.0f);
			ImGui::TextWrapped("%s", title);
			ImGui::PopFont();
		}

		ImGui::EndGroup();
		ImGui::PopTextWrapPos();
		ImGui::PopStyleVar();
		PopPrimaryColor();
	}
	EndFullscreenColumnWindow();
	EndFullscreenColumns();

	ImGui::PopStyleColor();
}

void FullscreenUI::DrawGameGrid(const ImVec2& heading_size)
{
	ImGuiIO& io = ImGui::GetIO();
	if (!BeginFullscreenWindow(
			ImVec2(0.0f, heading_size.y),
			ImVec2(io.DisplaySize.x, io.DisplaySize.y - heading_size.y - LayoutScale(LAYOUT_FOOTER_HEIGHT)), "game_grid",
			UIBackgroundColor))
	{
		EndFullscreenWindow();
		return;
	}

	if (!AreAnyDialogsOpen() && WantsToCloseMenu())
		SwitchToLanding();

	ResetFocusHere();
	BeginMenuButtons();

	const ImGuiStyle& style = ImGui::GetStyle();

	const float title_spacing = LayoutScale(10.0f);
	const float item_spacing = LayoutScale(20.0f);
	const float item_width_with_spacing = std::floor(LayoutScale(LAYOUT_SCREEN_WIDTH / 5.0f));
	const float item_width = item_width_with_spacing - item_spacing;
	const float image_width = item_width - (style.FramePadding.x * 2.0f);
	const float image_height = image_width * 1.33f;
	const ImVec2 image_size(image_width, image_height);
	const float item_height = (style.FramePadding.y * 2.0f) + image_height + title_spacing + g_medium_font.second;
	const ImVec2 item_size(item_width, item_height);
	const u32 grid_count_x = std::floor(ImGui::GetWindowWidth() / item_width_with_spacing);
	const float start_x =
		(static_cast<float>(ImGui::GetWindowWidth()) - (item_width_with_spacing * static_cast<float>(grid_count_x))) * 0.5f;

	SmallString draw_title;

	u32 grid_x = 0;
	for (const GameList::Entry* entry : s_game_list_sorted_entries)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			continue;

		if (grid_x == grid_count_x)
		{
			grid_x = 0;
			ImGui::SetCursorPosX(start_x);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + item_spacing);
		}
		else
		{
			ImGui::SameLine(start_x + static_cast<float>(grid_x) * (item_width + item_spacing));
		}

		const ImGuiID id = window->GetID(entry->path.c_str(), entry->path.c_str() + entry->path.length());
		const ImVec2 pos(window->DC.CursorPos);
		ImRect bb(pos, pos + item_size);
		ImGui::ItemSize(item_size);
		if (ImGui::ItemAdd(bb, id))
		{
			bool held;
			bool hovered;
			bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);
			if (hovered)
			{
				const ImU32 col = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered, 1.0f);

				const float t = std::min<float>(std::abs(std::sin(ImGui::GetTime() * 0.75) * 1.1), 1.0f);
				ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_Border, t));

				ImGuiFullscreen::DrawMenuButtonFrame(bb.Min, bb.Max, col, true, 0.0f);

				ImGui::PopStyleColor();
			}

			bb.Min += style.FramePadding;
			bb.Max -= style.FramePadding;

			DrawGameCover(entry, ImGui::GetWindowDrawList(), bb.Min, bb.Min + image_size);

			const bool show_titles = Host::GetBaseBoolSettingValue("UI", "FullscreenUIShowGameGridTitles", true);

			if (show_titles)
			{
				const ImRect title_bb(ImVec2(bb.Min.x, bb.Min.y + image_height + title_spacing), bb.Max);
				const std::string_view title(std::string_view(entry->GetTitle(true)).substr(0, 31));
				draw_title.clear();
				fmt::format_to(std::back_inserter(draw_title), "{}{}", title, (title.length() == entry->GetTitle(true).length()) ? "" : "...");
				ImGui::PushFont(g_medium_font.first, g_medium_font.second);
				ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, draw_title.c_str(), draw_title.c_str() + draw_title.length(), nullptr,
					ImVec2(0.5f, 0.0f), &title_bb);
				ImGui::PopFont();
			}

			if (pressed)
			{
				HandleGameListActivate(entry);
			}
			else if (hovered && (ImGui::IsItemClicked(ImGuiMouseButton_Right) || ImGui::Shortcut(ImGuiKey_NavGamepadMenu) ||
									ImGui::Shortcut(ImGuiKey_F3)))
			{
				HandleGameListOptions(entry);
			}
		}

		grid_x++;
	}

	EndMenuButtons();
	EndFullscreenWindow();
}

void FullscreenUI::HandleGameListActivate(const GameList::Entry* entry)
{
	// launch game
	if (!OpenLoadStateSelectorForGameResume(entry))
		DoStartPath(entry->path);
}

void FullscreenUI::HandleGameListOptions(const GameList::Entry* entry)
{
	ImGuiFullscreen::ChoiceDialogOptions options = {
		{FSUI_ICONSTR(ICON_FA_WRENCH, "Game Properties"), false},
		{FSUI_ICONSTR(ICON_FA_PLAY, "Resume Game"), false},
		{FSUI_ICONSTR(ICON_FA_ARROW_ROTATE_LEFT, "Load State"), false},
		{FSUI_ICONSTR(ICON_PF_STAR, "Default Boot"), false},
		{FSUI_ICONSTR(ICON_FA_FORWARD_FAST, "Fast Boot"), false},
		{FSUI_ICONSTR(ICON_FA_COMPACT_DISC, "Full Boot"), false},
	};

	const time_t entry_played_time = GameList::GetCachedPlayedTimeForSerial(entry->serial);
	if (entry_played_time)
		options.emplace_back(FSUI_ICONSTR(ICON_FA_STOPWATCH, "Reset Play Time"), false);
	options.emplace_back(FSUI_ICONSTR(ICON_FA_SQUARE_XMARK, "Close Menu"), false);

	const bool has_resume_state = VMManager::HasSaveStateInSlot(entry->serial.c_str(), entry->crc, -1);
	OpenChoiceDialog(entry->GetTitle(true).c_str(), false, std::move(options),
		[has_resume_state, entry_path = entry->path, entry_serial = entry->serial, entry_title = entry->title, entry_played_time]
		(s32 index, const std::string& title, bool checked) {
			switch (index)
			{
				case 0: // Open Game Properties
					SwitchToGameSettings(entry_path);
					break;
				case 1: // Resume Game
					DoStartPath(entry_path, has_resume_state ? std::optional<s32>(-1) : std::optional<s32>());
					break;
				case 2: // Load State
					OpenLoadStateSelectorForGame(entry_path);
					break;
				case 3: // Default Boot
					DoStartPath(entry_path);
					break;
				case 4: // Fast Boot
					DoStartPath(entry_path, std::nullopt, true);
					break;
				case 5: // Full Boot
					DoStartPath(entry_path, std::nullopt, false);
					break;
				case 6:
					{
						// Close Menu
						if (!entry_played_time)
							break;

						// Reset Play Time
						OpenConfirmMessageDialog(FSUI_ICONSTR(ICON_FA_STOPWATCH, "Confirm Reset"),
							fmt::format(FSUI_FSTR("Are you sure you want to reset the play time for '{}' ({})?\n\n"
												  "Your current play time is {}.\n\nThis action cannot be undone."),
											entry_title.empty() ? FSUI_STR("empty title") : entry_title,
											entry_serial.empty() ? FSUI_STR("no serial") : entry_serial,
											GameList::FormatTimespan(entry_played_time, true)),
							[entry_serial](bool result) {
								if (result)
									GameList::ClearPlayedTimeForSerial(entry_serial);
							}, false);
					}
					break;
				default: // Close Menu
					break;
			}

			CloseChoiceDialog();
		});
}

void FullscreenUI::DrawGameListSettingsWindow()
{
	ImGuiIO& io = ImGui::GetIO();
	const ImVec2 heading_size =
		ImVec2(io.DisplaySize.x, LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY) +
									 (LayoutScale(LAYOUT_MENU_BUTTON_Y_PADDING) * 2.0f) + LayoutScale(2.0f));

	const float bg_alpha = VMManager::HasValidVM() ? 0.90f : 1.0f;

	if (BeginFullscreenWindow(ImVec2(0.0f, 0.0f), heading_size, "gamelist_view", MulAlpha(UIPrimaryColor, bg_alpha)))
	{
		BeginNavBar();

		if (NavButton(ICON_PF_BACKWARD, true, true))
		{
			s_current_main_window = MainWindowType::GameList;
			QueueResetFocus(FocusResetType::WindowChanged);
		}

		NavTitle(FSUI_CSTR("Game List Settings"));
		EndNavBar();
	}

	EndFullscreenWindow();

	if (!BeginFullscreenWindow(
			ImVec2(0.0f, heading_size.y),
			ImVec2(io.DisplaySize.x, io.DisplaySize.y - heading_size.y - LayoutScale(LAYOUT_FOOTER_HEIGHT)),
			"settings_parent", UIBackgroundColor, 0.0f, ImVec2(ImGuiFullscreen::LAYOUT_MENU_WINDOW_X_PADDING, 0.0f)))
	{
		EndFullscreenWindow();
		return;
	}

	if (ImGui::IsWindowFocused() && WantsToCloseMenu())
	{
		s_current_main_window = MainWindowType::GameList;
		QueueResetFocus(FocusResetType::WindowChanged);
	}

	auto lock = Host::GetSettingsLock();
	SettingsInterface* bsi = GetEditingSettingsInterface(false);

	BeginMenuButtons();

	MenuHeading(FSUI_CSTR("Search Directories"));
	if (MenuButton(FSUI_ICONSTR(ICON_FA_FOLDER_PLUS, "Add Search Directory"), FSUI_CSTR("Adds a new directory to the game search list.")))
	{
		OpenFileSelector(FSUI_ICONSTR(ICON_FA_FOLDER_PLUS, "Add Search Directory"), true, [](const std::string& dir) {
			if (!dir.empty())
			{
				auto lock = Host::GetSettingsLock();
				SettingsInterface* bsi = Host::Internal::GetBaseSettingsLayer();

				bsi->AddToStringList("GameList", "RecursivePaths", dir.c_str());
				bsi->RemoveFromStringList("GameList", "Paths", dir.c_str());
				SetSettingsChanged(bsi);
				PopulateGameListDirectoryCache(bsi);
				Host::RefreshGameListAsync(false);
			}

			CloseFileSelector();
		});
	}

	for (const auto& it : s_game_list_directories_cache)
	{
		if (MenuButton(it.first.c_str(), it.second ? FSUI_CSTR("Scanning Subdirectories") : FSUI_CSTR("Not Scanning Subdirectories")))
		{
			ImGuiFullscreen::ChoiceDialogOptions options = {
				{FSUI_ICONSTR(ICON_FA_FOLDER_OPEN, "Open in File Browser"), false},
				{it.second ? (FSUI_ICONSTR(ICON_FA_FOLDER_MINUS, "Disable Subdirectory Scanning")) :
							 (FSUI_ICONSTR(ICON_FA_FOLDER_PLUS, "Enable Subdirectory Scanning")),
					false},
				{FSUI_ICONSTR(ICON_FA_TRASH, "Remove From List"), false},
				{FSUI_ICONSTR(ICON_FA_SQUARE_XMARK, "Close Menu"), false},
			};

			OpenChoiceDialog(SmallString::from_format(ICON_FA_FOLDER " {}", it.first).c_str(), false, std::move(options),
				[dir = it.first, recursive = it.second](s32 index, const std::string& title, bool checked) {
					if (index < 0)
						return;

					if (index == 0)
					{
						// Open In File Browser.
						ExitFullscreenAndOpenURL(Path::CreateFileURL(dir));
					}
					else if (index == 1)
					{
						// Toggle Subdirectory Scanning.
						{
							auto lock = Host::GetSettingsLock();
							SettingsInterface* bsi = Host::Internal::GetBaseSettingsLayer();
							if (!recursive)
							{
								bsi->RemoveFromStringList("GameList", "Paths", dir.c_str());
								bsi->AddToStringList("GameList", "RecursivePaths", dir.c_str());
							}
							else
							{
								bsi->RemoveFromStringList("GameList", "RecursivePaths", dir.c_str());
								bsi->AddToStringList("GameList", "Paths", dir.c_str());
							}

							SetSettingsChanged(bsi);
							PopulateGameListDirectoryCache(bsi);
						}

						Host::RefreshGameListAsync(false);
					}
					else if (index == 2)
					{
						// Remove From List.
						auto lock = Host::GetSettingsLock();
						SettingsInterface* bsi = Host::Internal::GetBaseSettingsLayer();
						bsi->RemoveFromStringList("GameList", "Paths", dir.c_str());
						bsi->RemoveFromStringList("GameList", "RecursivePaths", dir.c_str());
						SetSettingsChanged(bsi);
						PopulateGameListDirectoryCache(bsi);
						Host::RefreshGameListAsync(false);
					}

					CloseChoiceDialog();
				});
		}
	}

	static constexpr const char* view_types[] = {
		FSUI_NSTR("Game Grid"),
		FSUI_NSTR("Game List"),
	};
	static constexpr const char* sort_types[] = {
		FSUI_NSTR("Type"),
		FSUI_NSTR("Serial"),
		FSUI_NSTR("Title"),
		FSUI_NSTR("File Title"),
		FSUI_NSTR("CRC"),
		FSUI_NSTR("Time Played"),
		FSUI_NSTR("Last Played"),
		FSUI_NSTR("Size"),
	};

	MenuHeading(FSUI_CSTR("List Settings"));
	{
		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_BORDER_ALL, "Default View"), FSUI_CSTR("Sets which view the game list will open to."),
			"UI", "DefaultFullscreenUIGameView", 0, view_types, std::size(view_types), true);
		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_SORT, "Sort By"), FSUI_CSTR("Determines which field the game list will be sorted by."),
			"UI", "FullscreenUIGameSort", 0, sort_types, std::size(sort_types), true);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_ARROW_DOWN_A_Z, "Sort Reversed"),
			FSUI_CSTR("Reverses the game list sort order from the default (usually ascending to descending)."), "UI",
			"FullscreenUIGameSortReverse", false);
		DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_TAG, "Show Titles"),
			FSUI_CSTR("Shows Titles for Games when in Game Grid View Mode"), "UI",
			"FullscreenUIShowGameGridTitles", true);
	}

	MenuHeading(FSUI_CSTR("Cover Settings"));
	{
		DrawFolderSetting(bsi, FSUI_ICONSTR(ICON_FA_FOLDER, "Covers Directory"), "Folders", "Covers", EmuFolders::Covers);
		if (MenuButton(
				FSUI_ICONSTR(ICON_FA_DOWNLOAD, "Download Covers"), FSUI_CSTR("Downloads covers from a user-specified URL template.")))
		{
			Host::OnCoverDownloaderOpenRequested();
		}
	}

	MenuHeading(FSUI_CSTR("Operations"));
	{
		if (MenuButton(
				FSUI_ICONSTR(ICON_FA_MAGNIFYING_GLASS, "Scan For New Games"), FSUI_CSTR("Identifies any new files added to the game directories.")))
		{
			Host::RefreshGameListAsync(false);
		}
		if (MenuButton(FSUI_ICONSTR(ICON_FA_ARROW_ROTATE_RIGHT, "Rescan All Games"),
				FSUI_CSTR("Forces a full rescan of all games previously identified.")))
		{
			Host::RefreshGameListAsync(true);
		}
	}

	EndMenuButtons();

	EndFullscreenWindow();

	SetStandardSelectionFooterText(true);
}

void FullscreenUI::SwitchToGameList()
{
	s_current_main_window = MainWindowType::GameList;
	s_game_list_view = static_cast<GameListView>(Host::GetBaseIntSettingValue("UI", "DefaultFullscreenUIGameView", 0));
	{
		auto lock = Host::GetSettingsLock();
		PopulateGameListDirectoryCache(Host::Internal::GetBaseSettingsLayer());
	}
	QueueResetFocus(FocusResetType::WindowChanged);
}

GSTexture* FullscreenUI::GetGameListCover(const GameList::Entry* entry)
{
	// lookup and grab cover image
	auto cover_it = s_cover_image_map.find(entry->path);
	if (cover_it == s_cover_image_map.end())
	{
		std::string cover_path(GameList::GetCoverImagePathForEntry(entry));
		cover_it = s_cover_image_map.emplace(entry->path, std::move(cover_path)).first;
	}

	return (!cover_it->second.empty()) ? GetCachedTextureAsync(cover_it->second.c_str()) : nullptr;
}

GSTexture* FullscreenUI::GetTextureForGameListEntryType(GameList::EntryType type, const ImVec2& size, SvgScaling mode)
{
	switch (type)
	{
		case GameList::EntryType::ELF:
			return GetCachedSvgTexture("fullscreenui/applications-system.svg", size, mode);

		case GameList::EntryType::PS1Disc:
		case GameList::EntryType::PS2Disc:
		default:
			return GetCachedSvgTexture("fullscreenui/media-cdrom.svg", size, mode);
	}
}

void FullscreenUI::DrawGameCover(const GameList::Entry* entry, const ImVec2& size)
{
	// Used in DrawGameList (selected preview)
	const GSTexture* cover_texture = GetGameListCover(entry);

	pxAssert(ImGui::GetCurrentContext()->Style.ImageBorderSize == 0);
	const ImVec2 origin = ImGui::GetCursorPos();

	if (cover_texture)
	{
		const ImRect image_rect(CenterImage(size,
			ImVec2(static_cast<float>(cover_texture->GetWidth()), static_cast<float>(cover_texture->GetHeight()))));

		ImGui::SetCursorPos(origin + image_rect.Min);
		ImGui::Image(reinterpret_cast<ImTextureID>(cover_texture->GetNativeHandle()), image_rect.GetSize());
	}
	else
	{
		const float min_size = std::min(size.x, size.y);
		const ImVec2 image_square(min_size, min_size);
		GSTexture* const icon_texture = GetTextureForGameListEntryType(entry->type, image_square);

		const ImRect image_rect(CenterImage(size, image_square));

		ImGui::SetCursorPos(origin + image_rect.Min);
		DrawSvgTexture(icon_texture, image_square);
	}
	// Pretend the image we drew was the the size passed to us
	ImGui::SetCursorPos(origin);
	ImGui::Dummy(size);
}

void FullscreenUI::DrawGameCover(const GameList::Entry* entry, ImDrawList* draw_list, const ImVec2& min, const ImVec2& max)
{
	// Used in DrawPauseMenu, DrawGameList (list item), DrawGameGrid
	const GSTexture* cover_texture = GetGameListCover(entry);

	if (cover_texture)
	{
		const ImRect image_rect(CenterImage(ImRect(min, max),
			ImVec2(static_cast<float>(cover_texture->GetWidth()), static_cast<float>(cover_texture->GetHeight()))));

		draw_list->AddImage(reinterpret_cast<ImTextureID>(cover_texture->GetNativeHandle()),
			image_rect.Min, image_rect.Max);
	}
	else
	{
		const float min_size = std::min(max.x - min.x, max.y - min.y);
		const ImVec2 image_square(min_size, min_size);

		const ImRect image_rect(CenterImage(ImRect(min, max), image_square));

		DrawListSvgTexture(draw_list, GetTextureForGameListEntryType(entry->type, image_square, SvgScaling::Fit),
			image_rect.Min, image_rect.Max);
	}
}

void FullscreenUI::DrawFallbackCover(const ImVec2& size)
{
	pxAssert(ImGui::GetCurrentContext()->Style.ImageBorderSize == 0);
	const ImVec2 origin = ImGui::GetCursorPos();

	const float min_size = std::min(size.x, size.y);
	const ImVec2 image_square(min_size, min_size);
	GSTexture* const icon_texture = GetTextureForGameListEntryType(GameList::EntryType::PS2Disc, image_square);

	const ImRect image_rect(CenterImage(size, image_square));

	ImGui::SetCursorPos(origin + image_rect.Min);
	DrawSvgTexture(icon_texture, image_square);

	// Pretend the image we drew was the the size passed to us
	ImGui::SetCursorPos(origin);
	ImGui::Dummy(size);
}

void FullscreenUI::DrawFallbackCover(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max)
{
	const float min_size = std::min(max.x - min.x, max.y - min.y);
	const ImVec2 image_square(min_size, min_size);

	const ImRect image_rect(CenterImage(ImRect(min, max), image_square));

	DrawListSvgTexture(draw_list, GetTextureForGameListEntryType(GameList::EntryType::PS2Disc, image_square, SvgScaling::Fit),
		image_rect.Min, image_rect.Max);
}

//////////////////////////////////////////////////////////////////////////
// Overlays
//////////////////////////////////////////////////////////////////////////

void FullscreenUI::ExitFullscreenAndOpenURL(const std::string_view url)
{
	Host::RunOnCPUThread([url = std::string(url)]() {
		if (Host::IsFullscreen())
			Host::SetFullscreen(false);

		Host::OpenURL(url);
	});
}

void FullscreenUI::CopyTextToClipboard(std::string title, const std::string_view text)
{
	if (Host::CopyTextToClipboard(text))
		ShowToast(std::string(), std::move(title));
	else
		ShowToast(std::string(), FSUI_STR("Failed to copy text to clipboard."));
}

void FullscreenUI::OpenAboutWindow()
{
	s_about_window_open = true;
}

void FullscreenUI::DrawAboutWindow()
{
	ImGui::SetNextWindowSize(LayoutScale(1000.0f, 600.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::OpenPopup(FSUI_CSTR("About PCSX2"));

	ImGui::PushFont(g_large_font.first, g_large_font.second);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(30.0f, 30.0f));

	if (ImGui::BeginPopupModal(FSUI_CSTR("About PCSX2"), &s_about_window_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
	{
		const ImVec2 image_size = LayoutScale(500.0f, 76.0f);
		const ImRect image_bb(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImVec2(ImGui::GetCurrentWindow()->WorkRect.GetWidth(), image_size.y));
		const ImRect image_rect(CenterImage(image_bb, image_size));

		DrawListSvgTexture(ImGui::GetWindowDrawList(), s_banner_texture.get(), image_rect.Min, image_rect.Max);

		const float indent = image_size.y + LayoutScale(12.0f);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + indent);
		ImGui::TextWrapped("%s", FSUI_CSTR(
									 "PCSX2 is a free and open-source PlayStation 2 (PS2) emulator. Its purpose is to emulate the PS2's hardware, using a "
									 "combination of MIPS CPU Interpreters, Recompilers and a Virtual Machine which manages hardware states and PS2 system memory. "
									 "This allows you to play PS2 games on your PC, with many additional features and benefits."));
		ImGui::NewLine();

		ImGui::TextWrapped("%s",
			FSUI_CSTR("PlayStation 2 and PS2 are registered trademarks of Sony Interactive Entertainment. This application is not "
					  "affiliated in any way with Sony Interactive Entertainment."));

		BeginMenuButtons();

		if (ActiveButton(FSUI_ICONSTR(ICON_FA_GLOBE, "Website"), false))
			ExitFullscreenAndOpenURL(PCSX2_WEBSITE_URL);

		if (ActiveButton(FSUI_ICONSTR(ICON_FA_PERSON_BOOTH, "Support Forums"), false))
			ExitFullscreenAndOpenURL(PCSX2_FORUMS_URL);

		if (ActiveButton(FSUI_ICONSTR(ICON_FA_BUG, "GitHub Repository"), false))
			ExitFullscreenAndOpenURL(PCSX2_GITHUB_URL);

		if (ActiveButton(FSUI_ICONSTR(ICON_FA_NEWSPAPER, "License"), false))
			ExitFullscreenAndOpenURL(PCSX2_LICENSE_URL);

		if (ActiveButton(FSUI_ICONSTR(ICON_FA_SQUARE_XMARK, "Close"), false) || WantsToCloseMenu())
		{
			ImGui::CloseCurrentPopup();
			s_about_window_open = false;
		}
		else
		{
			SetStandardSelectionFooterText(true);
		}

		EndMenuButtons();

		const float alignment = image_size.x + image_size.y;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + alignment);
		ImGui::TextWrapped(FSUI_CSTR("Version: %s"), BuildVersion::GitRev);

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(2);
	ImGui::PopFont();
}

bool FullscreenUI::OpenAchievementsWindow()
{
	if (!VMManager::HasValidVM() || !Achievements::IsActive())
		return false;

	MTGS::RunOnGSThread([]() {
		if (!ImGuiManager::InitializeFullscreenUI())
			return;

		SwitchToAchievementsWindow();
	});

	return true;
}

void FullscreenUI::DrawAchievementsLoginWindow()
{
	ImGui::SetNextWindowSize(LayoutScale(400.0f, 330.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(12.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(24.0f, 24.0f));
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.13f, 0.13f, 0.13f, 0.95f));

	if (ImGui::BeginPopupModal("RetroAchievements", &s_achievements_login_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
	{
		const float content_width = ImGui::GetContentRegionAvail().x;

		ImGui::PushFont(g_large_font.first, g_large_font.second);

		const float icon_height = LayoutScale(24.0f);
		const float icon_width = icon_height * (500.0f / 275.0f);
		GSTexture* ra_icon = GetCachedSvgTextureAsync("icons/ra-icon.svg", ImVec2(icon_width, icon_height));
		const float title_width = ImGui::CalcTextSize("RetroAchievements").x;
		const float header_width = (ra_icon ? icon_width + LayoutScale(10.0f) : 0.0f) + title_width;
		const float header_start = (content_width - header_width) * 0.5f;

		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + header_start);

		if (ra_icon)
		{
			ImGui::Image(reinterpret_cast<ImTextureID>(ra_icon->GetNativeHandle()),
				ImVec2(icon_width, icon_height));
			ImGui::SameLine();
		}

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(1.0f));
		ImGui::TextUnformatted(FSUI_CSTR("RetroAchievements"));
		ImGui::PopFont();

		ImGui::Spacing();
		ImGui::Spacing();

		ImGui::PushTextWrapPos(content_width);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
		ImGui::TextWrapped("%s", FSUI_CSTR("Please enter your user name and password for retroachievements.org below.\n\nYour password will not be saved in PCSX2, an access token will be generated and used instead."));
		ImGui::PopStyleColor();
		ImGui::PopTextWrapPos();

		ImGui::Spacing();
		ImGui::Spacing();

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, LayoutScale(8.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(12.0f, 10.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, LayoutScale(1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

		if (s_achievements_login_logging_in)
			ImGui::BeginDisabled();

		ImGui::SetNextItemWidth(content_width);
		ImGui::InputTextWithHint("##username", FSUI_CSTR("Username"), s_achievements_login_username, sizeof(s_achievements_login_username));

		ImGui::Spacing();

		ImGui::SetNextItemWidth(content_width);
		ImGui::InputTextWithHint("##password", FSUI_CSTR("Password"), s_achievements_login_password, sizeof(s_achievements_login_password), ImGuiInputTextFlags_Password);

		ImGui::PopStyleColor(5);
		ImGui::PopStyleVar(3);

		if (s_achievements_login_logging_in)
			ImGui::EndDisabled();

		ImGui::Spacing();
		ImGui::Spacing();

		if (s_achievements_login_logging_in)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
			const float status_width = ImGui::CalcTextSize("Logging in...").x;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (content_width - status_width) * 0.5f);
			ImGui::TextUnformatted(FSUI_CSTR("Logging in..."));
			ImGui::PopStyleColor();
			ImGui::Spacing();
		}

		const float button_height = LayoutScale(36.0f);
		const float button_width = LayoutScale(100.0f);
		const float button_spacing = LayoutScale(12.0f);
		const float total_width = (button_width * 2) + button_spacing;
		const float start_x = (content_width - total_width) * 0.5f;

		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + start_x);

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, LayoutScale(8.0f));

		const bool can_login = !s_achievements_login_logging_in &&
							   strlen(s_achievements_login_username) > 0 &&
							   strlen(s_achievements_login_password) > 0;

		if (can_login)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.4f, 0.8f, 1.0f));
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
		}

		if (ImGui::Button(FSUI_CSTR("Login"), ImVec2(button_width, button_height)) && can_login)
		{
			s_achievements_login_logging_in = true;

			Host::RunOnCPUThread([username = std::string(s_achievements_login_username),
									 password = std::string(s_achievements_login_password)]() {
				Error error;
				const bool result = Achievements::Login(username.c_str(), password.c_str(), &error);

				s_achievements_login_logging_in = false;

				if (!result)
				{
					ShowToast(std::string(), fmt::format(FSUI_FSTR("Login failed.\nError: {}\n\nPlease check your username and password, and try again."),
									 error.GetDescription()));
					return;
				}

				ImGui::CloseCurrentPopup();
				s_achievements_login_open = false;

				s_achievements_login_username[0] = '\0';
				s_achievements_login_password[0] = '\0';

				if (s_achievements_login_reason == Achievements::LoginRequestReason::UserInitiated)
				{
					if (!Host::GetBaseBoolSettingValue("Achievements", "Enabled", false))
					{
						OpenConfirmMessageDialog(FSUI_STR("Enable Achievements"),
							FSUI_STR("Achievement tracking is not currently enabled. Your login will have no effect until "
									 "after tracking is enabled.\n\nDo you want to enable tracking now?"),
							[](bool result) {
								if (result)
								{
									Host::SetBaseBoolSettingValue("Achievements", "Enabled", true);
									Host::CommitBaseSettingChanges();
									VMManager::ApplySettings();
								}
							});
					}

					if (!Host::GetBaseBoolSettingValue("Achievements", "ChallengeMode", false))
					{
						OpenConfirmMessageDialog(FSUI_STR("Enable Hardcore Mode"),
							FSUI_STR("Hardcore mode is not currently enabled. Enabling hardcore mode allows you to set times, scores, and "
									 "participate in game-specific leaderboards.\n\nHowever, hardcore mode also prevents the usage of save "
									 "states, cheats and slowdown functionality.\n\nDo you want to enable hardcore mode?"),
							[](bool result) {
								if (result)
								{
									Host::SetBaseBoolSettingValue("Achievements", "ChallengeMode", true);
									Host::CommitBaseSettingChanges();
									VMManager::ApplySettings();

									bool has_active_game;
									{
										auto lock = Achievements::GetLock();
										has_active_game = Achievements::HasActiveGame();
									}

									if (has_active_game)
									{
										OpenConfirmMessageDialog(FSUI_STR("Reset System"),
											FSUI_STR("Hardcore mode will not be enabled until the system is reset. Do you want to reset the system now?"),
											[](bool reset) {
												if (reset && VMManager::HasValidVM())
													RequestReset();
											});
									}
								}
							});
					}
				}
			});
		}

		ImGui::PopStyleColor(3);

		ImGui::SameLine(0, button_spacing);

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));

		if (ImGui::Button(FSUI_CSTR("Cancel"), ImVec2(button_width, button_height)) && !s_achievements_login_logging_in)
		{
			if (s_achievements_login_reason == Achievements::LoginRequestReason::TokenInvalid)
			{
				if (VMManager::HasValidVM() && !Achievements::HasActiveGame())
					Achievements::DisableHardcoreMode();
			}

			ImGui::CloseCurrentPopup();
			s_achievements_login_open = false;
			s_achievements_login_logging_in = false;

			s_achievements_login_username[0] = '\0';
			s_achievements_login_password[0] = '\0';
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);

	if (!ImGui::IsPopupOpen("RetroAchievements"))
		ImGui::OpenPopup("RetroAchievements");
}

bool FullscreenUI::IsAchievementsWindowOpen()
{
	return (s_current_main_window == MainWindowType::Achievements);
}

void FullscreenUI::SwitchToAchievementsWindow()
{
	if (!VMManager::HasValidVM())
		return;

	if (!Achievements::HasAchievements())
	{
		ShowToast(std::string(), FSUI_STR("This game has no achievements."));
		return;
	}

	if (!Achievements::PrepareAchievementsWindow())
		return;

	if (s_current_main_window != MainWindowType::PauseMenu)
	{
		PauseForMenuOpen(false);
		ForceKeyNavEnabled();
	}

	s_current_main_window = MainWindowType::Achievements;
	QueueResetFocus(FocusResetType::WindowChanged);
}

bool FullscreenUI::OpenLeaderboardsWindow()
{
	if (!VMManager::HasValidVM() || !Achievements::IsActive())
		return false;

	MTGS::RunOnGSThread([]() {
		if (!ImGuiManager::InitializeFullscreenUI())
			return;

		SwitchToLeaderboardsWindow();
	});

	return true;
}

bool FullscreenUI::IsLeaderboardsWindowOpen()
{
	return (s_current_main_window == MainWindowType::Leaderboards);
}

void FullscreenUI::SwitchToLeaderboardsWindow()
{
	if (!VMManager::HasValidVM())
		return;

	if (!Achievements::HasLeaderboards())
	{
		ShowToast(std::string(), FSUI_STR("This game has no leaderboards."));
		return;
	}

	if (!Achievements::PrepareLeaderboardsWindow())
		return;

	if (s_current_main_window != MainWindowType::PauseMenu)
	{
		PauseForMenuOpen(false);
		ForceKeyNavEnabled();
	}

	s_current_main_window = MainWindowType::Leaderboards;
	QueueResetFocus(FocusResetType::WindowChanged);
}

void FullscreenUI::DrawAchievementsSettingsPage(std::unique_lock<std::mutex>& settings_lock)
{
#ifdef ENABLE_RAINTEGRATION
	if (Achievements::IsUsingRAIntegration())
	{
		BeginMenuButtons();
		ActiveButton(FSUI_ICONSTR(ICON_FA_BAN, "RAIntegration is being used instead of the built-in achievements implementation."), false,
			false, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		EndMenuButtons();
		return;
	}
#endif

	SettingsInterface* bsi = GetEditingSettingsInterface();
	bool check_challenge_state = false;

	BeginMenuButtons();

	MenuHeading(FSUI_CSTR("Settings"));
	check_challenge_state = DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_TROPHY, "Enable Achievements"),
		FSUI_CSTR("When enabled and logged in, PCSX2 will scan for achievements on startup."), "Achievements", "Enabled", false);

	const bool enabled = bsi->GetBoolValue("Achievements", "Enabled", false);

	check_challenge_state |= DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_PF_DUMBELL, "Hardcore Mode"),
		FSUI_CSTR(
			"\"Challenge\" mode for achievements, including leaderboard tracking. Disables save state, cheats, and slowdown functions."),
		"Achievements", "ChallengeMode", false, enabled);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_BELL, "Achievement Notifications"),
		FSUI_CSTR("Displays popup messages on events such as achievement unlocks and leaderboard submissions."), "Achievements",
		"Notifications", true, enabled);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_LIST_OL, "Leaderboard Notifications"),
		FSUI_CSTR("Displays popup messages when starting, submitting, or failing a leaderboard challenge."), "Achievements",
		"LeaderboardNotifications", true, enabled);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_HEADPHONES, "Sound Effects"),
		FSUI_CSTR("Plays sound effects for events such as achievement unlocks and leaderboard submissions."), "Achievements",
		"SoundEffects", true, enabled);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_PF_HEARTBEAT_ALT, "Enable In-Game Overlays"),
		FSUI_CSTR("Shows icons in the screen when a challenge/primed achievement is active."), "Achievements",
		"Overlays", true, enabled);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_PF_HEARTBEAT_ALT, "Enable In-Game Leaderboard Overlays"),
		FSUI_CSTR("Shows icons in the screen when leaderboard tracking is active."), "Achievements",
		"LBOverlays", true, enabled);

	if (enabled)
	{
		const char* alignment_options[] = {
			TRANSLATE_NOOP("FullscreenUI", "Top Left"),
			TRANSLATE_NOOP("FullscreenUI", "Top Center"),
			TRANSLATE_NOOP("FullscreenUI", "Top Right"),
			TRANSLATE_NOOP("FullscreenUI", "Center Left"),
			TRANSLATE_NOOP("FullscreenUI", "Center"),
			TRANSLATE_NOOP("FullscreenUI", "Center Right"),
			TRANSLATE_NOOP("FullscreenUI", "Bottom Left"),
			TRANSLATE_NOOP("FullscreenUI", "Bottom Center"),
			TRANSLATE_NOOP("FullscreenUI", "Bottom Right")
		};

		DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_ALIGN_CENTER, "Overlay Position"),
			FSUI_CSTR("Determines where achievement/leaderboard overlays are positioned on the screen."), "Achievements", "OverlayPosition",
			8, alignment_options, std::size(alignment_options), true, 0, enabled);

		const bool notifications_enabled = GetEffectiveBoolSetting(bsi, "Achievements", "Notifications", true) ||
											GetEffectiveBoolSetting(bsi, "Achievements", "LeaderboardNotifications", true);
		if (notifications_enabled)
		{
			DrawIntListSetting(bsi, FSUI_ICONSTR(ICON_FA_BELL, "Notification Position"),
				FSUI_CSTR("Determines where achievement/leaderboard notification popups are positioned on the screen."), "Achievements", "NotificationPosition",
				2, alignment_options, std::size(alignment_options), true, 0, enabled);
		}
	}
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_LOCK, "Encore Mode"),
		FSUI_CSTR("When enabled, each session will behave as if no achievements have been unlocked."), "Achievements", "EncoreMode", false,
		enabled);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_EYE, "Spectator Mode"),
		FSUI_CSTR("When enabled, PCSX2 will assume all achievements are locked and not send any unlock notifications to the server."),
		"Achievements", "SpectatorMode", false, enabled);
	DrawToggleSetting(bsi, FSUI_ICONSTR(ICON_FA_MEDAL, "Test Unofficial Achievements"),
		FSUI_CSTR(
			"When enabled, PCSX2 will list achievements from unofficial sets. These achievements are not tracked by RetroAchievements."),
		"Achievements", "UnofficialTestMode", false, enabled);

	// Check for challenge mode just being enabled.
	if (check_challenge_state && enabled && bsi->GetBoolValue("Achievements", "ChallengeMode", false) && VMManager::HasValidVM())
	{
		// don't bother prompting if the game doesn't have achievements
		auto lock = Achievements::GetLock();
		if (Achievements::HasActiveGame() && Achievements::HasAchievementsOrLeaderboards())
		{
			ImGuiFullscreen::OpenConfirmMessageDialog(FSUI_STR("Reset System"),
				FSUI_STR("Hardcore mode will not be enabled until the system is reset. Do you want to reset the system now?"), [](bool reset) {
					if (!VMManager::HasValidVM())
						return;

					if (reset)
						RequestReset();
				});
		}
	}

	if (!IsEditingGameSettings(bsi))
	{
		MenuHeading(FSUI_CSTR("Sound Effects"));
		if (MenuButton(FSUI_ICONSTR(ICON_FA_MUSIC, "Notification Sound"), bsi->GetTinyStringValue("Achievements", "InfoSoundName")))
		{
			auto callback = [bsi](const std::string& path) {
				if (!path.empty())
				{
					bsi->SetStringValue("Achievements", "InfoSoundName", path.c_str());
					SetSettingsChanged(bsi);
				}
				CloseFileSelector();
			};
			OpenFileSelector(FSUI_ICONSTR(ICON_FA_FOLDER_OPEN, "Select Notification Sound"), false, std::move(callback), GetAudioFileFilters());
		}

		if (MenuButton(FSUI_ICONSTR(ICON_FA_MUSIC, "Unlock Sound"), bsi->GetTinyStringValue("Achievements", "UnlockSoundName")))
		{
			auto callback = [bsi](const std::string& path) {
				if (!path.empty())
				{
					bsi->SetStringValue("Achievements", "UnlockSoundName", path.c_str());
					SetSettingsChanged(bsi);
				}
				CloseFileSelector();
			};
			OpenFileSelector(FSUI_ICONSTR(ICON_FA_FOLDER_OPEN, "Select Unlock Sound"), false, std::move(callback), GetAudioFileFilters());
		}

		if (MenuButton(FSUI_ICONSTR(ICON_FA_MUSIC, "Leaderboard Submit Sound"), bsi->GetTinyStringValue("Achievements", "LBSubmitSoundName")))
		{
			auto callback = [bsi](const std::string& path) {
				if (!path.empty())
				{
					bsi->SetStringValue("Achievements", "LBSubmitSoundName", path.c_str());
					SetSettingsChanged(bsi);
				}
				CloseFileSelector();
			};
			OpenFileSelector(FSUI_ICONSTR(ICON_FA_FOLDER_OPEN, "Select Leaderboard Submit Sound"), false, std::move(callback), GetAudioFileFilters());
		}

		MenuHeading(FSUI_CSTR("Account"));
		if (bsi->ContainsValue("Achievements", "Token"))
		{
			ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::GetStyle().Colors[ImGuiCol_Text]);
			ActiveButton(SmallString::from_format(
							 fmt::runtime(FSUI_ICONSTR(ICON_FA_USER, "Username: {}")), bsi->GetTinyStringValue("Achievements", "Username")),
				false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
			ActiveButton(SmallString::from_format(fmt::runtime(FSUI_ICONSTR(ICON_FA_CLOCK, "Login token generated on {}")),
							 TimeToPrintableString(static_cast<time_t>(
								 StringUtil::FromChars<u64>(bsi->GetTinyStringValue("Achievements", "LoginTimestamp", "0")).value_or(0)))),
				false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
			ImGui::PopStyleColor();

			if (MenuButton(FSUI_ICONSTR(ICON_FA_KEY, "Logout"), FSUI_CSTR("Logs out of RetroAchievements.")))
			{
				Host::RunOnCPUThread([]() { Achievements::Logout(); });
			}
		}
		else
		{
			ActiveButton(FSUI_ICONSTR(ICON_FA_USER, "Not Logged In"), false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

			if (MenuButton(FSUI_ICONSTR(ICON_FA_KEY, "Login"), FSUI_CSTR("Logs in to RetroAchievements.")))
			{
				s_achievements_login_reason = Achievements::LoginRequestReason::UserInitiated;
				s_achievements_login_open = true;
			}
		}

		MenuHeading(FSUI_CSTR("Current Game"));
		if (Achievements::HasActiveGame())
		{
			const auto lock = Achievements::GetLock();

			ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::GetStyle().Colors[ImGuiCol_Text]);
			ActiveButton(SmallString::from_format(fmt::runtime(FSUI_ICONSTR(ICON_FA_BOOKMARK, "Game: {0} ({1})")), Achievements::GetGameID(),
							 Achievements::GetGameTitle()),
				false, false, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

			const std::string& rich_presence_string = Achievements::GetRichPresenceString();
			if (!rich_presence_string.empty())
			{
				ActiveButton(
					SmallString::from_format(ICON_FA_MAP "{}", rich_presence_string), false, false, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
			}
			else
			{
				ActiveButton(FSUI_ICONSTR(ICON_FA_MAP, "Rich presence inactive or unsupported."), false, false,
					LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
			}

			ImGui::PopStyleColor();
		}
		else
		{
			ActiveButton(FSUI_ICONSTR(ICON_FA_BAN, "Game not loaded or no RetroAchievements available."), false, false,
				LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		}
	}

	EndMenuButtons();
}

void FullscreenUI::ReportStateLoadError(const std::string& message, std::optional<s32> slot, bool backup)
{
	MTGS::RunOnGSThread([message, slot, backup]() {
		const bool prompt_on_error = Host::GetBaseBoolSettingValue("UI", "PromptOnStateLoadSaveFailure", true);
		if (!prompt_on_error || !ImGuiManager::InitializeFullscreenUI())
		{
			SaveState_ReportLoadErrorOSD(message, slot, backup);
			return;
		}

		std::string title;
		if (slot.has_value())
		{
			if (backup)
				title = fmt::format(FSUI_FSTR("Failed to Load State From Backup Slot {}"), *slot);
			else
				title = fmt::format(FSUI_FSTR("Failed to Load State From Slot {}"), *slot);
		}
		else
		{
			title = FSUI_STR("Failed to Load State");
		}

		ImGuiFullscreen::InfoMessageDialogCallback callback;
		if (VMManager::GetState() == VMState::Running)
		{
			Host::RunOnCPUThread([]() { VMManager::SetPaused(true); });
			callback = []() {
				Host::RunOnCPUThread([]() { VMManager::SetPaused(false); });
			};
		}

		ImGuiFullscreen::OpenInfoMessageDialog(
			fmt::format("{} {}", ICON_FA_TRIANGLE_EXCLAMATION, title),
			std::move(message), std::move(callback));
	});
}

void FullscreenUI::ReportStateSaveError(const std::string& message, std::optional<s32> slot)
{
	MTGS::RunOnGSThread([message, slot]() {
		const bool prompt_on_error = Host::GetBaseBoolSettingValue("UI", "PromptOnStateLoadSaveFailure", true);
		if (!prompt_on_error || !ImGuiManager::InitializeFullscreenUI())
		{
			SaveState_ReportSaveErrorOSD(message, slot);
			return;
		}

		std::string title;
		if (slot.has_value())
			title = fmt::format(FSUI_FSTR("Failed to Save State To Slot {}"), *slot);
		else
			title = FSUI_STR("Failed to Save State");

		ImGuiFullscreen::InfoMessageDialogCallback callback;
		if (VMManager::GetState() == VMState::Running)
		{
			Host::RunOnCPUThread([]() { VMManager::SetPaused(true); });
			callback = []() {
				Host::RunOnCPUThread([]() { VMManager::SetPaused(false); });
			};
		}

		ImGuiFullscreen::OpenInfoMessageDialog(
			fmt::format("{} {}", ICON_FA_TRIANGLE_EXCLAMATION, title),
			std::move(message), std::move(callback));
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Translation String Area
// To avoid having to type T_RANSLATE("FullscreenUI", ...) everywhere, we use the shorter macros in the internal 
// header file, then preprocess and generate a bunch of noops here to define the strings. Sadly that means
// the view in Linguist is gonna suck, but you can search the file for the string for more context.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
// TRANSLATION-STRING-AREA-BEGIN
TRANSLATE_NOOP("FullscreenUI", "Error");
TRANSLATE_NOOP("FullscreenUI", "Could not find any CD/DVD-ROM devices. Please ensure you have a drive connected and sufficient permissions to access it.");
TRANSLATE_NOOP("FullscreenUI", "Your memory card is still saving data.\n\nWARNING: Shutting down now can IRREVERSIBLY CORRUPT YOUR MEMORY CARD.\n\nYou are strongly advised to select 'No' and let the save finish.\n\nDo you want to shutdown anyway and IRREVERSIBLY CORRUPT YOUR MEMORY CARD?");
TRANSLATE_NOOP("FullscreenUI", "No save present in this slot.");
TRANSLATE_NOOP("FullscreenUI", "No save states found.");
TRANSLATE_NOOP("FullscreenUI", "Failed to delete save state.");
TRANSLATE_NOOP("FullscreenUI", "empty title");
TRANSLATE_NOOP("FullscreenUI", "no serial");
TRANSLATE_NOOP("FullscreenUI", "Failed to copy text to clipboard.");
TRANSLATE_NOOP("FullscreenUI", "Enable Achievements");
TRANSLATE_NOOP("FullscreenUI", "Achievement tracking is not currently enabled. Your login will have no effect until after tracking is enabled.\n\nDo you want to enable tracking now?");
TRANSLATE_NOOP("FullscreenUI", "Enable Hardcore Mode");
TRANSLATE_NOOP("FullscreenUI", "Hardcore mode is not currently enabled. Enabling hardcore mode allows you to set times, scores, and participate in game-specific leaderboards.\n\nHowever, hardcore mode also prevents the usage of save states, cheats and slowdown functionality.\n\nDo you want to enable hardcore mode?");
TRANSLATE_NOOP("FullscreenUI", "Reset System");
TRANSLATE_NOOP("FullscreenUI", "Hardcore mode will not be enabled until the system is reset. Do you want to reset the system now?");
TRANSLATE_NOOP("FullscreenUI", "This game has no achievements.");
TRANSLATE_NOOP("FullscreenUI", "This game has no leaderboards.");
TRANSLATE_NOOP("FullscreenUI", "Failed to Load State");
TRANSLATE_NOOP("FullscreenUI", "Failed to Save State");
TRANSLATE_NOOP("FullscreenUI", "Game List");
TRANSLATE_NOOP("FullscreenUI", "Launch a game from images scanned from your game directories.");
TRANSLATE_NOOP("FullscreenUI", "Start Game");
TRANSLATE_NOOP("FullscreenUI", "Launch a game from a file, disc, or starts the console without any disc inserted.");
TRANSLATE_NOOP("FullscreenUI", "Settings");
TRANSLATE_NOOP("FullscreenUI", "Changes settings for the application.");
TRANSLATE_NOOP("FullscreenUI", "Exit");
TRANSLATE_NOOP("FullscreenUI", "Return to desktop mode, or exit the application.");
TRANSLATE_NOOP("FullscreenUI", "Start File");
TRANSLATE_NOOP("FullscreenUI", "Launch a game by selecting a file/disc image.");
TRANSLATE_NOOP("FullscreenUI", "Start Disc");
TRANSLATE_NOOP("FullscreenUI", "Start a game from a disc in your PC's DVD drive.");
TRANSLATE_NOOP("FullscreenUI", "Start BIOS");
TRANSLATE_NOOP("FullscreenUI", "Start the console without any disc inserted.");
TRANSLATE_NOOP("FullscreenUI", "Back");
TRANSLATE_NOOP("FullscreenUI", "Return to the previous menu.");
TRANSLATE_NOOP("FullscreenUI", "Exit PCSX2");
TRANSLATE_NOOP("FullscreenUI", "Completely exits the application, returning you to your desktop.");
TRANSLATE_NOOP("FullscreenUI", "Desktop Mode");
TRANSLATE_NOOP("FullscreenUI", "Exits Big Picture mode, returning to the desktop interface.");
TRANSLATE_NOOP("FullscreenUI", "Load State");
TRANSLATE_NOOP("FullscreenUI", "Save State");
TRANSLATE_NOOP("FullscreenUI", "Load Resume State");
TRANSLATE_NOOP("FullscreenUI", "A resume save state created at %s was found.\n\nDo you want to load this save and continue?");
TRANSLATE_NOOP("FullscreenUI", "Region: ");
TRANSLATE_NOOP("FullscreenUI", "Compatibility: ");
TRANSLATE_NOOP("FullscreenUI", "No Game Selected");
TRANSLATE_NOOP("FullscreenUI", "Game List Settings");
TRANSLATE_NOOP("FullscreenUI", "Search Directories");
TRANSLATE_NOOP("FullscreenUI", "Adds a new directory to the game search list.");
TRANSLATE_NOOP("FullscreenUI", "Scanning Subdirectories");
TRANSLATE_NOOP("FullscreenUI", "Not Scanning Subdirectories");
TRANSLATE_NOOP("FullscreenUI", "List Settings");
TRANSLATE_NOOP("FullscreenUI", "Sets which view the game list will open to.");
TRANSLATE_NOOP("FullscreenUI", "Determines which field the game list will be sorted by.");
TRANSLATE_NOOP("FullscreenUI", "Reverses the game list sort order from the default (usually ascending to descending).");
TRANSLATE_NOOP("FullscreenUI", "Shows Titles for Games when in Game Grid View Mode");
TRANSLATE_NOOP("FullscreenUI", "Cover Settings");
TRANSLATE_NOOP("FullscreenUI", "Downloads covers from a user-specified URL template.");
TRANSLATE_NOOP("FullscreenUI", "Operations");
TRANSLATE_NOOP("FullscreenUI", "Identifies any new files added to the game directories.");
TRANSLATE_NOOP("FullscreenUI", "Forces a full rescan of all games previously identified.");
TRANSLATE_NOOP("FullscreenUI", "About PCSX2");
TRANSLATE_NOOP("FullscreenUI", "PCSX2 is a free and open-source PlayStation 2 (PS2) emulator. Its purpose is to emulate the PS2's hardware, using a combination of MIPS CPU Interpreters, Recompilers and a Virtual Machine which manages hardware states and PS2 system memory. This allows you to play PS2 games on your PC, with many additional features and benefits.");
TRANSLATE_NOOP("FullscreenUI", "PlayStation 2 and PS2 are registered trademarks of Sony Interactive Entertainment. This application is not affiliated in any way with Sony Interactive Entertainment.");
TRANSLATE_NOOP("FullscreenUI", "Version: %s");
TRANSLATE_NOOP("FullscreenUI", "RetroAchievements");
TRANSLATE_NOOP("FullscreenUI", "Please enter your user name and password for retroachievements.org below.\n\nYour password will not be saved in PCSX2, an access token will be generated and used instead.");
TRANSLATE_NOOP("FullscreenUI", "Username");
TRANSLATE_NOOP("FullscreenUI", "Password");
TRANSLATE_NOOP("FullscreenUI", "Logging in...");
TRANSLATE_NOOP("FullscreenUI", "Login");
TRANSLATE_NOOP("FullscreenUI", "Cancel");
TRANSLATE_NOOP("FullscreenUI", "When enabled and logged in, PCSX2 will scan for achievements on startup.");
TRANSLATE_NOOP("FullscreenUI", "\"Challenge\" mode for achievements, including leaderboard tracking. Disables save state, cheats, and slowdown functions.");
TRANSLATE_NOOP("FullscreenUI", "Displays popup messages on events such as achievement unlocks and leaderboard submissions.");
TRANSLATE_NOOP("FullscreenUI", "Displays popup messages when starting, submitting, or failing a leaderboard challenge.");
TRANSLATE_NOOP("FullscreenUI", "Plays sound effects for events such as achievement unlocks and leaderboard submissions.");
TRANSLATE_NOOP("FullscreenUI", "Shows icons in the screen when a challenge/primed achievement is active.");
TRANSLATE_NOOP("FullscreenUI", "Shows icons in the screen when leaderboard tracking is active.");
TRANSLATE_NOOP("FullscreenUI", "Determines where achievement/leaderboard overlays are positioned on the screen.");
TRANSLATE_NOOP("FullscreenUI", "Determines where achievement/leaderboard notification popups are positioned on the screen.");
TRANSLATE_NOOP("FullscreenUI", "When enabled, each session will behave as if no achievements have been unlocked.");
TRANSLATE_NOOP("FullscreenUI", "When enabled, PCSX2 will assume all achievements are locked and not send any unlock notifications to the server.");
TRANSLATE_NOOP("FullscreenUI", "When enabled, PCSX2 will list achievements from unofficial sets. These achievements are not tracked by RetroAchievements.");
TRANSLATE_NOOP("FullscreenUI", "Sound Effects");
TRANSLATE_NOOP("FullscreenUI", "Account");
TRANSLATE_NOOP("FullscreenUI", "Logs out of RetroAchievements.");
TRANSLATE_NOOP("FullscreenUI", "Logs in to RetroAchievements.");
TRANSLATE_NOOP("FullscreenUI", "Current Game");
TRANSLATE_NOOP("FullscreenUI", "An error occurred while deleting empty game settings:\n{}");
TRANSLATE_NOOP("FullscreenUI", "An error occurred while saving game settings:\n{}");
TRANSLATE_NOOP("FullscreenUI", "{} is not a valid disc image.");
TRANSLATE_NOOP("FullscreenUI", "{:%H:%M}");
TRANSLATE_NOOP("FullscreenUI", "This Session: {}");
TRANSLATE_NOOP("FullscreenUI", "All Time: {}");
TRANSLATE_NOOP("FullscreenUI", "Save Slot {0}");
TRANSLATE_NOOP("FullscreenUI", "{0} Slot {1}");
TRANSLATE_NOOP("FullscreenUI", "Saved {}");
TRANSLATE_NOOP("FullscreenUI", "{} does not exist.");
TRANSLATE_NOOP("FullscreenUI", "{} deleted.");
TRANSLATE_NOOP("FullscreenUI", "Failed to delete {}.");
TRANSLATE_NOOP("FullscreenUI", "File: {}");
TRANSLATE_NOOP("FullscreenUI", "CRC: {:08X}");
TRANSLATE_NOOP("FullscreenUI", "Time Played: {}");
TRANSLATE_NOOP("FullscreenUI", "Last Played: {}");
TRANSLATE_NOOP("FullscreenUI", "Size: {:.2f} MB");
TRANSLATE_NOOP("FullscreenUI", "Are you sure you want to reset the play time for '{}' ({})?\n\nYour current play time is {}.\n\nThis action cannot be undone.");
TRANSLATE_NOOP("FullscreenUI", "Login failed.\nError: {}\n\nPlease check your username and password, and try again.");
TRANSLATE_NOOP("FullscreenUI", "Failed to Load State From Backup Slot {}");
TRANSLATE_NOOP("FullscreenUI", "Failed to Load State From Slot {}");
TRANSLATE_NOOP("FullscreenUI", "Failed to Save State To Slot {}");
TRANSLATE_NOOP("FullscreenUI", "Game Grid");
TRANSLATE_NOOP("FullscreenUI", "Type");
TRANSLATE_NOOP("FullscreenUI", "Serial");
TRANSLATE_NOOP("FullscreenUI", "Title");
TRANSLATE_NOOP("FullscreenUI", "File Title");
TRANSLATE_NOOP("FullscreenUI", "CRC");
TRANSLATE_NOOP("FullscreenUI", "Time Played");
TRANSLATE_NOOP("FullscreenUI", "Last Played");
TRANSLATE_NOOP("FullscreenUI", "Size");
TRANSLATE_NOOP("FullscreenUI", "Change Selection");
TRANSLATE_NOOP("FullscreenUI", "Select");
TRANSLATE_NOOP("FullscreenUI", "Parent Directory");
TRANSLATE_NOOP("FullscreenUI", "Enter Value");
TRANSLATE_NOOP("FullscreenUI", "About");
TRANSLATE_NOOP("FullscreenUI", "Navigate");
TRANSLATE_NOOP("FullscreenUI", "Toggle Fullscreen");
TRANSLATE_NOOP("FullscreenUI", "Load Global State");
TRANSLATE_NOOP("FullscreenUI", "Return To Game");
TRANSLATE_NOOP("FullscreenUI", "Select State");
TRANSLATE_NOOP("FullscreenUI", "Options");
TRANSLATE_NOOP("FullscreenUI", "Load/Save State");
TRANSLATE_NOOP("FullscreenUI", "Select Game");
TRANSLATE_NOOP("FullscreenUI", "Change View");
TRANSLATE_NOOP("FullscreenUI", "Launch Options");
TRANSLATE_NOOP("FullscreenUI", "Startup Error");
TRANSLATE_NOOP("FullscreenUI", "Select Disc Image");
TRANSLATE_NOOP("FullscreenUI", "Select Disc Drive");
TRANSLATE_NOOP("FullscreenUI", "WARNING: Memory Card Busy");
TRANSLATE_NOOP("FullscreenUI", "Resume Game");
TRANSLATE_NOOP("FullscreenUI", "Close Game");
TRANSLATE_NOOP("FullscreenUI", "Back To Pause Menu");
TRANSLATE_NOOP("FullscreenUI", "Exit Without Saving");
TRANSLATE_NOOP("FullscreenUI", "Leaderboards");
TRANSLATE_NOOP("FullscreenUI", "Toggle Frame Limit");
TRANSLATE_NOOP("FullscreenUI", "Game Properties");
TRANSLATE_NOOP("FullscreenUI", "Achievements");
TRANSLATE_NOOP("FullscreenUI", "Save Screenshot");
TRANSLATE_NOOP("FullscreenUI", "Switch To Software Renderer");
TRANSLATE_NOOP("FullscreenUI", "Switch To Hardware Renderer");
TRANSLATE_NOOP("FullscreenUI", "Change Disc");
TRANSLATE_NOOP("FullscreenUI", "Exit And Save State");
TRANSLATE_NOOP("FullscreenUI", "Delete Save");
TRANSLATE_NOOP("FullscreenUI", "Close Menu");
TRANSLATE_NOOP("FullscreenUI", "Default Boot");
TRANSLATE_NOOP("FullscreenUI", "Delete State");
TRANSLATE_NOOP("FullscreenUI", "Fast Boot");
TRANSLATE_NOOP("FullscreenUI", "Full Boot");
TRANSLATE_NOOP("FullscreenUI", "Reset Play Time");
TRANSLATE_NOOP("FullscreenUI", "Confirm Reset");
TRANSLATE_NOOP("FullscreenUI", "Add Search Directory");
TRANSLATE_NOOP("FullscreenUI", "Open in File Browser");
TRANSLATE_NOOP("FullscreenUI", "Disable Subdirectory Scanning");
TRANSLATE_NOOP("FullscreenUI", "Enable Subdirectory Scanning");
TRANSLATE_NOOP("FullscreenUI", "Remove From List");
TRANSLATE_NOOP("FullscreenUI", "Default View");
TRANSLATE_NOOP("FullscreenUI", "Sort By");
TRANSLATE_NOOP("FullscreenUI", "Sort Reversed");
TRANSLATE_NOOP("FullscreenUI", "Show Titles");
TRANSLATE_NOOP("FullscreenUI", "Covers Directory");
TRANSLATE_NOOP("FullscreenUI", "Download Covers");
TRANSLATE_NOOP("FullscreenUI", "Scan For New Games");
TRANSLATE_NOOP("FullscreenUI", "Rescan All Games");
TRANSLATE_NOOP("FullscreenUI", "Website");
TRANSLATE_NOOP("FullscreenUI", "Support Forums");
TRANSLATE_NOOP("FullscreenUI", "GitHub Repository");
TRANSLATE_NOOP("FullscreenUI", "License");
TRANSLATE_NOOP("FullscreenUI", "Close");
TRANSLATE_NOOP("FullscreenUI", "RAIntegration is being used instead of the built-in achievements implementation.");
TRANSLATE_NOOP("FullscreenUI", "Hardcore Mode");
TRANSLATE_NOOP("FullscreenUI", "Achievement Notifications");
TRANSLATE_NOOP("FullscreenUI", "Leaderboard Notifications");
TRANSLATE_NOOP("FullscreenUI", "Enable In-Game Overlays");
TRANSLATE_NOOP("FullscreenUI", "Enable In-Game Leaderboard Overlays");
TRANSLATE_NOOP("FullscreenUI", "Overlay Position");
TRANSLATE_NOOP("FullscreenUI", "Notification Position");
TRANSLATE_NOOP("FullscreenUI", "Encore Mode");
TRANSLATE_NOOP("FullscreenUI", "Spectator Mode");
TRANSLATE_NOOP("FullscreenUI", "Test Unofficial Achievements");
TRANSLATE_NOOP("FullscreenUI", "Notification Sound");
TRANSLATE_NOOP("FullscreenUI", "Select Notification Sound");
TRANSLATE_NOOP("FullscreenUI", "Unlock Sound");
TRANSLATE_NOOP("FullscreenUI", "Select Unlock Sound");
TRANSLATE_NOOP("FullscreenUI", "Leaderboard Submit Sound");
TRANSLATE_NOOP("FullscreenUI", "Select Leaderboard Submit Sound");
TRANSLATE_NOOP("FullscreenUI", "Username: {}");
TRANSLATE_NOOP("FullscreenUI", "Login token generated on {}");
TRANSLATE_NOOP("FullscreenUI", "Logout");
TRANSLATE_NOOP("FullscreenUI", "Not Logged In");
TRANSLATE_NOOP("FullscreenUI", "Game: {0} ({1})");
TRANSLATE_NOOP("FullscreenUI", "Rich presence inactive or unsupported.");
TRANSLATE_NOOP("FullscreenUI", "Game not loaded or no RetroAchievements available.");
// TRANSLATION-STRING-AREA-END
#endif
