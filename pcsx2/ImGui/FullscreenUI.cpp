/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "CDVD/CDVDcommon.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "GS/Renderers/Common/GSTexture.h"
#include "Achievements.h"
#include "CDVD/CDVDdiscReader.h"
#include "GameList.h"
#include "Host.h"
#include "INISettingsInterface.h"
#include "ImGui/FullscreenUI.h"
#include "ImGui/ImGuiFullscreen.h"
#include "ImGui/ImGuiManager.h"
#include "Input/InputManager.h"
#include "MTGS.h"
#include "USB/USB.h"
#include "VMManager.h"
#include "ps2/BiosTools.h"
#include "Patch.h"
#include "svnrev.h"
#include "SysForwardDefs.h"

#include "common/FileSystem.h"
#include "common/Console.h"
#include "common/Image.h"
#include "common/Path.h"
#include "common/SettingsInterface.h"
#include "common/SettingsWrapper.h"
#include "common/StringUtil.h"
#include "common/Threading.h"
#include "common/Timer.h"

#include "SIO/Memcard/MemoryCardFile.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Sio.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "IconsFontAwesome5.h"

#include "fmt/core.h"

#include <array>
#include <bitset>
#include <thread>
#include <utility>
#include <vector>

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
using ImGuiFullscreen::UISecondaryWeakColor;
using ImGuiFullscreen::UISecondaryStrongColor;
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
using ImGuiFullscreen::MenuButtonWithoutSummary;
using ImGuiFullscreen::MenuButtonWithValue;
using ImGuiFullscreen::MenuHeading;
using ImGuiFullscreen::MenuHeadingButton;
using ImGuiFullscreen::MenuImageButton;
using ImGuiFullscreen::ModAlpha;
using ImGuiFullscreen::MulAlpha;
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
		PauseMenu,
#ifdef ENABLE_ACHIEVEMENTS
		Achievements,
		Leaderboards,
#endif
	};

	enum class PauseSubMenu
	{
		None,
		Exit,
#ifdef ENABLE_ACHIEVEMENTS
		Achievements,
#endif
	};

	enum class SettingsPage
	{
		Summary,
		Interface,
		BIOS,
		Emulation,
		Graphics,
		Audio,
		MemoryCard,
		Controller,
		Hotkey,
		Achievements,
		Folders,
		Advanced,
		Patches,
		Cheats,
		GameFixes,
		Count
	};

	enum class GameListPage
	{
		Grid,
		List,
		Settings,
		Count
	};

	//////////////////////////////////////////////////////////////////////////
	// Utility
	//////////////////////////////////////////////////////////////////////////
	static void ReleaseTexture(std::unique_ptr<GSTexture>& tex);
	static std::string TimeToPrintableString(time_t t);
	static void StartAsyncOp(std::function<void(::ProgressCallback*)> callback, std::string name);
	static void AsyncOpThreadEntryPoint(std::function<void(::ProgressCallback*)> callback, FullscreenUI::ProgressCallback* progress);
	static void CancelAsyncOpWithName(const std::string_view& name);
	static void CancelAsyncOps();

	//////////////////////////////////////////////////////////////////////////
	// Main
	//////////////////////////////////////////////////////////////////////////
	static void UpdateGameDetails(std::string path, std::string serial, std::string title, u32 disc_crc, u32 crc);
	static void ToggleTheme();
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

	// async operations (e.g. cover downloads)
	using AsyncOpEntry = std::pair<std::thread, std::unique_ptr<FullscreenUI::ProgressCallback>>;
	static std::mutex s_async_op_mutex;
	static std::deque<AsyncOpEntry> s_async_ops;

	// local copies of the currently-running game
	static std::string s_current_game_title;
	static std::string s_current_game_subtitle;
	static std::string s_current_disc_serial;
	static std::string s_current_disc_path;
	static u32 s_current_disc_crc;

	//////////////////////////////////////////////////////////////////////////
	// Resources
	//////////////////////////////////////////////////////////////////////////
	static bool LoadResources();
	static void DestroyResources();

	static std::shared_ptr<GSTexture> s_app_icon_texture;
	static std::array<std::shared_ptr<GSTexture>, static_cast<u32>(GameDatabaseSchema::Compatibility::Perfect)>
		s_game_compatibility_textures;
	static std::shared_ptr<GSTexture> s_fallback_disc_texture;
	static std::shared_ptr<GSTexture> s_fallback_exe_texture;
	static std::vector<std::unique_ptr<GSTexture>> s_cleanup_textures;

	//////////////////////////////////////////////////////////////////////////
	// Landing
	//////////////////////////////////////////////////////////////////////////
	static void SwitchToLanding();
	static ImGuiFullscreen::FileSelectorFilters GetOpenFileFilters();
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
	static void DrawCoverDownloaderWindow();
	static void DrawBIOSSettingsPage();
	static void DrawEmulationSettingsPage();
	static void DrawGraphicsSettingsPage();
	static void DrawAudioSettingsPage();
	static void DrawMemoryCardSettingsPage();
	static void DrawCreateMemoryCardWindow();
	static void DrawControllerSettingsPage();
	static void DrawHotkeySettingsPage();
	static void DrawAchievementsSettingsPage(std::unique_lock<std::mutex>& settings_lock);
	static void DrawFoldersSettingsPage();
	static void DrawAchievementsLoginWindow();
	static void DrawAdvancedSettingsPage();
	static void DrawPatchesOrCheatsSettingsPage(bool cheats);
	static void DrawGameFixesSettingsPage();

	static bool IsEditingGameSettings(SettingsInterface* bsi);
	static SettingsInterface* GetEditingSettingsInterface();
	static SettingsInterface* GetEditingSettingsInterface(bool game_settings);
	static bool ShouldShowAdvancedSettings(SettingsInterface* bsi);
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
	static void DrawIntSpinBoxSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		int default_value, int min_value, int max_value, int step_value, const char* format = "%d", bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font, ImFont* summary_font = g_medium_font);
#if 0
	// Unused as of now
	static void DrawFloatRangeSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		float default_value, float min_value, float max_value, const char* format = "%f", float multiplier = 1.0f, bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font, ImFont* summary_font = g_medium_font);
#endif
	static void DrawFloatSpinBoxSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
		const char* key, float default_value, float min_value, float max_value, float step_value, float multiplier,
		const char* format = "%f", bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT,
		ImFont* font = g_large_font, ImFont* summary_font = g_medium_font);
	static void DrawIntRectSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
		const char* left_key, int default_left, const char* top_key, int default_top, const char* right_key, int default_right,
		const char* bottom_key, int default_bottom, int min_value, int max_value, int step_value, const char* format = "%d",
		bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font,
		ImFont* summary_font = g_medium_font);
	static void DrawStringListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		const char* default_value, const char* const* options, const char* const* option_values, size_t option_count, bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font, ImFont* summary_font = g_medium_font);
	static void DrawStringListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		const char* default_value, SettingInfo::GetOptionsCallback options_callback, bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font, ImFont* summary_font = g_medium_font);
	static void DrawFloatListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		float default_value, const char* const* options, const float* option_values, size_t option_count, bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font, ImFont* summary_font = g_medium_font);
	static void DrawFolderSetting(SettingsInterface* bsi, const char* title, const char* section, const char* key,
		const std::string& runtime_var, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font,
		ImFont* summary_font = g_medium_font);
	static void DrawPathSetting(SettingsInterface* bsi, const char* title, const char* section, const char* key, const char* default_value,
		bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, ImFont* font = g_large_font,
		ImFont* summary_font = g_medium_font);
	static void DrawClampingModeSetting(SettingsInterface* bsi, const char* title, const char* summary, int vunum);
	static void PopulateGraphicsAdapterList();
	static void PopulateGameListDirectoryCache(SettingsInterface* si);
	static void PopulatePatchesAndCheatsList(const std::string_view& serial, u32 crc);
	static void BeginInputBinding(SettingsInterface* bsi, InputBindingInfo::Type type, const std::string_view& section,
		const std::string_view& key, const std::string_view& display_name);
	static void DrawInputBindingWindow();
	static void DrawInputBindingButton(SettingsInterface* bsi, InputBindingInfo::Type type, const char* section, const char* name,
		const char* display_name, bool show_type = true);
	static void ClearInputBindingVariables();
	static void StartAutomaticBinding(u32 port);
	static void DrawSettingInfoSetting(SettingsInterface* bsi, const char* section, const char* key, const SettingInfo& si);

	static SettingsPage s_settings_page = SettingsPage::Interface;
	static std::unique_ptr<INISettingsInterface> s_game_settings_interface;
	static std::unique_ptr<GameList::Entry> s_game_settings_entry;
	static std::vector<std::pair<std::string, bool>> s_game_list_directories_cache;
	static std::vector<std::string> s_graphics_adapter_list_cache;
	static std::vector<std::string> s_fullscreen_mode_list_cache;
	static Patch::PatchInfoList s_game_patch_list;
	static std::vector<std::string> s_enabled_game_patch_cache;
	static Patch::PatchInfoList s_game_cheats_list;
	static std::vector<std::string> s_enabled_game_cheat_cache;
	static u32 s_game_cheat_unlabelled_count = 0;
	static std::vector<const HotkeyInfo*> s_hotkey_list_cache;
	static std::atomic_bool s_settings_changed{false};
	static std::atomic_bool s_game_settings_changed{false};
	static InputBindingInfo::Type s_input_binding_type = InputBindingInfo::Type::Unknown;
	static std::string s_input_binding_section;
	static std::string s_input_binding_key;
	static std::string s_input_binding_display_name;
	static std::vector<InputBindingKey> s_input_binding_new_bindings;
	static std::vector<std::pair<InputBindingKey, std::pair<float, float>>> s_input_binding_value_ranges;
	static Common::Timer s_input_binding_timer;

	//////////////////////////////////////////////////////////////////////////
	// Save State List
	//////////////////////////////////////////////////////////////////////////
	struct SaveStateListEntry
	{
		std::string title;
		std::string summary;
		std::string path;
		std::unique_ptr<GSTexture> preview_texture;
		time_t timestamp;
		s32 slot;
	};

	static void InitializePlaceholderSaveStateListEntry(SaveStateListEntry* li, s32 slot);
	static bool InitializeSaveStateListEntry(
		SaveStateListEntry* li, const std::string& title, const std::string& serial, u32 crc, s32 slot);
	static void ClearSaveStateEntryList();
	static u32 PopulateSaveStateListEntries(const std::string& title, const std::string& serial, u32 crc);
	static bool OpenLoadStateSelectorForGame(const std::string& game_path);
	static bool OpenSaveStateSelector(bool is_loading);
	static void CloseSaveStateSelector();
	static void DrawSaveStateSelector(bool is_loading);
	static bool OpenLoadStateSelectorForGameResume(const GameList::Entry* entry);
	static void DrawResumeStateSelector();
	static void DoLoadState(std::string path);

	static std::vector<SaveStateListEntry> s_save_state_selector_slots;
	static std::string s_save_state_selector_game_path;
	static s32 s_save_state_selector_submenu_index = -1;
	static bool s_save_state_selector_open = false;
	static bool s_save_state_selector_loading = true;
	static bool s_save_state_selector_resuming = false;

	//////////////////////////////////////////////////////////////////////////
	// Game List
	//////////////////////////////////////////////////////////////////////////
	static void DrawGameListWindow();
	static void DrawGameList(const ImVec2& heading_size);
	static void DrawGameGrid(const ImVec2& heading_size);
	static void HandleGameListActivate(const GameList::Entry* entry);
	static void HandleGameListOptions(const GameList::Entry* entry);
	static void DrawGameListSettingsPage(const ImVec2& heading_size);
	static void SwitchToGameList();
	static void PopulateGameListEntryList();
	static GSTexture* GetTextureForGameListEntryType(GameList::EntryType type);
	static GSTexture* GetGameListCover(const GameList::Entry* entry);
	static GSTexture* GetCoverForCurrentGame();

	// Lazily populated cover images.
	static std::unordered_map<std::string, std::string> s_cover_image_map;
	static std::vector<const GameList::Entry*> s_game_list_sorted_entries;
	static GameListPage s_game_list_page = GameListPage::Grid;

#ifdef ENABLE_ACHIEVEMENTS
	//////////////////////////////////////////////////////////////////////////
	// Achievements
	//////////////////////////////////////////////////////////////////////////
	static void SwitchToAchievementsWindow();
	static void DrawAchievementsWindow();
	static void DrawAchievement(const Achievements::Achievement& cheevo);
	static void DrawPrimedAchievementsIcons();
	static void DrawPrimedAchievementsList();
	static void SwitchToLeaderboardsWindow();
	static void DrawLeaderboardsWindow();
	static void DrawLeaderboardListEntry(const Achievements::Leaderboard& lboard);
	static void DrawLeaderboardEntry(
		const Achievements::LeaderboardEntry& lbEntry, float rank_column_width, float name_column_width, float column_spacing);

	static std::optional<u32> s_open_leaderboard_id;
#endif
} // namespace FullscreenUI

//////////////////////////////////////////////////////////////////////////
// Utility
//////////////////////////////////////////////////////////////////////////

void FullscreenUI::ReleaseTexture(std::unique_ptr<GSTexture>& tex)
{
	if (tex)
		g_gs_device->Recycle(tex.release());
}

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

void FullscreenUI::StartAsyncOp(std::function<void(::ProgressCallback*)> callback, std::string name)
{
	CancelAsyncOpWithName(name);

	std::unique_lock lock(s_async_op_mutex);
	std::unique_ptr<FullscreenUI::ProgressCallback> progress(std::make_unique<FullscreenUI::ProgressCallback>(std::move(name)));
	std::thread thread(AsyncOpThreadEntryPoint, std::move(callback), progress.get());
	s_async_ops.emplace_back(std::move(thread), std::move(progress));
}

void FullscreenUI::CancelAsyncOpWithName(const std::string_view& name)
{
	std::unique_lock lock(s_async_op_mutex);
	for (auto iter = s_async_ops.begin(); iter != s_async_ops.end(); ++iter)
	{
		if (name != iter->second->GetName())
			continue;

		// move the thread out so it doesn't detach itself, then join
		std::unique_ptr<FullscreenUI::ProgressCallback> progress(std::move(iter->second));
		std::thread thread(std::move(iter->first));
		progress->SetCancelled();
		s_async_ops.erase(iter);
		lock.unlock();
		if (thread.joinable())
			thread.join();
		lock.lock();
		break;
	}
}

void FullscreenUI::CancelAsyncOps()
{
	std::unique_lock lock(s_async_op_mutex);
	while (!s_async_ops.empty())
	{
		auto iter = s_async_ops.begin();

		// move the thread out so it doesn't detach itself, then join
		std::unique_ptr<FullscreenUI::ProgressCallback> progress(std::move(iter->second));
		std::thread thread(std::move(iter->first));
		progress->SetCancelled();
		s_async_ops.erase(iter);
		lock.unlock();
		if (thread.joinable())
			thread.join();
		lock.lock();
	}
}

