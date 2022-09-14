/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#define IMGUI_DEFINE_MATH_OPERATORS

#include "Frontend/FullscreenUI.h"
#include "Frontend/ImGuiManager.h"
#include "Frontend/ImGuiFullscreen.h"
#include "Frontend/InputManager.h"
#include "Frontend/GameList.h"
#include "IconsFontAwesome5.h"

#include "common/FileSystem.h"
#include "common/Console.h"
#include "common/Image.h"
#include "common/Path.h"
#include "common/SettingsInterface.h"
#include "common/SettingsWrapper.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "CDVD/CDVDcommon.h"
#include "CDVD/CDVDdiscReader.h"
#include "GS.h"
#include "Host.h"
#include "HostDisplay.h"
#include "HostSettings.h"
#include "INISettingsInterface.h"
#include "MemoryCardFile.h"
#include "PAD/Host/PAD.h"
#include "ps2/BiosTools.h"
#include "Sio.h"
#include "VMManager.h"

#include "svnrev.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "fmt/core.h"

#include <array>
#include <bitset>
#include <thread>

static constexpr s32 MAX_SAVE_STATE_SLOTS = 10;

using ImGuiFullscreen::g_large_font;
using ImGuiFullscreen::g_layout_padding_left;
using ImGuiFullscreen::g_layout_padding_top;
using ImGuiFullscreen::g_medium_font;
using ImGuiFullscreen::LAYOUT_LARGE_FONT_SIZE;
using ImGuiFullscreen::LAYOUT_MEDIUM_FONT_SIZE;
using ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT;
using ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY;
using ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING;
using ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING;
using ImGuiFullscreen::LAYOUT_SCREEN_HEIGHT;
using ImGuiFullscreen::LAYOUT_SCREEN_WIDTH;
using ImGuiFullscreen::UIBackgroundColor;
using ImGuiFullscreen::UIBackgroundHighlightColor;
using ImGuiFullscreen::UIBackgroundLineColor;
using ImGuiFullscreen::UIBackgroundTextColor;
using ImGuiFullscreen::UIDisabledColor;
using ImGuiFullscreen::UIPrimaryColor;
using ImGuiFullscreen::UIPrimaryDarkColor;
using ImGuiFullscreen::UIPrimaryLightColor;
using ImGuiFullscreen::UIPrimaryLineColor;
using ImGuiFullscreen::UIPrimaryTextColor;
using ImGuiFullscreen::UISecondaryColor;
using ImGuiFullscreen::UISecondaryDarkColor;
using ImGuiFullscreen::UISecondaryLightColor;
using ImGuiFullscreen::UISecondaryTextColor;
using ImGuiFullscreen::UITextHighlightColor;

using ImGuiFullscreen::ActiveButton;
using ImGuiFullscreen::AddNotification;
using ImGuiFullscreen::BeginFullscreenColumns;
using ImGuiFullscreen::BeginFullscreenColumnWindow;
using ImGuiFullscreen::BeginFullscreenWindow;
using ImGuiFullscreen::BeginMenuButtons;
using ImGuiFullscreen::BeginNavBar;
using ImGuiFullscreen::CenterImage;
using ImGuiFullscreen::CloseChoiceDialog;
using ImGuiFullscreen::CloseFileSelector;
using ImGuiFullscreen::DPIScale;
using ImGuiFullscreen::EndFullscreenColumns;
using ImGuiFullscreen::EndFullscreenColumnWindow;
using ImGuiFullscreen::EndFullscreenWindow;
using ImGuiFullscreen::EndMenuButtons;
using ImGuiFullscreen::EndNavBar;
using ImGuiFullscreen::EnumChoiceButton;
using ImGuiFullscreen::FloatingButton;
using ImGuiFullscreen::GetCachedTexture;
using ImGuiFullscreen::GetCachedTextureAsync;
using ImGuiFullscreen::GetPlaceholderTexture;
using ImGuiFullscreen::LayoutScale;
using ImGuiFullscreen::LoadTexture;
using ImGuiFullscreen::MenuButton;
using ImGuiFullscreen::MenuButtonFrame;
using ImGuiFullscreen::MenuButtonWithValue;
using ImGuiFullscreen::MenuHeading;
using ImGuiFullscreen::MenuHeadingButton;
using ImGuiFullscreen::MenuImageButton;
using ImGuiFullscreen::NavButton;
using ImGuiFullscreen::NavTitle;
using ImGuiFullscreen::OpenChoiceDialog;
using ImGuiFullscreen::OpenConfirmMessageDialog;
using ImGuiFullscreen::OpenFileSelector;
using ImGuiFullscreen::OpenInfoMessageDialog;
using ImGuiFullscreen::OpenInputStringDialog;
using ImGuiFullscreen::PopPrimaryColor;
using ImGuiFullscreen::PushPrimaryColor;
using ImGuiFullscreen::QueueResetFocus;
using ImGuiFullscreen::RangeButton;
using ImGuiFullscreen::ResetFocusHere;
using ImGuiFullscreen::RightAlignNavButtons;
using ImGuiFullscreen::ShowToast;
using ImGuiFullscreen::ThreeWayToggleButton;
using ImGuiFullscreen::ToggleButton;
using ImGuiFullscreen::WantsToCloseMenu;

namespace FullscreenUI
{
	enum class MainWindowType
	{
		None,
		Landing,
		GameList,
		Settings,
		PauseMenu
	};

	enum class PauseSubMenu
	{
		None,
		Exit,
	};

	enum class SettingsPage
	{
		Summary,
		Interface,
		GameList,
		BIOS,
		Emulation,
		System,
		Graphics,
		Audio,
		MemoryCard,
		Controller,
		Hotkey,
		Achievements,
		Advanced,
		GameFixes,
		Count
	};

	//////////////////////////////////////////////////////////////////////////
	// Utility
	//////////////////////////////////////////////////////////////////////////
	static std::string TimeToPrintableString(time_t t);

	//////////////////////////////////////////////////////////////////////////
	// Main
	//////////////////////////////////////////////////////////////////////////
	static void UpdateForcedVsync(bool should_force);
	static void UpdateGameDetails(std::string path, std::string serial, std::string title, u32 crc);
	static void PauseForMenuOpen();
	static void ClosePauseMenu();
	static void OpenPauseSubMenu(PauseSubMenu submenu);
	static void ReturnToMainWindow();
	static void DrawLandingWindow();
	static void DrawPauseMenu(MainWindowType type);
	static void ExitFullscreenAndOpenURL(const std::string_view& url);
	static void CopyTextToClipboard(std::string title, const std::string_view& text);
	static void DrawAboutWindow();
	static void OpenAboutWindow();

	static MainWindowType s_current_main_window = MainWindowType::None;
	static PauseSubMenu s_current_pause_submenu = PauseSubMenu::None;
	static bool s_initialized = false;
	static bool s_tried_to_initialize = false;
	static bool s_pause_menu_was_open = false;
	static bool s_was_paused_on_quick_menu_open = false;
	static bool s_about_window_open = false;

	// local copies of the currently-running game
	static std::string s_current_game_title;
	static std::string s_current_game_subtitle;
	static std::string s_current_game_serial;
	static std::string s_current_game_path;
	static u32 s_current_game_crc;

	//////////////////////////////////////////////////////////////////////////
	// Resources
	//////////////////////////////////////////////////////////////////////////
	static bool LoadResources();
	static void DestroyResources();

	static std::shared_ptr<HostDisplayTexture> s_app_icon_texture;
	static std::array<std::shared_ptr<HostDisplayTexture>, static_cast<u32>(GameDatabaseSchema::Compatibility::Perfect)>
		s_game_compatibility_textures;
	static std::shared_ptr<HostDisplayTexture> s_fallback_disc_texture;
	static std::shared_ptr<HostDisplayTexture> s_fallback_exe_texture;
	static std::vector<std::unique_ptr<HostDisplayTexture>> s_cleanup_textures;

	//////////////////////////////////////////////////////////////////////////
	// Landing
	//////////////////////////////////////////////////////////////////////////
	static void SwitchToLanding();
	static ImGuiFullscreen::FileSelectorFilters GetDiscImageFilters();
	static void DoStartPath(
		const std::string& path, std::optional<s32> state_index = std::nullopt, std::optional<bool> fast_boot = std::nullopt);
	static void DoStartFile();
	static void DoStartBIOS();
	static void DoStartDisc(const std::string& drive);
	static void DoStartDisc();
	static void DoToggleFrameLimit();
	static void DoToggleSoftwareRenderer();
	static void DoShutdown(bool save_state);
	static void DoReset();
	static void DoChangeDiscFromFile();
	static void DoChangeDisc();
	static void DoRequestExit();
	static void DoToggleFullscreen();

	//////////////////////////////////////////////////////////////////////////
	// Settings
	//////////////////////////////////////////////////////////////////////////

	static constexpr double INPUT_BINDING_TIMEOUT_SECONDS = 5.0;
	static constexpr u32 NUM_MEMORY_CARD_PORTS = 2;

	static void SwitchToSettings();
	static void SwitchToGameSettings();
	static void SwitchToGameSettings(const std::string& path);
	static void SwitchToGameSettings(const GameList::Entry* entry);
	static void SwitchToGameSettings(const std::string_view& serial, u32 crc);
	static void DrawSettingsWindow();
	static void DrawSummarySettingsPage();
	static void DrawInterfaceSettingsPage();
	static void DrawGameListSettingsPage();
	static void DrawBIOSSettingsPage();
	static void DrawEmulationSettingsPage();
	static void DrawSystemSettingsPage();
	static void DrawGraphicsSettingsPage();
	static void DrawAudioSettingsPage();
	static void DrawMemoryCardSettingsPage();
	static void DrawCreateMemoryCardWindow();
	static void DrawControllerSettingsPage();
	static void DrawHotkeySettingsPage();
	static void DrawAchievementsSettingsPage();
	static void DrawAdvancedSettingsPage();
	static void DrawGameFixesSettingsPage();

	static bool IsEditingGameSettings(SettingsInterface* bsi);
	static SettingsInterface* GetEditingSettingsInterface();
	static SettingsInterface* GetEditingSettingsInterface(bool game_settings);
	static void SetSettingsChanged(SettingsInterface* bsi);
	static bool GetEffectiveBoolSetting(SettingsInterface* bsi, const char* section, const char* key, bool default_value);
	static s32 GetEffectiveIntSetting(SettingsInterface* bsi, const char* section, const char* key, s32 default_value);
	static void DoCopyGameSettings();
	static void DoClearGameSettings();
	static void CopyGlobalControllerSettingsToGame();
	static void ResetControllerSettings();
	static void DoLoadInputProfile();
	static void DoSaveInputProfile();
	static void DoSaveInputProfile(const std::string& name);
	static void DoResetSettings();

	static bool DrawToggleSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		bool default_value, bool enabled = true, bool allow_tristate = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT,
		ImFont* font = g_large_font, ImFont* summary_font = g_medium_font);
	static void DrawIntListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		int default_value, const char* const* options, size_t option_count, int option_offset = 0, bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font, ImFont* summary_font = g_medium_font);
	static void DrawIntRangeSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		int default_value, int min_value, int max_value, const char* format = "%d", bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font, ImFont* summary_font = g_medium_font);
	static void DrawFloatRangeSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		float default_value, float min_value, float max_value, const char* format = "%f", float multiplier = 1.0f, bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font, ImFont* summary_font = g_medium_font);
	static void DrawIntRectSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
		const char* left_key, int default_left, const char* top_key, int default_top, const char* right_key, int default_right,
		const char* bottom_key, int default_bottom, int min_value, int max_value, const char* format = "%d", bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font, ImFont* summary_font = g_medium_font);
	static void DrawStringListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		const char* default_value, const char* const* options, const char* const* option_values, size_t option_count, bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font, ImFont* summary_font = g_medium_font);
	static void DrawFloatListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		float default_value, const char* const* options, const float* option_values, size_t option_count, bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font, ImFont* summary_font = g_medium_font);
	static void DrawFolderSetting(SettingsInterface* bsi, const char* title, const char* section, const char* key,
		const std::string& runtime_var, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font,
		ImFont* summary_font = g_medium_font);
	static void DrawClampingModeSetting(SettingsInterface* bsi, const char* title, const char* summary, bool vu);
	static void PopulateGraphicsAdapterList();
	static void PopulateGameListDirectoryCache(SettingsInterface* si);
	static ImGuiFullscreen::ChoiceDialogOptions GetGameListDirectoryOptions(bool recursive_as_checked);
	static void BeginInputBinding(SettingsInterface* bsi, PAD::ControllerBindingType type, const std::string_view& section,
		const std::string_view& key, const std::string_view& display_name);
	static void DrawInputBindingWindow();
	static void DrawInputBindingButton(SettingsInterface* bsi, PAD::ControllerBindingType type, const char* section, const char* name,
		const char* display_name, bool show_type = true);
	static void ClearInputBindingVariables();
	static void StartAutomaticBinding(u32 port);

	static SettingsPage s_settings_page = SettingsPage::Interface;
	static std::unique_ptr<INISettingsInterface> s_game_settings_interface;
	static std::unique_ptr<GameList::Entry> s_game_settings_entry;
	static std::vector<std::pair<std::string, bool>> s_game_list_directories_cache;
	static std::vector<std::string> s_graphics_adapter_list_cache;
	static std::vector<std::string> s_fullscreen_mode_list_cache;
	static std::vector<const HotkeyInfo*> s_hotkey_list_cache;
	static std::atomic_bool s_settings_changed{false};
	static std::atomic_bool s_game_settings_changed{false};
	static PAD::ControllerBindingType s_input_binding_type = PAD::ControllerBindingType::Unknown;
	static std::string s_input_binding_section;
	static std::string s_input_binding_key;
	static std::string s_input_binding_display_name;
	static std::vector<InputBindingKey> s_input_binding_new_bindings;
	static Common::Timer s_input_binding_timer;

	//////////////////////////////////////////////////////////////////////////
	// Save State List
	//////////////////////////////////////////////////////////////////////////
	struct SaveStateListEntry
	{
		std::string title;
		std::string summary;
		std::string path;
		std::unique_ptr<HostDisplayTexture> preview_texture;
		s32 slot;
	};

	static void InitializePlaceholderSaveStateListEntry(
		SaveStateListEntry* li, const std::string& title, const std::string& serial, u32 crc, s32 slot);
	static bool InitializeSaveStateListEntry(
		SaveStateListEntry* li, const std::string& title, const std::string& serial, u32 crc, s32 slot);
	static void ClearSaveStateEntryList();
	static u32 PopulateSaveStateListEntries(const std::string& title, const std::string& serial, u32 crc);
	static bool OpenLoadStateSelectorForGame(const std::string& game_path);
	static bool OpenSaveStateSelector(bool is_loading);
	static void CloseSaveStateSelector();
	static void DrawSaveStateSelector(bool is_loading, bool fullscreen);
	static void DoLoadState(std::string path);

	static std::vector<SaveStateListEntry> s_save_state_selector_slots;
	static std::string s_save_state_selector_game_path;
	static bool s_save_state_selector_open = false;
	static bool s_save_state_selector_loading = true;

	//////////////////////////////////////////////////////////////////////////
	// Game List
	//////////////////////////////////////////////////////////////////////////
	static void DrawGameListWindow();
	static void SwitchToGameList();
	static void PopulateGameListEntryList();
	static HostDisplayTexture* GetTextureForGameListEntryType(GameList::EntryType type);
	static HostDisplayTexture* GetGameListCover(const GameList::Entry* entry);
	static HostDisplayTexture* GetCoverForCurrentGame();
	static std::string GetNotificationImageForGame(const GameList::Entry* entry);
	static std::string GetNotificationImageForGame(const std::string& game_path);

	// Lazily populated cover images.
	static std::unordered_map<std::string, std::string> s_cover_image_map;
	static std::vector<const GameList::Entry*> s_game_list_sorted_entries;
} // namespace FullscreenUI

//////////////////////////////////////////////////////////////////////////
// Utility
//////////////////////////////////////////////////////////////////////////

std::string FullscreenUI::TimeToPrintableString(time_t t)
{
	struct tm lt = {};
#ifdef _MSC_VER
	localtime_s(&lt, &t);
#else
	localtime_r(&t, &lt);
#endif

	char buf[256];
	std::strftime(buf, sizeof(buf), "%c", &lt);
	return std::string(buf);
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

	ImGuiFullscreen::SetTheme();
	ImGuiFullscreen::UpdateLayoutScale();

	if (!ImGuiManager::AddFullscreenFontsIfMissing() || !ImGuiFullscreen::Initialize("fullscreenui/placeholder.png") || !LoadResources())
	{
		DestroyResources();
		ImGuiFullscreen::Shutdown();
		s_tried_to_initialize = true;
		return false;
	}

	s_initialized = true;
	s_current_main_window = MainWindowType::None;
	s_current_pause_submenu = PauseSubMenu::None;
	s_pause_menu_was_open = false;
	s_was_paused_on_quick_menu_open = false;
	s_about_window_open = false;
	s_hotkey_list_cache = InputManager::GetHotkeyList();
	GetMTGS().SetRunIdle(true);

	if (VMManager::HasValidVM())
	{
		UpdateGameDetails(VMManager::GetDiscPath(), VMManager::GetGameSerial(), VMManager::GetGameName(), VMManager::GetGameCRC());
	}
	else
	{
		SwitchToLanding();
	}

	// force vsync on so we don't run at thousands of fps
	// Initialize is called on the GS thread, so we can access the display directly.
	UpdateForcedVsync(VMManager::GetState() != VMState::Running);

	return true;
}

bool FullscreenUI::IsInitialized()
{
	return s_initialized;
}

bool FullscreenUI::HasActiveWindow()
{
	return s_current_main_window != MainWindowType::None || s_save_state_selector_open || ImGuiFullscreen::IsChoiceDialogOpen() ||
		   ImGuiFullscreen::IsFileSelectorOpen();
}

void FullscreenUI::UpdateForcedVsync(bool should_force)
{
	// force vsync on so we don't run at thousands of fps
	const VsyncMode mode = EmuConfig.GetEffectiveVsyncMode();

	// toss it through regardless of the mode, because options can change it
	g_host_display->SetVSync((should_force && mode == VsyncMode::Off) ? VsyncMode::On : mode);
}

void FullscreenUI::OnVMStarted()
{
	if (!IsInitialized())
		return;

	GetMTGS().RunOnGSThread([]() {
		if (!IsInitialized())
			return;

		s_current_main_window = MainWindowType::None;
		QueueResetFocus();
	});
}

void FullscreenUI::OnVMPaused()
{
	if (!IsInitialized())
		return;

	GetMTGS().RunOnGSThread([]() {
		if (!IsInitialized())
			return;

		UpdateForcedVsync(true);
	});
}

void FullscreenUI::OnVMResumed()
{
	if (!IsInitialized())
		return;

	GetMTGS().RunOnGSThread([]() {
		if (!IsInitialized())
			return;

		UpdateForcedVsync(false);
	});
}

void FullscreenUI::OnVMDestroyed()
{
	if (!IsInitialized())
		return;

	GetMTGS().RunOnGSThread([]() {
		if (!IsInitialized())
			return;

		s_pause_menu_was_open = false;
		SwitchToLanding();
		UpdateForcedVsync(true);
	});
}

void FullscreenUI::OnRunningGameChanged(std::string path, std::string serial, std::string title, u32 crc)
{
	if (!IsInitialized())
		return;

	GetMTGS().RunOnGSThread([path = std::move(path), serial = std::move(serial), title = std::move(title), crc]() {
		if (!IsInitialized())
			return;

		UpdateGameDetails(std::move(path), std::move(serial), std::move(title), crc);
	});
}

void FullscreenUI::UpdateGameDetails(std::string path, std::string serial, std::string title, u32 crc)
{
	if (!serial.empty())
		s_current_game_subtitle = fmt::format("{0} - {1}", serial, Path::GetFileName(path));
	else
		s_current_game_subtitle = {};

	s_current_game_title = std::move(title);
	s_current_game_serial = std::move(serial);
	s_current_game_path = std::move(path);
	s_current_game_crc = crc;
}

void FullscreenUI::PauseForMenuOpen()
{
	s_was_paused_on_quick_menu_open = (VMManager::GetState() == VMState::Paused);
	if (Host::GetBoolSettingValue("UI", "PauseOnMenu", true) && !s_was_paused_on_quick_menu_open)
		Host::RunOnCPUThread([]() { VMManager::SetPaused(true); });

	s_pause_menu_was_open = true;
}

void FullscreenUI::OpenPauseMenu()
{
	if (!VMManager::HasValidVM())
		return;

	GetMTGS().RunOnGSThread([]() {
		if (!Initialize() || s_current_main_window != MainWindowType::None)
			return;

		PauseForMenuOpen();
		s_current_main_window = MainWindowType::PauseMenu;
		s_current_pause_submenu = PauseSubMenu::None;
		QueueResetFocus();
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
	QueueResetFocus();
}

void FullscreenUI::OpenPauseSubMenu(PauseSubMenu submenu)
{
	s_current_main_window = MainWindowType::PauseMenu;
	s_current_pause_submenu = submenu;
	QueueResetFocus();
}

void FullscreenUI::Shutdown()
{
	CloseSaveStateSelector();
	s_cover_image_map.clear();
	s_game_list_sorted_entries = {};
	s_game_list_directories_cache = {};
	s_fullscreen_mode_list_cache = {};
	s_graphics_adapter_list_cache = {};
	s_hotkey_list_cache = {};
	s_current_game_title = {};
	s_current_game_subtitle = {};
	s_current_game_serial = {};
	s_current_game_path = {};
	s_current_game_crc = 0;
	DestroyResources();
	ImGuiFullscreen::Shutdown();
	s_initialized = false;
	s_tried_to_initialize = false;
}

void FullscreenUI::Render()
{
	if (!s_initialized)
		return;

	for (std::unique_ptr<HostDisplayTexture>& tex : s_cleanup_textures)
		tex.reset();
	s_cleanup_textures.clear();
	ImGuiFullscreen::UploadAsyncTextures();

	ImGuiFullscreen::BeginLayout();

	switch (s_current_main_window)
	{
		case MainWindowType::Landing:
			DrawLandingWindow();
			break;
		case MainWindowType::GameList:
			DrawGameListWindow();
			break;
		case MainWindowType::Settings:
			DrawSettingsWindow();
			break;
		case MainWindowType::PauseMenu:
			DrawPauseMenu(s_current_main_window);
			break;
		default:
			break;
	}

	if (s_save_state_selector_open)
		DrawSaveStateSelector(s_save_state_selector_loading, false);

	if (s_about_window_open)
		DrawAboutWindow();

	if (s_input_binding_type != PAD::ControllerBindingType::Unknown)
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
			s_game_settings_interface->Save();
			if (VMManager::HasValidVM())
				Host::RunOnCPUThread([]() { VMManager::ReloadGameSettings(); });
		}
		s_game_settings_changed.store(false, std::memory_order_release);
	}

	ImGuiFullscreen::ResetCloseMenuIfNeeded();
}

void FullscreenUI::ReturnToMainWindow()
{
	if (s_pause_menu_was_open)
		ClosePauseMenu();

	s_current_main_window = VMManager::HasValidVM() ? MainWindowType::None : MainWindowType::Landing;
}

bool FullscreenUI::LoadResources()
{
	s_app_icon_texture = LoadTexture("icons/AppIconLarge.png");

	s_fallback_disc_texture = LoadTexture("fullscreenui/media-cdrom.png");
	s_fallback_exe_texture = LoadTexture("fullscreenui/applications-system.png");

	for (u32 i = static_cast<u32>(GameDatabaseSchema::Compatibility::Nothing);
		 i <= static_cast<u32>(GameDatabaseSchema::Compatibility::Perfect); i++)
	{
		s_game_compatibility_textures[i - 1] = LoadTexture(fmt::format("icons/star-{}.png", i - 1).c_str());
	}

	return true;
}

void FullscreenUI::DestroyResources()
{
	s_app_icon_texture.reset();
	s_fallback_exe_texture.reset();
	s_fallback_disc_texture.reset();
	for (auto& tex : s_game_compatibility_textures)
		tex.reset();
	for (auto& tex : s_cleanup_textures)
		tex.reset();
}

//////////////////////////////////////////////////////////////////////////
// Utility
//////////////////////////////////////////////////////////////////////////

ImGuiFullscreen::FileSelectorFilters FullscreenUI::GetDiscImageFilters()
{
	return {"*.bin", "*.iso", "*.cue", "*.chd", "*.cso", "*.gz", "*.elf", "*.irx", "*.m3u", "*.gs", "*.gs.xz", "*.gs.zst"};
}

void FullscreenUI::DoStartPath(const std::string& path, std::optional<s32> state_index, std::optional<bool> fast_boot)
{
	VMBootParameters params;
	params.filename = path;
	params.state_index = state_index;
	params.fast_boot = fast_boot;

	Host::RunOnCPUThread([params = std::move(params)]() {
		if (VMManager::HasValidVM())
			return;

		if (VMManager::Initialize(params))
			VMManager::SetState(VMState::Running);
		else
			SwitchToLanding();
	});

	// switch to nothing, we'll get brought back if init fails
	s_current_main_window = MainWindowType::None;
}

void FullscreenUI::DoStartFile()
{
	auto callback = [](const std::string& path) {
		if (!path.empty())
			DoStartPath(path);

		QueueResetFocus();
		CloseFileSelector();
	};

	OpenFileSelector(ICON_FA_FOLDER_OPEN "  Select Disc Image", false, std::move(callback), GetDiscImageFilters());
}

void FullscreenUI::DoStartBIOS()
{
	Host::RunOnCPUThread([]() {
		if (VMManager::HasValidVM())
			return;

		VMBootParameters params;
		if (VMManager::Initialize(params))
			VMManager::SetState(VMState::Running);
		else
			SwitchToLanding();
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
		if (VMManager::Initialize(params))
			VMManager::SetState(VMState::Running);
		else
			SwitchToLanding();
	});
}

void FullscreenUI::DoStartDisc()
{
	std::vector<std::string> devices(GetOpticalDriveList());
	if (devices.empty())
	{
		ShowToast(std::string(),
			"Could not find any CD/DVD-ROM devices. Please ensure you have a drive connected and sufficient permissions to access it.");
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
	OpenChoiceDialog(ICON_FA_COMPACT_DISC "  Select Disc Drive", false, std::move(options), [](s32, const std::string& path, bool) {
		DoStartDisc(path);
		CloseChoiceDialog();
		QueueResetFocus();
	});
}

void FullscreenUI::DoToggleFrameLimit()
{
	Host::RunOnCPUThread([]() {
		if (!VMManager::HasValidVM())
			return;

		VMManager::SetLimiterMode(
			(EmuConfig.LimiterMode != LimiterModeType::Unlimited) ? LimiterModeType::Unlimited : LimiterModeType::Nominal);
	});
}

void FullscreenUI::DoToggleSoftwareRenderer()
{
	Host::RunOnCPUThread([]() {
		if (!VMManager::HasValidVM())
			return;

		GetMTGS().ToggleSoftwareRendering();
	});
}

void FullscreenUI::DoShutdown(bool save_state)
{
	Host::RunOnCPUThread([save_state]() { Host::RequestVMShutdown(false, save_state, save_state); });
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
				ShowToast({}, fmt::format("{} is not a valid disc image.", FileSystem::GetDisplayNameFromPath(path)));
			}
			else
			{
				Host::RunOnCPUThread([path]() { VMManager::ChangeDisc(CDVD_SourceType::Iso, std::move(path)); });
			}
		}

		QueueResetFocus();
		CloseFileSelector();
		ReturnToMainWindow();
	};

	OpenFileSelector(ICON_FA_COMPACT_DISC "  Select Disc Image", false, std::move(callback), GetDiscImageFilters(),
		std::string(Path::GetDirectory(s_current_game_path)));
}

void FullscreenUI::DoChangeDisc()
{
	DoChangeDiscFromFile();
}

void FullscreenUI::DoRequestExit()
{
	Host::RunOnCPUThread([]() { Host::RequestExit(EmuConfig.SaveStateOnShutdown); });
}

void FullscreenUI::DoToggleFullscreen()
{
	Host::RunOnCPUThread([]() { Host::SetFullscreen(!Host::IsFullscreen()); });
}

//////////////////////////////////////////////////////////////////////////
// Landing Window
//////////////////////////////////////////////////////////////////////////

void FullscreenUI::SwitchToLanding()
{
	s_current_main_window = MainWindowType::Landing;
	QueueResetFocus();
}

void FullscreenUI::DrawLandingWindow()
{
	BeginFullscreenColumns();

	if (BeginFullscreenColumnWindow(0.0f, 570.0f, "logo", UIPrimaryDarkColor))
	{
		const float image_size = LayoutScale(380.f);
		ImGui::SetCursorPos(
			ImVec2((ImGui::GetWindowWidth() * 0.5f) - (image_size * 0.5f), (ImGui::GetWindowHeight() * 0.5f) - (image_size * 0.5f)));
		ImGui::Image(s_app_icon_texture->GetHandle(), ImVec2(image_size, image_size));

		ImGui::SetCursorPos(ImVec2(LayoutScale(20.0f), ImGui::GetWindowHeight() - g_medium_font->FontSize - LayoutScale(20.0f)));
		ImGui::PushFont(g_medium_font);
		ImGui::Text(GIT_REV);
		ImGui::PopFont();
	}
	EndFullscreenColumnWindow();

	if (BeginFullscreenColumnWindow(570.0f, LAYOUT_SCREEN_WIDTH, "menu", UIBackgroundColor))
	{
		ResetFocusHere();

		BeginMenuButtons(6, 0.5f);

		if (MenuButton(" " ICON_FA_LIST "  Game List", "Launch a game from images scanned from your game directories."))
		{
			SwitchToGameList();
		}

		if (MenuButton(" " ICON_FA_FOLDER_OPEN "  Start File", "Launch a game by selecting a file/disc image."))
		{
			DoStartFile();
		}

		if (MenuButton(" " ICON_FA_TOOLBOX "  Start BIOS", "Start the console without any disc inserted."))
		{
			DoStartBIOS();
		}

		if (MenuButton(" " ICON_FA_COMPACT_DISC "  Start Disc", "Start a game from a disc in your PC's DVD drive."))
		{
			DoStartDisc();
		}

		if (MenuButton(" " ICON_FA_SLIDERS_H "  Settings", "Change settings for the emulator."))
			SwitchToSettings();

		if (MenuButton(" " ICON_FA_SIGN_OUT_ALT "  Exit", "Exits the program."))
		{
			DoRequestExit();
		}

		{
			ImVec2 fullscreen_pos;
			if (FloatingButton(ICON_FA_WINDOW_CLOSE, 0.0f, 0.0f, -1.0f, -1.0f, 1.0f, 0.0f, true, g_large_font, &fullscreen_pos))
			{
				DoRequestExit();
			}

			if (FloatingButton(ICON_FA_EXPAND, fullscreen_pos.x, 0.0f, -1.0f, -1.0f, -1.0f, 0.0f, true, g_large_font, &fullscreen_pos))
			{
				DoToggleFullscreen();
			}

			if (FloatingButton(ICON_FA_QUESTION_CIRCLE, fullscreen_pos.x, 0.0f, -1.0f, -1.0f, -1.0f, 0.0f))
				OpenAboutWindow();
		}

		EndMenuButtons();
	}

	EndFullscreenColumnWindow();

	EndFullscreenColumns();
}

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

void FullscreenUI::DrawInputBindingButton(SettingsInterface* bsi, PAD::ControllerBindingType type, const char* section, const char* name,
	const char* display_name, bool show_type)
{
	std::string title(fmt::format("{}/{}", section, name));

	ImRect bb;
	bool visible, hovered, clicked;
	clicked = MenuButtonFrame(title.c_str(), true, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, &visible, &hovered, &bb.Min, &bb.Max);
	if (!visible)
		return;

	const float midpoint = bb.Min.y + g_large_font->FontSize + LayoutScale(4.0f);
	const ImRect title_bb(bb.Min, ImVec2(bb.Max.x, midpoint));
	const ImRect summary_bb(ImVec2(bb.Min.x, midpoint), bb.Max);

	if (show_type)
	{
		switch (type)
		{
			case PAD::ControllerBindingType::Button:
				title = fmt::format(ICON_FA_DOT_CIRCLE "  {}", display_name);
				break;
			case PAD::ControllerBindingType::Axis:
			case PAD::ControllerBindingType::HalfAxis:
				title = fmt::format(ICON_FA_BULLSEYE "  {}", display_name);
				break;
			case PAD::ControllerBindingType::Motor:
				title = fmt::format(ICON_FA_BELL "  {}", display_name);
				break;
			case PAD::ControllerBindingType::Macro:
				title = fmt::format(ICON_FA_PIZZA_SLICE "  {}", display_name);
				break;
			default:
				title = display_name;
				break;
		}
	}

	ImGui::PushFont(g_large_font);
	ImGui::RenderTextClipped(
		title_bb.Min, title_bb.Max, show_type ? title.c_str() : display_name, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
	ImGui::PopFont();

	const std::string value(bsi->GetStringValue(section, name));
	ImGui::PushFont(g_medium_font);
	ImGui::RenderTextClipped(
		summary_bb.Min, summary_bb.Max, value.empty() ? "No Binding" : value.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
	ImGui::PopFont();

	if (clicked)
	{
		BeginInputBinding(bsi, type, section, name, display_name);
	}
	else if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		bsi->DeleteValue(section, name);
		SetSettingsChanged(bsi);
	}
}

void FullscreenUI::ClearInputBindingVariables()
{
	s_input_binding_type = PAD::ControllerBindingType::Unknown;
	s_input_binding_section = {};
	s_input_binding_key = {};
	s_input_binding_display_name = {};
	s_input_binding_new_bindings = {};
}

void FullscreenUI::BeginInputBinding(SettingsInterface* bsi, PAD::ControllerBindingType type, const std::string_view& section,
	const std::string_view& key, const std::string_view& display_name)
{
	if (s_input_binding_type != PAD::ControllerBindingType::Unknown)
	{
		InputManager::RemoveHook();
		ClearInputBindingVariables();
	}

	s_input_binding_type = type;
	s_input_binding_section = section;
	s_input_binding_key = key;
	s_input_binding_display_name = display_name;
	s_input_binding_new_bindings = {};
	s_input_binding_timer.Reset();

	const bool game_settings = IsEditingGameSettings(bsi);

	InputManager::SetHook([game_settings](InputBindingKey key, float value) -> InputInterceptHook::CallbackResult {
		// holding the settings lock here will protect the input binding list
		auto lock = Host::GetSettingsLock();

		const float abs_value = std::abs(value);

		for (InputBindingKey other_key : s_input_binding_new_bindings)
		{
			if (other_key.MaskDirection() == key.MaskDirection())
			{
				if (abs_value < 0.5f)
				{
					// if this key is in our new binding list, it's a "release", and we're done
					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					const std::string new_binding(InputManager::ConvertInputBindingKeysToString(
						s_input_binding_new_bindings.data(), s_input_binding_new_bindings.size()));
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
		if (abs_value >= 0.5f)
		{
			InputBindingKey key_to_add = key;
			key_to_add.negative = (value < 0.0f);
			s_input_binding_new_bindings.push_back(key_to_add);
		}

		return InputInterceptHook::CallbackResult::StopProcessingEvent;
	});
}

void FullscreenUI::DrawInputBindingWindow()
{
	pxAssert(s_input_binding_type != PAD::ControllerBindingType::Unknown);

	const double time_remaining = INPUT_BINDING_TIMEOUT_SECONDS - s_input_binding_timer.GetTimeSeconds();
	if (time_remaining <= 0.0)
	{
		InputManager::RemoveHook();
		ClearInputBindingVariables();
		return;
	}

	const char* title = ICON_FA_GAMEPAD "  Set Input Binding";
	ImGui::SetNextWindowSize(LayoutScale(500.0f, 0.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::OpenPopup(title);

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));

	if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs))
	{
		ImGui::TextWrapped("Setting %s binding %s.", s_input_binding_section.c_str(), s_input_binding_display_name.c_str());
		ImGui::TextUnformatted("Push a controller button or axis now.");
		ImGui::NewLine();
		ImGui::Text("Timing out in %.0f seconds...", time_remaining);
		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(3);
	ImGui::PopFont();
}

bool FullscreenUI::DrawToggleSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
	bool default_value, bool enabled, bool allow_tristate, float height, ImFont* font, ImFont* summary_font)
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
	int default_value, const char* const* options, size_t option_count, int option_offset, bool enabled, float height, ImFont* font,
	ImFont* summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<int> value =
		bsi->GetOptionalIntValue(section, key, game_settings ? std::nullopt : std::optional<int>(default_value));
	const int index = value.has_value() ? (value.value() - option_offset) : std::numeric_limits<int>::min();
	const char* value_text = (value.has_value()) ?
								 ((index < 0 || static_cast<size_t>(index) >= option_count) ? "Unknown" : options[index]) :
                                 "Use Global Setting";

	if (MenuButtonWithValue(title, summary, value_text, enabled, height, font, summary_font))
	{
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(option_count + 1);
		if (game_settings)
			cd_options.emplace_back("Use Global Setting", !value.has_value());
		for (size_t i = 0; i < option_count; i++)
			cd_options.emplace_back(options[i], (i == static_cast<size_t>(index)));
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
	int default_value, int min_value, int max_value, const char* format, bool enabled, float height, ImFont* font, ImFont* summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<int> value =
		bsi->GetOptionalIntValue(section, key, game_settings ? std::nullopt : std::optional<int>(default_value));
	const std::string value_text(
		value.has_value() ? StringUtil::StdStringFromFormat(format, value.value()) : std::string("Use Global Setting"));

	if (MenuButtonWithValue(title, summary, value_text.c_str(), enabled, height, font, summary_font))
		ImGui::OpenPopup(title);

	ImGui::SetNextWindowSize(LayoutScale(500.0f, 180.0f));

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));

	if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::SetNextItemWidth(LayoutScale(450.0f));
		s32 dlg_value = static_cast<s32>(value.value_or(default_value));
		if (ImGui::SliderInt("##value", &dlg_value, min_value, max_value, format, ImGuiSliderFlags_NoInput))
		{
			if (IsEditingGameSettings(bsi) && dlg_value == default_value)
				bsi->DeleteValue(section, key);
			else
				bsi->SetIntValue(section, key, dlg_value);

			SetSettingsChanged(bsi);
		}

		BeginMenuButtons();
		if (MenuButton("OK", nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
			ImGui::CloseCurrentPopup();
		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(3);
	ImGui::PopFont();
}

void FullscreenUI::DrawFloatRangeSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* key, float default_value, float min_value, float max_value, const char* format, float multiplier, bool enabled,
	float height, ImFont* font, ImFont* summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<float> value =
		bsi->GetOptionalFloatValue(section, key, game_settings ? std::nullopt : std::optional<float>(default_value));
	const std::string value_text(
		value.has_value() ? StringUtil::StdStringFromFormat(format, value.value() * multiplier) : std::string("Use Global Setting"));

	if (MenuButtonWithValue(title, summary, value_text.c_str(), enabled, height, font, summary_font))
		ImGui::OpenPopup(title);

	ImGui::SetNextWindowSize(LayoutScale(500.0f, 180.0f));

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));

	if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::SetNextItemWidth(LayoutScale(450.0f));
		float dlg_value = value.value_or(default_value) * multiplier;
		if (ImGui::SliderFloat("##value", &dlg_value, min_value, max_value, format, ImGuiSliderFlags_NoInput))
		{
			dlg_value /= multiplier;

			if (IsEditingGameSettings(bsi) && dlg_value == default_value)
				bsi->DeleteValue(section, key);
			else
				bsi->SetFloatValue(section, key, dlg_value);

			SetSettingsChanged(bsi);
		}

		BeginMenuButtons();
		if (MenuButton("OK", nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
			ImGui::CloseCurrentPopup();
		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(3);
	ImGui::PopFont();
}

void FullscreenUI::DrawIntRectSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* left_key, int default_left, const char* top_key, int default_top, const char* right_key, int default_right,
	const char* bottom_key, int default_bottom, int min_value, int max_value, const char* format, bool enabled, float height, ImFont* font,
	ImFont* summary_font)
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
	const std::string value_text(fmt::format("{}/{}/{}/{}",
		left_value.has_value() ? StringUtil::StdStringFromFormat(format, left_value.value()) : std::string("Default"),
		top_value.has_value() ? StringUtil::StdStringFromFormat(format, top_value.value()) : std::string("Default"),
		right_value.has_value() ? StringUtil::StdStringFromFormat(format, right_value.value()) : std::string("Default"),
		bottom_value.has_value() ? StringUtil::StdStringFromFormat(format, bottom_value.value()) : std::string("Default")));

	if (MenuButtonWithValue(title, summary, value_text.c_str(), enabled, height, font, summary_font))
		ImGui::OpenPopup(title);

	ImGui::SetNextWindowSize(LayoutScale(500.0f, 320.0f));

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));

	if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		s32 dlg_left_value = static_cast<s32>(left_value.value_or(default_left));
		s32 dlg_top_value = static_cast<s32>(top_value.value_or(default_top));
		s32 dlg_right_value = static_cast<s32>(right_value.value_or(default_right));
		s32 dlg_bottom_value = static_cast<s32>(bottom_value.value_or(default_bottom));

		ImGui::TextUnformatted("Left: ");
		ImGui::SameLine();
		const bool left_modified = ImGui::SliderInt("##left", &dlg_left_value, min_value, max_value, format, ImGuiSliderFlags_NoInput);
		ImGui::TextUnformatted("Top: ");
		ImGui::SameLine();
		const bool top_modified = ImGui::SliderInt("##top", &dlg_top_value, min_value, max_value, format, ImGuiSliderFlags_NoInput);
		ImGui::TextUnformatted("Right: ");
		ImGui::SameLine();
		const bool right_modified = ImGui::SliderInt("##right", &dlg_right_value, min_value, max_value, format, ImGuiSliderFlags_NoInput);
		ImGui::TextUnformatted("Bottom: ");
		ImGui::SameLine();
		const bool bottom_modified =
			ImGui::SliderInt("##bottom", &dlg_bottom_value, min_value, max_value, format, ImGuiSliderFlags_NoInput);
		if (left_modified)
		{
			if (IsEditingGameSettings(bsi) && dlg_left_value == default_left)
				bsi->DeleteValue(section, left_key);
			else
				bsi->SetIntValue(section, left_key, dlg_left_value);
		}
		if (top_modified)
		{
			if (IsEditingGameSettings(bsi) && dlg_top_value == default_top)
				bsi->DeleteValue(section, top_key);
			else
				bsi->SetIntValue(section, top_key, dlg_top_value);
		}
		if (right_modified)
		{
			if (IsEditingGameSettings(bsi) && dlg_right_value == default_right)
				bsi->DeleteValue(section, right_key);
			else
				bsi->SetIntValue(section, right_key, dlg_right_value);
		}
		if (bottom_modified)
		{
			if (IsEditingGameSettings(bsi) && dlg_bottom_value == default_bottom)
				bsi->DeleteValue(section, bottom_key);
			else
				bsi->SetIntValue(section, bottom_key, dlg_bottom_value);
		}

		if (left_modified || top_modified || right_modified || bottom_modified)
			SetSettingsChanged(bsi);

		BeginMenuButtons();
		if (MenuButton("OK", nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
			ImGui::CloseCurrentPopup();
		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(3);
	ImGui::PopFont();
}

void FullscreenUI::DrawStringListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* key, const char* default_value, const char* const* options, const char* const* option_values, size_t option_count,
	bool enabled, float height, ImFont* font, ImFont* summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<std::string> value(
		bsi->GetOptionalStringValue(section, key, game_settings ? std::nullopt : std::optional<const char*>(default_value)));

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
			value.has_value() ? ((index < option_count) ? options[index] : "Unknown") : "Use Global Setting", enabled, height, font,
			summary_font))
	{
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(option_count + 1);
		if (game_settings)
			cd_options.emplace_back("Use Global Setting", !value.has_value());
		for (size_t i = 0; i < option_count; i++)
			cd_options.emplace_back(options[i], (value.has_value() && i == static_cast<size_t>(index)));
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

void FullscreenUI::DrawFloatListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* key, float default_value, const char* const* options, const float* option_values, size_t option_count, bool enabled,
	float height, ImFont* font, ImFont* summary_font)
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
			value.has_value() ? ((index < option_count) ? options[index] : "Unknown") : "Use Global Setting", enabled, height, font,
			summary_font))
	{
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(option_count + 1);
		if (game_settings)
			cd_options.emplace_back("Use Global Setting", !value.has_value());
		for (size_t i = 0; i < option_count; i++)
			cd_options.emplace_back(options[i], (value.has_value() && i == static_cast<size_t>(index)));
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

void FullscreenUI::DrawFolderSetting(SettingsInterface* bsi, const char* title, const char* section, const char* key,
	const std::string& runtime_var, float height /* = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT */, ImFont* font /* = g_large_font */,
	ImFont* summary_font /* = g_medium_font */)
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

				Host::RunOnCPUThread(&Host::Internal::UpdateEmuFolders);

				CloseFileSelector();
			});
	}
}