void FullscreenUI::AsyncOpThreadEntryPoint(std::function<void(::ProgressCallback*)> callback, FullscreenUI::ProgressCallback* progress)
{
	Threading::SetNameOfCurrentThread(fmt::format("{} Async Op", progress->GetName()).c_str());

	callback(progress);

	// if we were removed from the list, it means we got cancelled, and the main thread is blocking
	std::unique_lock lock(s_async_op_mutex);
	for (auto iter = s_async_ops.begin(); iter != s_async_ops.end(); ++iter)
	{
		if (iter->second.get() == progress)
		{
			iter->first.detach();
			s_async_ops.erase(iter);
			break;
		}
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

	ImGuiFullscreen::SetTheme(Host::GetBaseBoolSettingValue("UI", "UseLightFullscreenUITheme", false));
	ImGuiFullscreen::UpdateLayoutScale();

	if (!ImGuiManager::AddFullscreenFontsIfMissing() || !ImGuiFullscreen::Initialize("fullscreenui/placeholder.png") || !LoadResources())
	{
		DestroyResources();
		ImGuiFullscreen::Shutdown(true);
		s_tried_to_initialize = true;
		return false;
	}

	s_initialized = true;
	s_hotkey_list_cache = InputManager::GetHotkeyList();
	MTGS::SetRunIdle(true);

	if (VMManager::HasValidVM())
	{
		UpdateGameDetails(VMManager::GetDiscPath(), VMManager::GetDiscSerial(), VMManager::GetTitle(),
			VMManager::GetDiscCRC(), VMManager::GetCurrentCRC());
	}
	else
	{
		// only switch to landing if we weren't e.g. in settings
		if (s_current_main_window == MainWindowType::None)
			SwitchToLanding();
	}

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

void FullscreenUI::CheckForConfigChanges(const Pcsx2Config& old_config)
{
	if (!IsInitialized())
		return;

#ifdef ENABLE_ACHIEVEMENTS
	// If achievements got disabled, we might have the menu open...
	// That means we're going to be reaching achievement state.
	if (old_config.Achievements.Enabled && !EmuConfig.Achievements.Enabled)
	{
		// So, wait just in case.
		MTGS::RunOnGSThread([]() {
			if (s_current_main_window == MainWindowType::Achievements || s_current_main_window == MainWindowType::Leaderboards)
			{
				ReturnToMainWindow();
			}
		});
		MTGS::WaitGS(false, false, false);
	}
#endif
}

void FullscreenUI::OnVMStarted()
{
	if (!IsInitialized())
		return;

	MTGS::RunOnGSThread([]() {
		if (!IsInitialized())
			return;

		s_current_main_window = MainWindowType::None;
		QueueResetFocus();
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
		SwitchToLanding();
	});
}

void FullscreenUI::GameChanged(std::string path, std::string serial, std::string title, u32 disc_crc, u32 crc)
{
	if (!IsInitialized())
		return;

	MTGS::RunOnGSThread(
		[path = std::move(path), serial = std::move(serial), title = std::move(title), disc_crc, crc]() {
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

void FullscreenUI::ToggleTheme()
{
	const bool new_light = !Host::GetBaseBoolSettingValue("UI", "UseLightFullscreenUITheme", false);
	Host::SetBaseBoolSettingValue("UI", "UseLightFullscreenUITheme", new_light);
	Host::CommitBaseSettingChanges();
	ImGuiFullscreen::SetTheme(new_light);
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

	MTGS::RunOnGSThread([]() {
		if (!ImGuiManager::InitializeFullscreenUI() || s_current_main_window != MainWindowType::None)
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

void FullscreenUI::Shutdown(bool clear_state)
{
	if (clear_state)
	{
		CancelAsyncOps();
		CloseSaveStateSelector();
		s_cover_image_map.clear();
		s_game_list_sorted_entries = {};
		s_game_list_directories_cache = {};
		s_game_cheat_unlabelled_count = 0;
		s_enabled_game_cheat_cache = {};
		s_game_cheats_list = {};
		s_enabled_game_patch_cache = {};
		s_game_patch_list = {};
		s_fullscreen_mode_list_cache = {};
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
	DestroyResources();
	ImGuiFullscreen::Shutdown(clear_state);
	s_initialized = false;
	s_tried_to_initialize = false;
}

void FullscreenUI::Render()
{
	if (!s_initialized)
		return;

	for (std::unique_ptr<GSTexture>& tex : s_cleanup_textures)
		g_gs_device->Recycle(tex.release());
	s_cleanup_textures.clear();
	ImGuiFullscreen::UploadAsyncTextures();

	ImGuiFullscreen::BeginLayout();

#ifdef ENABLE_ACHIEVEMENTS
	// Primed achievements must come first, because we don't want the pause screen to be behind them.
	if (EmuConfig.Achievements.PrimedIndicators && s_current_main_window == MainWindowType::None &&
		Achievements::GetPrimedAchievementCount() > 0)
	{
		DrawPrimedAchievementsIcons();
	}
#endif

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
#ifdef ENABLE_ACHIEVEMENTS
		case MainWindowType::Achievements:
			DrawAchievementsWindow();
			break;
		case MainWindowType::Leaderboards:
			DrawLeaderboardsWindow();
			break;
#endif
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
			s_game_settings_interface->Save();
			if (VMManager::HasValidVM())
				Host::RunOnCPUThread([]() { VMManager::ReloadGameSettings(); });
		}
		s_game_settings_changed.store(false, std::memory_order_release);
	}

	ImGuiFullscreen::ResetCloseMenuIfNeeded();
}

void FullscreenUI::InvalidateCoverCache()
{
	if (!IsInitialized())
		return;

	MTGS::RunOnGSThread([]() { s_cover_image_map.clear(); });
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
		g_gs_device->Recycle(tex.release());
}

//////////////////////////////////////////////////////////////////////////
// Utility
//////////////////////////////////////////////////////////////////////////

ImGuiFullscreen::FileSelectorFilters FullscreenUI::GetOpenFileFilters()
{
	return {"*.bin", "*.iso", "*.cue", "*.mdf", "*.chd", "*.cso", "*.gz", "*.elf", "*.irx", "*.gs", "*.gs.xz", "*.gs.zst", "*.dump"};
}

ImGuiFullscreen::FileSelectorFilters FullscreenUI::GetDiscImageFilters()
{
	return {"*.bin", "*.iso", "*.cue", "*.mdf", "*.chd", "*.cso", "*.gz"};
}

void FullscreenUI::DoStartPath(const std::string& path, std::optional<s32> state_index, std::optional<bool> fast_boot)
{
	VMBootParameters params;
	params.filename = path;
	params.state_index = state_index;
	params.fast_boot = fast_boot;

	// switch to nothing, we'll get brought back if init fails
	const MainWindowType prev_window = s_current_main_window;
	s_current_main_window = MainWindowType::None;

	Host::RunOnCPUThread([params = std::move(params), prev_window]() {
		if (VMManager::HasValidVM())
			return;

		if (VMManager::Initialize(std::move(params)))
			VMManager::SetState(VMState::Running);
		else
			s_current_main_window = prev_window;
	});
}

void FullscreenUI::DoStartFile()
{
	auto callback = [](const std::string& path) {
		if (!path.empty())
			DoStartPath(path);

		QueueResetFocus();
		CloseFileSelector();
	};

	OpenFileSelector(ICON_FA_FOLDER_OPEN " Select Disc Image", false, std::move(callback), GetOpenFileFilters());
}

void FullscreenUI::DoStartBIOS()
{
	Host::RunOnCPUThread([]() {
		if (VMManager::HasValidVM())
			return;

		VMBootParameters params;
		if (VMManager::Initialize(std::move(params)))
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
	OpenChoiceDialog(ICON_FA_COMPACT_DISC " Select Disc Drive", false, std::move(options), [](s32, const std::string& path, bool) {
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

		MTGS::ToggleSoftwareRendering();
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

	OpenFileSelector(ICON_FA_COMPACT_DISC " Select Disc Image", false, std::move(callback), GetDiscImageFilters(),
		std::string(Path::GetDirectory(s_current_disc_path)));
}

void FullscreenUI::DoChangeDisc()
{
	DoChangeDiscFromFile();
}

void FullscreenUI::DoRequestExit()
{
	Host::RunOnCPUThread([]() { Host::RequestExit(true); });
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
	BeginFullscreenColumns(nullptr, 0.0f, true);

	if (BeginFullscreenColumnWindow(0.0f, -710.0f, "logo", UIPrimaryDarkColor))
	{
		const float image_size = LayoutScale(380.f);
		ImGui::SetCursorPos(
			ImVec2((ImGui::GetWindowWidth() * 0.5f) - (image_size * 0.5f), (ImGui::GetWindowHeight() * 0.5f) - (image_size * 0.5f)));
		ImGui::Image(s_app_icon_texture->GetNativeHandle(), ImVec2(image_size, image_size));
	}
	EndFullscreenColumnWindow();

	if (BeginFullscreenColumnWindow(-710.0f, 0.0f, "menu", UIBackgroundColor))
	{
		ResetFocusHere();

		BeginMenuButtons(6, 0.5f);

		if (MenuButton(ICON_FA_LIST " Game List", "Launch a game from images scanned from your game directories."))
		{
			SwitchToGameList();
		}

		if (MenuButton(ICON_FA_FOLDER_OPEN " Start File", "Launch a game by selecting a file/disc image."))
		{
			DoStartFile();
		}

		if (MenuButton(ICON_FA_TOOLBOX " Start BIOS", "Start the console without any disc inserted."))
		{
			DoStartBIOS();
		}

		if (MenuButton(ICON_FA_COMPACT_DISC " Start Disc", "Start a game from a disc in your PC's DVD drive."))
		{
			DoStartDisc();
		}

		if (MenuButton(ICON_FA_SLIDERS_H " Settings", "Change settings for the emulator."))
			SwitchToSettings();

		if (MenuButton(ICON_FA_SIGN_OUT_ALT " Exit", "Exits the program."))
		{
			DoRequestExit();
		}

		{
			ImVec2 fullscreen_pos;
			if (FloatingButton(ICON_FA_WINDOW_CLOSE, 0.0f, 0.0f, -1.0f, -1.0f, 1.0f, 0.0f, true, g_large_font, &fullscreen_pos))
				DoRequestExit();

			if (FloatingButton(ICON_FA_EXPAND, fullscreen_pos.x, 0.0f, -1.0f, -1.0f, -1.0f, 0.0f, true, g_large_font, &fullscreen_pos))
				DoToggleFullscreen();

			if (FloatingButton(
					ICON_FA_QUESTION_CIRCLE, fullscreen_pos.x, 0.0f, -1.0f, -1.0f, -1.0f, 0.0f, true, g_large_font, &fullscreen_pos))
				OpenAboutWindow();

			if (FloatingButton(ICON_FA_LIGHTBULB, fullscreen_pos.x, 0.0f, -1.0f, -1.0f, -1.0f, 0.0f, true, g_large_font, &fullscreen_pos))
				ToggleTheme();
		}

		EndMenuButtons();

		const ImVec2 rev_size(g_medium_font->CalcTextSizeA(g_medium_font->FontSize, FLT_MAX, 0.0f, GIT_REV));
		ImGui::SetCursorPos(
			ImVec2(ImGui::GetWindowWidth() - rev_size.x - LayoutScale(20.0f), ImGui::GetWindowHeight() - rev_size.y - LayoutScale(20.0f)));
		ImGui::PushFont(g_medium_font);
		ImGui::Text(GIT_REV);
		ImGui::PopFont();
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
	SettingsInterface* bsi, InputBindingInfo::Type type, const char* section, const char* name, const char* display_name, bool show_type)
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
			case InputBindingInfo::Type::Button:
				title = fmt::format(ICON_FA_DOT_CIRCLE " {}", display_name);
				break;
			case InputBindingInfo::Type::Axis:
			case InputBindingInfo::Type::HalfAxis:
				title = fmt::format(ICON_FA_BULLSEYE " {}", display_name);
				break;
			case InputBindingInfo::Type::Motor:
				title = fmt::format(ICON_FA_BELL " {}", display_name);
				break;
			case InputBindingInfo::Type::Macro:
				title = fmt::format(ICON_FA_PIZZA_SLICE " {}", display_name);
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
	else if (ImGui::IsItemClicked(ImGuiMouseButton_Right) || ImGui::IsNavInputTest(ImGuiNavInput_Input, ImGuiNavReadMode_Pressed))
	{
		bsi->DeleteValue(section, name);
		SetSettingsChanged(bsi);
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

void FullscreenUI::BeginInputBinding(SettingsInterface* bsi, InputBindingInfo::Type type, const std::string_view& section,
	const std::string_view& key, const std::string_view& display_name)
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

	const char* title = ICON_FA_GAMEPAD " Set Input Binding";
	ImGui::SetNextWindowSize(LayoutScale(500.0f, 0.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::OpenPopup(title);

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

	if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs))
	{
		ImGui::TextWrapped("Setting %s binding %s.", s_input_binding_section.c_str(), s_input_binding_display_name.c_str());
		ImGui::TextUnformatted("Push a controller button or axis now.");
		ImGui::NewLine();
		ImGui::Text("Timing out in %.0f seconds...", time_remaining);
		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(4);
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

	ImGui::SetNextWindowSize(LayoutScale(500.0f, 190.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

	bool is_open = true;
	if (ImGui::BeginPopupModal(title, &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		BeginMenuButtons();

		const float end = ImGui::GetCurrentWindow()->WorkRect.GetWidth();
		ImGui::SetNextItemWidth(end);
		s32 dlg_value = static_cast<s32>(value.value_or(default_value));
		if (ImGui::SliderInt("##value", &dlg_value, min_value, max_value, format, ImGuiSliderFlags_NoInput))
		{
			if (IsEditingGameSettings(bsi) && dlg_value == default_value)
				bsi->DeleteValue(section, key);
			else
				bsi->SetIntValue(section, key, dlg_value);

			SetSettingsChanged(bsi);
		}

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(10.0f));
		if (MenuButtonWithoutSummary("OK", true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, g_large_font, ImVec2(0.5f, 0.0f)))
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
	ImFont* font, ImFont* summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<int> value =
		bsi->GetOptionalIntValue(section, key, game_settings ? std::nullopt : std::optional<int>(default_value));
	const std::string value_text(
		value.has_value() ? StringUtil::StdStringFromFormat(format, value.value()) : std::string("Use Global Setting"));

	static bool manual_input = false;

	if (MenuButtonWithValue(title, summary, value_text.c_str(), enabled, height, font, summary_font))
	{
		ImGui::OpenPopup(title);
		manual_input = false;
	}

	ImGui::SetNextWindowSize(LayoutScale(500.0f, 190.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

	bool is_open = true;
	if (ImGui::BeginPopupModal(title, &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
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

			if (ImGui::InputText("##value", str_value, std::size(str_value), ImGuiInputTextFlags_CharsDecimal))
			{
				const s32 new_value = StringUtil::FromChars<s32>(str_value).value_or(dlg_value);
				dlg_value_changed = (dlg_value != new_value);
				dlg_value = new_value;
			}

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(10.0f));
		}
		else
		{
			const ImVec2& padding(ImGui::GetStyle().FramePadding);
			ImVec2 button_pos(ImGui::GetCursorPos());

			// Align value text in middle.
			ImGui::SetCursorPosY(
				button_pos.y + ((LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY) + padding.y * 2.0f) - g_large_font->FontSize) * 0.5f);
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

		if (MenuButtonWithoutSummary("OK", true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, g_large_font, ImVec2(0.5f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(4);
	ImGui::PopFont();
}

#if 0
// Unused as of now
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

	ImGui::SetNextWindowSize(LayoutScale(500.0f, 190.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

	bool is_open = true;
	if (ImGui::BeginPopupModal(title, &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		BeginMenuButtons();

		const float end = ImGui::GetCurrentWindow()->WorkRect.GetWidth();
		ImGui::SetNextItemWidth(end);
		float dlg_value = value.value_or(default_value) * multiplier;
		if (ImGui::SliderFloat("##value", &dlg_value, min_value * multiplier, max_value * multiplier, format, ImGuiSliderFlags_NoInput))
		{
			dlg_value /= multiplier;

			if (IsEditingGameSettings(bsi) && dlg_value == default_value)
				bsi->DeleteValue(section, key);
			else
				bsi->SetFloatValue(section, key, dlg_value);

			SetSettingsChanged(bsi);
		}

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(10.0f));
		if (MenuButtonWithoutSummary("OK", true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, g_large_font, ImVec2(0.5f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(4);
	ImGui::PopFont();
}
#endif

void FullscreenUI::DrawFloatSpinBoxSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* key, float default_value, float min_value, float max_value, float step_value, float multiplier, const char* format,
	bool enabled, float height, ImFont* font, ImFont* summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<float> value =
		bsi->GetOptionalFloatValue(section, key, game_settings ? std::nullopt : std::optional<int>(default_value));
	const std::string value_text(
		value.has_value() ? StringUtil::StdStringFromFormat(format, value.value() * multiplier) : std::string("Use Global Setting"));

	static bool manual_input = false;

	if (MenuButtonWithValue(title, summary, value_text.c_str(), enabled, height, font, summary_font))
	{
		ImGui::OpenPopup(title);
		manual_input = false;
	}

	ImGui::SetNextWindowSize(LayoutScale(500.0f, 190.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushFont(g_large_font);
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

			if (ImGui::InputText("##value", str_value, std::size(str_value), ImGuiInputTextFlags_CharsDecimal))
			{
				const float new_value = StringUtil::FromChars<float>(str_value).value_or(dlg_value);
				dlg_value_changed = (dlg_value != new_value);
				dlg_value = new_value;
			}

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(10.0f));
		}
		else
		{
			const ImVec2& padding(ImGui::GetStyle().FramePadding);
			ImVec2 button_pos(ImGui::GetCursorPos());

			// Align value text in middle.
			ImGui::SetCursorPosY(
				button_pos.y + ((LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY) + padding.y * 2.0f) - g_large_font->FontSize) * 0.5f);
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

		if (MenuButtonWithoutSummary("OK", true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, g_large_font, ImVec2(0.5f, 0.0f)))
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
	float height, ImFont* font, ImFont* summary_font)
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

	static bool manual_input = false;

	if (MenuButtonWithValue(title, summary, value_text.c_str(), enabled, height, font, summary_font))
	{
		ImGui::OpenPopup(title);
		manual_input = false;
	}

	ImGui::SetNextWindowSize(LayoutScale(550.0f, 370.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

	bool is_open = true;
	if (ImGui::BeginPopupModal(title, &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		static constexpr const char* labels[4] = {"Left: ", "Top: ", "Right: ", "Bottom: "};
		const char* keys[4] = {left_key, top_key, right_key, bottom_key};
		int defaults[4] = {default_left, default_top, default_right, default_bottom};
		s32 values[4] = {static_cast<s32>(left_value.value_or(default_left)), static_cast<s32>(top_value.value_or(default_top)),
			static_cast<s32>(right_value.value_or(default_right)), static_cast<s32>(bottom_value.value_or(default_bottom))};

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
								 ((LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY) + padding.y * 2.0f) - g_large_font->FontSize) * 0.5f);
			ImGui::TextUnformatted(labels[i]);
			ImGui::SameLine(midpoint);
			ImGui::SetNextItemWidth(end);
			button_pos.x = ImGui::GetCursorPosX();

			if (manual_input)
			{
				ImGui::SetNextItemWidth(end);
				ImGui::SetCursorPosY(button_pos.y);

				if (ImGui::InputText("##value", str_value, std::size(str_value), ImGuiInputTextFlags_CharsDecimal))
				{
					const s32 new_value = StringUtil::FromChars<s32>(str_value).value_or(dlg_value);
					dlg_value_changed = (dlg_value != new_value);
					dlg_value = new_value;
				}

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

		if (MenuButtonWithoutSummary("OK", true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, g_large_font, ImVec2(0.5f, 0.0f)))
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

void FullscreenUI::DrawStringListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
	const char* key, const char* default_value, SettingInfo::GetOptionsCallback option_callback, bool enabled, float height, ImFont* font,
	ImFont* summary_font)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<std::string> value(
		bsi->GetOptionalStringValue(section, key, game_settings ? std::nullopt : std::optional<const char*>(default_value)));

	if (MenuButtonWithValue(title, summary, value.has_value() ? value->c_str() : "Use Global Setting", enabled, height, font, summary_font))
	{
		std::vector<std::pair<std::string, std::string>> raw_options(option_callback());
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(raw_options.size() + 1);
		if (game_settings)
			cd_options.emplace_back("Use Global Setting", !value.has_value());
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

				Host::RunOnCPUThread(&VMManager::Internal::UpdateEmuFolders);
				s_cover_image_map.clear();

				CloseFileSelector();
			});
	}
}

void FullscreenUI::DrawPathSetting(SettingsInterface* bsi, const char* title, const char* section, const char* key,
	const char* default_value, bool enabled /* = true */, float height /* = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT */,
	ImFont* font /* = g_large_font */, ImFont* summary_font /* = g_medium_font */)
{
	const bool game_settings = IsEditingGameSettings(bsi);
	const std::optional<std::string> value(
		bsi->GetOptionalStringValue(section, key, game_settings ? std::nullopt : std::optional<const char*>(default_value)));

	if (MenuButton(title, value.has_value() ? value->c_str() : "Use Global Setting"))
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

void FullscreenUI::StartAutomaticBinding(u32 port)
{
	// messy because the enumeration has to happen on the input thread
	Host::RunOnCPUThread([port]() {
		std::vector<std::pair<std::string, std::string>> devices(InputManager::EnumerateDevices());
		MTGS::RunOnGSThread([port, devices = std::move(devices)]() {
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
						SettingsInterface* bsi = GetEditingSettingsInterface();
						const bool result = Pad::MapController(*bsi, port, InputManager::GetGenericBindingMapping(name));
						SetSettingsChanged(bsi);


						// and the toast needs to happen on the UI thread.
						MTGS::RunOnGSThread([result, name = std::move(name)]() {
							ShowToast({}, result ? fmt::format("Automatic mapping completed for {}.", name) :
												   fmt::format("Automatic mapping failed for {}.", name));
						});
					});
					CloseChoiceDialog();
				});
		});
	});
}

void FullscreenUI::DrawSettingInfoSetting(SettingsInterface* bsi, const char* section, const char* key, const SettingInfo& si)
{
	std::string title(fmt::format(ICON_FA_COG " {}", si.display_name));
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
				bsi, title.c_str(), si.description, section, key, si.IntegerDefaultValue(), si.options, 0, si.IntegerMinValue(), true);
			break;

		case SettingInfo::Type::Float:
			DrawFloatSpinBoxSetting(bsi, title.c_str(), si.description, section, key, si.FloatDefaultValue(), si.FloatMinValue(),
				si.FloatMaxValue(), si.FloatStepValue(), si.multiplier, si.format, true);
			break;

		case SettingInfo::Type::StringList:
		{
			if (si.get_options)
				DrawStringListSetting(bsi, title.c_str(), si.description, section, key, si.StringDefaultValue(), si.get_options, true);
			else
				DrawStringListSetting(
					bsi, title.c_str(), si.description, section, key, si.StringDefaultValue(), si.options, si.options, 0, true);
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

void FullscreenUI::SwitchToGameSettings(const std::string_view& serial, u32 crc)
{
	s_game_settings_entry.reset();
	s_game_settings_interface = std::make_unique<INISettingsInterface>(VMManager::GetGameSettingsPath(serial, crc));
	s_game_settings_interface->Load();
	PopulatePatchesAndCheatsList(serial, crc);
	s_current_main_window = MainWindowType::Settings;
	s_settings_page = SettingsPage::Summary;
	QueueResetFocus();
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
	GSGetAdaptersAndFullscreenModes(GSConfig.Renderer, &s_graphics_adapter_list_cache, &s_fullscreen_mode_list_cache);
}

void FullscreenUI::PopulateGameListDirectoryCache(SettingsInterface* si)
{
	s_game_list_directories_cache.clear();
	for (std::string& dir : si->GetStringList("GameList", "Paths"))
		s_game_list_directories_cache.emplace_back(std::move(dir), false);
	for (std::string& dir : si->GetStringList("GameList", "RecursivePaths"))
		s_game_list_directories_cache.emplace_back(std::move(dir), true);
}

void FullscreenUI::PopulatePatchesAndCheatsList(const std::string_view& serial, u32 crc)
{
	constexpr auto sort_patches = [](Patch::PatchInfoList& list) {
		std::sort(list.begin(), list.end(),
			[](const Patch::PatchInfo& lhs, const Patch::PatchInfo& rhs) { return lhs.name < rhs.name; });
	};

	s_game_patch_list = Patch::GetPatchInfo(serial, crc, false, nullptr);
	sort_patches(s_game_patch_list);
	s_game_cheats_list = Patch::GetPatchInfo(serial, crc, true, &s_game_cheat_unlabelled_count);
	sort_patches(s_game_cheats_list);

	pxAssert(s_game_settings_interface);
	s_enabled_game_patch_cache =
		s_game_settings_interface->GetStringList(Patch::PATCHES_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
	s_enabled_game_cheat_cache =
		s_game_settings_interface->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
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

	if (BeginFullscreenWindow(
			ImVec2(0.0f, 0.0f), heading_size, "settings_category", ImVec4(UIPrimaryColor.x, UIPrimaryColor.y, UIPrimaryColor.z, bg_alpha)))
	{
		static constexpr float ITEM_WIDTH = 25.0f;

		static constexpr const char* global_icons[] = {ICON_FA_WINDOW_MAXIMIZE, ICON_FA_MICROCHIP, ICON_FA_SLIDERS_H,
			ICON_FA_MAGIC, ICON_FA_HEADPHONES, ICON_FA_SD_CARD, ICON_FA_GAMEPAD, ICON_FA_KEYBOARD, ICON_FA_TROPHY,
			ICON_FA_FOLDER_OPEN, ICON_FA_COGS};
		static constexpr const char* per_game_icons[] = {ICON_FA_PARAGRAPH, ICON_FA_SLIDERS_H, ICON_FA_MICROCHIP,
			ICON_FA_FROWN, ICON_FA_MAGIC, ICON_FA_HEADPHONES, ICON_FA_SD_CARD, ICON_FA_GAMEPAD, ICON_FA_BAN};
		static constexpr SettingsPage global_pages[] = {SettingsPage::Interface, SettingsPage::BIOS,
			SettingsPage::Emulation, SettingsPage::Graphics, SettingsPage::Audio, SettingsPage::MemoryCard,
			SettingsPage::Controller, SettingsPage::Hotkey, SettingsPage::Achievements, SettingsPage::Folders,
			SettingsPage::Advanced};
		static constexpr SettingsPage per_game_pages[] = {SettingsPage::Summary, SettingsPage::Emulation,
			SettingsPage::Patches, SettingsPage::Cheats, SettingsPage::Graphics, SettingsPage::Audio,
			SettingsPage::MemoryCard, SettingsPage::Controller, SettingsPage::GameFixes};
		static constexpr const char* titles[] = {"Summary", "Interface Settings", "BIOS Settings", "Emulation Settings",
			"Graphics Settings", "Audio Settings", "Memory Card Settings", "Controller Settings", "Hotkey Settings",
			"Achievements Settings", "Folder Settings", "Advanced Settings", "Patches", "Cheats", "Game Fixes"};

		SettingsInterface* bsi = GetEditingSettingsInterface();
		const bool game_settings = IsEditingGameSettings(bsi);

		const u32 count = game_settings ? (ShouldShowAdvancedSettings(bsi) ? std::size(per_game_pages) : (std::size(per_game_pages) - 1)) :
										  std::size(global_pages);
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

			case SettingsPage::BIOS:
				DrawBIOSSettingsPage();
				break;

			case SettingsPage::Emulation:
				DrawEmulationSettingsPage();
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
				DrawAchievementsSettingsPage(lock);
				break;

			case SettingsPage::Folders:
				DrawFoldersSettingsPage();
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
}

void FullscreenUI::DrawSummarySettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading("Details");

	if (s_game_settings_entry)
	{
		if (MenuButton(ICON_FA_WINDOW_MAXIMIZE " Title", s_game_settings_entry->title.c_str(), true))
			CopyTextToClipboard("Game title copied to clipboard.", s_game_settings_entry->title);
		if (MenuButton(ICON_FA_PAGER " Serial", s_game_settings_entry->serial.c_str(), true))
			CopyTextToClipboard("Game serial copied to clipboard.", s_game_settings_entry->serial);
		if (MenuButton(ICON_FA_CODE " CRC", fmt::format("{:08X}", s_game_settings_entry->crc).c_str(), true))
			CopyTextToClipboard("Game CRC copied to clipboard.", fmt::format("{:08X}", s_game_settings_entry->crc));
		if (MenuButton(ICON_FA_LIST " Type", GameList::EntryTypeToString(s_game_settings_entry->type), true))
			CopyTextToClipboard("Game type copied to clipboard.", GameList::EntryTypeToString(s_game_settings_entry->type));
		if (MenuButton(ICON_FA_BOX " Region", GameList::RegionToString(s_game_settings_entry->region), true))
			CopyTextToClipboard("Game region copied to clipboard.", GameList::RegionToString(s_game_settings_entry->region));
		if (MenuButton(ICON_FA_STAR " Compatibility Rating",
				GameList::EntryCompatibilityRatingToString(s_game_settings_entry->compatibility_rating), true))
		{
			CopyTextToClipboard("Game compatibility copied to clipboard.",
				GameList::EntryCompatibilityRatingToString(s_game_settings_entry->compatibility_rating));
		}
		if (MenuButton(ICON_FA_FOLDER_OPEN " Path", s_game_settings_entry->path.c_str(), true))
			CopyTextToClipboard("Game path copied to clipboard.", s_game_settings_entry->path);

		if (s_game_settings_entry->type == GameList::EntryType::ELF)
		{
			const std::string iso_path(bsi->GetStringValue("EmuCore", "DiscPath"));
			if (MenuButton(ICON_FA_COMPACT_DISC " Disc Path", iso_path.empty() ? "No Disc" : iso_path.c_str()))
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

					QueueResetFocus();
					CloseFileSelector();
				};

				OpenFileSelector(ICON_FA_COMPACT_DISC " Select Disc Path", false, std::move(callback), GetDiscImageFilters());
			}
		}
	}
	else
	{
		MenuButton(ICON_FA_BAN " Details unavailable for game not scanned in game list.", "");
	}

	MenuHeading("Options");

	if (MenuButton(ICON_FA_COPY " Copy Settings", "Copies the current global settings to this game."))
		DoCopyGameSettings();
	if (MenuButton(ICON_FA_TRASH " Clear Settings", "Clears all settings set for this game."))
		DoClearGameSettings();

	EndMenuButtons();
}

void FullscreenUI::DrawInterfaceSettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading("Behaviour");

	DrawToggleSetting(bsi, ICON_FA_MAGIC " Inhibit Screensaver",
		"Prevents the screen saver from activating and the host from sleeping while emulation is running.", "EmuCore", "InhibitScreensaver",
		true);
#ifdef WITH_DISCORD_PRESENCE
	DrawToggleSetting(bsi, "Enable Discord Presence", "Shows the game you are currently playing as part of your profile on Discord.", "UI",
		"DiscordPresence", false);
#endif
	DrawToggleSetting(bsi, ICON_FA_PAUSE " Pause On Start", "Pauses the emulator when a game is started.", "UI", "StartPaused", false);
	DrawToggleSetting(bsi, ICON_FA_VIDEO " Pause On Focus Loss",
		"Pauses the emulator when you minimize the window or switch to another application, and unpauses when you switch back.", "UI",
		"PauseOnFocusLoss", false);
	DrawToggleSetting(bsi, ICON_FA_WINDOW_MAXIMIZE " Pause On Menu",
		"Pauses the emulator when you open the quick menu, and unpauses when you close it.", "UI", "PauseOnMenu", true);
	DrawToggleSetting(bsi, ICON_FA_POWER_OFF " Confirm Shutdown",
		"Determines whether a prompt will be displayed to confirm shutting down the emulator/game when the hotkey is pressed.", "UI",
		"ConfirmShutdown", true);
	DrawToggleSetting(bsi, ICON_FA_SAVE " Save State On Shutdown",
		"Automatically saves the emulator state when powering down or exiting. You can then resume directly from where you left off next "
		"time.",
		"EmuCore", "SaveStateOnShutdown", false);
	if (DrawToggleSetting(bsi, ICON_FA_WRENCH " Enable Per-Game Settings",
			"Enables loading ini overlays from gamesettings, or custom settings per-game.", "EmuCore", "EnablePerGameSettings",
			IsEditingGameSettings(bsi)))
	{
		Host::RunOnCPUThread(&VMManager::ReloadGameSettings);
	}
	if (DrawToggleSetting(bsi, ICON_FA_PAINT_BRUSH " Use Light Theme", "Uses a light coloured theme instead of the default dark theme.",
			"UI", "UseLightFullscreenUITheme", false))
	{
		ImGuiFullscreen::SetTheme(bsi->GetBoolValue("UI", "UseLightFullscreenUITheme", false));
	}

	MenuHeading("Game Display");
	DrawToggleSetting(bsi, ICON_FA_TV " Start Fullscreen", "Automatically switches to fullscreen mode when the program is started.", "UI",
		"StartFullscreen", false);
	DrawToggleSetting(bsi, ICON_FA_MOUSE " Double-Click Toggles Fullscreen",
		"Switches between full screen and windowed when the window is double-clicked.", "UI", "DoubleClickTogglesFullscreen", true);
	DrawToggleSetting(bsi, ICON_FA_MOUSE_POINTER " Hide Cursor In Fullscreen",
		"Hides the mouse pointer/cursor when the emulator is in fullscreen mode.", "UI", "HideMouseCursor", false);

	MenuHeading("On-Screen Display");
	DrawIntSpinBoxSetting(bsi, ICON_FA_SEARCH " OSD Scale", "Determines how large the on-screen messages and monitor are.", "EmuCore/GS",
		"OsdScale", 100, 25, 500, 1, "%d%%");
	DrawToggleSetting(bsi, ICON_FA_LIST " Show Messages",
		"Shows on-screen-display messages when events occur such as save states being created/loaded, screenshots being taken, etc.",
		"EmuCore/GS", "OsdShowMessages", true);
	DrawToggleSetting(bsi, ICON_FA_CLOCK " Show Speed",
		"Shows the current emulation speed of the system in the top-right corner of the display as a percentage.", "EmuCore/GS",
		"OsdShowSpeed", false);
	DrawToggleSetting(bsi, ICON_FA_RULER " Show FPS",
		"Shows the number of video frames (or v-syncs) displayed per second by the system in the top-right corner of the display.",
		"EmuCore/GS", "OsdShowFPS", false);
	DrawToggleSetting(bsi, ICON_FA_BATTERY_HALF " Show CPU Usage",
		"Shows the CPU usage based on threads in the top-right corner of the display.", "EmuCore/GS", "OsdShowCPU", false);
	DrawToggleSetting(bsi, ICON_FA_SPINNER " Show GPU Usage", "Shows the host's GPU usage in the top-right corner of the display.",
		"EmuCore/GS", "OsdShowGPU", false);
	DrawToggleSetting(bsi, ICON_FA_RULER_VERTICAL " Show Resolution",
		"Shows the resolution the game is rendering at in the top-right corner of the display.", "EmuCore/GS", "OsdShowResolution", false);
	DrawToggleSetting(bsi, ICON_FA_BARS " Show GS Statistics",
		"Shows statistics about GS (primitives, draw calls) in the top-right corner of the display.", "EmuCore/GS", "OsdShowGSStats",
		false);
	DrawToggleSetting(bsi, ICON_FA_PLAY " Show Status Indicators",
		"Shows indicators when fast forwarding, pausing, and other abnormal states are active.", "EmuCore/GS", "OsdShowIndicators", true);
	DrawToggleSetting(bsi, ICON_FA_SLIDERS_H " Show Settings", "Shows the current configuration in the bottom-right corner of the display.",
		"EmuCore/GS", "OsdShowSettings", false);
	DrawToggleSetting(bsi, ICON_FA_GAMEPAD " Show Inputs",
		"Shows the current controller state of the system in the bottom-left corner of the display.", "EmuCore/GS", "OsdShowInputs", false);
	DrawToggleSetting(bsi, ICON_FA_RULER_HORIZONTAL " Show Frame Times",
		"Shows a visual history of frame times in the upper-left corner of the display.", "EmuCore/GS", "OsdShowFrameTimes", false);
	DrawToggleSetting(bsi, ICON_FA_EXCLAMATION_CIRCLE " Warn About Unsafe Settings",
		"Displays warnings when settings are enabled which may break games.", "EmuCore", "WarnAboutUnsafeSettings", true);

	MenuHeading("Operations");
	if (MenuButton(ICON_FA_FOLDER_MINUS " Reset Settings", "Resets configuration to defaults (excluding controller settings).",
			!IsEditingGameSettings(bsi)))
	{
		DoResetSettings();
	}

	EndMenuButtons();
}

void FullscreenUI::DrawBIOSSettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading("BIOS Configuration");

	DrawFolderSetting(bsi, ICON_FA_FOLDER_OPEN " Change Search Directory", "Folders", "Bios", EmuFolders::Bios);

	const std::string bios_selection(GetEditingSettingsInterface()->GetStringValue("Filenames", "BIOS", ""));
	if (MenuButtonWithValue(ICON_FA_MICROCHIP " BIOS Selection", "Changes the BIOS image used to start future sessions.",
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
		bsi, ICON_FA_LIGHTBULB " Fast Boot", "Skips the intro screen, and bypasses region checks.", "EmuCore", "EnableFastBoot", true);

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
	static constexpr const char* ee_cycle_rate_settings[] = {
		"50% Speed", "60% Speed", "75% Speed", "100% Speed (Default)", "130% Speed", "180% Speed", "300% Speed"};
	static constexpr const char* ee_cycle_skip_settings[] = {
		"Normal (Default)", "Mild Underclock", "Moderate Underclock", "Maximum Underclock"};
	static constexpr const char* affinity_control_settings[] = {
		"Disabled", "EE > VU > GS", "EE > GS > VU", "VU > EE > GS", "VU > GS > EE", "GS > EE > VU", "GS > VU > EE"};
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

	MenuHeading("System Settings");

	DrawIntListSetting(bsi, "EE Cycle Rate", "Underclocks or overclocks the emulated Emotion Engine CPU.", "EmuCore/Speedhacks",
		"EECycleRate", 0, ee_cycle_rate_settings, std::size(ee_cycle_rate_settings), -3);
	DrawIntListSetting(bsi, "EE Cycle Skipping", "Adds a penalty to the Emulated Emotion Engine for executing VU programs.",
		"EmuCore/Speedhacks", "EECycleSkip", 0, ee_cycle_skip_settings, std::size(ee_cycle_skip_settings));
	DrawIntListSetting(bsi, "Affinity Control Mode",
		"Pins emulation threads to CPU cores to potentially improve performance/frame time variance.", "EmuCore/CPU", "AffinityControlMode",
		0, affinity_control_settings, std::size(affinity_control_settings), 0);
	DrawToggleSetting(bsi, "Enable MTVU (Multi-Threaded VU1)", "Uses a second thread for VU1 micro programs. Sizable speed boost.",
		"EmuCore/Speedhacks", "vuThread", false);
	DrawToggleSetting(bsi, "Enable Instant VU1",
		"Reduces timeslicing between VU1 and EE recompilers, effectively running VU1 at an infinite clock speed.", "EmuCore/Speedhacks",
		"vu1Instant", true);
	DrawToggleSetting(bsi, "Enable Cheats", "Enables loading cheats from pnach files.", "EmuCore", "EnableCheats", false);
	DrawToggleSetting(bsi, "Enable Host Filesystem", "Enables access to files from the host: namespace in the virtual machine.", "EmuCore",
		"HostFs", false);

	if (IsEditingGameSettings(bsi))
	{
		DrawToggleSetting(
			bsi, "Enable Fast CDVD", "Fast disc access, less loading times. Not recommended.", "EmuCore/Speedhacks", "fastCDVD", false);
	}

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
		"Use Global Setting", "None", "Normal (Default)", "Extra + Preserve Sign", "Full"};
	static constexpr const char* vu_clamping_mode_settings[] = {
		"Use Global Setting", "None", "Normal (Default)", "Extra", "Extra + Preserve Sign"};
	const char* const* options = (vunum >= 0) ? vu_clamping_mode_settings : ee_clamping_mode_settings;
	const int setting_offset = IsEditingGameSettings(bsi) ? 0 : 1;

	if (MenuButtonWithValue(title, summary, options[index + setting_offset]))
	{
		ImGuiFullscreen::ChoiceDialogOptions cd_options;
		cd_options.reserve(std::size(ee_clamping_mode_settings));
		for (int i = setting_offset; i < static_cast<int>(std::size(ee_clamping_mode_settings)); i++)
			cd_options.emplace_back(options[i], (i == (index + setting_offset)));
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

void FullscreenUI::DrawGraphicsSettingsPage()
{
	static constexpr const char* s_renderer_names[] = {"Automatic (Default)",
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
	static constexpr const char* s_bilinear_present_options[] = {"Off", "Bilinear (Smooth)", "Bilinear (Sharp)"};
	static constexpr const char* s_deinterlacing_options[] = {"Automatic (Default)", "None", "Weave (Top Field First, Sawtooth)",
		"Weave (Bottom Field First, Sawtooth)", "Bob (Top Field First)", "Bob (Bottom Field First)", "Blend (Top Field First, Half FPS)",
		"Blend (Bottom Field First, Half FPS)", "Adaptive (Top Field First)", "Adaptive (Bottom Field First)"};
	static const char* s_resolution_options[] = {
		"Native (PS2)",
		"1.25x Native",
		"1.5x Native",
		"1.75x Native",
		"2x Native (~720p)",
		"2.25x Native",
		"2.5x Native",
		"2.75x Native",
		"3x Native (~1080p)",
		"3.5x Native",
		"4x Native (~1440p/2K)",
		"5x Native (~1620p)",
		"6x Native (~2160p/4K)",
		"7x Native (~2520p)",
		"8x Native (~2880p)",
	};
	static const char* s_resolution_values[] = {
		"1",
		"1.25",
		"1.5",
		"1.75",
		"2",
		"2.25",
		"2.5",
		"2.75",
		"3",
		"3.5",
		"4",
		"5",
		"6",
		"7",
		"8",
	};
	static constexpr const char* s_mipmapping_options[] = {"Automatic (Default)", "Off", "Basic (Generated Mipmaps)", "Full (PS2 Mipmaps)"};
	static constexpr const char* s_bilinear_options[] = {
		"Nearest", "Bilinear (Forced)", "Bilinear (PS2)", "Bilinear (Forced excluding sprite)"};
	static constexpr const char* s_trilinear_options[] = {"Automatic (Default)", "Off (None)", "Trilinear (PS2)", "Trilinear (Forced)"};
	static constexpr const char* s_dithering_options[] = {"Off", "Scaled", "Unscaled (Default)"};
	static constexpr const char* s_blending_options[] = {
		"Minimum", "Basic (Recommended)", "Medium", "High", "Full (Slow)", "Maximum (Very Slow)"};
	static constexpr const char* s_anisotropic_filtering_entries[] = {"Off (Default)", "2x", "4x", "8x", "16x"};
	static constexpr const char* s_anisotropic_filtering_values[] = {"0", "2", "4", "8", "16"};
	static constexpr const char* s_preloading_options[] = {"None", "Partial", "Full (Hash Cache)"};
	static constexpr const char* s_generic_options[] = {"Automatic (Default)", "Force Disabled", "Force Enabled"};
	static constexpr const char* s_hw_download[] = {"Accurate (Recommended)", "Disable Readbacks (Synchronize GS Thread)",
		"Unsynchronized (Non-Deterministic)", "Disabled (Ignore Transfers)"};
	static constexpr const char* s_screenshot_sizes[] = {"Screen Resolution", "Internal Resolution", "Internal Resolution (Uncorrected)"};
	static constexpr const char* s_screenshot_formats[] = {"PNG", "JPEG"};

	SettingsInterface* bsi = GetEditingSettingsInterface();

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
		"Selects the algorithm used to convert the PS2's interlaced output to progressive for display.", "EmuCore/GS", "deinterlace_mode",
		static_cast<int>(GSInterlaceMode::Automatic), s_deinterlacing_options, std::size(s_deinterlacing_options));
	DrawIntListSetting(bsi, "Screenshot Size", "Determines the resolution at which screenshots will be saved.", "EmuCore/GS",
		"ScreenshotSize", static_cast<int>(GSScreenshotSize::WindowResolution), s_screenshot_sizes, std::size(s_screenshot_sizes));
	DrawIntListSetting(bsi, "Screenshot Format", "Selects the format which will be used to save screenshots.", "EmuCore/GS",
		"ScreenshotFormat", static_cast<int>(GSScreenshotFormat::PNG), s_screenshot_formats, std::size(s_screenshot_formats));
	DrawIntRangeSetting(bsi, "Screenshot Quality", "Selects the quality at which screenshots will be compressed.", "EmuCore/GS",
		"ScreenshotQuality", 50, 1, 100, "%d%%");
	DrawIntRangeSetting(bsi, "Vertical Stretch", "Increases or decreases the virtual picture size vertically.", "EmuCore/GS", "StretchY",
		100, 10, 300, "%d%%");
	DrawIntRectSetting(bsi, "Crop", "Crops the image, while respecting aspect ratio.", "EmuCore/GS", "CropLeft", 0, "CropTop", 0,
		"CropRight", 0, "CropBottom", 0, 0, 720, 1, "%dpx");
	DrawToggleSetting(bsi, "Enable Widescreen Patches", "Enables loading widescreen patches from pnach files.", "EmuCore",
		"EnableWideScreenPatches", false);
	DrawToggleSetting(bsi, "Enable No-Interlacing Patches", "Enables loading no-interlacing patches from pnach files.", "EmuCore",
		"EnableNoInterlacingPatches", false);
	DrawIntListSetting(bsi, "Bilinear Upscaling", "Smooths out the image when upscaling the console to the screen.", "EmuCore/GS",
		"linear_present_mode", static_cast<int>(GSPostBilinearMode::BilinearSharp), s_bilinear_present_options,
		std::size(s_bilinear_present_options));
	DrawToggleSetting(bsi, "Integer Upscaling",
		"Adds padding to the display area to ensure that the ratio between pixels on the host to pixels in the console is an integer "
		"number. May result in a sharper image in some 2D games.",
		"EmuCore/GS", "IntegerScaling", false);
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
		DrawStringListSetting(bsi, "Internal Resolution", "Multiplies the render resolution by the specified factor (upscaling).",
			"EmuCore/GS", "upscale_multiplier", "1.000000", s_resolution_options, s_resolution_values, std::size(s_resolution_options));
		DrawIntListSetting(bsi, "Mipmapping", "Determines how mipmaps are used when rendering textures.", "EmuCore/GS", "mipmap_hw",
			static_cast<int>(HWMipmapLevel::Automatic), s_mipmapping_options, std::size(s_mipmapping_options), -1);
		DrawIntListSetting(bsi, "Bilinear Filtering", "Selects where bilinear filtering is utilized when rendering textures.", "EmuCore/GS",
			"filter", static_cast<int>(BiFiltering::PS2), s_bilinear_options, std::size(s_bilinear_options));
		DrawIntListSetting(bsi, "Trilinear Filtering", "Selects where trilinear filtering is utilized when rendering textures.",
			"EmuCore/GS", "TriFilter", static_cast<int>(TriFiltering::Automatic), s_trilinear_options, std::size(s_trilinear_options), -1);
		DrawStringListSetting(bsi, "Anisotropic Filtering", "Selects where anistropic filtering is utilized when rendering textures.",
			"EmuCore/GS", "MaxAnisotropy", "0", s_anisotropic_filtering_entries, s_anisotropic_filtering_values,
			std::size(s_anisotropic_filtering_entries));
		DrawIntListSetting(bsi, "Dithering", "Selects the type of dithering applies when the game requests it.", "EmuCore/GS",
			"dithering_ps2", 2, s_dithering_options, std::size(s_dithering_options));
		DrawIntListSetting(bsi, "Blending Accuracy",
			"Determines the level of accuracy when emulating blend modes not supported by the host graphics API.", "EmuCore/GS",
			"accurate_blending_unit", static_cast<int>(AccBlendLevel::Basic), s_blending_options, std::size(s_blending_options));
		DrawIntListSetting(bsi, "Texture Preloading",
			"Uploads full textures to the GPU on use, rather than only the utilized regions. Can improve performance in some games.",
			"EmuCore/GS", "texture_preloading", static_cast<int>(TexturePreloadingLevel::Off), s_preloading_options,
			std::size(s_preloading_options));
	}
	else
	{
		DrawIntRangeSetting(bsi, "Software Rendering Threads",
			"Number of threads to use in addition to the main GS thread for rasterization.", "EmuCore/GS", "extrathreads", 2, 0, 10);
		DrawToggleSetting(bsi, "Auto Flush (Software)", "Force a primitive flush when a framebuffer is also an input texture.",
			"EmuCore/GS", "autoflush_sw", true);
		DrawToggleSetting(bsi, "Edge AA (AA1)", "Enables emulation of the GS's edge anti-aliasing (AA1).", "EmuCore/GS", "aa1", true);
		DrawToggleSetting(bsi, "Mipmapping", "Enables emulation of the GS's texture mipmapping.", "EmuCore/GS", "mipmap", true);
	}

	if (hw_fixes_visible)
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
			static constexpr const char* s_cpu_sprite_render_level_options[] = {
				"Sprites Only", "Sprites/Triangles", "Blended Sprites/Triangles"};
			static constexpr const char* s_cpu_clut_render_options[] = {"0 (Disabled)", "1 (Normal)", "2 (Aggressive)"};
			static constexpr const char* s_texture_inside_rt_options[] = {"Disabled", "Inside Target", "Merge Targets"};
			static constexpr const char* s_half_pixel_offset_options[] = {
				"Off (Default)", "Normal (Vertex)", "Special (Texture)", "Special (Texture - Aggressive)"};
			static constexpr const char* s_round_sprite_options[] = {"Off (Default)", "Half", "Full"};
			static constexpr const char* s_bilinear_dirty_options[] = {"Automatic (Default)", "Force Bilinear", "Force Nearest"};
			static constexpr const char* s_auto_flush_options[] = {
				"Disabled (Default)", "Enabled (Sprites Only)", "Enabled (All Primitives)"};

			DrawIntListSetting(bsi, "CPU Sprite Render Size", "Uses software renderer to draw texture decompression-like sprites.",
				"EmuCore/GS", "UserHacks_CPUSpriteRenderBW", 0, s_cpu_sprite_render_bw_options, std::size(s_cpu_sprite_render_bw_options));
			DrawIntListSetting(bsi, "CPU Sprite Render Level", "Determines filter level for CPU sprite render.", "EmuCore/GS",
				"UserHacks_CPUSpriteRenderLevel", 0, s_cpu_sprite_render_level_options, std::size(s_cpu_sprite_render_level_options));
			DrawIntListSetting(bsi, "Software CLUT Render", "Uses software renderer to draw texture CLUT points/sprites.", "EmuCore/GS",
				"UserHacks_CPUCLUTRender", 0, s_cpu_clut_render_options, std::size(s_cpu_clut_render_options));
			DrawIntSpinBoxSetting(
				bsi, "Skip Draw Start", "Object range to skip drawing.", "EmuCore/GS", "UserHacks_SkipDraw_Start", 0, 0, 5000, 1);
			DrawIntSpinBoxSetting(
				bsi, "Skip Draw End", "Object range to skip drawing.", "EmuCore/GS", "UserHacks_SkipDraw_End", 0, 0, 5000, 1);
			DrawIntListSetting(bsi, "Auto Flush (Hardware)", "Force a primitive flush when a framebuffer is also an input texture.",
				"EmuCore/GS", "UserHacks_AutoFlushLevel", 0, s_auto_flush_options, std::size(s_auto_flush_options), 0, manual_hw_fixes);
			DrawToggleSetting(bsi, "CPU Framebuffer Conversion", "Convert 4-bit and 8-bit frame buffer on the CPU instead of the GPU.",
				"EmuCore/GS", "UserHacks_CPU_FB_Conversion", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Disable Depth Support", "Disable the support of depth buffer in the texture cache.", "EmuCore/GS",
				"UserHacks_DisableDepthSupport", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Disable Safe Features", "This option disables multiple safe features.", "EmuCore/GS",
				"UserHacks_Disable_Safe_Features", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Disable Render Features", "This option disables game-specific render fixes.", "EmuCore/GS",
				"UserHacks_DisableRenderFixes", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Preload Frame", "Uploads GS data when rendering a new frame to reproduce some effects accurately.",
				"EmuCore/GS", "preload_frame_with_gs_data", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Disable Partial Invalidation",
				"Removes texture cache entries when there is any intersection, rather than only the intersected areas.", "EmuCore/GS",
				"UserHacks_DisablePartialInvalidation", false, manual_hw_fixes);
			DrawIntListSetting(bsi, "Texture Inside Render Target",
				"Allows the texture cache to reuse as an input texture the inner portion of a previous framebuffer.", "EmuCore/GS",
				"UserHacks_TextureInsideRt", 0, s_texture_inside_rt_options, std::size(s_texture_inside_rt_options), 0, manual_hw_fixes);
			DrawToggleSetting(bsi, "Target Partial Invalidation",
				"Allows partial invalidation of render targets, which can fix graphical errors in some games.", "EmuCore/GS",
				"UserHacks_TargetPartialInvalidation", false,
				!GetEffectiveBoolSetting(bsi, "EmuCore/GS", "UserHacks_TextureInsideRt", false));
			DrawToggleSetting(bsi, "Read Targets When Closing",
				"Flushes all targets in the texture cache back to local memory when shutting down.", "EmuCore/GS",
				"UserHacks_ReadTCOnClose", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Estimate Texture Region",
				"Attempts to reduce the texture size when games do not set it themselves (e.g. Snowblind games).", "EmuCore/GS",
				"UserHacks_EstimateTextureRegion", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "GPU Palette Conversion",
				"Applies palettes to textures on the GPU instead of the CPU. Can result in speed improvements in some games.", "EmuCore/GS",
				"paltex", false);

			MenuHeading("Upscaling Fixes");
			DrawIntListSetting(bsi, "Half-Pixel Offset", "Adjusts vertices relative to upscaling.", "EmuCore/GS",
				"UserHacks_HalfPixelOffset", 0, s_half_pixel_offset_options, std::size(s_half_pixel_offset_options));
			DrawIntListSetting(bsi, "Round Sprite", "Adjusts sprite coordinates.", "EmuCore/GS", "UserHacks_round_sprite_offset", 0,
				s_round_sprite_options, std::size(s_round_sprite_options));
			DrawIntListSetting(bsi, "Bilinear Upscale",
				"Can smooth out textures due to be bilinear filtered when upscaling. E.g. Brave sun glare.", "EmuCore/GS",
				"UserHacks_BilinearHack", static_cast<int>(GSBilinearDirtyMode::Automatic),
				s_bilinear_dirty_options, std::size(s_bilinear_dirty_options));
			DrawIntSpinBoxSetting(
				bsi, "TC Offset X", "Adjusts target texture offsets.", "EmuCore/GS", "UserHacks_TCOffsetX", 0, -4096, 4096, 1);
			DrawIntSpinBoxSetting(
				bsi, "TC Offset Y", "Adjusts target texture offsets.", "EmuCore/GS", "UserHacks_TCOffsetY", 0, -4096, 4096, 1);
			DrawToggleSetting(bsi, "Align Sprite", "Fixes issues with upscaling (vertical lines) in some games.", "EmuCore/GS",
				"UserHacks_align_sprite_X", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Merge Sprite", "Replaces multiple post-processing sprites with a larger single sprite.", "EmuCore/GS",
				"UserHacks_merge_pp_sprite", false, manual_hw_fixes);
			DrawToggleSetting(bsi, "Wild Arms Hack",
				"Lowers the GS precision to avoid gaps between pixels when upscaling. Fixes the text on Wild Arms games.", "EmuCore/GS",
				"UserHacks_WildHack", false, manual_hw_fixes);			
			DrawToggleSetting(bsi, "Unscaled Palette Texture Draws", "Can fix some broken effects which rely on pixel perfect precision.",
				"EmuCore/GS", "UserHacks_NativePaletteDraw", false, manual_hw_fixes);
		}
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
		DrawFolderSetting(bsi, "Replacements Directory", "Folders", "Textures", EmuFolders::Textures);

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
		static constexpr const char* s_cas_options[] = {
			"None (Default)", "Sharpen Only (Internal Resolution)", "Sharpen and Resize (Display Resolution)"};
		const bool cas_active = (GetEffectiveIntSetting(bsi, "EmuCore/GS", "CASMode", 0) != static_cast<int>(GSCASMode::Disabled));

		DrawToggleSetting(bsi, "FXAA", "Enables FXAA post-processing shader.", "EmuCore/GS", "fxaa", false);
		DrawIntListSetting(bsi, "Contrast Adaptive Sharpening", "Enables FidelityFX Contrast Adaptive Sharpening.", "EmuCore/GS", "CASMode",
			static_cast<int>(GSCASMode::Disabled), s_cas_options, std::size(s_cas_options));
		DrawIntSpinBoxSetting(bsi, "CAS Sharpness", "Determines the intensity the sharpening effect in CAS post-processing.", "EmuCore/GS",
			"CASSharpness", 50, 0, 100, 1, "%d%%", cas_active);
	}

	MenuHeading("Filters");
	{
		const bool shadeboost_active = GetEffectiveBoolSetting(bsi, "EmuCore/GS", "ShadeBoost", false);

		DrawToggleSetting(bsi, "Shade Boost", "Enables brightness/contrast/saturation adjustment.", "EmuCore/GS", "ShadeBoost", false);
		DrawIntRangeSetting(bsi, "Shade Boost Brightness", "Adjusts brightness. 50 is normal.", "EmuCore/GS", "ShadeBoost_Brightness", 50,
			1, 100, "%d", shadeboost_active);
		DrawIntRangeSetting(bsi, "Shade Boost Contrast", "Adjusts contrast. 50 is normal.", "EmuCore/GS", "ShadeBoost_Contrast", 50, 1, 100,
			"%d", shadeboost_active);
		DrawIntRangeSetting(bsi, "Shade Boost Saturation", "Adjusts saturation. 50 is normal.", "EmuCore/GS", "ShadeBoost_Saturation", 50,
			1, 100, "%d", shadeboost_active);

		static constexpr const char* s_tv_shaders[] = {
			"None (Default)", "Scanline Filter", "Diagonal Filter", "Triangular Filter", "Wave Filter", "Lottes CRT", "4xRGSS", "NxAGSS"};
		DrawIntListSetting(
			bsi, "TV Shaders", "Selects post-processing TV shader.", "EmuCore/GS", "TVShader", 0, s_tv_shaders, std::size(s_tv_shaders));
	}

	static constexpr const char* s_gsdump_compression[] = {"Uncompressed", "LZMA (xz)", "Zstandard (zst)"};

	MenuHeading("Advanced");
	DrawToggleSetting(bsi, "Skip Presenting Duplicate Frames",
		"Skips displaying frames that don't change in 25/30fps games. Can improve speed but increase input lag/make frame pacing worse.",
		"EmuCore/GS", "SkipDuplicateFrames", false);
	DrawToggleSetting(bsi, "Disable Threaded Presentation",
		"Presents frames on a worker thread, instead of on the GS thread. Can improve frame times on some systems, at the cost of "
		"potentially worse frame pacing.",
		"EmuCore/GS", "DisableThreadedPresentation", false);
	if (hw_fixes_visible)
	{
		DrawIntListSetting(bsi, "Hardware Download Mode", "Changes synchronization behavior for GS downloads.", "EmuCore/GS",
			"HWDownloadMode", static_cast<int>(GSHardwareDownloadMode::Enabled), s_hw_download, std::size(s_hw_download));
	}
	DrawIntListSetting(bsi, "Allow Exclusive Fullscreen",
		"Overrides the driver's heuristics for enabling exclusive fullscreen, or direct flip/scanout.", "EmuCore/GS",
		"ExclusiveFullscreenControl", -1, s_generic_options, std::size(s_generic_options), -1,
		(renderer == GSRendererType::Auto || renderer == GSRendererType::VK));
	DrawIntListSetting(bsi, "Override Texture Barriers", "Forces texture barrier functionality to the specified value.", "EmuCore/GS",
		"OverrideTextureBarriers", -1, s_generic_options, std::size(s_generic_options), -1);
	DrawIntListSetting(bsi, "GS Dump Compression", "Sets the compression algorithm for GS dumps.", "EmuCore/GS", "GSDumpCompression",
		static_cast<int>(GSDumpCompressionMethod::LZMA), s_gsdump_compression, std::size(s_gsdump_compression));
	DrawToggleSetting(bsi, "Disable Framebuffer Fetch", "Prevents the usage of framebuffer fetch when supported by host GPU.", "EmuCore/GS",
		"DisableFramebufferFetch", false);
	DrawToggleSetting(bsi, "Disable Dual-Source Blending", "Prevents the usage of dual-source blending when supported by host GPU.",
		"EmuCore/GS", "DisableDualSourceBlend", false);
	DrawToggleSetting(bsi, "Disable Shader Cache", "Prevents the loading and saving of shaders/pipelines to disk.", "EmuCore/GS",
		"DisableShaderCache", false);
	DrawToggleSetting(bsi, "Disable Vertex Shader Expand", "Falls back to the CPU for expanding sprites/lines.", "EmuCore/GS",
		"DisableVertexShaderExpand", false);

	EndMenuButtons();
}

void FullscreenUI::DrawAudioSettingsPage()
{
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
	DrawIntRangeSetting(bsi, ICON_FA_VOLUME_UP " Output Volume", "Applies a global volume modifier to all sound produced by the game.",
		"SPU2/Mixing", "FinalVolume", 100, 0, 200, "%d%%");

	MenuHeading("Mixing Settings");
	DrawIntListSetting(bsi, ICON_FA_RULER " Synchronization Mode", "Changes when SPU samples are generated relative to system emulation.",
		"SPU2/Output", "SynchMode", static_cast<int>(Pcsx2Config::SPU2Options::SynchronizationMode::TimeStretch), synchronization_modes,
		std::size(synchronization_modes));
	DrawIntListSetting(bsi, ICON_FA_PLUS " Expansion Mode", "Determines how the stereo output is transformed to greater speaker counts.",
		"SPU2/Output", "SpeakerConfiguration", 0, expansion_modes, std::size(expansion_modes));

	MenuHeading("Output Settings");
	DrawStringListSetting(bsi, ICON_FA_PLAY_CIRCLE " Output Module", "Determines which API is used to play back audio samples on the host.",
		"SPU2/Output", "OutputModule", default_output_module, output_entries, output_values, std::size(output_entries));
	DrawIntRangeSetting(bsi, ICON_FA_CLOCK " Latency", "Sets the average output latency when using the cubeb backend.", "SPU2/Output",
		"Latency", 100, 15, 200, "%d ms (avg)");

	MenuHeading("Timestretch Settings");
	DrawIntRangeSetting(bsi, ICON_FA_RULER_HORIZONTAL " Sequence Length",
		"Affects how the timestretcher operates when not running at 100% speed.", "Soundtouch", "SequenceLengthMS", 30, 20, 100, "%d ms");
	DrawIntRangeSetting(bsi, ICON_FA_WINDOW_MAXIMIZE " Seekwindow Size",
		"Affects how the timestretcher operates when not running at 100% speed.", "Soundtouch", "SeekWindowMS", 20, 10, 30, "%d ms");
	DrawIntRangeSetting(bsi, ICON_FA_RECEIPT " Overlap", "Affects how the timestretcher operates when not running at 100% speed.",
		"Soundtouch", "OverlapMS", 20, 5, 15, "%d ms");

	EndMenuButtons();
}

void FullscreenUI::DrawMemoryCardSettingsPage()
{
	BeginMenuButtons();

	SettingsInterface* bsi = GetEditingSettingsInterface();

	MenuHeading("Settings and Operations");
	if (MenuButton(ICON_FA_PLUS " Create Memory Card", "Creates a new memory card file or folder."))
		ImGui::OpenPopup("Create Memory Card");
	DrawCreateMemoryCardWindow();

	DrawFolderSetting(bsi, ICON_FA_FOLDER_OPEN " Memory Card Directory", "Folders", "MemoryCards", EmuFolders::MemoryCards);
	DrawToggleSetting(bsi, ICON_FA_SEARCH " Folder Memory Card Filter",
		"Simulates a larger memory card by filtering saves only to the current game.", "EmuCore", "McdFolderAutoManage", true);
	DrawToggleSetting(bsi, ICON_FA_MAGIC " Auto Eject When Loading",
		"Automatically ejects Memory Cards when they differ after loading a state.", "EmuCore", "McdEnableEjection", true);

	for (u32 port = 0; port < NUM_MEMORY_CARD_PORTS; port++)
	{
		const std::string title(fmt::format("Console Port {}", port + 1));
		MenuHeading(title.c_str());

		std::string enable_key(fmt::format("Slot{}_Enable", port + 1));
		std::string file_key(fmt::format("Slot{}_Filename", port + 1));

		DrawToggleSetting(bsi, fmt::format(ICON_FA_SD_CARD " Card Enabled##card_enabled_{}", port).c_str(),
			"If not set, this card will be considered unplugged.", "MemoryCards", enable_key.c_str(), true);

		const bool enabled = GetEffectiveBoolSetting(bsi, "MemoryCards", enable_key.c_str(), true);

		std::optional<std::string> value(bsi->GetOptionalStringValue("MemoryCards", file_key.c_str(),
			IsEditingGameSettings(bsi) ? std::nullopt : std::optional<const char*>(FileMcd_GetDefaultName(port).c_str())));

		if (MenuButtonWithValue(fmt::format(ICON_FA_FILE " Card Name##card_name_{}", port).c_str(),
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
				fmt::format(ICON_FA_EJECT " Eject Card##eject_card_{}", port).c_str(), "Resets the card name for this slot.", enabled))
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
						   "cards, or folder Memory Cards for best compatibility.");
		ImGui::NewLine();

		static char memcard_name[256] = {};
		ImGui::Text("Card Name: ");
		ImGui::InputText("##name", memcard_name, sizeof(memcard_name));

		ImGui::NewLine();

		static constexpr std::tuple<const char*, MemoryCardType, MemoryCardFileType> memcard_types[] = {
			{"8 MB [Most Compatible]", MemoryCardType::File, MemoryCardFileType::PS2_8MB},
			{"16 MB", MemoryCardType::File, MemoryCardFileType::PS2_16MB},
			{"32 MB", MemoryCardType::File, MemoryCardFileType::PS2_32MB},
			{"64 MB", MemoryCardType::File, MemoryCardFileType::PS2_64MB},
			{"Folder [Recommended]", MemoryCardType::Folder, MemoryCardFileType::PS2_8MB},
			{"128 KB [PS1]", MemoryCardType::File, MemoryCardFileType::PS1},
		};

		static int memcard_type = 0;
		for (int i = 0; i < static_cast<int>(std::size(memcard_types)); i++)
			ImGui::RadioButton(std::get<0>(memcard_types[i]), &memcard_type, i);

		ImGui::NewLine();

		BeginMenuButtons();

		const bool create_enabled = (std::strlen(memcard_name) > 0);

		if (ActiveButton(ICON_FA_FOLDER_OPEN " Create", false, create_enabled) && std::strlen(memcard_name) > 0)
		{
			const std::string real_card_name = fmt::format("{}.{}", memcard_name,
				(std::get<2>(memcard_types[memcard_type]) == MemoryCardFileType::PS1 ? "mcr" : "ps2"));
			if (!Path::IsValidFileName(real_card_name, false))
			{
				ShowToast(std::string(), fmt::format("Memory card name '{}' is not valid.", real_card_name));
			}
			else if (!FileMcd_GetCardInfo(real_card_name).has_value())
			{
				const auto& [type_title, type, file_type] = memcard_types[memcard_type];
				if (FileMcd_CreateNewCard(real_card_name, type, file_type))
				{
					ShowToast(std::string(), fmt::format("Memory Card '{}' created.", real_card_name));

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

		if (ActiveButton(ICON_FA_TIMES " Cancel", false))
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

	Pad::CopyConfiguration(dsi, *ssi, true, true, false);
	USB::CopyConfiguration(dsi, *ssi, true, true);
	SetSettingsChanged(dsi);

	ShowToast(std::string(), "Per-game controller configuration initialized with global settings.");
}

void FullscreenUI::ResetControllerSettings()
{
	SettingsInterface* dsi = GetEditingSettingsInterface();

	Pad::SetDefaultControllerConfig(*dsi);
	Pad::SetDefaultHotkeyConfig(*dsi);
	USB::SetDefaultConfiguration(dsi);
	ShowToast(std::string(), "Controller settings reset to default.");
}

void FullscreenUI::DoLoadInputProfile()
{
	std::vector<std::string> profiles = Pad::GetInputProfileNames();
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
		ICON_FA_FOLDER_OPEN " Load Profile", false, std::move(coptions), [](s32 index, const std::string& title, bool checked) {
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
			Pad::CopyConfiguration(dsi, ssi, true, true, IsEditingGameSettings(dsi));
			USB::CopyConfiguration(dsi, ssi, true, true);
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
	Pad::CopyConfiguration(&dsi, *ssi, true, true, IsEditingGameSettings(ssi));
	USB::CopyConfiguration(&dsi, *ssi, true, true);
	if (dsi.Save())
		ShowToast(std::string(), fmt::format("Input profile '{}' saved.", name));
	else
		ShowToast(std::string(), fmt::format("Failed to save input profile '{}'.", name));
}

void FullscreenUI::DoSaveInputProfile()
{
	std::vector<std::string> profiles = Pad::GetInputProfileNames();

	ImGuiFullscreen::ChoiceDialogOptions coptions;
	coptions.reserve(profiles.size() + 1);
	coptions.emplace_back("Create New...", false);
	for (std::string& name : profiles)
		coptions.emplace_back(std::move(name), false);
	OpenChoiceDialog(ICON_FA_SAVE " Save Profile", false, std::move(coptions), [](s32 index, const std::string& title, bool checked) {
		if (index < 0)
			return;

		if (index > 0)
		{
			DoSaveInputProfile(title);
			CloseChoiceDialog();
			return;
		}

		CloseChoiceDialog();

		OpenInputStringDialog(ICON_FA_SAVE " Save Profile", "Enter the name of the input profile you wish to create.", std::string(),
			ICON_FA_FOLDER_PLUS " Create", [](std::string title) {
				if (!title.empty())
					DoSaveInputProfile(title);
			});
	});
}

void FullscreenUI::DoResetSettings()
{
	OpenConfirmMessageDialog(ICON_FA_FOLDER_MINUS " Reset Settings",
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
		if (DrawToggleSetting(bsi, ICON_FA_COG " Per-Game Configuration", "Uses game-specific settings for controllers for this game.",
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
		if (MenuButton(ICON_FA_COPY " Copy Global Settings", "Copies the global controller configuration to this game."))
			CopyGlobalControllerSettingsToGame();
	}
	else
	{
		if (MenuButton(ICON_FA_FOLDER_MINUS " Reset Settings", "Resets all configuration to defaults (including bindings)."))
			ResetControllerSettings();
	}

	if (MenuButton(ICON_FA_FOLDER_OPEN " Load Profile", "Replaces these settings with a previously saved input profile."))
		DoLoadInputProfile();
	if (MenuButton(ICON_FA_SAVE " Save Profile", "Stores the current settings to an input profile."))
		DoSaveInputProfile();

	MenuHeading("Input Sources");

#ifdef SDL_BUILD
	DrawToggleSetting(bsi, ICON_FA_COG " Enable SDL Input Source", "The SDL input source supports most controllers.", "InputSources", "SDL",
		true, true, false);
	DrawToggleSetting(bsi, ICON_FA_WIFI " SDL DualShock 4 / DualSense Enhanced Mode",
		"Provides vibration and LED control support over Bluetooth.", "InputSources", "SDLControllerEnhancedMode", false,
		bsi->GetBoolValue("InputSources", "SDL", true), false);
#endif
#if defined(SDL_BUILD) && defined(_WIN32)
	DrawToggleSetting(bsi, ICON_FA_COG " SDL Raw Input", "Allow SDL to use raw access to input devices.", "InputSources", "SDLRawInput",
		false, bsi->GetBoolValue("InputSources", "SDL", true), false);
#endif
#ifdef _WIN32
	DrawToggleSetting(bsi, ICON_FA_COG " Enable XInput Input Source",
		"The XInput source provides support for XBox 360/XBox One/XBox Series controllers.", "InputSources", "XInput", false, true, false);
#endif

	MenuHeading("Multitap");
	DrawToggleSetting(bsi, ICON_FA_PLUS_SQUARE " Enable Console Port 1 Multitap",
		"Enables an additional three controller slots. Not supported in all games.", "Pad", "MultitapPort1", false, true, false);
	DrawToggleSetting(bsi, ICON_FA_PLUS_SQUARE " Enable Console Port 2 Multitap",
		"Enables an additional three controller slots. Not supported in all games.", "Pad", "MultitapPort2", false, true, false);

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
		MenuHeading(
			(mtap_enabled[mtap_port] ? fmt::format(ICON_FA_PLUG " Controller Port {}{}", mtap_port + 1, mtap_slot_names[mtap_slot]) :
									   fmt::format(ICON_FA_PLUG " Controller Port {}", mtap_port + 1))
				.c_str());

		const char* section = sections[global_slot];
		const Pad::ControllerInfo* ci = Pad::GetConfigControllerType(*bsi, section, global_slot);
		if (MenuButton(ICON_FA_GAMEPAD " Controller Type", ci ? ci->display_name : "Unknown"))
		{
			const std::vector<std::pair<const char*, const char*>> raw_options = Pad::GetControllerTypeNames();
			ImGuiFullscreen::ChoiceDialogOptions options;
			options.reserve(raw_options.size());
			for (auto& it : raw_options)
				options.emplace_back(it.second, (ci && ci->name == it.first));
			OpenChoiceDialog(fmt::format("Port {} Controller Type", global_slot + 1).c_str(), false, std::move(options),
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

		if (MenuButton(ICON_FA_MAGIC " Automatic Mapping", "Attempts to map the selected port to a chosen controller."))
			StartAutomaticBinding(global_slot);

		for (const InputBindingInfo& bi : ci->bindings)
			DrawInputBindingButton(bsi, bi.bind_type, section, bi.name, Host::TranslateToCString("Pad", bi.display_name), true);

		MenuHeading((mtap_enabled[mtap_port] ?
						 fmt::format(ICON_FA_MICROCHIP " Controller Port {}{} Macros", mtap_port + 1, mtap_slot_names[mtap_slot]) :
						 fmt::format(ICON_FA_MICROCHIP " Controller Port {} Macros", mtap_port + 1))
						.c_str());

		static bool macro_button_expanded[Pad::NUM_CONTROLLER_PORTS][Pad::NUM_MACRO_BUTTONS_PER_CONTROLLER] = {};

		for (u32 macro_index = 0; macro_index < Pad::NUM_MACRO_BUTTONS_PER_CONTROLLER; macro_index++)
		{
			bool& expanded = macro_button_expanded[global_slot][macro_index];
			expanded ^= MenuHeadingButton(fmt::format(ICON_FA_MICROCHIP " Macro Button {}", macro_index + 1).c_str(),
				macro_button_expanded[global_slot][macro_index] ? ICON_FA_CHEVRON_UP : ICON_FA_CHEVRON_DOWN);
			if (!expanded)
				continue;

			DrawInputBindingButton(bsi, InputBindingInfo::Type::Macro, section, fmt::format("Macro{}", macro_index + 1).c_str(), "Trigger");

			std::string binds_string(bsi->GetStringValue(section, fmt::format("Macro{}Binds", macro_index + 1).c_str()));
			if (MenuButton(fmt::format(ICON_FA_KEYBOARD " Buttons", macro_index + 1).c_str(),
					binds_string.empty() ? "No Buttons Selected" : binds_string.c_str()))
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
					options.emplace_back(Host::TranslateToCString("Pad", bi.display_name), std::any_of(buttons_split.begin(), buttons_split.end(),
																							   [bi](const std::string_view& it) { return (it == bi.name); }));
				}

				OpenChoiceDialog(fmt::format("Select Macro {} Binds", macro_index + 1).c_str(), true, std::move(options),
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
					});
			}

			const std::string freq_key(fmt::format("Macro{}Frequency", macro_index + 1));
			s32 frequency = bsi->GetIntValue(section, freq_key.c_str(), 0);
			const std::string freq_summary((frequency == 0) ? std::string("Macro will not auto-toggle.") :
															  fmt::format("Macro will toggle every {} frames.", frequency));
			if (MenuButton(ICON_FA_LIGHTBULB " Frequency", freq_summary.c_str()))
				ImGui::OpenPopup(freq_key.c_str());

			const std::string pressure_key(fmt::format("Macro{}Pressure", macro_index + 1));
			DrawFloatSpinBoxSetting(bsi, ICON_FA_ARROW_DOWN " Pressure", "Determines how much pressure is simulated when macro is active.",
				section, pressure_key.c_str(), 1.0f, 0.01f, 1.0f, 0.01f, 100.0f, "%.0f%%");

			const std::string deadzone_key(fmt::format("Macro{}Deadzone", macro_index + 1));
			DrawFloatSpinBoxSetting(bsi, ICON_FA_ARROW_DOWN " Pressure", "Determines the pressure required to activate the macro.",
				section, deadzone_key.c_str(), 0.0f, 0.00f, 1.0f, 0.01f, 100.0f, "%.0f%%");

			ImGui::SetNextWindowSize(LayoutScale(500.0f, 180.0f));
			ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

			ImGui::PushFont(g_large_font);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
				LayoutScale(ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING));

			if (ImGui::BeginPopupModal(
					freq_key.c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
			{
				ImGui::SetNextItemWidth(LayoutScale(450.0f));
				if (ImGui::SliderInt("##value", &frequency, 0, 60, "Toggle every %d frames", ImGuiSliderFlags_NoInput))
				{
					if (frequency == 0)
						bsi->DeleteValue(section, freq_key.c_str());
					else
						bsi->SetIntValue(section, freq_key.c_str(), frequency);
				}

				BeginMenuButtons();
				if (MenuButton("OK", nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
					ImGui::CloseCurrentPopup();
				EndMenuButtons();

				ImGui::EndPopup();
			}

			ImGui::PopStyleVar(4);
			ImGui::PopFont();
		}

		if (!ci->settings.empty())
		{
			MenuHeading((mtap_enabled[mtap_port] ?
							 fmt::format(ICON_FA_SLIDERS_H " Controller Port {}{} Settings", mtap_port + 1, mtap_slot_names[mtap_slot]) :
							 fmt::format(ICON_FA_SLIDERS_H " Controller Port {} Settings", mtap_port + 1))
							.c_str());

			for (const SettingInfo& si : ci->settings)
				DrawSettingInfoSetting(bsi, section, si.name, si);
		}

		ImGui::PopID();
	}

	for (u32 port = 0; port < USB::NUM_PORTS; port++)
	{
		ImGui::PushID(port);
		MenuHeading(fmt::format(ICON_FA_PLUG " USB Port {}", port + 1).c_str());

		const std::string type(USB::GetConfigDevice(*bsi, port));
		if (MenuButton(ICON_FA_GAMEPAD " Device Type", USB::GetDeviceName(type)))
		{
			const std::vector<std::pair<const char*, const char*>> raw_options = USB::GetDeviceTypes();
			ImGuiFullscreen::ChoiceDialogOptions options;
			options.reserve(raw_options.size());
			for (auto& it : raw_options)
			{
				options.emplace_back(it.second, type == it.first);
			}
			OpenChoiceDialog(fmt::format("Port {} Device", port + 1).c_str(), false, std::move(options),
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
			if (MenuButton(ICON_FA_COG " Device Subtype", subtype_name))
			{
				ImGuiFullscreen::ChoiceDialogOptions options;
				options.reserve(subtypes.size());
				for (u32 i = 0; i < subtypes.size(); i++)
					options.emplace_back(subtypes[i], i == subtype);

				OpenChoiceDialog(fmt::format("Port {} Subtype", port + 1).c_str(), false, std::move(options),
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
			MenuHeading(fmt::format(ICON_FA_KEYBOARD " {} Bindings", USB::GetDeviceName(type)).c_str());

			if (MenuButton(ICON_FA_FOLDER_MINUS " Clear Bindings", "Clears all bindings for this USB controller."))
			{
				USB::ClearPortBindings(*bsi, port);
				SetSettingsChanged(bsi);
			}

			const std::string section(USB::GetConfigSection(port));
			for (const InputBindingInfo& bi : bindings)
			{
				DrawInputBindingButton(bsi, bi.bind_type, section.c_str(), USB::GetConfigSubKey(type, bi.name).c_str(),
					Host::TranslateToCString("USB", bi.display_name));
			}
		}

		const std::span<const SettingInfo> settings(USB::GetDeviceSettings(type, subtype));
		if (!settings.empty())
		{
			MenuHeading(fmt::format(ICON_FA_SLIDERS_H " {} Settings", USB::GetDeviceName(type)).c_str());

			const std::string section(USB::GetConfigSection(port));
			for (const SettingInfo& si : settings)
				DrawSettingInfoSetting(bsi, section.c_str(), USB::GetConfigSubKey(type, si.name).c_str(), si);
		}
		ImGui::PopID();
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
		if (!last_category || std::strcmp(hotkey->category, last_category->category) != 0)
		{
			MenuHeading(hotkey->category);
			last_category = hotkey;
		}

		DrawInputBindingButton(bsi, InputBindingInfo::Type::Button, "Hotkeys", hotkey->name, hotkey->display_name, false);
	}

	EndMenuButtons();
}

void FullscreenUI::DrawFoldersSettingsPage()
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	BeginMenuButtons();

	MenuHeading("Data Save Locations");

	DrawFolderSetting(bsi, ICON_FA_CALENDAR " Cache Directory", "Folders", "Cache", EmuFolders::Cache);
	DrawFolderSetting(bsi, ICON_FA_FOLDER " Covers Directory", "Folders", "Covers", EmuFolders::Covers);
	DrawFolderSetting(bsi, ICON_FA_CAMERA " Snapshots Directory", "Folders", "Snapshots", EmuFolders::Snapshots);
	DrawFolderSetting(bsi, ICON_FA_DOWNLOAD " Save States Directory", "Folders", "Savestates", EmuFolders::Savestates);
	DrawFolderSetting(bsi, ICON_FA_WRENCH " Game Settings Directory", "Folders", "GameSettings", EmuFolders::GameSettings);
	DrawFolderSetting(bsi, ICON_FA_GAMEPAD " Input Profile Directory", "Folders", "InputProfiles", EmuFolders::InputProfiles);
	DrawFolderSetting(bsi, ICON_FA_FROWN " Cheats Directory", "Folders", "Cheats", EmuFolders::Cheats);
	DrawFolderSetting(bsi, ICON_FA_MAGIC " Patches Directory", "Folders", "Patches", EmuFolders::Patches);
	DrawFolderSetting(bsi, ICON_FA_SLIDERS_H "Texture Replacements Directory", "Folders", "Textures", EmuFolders::Textures);
	DrawFolderSetting(bsi, ICON_FA_SLIDERS_H "Video Dumping Directory", "Folders", "Videos", EmuFolders::Videos);

	EndMenuButtons();
}

void FullscreenUI::DrawAdvancedSettingsPage()
{
	static constexpr const char* ee_rounding_mode_settings[] = {"Nearest", "Negative", "Positive", "Chop/Zero (Default)"};

	SettingsInterface* bsi = GetEditingSettingsInterface();

	const bool show_advanced_settings = ShouldShowAdvancedSettings(bsi);

	BeginMenuButtons();

	if (!IsEditingGameSettings(bsi))
	{
		DrawToggleSetting(bsi, "Show Advanced Settings",
			"Changing these options may cause games to become non-functional. Modify at your own risk, the PCSX2 team will not provide "
			"support for configurations with these settings changed.",
			"UI", "ShowAdvancedSettings", false);
	}

	MenuHeading("Logging");

	DrawToggleSetting(bsi, "System Console", "Writes log messages to the system console (console window/standard output).", "Logging",
		"EnableSystemConsole", false);
	DrawToggleSetting(bsi, "File Logging", "Writes log messages to emulog.txt.", "Logging", "EnableFileLogging", false);
	DrawToggleSetting(bsi, "Verbose Logging", "Writes dev log messages to log sinks.", "Logging", "EnableVerbose", false, !IsDevBuild);

	if (show_advanced_settings)
	{
		DrawToggleSetting(bsi, "Log Timestamps", "Writes timestamps alongside log messages.", "Logging", "EnableTimestamps", true);
		DrawToggleSetting(
			bsi, "EE Console", "Writes debug messages from the game's EE code to the console.", "Logging", "EnableEEConsole", true);
		DrawToggleSetting(
			bsi, "IOP Console", "Writes debug messages from the game's IOP code to the console.", "Logging", "EnableIOPConsole", true);
		DrawToggleSetting(bsi, "CDVD Verbose Reads", "Logs disc reads from games.", "EmuCore", "CdvdVerboseReads", false);
	}

	if (show_advanced_settings)
	{
		MenuHeading("Emotion Engine");

		DrawIntListSetting(bsi, "Rounding Mode##ee_rounding_mode",
			"Determines how the results of floating-point operations are rounded. Some games need specific settings.", "EmuCore/CPU",
			"FPU.Roundmode", 3, ee_rounding_mode_settings, std::size(ee_rounding_mode_settings));
		DrawClampingModeSetting(bsi, "Clamping Mode##ee_clamping_mode",
			"Determines how out-of-range floating point numbers are handled. Some games need specific settings.", -1);

		DrawToggleSetting(bsi, "Enable EE Recompiler",
			"Performs just-in-time binary translation of 64-bit MIPS-IV machine code to native code.", "EmuCore/CPU/Recompiler", "EnableEE",
			true);
		DrawToggleSetting(
			bsi, "Enable EE Cache", "Enables simulation of the EE's cache. Slow.", "EmuCore/CPU/Recompiler", "EnableEECache", false);
		DrawToggleSetting(bsi, "Enable INTC Spin Detection", "Huge speedup for some games, with almost no compatibility side effects.",
			"EmuCore/Speedhacks", "IntcStat", true);
		DrawToggleSetting(bsi, "Enable Wait Loop Detection", "Moderate speedup for some games, with no known side effects.",
			"EmuCore/Speedhacks", "WaitLoop", true);
		DrawToggleSetting(bsi, "Enable Fast Memory Access", "Uses backpatching to avoid register flushing on every memory access.",
			"EmuCore/CPU/Recompiler", "EnableFastmem", true);

		MenuHeading("Vector Units");
		DrawIntListSetting(bsi, "VU0 Rounding Mode##vu_rounding_mode",
			"Determines how the results of floating-point operations are rounded. Some games need specific settings.", "EmuCore/CPU",
			"VU0.Roundmode", 3, ee_rounding_mode_settings, std::size(ee_rounding_mode_settings));
		DrawClampingModeSetting(bsi, "VU0 Clamping Mode##vu_clamping_mode",
			"Determines how out-of-range floating point numbers are handled. Some games need specific settings.", 0);
		DrawIntListSetting(bsi, "VU1 Rounding Mode##vu_rounding_mode",
			"Determines how the results of floating-point operations are rounded. Some games need specific settings.", "EmuCore/CPU",
			"VU1.Roundmode", 3, ee_rounding_mode_settings, std::size(ee_rounding_mode_settings));
		DrawClampingModeSetting(bsi, "VU1 Clamping Mode##vu_clamping_mode",
			"Determines how out-of-range floating point numbers are handled. Some games need specific settings.", 1);
		DrawToggleSetting(bsi, "Enable VU0 Recompiler (Micro Mode)",
			"New Vector Unit recompiler with much improved compatibility. Recommended.", "EmuCore/CPU/Recompiler", "EnableVU0", true);
		DrawToggleSetting(bsi, "Enable VU1 Recompiler", "New Vector Unit recompiler with much improved compatibility. Recommended.",
			"EmuCore/CPU/Recompiler", "EnableVU1", true);
		DrawToggleSetting(bsi, "Enable VU Flag Optimization", "Good speedup and high compatibility, may cause graphical errors.",
			"EmuCore/Speedhacks", "vuFlagHack", true);

		MenuHeading("I/O Processor");
		DrawToggleSetting(bsi, "Enable IOP Recompiler",
			"Performs just-in-time binary translation of 32-bit MIPS-I machine code to native code.", "EmuCore/CPU/Recompiler", "EnableIOP",
			true);

		MenuHeading("Graphics");
		DrawToggleSetting(
			bsi, "Use Debug Device", "Enables API-level validation of graphics commands", "EmuCore/GS", "UseDebugDevice", false);
	}

	EndMenuButtons();
}

void FullscreenUI::DrawPatchesOrCheatsSettingsPage(bool cheats)
{
	SettingsInterface* bsi = GetEditingSettingsInterface();

	const Patch::PatchInfoList& patch_list = cheats ? s_game_cheats_list : s_game_patch_list;
	std::vector<std::string>& enable_list = cheats ? s_enabled_game_cheat_cache : s_enabled_game_patch_cache;
	const char* section = cheats ? Patch::CHEATS_CONFIG_SECTION : Patch::PATCHES_CONFIG_SECTION;
	const bool master_enable = cheats ? GetEffectiveBoolSetting(bsi, "EmuCore", "EnableCheats", false) : true;

	BeginMenuButtons();

	if (cheats)
	{
		MenuHeading("Settings");
		DrawToggleSetting(
			bsi, "Enable Cheats", "Enables loading cheats from pnach files.", "EmuCore", "EnableCheats", false);

		if (patch_list.empty())
		{
			ActiveButton("No cheats are available for this game.", false, false,
				ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		}
		else
		{
			MenuHeading("Cheat Codes");
		}
	}
	else
	{
		if (patch_list.empty())
		{
			ActiveButton("No patches are available for this game.", false, false,
				ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		}
		else
		{
			MenuHeading("Game Patches");
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
		ActiveButton(
			master_enable ?
				fmt::format("{} unlabelled patch codes will automatically activate.", s_game_cheat_unlabelled_count)
					.c_str() :
				fmt::format("{} unlabelled patch codes found but not enabled.", s_game_cheat_unlabelled_count).c_str(),
			false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
	}

	if (!patch_list.empty() || (cheats && s_game_cheat_unlabelled_count > 0))
	{
		ActiveButton(
			cheats ?
				"Activating cheats can cause unpredictable behavior, crashing, soft-locks, or broken saved games." :
				"Activating game patches can cause unpredictable behavior, crashing, soft-locks, or broken saved "
				"games.",
			false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		ActiveButton("Use patches at your own risk, the PCSX2 team will provide no support for users who have enabled "
					 "game patches.",
			false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
	}

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
	DrawToggleSetting(bsi, "Instant DMA Hack", "Known to affect following games: Fire Pro Wrestling Z (Bad ring graphics).",
		"EmuCore/Gamefixes", "InstantDMAHack", false);
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
	DrawToggleSetting(
		bsi, "Full VU0 Synchronization", "Forces tight VU0 sync on every COP2 instruction.", "EmuCore/Gamefixes", "FullVU0SyncHack", false);
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

static void DrawShadowedText(
	ImDrawList* dl, ImFont* font, const ImVec2& pos, u32 col, const char* text, const char* text_end = nullptr, float wrap_width = 0.0f)
{
	dl->AddText(font, font->FontSize, pos + LayoutScale(1.0f, 1.0f), IM_COL32(0, 0, 0, 100), text, text_end, wrap_width);
	dl->AddText(font, font->FontSize, pos, col, text, text_end, wrap_width);
}

void FullscreenUI::DrawPauseMenu(MainWindowType type)
{
	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	const ImVec2 display_size(ImGui::GetIO().DisplaySize);
	const ImU32 text_color = IM_COL32(UIBackgroundTextColor.x * 255, UIBackgroundTextColor.y * 255, UIBackgroundTextColor.z * 255, 255);
	dl->AddRectFilled(ImVec2(0.0f, 0.0f), display_size, IM_COL32(UIBackgroundColor.x * 255, UIBackgroundColor.y * 255, UIBackgroundColor.z * 255, 200));

	// title info
	{
#ifdef ENABLE_ACHIEVEMENTS
		const bool has_rich_presence = Achievements::IsActive() && !Achievements::GetRichPresenceString().empty();
#else
		const bool has_rich_presence = false;
#endif

		const float image_width = has_rich_presence ? 60.0f : 50.0f;
		const float image_height = has_rich_presence ? 90.0f : 75.0f;
		const std::string_view path_string(Path::GetFileName(s_current_disc_path));
		const ImVec2 title_size(
			g_large_font->CalcTextSizeA(g_large_font->FontSize, std::numeric_limits<float>::max(), -1.0f, s_current_game_title.c_str()));
		const ImVec2 path_size(path_string.empty() ?
								   ImVec2(0.0f, 0.0f) :
								   g_medium_font->CalcTextSizeA(g_medium_font->FontSize, std::numeric_limits<float>::max(), -1.0f,
									   path_string.data(), path_string.data() + path_string.length()));
		const ImVec2 subtitle_size(g_medium_font->CalcTextSizeA(
			g_medium_font->FontSize, std::numeric_limits<float>::max(), -1.0f, s_current_game_subtitle.c_str()));

		ImVec2 title_pos(
			display_size.x - LayoutScale(10.0f + image_width + 20.0f) - title_size.x, display_size.y - LayoutScale(10.0f + image_height));
		ImVec2 path_pos(display_size.x - LayoutScale(10.0f + image_width + 20.0f) - path_size.x,
			title_pos.y + g_large_font->FontSize + LayoutScale(4.0f));
		ImVec2 subtitle_pos(display_size.x - LayoutScale(10.0f + image_width + 20.0f) - subtitle_size.x,
			(path_string.empty() ? title_pos.y : path_pos.y) + g_medium_font->FontSize + LayoutScale(4.0f));

		float rp_height = 0.0f;

		DrawShadowedText(dl, g_large_font, title_pos, text_color, s_current_game_title.c_str());
		if (!path_string.empty())
		{
			DrawShadowedText(
				dl, g_medium_font, path_pos, text_color, path_string.data(), path_string.data() + path_string.length());
		}
		DrawShadowedText(dl, g_medium_font, subtitle_pos, text_color, s_current_game_subtitle.c_str());

#ifdef ENABLE_ACHIEVEMENTS
		if (has_rich_presence)
		{
			const auto lock = Achievements::GetLock();
			const std::string& rp = Achievements::GetRichPresenceString();
			if (!rp.empty())
			{
				const float wrap_width = LayoutScale(350.0f);
				const ImVec2 rp_size = g_medium_font->CalcTextSizeA(
					g_medium_font->FontSize, std::numeric_limits<float>::max(), wrap_width, rp.data(), rp.data() + rp.size());

				// we make the image one line higher, so we only need to compensate when it's multiline RP
				rp_height = rp_size.y - g_medium_font->FontSize;

				const ImVec2 rp_pos(display_size.x - LayoutScale(20.0f + 50.0f + 20.0f) - rp_size.x - rp_height,
					subtitle_pos.y + g_medium_font->FontSize + LayoutScale(4.0f));

				title_pos.y -= rp_height;
				path_pos.y -= rp_height;
				subtitle_pos.y -= rp_height;

				DrawShadowedText(dl, g_medium_font, rp_pos, text_color, rp.data(), rp.data() + rp.size(), wrap_width);
			}
		}
#endif


		GSTexture* const cover = GetCoverForCurrentGame();
		const ImVec2 image_min(
			display_size.x - LayoutScale(10.0f + image_width) - rp_height, display_size.y - LayoutScale(10.0f + image_height) - rp_height);
		const ImVec2 image_max(image_min.x + LayoutScale(image_width) + rp_height, image_min.y + LayoutScale(image_height) + rp_height);
		const ImRect image_rect(CenterImage(
			ImRect(image_min, image_max), ImVec2(static_cast<float>(cover->GetWidth()), static_cast<float>(cover->GetHeight()))));
		dl->AddImage(cover->GetNativeHandle(), image_rect.Min, image_rect.Max);
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

		const ImVec2 time_size(g_large_font->CalcTextSizeA(g_large_font->FontSize, std::numeric_limits<float>::max(), -1.0f, buf));
		const ImVec2 time_pos(display_size.x - LayoutScale(10.0f) - time_size.x, LayoutScale(10.0f));
		DrawShadowedText(dl, g_large_font, time_pos, text_color, buf);

		if (!s_current_disc_serial.empty())
		{
			const std::time_t cached_played_time = GameList::GetCachedPlayedTimeForSerial(s_current_disc_serial);
			const std::time_t session_time = static_cast<std::time_t>(VMManager::GetSessionPlayedTime());
			const std::string played_time_str(GameList::FormatTimespan(cached_played_time + session_time, true));
			const std::string session_time_str(GameList::FormatTimespan(session_time, true));

			std::snprintf(buf, std::size(buf), "This Session: %s", session_time_str.c_str());
			const ImVec2 session_size(g_medium_font->CalcTextSizeA(g_medium_font->FontSize, std::numeric_limits<float>::max(), -1.0f, buf));
			const ImVec2 session_pos(
				display_size.x - LayoutScale(10.0f) - session_size.x, time_pos.y + g_large_font->FontSize + LayoutScale(4.0f));
			DrawShadowedText(dl, g_medium_font, session_pos, text_color, buf);

			std::snprintf(buf, std::size(buf), "All Time: %s", played_time_str.c_str());
			const ImVec2 total_size(g_medium_font->CalcTextSizeA(g_medium_font->FontSize, std::numeric_limits<float>::max(), -1.0f, buf));
			const ImVec2 total_pos(
				display_size.x - LayoutScale(10.0f) - total_size.x, session_pos.y + g_medium_font->FontSize + LayoutScale(4.0f));
			DrawShadowedText(dl, g_medium_font, total_pos, text_color, buf);
		}
	}

	const ImVec2 window_size(LayoutScale(500.0f, LAYOUT_SCREEN_HEIGHT));
	const ImVec2 window_pos(0.0f, display_size.y - window_size.y);

	if (BeginFullscreenWindow(
			window_pos, window_size, "pause_menu", ImVec4(0.0f, 0.0f, 0.0f, 0.0f), 0.0f, 10.0f, ImGuiWindowFlags_NoBackground))
	{
		static constexpr u32 submenu_item_count[] = {
			11, // None
			4, // Exit
#ifdef ENABLE_ACHIEVEMENTS
			3, // Achievements
#endif
		};

		const bool just_focused = ResetFocusHere();
		BeginMenuButtons(submenu_item_count[static_cast<u32>(s_current_pause_submenu)], 1.0f, ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING,
			ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

		switch (s_current_pause_submenu)
		{
			case PauseSubMenu::None:
			{
				// NOTE: Menu close must come first, because otherwise VM destruction options will race.
				const bool can_load_or_save_state = s_current_disc_crc != 0;

				if (ActiveButton(ICON_FA_PLAY " Resume Game", false) || WantsToCloseMenu())
					ClosePauseMenu();

				if (ActiveButton(ICON_FA_FAST_FORWARD " Toggle Frame Limit", false))
				{
					ClosePauseMenu();
					DoToggleFrameLimit();
				}

				if (ActiveButton(ICON_FA_UNDO " Load State", false, can_load_or_save_state))
				{
					if (OpenSaveStateSelector(true))
						s_current_main_window = MainWindowType::None;
				}

				if (ActiveButton(ICON_FA_DOWNLOAD " Save State", false, can_load_or_save_state))
				{
					if (OpenSaveStateSelector(false))
						s_current_main_window = MainWindowType::None;
				}

				if (ActiveButton(ICON_FA_WRENCH " Game Properties", false, can_load_or_save_state))
				{
					SwitchToGameSettings();
				}

#ifdef ENABLE_ACHIEVEMENTS
				if (ActiveButton(ICON_FA_TROPHY " Achievements", false,
						Achievements::HasActiveGame() && Achievements::SafeHasAchievementsOrLeaderboards()))
				{
					const auto lock = Achievements::GetLock();

					// skip second menu and go straight to cheevos if there's no lbs
					if (Achievements::GetLeaderboardCount() == 0)
						SwitchToAchievementsWindow();
					else
						OpenPauseSubMenu(PauseSubMenu::Achievements);
				}
#else
				ActiveButton(ICON_FA_TROPHY " Achievements", false, false);
#endif

				if (ActiveButton(ICON_FA_CAMERA " Save Screenshot", false))
				{
					GSQueueSnapshot(std::string());
					ClosePauseMenu();
				}

				if (ActiveButton(GSConfig.UseHardwareRenderer() ? (ICON_FA_PAINT_BRUSH " Switch To Software Renderer") :
																  (ICON_FA_PAINT_BRUSH " Switch To Hardware Renderer"),
						false))
				{
					ClosePauseMenu();
					DoToggleSoftwareRenderer();
				}

				if (ActiveButton(ICON_FA_COMPACT_DISC " Change Disc", false))
				{
					s_current_main_window = MainWindowType::None;
					DoChangeDisc();
				}

				if (ActiveButton(ICON_FA_SLIDERS_H " Settings", false))
					SwitchToSettings();

				if (ActiveButton(ICON_FA_POWER_OFF " Close Game", false))
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
					ImGui::SetFocusID(ImGui::GetID(ICON_FA_POWER_OFF " Exit Without Saving"), ImGui::GetCurrentWindow());

				if (ActiveButton(ICON_FA_BACKWARD " Back To Pause Menu", false))
				{
					OpenPauseSubMenu(PauseSubMenu::None);
				}

				if (ActiveButton(ICON_FA_SYNC " Reset System", false))
				{
					ClosePauseMenu();
					DoReset();
				}

				if (ActiveButton(ICON_FA_SAVE " Exit And Save State", false))
					DoShutdown(true);

				if (ActiveButton(ICON_FA_POWER_OFF " Exit Without Saving", false))
					DoShutdown(false);
			}
			break;

#ifdef ENABLE_ACHIEVEMENTS
			case PauseSubMenu::Achievements:
			{
				if (ActiveButton(ICON_FA_BACKWARD "  Back To Pause Menu", false))
					OpenPauseSubMenu(PauseSubMenu::None);

				if (ActiveButton(ICON_FA_TROPHY "  Achievements", false))
					SwitchToAchievementsWindow();

				if (ActiveButton(ICON_FA_STOPWATCH "  Leaderboards", false))
					SwitchToLeaderboardsWindow();
			}
			break;
#endif
		}

		EndMenuButtons();

		EndFullscreenWindow();
	}

#ifdef ENABLE_ACHIEVEMENTS
	// Primed achievements must come first, because we don't want the pause screen to be behind them.
	if (Achievements::GetPrimedAchievementCount() > 0)
		DrawPrimedAchievementsList();
#endif
}

void FullscreenUI::InitializePlaceholderSaveStateListEntry(SaveStateListEntry* li, s32 slot)
{
	li->title = (slot == 0) ? std::string("Quick Save Slot") : fmt::format("Save Slot {0}##game_slot_{0}", slot);
	li->summary = "No save present in this slot.";
	li->path = {};
	li->timestamp = 0;
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
		InitializePlaceholderSaveStateListEntry(li, slot);
		return false;
	}

	li->title = (slot == 0) ? std::string("Quick Save Slot") : fmt::format("Save Slot {0}##game_slot_{0}", slot);
	li->summary = fmt::format("Saved {}", TimeToPrintableString(sd.ModificationTime));
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
			ReleaseTexture(li->preview_texture);
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

	ShowToast({}, "No save states found.", 5.0f);
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

	ShowToast({}, "No save states found.", 5.0f);
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
	if (s_current_main_window != MainWindowType::GameList)
		ReturnToMainWindow();
}

void FullscreenUI::DrawSaveStateSelector(bool is_loading)
{
	ImGuiIO& io = ImGui::GetIO();

	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(io.DisplaySize);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);

	const char* window_title = is_loading ? "Load State" : "Save State";
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
			CloseSaveStateSelector();
		return;
	}

	ImVec2 heading_size =
		ImVec2(io.DisplaySize.x, LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY + LAYOUT_MENU_BUTTON_Y_PADDING * 2.0f + 2.0f));

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ModAlpha(UIPrimaryColor, 0.9f));

	if (ImGui::BeginChild("state_titlebar", heading_size, false, ImGuiWindowFlags_NavFlattened))
	{
		BeginNavBar();
		if (NavButton(ICON_FA_BACKWARD, true, true))
			CloseSaveStateSelector();

		NavTitle(is_loading ? "Load State" : "Save State");
		EndNavBar();
		ImGui::EndChild();
	}

	ImGui::PopStyleColor();
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ModAlpha(UIBackgroundColor, 0.9f));
	ImGui::SetCursorPos(ImVec2(0.0f, heading_size.y));

	bool close_handled = false;
	if (s_save_state_selector_open &&
		ImGui::BeginChild("state_list", ImVec2(io.DisplaySize.x, io.DisplaySize.y - heading_size.y), false, ImGuiWindowFlags_NavFlattened))
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
		const float item_height = (style.FramePadding.y * 2.0f) + image_height + title_spacing + g_large_font->FontSize + summary_spacing +
								  g_medium_font->FontSize;
		const ImVec2 item_size(item_width, item_height);
		const u32 grid_count_x = std::floor(ImGui::GetWindowWidth() / item_width_with_spacing);
		const float start_x =
			(static_cast<float>(ImGui::GetWindowWidth()) - (item_width_with_spacing * static_cast<float>(grid_count_x))) * 0.5f;

		u32 grid_x = 0;
		ImGui::SetCursorPos(ImVec2(start_x, 0.0f));
		for (u32 i = 0; i < s_save_state_selector_slots.size();)
		{
			if (i == 0)
				ResetFocusHere();

			if (static_cast<s32>(i) == s_save_state_selector_submenu_index)
			{
				SaveStateListEntry& entry = s_save_state_selector_slots[i];

				// can't use a choice dialog here, because we're already in a modal...
				ImGuiFullscreen::PushResetLayout();
				ImGui::PushFont(g_large_font);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING, LAYOUT_MENU_BUTTON_Y_PADDING));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
				ImGui::PushStyleColor(ImGuiCol_Text, UIPrimaryTextColor);
				ImGui::PushStyleColor(ImGuiCol_TitleBg, UIPrimaryDarkColor);
				ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UIPrimaryColor);
				ImGui::PushStyleColor(ImGuiCol_PopupBg, MulAlpha(UIBackgroundColor, 0.95f));

				const float width = LayoutScale(600.0f);
				const float title_height =
					g_large_font->FontSize + ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetStyle().WindowPadding.y * 2.0f;
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

					if (ActiveButton(is_loading ? ICON_FA_FOLDER_OPEN " Load State" : ICON_FA_FOLDER_OPEN " Save State", false, true,
							LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
					{
						if (is_loading)
							DoLoadState(std::move(entry.path));
						else
							Host::RunOnCPUThread([slot = entry.slot]() { VMManager::SaveStateToSlot(slot); });

						CloseSaveStateSelector();
						closed = true;
					}

					if (ActiveButton(ICON_FA_FOLDER_MINUS " Delete Save", false, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
					{
						if (!FileSystem::FileExists(entry.path.c_str()))
						{
							ShowToast({}, fmt::format("{} does not exist.", ImGuiFullscreen::RemoveHash(entry.title)));
							is_open = true;
						}
						else if (FileSystem::DeleteFilePath(entry.path.c_str()))
						{
							ShowToast({}, fmt::format("{} deleted.", ImGuiFullscreen::RemoveHash(entry.title)));
							if (s_save_state_selector_loading)
								s_save_state_selector_slots.erase(s_save_state_selector_slots.begin() + i);
							else
								InitializePlaceholderSaveStateListEntry(&entry, entry.slot);

							// Close if this was the last state.
							if (s_save_state_selector_slots.empty())
							{
								CloseSaveStateSelector();
								closed = true;
							}
							else
							{
								is_open = false;
							}
						}
						else
						{
							ShowToast({}, fmt::format("Failed to delete {}.", ImGuiFullscreen::RemoveHash(entry.title)));
							is_open = false;
						}
					}

					if (ActiveButton(ICON_FA_WINDOW_CLOSE " Close Menu", false, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
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
						QueueResetFocus();
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

					ImGui::RenderFrame(bb.Min, bb.Max, col, true, 0.0f);

					ImGui::PopStyleColor();
				}

				bb.Min += style.FramePadding;
				bb.Max -= style.FramePadding;

				const GSTexture* const screenshot = entry.preview_texture ? entry.preview_texture.get() : GetPlaceholderTexture().get();
				const ImRect image_rect(CenterImage(ImRect(bb.Min, bb.Min + image_size),
					ImVec2(static_cast<float>(screenshot->GetWidth()), static_cast<float>(screenshot->GetHeight()))));

				ImGui::GetWindowDrawList()->AddImage(screenshot->GetNativeHandle(), image_rect.Min, image_rect.Max, ImVec2(0.0f, 0.0f),
					ImVec2(1.0f, 1.0f), IM_COL32(255, 255, 255, 255));

				const ImVec2 title_pos(bb.Min.x, bb.Min.y + image_height + title_spacing);
				const ImRect title_bb(title_pos, ImVec2(bb.Max.x, title_pos.y + g_large_font->FontSize));
				ImGui::PushFont(g_large_font);
				ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, entry.title.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
				ImGui::PopFont();

				if (!entry.summary.empty())
				{
					const ImVec2 summary_pos(bb.Min.x, title_pos.y + g_large_font->FontSize + summary_spacing);
					const ImRect summary_bb(summary_pos, ImVec2(bb.Max.x, summary_pos.y + g_medium_font->FontSize));
					ImGui::PushFont(g_medium_font);
					ImGui::RenderTextClipped(
						summary_bb.Min, summary_bb.Max, entry.summary.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
					ImGui::PopFont();
				}

				if (pressed)
				{
					if (is_loading)
					{
						DoLoadState(entry.path);
						CloseSaveStateSelector();
						break;
					}
					else
					{
						Host::RunOnCPUThread([slot = entry.slot]() { VMManager::SaveStateToSlot(slot); });
						CloseSaveStateSelector();
						break;
					}
				}

				if (hovered &&
					(ImGui::IsItemClicked(ImGuiMouseButton_Right) || ImGui::IsNavInputTest(ImGuiNavInput_Input, ImGuiNavReadMode_Pressed)))
				{
					s_save_state_selector_submenu_index = static_cast<s32>(i);
				}
			}

			grid_x++;
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

			i++;
		}

		EndMenuButtons();
		ImGui::EndChild();
	}

	ImGui::PopStyleColor();

	ImGui::EndPopup();
	ImGui::PopStyleVar(5);

	if (!close_handled && WantsToCloseMenu())
		CloseSaveStateSelector();
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
	ImGui::OpenPopup("Load Resume State");

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));

	bool is_open = true;
	if (ImGui::BeginPopupModal("Load Resume State", &is_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
	{
		const SaveStateListEntry& entry = s_save_state_selector_slots.front();
		ImGui::TextWrapped("A resume save state created at %s was found.\n\nDo you want to load this save and continue?",
			TimeToPrintableString(entry.timestamp).c_str());

		const GSTexture* image = entry.preview_texture ? entry.preview_texture.get() : GetPlaceholderTexture().get();
		const float image_height = LayoutScale(250.0f);
		const float image_width = image_height * (static_cast<float>(image->GetWidth()) / static_cast<float>(image->GetHeight()));
		const ImVec2 pos(ImGui::GetCursorScreenPos() +
						 ImVec2((ImGui::GetCurrentWindow()->WorkRect.GetWidth() - image_width) * 0.5f, LayoutScale(20.0f)));
		const ImRect image_bb(pos, pos + ImVec2(image_width, image_height));
		ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(entry.preview_texture ? entry.preview_texture->GetNativeHandle() :
																							  GetPlaceholderTexture()->GetNativeHandle()),
			image_bb.Min, image_bb.Max);

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + image_height + LayoutScale(40.0f));

		BeginMenuButtons();

		if (ActiveButton(ICON_FA_PLAY " Load State", false))
		{
			DoStartPath(s_save_state_selector_game_path, -1);
			is_open = false;
		}

		if (ActiveButton(ICON_FA_LIGHTBULB " Clean Boot", false))
		{
			DoStartPath(s_save_state_selector_game_path);
			is_open = false;
		}

		if (ActiveButton(ICON_FA_FOLDER_MINUS " Delete State", false))
		{
			if (FileSystem::DeleteFilePath(entry.path.c_str()))
			{
				DoStartPath(s_save_state_selector_game_path);
				is_open = false;
			}
			else
			{
				ShowToast(std::string(), "Failed to delete save state.");
			}
		}

		if (ActiveButton(ICON_FA_WINDOW_CLOSE " Cancel", false))
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
			if (VMManager::Initialize(std::move(params)))
				VMManager::SetState(VMState::Running);
		}
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
						return reverse ? (lhs->crc >= rhs->crc) : (lhs->crc < rhs->crc);
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
			const int res = StringUtil::Strcasecmp(lhs->title.c_str(), rhs->title.c_str());
			return reverse ? (res > 0) : (res < 0);
		});
}

void FullscreenUI::DrawGameListWindow()
{
	auto game_list_lock = GameList::GetLock();
	PopulateGameListEntryList();

	ImGuiIO& io = ImGui::GetIO();
	ImVec2 heading_size =
		ImVec2(io.DisplaySize.x, LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY + LAYOUT_MENU_BUTTON_Y_PADDING * 2.0f + 2.0f));

	const float bg_alpha = VMManager::HasValidVM() ? 0.90f : 1.0f;

	if (BeginFullscreenWindow(ImVec2(0.0f, 0.0f), heading_size, "gamelist_view", MulAlpha(UIPrimaryColor, bg_alpha)))
	{
		static constexpr float ITEM_WIDTH = 25.0f;
		static constexpr const char* icons[] = {ICON_FA_BORDER_ALL, ICON_FA_LIST, ICON_FA_COG};
		static constexpr const char* titles[] = {"Game Grid", "Game List", "Game List Settings"};
		static constexpr u32 count = std::size(titles);

		BeginNavBar();

		if (!ImGui::IsPopupOpen(0u, ImGuiPopupFlags_AnyPopup))
		{
			if (ImGui::IsNavInputTest(ImGuiNavInput_FocusPrev, ImGuiNavReadMode_Pressed))
			{
				s_game_list_page = static_cast<GameListPage>(
					(s_game_list_page == static_cast<GameListPage>(0)) ? (count - 1) : (static_cast<u32>(s_game_list_page) - 1));
			}
			else if (ImGui::IsNavInputTest(ImGuiNavInput_FocusNext, ImGuiNavReadMode_Pressed))
			{
				s_game_list_page = static_cast<GameListPage>((static_cast<u32>(s_game_list_page) + 1) % count);
			}
		}

		if (NavButton(ICON_FA_BACKWARD, true, true))
			ReturnToMainWindow();

		NavTitle(titles[static_cast<u32>(s_game_list_page)]);
		RightAlignNavButtons(count, ITEM_WIDTH, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

		for (u32 i = 0; i < count; i++)
		{
			if (NavButton(
					icons[i], static_cast<GameListPage>(i) == s_game_list_page, true, ITEM_WIDTH, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
			{
				s_game_list_page = static_cast<GameListPage>(i);
			}
		}

		EndNavBar();
	}

	EndFullscreenWindow();

	switch (s_game_list_page)
	{
		case GameListPage::Grid:
			DrawGameGrid(heading_size);
			break;
		case GameListPage::List:
			DrawGameList(heading_size);
			break;
		case GameListPage::Settings:
			DrawGameListSettingsPage(heading_size);
			break;
		default:
			break;
	}
}

void FullscreenUI::DrawGameList(const ImVec2& heading_size)
{
	if (!BeginFullscreenColumns(nullptr, heading_size.y, true))
	{
		EndFullscreenColumns();
		return;
	}

	if (WantsToCloseMenu())
	{
		if (ImGui::IsWindowFocused())
			ReturnToMainWindow();
	}

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

			GSTexture* cover_texture = GetGameListCover(entry);

			summary.clear();
			if (entry->serial.empty())
				fmt::format_to(std::back_inserter(summary), "{} - ", GameList::RegionToString(entry->region));
			else
				fmt::format_to(std::back_inserter(summary), "{} - {} - ", entry->serial, GameList::RegionToString(entry->region));

			const std::string_view filename(Path::GetFileName(entry->path));
			summary.append(filename);

			const ImRect image_rect(CenterImage(ImRect(bb.Min, bb.Min + image_size),
				ImVec2(static_cast<float>(cover_texture->GetWidth()), static_cast<float>(cover_texture->GetHeight()))));

			ImGui::GetWindowDrawList()->AddImage(cover_texture->GetNativeHandle(), image_rect.Min, image_rect.Max, ImVec2(0.0f, 0.0f),
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
				HandleGameListActivate(entry);

			if (hovered)
				selected_entry = entry;

			if (selected_entry &&
				(ImGui::IsItemClicked(ImGuiMouseButton_Right) || ImGui::IsNavInputTest(ImGuiNavInput_Input, ImGuiNavReadMode_Pressed)))
			{
				HandleGameListOptions(selected_entry);
			}
		}

		EndMenuButtons();
	}
	EndFullscreenColumnWindow();

	if (BeginFullscreenColumnWindow(-530.0f, 0.0f, "game_list_info", UIPrimaryDarkColor))
	{
		const GSTexture* cover_texture =
			selected_entry ? GetGameListCover(selected_entry) : GetTextureForGameListEntryType(GameList::EntryType::Count);
		if (cover_texture)
		{
			const ImRect image_rect(CenterImage(LayoutScale(ImVec2(275.0f, 400.0f)),
				ImVec2(static_cast<float>(cover_texture->GetWidth()), static_cast<float>(cover_texture->GetHeight()))));

			ImGui::SetCursorPos(LayoutScale(ImVec2(128.0f, 20.0f)) + image_rect.Min);
			ImGui::Image(selected_entry ? GetGameListCover(selected_entry)->GetNativeHandle() :
										  GetTextureForGameListEntryType(GameList::EntryType::Count)->GetNativeHandle(),
				image_rect.GetSize());
		}

		const float work_width = ImGui::GetCurrentWindow()->WorkRect.GetWidth();
		constexpr float field_margin_y = 10.0f;
		constexpr float start_x = 50.0f;
		float text_y = 440.0f;
		float text_width;

		PushPrimaryColor();
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
				ImGui::Image(GetCachedTextureAsync(flag_texture.c_str())->GetNativeHandle(), LayoutScale(23.0f, 16.0f));
				ImGui::SameLine();
				ImGui::Text(" (%s)", GameList::RegionToString(selected_entry->region));
			}

			// compatibility
			ImGui::TextUnformatted("Compatibility: ");
			ImGui::SameLine();
			if (selected_entry->compatibility_rating != GameDatabaseSchema::Compatibility::Unknown)
			{
				ImGui::Image(s_game_compatibility_textures[static_cast<u32>(selected_entry->compatibility_rating) - 1]->GetNativeHandle(),
					LayoutScale(64.0f, 16.0f));
				ImGui::SameLine();
			}
			ImGui::Text(" (%s)", GameList::EntryCompatibilityRatingToString(selected_entry->compatibility_rating));

			// play time
			ImGui::Text("Time Played: %s", GameList::FormatTimespan(selected_entry->total_played_time).c_str());
			ImGui::Text("Last Played: %s", GameList::FormatTimestamp(selected_entry->last_played_time).c_str());

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
		PopPrimaryColor();
	}
	EndFullscreenColumnWindow();
	EndFullscreenColumns();
}

void FullscreenUI::DrawGameGrid(const ImVec2& heading_size)
{
	ImGuiIO& io = ImGui::GetIO();
	if (!BeginFullscreenWindow(
			ImVec2(0.0f, heading_size.y), ImVec2(io.DisplaySize.x, io.DisplaySize.y - heading_size.y), "game_grid", UIBackgroundColor))
	{
		EndFullscreenWindow();
		return;
	}

	if (WantsToCloseMenu())
	{
		if (ImGui::IsWindowFocused())
			ReturnToMainWindow();
	}

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
	const float item_height = (style.FramePadding.y * 2.0f) + image_height + title_spacing + g_medium_font->FontSize;
	const ImVec2 item_size(item_width, item_height);
	const u32 grid_count_x = std::floor(ImGui::GetWindowWidth() / item_width_with_spacing);
	const float start_x =
		(static_cast<float>(ImGui::GetWindowWidth()) - (item_width_with_spacing * static_cast<float>(grid_count_x))) * 0.5f;

	// TODO: replace with something not heap allocating
	std::string draw_title;

	u32 grid_x = 0;
	ImGui::SetCursorPos(ImVec2(start_x, 0.0f));
	for (const GameList::Entry* entry : s_game_list_sorted_entries)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			continue;

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

				ImGui::RenderFrame(bb.Min, bb.Max, col, true, 0.0f);

				ImGui::PopStyleColor();
			}

			bb.Min += style.FramePadding;
			bb.Max -= style.FramePadding;

			const GSTexture* const cover_texture = GetGameListCover(entry);
			const ImRect image_rect(CenterImage(ImRect(bb.Min, bb.Min + image_size),
				ImVec2(static_cast<float>(cover_texture->GetWidth()), static_cast<float>(cover_texture->GetHeight()))));

			ImGui::GetWindowDrawList()->AddImage(cover_texture->GetNativeHandle(), image_rect.Min, image_rect.Max, ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f), IM_COL32(255, 255, 255, 255));

			const ImRect title_bb(ImVec2(bb.Min.x, bb.Min.y + image_height + title_spacing), bb.Max);
			const std::string_view title(
				std::string_view(entry->title).substr(0, (entry->title.length() > 31) ? 31 : std::string_view::npos));
			draw_title.clear();
			fmt::format_to(std::back_inserter(draw_title), "{}{}", title, (title.length() == entry->title.length()) ? "" : "...");
			ImGui::PushFont(g_medium_font);
			ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, draw_title.c_str(), draw_title.c_str() + draw_title.length(), nullptr,
				ImVec2(0.5f, 0.0f), &title_bb);
			ImGui::PopFont();

			if (pressed)
				HandleGameListActivate(entry);

			if (hovered &&
				(ImGui::IsItemClicked(ImGuiMouseButton_Right) || ImGui::IsNavInputTest(ImGuiNavInput_Input, ImGuiNavReadMode_Pressed)))
			{
				HandleGameListOptions(entry);
			}
		}

		grid_x++;
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
		{ICON_FA_WRENCH " Game Properties", false},
		{ICON_FA_PLAY " Resume Game", false},
		{ICON_FA_UNDO " Load State", false},
		{ICON_FA_COMPACT_DISC " Default Boot", false},
		{ICON_FA_LIGHTBULB " Fast Boot", false},
		{ICON_FA_MAGIC " Slow Boot", false},
		{ICON_FA_FOLDER_MINUS " Reset Play Time", false},
		{ICON_FA_WINDOW_CLOSE " Close Menu", false},
	};

	const bool has_resume_state = VMManager::HasSaveStateInSlot(entry->serial.c_str(), entry->crc, -1);
	OpenChoiceDialog(entry->title.c_str(), false, std::move(options),
		[has_resume_state, entry_path = entry->path, entry_serial = entry->serial](s32 index, const std::string& title, bool checked) {
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
				case 5: // Slow Boot
					DoStartPath(entry_path, std::nullopt, false);
					break;
				case 6: // Reset Play Time
					GameList::ClearPlayedTimeForSerial(entry_serial);
					break;
				default:
					break;
			}

			CloseChoiceDialog();
		});
}

void FullscreenUI::DrawGameListSettingsPage(const ImVec2& heading_size)
{
	const ImGuiIO& io = ImGui::GetIO();
	if (!BeginFullscreenWindow(ImVec2(0.0f, heading_size.y), ImVec2(io.DisplaySize.x, io.DisplaySize.y - heading_size.y), "settings_parent",
			UIBackgroundColor))
	{
		EndFullscreenWindow();
		return;
	}

	if (WantsToCloseMenu())
	{
		if (ImGui::IsWindowFocused())
			ReturnToMainWindow();
	}

	auto lock = Host::GetSettingsLock();
	SettingsInterface* bsi = GetEditingSettingsInterface(false);

	BeginMenuButtons();

	MenuHeading("Search Directories");
	if (MenuButton(ICON_FA_FOLDER_PLUS " Add Search Directory", "Adds a new directory to the game search list."))
	{
		OpenFileSelector(ICON_FA_FOLDER_PLUS " Add Search Directory", true, [](const std::string& dir) {
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
		if (MenuButton(it.first.c_str(), it.second ? "Scanning Subdirectories" : "Not Scanning Subdirectories"))
		{
			ImGuiFullscreen::ChoiceDialogOptions options = {
				{ICON_FA_FOLDER_OPEN " Open in File Browser", false},
				{it.second ? (ICON_FA_FOLDER_MINUS " Disable Subdirectory Scanning") :
							 (ICON_FA_FOLDER_PLUS " Enable Subdirectory Scanning"),
					false},
				{ICON_FA_TIMES " Remove From List", false},
				{ICON_FA_WINDOW_CLOSE " Close Menu", false},
			};

			OpenChoiceDialog(fmt::format(ICON_FA_FOLDER " {}", it.first).c_str(), false, std::move(options),
				[dir = it.first, recursive = it.second](s32 index, const std::string& title, bool checked) {
					if (index < 0)
						return;

					if (index == 0)
					{
						// Open in file browser... todo
						Host::ReportErrorAsync("Error", "Not implemented");
					}
					else if (index == 1)
					{
						// toggle subdirectory scanning
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
						// remove from list
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

	static constexpr const char* view_types[] = {"Game Grid", "Game List"};
	static constexpr const char* sort_types[] = {"Type", "Serial", "Title", "File Title", "CRC", "Time Played", "Last Played", "Size"};

	MenuHeading("List Settings");
	{
		DrawIntListSetting(bsi, ICON_FA_BORDER_ALL " Default View", "Sets which view the game list will open to.", "UI",
			"DefaultFullscreenUIGameView", 0, view_types, std::size(view_types));
		DrawIntListSetting(bsi, ICON_FA_SORT " Sort By", "Determines which field the game list will be sorted by.", "UI",
			"FullscreenUIGameSort", 0, sort_types, std::size(sort_types));
		DrawToggleSetting(bsi, ICON_FA_SORT_ALPHA_DOWN " Sort Reversed",
			"Reverses the game list sort order from the default (usually ascending to descending).", "UI", "FullscreenUIGameSortReverse",
			false);
	}

	MenuHeading("Cover Settings");
	{
		DrawFolderSetting(bsi, ICON_FA_FOLDER " Covers Directory", "Folders", "Covers", EmuFolders::Covers);
		if (MenuButton(ICON_FA_DOWNLOAD " Download Covers", "Downloads covers from a user-specified URL template."))
			ImGui::OpenPopup("Download Covers");
	}

	MenuHeading("Operations");
	{
		if (MenuButton(ICON_FA_SEARCH " Scan For New Games", "Identifies any new files added to the game directories."))
			Host::RefreshGameListAsync(false);
		if (MenuButton(ICON_FA_SEARCH_PLUS " Rescan All Games", "Forces a full rescan of all games previously identified."))
			Host::RefreshGameListAsync(true);
	}

	EndMenuButtons();

	DrawCoverDownloaderWindow();
	EndFullscreenWindow();
}

void FullscreenUI::DrawCoverDownloaderWindow()
{
	ImGui::SetNextWindowSize(LayoutScale(1000.0f, 0.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushFont(g_large_font);

	bool is_open = true;
	if (ImGui::BeginPopupModal("Download Covers", &is_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
	{
		ImGui::TextWrapped("PCSX2 can automatically download covers for games which do not currently have a cover set. We do not host any "
						   "cover images, the user must provide their own source for images.");
		ImGui::NewLine();
		ImGui::TextWrapped("In the form below, specify the URLs to download covers from, with one template URL per line. The following "
						   "variables are available:");
		ImGui::NewLine();
		ImGui::TextWrapped(
			"${title}: Title of the game.\n${filetitle}: Name component of the game's filename.\n${serial}: Serial of the game.");
		ImGui::NewLine();
		ImGui::TextWrapped("Example: https://www.example-not-a-real-domain.com/covers/${serial}.jpg");
		ImGui::NewLine();

		BeginMenuButtons();

		static char template_urls[512];
		ImGui::InputTextMultiline("##templates", template_urls, sizeof(template_urls),
			ImVec2(ImGui::GetCurrentWindow()->WorkRect.GetWidth(), LayoutScale(175.0f)));

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(5.0f));

		static bool use_serial_names;
		ImGui::PushFont(g_medium_font);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(2.0f, 2.0f));
		ImGui::Checkbox("Use Serial File Names", &use_serial_names);
		ImGui::PopStyleVar(1);
		ImGui::PopFont();

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(10.0f));

		const bool download_enabled = (std::strlen(template_urls) > 0);

		if (ActiveButton(ICON_FA_DOWNLOAD " Start Download", false, download_enabled))
		{
			StartAsyncOp(
				[urls = StringUtil::splitOnNewLine(template_urls), use_serial_names = use_serial_names](::ProgressCallback* progress) {
					GameList::DownloadCovers(urls, use_serial_names, progress, [](const GameList::Entry* entry, std::string save_path) {
						// cache the cover path on our side once it's saved
						Host::RunOnCPUThread([path = entry->path, save_path = std::move(save_path)]() {
							MTGS::RunOnGSThread([path = std::move(path), save_path = std::move(save_path)]() {
								s_cover_image_map[std::move(path)] = std::move(save_path);
							});
						});
					});
				},
				"Download Covers");
			std::memset(template_urls, 0, sizeof(template_urls));
			use_serial_names = false;
			ImGui::CloseCurrentPopup();
		}

		if (ActiveButton(ICON_FA_TIMES " Cancel", false))
		{
			std::memset(template_urls, 0, sizeof(template_urls));
			use_serial_names = false;
			ImGui::CloseCurrentPopup();
		}

		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopFont();
	ImGui::PopStyleVar(2);
}

void FullscreenUI::SwitchToGameList()
{
	s_current_main_window = MainWindowType::GameList;
	s_game_list_page = static_cast<GameListPage>(Host::GetBaseIntSettingValue("UI", "DefaultFullscreenUIGameView", 0));
	{
		auto lock = Host::GetSettingsLock();
		PopulateGameListDirectoryCache(Host::Internal::GetBaseSettingsLayer());
	}
	QueueResetFocus();
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

	GSTexture* tex = (!cover_it->second.empty()) ? GetCachedTextureAsync(cover_it->second.c_str()) : nullptr;
	return tex ? tex : GetTextureForGameListEntryType(entry->type);
}

GSTexture* FullscreenUI::GetTextureForGameListEntryType(GameList::EntryType type)
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

GSTexture* FullscreenUI::GetCoverForCurrentGame()
{
	auto lock = GameList::GetLock();

	const GameList::Entry* entry = GameList::GetEntryForPath(s_current_disc_path.c_str());
	if (!entry)
		return s_fallback_disc_texture.get();

	return GetGameListCover(entry);
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

		if (ActiveButton(ICON_FA_GLOBE " Website", false))
			ExitFullscreenAndOpenURL(PCSX2_WEBSITE_URL);

		if (ActiveButton(ICON_FA_PERSON_BOOTH " Support Forums", false))
			ExitFullscreenAndOpenURL(PCSX2_FORUMS_URL);

		if (ActiveButton(ICON_FA_BUG " GitHub Repository", false))
			ExitFullscreenAndOpenURL(PCSX2_GITHUB_URL);

		if (ActiveButton(ICON_FA_NEWSPAPER " License", false))
			ExitFullscreenAndOpenURL(PCSX2_LICENSE_URL);

		if (ActiveButton(ICON_FA_WINDOW_CLOSE " Close", false))
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
	ShowToast(std::string(), message);
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

#ifdef ENABLE_ACHIEVEMENTS

void FullscreenUI::OpenAchievementsWindow()
{
	if (!VMManager::HasValidVM() || !Achievements::IsActive())
		return;

	MTGS::RunOnGSThread([]() {
		if (!ImGuiManager::InitializeFullscreenUI())
			return;

		SwitchToAchievementsWindow();
	});
}

void FullscreenUI::SwitchToAchievementsWindow()
{
	if (!VMManager::HasValidVM())
		return;

	if (!Achievements::HasActiveGame() || Achievements::GetAchievementCount() == 0)
	{
		ShowToast(std::string(), "This game has no achievements.");
		return;
	}

	if (s_current_main_window != MainWindowType::PauseMenu)
		PauseForMenuOpen();

	s_current_main_window = MainWindowType::Achievements;
	QueueResetFocus();
}

void FullscreenUI::DrawAchievement(const Achievements::Achievement& cheevo)
{
	static constexpr float alpha = 0.8f;
	static constexpr float progress_height_unscaled = 20.0f;
	static constexpr float progress_spacing_unscaled = 5.0f;

	std::string id_str(fmt::format("chv_{}", cheevo.id));

	const auto progress = Achievements::GetAchievementProgress(cheevo);
	const bool is_measured = progress.second != 0;

	ImRect bb;
	bool visible, hovered;
	bool pressed = MenuButtonFrame(id_str.c_str(), true,
		!is_measured ? LAYOUT_MENU_BUTTON_HEIGHT : LAYOUT_MENU_BUTTON_HEIGHT + progress_height_unscaled + progress_spacing_unscaled,
		&visible, &hovered, &bb.Min, &bb.Max, 0, alpha);
	if (!visible)
		return;

	const ImVec2 image_size(LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT, LAYOUT_MENU_BUTTON_HEIGHT));
	const std::string& badge_path = Achievements::GetAchievementBadgePath(cheevo);
	if (!badge_path.empty())
	{
		GSTexture* badge = GetCachedTextureAsync(badge_path.c_str());
		if (badge)
		{
			ImGui::GetWindowDrawList()->AddImage(badge->GetNativeHandle(), bb.Min, bb.Min + image_size, ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f), IM_COL32(255, 255, 255, 255));
		}
	}

	const float midpoint = bb.Min.y + g_large_font->FontSize + LayoutScale(4.0f);
	std::string points_text(fmt::format("{} point{}", cheevo.points, cheevo.points != 1 ? "s" : ""));
	const ImVec2 points_template_size(g_medium_font->CalcTextSizeA(g_medium_font->FontSize, FLT_MAX, 0.0f, "XXX points"));
	const ImVec2 points_size(g_medium_font->CalcTextSizeA(
		g_medium_font->FontSize, FLT_MAX, 0.0f, points_text.c_str(), points_text.c_str() + points_text.length()));
	const float points_template_start = bb.Max.x - points_template_size.x;
	const float points_start = points_template_start + ((points_template_size.x - points_size.x) * 0.5f);
	const char* lock_text = cheevo.locked ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN;
	const ImVec2 lock_size(g_large_font->CalcTextSizeA(g_large_font->FontSize, FLT_MAX, 0.0f, lock_text));

	const float text_start_x = bb.Min.x + image_size.x + LayoutScale(15.0f);
	const ImRect title_bb(ImVec2(text_start_x, bb.Min.y), ImVec2(points_start, midpoint));
	const ImRect summary_bb(ImVec2(text_start_x, midpoint), ImVec2(points_start, bb.Max.y));
	const ImRect points_bb(ImVec2(points_start, midpoint), bb.Max);
	const ImRect lock_bb(
		ImVec2(points_template_start + ((points_template_size.x - lock_size.x) * 0.5f), bb.Min.y), ImVec2(bb.Max.x, midpoint));

	ImGui::PushFont(g_large_font);
	ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, cheevo.title.c_str(), cheevo.title.c_str() + cheevo.title.size(), nullptr,
		ImVec2(0.0f, 0.0f), &title_bb);
	ImGui::RenderTextClipped(lock_bb.Min, lock_bb.Max, lock_text, nullptr, &lock_size, ImVec2(0.0f, 0.0f), &lock_bb);
	ImGui::PopFont();

	ImGui::PushFont(g_medium_font);
	if (!cheevo.description.empty())
	{
		ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, cheevo.description.c_str(),
			cheevo.description.c_str() + cheevo.description.size(), nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
	}
	ImGui::RenderTextClipped(points_bb.Min, points_bb.Max, points_text.c_str(), points_text.c_str() + points_text.length(), &points_size,
		ImVec2(0.0f, 0.0f), &points_bb);
	ImGui::PopFont();

	if (is_measured)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float progress_height = LayoutScale(progress_height_unscaled);
		const float progress_spacing = LayoutScale(progress_spacing_unscaled);
		const float top = midpoint + g_medium_font->FontSize + progress_spacing;
		const ImRect progress_bb(ImVec2(text_start_x, top), ImVec2(bb.Max.x, top + progress_height));
		const float fraction = static_cast<float>(progress.first) / static_cast<float>(progress.second);
		dl->AddRectFilled(progress_bb.Min, progress_bb.Max, ImGui::GetColorU32(ImGuiFullscreen::UIPrimaryDarkColor));
		dl->AddRectFilled(progress_bb.Min, ImVec2(progress_bb.Min.x + fraction * progress_bb.GetWidth(), progress_bb.Max.y),
			ImGui::GetColorU32(ImGuiFullscreen::UISecondaryColor));

		const std::string text(Achievements::GetAchievementProgressText(cheevo));
		const ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
		const ImVec2 text_pos(progress_bb.Min.x + ((progress_bb.Max.x - progress_bb.Min.x) / 2.0f) - (text_size.x / 2.0f),
			progress_bb.Min.y + ((progress_bb.Max.y - progress_bb.Min.y) / 2.0f) - (text_size.y / 2.0f));
		dl->AddText(g_medium_font, g_medium_font->FontSize, text_pos, ImGui::GetColorU32(ImGuiFullscreen::UIPrimaryTextColor), text.c_str(),
			text.c_str() + text.size());
	}

#if 0
	// The API doesn't seem to send us this :(
	if (!cheevo.locked)
	{
		ImGui::PushFont(g_medium_font);

		const ImRect time_bb(ImVec2(text_start_x, bb.Min.y),
			ImVec2(bb.Max.x, bb.Min.y + g_medium_font->FontSize + LayoutScale(4.0f)));
		text.Format("Unlocked 21 Feb, 2019 @ 3:14am");
		ImGui::RenderTextClipped(time_bb.Min, time_bb.Max, text.GetCharArray(), text.GetCharArray() + text.GetLength(),
			nullptr, ImVec2(1.0f, 0.0f), &time_bb);
		ImGui::PopFont();
	}
#endif

	if (pressed)
	{
		// TODO: What should we do here?
		// Display information or something..
	}
}

void FullscreenUI::DrawAchievementsWindow()
{
	// ensure image downloads still happen while we're paused
	Achievements::ProcessPendingHTTPRequestsFromGSThread();

	static constexpr float alpha = 0.8f;
	static constexpr float heading_height_unscaled = 110.0f;

	ImGui::SetNextWindowBgAlpha(alpha);

	const ImVec4 background(0.13f, 0.13f, 0.13f, alpha);
	const ImVec2 display_size(ImGui::GetIO().DisplaySize);
	const float heading_height = LayoutScale(heading_height_unscaled);

	if (BeginFullscreenWindow(ImVec2(0.0f, 0.0f), ImVec2(display_size.x, heading_height), "achievements_heading", background, 0.0f, 0.0f,
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse))
	{
		auto lock = Achievements::GetLock();

		ImRect bb;
		bool visible, hovered;
		/*bool pressed = */ MenuButtonFrame(
			"achievements_heading", false, heading_height_unscaled, &visible, &hovered, &bb.Min, &bb.Max, 0, alpha);

		if (visible)
		{
			const float padding = LayoutScale(10.0f);
			const float spacing = LayoutScale(10.0f);
			const float image_height = LayoutScale(85.0f);

			const ImVec2 icon_min(bb.Min + ImVec2(padding, padding));
			const ImVec2 icon_max(icon_min + ImVec2(image_height, image_height));

			const std::string& icon_path = Achievements::GetGameIcon();
			if (!icon_path.empty())
			{
				GSTexture* badge = GetCachedTexture(icon_path.c_str());
				if (badge)
				{
					ImGui::GetWindowDrawList()->AddImage(
						badge->GetNativeHandle(), icon_min, icon_max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), IM_COL32(255, 255, 255, 255));
				}
			}

			float left = bb.Min.x + padding + image_height + spacing;
			float right = bb.Max.x - padding;
			float top = bb.Min.y + padding;
			ImDrawList* dl = ImGui::GetWindowDrawList();
			std::string text;
			ImVec2 text_size;

			const u32 unlocked_count = Achievements::GetUnlockedAchiementCount();
			const u32 achievement_count = Achievements::GetAchievementCount();
			const u32 current_points = Achievements::GetCurrentPointsForGame();
			const u32 total_points = Achievements::GetMaximumPointsForGame();

			if (FloatingButton(ICON_FA_WINDOW_CLOSE, 10.0f, 10.0f, -1.0f, -1.0f, 1.0f, 0.0f, true, g_large_font) || WantsToCloseMenu())
			{
				ReturnToMainWindow();
			}

			const ImRect title_bb(ImVec2(left, top), ImVec2(right, top + g_large_font->FontSize));
			text = Achievements::GetGameTitle();

			if (Achievements::ChallengeModeActive())
				text += " (Hardcore Mode)";

			top += g_large_font->FontSize + spacing;

			ImGui::PushFont(g_large_font);
			ImGui::RenderTextClipped(
				title_bb.Min, title_bb.Max, text.c_str(), text.c_str() + text.length(), nullptr, ImVec2(0.0f, 0.0f), &title_bb);
			ImGui::PopFont();

			const ImRect summary_bb(ImVec2(left, top), ImVec2(right, top + g_medium_font->FontSize));
			if (unlocked_count == achievement_count)
			{
				text = fmt::format("You have unlocked all achievements and earned {} points!", total_points);
			}
			else
			{
				text = fmt::format("You have unlocked {} of {} achievements, earning {} of {} possible points.", unlocked_count,
					achievement_count, current_points, total_points);
			}

			top += g_medium_font->FontSize + spacing;

			ImGui::PushFont(g_medium_font);
			ImGui::RenderTextClipped(
				summary_bb.Min, summary_bb.Max, text.c_str(), text.c_str() + text.length(), nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
			ImGui::PopFont();

			const float progress_height = LayoutScale(20.0f);
			const ImRect progress_bb(ImVec2(left, top), ImVec2(right, top + progress_height));
			const float fraction = static_cast<float>(unlocked_count) / static_cast<float>(achievement_count);
			dl->AddRectFilled(progress_bb.Min, progress_bb.Max, ImGui::GetColorU32(ImGuiFullscreen::UIPrimaryDarkColor));
			dl->AddRectFilled(progress_bb.Min, ImVec2(progress_bb.Min.x + fraction * progress_bb.GetWidth(), progress_bb.Max.y),
				ImGui::GetColorU32(ImGuiFullscreen::UISecondaryColor));

			text = fmt::format("{}%", static_cast<int>(std::round(fraction * 100.0f)));
			text_size = ImGui::CalcTextSize(text.c_str());
			const ImVec2 text_pos(progress_bb.Min.x + ((progress_bb.Max.x - progress_bb.Min.x) / 2.0f) - (text_size.x / 2.0f),
				progress_bb.Min.y + ((progress_bb.Max.y - progress_bb.Min.y) / 2.0f) - (text_size.y / 2.0f));
			dl->AddText(g_medium_font, g_medium_font->FontSize, text_pos, ImGui::GetColorU32(ImGuiFullscreen::UIPrimaryTextColor),
				text.c_str(), text.c_str() + text.length());
			top += progress_height + spacing;
		}
	}
	EndFullscreenWindow();

	ImGui::SetNextWindowBgAlpha(alpha);

	if (BeginFullscreenWindow(ImVec2(0.0f, heading_height), ImVec2(display_size.x, display_size.y - heading_height), "achievements",
			background, 0.0f, 0.0f, 0))
	{
		BeginMenuButtons();

		static bool unlocked_achievements_collapsed = false;

		unlocked_achievements_collapsed ^=
			MenuHeadingButton("Unlocked Achievements", unlocked_achievements_collapsed ? ICON_FA_CHEVRON_DOWN : ICON_FA_CHEVRON_UP);
		if (!unlocked_achievements_collapsed)
		{
			Achievements::EnumerateAchievements([](const Achievements::Achievement& cheevo) -> bool {
				if (!cheevo.locked)
					DrawAchievement(cheevo);

				return true;
			});
		}

		if (Achievements::GetUnlockedAchiementCount() != Achievements::GetAchievementCount())
		{
			static bool locked_achievements_collapsed = false;
			locked_achievements_collapsed ^=
				MenuHeadingButton("Locked Achievements", locked_achievements_collapsed ? ICON_FA_CHEVRON_DOWN : ICON_FA_CHEVRON_UP);
			if (!locked_achievements_collapsed)
			{
				Achievements::EnumerateAchievements([](const Achievements::Achievement& cheevo) -> bool {
					if (cheevo.locked)
						DrawAchievement(cheevo);

					return true;
				});
			}
		}

		EndMenuButtons();
	}
	EndFullscreenWindow();
}

void FullscreenUI::DrawPrimedAchievementsIcons()
{
	const ImVec2 image_size(LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT, LAYOUT_MENU_BUTTON_HEIGHT));
	const float spacing = LayoutScale(10.0f);
	const float padding = LayoutScale(10.0f);

	const ImGuiIO& io = ImGui::GetIO();
	const float x_advance = image_size.x + spacing;
	ImVec2 position(io.DisplaySize.x - padding - image_size.x, io.DisplaySize.y - padding - image_size.y);

	auto lock = Achievements::GetLock();
	Achievements::EnumerateAchievements([&image_size, &x_advance, &position](const Achievements::Achievement& achievement) {
		if (!achievement.primed)
			return true;

		const std::string& badge_path = Achievements::GetAchievementBadgePath(achievement, true, true);
		if (badge_path.empty())
			return true;

		GSTexture* badge = GetCachedTextureAsync(badge_path.c_str());
		if (!badge)
			return true;

		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		dl->AddImage(badge->GetNativeHandle(), position, position + image_size);
		position.x -= x_advance;
		return true;
	});
}

void FullscreenUI::DrawPrimedAchievementsList()
{
	auto lock = Achievements::GetLock();
	const u32 primed_count = Achievements::GetPrimedAchievementCount();

	const ImGuiIO& io = ImGui::GetIO();
	ImFont* font = g_medium_font;

	const ImVec2 image_size(LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY));
	const float margin = LayoutScale(10.0f);
	const float spacing = LayoutScale(10.0f);
	const float padding = LayoutScale(10.0f);

	const float max_text_width = LayoutScale(300.0f);
	const float row_width = max_text_width + padding + padding + image_size.x + spacing;
	const float title_height = padding + font->FontSize + padding;
	const ImVec2 box_min(io.DisplaySize.x - row_width - margin, margin);
	const ImVec2 box_max(box_min.x + row_width, box_min.y + title_height + (static_cast<float>(primed_count) * (image_size.y + padding)));

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	dl->AddRectFilled(box_min, box_max, IM_COL32(0x21, 0x21, 0x21, 200), LayoutScale(10.0f));
	dl->AddText(font, font->FontSize, ImVec2(box_min.x + padding, box_min.y + padding), IM_COL32(255, 255, 255, 255),
		"Active Challenge Achievements");

	const float y_advance = image_size.y + spacing;
	const float acheivement_name_offset = (image_size.y - font->FontSize) / 2.0f;
	const float max_non_ellipised_text_width = max_text_width - LayoutScale(10.0f);
	ImVec2 position(box_min.x + padding, box_min.y + title_height);

	Achievements::EnumerateAchievements([font, &image_size, max_text_width, spacing, y_advance, acheivement_name_offset,
											max_non_ellipised_text_width, &position](const Achievements::Achievement& achievement) {
		if (!achievement.primed)
			return true;

		const std::string& badge_path = Achievements::GetAchievementBadgePath(achievement, true, true);
		if (badge_path.empty())
			return true;

		GSTexture* badge = GetCachedTextureAsync(badge_path.c_str());
		if (!badge)
			return true;

		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		dl->AddImage(badge->GetNativeHandle(), position, position + image_size);

		const char* achievement_title = achievement.title.c_str();
		const char* achievement_tile_end = achievement_title + achievement.title.length();
		const char* remaining_text = nullptr;
		const ImVec2 text_width(font->CalcTextSizeA(
			font->FontSize, max_non_ellipised_text_width, 0.0f, achievement_title, achievement_tile_end, &remaining_text));
		const ImVec2 text_position(position.x + image_size.x + spacing, position.y + acheivement_name_offset);
		const ImVec4 text_bbox(text_position.x, text_position.y, text_position.x + max_text_width, text_position.y + image_size.y);
		const u32 text_color = IM_COL32(255, 255, 255, 255);

		if (remaining_text < achievement_tile_end)
		{
			dl->AddText(font, font->FontSize, text_position, text_color, achievement_title, remaining_text, 0.0f, &text_bbox);
			dl->AddText(font, font->FontSize, ImVec2(text_position.x + text_width.x, text_position.y), text_color, "...", nullptr, 0.0f,
				&text_bbox);
		}
		else
		{
			dl->AddText(font, font->FontSize, text_position, text_color, achievement_title, achievement_title + achievement.title.length(),
				0.0f, &text_bbox);
		}

		position.y += y_advance;
		return true;
	});
}

void FullscreenUI::OpenLeaderboardsWindow()
{
	if (!VMManager::HasValidVM() || !Achievements::IsActive())
		return;

	MTGS::RunOnGSThread([]() {
		if (!ImGuiManager::InitializeFullscreenUI())
			return;

		SwitchToLeaderboardsWindow();
	});
}

void FullscreenUI::SwitchToLeaderboardsWindow()
{
	if (!VMManager::HasValidVM())
		return;

	if (!Achievements::HasActiveGame() || Achievements::GetLeaderboardCount() == 0)
	{
		ShowToast(std::string(), "This game has no leaderboards.");
		return;
	}

	if (s_current_main_window != MainWindowType::PauseMenu)
		PauseForMenuOpen();

	s_current_main_window = MainWindowType::Leaderboards;
	s_open_leaderboard_id.reset();
	QueueResetFocus();
}

void FullscreenUI::DrawLeaderboardListEntry(const Achievements::Leaderboard& lboard)
{
	static constexpr float alpha = 0.8f;

	std::string id_str(fmt::format("lb_{}", lboard.id));

	ImRect bb;
	bool visible, hovered;
	bool pressed = MenuButtonFrame(id_str.c_str(), true, LAYOUT_MENU_BUTTON_HEIGHT, &visible, &hovered, &bb.Min, &bb.Max, 0, alpha);
	if (!visible)
		return;

	const float midpoint = bb.Min.y + g_large_font->FontSize + LayoutScale(4.0f);
	const float text_start_x = bb.Min.x + LayoutScale(15.0f);
	const ImRect title_bb(ImVec2(text_start_x, bb.Min.y), ImVec2(bb.Max.x, midpoint));
	const ImRect summary_bb(ImVec2(text_start_x, midpoint), bb.Max);

	ImGui::PushFont(g_large_font);
	ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, lboard.title.c_str(), lboard.title.c_str() + lboard.title.size(), nullptr,
		ImVec2(0.0f, 0.0f), &title_bb);
	ImGui::PopFont();

	if (!lboard.description.empty())
	{
		ImGui::PushFont(g_medium_font);
		ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, lboard.description.c_str(),
			lboard.description.c_str() + lboard.description.size(), nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
		ImGui::PopFont();
	}

	if (pressed)
	{
		s_open_leaderboard_id = lboard.id;
	}
}

void FullscreenUI::DrawLeaderboardEntry(
	const Achievements::LeaderboardEntry& lbEntry, float rank_column_width, float name_column_width, float column_spacing)
{
	static constexpr float alpha = 0.8f;

	ImRect bb;
	bool visible, hovered;
	bool pressed =
		MenuButtonFrame(lbEntry.user.c_str(), true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, &visible, &hovered, &bb.Min, &bb.Max, 0, alpha);
	if (!visible)
		return;

	const float midpoint = bb.Min.y + g_large_font->FontSize + LayoutScale(4.0f);
	float text_start_x = bb.Min.x + LayoutScale(15.0f);
	std::string rank_str(fmt::format("{}", lbEntry.rank));

	ImGui::PushFont(g_large_font);
	if (lbEntry.is_self)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 242, 0, 255));
	}

	const ImRect rank_bb(ImVec2(text_start_x, bb.Min.y), ImVec2(bb.Max.x, midpoint));
	ImGui::RenderTextClipped(rank_bb.Min, rank_bb.Max, rank_str.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &rank_bb);
	text_start_x += rank_column_width + column_spacing;

	const ImRect user_bb(ImVec2(text_start_x, bb.Min.y), ImVec2(bb.Max.x, midpoint));
	ImGui::RenderTextClipped(
		user_bb.Min, user_bb.Max, lbEntry.user.c_str(), lbEntry.user.c_str() + lbEntry.user.size(), nullptr, ImVec2(0.0f, 0.0f), &user_bb);
	text_start_x += name_column_width + column_spacing;

	const ImRect score_bb(ImVec2(text_start_x, bb.Min.y), ImVec2(bb.Max.x, midpoint));
	ImGui::RenderTextClipped(score_bb.Min, score_bb.Max, lbEntry.formatted_score.c_str(),
		lbEntry.formatted_score.c_str() + lbEntry.formatted_score.size(), nullptr, ImVec2(0.0f, 0.0f), &score_bb);

	if (lbEntry.is_self)
	{
		ImGui::PopStyleColor();
	}

	ImGui::PopFont();

	// This API DOES list the submission date/time, but is it relevant?
#if 0
	if (!cheevo.locked)
	{
		ImGui::PushFont(g_medium_font);

		const ImRect time_bb(ImVec2(text_start_x, bb.Min.y),
			ImVec2(bb.Max.x, bb.Min.y + g_medium_font->FontSize + LayoutScale(4.0f)));
		text.Format("Unlocked 21 Feb, 2019 @ 3:14am");
		ImGui::RenderTextClipped(time_bb.Min, time_bb.Max, text.GetCharArray(), text.GetCharArray() + text.GetLength(),
			nullptr, ImVec2(1.0f, 0.0f), &time_bb);
		ImGui::PopFont();
	}
#endif

	if (pressed)
	{
		// Anything?
	}
}

void FullscreenUI::DrawLeaderboardsWindow()
{
	static constexpr float alpha = 0.8f;
	static constexpr float heading_height_unscaled = 110.0f;

	// ensure image downloads still happen while we're paused
	Achievements::ProcessPendingHTTPRequestsFromGSThread();

	ImGui::SetNextWindowBgAlpha(alpha);

	const bool is_leaderboard_open = s_open_leaderboard_id.has_value();
	bool close_leaderboard_on_exit = false;

	const ImVec4 background(0.13f, 0.13f, 0.13f, alpha);
	const ImVec2 display_size(ImGui::GetIO().DisplaySize);
	const float padding = LayoutScale(10.0f);
	const float spacing = LayoutScale(10.0f);
	const float spacing_small = spacing / 2.0f;
	float heading_height = LayoutScale(heading_height_unscaled);
	if (is_leaderboard_open)
	{
		// Add space for a legend - spacing + 1 line of text + spacing + line
		heading_height += spacing + LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY) + spacing;
	}

	const float rank_column_width =
		g_large_font->CalcTextSizeA(g_large_font->FontSize, std::numeric_limits<float>::max(), -1.0f, "99999").x;
	const float name_column_width =
		g_large_font->CalcTextSizeA(g_large_font->FontSize, std::numeric_limits<float>::max(), -1.0f, "WWWWWWWWWWWWWWWWWWWW").x;
	const float column_spacing = spacing * 2.0f;

	if (BeginFullscreenWindow(ImVec2(0.0f, 0.0f), ImVec2(display_size.x, heading_height), "leaderboards_heading", background, 0.0f, 0.0f,
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImRect bb;
		bool visible, hovered;
		/*bool pressed = */
		MenuButtonFrame("leaderboards_heading", false, heading_height_unscaled, &visible, &hovered, &bb.Min, &bb.Max, 0, alpha);

		if (visible)
		{
			const float image_height = LayoutScale(85.0f);

			const ImVec2 icon_min(bb.Min + ImVec2(padding, padding));
			const ImVec2 icon_max(icon_min + ImVec2(image_height, image_height));

			const std::string& icon_path = Achievements::GetGameIcon();
			if (!icon_path.empty())
			{
				GSTexture* badge = GetCachedTexture(icon_path.c_str());
				if (badge)
				{
					ImGui::GetWindowDrawList()->AddImage(
						badge->GetNativeHandle(), icon_min, icon_max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), IM_COL32(255, 255, 255, 255));
				}
			}

			float left = bb.Min.x + padding + image_height + spacing;
			float right = bb.Max.x - padding;
			float top = bb.Min.y + padding;

			const u32 leaderboard_count = Achievements::GetLeaderboardCount();

			if (!is_leaderboard_open)
			{
				if (FloatingButton(ICON_FA_WINDOW_CLOSE, 10.0f, 10.0f, -1.0f, -1.0f, 1.0f, 0.0f, true, g_large_font) || WantsToCloseMenu())
				{
					ReturnToMainWindow();
				}
			}
			else
			{
				if (FloatingButton(ICON_FA_CARET_SQUARE_LEFT, 10.0f, 10.0f, -1.0f, -1.0f, 1.0f, 0.0f, true, g_large_font) ||
					WantsToCloseMenu())
				{
					close_leaderboard_on_exit = true;
				}
			}

			const ImRect title_bb(ImVec2(left, top), ImVec2(right, top + g_large_font->FontSize));
			const std::string& title = Achievements::GetGameTitle();

			top += g_large_font->FontSize + spacing;

			ImGui::PushFont(g_large_font);
			ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
			ImGui::PopFont();

			std::string lb_description;
			if (s_open_leaderboard_id.has_value())
			{
				const Achievements::Leaderboard* lboard = Achievements::GetLeaderboardByID(s_open_leaderboard_id.value());
				if (lboard != nullptr)
				{
					const ImRect subtitle_bb(ImVec2(left, top), ImVec2(right, top + g_large_font->FontSize));
					const std::string& subtitle = lboard->title;

					top += g_large_font->FontSize + spacing_small;

					ImGui::PushFont(g_large_font);
					ImGui::RenderTextClipped(
						subtitle_bb.Min, subtitle_bb.Max, subtitle.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &subtitle_bb);
					ImGui::PopFont();

					lb_description = lboard->description;
				}
			}
			else
			{
				lb_description = fmt::format("This game has {} leaderboards.", leaderboard_count);
			}

			const ImRect summary_bb(ImVec2(left, top), ImVec2(right, top + g_medium_font->FontSize));
			top += g_medium_font->FontSize + spacing_small;

			ImGui::PushFont(g_medium_font);
			ImGui::RenderTextClipped(
				summary_bb.Min, summary_bb.Max, lb_description.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.0f), &summary_bb);

			if (!Achievements::ChallengeModeActive())
			{
				const ImRect hardcore_warning_bb(ImVec2(left, top), ImVec2(right, top + g_medium_font->FontSize));
				top += g_medium_font->FontSize + spacing_small;

				ImGui::RenderTextClipped(hardcore_warning_bb.Min, hardcore_warning_bb.Max,
					"Submitting scores is disabled because hardcore mode is off. Leaderboards are read-only.", nullptr, nullptr,
					ImVec2(0.0f, 0.0f), &hardcore_warning_bb);
			}

			ImGui::PopFont();
		}

		if (is_leaderboard_open)
		{
			/*bool pressed = */
			MenuButtonFrame("legend", false, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, &visible, &hovered, &bb.Min, &bb.Max, 0, alpha);

			if (visible)
			{
				const Achievements::Leaderboard* lboard = Achievements::GetLeaderboardByID(s_open_leaderboard_id.value());

				const float midpoint = bb.Min.y + g_large_font->FontSize + LayoutScale(4.0f);
				float text_start_x = bb.Min.x + LayoutScale(15.0f) + padding;

				ImGui::PushFont(g_large_font);

				const ImRect rank_bb(ImVec2(text_start_x, bb.Min.y), ImVec2(bb.Max.x, midpoint));
				ImGui::RenderTextClipped(rank_bb.Min, rank_bb.Max, "Rank", nullptr, nullptr, ImVec2(0.0f, 0.0f), &rank_bb);
				text_start_x += rank_column_width + column_spacing;

				const ImRect user_bb(ImVec2(text_start_x, bb.Min.y), ImVec2(bb.Max.x, midpoint));
				ImGui::RenderTextClipped(user_bb.Min, user_bb.Max, "Name", nullptr, nullptr, ImVec2(0.0f, 0.0f), &user_bb);
				text_start_x += name_column_width + column_spacing;

				const ImRect score_bb(ImVec2(text_start_x, bb.Min.y), ImVec2(bb.Max.x, midpoint));
				ImGui::RenderTextClipped(score_bb.Min, score_bb.Max,
					lboard != nullptr && Achievements::IsLeaderboardTimeType(*lboard) ? "Time" : "Score", nullptr, nullptr,
					ImVec2(0.0f, 0.0f), &score_bb);

				ImGui::PopFont();

				const float line_thickness = LayoutScale(1.0f);
				const float line_padding = LayoutScale(5.0f);
				const ImVec2 line_start(bb.Min.x, bb.Min.y + g_large_font->FontSize + line_padding);
				const ImVec2 line_end(bb.Max.x, line_start.y);
				ImGui::GetWindowDrawList()->AddLine(line_start, line_end, ImGui::GetColorU32(ImGuiCol_TextDisabled), line_thickness);
			}
		}
	}
	EndFullscreenWindow();

	ImGui::SetNextWindowBgAlpha(alpha);

	if (!is_leaderboard_open)
	{
		if (BeginFullscreenWindow(ImVec2(0.0f, heading_height), ImVec2(display_size.x, display_size.y - heading_height), "leaderboards",
				background, 0.0f, 0.0f, 0))
		{
			BeginMenuButtons();

			Achievements::EnumerateLeaderboards([](const Achievements::Leaderboard& lboard) -> bool {
				DrawLeaderboardListEntry(lboard);

				return true;
			});

			EndMenuButtons();
		}
		EndFullscreenWindow();
	}
	else
	{
		if (BeginFullscreenWindow(ImVec2(0.0f, heading_height), ImVec2(display_size.x, display_size.y - heading_height), "leaderboard",
				background, 0.0f, 0.0f, 0))
		{
			BeginMenuButtons();

			const auto result = Achievements::TryEnumerateLeaderboardEntries(s_open_leaderboard_id.value(),
				[rank_column_width, name_column_width, column_spacing](const Achievements::LeaderboardEntry& lbEntry) -> bool {
					DrawLeaderboardEntry(lbEntry, rank_column_width, name_column_width, column_spacing);
					return true;
				});

			if (!result.has_value())
			{
				ImGui::PushFont(g_large_font);

				const ImVec2 pos_min(0.0f, heading_height);
				const ImVec2 pos_max(display_size.x, display_size.y);
				ImGui::RenderTextClipped(
					pos_min, pos_max, "Downloading leaderboard data, please wait...", nullptr, nullptr, ImVec2(0.5f, 0.5f));

				ImGui::PopFont();
			}

			EndMenuButtons();
		}
		EndFullscreenWindow();
	}

	if (close_leaderboard_on_exit)
		s_open_leaderboard_id.reset();
}

void FullscreenUI::DrawAchievementsSettingsPage(std::unique_lock<std::mutex>& settings_lock)
{
#ifdef ENABLE_RAINTEGRATION
	if (Achievements::IsUsingRAIntegration())
	{
		BeginMenuButtons();
		ActiveButton(ICON_FA_BAN "  RAIntegration is being used instead of the built-in achievements implementation.", false, false,
			LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		EndMenuButtons();
		return;
	}
#endif

	SettingsInterface* bsi = GetEditingSettingsInterface();
	bool check_challenge_state = false;

	BeginMenuButtons();

	MenuHeading("Settings");
	check_challenge_state = DrawToggleSetting(bsi, ICON_FA_TROPHY "  Enable Achievements",
		"When enabled and logged in, PCSX2 will scan for achievements on startup.", "Achievements", "Enabled", false);

	const bool enabled = bsi->GetBoolValue("Achievements", "Enabled", false);
	const bool challenge = bsi->GetBoolValue("Achievements", "ChallengeMode", false);

	DrawToggleSetting(bsi, ICON_FA_USER_FRIENDS " Rich Presence",
		"When enabled, rich presence information will be collected and sent to the server where supported.", "Achievements", "RichPresence",
		true, enabled);
	check_challenge_state |= DrawToggleSetting(bsi, ICON_FA_HARD_HAT " Hardcore Mode",
		"\"Challenge\" mode for achievements, including leaderboard tracking. Disables save state, cheats, and slowdown functions.",
		"Achievements", "ChallengeMode", false, enabled);
	DrawToggleSetting(bsi, ICON_FA_LIST_OL " Leaderboards", "Enables tracking and submission of leaderboards in supported games.",
		"Achievements", "Leaderboards", true, enabled && challenge);
	DrawToggleSetting(bsi, ICON_FA_INBOX " Show Notifications",
		"Displays popup messages on events such as achievement unlocks and leaderboard submissions.", "Achievements", "Notifications", true,
		enabled);
	DrawToggleSetting(bsi, ICON_FA_HEADPHONES " Sound Effects",
		"Plays sound effects for events such as achievement unlocks and leaderboard submissions.", "Achievements", "SoundEffects", true,
		enabled);
	DrawToggleSetting(bsi, ICON_FA_MAGIC " Show Challenge Indicators",
		"Shows icons in the lower-right corner of the screen when a challenge/primed achievement is active.", "Achievements",
		"PrimedIndicators", true, enabled);
	DrawToggleSetting(bsi, ICON_FA_MEDAL " Test Unofficial Achievements",
		"When enabled, PCSX2 will list achievements from unofficial sets. These achievements are not tracked by RetroAchievements.",
		"Achievements", "UnofficialTestMode", false, enabled);
	DrawToggleSetting(bsi, ICON_FA_STETHOSCOPE " Test Mode",
		"When enabled, PCSX2 will assume all achievements are locked and not send any unlock notifications to the server.", "Achievements",
		"TestMode", false, enabled);

	// Check for challenge mode just being enabled.
	if (check_challenge_state && enabled && bsi->GetBoolValue("Achievements", "ChallengeMode", false) && VMManager::HasValidVM())
	{
		ImGuiFullscreen::OpenConfirmMessageDialog("Reset System",
			"Hardcore mode will not be enabled until the system is reset. Do you want to reset the system now?", [](bool reset) {
				if (!VMManager::HasValidVM())
					return;

				if (reset)
					DoReset();
			});
	}

	// Potential deadlock here: when we enable achievements, CPU thread reads settings, releases lock, then there's a brief
	// time when we can progress here and get the setting lock, by the time it goes to read the username out of settings,
	// we've got it here, but can't get the achievements lock. So only hold one at once.
	const u64 ts = StringUtil::FromChars<u64>(bsi->GetStringValue("Achievements", "LoginTimestamp", "0")).value_or(0);
	settings_lock.unlock();
	{
		const auto achievements_lock = Achievements::GetLock();
		if (Achievements::IsActive())
			Achievements::ProcessPendingHTTPRequestsFromGSThread();

		MenuHeading("Account");
		if (Achievements::HasSavedCredentials())
		{
			ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::GetStyle().Colors[ImGuiCol_Text]);
			ActiveButton(fmt::format(ICON_FA_USER "  Username: {}", Achievements::GetUsername()).c_str(), false, false,
				LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

			ActiveButton(fmt::format(ICON_FA_CLOCK "  Login token generated on {}", TimeToPrintableString(static_cast<time_t>(ts))).c_str(),
				false, false, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
			ImGui::PopStyleColor();

			if (MenuButton(ICON_FA_KEY "  Logout", "Logs out of RetroAchievements."))
			{
				Host::RunOnCPUThread([]() { Achievements::Logout(); });
			}
		}
		else if (Achievements::IsActive())
		{
			ActiveButton(ICON_FA_USER "  Not Logged In", false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

			if (MenuButton(ICON_FA_KEY "  Login", "Logs in to RetroAchievements."))
				ImGui::OpenPopup("Achievements Login");

			DrawAchievementsLoginWindow();
		}
		else
		{
			ActiveButton(ICON_FA_USER "  Achievements are disabled.", false, false, ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		}
	}

	MenuHeading("Current Game");
	if (Achievements::HasActiveGame())
	{
		ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::GetStyle().Colors[ImGuiCol_Text]);
		ActiveButton(fmt::format(ICON_FA_BOOKMARK "  Game ID: {}", Achievements::GetGameID()).c_str(), false, false,
			LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		ActiveButton(fmt::format(ICON_FA_BOOK "  Game Title: {}", Achievements::GetGameTitle()).c_str(), false, false,
			LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		ActiveButton(fmt::format(ICON_FA_TROPHY "  Achievements: {} ({} points)", Achievements::GetAchievementCount(),
						 Achievements::GetMaximumPointsForGame())
						 .c_str(),
			false, false, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);

		const std::string& rich_presence_string = Achievements::GetRichPresenceString();
		if (!rich_presence_string.empty())
		{
			ActiveButton(fmt::format(ICON_FA_MAP "  {}", rich_presence_string).c_str(), false, false, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		}
		else
		{
			ActiveButton(ICON_FA_MAP "  Rich presence inactive or unsupported.", false, false, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		}

		ImGui::PopStyleColor();
	}
	else
	{
		ActiveButton(
			ICON_FA_BAN "  Game not loaded or no RetroAchievements available.", false, false, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
	}

	EndMenuButtons();

	settings_lock.lock();
}

void FullscreenUI::DrawAchievementsLoginWindow()
{
	ImGui::SetNextWindowSize(LayoutScale(700.0f, 0.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushFont(g_large_font);

	bool is_open = true;
	if (ImGui::BeginPopupModal("Achievements Login", &is_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
	{

		ImGui::TextWrapped("Please enter your user name and password for retroachievements.org.");
		ImGui::NewLine();
		ImGui::TextWrapped("Your password will not be saved in PCSX2, an access token will be generated and used instead.");

		ImGui::NewLine();

		static char username[256] = {};
		static char password[256] = {};

		ImGui::Text("User Name: ");
		ImGui::SameLine(LayoutScale(200.0f));
		ImGui::InputText("##username", username, sizeof(username));

		ImGui::Text("Password: ");
		ImGui::SameLine(LayoutScale(200.0f));
		ImGui::InputText("##password", password, sizeof(password), ImGuiInputTextFlags_Password);

		ImGui::NewLine();

		BeginMenuButtons();

		const bool login_enabled = (std::strlen(username) > 0 && std::strlen(password) > 0);

		if (ActiveButton(ICON_FA_KEY "  Login", false, login_enabled))
		{
			Achievements::LoginAsync(username, password);
			std::memset(username, 0, sizeof(username));
			std::memset(password, 0, sizeof(password));
			ImGui::CloseCurrentPopup();
		}

		if (ActiveButton(ICON_FA_TIMES "  Cancel", false))
		{
			std::memset(username, 0, sizeof(username));
			std::memset(password, 0, sizeof(password));
			ImGui::CloseCurrentPopup();
		}

		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopFont();
	ImGui::PopStyleVar(2);
}

#else

void FullscreenUI::OpenAchievementsWindow()
{
}

void FullscreenUI::OpenLeaderboardsWindow()
{
}

void FullscreenUI::DrawAchievementsSettingsPage(std::unique_lock<std::mutex>& settings_lock)
{
	BeginMenuButtons();
	ActiveButton(ICON_FA_BAN "  This build was not compiled with RetroAchievements support.", false, false,
		ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
	EndMenuButtons();
}

void FullscreenUI::DrawAchievementsLoginWindow()
{
}

#endif