void FullscreenUI::StartAutomaticBinding(u32 port)
{
	// messy because the enumeration has to happen on the input thread
	Host::RunOnCPUThread([port]() {
		std::vector<std::pair<std::string, std::string>> devices(InputManager::EnumerateDevices());
		GetMTGS().RunOnGSThread([port, devices = std::move(devices)]() {
			if (devices.empty())
			{
				ShowToast({}, "Automatic binding failed, no devices are available.");
				return;
			}

			std::vector<std::string> names;
			ImGuiFullscreen::ChoiceDialogOptions options;
			options.reserve(devices.size());
			names.reserve(devices.size());
			for (auto& [name, display_name] : devices)
			{
				names.push_back(std::move(name));
				options.emplace_back(std::move(display_name), false);
			}
			OpenChoiceDialog("Select Device", false, std::move(options),
				[port, names = std::move(names)](s32 index, const std::string& title, bool checked) {
					if (index < 0)
						return;

					// since this is working with the device, it has to happen on the input thread too
					Host::RunOnCPUThread([port, name = std::move(names[index])]() {
						auto lock = Host::GetSettingsLock();
						const bool result =
							PAD::MapController(*GetEditingSettingsInterface(), port, InputManager::GetGenericBindingMapping(name));

						// and the toast needs to happen on the UI thread.
						GetMTGS().RunOnGSThread([result, name = std::move(name)]() {
							ShowToast({}, result ? fmt::format("Automatic mapping completed for {}.", name) :
                                                   fmt::format("Automatic mapping failed for {}.", name));
						});
					});
					CloseChoiceDialog();
				});
		});
	});
}

void FullscreenUI::SwitchToSettings()
{
	s_game_settings_entry.reset();
	s_game_settings_interface.reset();

	// populate the cache with all settings from ini
	auto lock = Host::GetSettingsLock();
	SettingsInterface* bsi = Host::Internal::GetBaseSettingsLayer();

	PopulateGameListDirectoryCache(bsi);
	PopulateGraphicsAdapterList();

	s_current_main_window = MainWindowType::Settings;
	s_settings_page = SettingsPage::Interface;
}

void FullscreenUI::SwitchToGameSettings(const std::string_view& serial, u32 crc)
{
	s_game_settings_entry.reset();
	s_game_settings_interface = std::make_unique<INISettingsInterface>(VMManager::GetGameSettingsPath(serial, crc));
	s_game_settings_interface->Load();
	s_current_main_window = MainWindowType::Settings;
	s_settings_page = SettingsPage::Summary;
	QueueResetFocus();
}

void FullscreenUI::SwitchToGameSettings()
{
	if (s_current_game_serial.empty() || s_current_game_crc == 0)
		return;

	auto lock = GameList::GetLock();
	const GameList::Entry* entry = GameList::GetEntryForPath(s_current_game_path.c_str());
	if (!entry)
	{
		entry = GameList::GetEntryBySerialAndCRC(s_current_game_serial.c_str(), s_current_game_crc);
		if (!entry)
		{
			SwitchToGameSettings(s_current_game_serial.c_str(), s_current_game_crc);
			return;
		}
	}

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
	SwitchToGameSettings(entry->serial.c_str(), entry->crc);
	s_game_settings_entry = std::make_unique<GameList::Entry>(*entry);
}

void FullscreenUI::PopulateGraphicsAdapterList()
{
	HostDisplay::AdapterAndModeList ml(g_host_display->GetAdapterAndModeList());
	s_graphics_adapter_list_cache = std::move(ml.adapter_names);
	s_fullscreen_mode_list_cache = std::move(ml.fullscreen_modes);
	s_fullscreen_mode_list_cache.insert(s_fullscreen_mode_list_cache.begin(), "Borderless Fullscreen");
}

void FullscreenUI::PopulateGameListDirectoryCache(SettingsInterface* si)
{
	s_game_list_directories_cache.clear();
	for (std::string& dir : si->GetStringList("GameList", "Paths"))
		s_game_list_directories_cache.emplace_back(std::move(dir), false);
	for (std::string& dir : si->GetStringList("GameList", "RecursivePaths"))
		s_game_list_directories_cache.emplace_back(std::move(dir), true);
}

ImGuiFullscreen::ChoiceDialogOptions FullscreenUI::GetGameListDirectoryOptions(bool recursive_as_checked)
{
	ImGuiFullscreen::ChoiceDialogOptions options;
	for (const auto& it : s_game_list_directories_cache)
		options.emplace_back(it.first, it.second && recursive_as_checked);
	return options;
}

void FullscreenUI::DoCopyGameSettings()
{
	if (!s_game_settings_interface)
		return;

	Pcsx2Config temp;
	{
		SettingsLoadWrapper wrapper(*GetEditingSettingsInterface(false));
		temp.LoadSave(wrapper);
	}
	{
		SettingsSaveWrapper wrapper(*s_game_settings_interface.get());
		temp.LoadSave(wrapper);
	}

	SetSettingsChanged(s_game_settings_interface.get());

	ShowToast(std::string(), fmt::format("Game settings initialized with global settings for '{}'.",
								 Path::GetFileTitle(s_game_settings_interface->GetFileName())));
}

void FullscreenUI::DoClearGameSettings()
{
	if (!s_game_settings_interface)
		return;

	s_game_settings_interface->Clear();
	if (!s_game_settings_interface->GetFileName().empty())
		FileSystem::DeleteFilePath(s_game_settings_interface->GetFileName().c_str());

	SetSettingsChanged(s_game_settings_interface.get());

	ShowToast(std::string(),
		fmt::format("Game settings have been cleared for '{}'.", Path::GetFileTitle(s_game_settings_interface->GetFileName())));
}

void FullscreenUI::DrawSettingsWindow()
{
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 heading_size =
		ImVec2(io.DisplaySize.x, LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY + LAYOUT_MENU_BUTTON_Y_PADDING * 2.0f + 2.0f));

	const float bg_alpha = VMManager::HasValidVM() ? 0.90f : 1.0f;

	if (BeginFullscreenWindow(ImVec2(0.0f, 0.0f), heading_size, "settings_category", UIPrimaryColor))
	{
		static constexpr float ITEM_WIDTH = 22.0f;

		static constexpr const char* global_icons[] = {ICON_FA_WINDOW_MAXIMIZE, ICON_FA_LIST, ICON_FA_MICROCHIP, ICON_FA_SLIDERS_H,
			ICON_FA_HDD, ICON_FA_MAGIC, ICON_FA_HEADPHONES, ICON_FA_SD_CARD, ICON_FA_GAMEPAD, ICON_FA_KEYBOARD, ICON_FA_TROPHY,
			ICON_FA_COGS};
		static constexpr const char* per_game_icons[] = {ICON_FA_PARAGRAPH, ICON_FA_SLIDERS_H, ICON_FA_HDD, ICON_FA_MAGIC,
			ICON_FA_HEADPHONES, ICON_FA_SD_CARD, ICON_FA_GAMEPAD, ICON_FA_BAN};
		static constexpr SettingsPage global_pages[] = {SettingsPage::Interface, SettingsPage::GameList, SettingsPage::BIOS,
			SettingsPage::Emulation, SettingsPage::System, SettingsPage::Graphics, SettingsPage::Audio, SettingsPage::MemoryCard,
			SettingsPage::Controller, SettingsPage::Hotkey, SettingsPage::Achievements, SettingsPage::Advanced};
		static constexpr SettingsPage per_game_pages[] = {SettingsPage::Summary, SettingsPage::Emulation, SettingsPage::System,
			SettingsPage::Graphics, SettingsPage::Audio, SettingsPage::MemoryCard, SettingsPage::Controller, SettingsPage::GameFixes};
		static constexpr const char* titles[] = {"Summary", "Interface Settings", "Game List Settings", "BIOS Settings",
			"Emulation Settings", "System Settings", "Graphics Settings", "Audio Settings", "Memory Card Settings", "Controller Settings",
			"Hotkey Settings", "Achievements Settings", "Advanced Settings", "Game Fixes"};

		const bool game_settings = IsEditingGameSettings(GetEditingSettingsInterface());
		const u32 count = game_settings ? std::size(per_game_pages) : std::size(global_pages);
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

		if (ImGui::IsNavInputTest(ImGuiNavInput_FocusPrev, ImGuiNavReadMode_Pressed))
		{
			index = (index == 0) ? (count - 1) : (index - 1);
			s_settings_page = pages[index];
		}
		else if (ImGui::IsNavInputTest(ImGuiNavInput_FocusNext, ImGuiNavReadMode_Pressed))
		{
			index = (index + 1) % count;
			s_settings_page = pages[index];
		}

		if (NavButton(ICON_FA_BACKWARD, true, true))
			ReturnToMainWindow();

		if (s_game_settings_entry)
			NavTitle(fmt::format("{} ({})", titles[static_cast<u32>(pages[index])], s_game_settings_entry->title).c_str());
		else
			NavTitle(titles[static_cast<u32>(pages[index])]);

		RightAlignNavButtons(count, ITEM_WIDTH, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

		for (u32 i = 0; i < count; i++)
		{
			if (NavButton(icons[i], i == index, true, ITEM_WIDTH, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
			{
				s_settings_page = pages[i];
			}
		}

		EndNavBar();
	}

	EndFullscreenWindow();

	if (BeginFullscreenWindow(ImVec2(0.0f, heading_size.y), ImVec2(io.DisplaySize.x, io.DisplaySize.y - heading_size.y), "settings_parent",
			ImVec4(UIBackgroundColor.x, UIBackgroundColor.y, UIBackgroundColor.z, bg_alpha)))
	{
		ResetFocusHere();

		if (WantsToCloseMenu())
		{
			if (ImGui::IsWindowFocused())
				ReturnToMainWindow();
		}

		auto lock = Host::GetSettingsLock();

		switch (s_settings_page)
		{
			case SettingsPage::Summary:
				DrawSummarySettingsPage();
				break;

			case SettingsPage::Interface:
				DrawInterfaceSettingsPage();
				break;

			case SettingsPage::GameList:
				DrawGameListSettingsPage();
				break;

			case SettingsPage::BIOS:
				DrawBIOSSettingsPage();
				break;

			case SettingsPage::Emulation:
				DrawEmulationSettingsPage();
				break;

			case SettingsPage::System:
				DrawSystemSettingsPage();
				break;

			case SettingsPage::Graphics:
				DrawGraphicsSettingsPage();
				break;

			case SettingsPage::Audio:
				DrawAudioSettingsPage();
				break;

			case SettingsPage::MemoryCard:
				DrawMemoryCardSettingsPage();
				break;

			case SettingsPage::Controller:
				DrawControllerSettingsPage();
				break;

			case SettingsPage::Hotkey:
				DrawHotkeySettingsPage();
				break;

			case SettingsPage::Achievements:
				DrawAchievementsSettingsPage();
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
}

void FullscreenUI::DrawSummarySettingsPage()
{
	BeginMenuButtons();

	MenuHeading("Details");

	if (s_game_settings_entry)
	{
		if (MenuButton(ICON_FA_WINDOW_MAXIMIZE "  Title", s_game_settings_entry->title.c_str(), true))
			CopyTextToClipboard("Game title copied to clipboard.", s_game_settings_entry->title);
		if (MenuButton(ICON_FA_PAGER "  Serial", s_game_settings_entry->serial.c_str(), true))
			CopyTextToClipboard("Game serial copied to clipboard.", s_game_settings_entry->serial);
		if (MenuButton(ICON_FA_CODE "  CRC", fmt::format("{:08X}", s_game_settings_entry->crc).c_str(), true))
			CopyTextToClipboard("Game CRC copied to clipboard.", fmt::format("{:08X}", s_game_settings_entry->crc));
		if (MenuButton(ICON_FA_COMPACT_DISC "  Type", GameList::EntryTypeToString(s_game_settings_entry->type), true))
			CopyTextToClipboard("Game type copied to clipboard.", GameList::EntryTypeToString(s_game_settings_entry->type));
		if (MenuButton(ICON_FA_BOX "  Region", GameList::RegionToString(s_game_settings_entry->region), true))
			CopyTextToClipboard("Game region copied to clipboard.", GameList::RegionToString(s_game_settings_entry->region));
		if (MenuButton(ICON_FA_STAR "  Compatibility Rating",
				GameList::EntryCompatibilityRatingToString(s_game_settings_entry->compatibility_rating), true))
		{
			CopyTextToClipboard("Game compatibility copied to clipboard.",
				GameList::EntryCompatibilityRatingToString(s_game_settings_entry->compatibility_rating));
		}
		if (MenuButton(ICON_FA_FOLDER_OPEN "  Path", s_game_settings_entry->path.c_str(), true))
			CopyTextToClipboard("Game path copied to clipboard.", s_game_settings_entry->path);
	}
	else
	{
		MenuButton(ICON_FA_BAN "  Details unavailable for game not scanned in game list.", "");
	}

	MenuHeading("Options");

	if (MenuButton(ICON_FA_COPY "  Copy Settings", "Copies the current global settings to this game."))
		DoCopyGameSettings();
	if (MenuButton(ICON_FA_TRASH "  Clear Settings", "Clears all settings set for this game."))
		DoClearGameSettings();

	EndMenuButtons();
}

void FullscreenUI::DrawInterfaceSettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading("Behaviour");

	DrawToggleSetting(bsi, ICON_FA_MAGIC "  Inhibit Screensaver",
		"Prevents the screen saver from activating and the host from sleeping while emulation is running.", "UI", "InhibitScreensaver",
		true);
#ifdef WITH_DISCORD_PRESENCE
	DrawToggleSetting(bsi, "Enable Discord Presence", "Shows the game you are currently playing as part of your profile on Discord.", "UI",
		"DiscordPresence", false);
#endif
	DrawToggleSetting(bsi, ICON_FA_PAUSE "  Pause On Start", "Pauses the emulator when a game is started.", "UI", "StartPaused", false);
	DrawToggleSetting(bsi, ICON_FA_VIDEO "  Pause On Focus Loss",
		"Pauses the emulator when you minimize the window or switch to another application, and unpauses when you switch back.", "UI",
		"PauseOnFocusLoss", false);
	DrawToggleSetting(bsi, ICON_FA_WINDOW_MAXIMIZE "  Pause On Menu",
		"Pauses the emulator when you open the quick menu, and unpauses when you close it.", "UI", "PauseOnMenu", true);
	DrawToggleSetting(bsi, ICON_FA_POWER_OFF "  Confirm Shutdown",
		"Determines whether a prompt will be displayed to confirm shutting down the emulator/game when the hotkey is pressed.", "UI",
		"ConfirmShutdown", true);
	DrawToggleSetting(bsi, ICON_FA_SAVE "  Save State On Shutdown",
		"Automatically saves the emulator state when powering down or exiting. You can then resume directly from where you left off next "
		"time.",
		"EmuCore", "SaveStateOnShutdown", false);

	MenuHeading("Game Display");
	DrawToggleSetting(bsi, ICON_FA_TV "  Start Fullscreen", "Automatically switches to fullscreen mode when the program is started.", "UI",
		"StartFullscreen", false);
	DrawToggleSetting(bsi, ICON_FA_MOUSE "  Double-Click Toggles Fullscreen",
		"Switches between full screen and windowed when the window is double-clicked.", "UI", "DoubleClickTogglesFullscreen", true);
	DrawToggleSetting(bsi, ICON_FA_MOUSE_POINTER "  Hide Cursor In Fullscreen",
		"Hides the mouse pointer/cursor when the emulator is in fullscreen mode.", "UI", "HideMouseCursor", false);

	MenuHeading("On-Screen Display");
	DrawIntRangeSetting(bsi, ICON_FA_SEARCH "  OSD Scale", "Determines how large the on-screen messages and monitor are.", "EmuCore/GS",
		"OsdScale", 100, 25, 500, "%d%%");
	DrawToggleSetting(bsi, ICON_FA_LIST "  Show Messages",
		"Shows on-screen-display messages when events occur such as save states being created/loaded, screenshots being taken, etc.",
		"EmuCore/GS", "OsdShowMessages", true);
	DrawToggleSetting(bsi, ICON_FA_CLOCK "  Show Speed",
		"Shows the current emulation speed of the system in the top-right corner of the display as a percentage.", "EmuCore/GS",
		"OsdShowSpeed", false);
	DrawToggleSetting(bsi, ICON_FA_RULER "  Show FPS",
		"Shows the number of video frames (or v-syncs) displayed per second by the system in the top-right corner of the display.",
		"EmuCore/GS", "OsdShowFPS", false);
	DrawToggleSetting(bsi, ICON_FA_BATTERY_HALF "  Show CPU Usage",
		"Shows the CPU usage based on threads in the top-right corner of the display.", "EmuCore/GS", "OsdShowCPU", false);
	DrawToggleSetting(bsi, ICON_FA_SPINNER "  Show GPU Usage", "Shows the host's GPU usage in the top-right corner of the display.",
		"EmuCore/GS", "OsdShowGPU", false);
	DrawToggleSetting(bsi, ICON_FA_RULER_VERTICAL "  Show Resolution",
		"Shows the resolution the game is rendering at in the top-right corner of the display.", "EmuCore/GS", "OsdShowResolution", false);
	DrawToggleSetting(bsi, ICON_FA_BARS "  Show GS Statistics",
		"Shows statistics about GS (primitives, draw calls) in the top-right corner of the display.", "EmuCore/GS", "OsdShowGSStats",
		false);
	DrawToggleSetting(bsi, ICON_FA_PLAY "  Show Status Indicators",
		"Shows indicators when fast forwarding, pausing, and other abnormal states are active.", "EmuCore/GS", "OsdShowIndicators", true);

	MenuHeading("Operations");
	if (MenuButton(ICON_FA_FOLDER_MINUS "  Reset Settings", "Resets configuration to defaults (excluding controller settings).",
			!IsEditingGameSettings(bsi)))
	{
		DoResetSettings();
	}

	EndMenuButtons();
}

void FullscreenUI::DrawGameListSettingsPage()
{
	BeginMenuButtons();

	MenuHeading("Game List");

	if (MenuButton(ICON_FA_FOLDER_PLUS "  Add Search Directory", "Adds a new directory to the game search list."))
	{
		OpenFileSelector(ICON_FA_FOLDER_PLUS "  Add Search Directory", true, [](const std::string& dir) {
			if (!dir.empty())
			{
				auto lock = Host::GetSettingsLock();
				SettingsInterface* bsi = Host::Internal::GetBaseSettingsLayer();

				bsi->AddToStringList("GameList", "RecursivePaths", dir.c_str());
				bsi->RemoveFromStringList("GameList", "Paths", dir.c_str());
				bsi->Save();
				PopulateGameListDirectoryCache(bsi);
				Host::RefreshGameListAsync(false);
			}

			CloseFileSelector();
		});
	}

	if (MenuButton(
			ICON_FA_FOLDER_OPEN "  Change Recursive Directories", "Sets whether subdirectories are searched for each game directory"))
	{
		OpenChoiceDialog(ICON_FA_FOLDER_OPEN "  Change Recursive Directories", true, GetGameListDirectoryOptions(true),
			[](s32 index, const std::string& title, bool checked) {
				if (index < 0)
					return;

				auto lock = Host::GetSettingsLock();
				SettingsInterface* bsi = Host::Internal::GetBaseSettingsLayer();
				if (checked)
				{
					bsi->RemoveFromStringList("GameList", "Paths", title.c_str());
					bsi->AddToStringList("GameList", "RecursivePaths", title.c_str());
				}
				else
				{
					bsi->RemoveFromStringList("GameList", "RecursivePaths", title.c_str());
					bsi->AddToStringList("GameList", "Paths", title.c_str());
				}

				bsi->Save();
				PopulateGameListDirectoryCache(bsi);
				Host::RefreshGameListAsync(false);
			});
	}

	if (MenuButton(ICON_FA_FOLDER_MINUS "  Remove Search Directory", "Removes a directory from the game search list."))
	{
		OpenChoiceDialog(ICON_FA_FOLDER_MINUS "  Remove Search Directory", false, GetGameListDirectoryOptions(false),
			[](s32 index, const std::string& title, bool checked) {
				if (index < 0)
					return;

				auto lock = Host::GetSettingsLock();
				SettingsInterface* bsi = Host::Internal::GetBaseSettingsLayer();
				bsi->RemoveFromStringList("GameList", "Paths", title.c_str());
				bsi->RemoveFromStringList("GameList", "RecursivePaths", title.c_str());
				bsi->Save();
				PopulateGameListDirectoryCache(bsi);
				Host::RefreshGameListAsync(false);
				CloseChoiceDialog();
			});
	}

	if (MenuButton(ICON_FA_SEARCH "  Scan For New Games", "Identifies any new files added to the game directories."))
		Host::RefreshGameListAsync(false);
	if (MenuButton(ICON_FA_SEARCH_PLUS "  Rescan All Games", "Forces a full rescan of all games previously identified."))
		Host::RefreshGameListAsync(true);

	MenuHeading("Search Directories");
	for (const auto& it : s_game_list_directories_cache)
		MenuButton(it.first.c_str(), it.second ? "Scanning Subdirectories" : "Not Scanning Subdirectories", false);

	EndMenuButtons();
}

void FullscreenUI::DrawBIOSSettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading("BIOS Configuration");

	DrawFolderSetting(bsi, ICON_FA_FOLDER_OPEN "  Change Search Directory", "Folders", "Bios", EmuFolders::Bios);

	const std::string bios_selection(GetEditingSettingsInterface()->GetStringValue("Filenames", "BIOS", ""));
	if (MenuButtonWithValue(ICON_FA_MICROCHIP "  BIOS Selection", "Changes the BIOS image used to start future sessions.",
			bios_selection.empty() ? "Automatic" : bios_selection.c_str()))
	{
		ImGuiFullscreen::ChoiceDialogOptions choices;
		choices.emplace_back("Automatic", bios_selection.empty());

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

		OpenChoiceDialog("BIOS Selection", false, std::move(choices),
			[game_settings = IsEditingGameSettings(bsi), values = std::move(values)](s32 index, const std::string& title, bool checked) {
				if (index < 0)
					return;

				auto lock = Host::GetSettingsLock();
				SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
				bsi->SetStringValue("Filenames", "BIOS", values[index].c_str());
				SetSettingsChanged(bsi);
				CloseChoiceDialog();
			});
	}

	MenuHeading("Options and Patches");
	DrawToggleSetting(
		bsi, ICON_FA_LIGHTBULB "  Fast Boot", "Skips the intro screen, and bypasses region checks.", "EmuCore", "EnableFastBoot", true);

	EndMenuButtons();
}

void FullscreenUI::DrawEmulationSettingsPage()
{
	static constexpr int DEFAULT_FRAME_LATENCY = 2;

	static constexpr const char* speed_entries[] = {
		"2% [1 FPS (NTSC) / 1 FPS (PAL)]",
		"10% [6 FPS (NTSC) / 5 FPS (PAL)]",
		"25% [15 FPS (NTSC) / 12 FPS (PAL)]",
		"50% [30 FPS (NTSC) / 25 FPS (PAL)]",
		"75% [45 FPS (NTSC) / 37 FPS (PAL)]",
		"90% [54 FPS (NTSC) / 45 FPS (PAL)]",
		"100% [60 FPS (NTSC) / 50 FPS (PAL)]",
		"110% [66 FPS (NTSC) / 55 FPS (PAL)]",
		"120% [72 FPS (NTSC) / 60 FPS (PAL)]",
		"150% [90 FPS (NTSC) / 75 FPS (PAL)]",
		"175% [105 FPS (NTSC) / 87 FPS (PAL)]",
		"200% [120 FPS (NTSC) / 100 FPS (PAL)]",
		"300% [180 FPS (NTSC) / 150 FPS (PAL)]",
		"400% [240 FPS (NTSC) / 200 FPS (PAL)]",
		"500% [300 FPS (NTSC) / 250 FPS (PAL)]",
		"1000% [600 FPS (NTSC) / 500 FPS (PAL)]",
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
	static constexpr const char* queue_entries[] = {"0 Frames (Hard Sync)", "1 Frame", "2 Frames", "3 Frames"};

	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading("Speed Control");

	DrawFloatListSetting(bsi, "Normal Speed", "Sets the speed when running without fast forwarding.", "Framerate", "NominalScalar", 1.00f,
		speed_entries, speed_values, std::size(speed_entries));
	DrawFloatListSetting(bsi, "Fast Forward Speed", "Sets the speed when using the fast forward hotkey.", "Framerate", "TurboScalar", 2.00f,
		speed_entries, speed_values, std::size(speed_entries));
	DrawFloatListSetting(bsi, "Slow Motion Speed", "Sets the speed when using the slow motion hotkey.", "Framerate", "SlomoScalar", 0.50f,
		speed_entries, speed_values, std::size(speed_entries));
	DrawToggleSetting(
		bsi, "Enable Speed Limiter", "When disabled, the game will run as fast as possible.", "EmuCore/GS", "FrameLimitEnable", true);

	MenuHeading("Frame Pacing/Latency Control");

	bool optimal_frame_pacing = (bsi->GetIntValue("EmuCore/GS", "VsyncQueueSize", DEFAULT_FRAME_LATENCY) == 0);

	DrawIntListSetting(bsi, "Maximum Frame Latency", "Sets the number of frames which can be queued.", "EmuCore/GS", "VsyncQueueSize",
		DEFAULT_FRAME_LATENCY, queue_entries, std::size(queue_entries), 0, !optimal_frame_pacing);

	if (ToggleButton("Optimal Frame Pacing",
			"Synchronize EE and GS threads after each frame. Lowest input latency, but increases system requirements.",
			&optimal_frame_pacing))
	{
		bsi->SetIntValue("EmuCore/GS", "VsyncQueueSize", optimal_frame_pacing ? 0 : DEFAULT_FRAME_LATENCY);
		SetSettingsChanged(bsi);
	}

	DrawToggleSetting(bsi, "Adjust To Host Refresh Rate", "Speeds up emulation so that the guest refresh rate matches the host.",
		"EmuCore/GS", "SyncToHostRefreshRate", false);

	MenuHeading("Game Settings");

	DrawToggleSetting(bsi, "Enable Cheats", "Enables loading cheats from pnach files.", "EmuCore", "EnableCheats", false);
	DrawToggleSetting(bsi, "Enable Widescreen Patches", "Enables loading widescreen patches from pnach files.", "EmuCore",
		"EnableWideScreenPatches", false);
	DrawToggleSetting(bsi, "Enable No-Interlacing Patches", "Enables loading no-interlacing patches from pnach files.", "EmuCore",
		"EnableNoInterlacingPatches", false);
	DrawToggleSetting(bsi, "Enable Per-Game Settings", "Enables loading ini overlays from gamesettings, or custom settings per-game.",
		"EmuCore", "EnablePerGameSettings", true);
	DrawToggleSetting(bsi, "Enable Host Filesystem", "Enables access to files from the host: namespace in the virtual machine.", "EmuCore",
		"HostFs", false);
	DrawToggleSetting(bsi, "Warn About Unsafe Settings", "Displays warnings when settings are enabled which may break games.", "EmuCore",
		"WarnAboutUnsafeSettings", true);

	EndMenuButtons();
}

void FullscreenUI::DrawClampingModeSetting(SettingsInterface* bsi, const char* title, const char* summary, bool vu)
{
	// This is so messy... maybe we should just make the mode an int in the settings too...
	const bool base = IsEditingGameSettings(bsi) ? 1 : 0;
	std::optional<bool> default_false = IsEditingGameSettings(bsi) ? std::nullopt : std::optional<bool>(false);
	std::optional<bool> default_true = IsEditingGameSettings(bsi) ? std::nullopt : std::optional<bool>(true);

	std::optional<bool> third = bsi->GetOptionalBoolValue("EmuCore/CPU/Recompiler", vu ? "vuSignOverflow" : "fpuFullMode", default_false);
	std::optional<bool> second =
		bsi->GetOptionalBoolValue("EmuCore/CPU/Recompiler", vu ? "vuExtraOverflow" : "fpuExtraOverflow", default_false);
	std::optional<bool> first = bsi->GetOptionalBoolValue("EmuCore/CPU/Recompiler", vu ? "vuOverflow" : "fpuOverflow", default_true);

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
		"Use Global Setting", "None", "Normal (Default)", "Extra + Preserve Sign", "Full"};
	static constexpr const char* vu_clamping_mode_settings[] = {
		"Use Global Setting", "None", "Normal (Default)", "Extra", "Extra + Preserve Sign"};
	const char* const* options = vu ? vu_clamping_mode_settings : ee_clamping_mode_settings;
	const int setting_offset = IsEditingGameSettings(bsi) ? 0 : 1;

	if (MenuButtonWithValue(title, summary, options[index + setting_offset]))
	{
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(std::size(ee_clamping_mode_settings));
		for (int i = setting_offset; i < static_cast<int>(std::size(ee_clamping_mode_settings)); i++)
			cd_options.emplace_back(options[i], (i == (index + setting_offset)));
		OpenChoiceDialog(title, false, std::move(cd_options),
			[game_settings = IsEditingGameSettings(bsi), vu](s32 index, const std::string& title, bool checked) {
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
					bsi->SetOptionalBoolValue("EmuCore/CPU/Recompiler", vu ? "vuSignOverflow" : "fpuFullMode", third);
					bsi->SetOptionalBoolValue("EmuCore/CPU/Recompiler", vu ? "vuExtraOverflow" : "fpuExtraOverflow", second);
					bsi->SetOptionalBoolValue("EmuCore/CPU/Recompiler", vu ? "vuOverflow" : "fpuOverflow", first);
					SetSettingsChanged(bsi);
				}

				CloseChoiceDialog();
			});
	}
}

void FullscreenUI::DrawSystemSettingsPage()
{
	static constexpr const char* ee_cycle_rate_settings[] = {
		"50% Speed", "60% Speed", "75% Speed", "100% Speed (Default)", "130% Speed", "180% Speed", "300% Speed"};
	static constexpr const char* ee_cycle_skip_settings[] = {
		"Normal (Default)", "Mild Underclock", "Moderate Underclock", "Maximum Underclock"};
	static constexpr const char* ee_rounding_mode_settings[] = {"Nearest", "Negative", "Positive", "Chop/Zero (Default)"};
	static constexpr const char* affinity_control_settings[] = {
		"Disabled", "EE > VU > GS", "EE > GS > VU", "VU > EE > GS", "VU > GS > EE", "GS > EE > VU", "GS > VU > EE"};

	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading("Emotion Engine (MIPS-III/MIPS-IV)");
	DrawIntListSetting(bsi, "Cycle Rate", "Underclocks or overclocks the emulated Emotion Engine CPU.", "EmuCore/Speedhacks", "EECycleRate",
		0, ee_cycle_rate_settings, std::size(ee_cycle_rate_settings), -3);
	DrawIntListSetting(bsi, "Cycle Skip", "Adds a penalty to the Emulated Emotion Engine for executing VU programs.", "EmuCore/Speedhacks",
		"EECycleSkip", 0, ee_cycle_skip_settings, std::size(ee_cycle_skip_settings));
	DrawIntListSetting(bsi, "Rounding Mode##ee_rounding_mode",
		"Determines how the results of floating-point operations are rounded. Some games need specific settings.", "EmuCore/CPU",
		"FPU.Roundmode", 3, ee_rounding_mode_settings, std::size(ee_rounding_mode_settings));
	DrawClampingModeSetting(bsi, "Clamping Mode##ee_clamping_mode",
		"Determines how out-of-range floating point numbers are handled. Some games need specific settings.", false);
	DrawIntListSetting(bsi, "Affinity Control Mode",
		"Pins emulation threads to CPU cores to potentially improve performance/frame time variance.", "EmuCore/CPU", "AffinityControlMode",
		0, affinity_control_settings, std::size(affinity_control_settings), 0);

	MenuHeading("Vector Units");
	DrawIntListSetting(bsi, "Rounding Mode##vu_rounding_mode",
		"Determines how the results of floating-point operations are rounded. Some games need specific settings.", "EmuCore/CPU",
		"VU.Roundmode", 3, ee_rounding_mode_settings, std::size(ee_rounding_mode_settings));
	DrawClampingModeSetting(bsi, "Clamping Mode##vu_clamping_mode",
		"Determines how out-of-range floating point numbers are handled. Some games need specific settings.", true);
	DrawToggleSetting(bsi, "Enable MTVU (Multi-Threaded VU1)", "Uses a second thread for VU1 micro programs. Sizable speed boost.",
		"EmuCore/Speedhacks", "vuThread", false);
	DrawToggleSetting(bsi, "Enable Instant VU1",
		"Reduces timeslicing between VU1 and EE recompilers, effectively running VU1 at an infinite clock speed.", "EmuCore/Speedhacks",
		"vu1Instant", true);

	MenuHeading("I/O Processor (MIPS-I)");
	DrawToggleSetting(
		bsi, "Enable Fast CDVD", "Fast disc access, less loading times. Not recommended.", "EmuCore/Speedhacks", "fastCDVD", false);

	EndMenuButtons();
}

void FullscreenUI::DrawGraphicsSettingsPage()
{
	static constexpr const char* s_renderer_names[] = {"Automatic",
#ifdef _WIN32
		"Direct3D 11", "Direct3D 12",
#endif
#ifdef ENABLE_OPENGL
		"OpenGL",
#endif
#ifdef ENABLE_VULKAN
		"Vulkan",
#endif
#ifdef __APPLE__
		"Metal",
#endif
		"Software", "Null"};
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
	static constexpr const char* s_vsync_values[] = {"Off", "On", "Adaptive"};
	static constexpr const char* s_deinterlacing_options[] = {"None", "Weave (Top Field First, Sawtooth)",
		"Weave (Bottom Field First, Sawtooth)", "Bob (Top Field First)", "Bob (Bottom Field First)", "Blend (Top Field First, Half FPS)",
		"Blend (Bottom Field First, Half FPS)", "Automatic (Default)"};
	static constexpr const char* s_resolution_options[] = {
		"Native (PS2)",
		"2x Native (~720p)",
		"3x Native (~1080p)",
		"4x Native (~1440p/2K)",
		"5x Native (~1620p)",
		"6x Native (~2160p/4K)",
		"7x Native (~2520p)",
		"8x Native (~2880p)",
	};
	static constexpr const char* s_mipmapping_options[] = {"Automatic (Default)", "Off", "Basic (Generated Mipmaps)", "Full (PS2 Mipmaps)"};
	static constexpr const char* s_bilinear_options[] = {
		"Nearest", "Bilinear (Forced)", "Bilinear (PS2)", "Bilinear (Forced excluding sprite)"};
	static constexpr const char* s_trilinear_options[] = {"Automatic (Default)", "Off (None)", "Trilinear (PS2)", "Trilinear (Forced)"};
	static constexpr const char* s_dithering_options[] = {"Off", "Scaled", "Unscaled (Default)"};
	static constexpr const char* s_crc_fix_options[] = {
		"Automatic (Default)", "None (Debug)", "Minimum (Debug)", "Partial (OpenGL)", "Full (Direct3D)", "Aggressive"};
	static constexpr const char* s_blending_options[] = {
		"Minimum", "Basic (Recommended)", "Medium", "High", "Full (Slow)", "Maximum (Very Slow)"};
	static constexpr const char* s_anisotropic_filtering_entries[] = {"Off (Default)", "2x", "4x", "8x", "16x"};
	static constexpr const char* s_anisotropic_filtering_values[] = {"0", "2", "4", "8", "16"};
	static constexpr const char* s_preloading_options[] = {"None", "Partial", "Full (Hash Cache)"};
	static constexpr const char* s_generic_options[] = {"Automatic (Default)", "Force Disabled", "Force Enabled"};

	SettingsInterface* bsi = GetEditingSettingsInterface();

	const GSRendererType renderer =
		static_cast<GSRendererType>(GetEffectiveIntSetting(bsi, "EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::Auto)));
	const bool is_hardware = (renderer == GSRendererType::Auto || renderer == GSRendererType::DX11 || renderer == GSRendererType::DX12 ||
							  renderer == GSRendererType::OGL || renderer == GSRendererType::VK || renderer == GSRendererType::Metal);
	//const bool is_software = (renderer == GSRendererType::SW);

	BeginMenuButtons();

	MenuHeading("Renderer");
	DrawStringListSetting(bsi, "Renderer", "Selects the API used to render the emulated GS.", "EmuCore/GS", "Renderer", "-1",
		s_renderer_names, s_renderer_values, std::size(s_renderer_names));
	DrawIntListSetting(bsi, "Sync To Host Refresh (VSync)", "Synchronizes frame presentation with host refresh.", "EmuCore/GS",
		"VsyncEnable", static_cast<int>(VsyncMode::Off), s_vsync_values, std::size(s_vsync_values));

	MenuHeading("Display");
	DrawStringListSetting(bsi, "Aspect Ratio", "Selects the aspect ratio to display the game content at.", "EmuCore/GS", "AspectRatio",
		"Auto 4:3/3:2", Pcsx2Config::GSOptions::AspectRatioNames, Pcsx2Config::GSOptions::AspectRatioNames, 0);
	DrawStringListSetting(bsi, "FMV Aspect Ratio", "Selects the aspect ratio for display when a FMV is detected as playing.", "EmuCore/GS",
		"FMVAspectRatioSwitch", "Auto 4:3/3:2", Pcsx2Config::GSOptions::FMVAspectRatioSwitchNames,
		Pcsx2Config::GSOptions::FMVAspectRatioSwitchNames, 0);
	DrawIntListSetting(bsi, "Deinterlacing",
		"Selects the algorithm used to convert the PS2's interlaced output to progressive for display.", "EmuCore/GS", "deinterlace",
		static_cast<int>(GSInterlaceMode::Automatic), s_deinterlacing_options, std::size(s_deinterlacing_options));
	DrawIntRangeSetting(bsi, "Zoom", "Increases or decreases the virtual picture size both horizontally and vertically.", "EmuCore/GS",
		"Zoom", 100, 10, 300, "%d%%");
	DrawIntRangeSetting(bsi, "Vertical Stretch", "Increases or decreases the virtual picture size vertically.", "EmuCore/GS", "StretchY",
		100, 10, 300, "%d%%");
	DrawIntRectSetting(bsi, "Crop", "Crops the image, while respecting aspect ratio.", "EmuCore/GS", "CropLeft", 0, "CropTop", 0,
		"CropRight", 0, "CropBottom", 0, 0, 720, "%dpx");
	DrawToggleSetting(
		bsi, "Bilinear Upscaling", "Smooths out the image when upscaling the console to the screen.", "EmuCore/GS", "linear_present", true);
	DrawToggleSetting(bsi, "Integer Upscaling",
		"Adds padding to the display area to ensure that the ratio between pixels on the host to pixels in the console is an integer "
		"number. May result in a sharper image in some 2D games.",
		"EmuCore/GS", "IntegerScaling", false);
	DrawToggleSetting(bsi, "Internal Resolution Screenshots",
		"Save screenshots at the full render resolution, rather than display resolution.", "EmuCore/GS", "InternalResolutionScreenshots",
		false);
	DrawToggleSetting(bsi, "Screen Offsets", "Enables PCRTC Offsets which position the screen as the game requests.", "EmuCore/GS",
		"pcrtc_offsets", false);
	DrawToggleSetting(bsi, "Show Overscan",
		"Enables the option to show the overscan area on games which draw more than the safe area of the screen.", "EmuCore/GS",
		"pcrtc_overscan", false);
	DrawToggleSetting(bsi, "Anti-Blur",
		"Enables internal Anti-Blur hacks. Less accurate to PS2 rendering but will make a lot of games look less blurry.", "EmuCore/GS",
		"pcrtc_antiblur", true);

	MenuHeading("Rendering");
	if (is_hardware)
	{
		DrawIntListSetting(bsi, "Internal Resolution", "Multiplies the render resolution by the specified factor (upscaling).",
			"EmuCore/GS", "upscale_multiplier", 1, s_resolution_options, std::size(s_resolution_options), 1);
		DrawIntListSetting(bsi, "Mipmapping", "Determines how mipmaps are used when rendering textures.", "EmuCore/GS", "mipmap_hw",
			static_cast<int>(HWMipmapLevel::Automatic), s_mipmapping_options, std::size(s_mipmapping_options), -1);
		DrawIntListSetting(bsi, "Bilinear Filtering", "Selects where bilinear filtering is utilized when rendering textures.", "EmuCore/GS",
			"filter", static_cast<int>(BiFiltering::PS2), s_bilinear_options, std::size(s_bilinear_options));
		DrawIntListSetting(bsi, "Trilinear Filtering", "Selects where trilinear filtering is utilized when rendering textures.",
			"EmuCore/GS", "UserHacks_TriFilter", static_cast<int>(TriFiltering::Automatic), s_trilinear_options,
			std::size(s_trilinear_options), -1);
		DrawStringListSetting(bsi, "Anisotropic Filtering", "Selects where anistropic filtering is utilized when rendering textures.",
			"EmuCore/GS", "MaxAnisotropy", "0", s_anisotropic_filtering_entries, s_anisotropic_filtering_values,
			std::size(s_anisotropic_filtering_entries));
		DrawIntListSetting(bsi, "Dithering", "Selects the type of dithering applies when the game requests it.", "EmuCore/GS",
			"dithering_ps2", 2, s_dithering_options, std::size(s_dithering_options));
		DrawIntListSetting(bsi, "CRC Fix Level", "Applies manual fixes to difficult-to-emulate effects in the hardware renderers.",
			"EmuCore/GS", "crc_hack_level", static_cast<int>(CRCHackLevel::Automatic), s_crc_fix_options, std::size(s_crc_fix_options), -1);
		DrawIntListSetting(bsi, "Blending Accuracy",
			"Determines the level of accuracy when emulating blend modes not supported by the host graphics API.", "EmuCore/GS",
			"accurate_blending_unit", static_cast<int>(AccBlendLevel::Basic), s_blending_options, std::size(s_blending_options));
		DrawIntListSetting(bsi, "Texture Preloading",
			"Uploads full textures to the GPU on use, rather than only the utilized regions. Can improve performance in some games.",
			"EmuCore/GS", "texture_preloading", static_cast<int>(TexturePreloadingLevel::Off), s_preloading_options,
			std::size(s_preloading_options));
		DrawToggleSetting(bsi, "GPU Palette Conversion",
			"Applies palettes to textures on the GPU instead of the CPU. Can result in speed improvements in some games.", "EmuCore/GS",
			"paltex", false);
	}
	else
	{
	}

	if (is_hardware)
	{
		MenuHeading("Hardware Fixes");
		DrawToggleSetting(bsi, "Manual Hardware Fixes", "Disables automatic hardware fixes, allowing you to set fixes manually.",
			"EmuCore/GS", "UserHacks", false);

		const bool manual_hw_fixes = GetEffectiveBoolSetting(bsi, "EmuCore/GS", "UserHacks", false);
		if (manual_hw_fixes)
		{
			static constexpr const char* s_cpu_sprite_render_bw_options[] = {"0 (Disabled)", "1 (64 Max Width)", "2 (128 Max Width)",
				"3 (192 Max Width)", "4 (256 Max Width)", "5 (320 Max Width)", "6 (384 Max Width)", "7 (448 Max Width)",
				"8 (512 Max Width)", "9 (576 Max Width)", "10 (640 Max Width)"};
			static constexpr const char* s_half_pixel_offset_options[] = {
				"Off (Default)", "Normal (Vertex)", "Special (Texture)", "Special (Texture - Aggressive)"};
			static constexpr const char* s_round_sprite_options[] = {"Off (Default)", "Half", "Full"};

			DrawIntListSetting(bsi, "Half-Bottom Override", "Control the half-screen fix detection on texture shuffling.", "EmuCore/GS",
				"UserHacks_Half_Bottom_Override", -1, s_generic_options, std::size(s_generic_options), -1);
			DrawIntListSetting(bsi, "CPU Sprite Render Size", "Uses sofware renderer to draw texture decompression-like sprites.",
				"EmuCore/GS", "UserHacks_CPUSpriteRenderBW", 0, s_cpu_sprite_render_bw_options, std::size(s_cpu_sprite_render_bw_options));
			DrawIntRangeSetting(
				bsi, "Skip Draw Start", "Object range to skip drawing.", "EmuCore/GS", "UserHacks_SkipDraw_Start", 0, 0, 5000);
			DrawIntRangeSetting(bsi, "Skip Draw End", "Object range to skip drawing.", "EmuCore/GS", "UserHacks_SkipDraw_End", 0, 0, 5000);
			DrawToggleSetting(bsi, "Auto Flush (Hardware)", "Force a primitive flush when a framebuffer is also an input texture.",
				"EmuCore/GS", "UserHacks_AutoFlush", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "CPU Framebuffer Conversion", "Convert 4-bit and 8-bit frame buffer on the CPU instead of the GPU.",
				"EmuCore/GS", "UserHacks_CPU_FB_Conversion", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Disable Depth Support", "Disable the support of depth buffer in the texture cache.", "EmuCore/GS",
				"UserHacks_DisableDepthSupport", false, manual_hw_fixes);
			DrawToggleSetting(
				bsi, "Wrap GS Memory", "Emulates GS memory wrapping accurately.", "EmuCore/GS", "wrap_gs_mem", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Disable Safe Features", "This option disables multiple safe features.", "EmuCore/GS",
				"UserHacks_Disable_Safe_Features", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Preload Frame", "Uploads GS data when rendering a new frame to reproduce some effects accurately.",
				"EmuCore/GS", "preload_frame_with_gs_data", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Disable Partial Invalidation",
				"Removes texture cache entries when there is any intersection, rather than only the intersected areas.", "EmuCore/GS",
				"UserHacks_DisablePartialInvalidation", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Texture Inside Render Target",
				"Allows the texture cache to reuse as an input texture the inner portion of a previous framebuffer.", "EmuCore/GS",
				"UserHacks_TextureInsideRt", false, manual_hw_fixes);

			MenuHeading("Upscaling Fixes");
			DrawIntListSetting(bsi, "Half-Pixel Offset", "Adjusts vertices relative to upscaling.", "EmuCore/GS",
				"UserHacks_HalfPixelOffset", 0, s_half_pixel_offset_options, std::size(s_half_pixel_offset_options));
			DrawIntListSetting(bsi, "Round Sprite", "Adjusts sprite coordinates.", "EmuCore/GS", "UserHacks_round_sprite_offset", 0,
				s_round_sprite_options, std::size(s_round_sprite_options));
			DrawIntRangeSetting(bsi, "TC Offset X", "Adjusts target texture offsets.", "EmuCore/GS", "UserHacks_TCOffsetX", 0, -4096, 4096);
			DrawIntRangeSetting(bsi, "TC Offset Y", "Adjusts target texture offsets.", "EmuCore/GS", "UserHacks_TCOffsetY", 0, -4096, 4096);
			DrawToggleSetting(bsi, "Align Sprite", "Fixes issues with upscaling (vertical lines) in some games.", "EmuCore/GS",
				"UserHacks_align_sprite_X", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Merge Sprite", "Replaces multiple post-processing sprites with a larger single sprite.", "EmuCore/GS",
				"UserHacks_merge_pp_sprite", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Wild Arms Hack",
				"Lowers the GS precision to avoid gaps between pixels when upscaling. Fixes the text on Wild Arms games.", "EmuCore/GS",
				"UserHacks_WildHack", false, manual_hw_fixes);
		}
	}
	else
	{
		// extrathreads
		DrawIntRangeSetting(bsi, "Software Rendering Threads",
			"Number of threads to use in addition to the main GS thread for rasterization.", "EmuCore/GS", "extrathreads", 2, 0, 10);
		DrawToggleSetting(bsi, "Auto Flush (Software)", "Force a primitive flush when a framebuffer is also an input texture.",
			"EmuCore/GS", "autoflush_sw", true);
		DrawToggleSetting(bsi, "Edge AA (AA1)", "Enables emulation of the GS's edge anti-aliasing (AA1).", "EmuCore/GS", "aa1", true);
		DrawToggleSetting(bsi, "Mipmapping", "Enables emulation of the GS's texture mipmapping.", "EmuCore/GS", "mipmap", true);
	}

	if (is_hardware)
	{
		const bool dumping_active = GetEffectiveBoolSetting(bsi, "EmuCore/GS", "DumpReplaceableTextures", false);
		const bool replacement_active = GetEffectiveBoolSetting(bsi, "EmuCore/GS", "LoadTextureReplacements", false);

		MenuHeading("Texture Replacement");
		DrawToggleSetting(bsi, "Load Textures", "Loads replacement textures where available and user-provided.", "EmuCore/GS",
			"LoadTextureReplacements", false);
		DrawToggleSetting(bsi, "Asynchronous Texture Loading",
			"Loads replacement textures on a worker thread, reducing microstutter when replacements are enabled.", "EmuCore/GS",
			"LoadTextureReplacementsAsync", true, replacement_active);
		DrawToggleSetting(bsi, "Precache Replacements",
			"Preloads all replacement textures to memory. Not necessary with asynchronous loading.", "EmuCore/GS",
			"PrecacheTextureReplacements", false, replacement_active);

		MenuHeading("Texture Dumping");
		DrawToggleSetting(bsi, "Dump Textures", "Dumps replacable textures to disk. Will reduce performance.", "EmuCore/GS",
			"DumpReplaceableTextures", false);
		DrawToggleSetting(
			bsi, "Dump Mipmaps", "Includes mipmaps when dumping textures.", "EmuCore/GS", "DumpReplaceableMipmaps", false, dumping_active);
		DrawToggleSetting(bsi, "Dump FMV Textures", "Allows texture dumping when FMVs are active. You should not enable this.",
			"EmuCore/GS", "DumpTexturesWithFMVActive", false, dumping_active);
	}

	MenuHeading("Post-Processing");
	{
		const bool shadeboost_active = GetEffectiveBoolSetting(bsi, "EmuCore/GS", "ShadeBoost", false);
		DrawToggleSetting(bsi, "FXAA", "Enables FXAA post-processing shader.", "EmuCore/GS", "fxaa", false);
		DrawToggleSetting(bsi, "Shade Boost", "Enables brightness/contrast/saturation adjustment.", "EmuCore/GS", "ShadeBoost", false);
		DrawIntRangeSetting(bsi, "Shade Boost Brightness", "Adjusts brightness. 50 is normal.", "EmuCore/GS", "ShadeBoost_Brightness", 50,
			1, 100, "%d", shadeboost_active);
		DrawIntRangeSetting(bsi, "Shade Boost Contrast", "Adjusts contrast. 50 is normal.", "EmuCore/GS", "ShadeBoost_Contrast", 50, 1, 100,
			"%d", shadeboost_active);
		DrawIntRangeSetting(bsi, "Shade Boost Saturation", "Adjusts saturation. 50 is normal.", "EmuCore/GS", "ShadeBoost_Saturation", 50,
			1, 100, "%d", shadeboost_active);

		static constexpr const char* s_tv_shaders[] = {
			"None", "Scanline Filter", "Diagonal Filter", "Triangular Filter", "Wave Filter", "Lottes CRT"};
		DrawIntListSetting(
			bsi, "TV Shaders", "Selects post-processing TV shader.", "EmuCore/GS", "TVShader", 0, s_tv_shaders, std::size(s_tv_shaders));
	}

	static constexpr const char* s_gsdump_compression[] = {"Uncompressed", "LZMA (xz)", "Zstandard (zst)"};

	MenuHeading("Advanced");
	DrawToggleSetting(bsi, "Skip Presenting Duplicate Frames",
		"Skips displaying frames that don't change in 25/30fps games. Can improve speed but increase input lag/make frame pacing worse.",
		"EmuCore/GS", "SkipDuplicateFrames", false);
	DrawToggleSetting(bsi, "Disable Hardware Readbacks",
		"Skips thread synchronization for GS downloads. Can improve speed, but break graphical effects.", "EmuCore/GS",
		"HWDisableReadbacks", false);
	DrawIntListSetting(bsi, "Override Texture Barriers", "Forces texture barrier functionality to the specified value.", "EmuCore/GS",
		"OverrideTextureBarriers", -1, s_generic_options, std::size(s_generic_options), -1);
	DrawIntListSetting(bsi, "Override Geometry Shaders", "Forces geometry shader functionality to the specified value.", "EmuCore/GS",
		"OverrideGeometryShaders", -1, s_generic_options, std::size(s_generic_options), -1);
	DrawIntListSetting(bsi, "GS Dump Compression", "Sets the compression algorithm for GS dumps.", "EmuCore/GS", "GSDumpCompression",
		static_cast<int>(GSDumpCompressionMethod::LZMA), s_gsdump_compression, std::size(s_gsdump_compression));
	DrawToggleSetting(bsi, "Disable Framebuffer Fetch", "Prevents the usage of framebuffer fetch when supported by host GPU.", "EmuCore/GS",
		"DisableFramebufferFetch", false);
	DrawToggleSetting(bsi, "Disable Dual-Source Blending", "Prevents the usage of dual-source blending when supported by host GPU.",
		"EmuCore/GS", "DisableDualSourceBlend", false);

	EndMenuButtons();
}

void FullscreenUI::DrawAudioSettingsPage()
{
	static constexpr const char* interpolation_modes[] = {
		"Nearest (Fastest / worst quality)",
		"Linear (Simple / okay sound)",
		"Cubic (Fake highs / okay sound)",
		"Hermite (Better highs / okay sound)",
		"Catmull-Rom (PS2-like / good sound)",
		"Gaussian (PS2-like / great sound)",
	};
	static constexpr const char* synchronization_modes[] = {
		"TimeStretch (Recommended)",
		"Async Mix (Breaks some games!)",
		"None (Audio can skip.)",
	};
	static constexpr const char* expansion_modes[] = {
		"Stereo (None, Default)",
		"Quadrafonic",
		"Surround 5.1",
		"Surround 7.1",
	};
	static constexpr const char* output_entries[] = {
		"No Sound (Emulate SPU2 only)",
#ifdef SPU2X_CUBEB
		"Cubeb (Cross-platform)",
#endif
#ifdef _WIN32
		"XAudio2",
#endif
	};
	static constexpr const char* output_values[] = {
		"nullout",
#ifdef SPU2X_CUBEB
		"cubeb",
#endif
#ifdef _WIN32
		"xaudio2",
#endif
	};
#if defined(SPU2X_CUBEB)
	static constexpr const char* default_output_module = "cubeb";
#elif defined(_WIN32)
	static constexpr const char* default_output_module = "xaudio2";
#else
	static constexpr const char* default_output_module = "nullout";
#endif

	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading("Runtime Settings");
	DrawIntRangeSetting(bsi, ICON_FA_VOLUME_UP "  Output Volume", "Applies a global volume modifier to all sound produced by the game.",
		"SPU2/Mixing", "FinalVolume", 100, 0, 100, "%d%%");

	MenuHeading("Mixing Settings");
	DrawIntListSetting(bsi, ICON_FA_MUSIC "  Interpolation Mode", "Determines how ADPCM samples are interpolated to the target pitch.",
		"SPU2/Mixing", "Interpolation", static_cast<int>(Pcsx2Config::SPU2Options::InterpolationMode::Gaussian), interpolation_modes,
		std::size(interpolation_modes));
	DrawIntListSetting(bsi, ICON_FA_RULER "  Synchronization Mode", "Changes when SPU samples are generated relative to system emulation.",
		"SPU2/Output", "SynchMode", static_cast<int>(Pcsx2Config::SPU2Options::SynchronizationMode::TimeStretch), synchronization_modes,
		std::size(synchronization_modes));
	DrawIntListSetting(bsi, ICON_FA_PLUS "  Expansion Mode", "Determines how the stereo output is transformed to greater speaker counts.",
		"SPU2/Output", "SpeakerConfiguration", 0, expansion_modes, std::size(expansion_modes));

	MenuHeading("Output Settings");
	DrawStringListSetting(bsi, ICON_FA_PLAY_CIRCLE "  Output Module",
		"Determines which API is used to play back audio samples on the host.", "SPU2/Output", "OutputModule", default_output_module,
		output_entries, output_values, std::size(output_entries));
	DrawIntRangeSetting(bsi, ICON_FA_CLOCK "  Latency", "Sets the average output latency when using the cubeb backend.", "SPU2/Output",
		"Latency", 100, 15, 200, "%d ms (avg)");

	MenuHeading("Timestretch Settings");
	DrawIntRangeSetting(bsi, ICON_FA_RULER_HORIZONTAL "  Sequence Length",
		"Affects how the timestretcher operates when not running at 100% speed.", "Soundtouch", "SequenceLengthMS", 30, 20, 100, "%d ms");
	DrawIntRangeSetting(bsi, ICON_FA_WINDOW_MAXIMIZE "  Seekwindow Size",
		"Affects how the timestretcher operates when not running at 100% speed.", "Soundtouch", "SeekWindowMS", 20, 10, 30, "%d ms");
	DrawIntRangeSetting(bsi, ICON_FA_RECEIPT "  Overlap", "Affects how the timestretcher operates when not running at 100% speed.",
		"Soundtouch", "OverlapMS", 20, 5, 15, "%d ms");

	EndMenuButtons();
}

void FullscreenUI::DrawMemoryCardSettingsPage()
{
	BeginMenuButtons();

	SettingsInterface* bsi = GetEditingSettingsInterface();

	MenuHeading("Settings and Operations");
	if (MenuButton(ICON_FA_PLUS "  Create Memory Card", "Creates a new memory card file or folder."))
		ImGui::OpenPopup("Create Memory Card");
	DrawCreateMemoryCardWindow();

	DrawFolderSetting(bsi, ICON_FA_FOLDER_OPEN "  Memory Card Directory", "Folders", "MemoryCards", EmuFolders::MemoryCards);
	DrawToggleSetting(bsi, ICON_FA_SEARCH "  Folder Memory Card Filter",
		"Simulates a larger memory card by filtering saves only to the current game.", "EmuCore", "McdFolderAutoManage", true);
	DrawToggleSetting(bsi, ICON_FA_MAGIC "  Auto Eject When Loading",
		"Automatically ejects memory cards when they differ after loading a state.", "EmuCore", "McdEnableEjection", true);

	for (u32 port = 0; port < NUM_MEMORY_CARD_PORTS; port++)
	{
		const std::string title(fmt::format("Console Port {}", port + 1));
		MenuHeading(title.c_str());

		std::string enable_key(fmt::format("Slot{}_Enable", port + 1));
		std::string file_key(fmt::format("Slot{}_Filename", port + 1));

		DrawToggleSetting(bsi, fmt::format(ICON_FA_SD_CARD "  Card Enabled##card_enabled_{}", port).c_str(),
			"If not set, this card will be considered unplugged.", "MemoryCards", enable_key.c_str(), true);

		const bool enabled = GetEffectiveBoolSetting(bsi, "MemoryCards", enable_key.c_str(), true);

		std::optional<std::string> value(bsi->GetOptionalStringValue("MemoryCards", file_key.c_str(),
			IsEditingGameSettings(bsi) ? std::nullopt : std::optional<const char*>(FileMcd_GetDefaultName(port).c_str())));

		if (MenuButtonWithValue(fmt::format(ICON_FA_FILE "  Card Name##card_name_{}", port).c_str(),
				"The selected memory card image will be used for this slot.", value.has_value() ? value->c_str() : "Use Global Setting",
				enabled))
		{
			ImGuiFullscreen::ChoiceDialogOptions options;
			std::vector<std::string> names;
			if (IsEditingGameSettings(bsi))
				options.emplace_back("Use Global Setting", !value.has_value());
			if (value.has_value() && !value->empty())
			{
				options.emplace_back(fmt::format("{} (Current)", value.value()), true);
				names.push_back(std::move(value.value()));
			}
			for (AvailableMcdInfo& mci : FileMcd_GetAvailableCards(IsEditingGameSettings(bsi)))
			{
				if (mci.type == MemoryCardType::Folder)
				{
					options.emplace_back(fmt::format("{} (Folder)", mci.name), false);
				}
				else
				{
					static constexpr const char* file_type_names[] = {
						"Unknown", "PS2 (8MB)", "PS2 (16MB)", "PS2 (32MB)", "PS2 (64MB)", "PS1"};
					options.emplace_back(fmt::format("{} ({})", mci.name, file_type_names[static_cast<u32>(mci.file_type)]), false);
				}
				names.push_back(std::move(mci.name));
			}
			OpenChoiceDialog(title.c_str(), false, std::move(options),
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

		if (MenuButton(
				fmt::format(ICON_FA_EJECT "  Eject Card##eject_card_{}", port).c_str(), "Resets the card name for this slot.", enabled))
		{
			bsi->SetStringValue("MemoryCards", file_key.c_str(), "");
			SetSettingsChanged(bsi);
		}
	}


	EndMenuButtons();
}

void FullscreenUI::DrawCreateMemoryCardWindow()
{
	ImGui::SetNextWindowSize(LayoutScale(700.0f, 0.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushFont(g_large_font);

	bool is_open = true;
	if (ImGui::BeginPopupModal("Create Memory Card", &is_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
	{
		ImGui::TextWrapped("Enter the name of the memory card you wish to create, and choose a size. We recommend either using 8MB memory "
						   "cards, or folder memory cards for best compatibility.");
		ImGui::NewLine();

		static char memcard_name[256] = {};
		ImGui::Text("Card Name: ");
		ImGui::InputText("##name", memcard_name, sizeof(memcard_name));

		ImGui::NewLine();

		static constexpr std::tuple<const char*, MemoryCardType, MemoryCardFileType> memcard_types[] = {
			{"8 MB [Most Compatible]", MemoryCardType::File, MemoryCardFileType::PS2_8MB},
			{"16 MB", MemoryCardType::File, MemoryCardFileType::PS2_8MB},
			{"32 MB", MemoryCardType::File, MemoryCardFileType::PS2_8MB},
			{"64 MB", MemoryCardType::File, MemoryCardFileType::PS2_8MB},
			{"Folder [Recommended]", MemoryCardType::Folder, MemoryCardFileType::PS2_8MB},
			{"128 KB [PS1]", MemoryCardType::File, MemoryCardFileType::PS1},
		};

		static int memcard_type = 0;
		for (int i = 0; i < static_cast<int>(std::size(memcard_types)); i++)
			ImGui::RadioButton(std::get<0>(memcard_types[i]), &memcard_type, i);

		ImGui::NewLine();

		BeginMenuButtons();

		const bool create_enabled = (std::strlen(memcard_name) > 0);

		if (ActiveButton(ICON_FA_FOLDER_OPEN "  Create", false, create_enabled) && std::strlen(memcard_name) > 0)
		{
			const std::string real_card_name(fmt::format("{}.ps2", memcard_name));
			if (!FileMcd_GetCardInfo(real_card_name).has_value())
			{
				const auto& [type_title, type, file_type] = memcard_types[memcard_type];
				if (FileMcd_CreateNewCard(real_card_name, type, file_type))
				{
					ShowToast(std::string(), fmt::format("Memory card '{}' created.", real_card_name));

					std::memset(memcard_name, 0, sizeof(memcard_name));
					memcard_type = 0;
					ImGui::CloseCurrentPopup();
				}
				else
				{
					ShowToast(std::string(), fmt::format("Failed to create memory card '{}'.", real_card_name));
				}
			}
			else
			{
				ShowToast(std::string(), fmt::format("A memory card with the name '{}' already exists.", real_card_name));
			}
		}

		if (ActiveButton(ICON_FA_TIMES "  Cancel", false))
		{
			std::memset(memcard_name, 0, sizeof(memcard_name));
			memcard_type = 0;

			ImGui::CloseCurrentPopup();
		}

		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopFont();
	ImGui::PopStyleVar(2);
}

void FullscreenUI::CopyGlobalControllerSettingsToGame()
{
	SettingsInterface* dsi = GetEditingSettingsInterface(true);
	SettingsInterface* ssi = GetEditingSettingsInterface(false);

	PAD::CopyConfiguration(dsi, *ssi, true, true, false);
	SetSettingsChanged(dsi);

	ShowToast(std::string(), "Per-game controller configuration initialized with global settings.");
}

void FullscreenUI::ResetControllerSettings()
{
	SettingsInterface* dsi = GetEditingSettingsInterface();

	PAD::SetDefaultControllerConfig(*dsi);
	PAD::SetDefaultHotkeyConfig(*dsi);
	ShowToast(std::string(), "Controller settings reset to default.");
}

void FullscreenUI::DoLoadInputProfile()
{
	std::vector<std::string> profiles(PAD::GetInputProfileNames());
	if (profiles.empty())
	{
		ShowToast(std::string(), "No input profiles available.");
		return;
	}

	ImGuiFullscreen::ChoiceDialogOptions coptions;
	coptions.reserve(profiles.size());
	for (std::string& name : profiles)
		coptions.emplace_back(std::move(name), false);
	OpenChoiceDialog(
		ICON_FA_FOLDER_OPEN "  Load Profile", false, std::move(coptions), [](s32 index, const std::string& title, bool checked) {
			if (index < 0)
				return;

			INISettingsInterface ssi(VMManager::GetInputProfilePath(title));
			if (!ssi.Load())
			{
				ShowToast(std::string(), fmt::format("Failed to load '{}'.", title));
				CloseChoiceDialog();
				return;
			}

			auto lock = Host::GetSettingsLock();
			SettingsInterface* dsi = GetEditingSettingsInterface();
			PAD::CopyConfiguration(dsi, ssi, true, true, IsEditingGameSettings(dsi));
			SetSettingsChanged(dsi);
			ShowToast(std::string(), fmt::format("Input profile '{}' loaded.", title));
			CloseChoiceDialog();
		});
}

void FullscreenUI::DoSaveInputProfile(const std::string& name)
{
	INISettingsInterface dsi(VMManager::GetInputProfilePath(name));

	auto lock = Host::GetSettingsLock();
	SettingsInterface* ssi = GetEditingSettingsInterface();
	PAD::CopyConfiguration(&dsi, *ssi, true, true, IsEditingGameSettings(ssi));
	if (dsi.Save())
		ShowToast(std::string(), fmt::format("Input profile '{}' saved.", name));
	else
		ShowToast(std::string(), fmt::format("Failed to save input profile '{}'.", name));
}

void FullscreenUI::DoSaveInputProfile()
{
	std::vector<std::string> profiles(PAD::GetInputProfileNames());
	if (profiles.empty())
	{
		ShowToast(std::string(), "No input profiles available.");
		return;
	}

	ImGuiFullscreen::ChoiceDialogOptions coptions;
	coptions.reserve(profiles.size() + 1);
	coptions.emplace_back("Create New...", false);
	for (std::string& name : profiles)
		coptions.emplace_back(std::move(name), false);
	OpenChoiceDialog(ICON_FA_SAVE "  Save Profile", false, std::move(coptions), [](s32 index, const std::string& title, bool checked) {
		if (index < 0)
			return;

		if (index > 0)
		{
			DoSaveInputProfile(title);
			CloseChoiceDialog();
			return;
		}

		CloseChoiceDialog();

		OpenInputStringDialog(ICON_FA_SAVE "  Save Profile", "Enter the name of the input profile you wish to create.", std::string(),
			ICON_FA_FOLDER_PLUS "  Create", [](std::string title) {
				if (!title.empty())
					DoSaveInputProfile(title);
			});
	});
}

void FullscreenUI::DoResetSettings()
{
	OpenConfirmMessageDialog(ICON_FA_FOLDER_MINUS "  Reset Settings",
		"Are you sure you want to restore the default settings? Any preferences will be lost.", [](bool result) {
			if (result)
			{
				Host::RunOnCPUThread([]() { Host::RequestResetSettings(false, true, false, false, false); });
				ShowToast(std::string(), "Settings reset to defaults.");
			}
		});
}

void FullscreenUI::DrawControllerSettingsPage()
{
	BeginMenuButtons();

	SettingsInterface* bsi = GetEditingSettingsInterface();

	MenuHeading("Configuration");

	if (IsEditingGameSettings(bsi))
	{
		if (DrawToggleSetting(bsi, ICON_FA_COG "  Per-Game Configuration", "Uses game-specific settings for controllers for this game.",
				"Pad", "UseGameSettingsForController", false, IsEditingGameSettings(bsi), false))
		{
			// did we just enable per-game for the first time?
			if (bsi->GetBoolValue("Pad", "UseGameSettingsForController", false) &&
				!bsi->GetBoolValue("Pad", "GameSettingsInitialized", false))
			{
				bsi->SetBoolValue("Pad", "GameSettingsInitialized", true);
				CopyGlobalControllerSettingsToGame();
			}
		}
	}

	if (IsEditingGameSettings(bsi) && !bsi->GetBoolValue("Pad", "UseGameSettingsForController", false))
	{
		// nothing to edit..
		EndMenuButtons();
		return;
	}

	if (IsEditingGameSettings(bsi))
	{
		if (MenuButton(ICON_FA_COPY "  Copy Global Settings", "Copies the global controller configuration to this game."))
			CopyGlobalControllerSettingsToGame();
	}
	else
	{
		if (MenuButton(ICON_FA_FOLDER_MINUS "  Reset Settings", "Resets all configuration to defaults (including bindings)."))
			ResetControllerSettings();
	}

	if (MenuButton(ICON_FA_FOLDER_OPEN "  Load Profile", "Replaces these settings with a previously saved input profile."))
		DoLoadInputProfile();
	if (MenuButton(ICON_FA_SAVE "  Save Profile", "Stores the current settings to an input profile."))
		DoSaveInputProfile();

	MenuHeading("Input Sources");

#ifdef SDL_BUILD
	DrawToggleSetting(bsi, ICON_FA_COG "  Enable SDL Input Source", "The SDL input source supports most controllers.", "InputSources",
		"SDL", true, true, false);
	DrawToggleSetting(bsi, ICON_FA_WIFI "  SDL DualShock 4 / DualSense Enhanced Mode",
		"Provides vibration and LED control support over Bluetooth.", "InputSources", "SDLControllerEnhancedMode", false,
		bsi->GetBoolValue("InputSources", "SDL", true), false);
#endif
#ifdef _WIN32
	DrawToggleSetting(bsi, ICON_FA_COG "  Enable XInput Input Source",
		"The XInput source provides support for XBox 360/XBox One/XBox Series controllers.", "InputSources", "XInput", false, true, false);
#endif

	MenuHeading("Multitap");
	DrawToggleSetting(bsi, ICON_FA_PLUS_SQUARE "  Enable Console Port 1 Multitap",
		"Enables an additional three controller slots. Not supported in all games.", "Pad", "MultitapPort1", false, true, false);
	DrawToggleSetting(bsi, ICON_FA_PLUS_SQUARE "  Enable Console Port 2 Multitap",
		"Enables an additional three controller slots. Not supported in all games.", "Pad", "MultitapPort2", false, true, false);

	const std::array<bool, 2> mtap_enabled = {
		{bsi->GetBoolValue("Pad", "MultitapPort1", false), bsi->GetBoolValue("Pad", "MultitapPort2", false)}};

	// we reorder things a little to make it look less silly for mtap
	static constexpr const std::array<char, 4> mtap_slot_names = {{'A', 'B', 'C', 'D'}};
	static constexpr const std::array<u32, PAD::NUM_CONTROLLER_PORTS> mtap_port_order = {{0, 2, 3, 4, 1, 5, 6, 7}};

	// create the ports
	for (u32 global_slot : mtap_port_order)
	{
		const bool is_mtap_port = sioPadIsMultitapSlot(global_slot);
		const auto [mtap_port, mtap_slot] = sioConvertPadToPortAndSlot(global_slot);
		if (is_mtap_port && !mtap_enabled[mtap_port])
			continue;

		MenuHeading(
			(mtap_enabled[mtap_port] ? fmt::format(ICON_FA_PLUG "  Controller Port {}{}", mtap_port + 1, mtap_slot_names[mtap_slot]) :
                                       fmt::format(ICON_FA_PLUG "  Controller Port {}", mtap_port + 1))
				.c_str());

		const std::string section(fmt::format("Pad{}", global_slot + 1));
		const std::string type(bsi->GetStringValue(section.c_str(), "Type", PAD::GetDefaultPadType(global_slot)));
		const PAD::ControllerInfo* ci = PAD::GetControllerInfo(type);
		if (MenuButton(fmt::format(ICON_FA_GAMEPAD "  Controller Type##type{}", global_slot).c_str(), ci ? ci->display_name : "Unknown"))
		{
			std::vector<std::pair<std::string, std::string>> raw_options(PAD::GetControllerTypeNames());
			ImGuiFullscreen::ChoiceDialogOptions options;
			options.reserve(raw_options.size());
			for (auto& it : raw_options)
			{
				options.emplace_back(std::move(it.second), type == it.first);
			}
			OpenChoiceDialog(fmt::format("Port {} Controller Type", global_slot + 1).c_str(), false, std::move(options),
				[game_settings = IsEditingGameSettings(bsi), section, raw_options = std::move(raw_options)](
					s32 index, const std::string& title, bool checked) {
					if (index < 0)
						return;

					auto lock = Host::GetSettingsLock();
					SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
					bsi->SetStringValue(section.c_str(), "Type", raw_options[index].first.c_str());
					SetSettingsChanged(bsi);
					CloseChoiceDialog();
				});
		}

		if (!ci || ci->num_bindings == 0)
			continue;

		if (MenuButton(ICON_FA_MAGIC "  Automatic Mapping", "Attempts to map the selected port to a chosen controller."))
			StartAutomaticBinding(global_slot);

		for (u32 i = 0; i < ci->num_bindings; i++)
		{
			const PAD::ControllerBindingInfo& bi = ci->bindings[i];
			DrawInputBindingButton(bsi, bi.type, section.c_str(), bi.name, bi.display_name, true);
		}

		MenuHeading((mtap_enabled[mtap_port] ?
						 fmt::format(ICON_FA_MICROCHIP "  Controller Port {}{} Macros", mtap_port + 1, mtap_slot_names[mtap_slot]) :
                         fmt::format(ICON_FA_MICROCHIP "  Controller Port {} Macros", mtap_port + 1))
						.c_str());

		for (u32 macro_index = 0; macro_index < PAD::NUM_MACRO_BUTTONS_PER_CONTROLLER; macro_index++)
		{
			DrawInputBindingButton(bsi, PAD::ControllerBindingType::Macro, section.c_str(), fmt::format("Macro{}", macro_index + 1).c_str(),
				fmt::format("Macro {} Trigger", macro_index + 1).c_str());

			std::string binds_string(bsi->GetStringValue(section.c_str(), fmt::format("Macro{}Binds", macro_index + 1).c_str()));
			if (MenuButton(fmt::format(ICON_FA_KEYBOARD "  Macro {} Buttons", macro_index + 1).c_str(),
					binds_string.empty() ? "No Buttons Selected" : binds_string.c_str()))
			{
				std::vector<std::string_view> buttons_split(StringUtil::SplitString(binds_string, '&', true));
				ImGuiFullscreen::ChoiceDialogOptions options;
				for (u32 i = 0; i < ci->num_bindings; i++)
				{
					const PAD::ControllerBindingInfo& bi = ci->bindings[i];
					if (bi.type != PAD::ControllerBindingType::Button && bi.type != PAD::ControllerBindingType::Axis &&
						bi.type != PAD::ControllerBindingType::HalfAxis)
					{
						continue;
					}
					options.emplace_back(bi.display_name, std::any_of(buttons_split.begin(), buttons_split.end(),
															  [bi](const std::string_view& it) { return (it == bi.name); }));
				}

				OpenChoiceDialog(fmt::format("Select Macro {} Binds", macro_index + 1).c_str(), true, std::move(options),
					[section, macro_index, ci](s32 index, const std::string& title, bool checked) {
						// convert display name back to bind name
						std::string_view to_modify;
						for (u32 j = 0; j < ci->num_bindings; j++)
						{
							const PAD::ControllerBindingInfo& bi = ci->bindings[j];
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

						std::string binds_string(bsi->GetStringValue(section.c_str(), key.c_str()));
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
							bsi->DeleteValue(section.c_str(), key.c_str());
						else
							bsi->SetStringValue(section.c_str(), key.c_str(), binds_string.c_str());
					});
			}

			const std::string freq_key(fmt::format("Macro{}Frequency", macro_index + 1));
			const std::string freq_title(fmt::format(ICON_FA_LIGHTBULB "  Macro {} Frequency", macro_index + 1));
			s32 frequency = bsi->GetIntValue(section.c_str(), freq_key.c_str(), 0);
			const std::string freq_summary((frequency == 0) ? std::string("Macro will not auto-toggle.") :
                                                              fmt::format("Macro will toggle every {} frames.", frequency));
			if (MenuButton(freq_title.c_str(), freq_summary.c_str()))
				ImGui::OpenPopup(freq_title.c_str());

			ImGui::SetNextWindowSize(LayoutScale(500.0f, 180.0f));

			ImGui::PushFont(g_large_font);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
				LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));

			if (ImGui::BeginPopupModal(
					freq_title.c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
			{
				ImGui::SetNextItemWidth(LayoutScale(450.0f));
				if (ImGui::SliderInt("##value", &frequency, 0, 60, "Toggle every %d frames", ImGuiSliderFlags_NoInput))
				{
					if (frequency == 0)
						bsi->DeleteValue(section.c_str(), freq_key.c_str());
					else
						bsi->SetIntValue(section.c_str(), freq_key.c_str(), frequency);
				}

				BeginMenuButtons();
				if (MenuButton("OK", nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
					ImGui::CloseCurrentPopup();
				EndMenuButtons();

				ImGui::EndPopup();
			}

			ImGui::PopStyleVar(3);
			ImGui::PopFont();
		}

		if (ci->num_settings > 0)
		{
			MenuHeading((mtap_enabled[mtap_port] ?
							 fmt::format(ICON_FA_SLIDERS_H "  Controller Port {}{} Settings", mtap_port + 1, mtap_slot_names[mtap_slot]) :
                             fmt::format(ICON_FA_SLIDERS_H "  Controller Port {} Settings", mtap_port + 1))
							.c_str());

			for (u32 i = 0; i < ci->num_settings; i++)
			{
				const PAD::ControllerSettingInfo& si = ci->settings[i];
				std::string title(fmt::format(ICON_FA_COG "  {}", si.display_name));
				switch (si.type)
				{
					case PAD::ControllerSettingInfo::Type::Boolean:
						DrawToggleSetting(
							bsi, title.c_str(), si.description, section.c_str(), si.name, si.BooleanDefaultValue(), true, false);
						break;
					case PAD::ControllerSettingInfo::Type::Integer:
						DrawIntRangeSetting(bsi, title.c_str(), si.description, section.c_str(), si.name, si.IntegerDefaultValue(),
							si.IntegerMinValue(), si.IntegerMaxValue(), si.format, true);
						break;
					case PAD::ControllerSettingInfo::Type::Float:
						DrawFloatRangeSetting(bsi, title.c_str(), si.description, section.c_str(), si.name, si.FloatDefaultValue(),
							si.FloatMinValue(), si.FloatMaxValue(), si.format, si.multiplier, true);
						break;
					default:
						break;
				}
			}
		}
	}

	EndMenuButtons();
}

void FullscreenUI::DrawHotkeySettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	InputManager::GetHotkeyList();

	const HotkeyInfo* last_category = nullptr;
	for (const HotkeyInfo* hotkey : s_hotkey_list_cache)
	{
		if (!last_category || hotkey->category != last_category->category)
		{
			MenuHeading(hotkey->category);
			last_category = hotkey;
		}

		DrawInputBindingButton(bsi, PAD::ControllerBindingType::Button, "Hotkeys", hotkey->name, hotkey->display_name, false);
	}

	EndMenuButtons();
}

void FullscreenUI::DrawAchievementsSettingsPage()
{
	// TODO: Implement once achievements are merged.

	BeginMenuButtons();
	ActiveButton(ICON_FA_BAN "  This build was not compiled with Achivements support.", false, false,
		ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
	EndMenuButtons();
}

void FullscreenUI::DrawAdvancedSettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading("Logging");

	DrawToggleSetting(bsi, "System Console", "Writes log messages to the system console (console window/standard output).", "Logging",
		"EnableSystemConsole", false);
	DrawToggleSetting(bsi, "File Logging", "Writes log messages to emulog.txt.", "Logging", "EnableFileLogging", false);
	DrawToggleSetting(bsi, "Verbose Logging", "Writes dev log messages to log sinks.", "Logging", "EnableVerbose", false, !IsDevBuild);
	DrawToggleSetting(bsi, "Log Timestamps", "Writes timestamps alongside log messages.", "Logging", "EnableTimestamps", true);
	DrawToggleSetting(
		bsi, "EE Console", "Writes debug messages from the game's EE code to the console.", "Logging", "EnableEEConsole", true);
	DrawToggleSetting(
		bsi, "IOP Console", "Writes debug messages from the game's IOP code to the console.", "Logging", "EnableIOPConsole", true);
	DrawToggleSetting(bsi, "CDVD Verbose Reads", "Logs disc reads from games.", "EmuCore", "CdvdVerboseReads", false);

	MenuHeading("Advanced System");

	DrawToggleSetting(bsi, "Enable EE Recompiler",
		"Performs just-in-time binary translation of 64-bit MIPS-IV machine code to native code.", "EmuCore/CPU/Recompiler", "EnableEE",
		true);
	DrawToggleSetting(
		bsi, "Enable EE Cache", "Enables simulation of the EE's cache. Slow.", "EmuCore/CPU/Recompiler", "EnableEECache", false);
	DrawToggleSetting(bsi, "Enable INTC Spin Detection", "Huge speedup for some games, with almost no compatibility side effects.",
		"EmuCore/Speedhacks", "IntcStat", true);
	DrawToggleSetting(bsi, "Enable Wait Loop Detection", "Moderate speedup for some games, with no known side effects.",
		"EmuCore/Speedhacks", "WaitLoop", true);
	DrawToggleSetting(bsi, "Enable VU0 Recompiler (Micro Mode)",
		"New Vector Unit recompiler with much improved compatibility. Recommended.", "EmuCore/CPU/Recompiler", "EnableVU0", true);
	DrawToggleSetting(bsi, "Enable VU1 Recompiler", "New Vector Unit recompiler with much improved compatibility. Recommended.",
		"EmuCore/CPU/Recompiler", "EnableVU1", true);
	DrawToggleSetting(bsi, "Enable VU Flag Optimization", "Good speedup and high compatibility, may cause graphical errors.",
		"EmuCore/Speedhacks", "vuFlagHack", true);
	DrawToggleSetting(bsi, "Enable IOP Recompiler",
		"Performs just-in-time binary translation of 32-bit MIPS-I machine code to native code.", "EmuCore/CPU/Recompiler", "EnableIOP",
		true);

	MenuHeading("Graphics");

	DrawToggleSetting(bsi, "Use Debug Device", "Enables API-level validation of graphics commands", "EmuCore/GS", "UseDebugDevice", false);



	EndMenuButtons();
}

void FullscreenUI::DrawGameFixesSettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading("Game Fixes");
	ActiveButton("Game fixes should not be modified unless you are aware of what each option does and the implications of doing so.", false,
		false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

	DrawToggleSetting(bsi, "FPU Multiply Hack", "For Tales of Destiny.", "EmuCore/Gamefixes", "FpuMulHack", false);
	DrawToggleSetting(bsi, "FPU Negative Div Hack", "For Gundam games.", "EmuCore/Gamefixes", "FpuNegDivHack", false);
	DrawToggleSetting(bsi, "Preload TLB Hack", "To avoid tlb miss on Goemon.", "EmuCore/Gamefixes", "GoemonTlbHack", false);
	DrawToggleSetting(bsi, "Switch to Software renderer for FMVs.", "Needed for some games with complex FMV rendering.",
		"EmuCore/Gamefixes", "SoftwareRendererFMVHack", false);
	DrawToggleSetting(
		bsi, "Skip MPEG Hack", "Skips videos/FMVs in games to avoid game hanging/freezes.", "EmuCore/Gamefixes", "SkipMPEGHack", false);
	DrawToggleSetting(bsi, "OPH Flag Hack", "Known to affect following games: Bleach Blade Battler, Growlanser II and III, Wizardry.",
		"EmuCore/Gamefixes", "OPHFlagHack", false);
	DrawToggleSetting(bsi, "EE Timing Hack",
		"Known to affect following games: Digital Devil Saga (Fixes FMV and crashes), SSX (Fixes bad graphics and crashes).",
		"EmuCore/Gamefixes", "EETimingHack", false);
	DrawToggleSetting(bsi, "Handle DMAC writes when it is busy.",
		"Known to affect following games: Mana Khemia 1 (Going \"off campus\"), Metal Saga (Intro FMV), Pilot Down Behind Enemy Lines.",
		"EmuCore/Gamefixes", "DMABusyHack", false);
	DrawToggleSetting(bsi, "Force GIF PATH3 transfers through FIFO", "(Fifa Street 2).", "EmuCore/Gamefixes", "GIFFIFOHack", false);
	DrawToggleSetting(bsi, "Simulate VIF1 FIFO read ahead. Fixes slow loading games.",
		"Known to affect following games: Test Drive Unlimited, Transformers.", "EmuCore/Gamefixes", "VIFFIFOHack", false);
	DrawToggleSetting(
		bsi, "Delay VIF1 Stalls (VIF1 FIFO)", "For SOCOM 2 HUD and Spy Hunter loading hang.", "EmuCore/Gamefixes", "VIF1StallHack", false);
	DrawToggleSetting(bsi, "VU Add Hack", "Games that need this hack to boot: Star Ocean 3, Radiata Stories, Valkyrie Profile 2.",
		"EmuCore/Gamefixes", "VuAddSubHack", false);
	DrawToggleSetting(bsi, "VU I bit Hack avoid constant recompilation in some games",
		"Scarface The World Is Yours, Crash Tag Team Racing.", "EmuCore/Gamefixes", "IbitHack", false);
	DrawToggleSetting(bsi, "VU Sync (Run behind)", "To avoid sync problems when reading or writing VU registers.", "EmuCore/Gamefixes",
		"VUSyncHack", false);
	DrawToggleSetting(
		bsi, "VU Overflow Hack", "To check for possible float overflows (Superman Returns).", "EmuCore/Gamefixes", "VUOverflowHack", false);
	DrawToggleSetting(bsi, "VU XGkick Sync", "Use accurate timing for VU XGKicks (slower).", "EmuCore/Gamefixes", "XgKickHack", false);
	DrawToggleSetting(bsi, "Use Blit for internal FPS",
		"Use alternative method to calclate internal FPS to avoid false readings in some games.", "EmuCore/Gamefixes",
		"BlitInternalFPSHack", false);

	EndMenuButtons();
}

void FullscreenUI::DrawPauseMenu(MainWindowType type)
{
	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	const ImVec2 display_size(ImGui::GetIO().DisplaySize);
	dl->AddRectFilled(ImVec2(0.0f, 0.0f), display_size, IM_COL32(0x21, 0x21, 0x21, 200));

	// title info
	{
		const ImVec2 title_size(
			g_large_font->CalcTextSizeA(g_large_font->FontSize, std::numeric_limits<float>::max(), -1.0f, s_current_game_title.c_str()));
		const ImVec2 subtitle_size(g_medium_font->CalcTextSizeA(
			g_medium_font->FontSize, std::numeric_limits<float>::max(), -1.0f, s_current_game_subtitle.c_str()));

		ImVec2 title_pos(display_size.x - LayoutScale(20.0f + 50.0f + 20.0f) - title_size.x, display_size.y - LayoutScale(20.0f + 50.0f));
		ImVec2 subtitle_pos(display_size.x - LayoutScale(20.0f + 50.0f + 20.0f) - subtitle_size.x,
			title_pos.y + g_large_font->FontSize + LayoutScale(4.0f));
		float rp_height = 0.0f;

		dl->AddText(g_large_font, g_large_font->FontSize, title_pos, IM_COL32(255, 255, 255, 255), s_current_game_title.c_str());
		dl->AddText(g_medium_font, g_medium_font->FontSize, subtitle_pos, IM_COL32(255, 255, 255, 255), s_current_game_subtitle.c_str());

		const ImVec2 image_min(
			display_size.x - LayoutScale(20.0f + 50.0f) - rp_height, display_size.y - LayoutScale(20.0f + 50.0f) - rp_height);
		const ImVec2 image_max(image_min.x + LayoutScale(50.0f) + rp_height, image_min.y + LayoutScale(50.0f) + rp_height);
		dl->AddImage(GetCoverForCurrentGame()->GetHandle(), image_min, image_max);
	}

	const ImVec2 window_size(LayoutScale(500.0f, LAYOUT_SCREEN_HEIGHT));
	const ImVec2 window_pos(0.0f, display_size.y - window_size.y);

	if (BeginFullscreenWindow(
			window_pos, window_size, "pause_menu", ImVec4(0.0f, 0.0f, 0.0f, 0.0f), 0.0f, 10.0f, ImGuiWindowFlags_NoBackground))
	{
		static constexpr u32 submenu_item_count[] = {
			10, // None
			4, // Exit
		};

		const bool just_focused = ResetFocusHere();
		BeginMenuButtons(submenu_item_count[static_cast<u32>(s_current_pause_submenu)], 1.0f, ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING,
			ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

		switch (s_current_pause_submenu)
		{
			case PauseSubMenu::None:
			{
				// NOTE: Menu close must come first, because otherwise VM destruction options will race.
				const bool can_load_or_save_state = s_current_game_crc != 0;

				if (ActiveButton(ICON_FA_PLAY "  Resume Game", false) || WantsToCloseMenu())
					ClosePauseMenu();

				if (ActiveButton(ICON_FA_FAST_FORWARD "  Toggle Frame Limit", false))
				{
					ClosePauseMenu();
					DoToggleFrameLimit();
				}

				if (ActiveButton(ICON_FA_UNDO "  Load State", false, can_load_or_save_state))
				{
					if (OpenSaveStateSelector(true))
						s_current_main_window = MainWindowType::None;
				}

				if (ActiveButton(ICON_FA_DOWNLOAD "  Save State", false, can_load_or_save_state))
				{
					if (OpenSaveStateSelector(false))
						s_current_main_window = MainWindowType::None;
				}

				if (ActiveButton(ICON_FA_WRENCH "  Game Properties", false, can_load_or_save_state))
				{
					SwitchToGameSettings();
				}

				if (ActiveButton(ICON_FA_CAMERA "  Save Screenshot", false))
				{
					GSQueueSnapshot(std::string());
					ClosePauseMenu();
				}

				if (ActiveButton(GSConfig.UseHardwareRenderer() ? (ICON_FA_PAINT_BRUSH "  Switch To Software Renderer") :
                                                                  (ICON_FA_PAINT_BRUSH "  Switch To Hardware Renderer"),
						false))
				{
					ClosePauseMenu();
					DoToggleSoftwareRenderer();
				}

				if (ActiveButton(ICON_FA_COMPACT_DISC "  Change Disc", false))
				{
					s_current_main_window = MainWindowType::None;
					DoChangeDisc();
				}

				if (ActiveButton(ICON_FA_SLIDERS_H "  Settings", false))
					SwitchToSettings();

				if (ActiveButton(ICON_FA_POWER_OFF "  Close Game", false))
				{
					// skip submenu when we can't save anyway
					if (!can_load_or_save_state)
						DoShutdown(false);
					else
						OpenPauseSubMenu(PauseSubMenu::Exit);
				}
			}
			break;

			case PauseSubMenu::Exit:
			{
				if (just_focused)
					ImGui::SetFocusID(ImGui::GetID(ICON_FA_POWER_OFF "  Exit Without Saving"), ImGui::GetCurrentWindow());

				if (ActiveButton(ICON_FA_BACKWARD "  Back To Pause Menu", false))
				{
					OpenPauseSubMenu(PauseSubMenu::None);
				}

				if (ActiveButton(ICON_FA_SYNC "  Reset System", false))
				{
					ClosePauseMenu();
					DoReset();
				}

				if (ActiveButton(ICON_FA_SAVE "  Exit And Save State", false))
					DoShutdown(true);

				if (ActiveButton(ICON_FA_POWER_OFF "  Exit Without Saving", false))
					DoShutdown(false);
			}
			break;
		}

		EndMenuButtons();

		EndFullscreenWindow();
	}
}

void FullscreenUI::InitializePlaceholderSaveStateListEntry(
	SaveStateListEntry* li, const std::string& title, const std::string& serial, u32 crc, s32 slot)
{
	li->title = fmt::format("{0} Slot {1}##game_slot_{1}", s_current_game_title, slot);
	li->summary = "No Save State";
	li->path = {};
	li->slot = slot;
	li->preview_texture = {};
}

bool FullscreenUI::InitializeSaveStateListEntry(
	SaveStateListEntry* li, const std::string& title, const std::string& serial, u32 crc, s32 slot)
{
	std::string filename(VMManager::GetSaveStateFileName(serial.c_str(), crc, slot));
	FILESYSTEM_STAT_DATA sd;
	if (filename.empty() || !FileSystem::StatFile(filename.c_str(), &sd))
	{
		InitializePlaceholderSaveStateListEntry(li, title, serial, crc, slot);
		return false;
	}

	li->title = fmt::format("{0} Slot {1}##game_slot_{1}", title, slot);
	li->summary = fmt::format("{0} - Saved {1}", serial, TimeToPrintableString(sd.ModificationTime));
	li->slot = slot;
	li->path = std::move(filename);

	li->preview_texture.reset();

	u32 screenshot_width, screenshot_height;
	std::vector<u32> screenshot_pixels;
	if (SaveState_ReadScreenshot(li->path, &screenshot_width, &screenshot_height, &screenshot_pixels))
	{
		li->preview_texture = g_host_display->CreateTexture(
			screenshot_width, screenshot_height, screenshot_pixels.data(), sizeof(u32) * screenshot_width, false);
		if (!li->preview_texture)
			Console.Error("Failed to upload save state image to GPU");
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

	for (s32 i = 0; i <= MAX_SAVE_STATE_SLOTS; i++)
	{
		SaveStateListEntry li;
		if (InitializeSaveStateListEntry(&li, title, serial, crc, i) || !s_save_state_selector_loading)
			s_save_state_selector_slots.push_back(std::move(li));
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
			s_save_state_selector_game_path = game_path;
			return true;
		}
	}

	ShowToast({}, "No save states found.", 5.0f);
	return false;
}

bool FullscreenUI::OpenSaveStateSelector(bool is_loading)
{
	s_save_state_selector_game_path = {};
	s_save_state_selector_loading = is_loading;
	if (PopulateSaveStateListEntries(s_current_game_title.c_str(), s_current_game_serial.c_str(), s_current_game_crc) > 0)
	{
		s_save_state_selector_open = true;
		return true;
	}

	ShowToast({}, "No save states found.", 5.0f);
	return false;
}

void FullscreenUI::CloseSaveStateSelector()
{
	ClearSaveStateEntryList();
	s_save_state_selector_open = false;
	s_save_state_selector_loading = false;
	s_save_state_selector_game_path = {};
	ReturnToMainWindow();
}

void FullscreenUI::DrawSaveStateSelector(bool is_loading, bool fullscreen)
{
	if (fullscreen)
	{
		if (!BeginFullscreenColumns())
		{
			EndFullscreenColumns();
			return;
		}

		if (!BeginFullscreenColumnWindow(0.0f, LAYOUT_SCREEN_WIDTH, "save_state_selector_slots"))
		{
			EndFullscreenColumnWindow();
			EndFullscreenColumns();
			return;
		}
	}
	else
	{
		const char* window_title = is_loading ? "Load State" : "Save State";

		ImGui::PushFont(g_large_font);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
			LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

		ImGui::SetNextWindowSize(LayoutScale(1000.0f, 680.0f));
		ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::OpenPopup(window_title);
		bool is_open = !WantsToCloseMenu();
		if (!ImGui::BeginPopupModal(
				window_title, &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove) ||
			!is_open)
		{
			ImGui::PopStyleVar(2);
			ImGui::PopFont();
			CloseSaveStateSelector();
			return;
		}
	}

	BeginMenuButtons();

	constexpr float padding = 10.0f;
	constexpr float button_height = 96.0f;
	constexpr float max_image_width = 96.0f;
	constexpr float max_image_height = 96.0f;

	for (const SaveStateListEntry& entry : s_save_state_selector_slots)
	{
		ImRect bb;
		bool visible, hovered;
		bool pressed = MenuButtonFrame(entry.title.c_str(), true, button_height, &visible, &hovered, &bb.Min, &bb.Max);
		if (!visible)
			continue;

		ImVec2 pos(bb.Min);

		// use aspect ratio of screenshot to determine height
		const HostDisplayTexture* image = entry.preview_texture ? entry.preview_texture.get() : GetPlaceholderTexture().get();
		const float image_height = max_image_width / (static_cast<float>(image->GetWidth()) / static_cast<float>(image->GetHeight()));
		const float image_margin = (max_image_height - image_height) / 2.0f;
		const ImRect image_bb(
			ImVec2(pos.x, pos.y + LayoutScale(image_margin)), pos + LayoutScale(max_image_width, image_margin + image_height));
		pos.x += LayoutScale(max_image_width + padding);

		ImRect text_bb(pos, ImVec2(bb.Max.x, pos.y + g_large_font->FontSize));
		ImGui::PushFont(g_large_font);
		ImGui::RenderTextClipped(text_bb.Min, text_bb.Max, entry.title.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &text_bb);
		ImGui::PopFont();

		ImGui::PushFont(g_medium_font);

		if (!entry.summary.empty())
		{
			text_bb.Min.y = text_bb.Max.y + LayoutScale(4.0f);
			text_bb.Max.y = text_bb.Min.y + g_medium_font->FontSize;
			ImGui::RenderTextClipped(text_bb.Min, text_bb.Max, entry.summary.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &text_bb);
		}

		if (!entry.path.empty())
		{
			text_bb.Min.y = text_bb.Max.y + LayoutScale(4.0f);
			text_bb.Max.y = text_bb.Min.y + g_medium_font->FontSize;
			ImGui::RenderTextClipped(text_bb.Min, text_bb.Max, entry.path.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &text_bb);
		}

		ImGui::PopFont();

		ImGui::GetWindowDrawList()->AddImage(
			static_cast<ImTextureID>(entry.preview_texture ? entry.preview_texture->GetHandle() : GetPlaceholderTexture()->GetHandle()),
			image_bb.Min, image_bb.Max);

		if (pressed)
		{
			if (is_loading)
			{
				DoLoadState(entry.path);
				CloseSaveStateSelector();
			}
			else
			{
				Host::RunOnCPUThread([slot = entry.slot]() { VMManager::SaveStateToSlot(slot); });
				CloseSaveStateSelector();
			}
		}
	}

	EndMenuButtons();

	if (fullscreen)
	{
		EndFullscreenColumnWindow();
		EndFullscreenColumns();
	}
	else
	{
		ImGui::EndPopup();
		ImGui::PopStyleVar(2);
		ImGui::PopFont();
	}
}

void FullscreenUI::DoLoadState(std::string path)
{
	Host::RunOnCPUThread([boot_path = s_save_state_selector_game_path, path = std::move(path)]() {
		if (VMManager::HasValidVM())
		{
			VMManager::LoadState(path.c_str());
			if (!boot_path.empty() && VMManager::GetDiscPath() != boot_path)
				VMManager::ChangeDisc(CDVD_SourceType::Iso, std::move(boot_path));
		}
		else
		{
			VMBootParameters params;
			params.filename = std::move(boot_path);
			params.save_state = std::move(path);
			if (VMManager::Initialize(params))
				VMManager::SetState(VMState::Running);
		}
	});
}

void FullscreenUI::PopulateGameListEntryList()
{
	const u32 count = GameList::GetEntryCount();
	s_game_list_sorted_entries.resize(count);
	for (u32 i = 0; i < count; i++)
		s_game_list_sorted_entries[i] = GameList::GetEntryByIndex(i);

	// TODO: Custom sort types
	std::sort(s_game_list_sorted_entries.begin(), s_game_list_sorted_entries.end(),
		[](const GameList::Entry* lhs, const GameList::Entry* rhs) { return lhs->title < rhs->title; });
}

void FullscreenUI::DrawGameListWindow()
{
	if (!BeginFullscreenColumns())
	{
		EndFullscreenColumns();
		return;
	}

	auto game_list_lock = GameList::GetLock();
	const GameList::Entry* selected_entry = nullptr;
	PopulateGameListEntryList();

	if (BeginFullscreenColumnWindow(0.0f, 750.0f, "game_list_entries"))
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

			HostDisplayTexture* cover_texture = GetGameListCover(entry);

			summary.clear();
			if (entry->serial.empty())
				fmt::format_to(std::back_inserter(summary), "{} - ", GameList::RegionToString(entry->region));
			else
				fmt::format_to(std::back_inserter(summary), "{} - {} - ", entry->serial, GameList::RegionToString(entry->region));

			const std::string_view filename(Path::GetFileName(entry->path));
			summary.append(filename);

			const ImRect image_rect(CenterImage(ImRect(bb.Min, bb.Min + image_size),
				ImVec2(static_cast<float>(cover_texture->GetWidth()), static_cast<float>(cover_texture->GetHeight()))));

			ImGui::GetWindowDrawList()->AddImage(cover_texture->GetHandle(), image_rect.Min, image_rect.Max, ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f), IM_COL32(255, 255, 255, 255));

			const float midpoint = bb.Min.y + g_large_font->FontSize + LayoutScale(4.0f);
			const float text_start_x = bb.Min.x + image_size.x + LayoutScale(15.0f);
			const ImRect title_bb(ImVec2(text_start_x, bb.Min.y), ImVec2(bb.Max.x, midpoint));
			const ImRect summary_bb(ImVec2(text_start_x, midpoint), bb.Max);

			ImGui::PushFont(g_large_font);
			ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, entry->title.c_str(), entry->title.c_str() + entry->title.size(), nullptr,
				ImVec2(0.0f, 0.0f), &title_bb);
			ImGui::PopFont();

			if (!summary.empty())
			{
				ImGui::PushFont(g_medium_font);
				ImGui::RenderTextClipped(
					summary_bb.Min, summary_bb.Max, summary.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
				ImGui::PopFont();
			}

			if (pressed)
			{
				// launch game
				DoStartPath(entry->path);
			}

			if (hovered)
				selected_entry = entry;

			if (selected_entry &&
				(ImGui::IsItemClicked(ImGuiMouseButton_Right) || ImGui::IsNavInputTest(ImGuiNavInput_Input, ImGuiNavReadMode_Pressed)))
			{
				ImGuiFullscreen::ChoiceDialogOptions options = {
					{"Open Game Properties", false},
					{"Resume Game", false},
					{"Load State", false},
					{"Default Boot", false},
					{"Fast Boot", false},
					{"Slow Boot", false},
					{"Close Menu", false},
				};

				OpenChoiceDialog(selected_entry->title.c_str(), false, std::move(options),
					[entry_path = selected_entry->path](s32 index, const std::string& title, bool checked) {
						switch (index)
						{
							case 0: // Open Game Properties
								SwitchToGameSettings(entry_path);
								break;
							case 1: // Resume Game
								DoStartPath(entry_path, -1);
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
							case 5: // Slow Boot
								DoStartPath(entry_path, std::nullopt, false);
								break;
							default:
								break;
						}

						CloseChoiceDialog();
					});
			}
		}

		EndMenuButtons();
	}
	EndFullscreenColumnWindow();

	if (BeginFullscreenColumnWindow(750.0f, LAYOUT_SCREEN_WIDTH, "game_list_info", UIPrimaryDarkColor))
	{
		const HostDisplayTexture* cover_texture =
			selected_entry ? GetGameListCover(selected_entry) : GetTextureForGameListEntryType(GameList::EntryType::Count);
		if (cover_texture)
		{
			const ImRect image_rect(CenterImage(LayoutScale(ImVec2(275.0f, 400.0f)),
				ImVec2(static_cast<float>(cover_texture->GetWidth()), static_cast<float>(cover_texture->GetHeight()))));

			ImGui::SetCursorPos(LayoutScale(ImVec2(128.0f, 20.0f)) + image_rect.Min);
			ImGui::Image(selected_entry ? GetGameListCover(selected_entry)->GetHandle() :
                                          GetTextureForGameListEntryType(GameList::EntryType::Count)->GetHandle(),
				image_rect.GetSize());
		}

		const float work_width = ImGui::GetCurrentWindow()->WorkRect.GetWidth();
		constexpr float field_margin_y = 10.0f;
		constexpr float start_x = 50.0f;
		float text_y = 440.0f;
		float text_width;

		ImGui::SetCursorPos(LayoutScale(start_x, text_y));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, field_margin_y));
		ImGui::PushTextWrapPos(LayoutScale(480.0f));
		ImGui::BeginGroup();

		if (selected_entry)
		{
			// title
			ImGui::PushFont(g_large_font);
			const std::string_view title(
				std::string_view(selected_entry->title).substr(0, (selected_entry->title.length() > 37) ? 37 : std::string_view::npos));
			text_width = ImGui::CalcTextSize(title.data(), title.data() + title.length(), false, work_width).x;
			ImGui::SetCursorPosX((work_width - text_width) / 2.0f);
			ImGui::TextWrapped(
				"%.*s%s", static_cast<int>(title.size()), title.data(), (title.length() == selected_entry->title.length()) ? "" : "...");
			ImGui::PopFont();

			ImGui::PushFont(g_medium_font);

			// code
			text_width = ImGui::CalcTextSize(selected_entry->serial.c_str(), nullptr, false, work_width).x;
			ImGui::SetCursorPosX((work_width - text_width) / 2.0f);
			ImGui::TextWrapped("%s", selected_entry->serial.c_str());
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 15.0f);

			// file tile
			const std::string_view filename(Path::GetFileName(selected_entry->path));
			ImGui::TextWrapped("File: %.*s", static_cast<int>(filename.size()), filename.data());

			// crc
			ImGui::Text("CRC: %08X", selected_entry->crc);

			// region
			{
				std::string flag_texture(fmt::format("icons/flags/{}.png", GameList::RegionToString(selected_entry->region)));
				ImGui::TextUnformatted("Region: ");
				ImGui::SameLine();
				ImGui::Image(GetCachedTextureAsync(flag_texture.c_str())->GetHandle(), LayoutScale(23.0f, 16.0f));
				ImGui::SameLine();
				ImGui::Text(" (%s)", GameList::RegionToString(selected_entry->region));
			}

			// compatibility
			ImGui::TextUnformatted("Compatibility: ");
			ImGui::SameLine();
			if (selected_entry->compatibility_rating != GameDatabaseSchema::Compatibility::Unknown)
			{
				ImGui::Image(s_game_compatibility_textures[static_cast<u32>(selected_entry->compatibility_rating) - 1]->GetHandle(),
					LayoutScale(64.0f, 16.0f));
				ImGui::SameLine();
			}
			ImGui::Text(" (%s)", GameList::EntryCompatibilityRatingToString(selected_entry->compatibility_rating));

			// size
			ImGui::Text("Size: %.2f MB", static_cast<float>(selected_entry->total_size) / 1048576.0f);

			ImGui::PopFont();
		}
		else
		{
			// title
			const char* title = "No Game Selected";
			ImGui::PushFont(g_large_font);
			text_width = ImGui::CalcTextSize(title, nullptr, false, work_width).x;
			ImGui::SetCursorPosX((work_width - text_width) / 2.0f);
			ImGui::TextWrapped("%s", title);
			ImGui::PopFont();
		}

		ImGui::EndGroup();
		ImGui::PopTextWrapPos();
		ImGui::PopStyleVar();

		ImGui::SetCursorPosY(ImGui::GetWindowHeight() - LayoutScale(50.0f));
		BeginMenuButtons();
		if (ActiveButton(ICON_FA_BACKWARD "  Back", false))
			ReturnToMainWindow();
		EndMenuButtons();
	}
	EndFullscreenColumnWindow();

	EndFullscreenColumns();
}

void FullscreenUI::SwitchToGameList()
{
	s_current_main_window = MainWindowType::GameList;
	QueueResetFocus();
}

HostDisplayTexture* FullscreenUI::GetGameListCover(const GameList::Entry* entry)
{
	// lookup and grab cover image
	auto cover_it = s_cover_image_map.find(entry->path);
	if (cover_it == s_cover_image_map.end())
	{
		std::string cover_path(GameList::GetCoverImagePathForEntry(entry));
		cover_it = s_cover_image_map.emplace(entry->path, std::move(cover_path)).first;
	}

	HostDisplayTexture* tex = (!cover_it->second.empty()) ? GetCachedTextureAsync(cover_it->second.c_str()) : nullptr;
	return tex ? tex : GetTextureForGameListEntryType(entry->type);
}

HostDisplayTexture* FullscreenUI::GetTextureForGameListEntryType(GameList::EntryType type)
{
	switch (type)
	{
		case GameList::EntryType::ELF:
			return s_fallback_exe_texture.get();

		case GameList::EntryType::PS1Disc:
		case GameList::EntryType::PS2Disc:
		default:
			return s_fallback_disc_texture.get();
	}
}

HostDisplayTexture* FullscreenUI::GetCoverForCurrentGame()
{
	auto lock = GameList::GetLock();

	const GameList::Entry* entry = GameList::GetEntryForPath(s_current_game_path.c_str());
	if (!entry)
		return s_fallback_disc_texture.get();

	return GetGameListCover(entry);
}

std::string FullscreenUI::GetNotificationImageForGame(const GameList::Entry* entry)
{
	std::string ret;

	if (entry)
		ret = GameList::GetCoverImagePathForEntry(entry);

	return ret;
}

std::string FullscreenUI::GetNotificationImageForGame(const std::string& game_path)
{
	auto lock = GameList::GetLock();
	const GameList::Entry* entry = GameList::GetEntryForPath(game_path.c_str());
	return entry ? GetNotificationImageForGame(entry) : std::string();
}

//////////////////////////////////////////////////////////////////////////
// Overlays
//////////////////////////////////////////////////////////////////////////

void FullscreenUI::ExitFullscreenAndOpenURL(const std::string_view& url)
{
	Host::RunOnCPUThread([url = std::string(url)]() {
		if (Host::IsFullscreen())
			Host::SetFullscreen(false);

		Host::OpenURL(url);
	});
}

void FullscreenUI::CopyTextToClipboard(std::string title, const std::string_view& text)
{
	if (Host::CopyTextToClipboard(text))
		ShowToast(std::string(), std::move(title));
	else
		ShowToast(std::string(), "Failed to copy text to clipboard.");
}

void FullscreenUI::OpenAboutWindow()
{
	s_about_window_open = true;
}

void FullscreenUI::DrawAboutWindow()
{
	ImGui::SetNextWindowSize(LayoutScale(1000.0f, 500.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::OpenPopup("About PCSX2");

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));

	if (ImGui::BeginPopupModal("About PCSX2", &s_about_window_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
	{
		ImGui::TextWrapped(
			"PCSX2 is a free and open-source PlayStation 2 (PS2) emulator. Its purpose is to emulate the PS2's hardware, using a "
			"combination of MIPS CPU Interpreters, Recompilers and a Virtual Machine which manages hardware states and PS2 system memory. "
			"This allows you to play PS2 games on your PC, with many additional features and benefits.");

		ImGui::NewLine();

		ImGui::TextWrapped("PlayStation 2 and PS2 are registered trademarks of Sony Interactive Entertainment. This application is not "
						   "affiliated in any way with Sony Interactive Entertainment.");

		ImGui::NewLine();

		BeginMenuButtons();

		if (ActiveButton(ICON_FA_GLOBE "  Website", false))
			ExitFullscreenAndOpenURL(PCSX2_WEBSITE_URL);

		if (ActiveButton(ICON_FA_PERSON_BOOTH "  Support Forums", false))
			ExitFullscreenAndOpenURL(PCSX2_FORUMS_URL);

		if (ActiveButton(ICON_FA_BUG "  GitHub Repository", false))
			ExitFullscreenAndOpenURL(PCSX2_GITHUB_URL);

		if (ActiveButton(ICON_FA_NEWSPAPER "  License", false))
			ExitFullscreenAndOpenURL(PCSX2_LICENSE_URL);

		if (ActiveButton(ICON_FA_WINDOW_CLOSE "  Close", false))
		{
			ImGui::CloseCurrentPopup();
			s_about_window_open = false;
		}
		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(2);
	ImGui::PopFont();
}

FullscreenUI::ProgressCallback::ProgressCallback(std::string name)
	: BaseProgressCallback()
	, m_name(std::move(name))
{
	ImGuiFullscreen::OpenBackgroundProgressDialog(m_name.c_str(), "", 0, 100, 0);
}

FullscreenUI::ProgressCallback::~ProgressCallback()
{
	ImGuiFullscreen::CloseBackgroundProgressDialog(m_name.c_str());
}

void FullscreenUI::ProgressCallback::PushState()
{
	BaseProgressCallback::PushState();
}

void FullscreenUI::ProgressCallback::PopState()
{
	BaseProgressCallback::PopState();
	Redraw(true);
}

void FullscreenUI::ProgressCallback::SetCancellable(bool cancellable)
{
	BaseProgressCallback::SetCancellable(cancellable);
	Redraw(true);
}

void FullscreenUI::ProgressCallback::SetTitle(const char* title)
{
	// todo?
}

void FullscreenUI::ProgressCallback::SetStatusText(const char* text)
{
	BaseProgressCallback::SetStatusText(text);
	Redraw(true);
}

void FullscreenUI::ProgressCallback::SetProgressRange(u32 range)
{
	u32 last_range = m_progress_range;

	BaseProgressCallback::SetProgressRange(range);

	if (m_progress_range != last_range)
		Redraw(false);
}

void FullscreenUI::ProgressCallback::SetProgressValue(u32 value)
{
	u32 lastValue = m_progress_value;

	BaseProgressCallback::SetProgressValue(value);

	if (m_progress_value != lastValue)
		Redraw(false);
}

void FullscreenUI::ProgressCallback::Redraw(bool force)
{
	const int percent = static_cast<int>((static_cast<float>(m_progress_value) / static_cast<float>(m_progress_range)) * 100.0f);
	if (percent == m_last_progress_percent && !force)
		return;

	m_last_progress_percent = percent;
	ImGuiFullscreen::UpdateBackgroundProgressDialog(m_name.c_str(), m_status_text.c_str(), 0, 100, percent);
}

void FullscreenUI::ProgressCallback::DisplayError(const char* message)
{
	Console.Error(message);
	Host::ReportErrorAsync("Error", message);
}

void FullscreenUI::ProgressCallback::DisplayWarning(const char* message)
{
	Console.Warning(message);
}

void FullscreenUI::ProgressCallback::DisplayInformation(const char* message)
{
	Console.WriteLn(message);
}

void FullscreenUI::ProgressCallback::DisplayDebugMessage(const char* message)
{
	DevCon.WriteLn(message);
}

void FullscreenUI::ProgressCallback::ModalError(const char* message)
{
	Console.Error(message);
	Host::ReportErrorAsync("Error", message);
}

bool FullscreenUI::ProgressCallback::ModalConfirmation(const char* message)
{
	return false;
}

void FullscreenUI::ProgressCallback::ModalInformation(const char* message)
{
	Console.WriteLn(message);
}

void FullscreenUI::ProgressCallback::SetCancelled()
{
	if (m_cancellable)
		m_cancelled = true;
}
