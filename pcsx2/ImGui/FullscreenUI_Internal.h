// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "FullscreenUI.h"
#include "ImGuiFullscreen.h"

#include "common/Timer.h"
#include "Input/InputManager.h"

#define TR_CONTEXT "FullscreenUI"

template <size_t L>
class IconStackString : public SmallStackString<L>
{
public:
	__fi IconStackString(const char* icon, const char* str)
	{
		SmallStackString<L>::format("{} {}", icon, Host::TranslateToStringView(TR_CONTEXT, str));
	}
	__fi IconStackString(const char8_t* icon, const char* str)
	{
		SmallStackString<L>::format("{} {}", reinterpret_cast<const char*>(icon), Host::TranslateToStringView(TR_CONTEXT, str));
	}
	__fi IconStackString(const char* icon, const char* str, const char* suffix)
	{
		SmallStackString<L>::format("{} {}##{}", icon, Host::TranslateToStringView(TR_CONTEXT, str), suffix);
	}
	__fi IconStackString(const char8_t* icon, const char* str, const char* suffix)
	{
		SmallStackString<L>::format("{} {}##{}", reinterpret_cast<const char*>(icon), Host::TranslateToStringView(TR_CONTEXT, str), suffix);
	}
};

#define FSUI_ICONSTR(icon, str) IconStackString<256>(icon, str).c_str()
#define FSUI_ICONSTR_S(icon, str, suffix) IconStackString<256>(icon, str, suffix).c_str()
#define FSUI_STR(str) Host::TranslateToString(TR_CONTEXT, str)
#define FSUI_CSTR(str) Host::TranslateToCString(TR_CONTEXT, str)
#define FSUI_VSTR(str) Host::TranslateToStringView(TR_CONTEXT, str)
#define FSUI_FSTR(str) fmt::runtime(Host::TranslateToStringView(TR_CONTEXT, str))
#define FSUI_NSTR(str) str

using ImGuiFullscreen::ActiveButton;
using ImGuiFullscreen::AddNotification;
using ImGuiFullscreen::BeginFullscreenColumns;
using ImGuiFullscreen::BeginFullscreenColumnWindow;
using ImGuiFullscreen::BeginFullscreenWindow;
using ImGuiFullscreen::BeginHorizontalMenu;
using ImGuiFullscreen::BeginMenuButtons;
using ImGuiFullscreen::BeginNavBar;
using ImGuiFullscreen::CenterImage;
using ImGuiFullscreen::CloseChoiceDialog;
using ImGuiFullscreen::CloseFileSelector;
using ImGuiFullscreen::EndFullscreenColumns;
using ImGuiFullscreen::EndFullscreenColumnWindow;
using ImGuiFullscreen::EndFullscreenWindow;
using ImGuiFullscreen::EndHorizontalMenu;
using ImGuiFullscreen::EndMenuButtons;
using ImGuiFullscreen::EndNavBar;
using ImGuiFullscreen::EnumChoiceButton;
using ImGuiFullscreen::FloatingButton;
using ImGuiFullscreen::FocusResetType;
using ImGuiFullscreen::ForceKeyNavEnabled;
using ImGuiFullscreen::g_large_font;
using ImGuiFullscreen::g_layout_padding_left;
using ImGuiFullscreen::g_layout_padding_top;
using ImGuiFullscreen::g_medium_font;
using ImGuiFullscreen::GetCachedSvgTexture;
using ImGuiFullscreen::GetCachedSvgTextureAsync;
using ImGuiFullscreen::GetCachedTexture;
using ImGuiFullscreen::GetCachedTextureAsync;
using ImGuiFullscreen::GetPlaceholderTexture;
using ImGuiFullscreen::GetQueuedFocusResetType;
using ImGuiFullscreen::HorizontalMenuItem;
using ImGuiFullscreen::HorizontalMenuSvgItem;
using ImGuiFullscreen::InputFilterType;
using ImGuiFullscreen::IsFocusResetQueued;
using ImGuiFullscreen::IsGamepadInputSource;
using ImGuiFullscreen::LAYOUT_FOOTER_HEIGHT;
using ImGuiFullscreen::LAYOUT_LARGE_FONT_SIZE;
using ImGuiFullscreen::LAYOUT_MEDIUM_FONT_SIZE;
using ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT;
using ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY;
using ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING;
using ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING;
using ImGuiFullscreen::LAYOUT_SCREEN_HEIGHT;
using ImGuiFullscreen::LAYOUT_SCREEN_WIDTH;
using ImGuiFullscreen::LayoutScale;
using ImGuiFullscreen::LoadSvgTexture;
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
using ImGuiFullscreen::ResetFocusHere;
using ImGuiFullscreen::RightAlignNavButtons;
using ImGuiFullscreen::SetFullscreenFooterText;
using ImGuiFullscreen::ShowToast;
using ImGuiFullscreen::SvgScaling;
using ImGuiFullscreen::ThreeWayToggleButton;
using ImGuiFullscreen::ToggleButton;
using ImGuiFullscreen::UIBackgroundColor;
using ImGuiFullscreen::UIBackgroundHighlightColor;
using ImGuiFullscreen::UIBackgroundLineColor;
using ImGuiFullscreen::UIBackgroundTextColor;
using ImGuiFullscreen::UIDisabledColor;
using ImGuiFullscreen::UIPopupBackgroundColor;
using ImGuiFullscreen::UIPrimaryColor;
using ImGuiFullscreen::UIPrimaryDarkColor;
using ImGuiFullscreen::UIPrimaryLightColor;
using ImGuiFullscreen::UIPrimaryLineColor;
using ImGuiFullscreen::UIPrimaryTextColor;
using ImGuiFullscreen::UISecondaryColor;
using ImGuiFullscreen::UISecondaryStrongColor;
using ImGuiFullscreen::UISecondaryTextColor;
using ImGuiFullscreen::UISecondaryWeakColor;
using ImGuiFullscreen::UITextHighlightColor;
using ImGuiFullscreen::WantsToCloseMenu;

namespace FullscreenUI
{
	enum class MainWindowType
	{
		None,
		Landing,
		StartGame,
		Exit,
		GameList,
		GameListSettings,
		Settings,
		PauseMenu,
		Achievements,
		Leaderboards,
	};

	enum class PauseSubMenu
	{
		None,
		Exit,
		Achievements,
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
		NetworkHDD,
		Folders,
		Achievements,
		Controller,
		Hotkey,
		Advanced,
		Patches,
		Cheats,
		GameFixes,
		Count
	};

	enum class GameListView
	{
		Grid,
		List,
		Count
	};

	enum class IPAddressType
	{
		PS2IP,
		SubnetMask,
		Gateway,
		DNS1,
		DNS2,
		Other
	};

	//////////////////////////////////////////////////////////////////////////
	// Main
	//////////////////////////////////////////////////////////////////////////
	void UpdateGameDetails(std::string path, std::string serial, std::string title, u32 disc_crc, u32 crc);
	bool AreAnyDialogsOpen();
	void PauseForMenuOpen(bool set_pause_menu_open);
	void ClosePauseMenu();
	void OpenPauseSubMenu(PauseSubMenu submenu);
	void DrawLandingTemplate(ImVec2* menu_pos, ImVec2* menu_size);
	void DrawLandingWindow();
	void DrawStartGameWindow();
	void DrawExitWindow();
	void DrawPauseMenu(MainWindowType type);
	void ExitFullscreenAndOpenURL(const std::string_view url);
	void CopyTextToClipboard(std::string title, const std::string_view text);
	void DrawAboutWindow();
	void OpenAboutWindow();
	void GetStandardSelectionFooterText(SmallStringBase& dest, bool back_instead_of_cancel);
	void ApplyLayoutSettings(const SettingsInterface* bsi = nullptr);

	void DrawSvgTexture(GSTexture* padded_texture, ImVec2 unpadded_size);
	void DrawCachedSvgTexture(const std::string& path, ImVec2 size, SvgScaling mode);
	void DrawCachedSvgTextureAsync(const std::string& path, ImVec2 size, SvgScaling mode);
	void DrawListSvgTexture(ImDrawList* drawList, GSTexture* padded_texture, const ImVec2& p_min, const ImVec2& p_unpadded_max);

	inline MainWindowType s_current_main_window = MainWindowType::None;
	inline PauseSubMenu s_current_pause_submenu = PauseSubMenu::None;
	inline bool s_initialized = false;
	inline bool s_tried_to_initialize = false;
	inline bool s_pause_menu_was_open = false;
	inline bool s_was_paused_on_quick_menu_open = false;
	inline bool s_about_window_open = false;

	// achievements login dialog state
	inline bool s_achievements_login_open = false;
	inline bool s_achievements_login_logging_in = false;
	inline char s_achievements_login_username[256] = {};
	inline char s_achievements_login_password[256] = {};
	inline Achievements::LoginRequestReason s_achievements_login_reason = Achievements::LoginRequestReason::UserInitiated;

	// local copies of the currently-running game
	inline std::string s_current_game_title;
	inline std::string s_current_game_subtitle;
	inline std::string s_current_disc_serial;
	inline std::string s_current_disc_path;
	inline u32 s_current_disc_crc;

	//////////////////////////////////////////////////////////////////////////
	// Resources
	//////////////////////////////////////////////////////////////////////////
	bool LoadResources();
	bool LoadSvgResources();
	void DestroyResources();

	inline std::array<std::shared_ptr<GSTexture>, static_cast<u32>(GameDatabaseSchema::Compatibility::Perfect)>
		s_game_compatibility_textures;
	inline std::shared_ptr<GSTexture> s_banner_texture;
	inline std::vector<std::unique_ptr<GSTexture>> s_cleanup_textures;

	//////////////////////////////////////////////////////////////////////////
	// Landing
	//////////////////////////////////////////////////////////////////////////
	void SwitchToLanding();
	ImGuiFullscreen::FileSelectorFilters GetOpenFileFilters();
	ImGuiFullscreen::FileSelectorFilters GetDiscImageFilters();
	ImGuiFullscreen::FileSelectorFilters GetAudioFileFilters();
	ImGuiFullscreen::FileSelectorFilters GetImageFileFilters();
	void DoVMInitialize(const VMBootParameters& boot_params, bool switch_to_landing_on_failure);
	void DoStartPath(
		const std::string& path, std::optional<s32> state_index = std::nullopt, std::optional<bool> fast_boot = std::nullopt);
	void DoStartFile();
	void DoStartBIOS();
	void DoStartDisc(const std::string& drive);
	void DoStartDisc();
	void DoToggleFrameLimit();
	void DoToggleSoftwareRenderer();
	void RequestShutdown(bool save_state);
	void DoShutdown(bool save_state);
	void RequestReset();
	void DoReset();
	void DoChangeDiscFromFile();
	void RequestChangeDisc();
	void DoRequestExit();
	void DoDesktopMode();
	void DoToggleFullscreen();

	void ConfirmShutdownIfMemcardBusy(std::function<void(bool)> callback);

	bool ShouldDefaultToGameList();

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

	void InitializePlaceholderSaveStateListEntry(SaveStateListEntry* li, s32 slot);
	bool InitializeSaveStateListEntry(
		SaveStateListEntry* li, const std::string& title, const std::string& serial, u32 crc, s32 slot, bool backup = false);
	void ClearSaveStateEntryList();
	u32 PopulateSaveStateListEntries(const std::string& title, const std::string& serial, u32 crc);
	bool OpenLoadStateSelectorForGame(const std::string& game_path);
	bool OpenSaveStateSelector(bool is_loading);
	void CloseSaveStateSelector();
	void DrawSaveStateSelector(bool is_loading);
	bool OpenLoadStateSelectorForGameResume(const GameList::Entry* entry);
	void DrawResumeStateSelector();
	void DoLoadState(std::string path, std::optional<s32> slot, bool backup);
	void DoSaveState(s32 slot);

	inline std::vector<SaveStateListEntry> s_save_state_selector_slots;
	inline std::string s_save_state_selector_game_path;
	inline s32 s_save_state_selector_submenu_index = -1;
	inline bool s_save_state_selector_open = false;
	inline bool s_save_state_selector_loading = true;
	inline bool s_save_state_selector_resuming = false;

	//////////////////////////////////////////////////////////////////////////
	// Game List
	//////////////////////////////////////////////////////////////////////////
	void DrawGameListWindow();
	void DrawGameList(const ImVec2& heading_size);
	void DrawGameGrid(const ImVec2& heading_size);
	void HandleGameListActivate(const GameList::Entry* entry);
	void HandleGameListOptions(const GameList::Entry* entry);
	void DrawGameListSettingsWindow();
	void SwitchToGameList();
	void PopulateGameListEntryList();
	GSTexture* GetTextureForGameListEntryType(GameList::EntryType type, const ImVec2& size, SvgScaling mode = SvgScaling::Stretch);
	GSTexture* GetGameListCover(const GameList::Entry* entry);
	void DrawGameCover(const GameList::Entry* entry, const ImVec2& size);
	void DrawGameCover(const GameList::Entry* entry, ImDrawList* draw_list, const ImVec2& min, const ImVec2& max);
	// For when we have no GameList entry
	void DrawFallbackCover(const ImVec2& size);
	void DrawFallbackCover(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max);

	// Lazily populated cover images.
	inline std::unordered_map<std::string, std::string> s_cover_image_map;
	inline std::vector<const GameList::Entry*> s_game_list_sorted_entries;
	inline GameListView s_game_list_view = GameListView::Grid;

	//////////////////////////////////////////////////////////////////////////
	// Background
	//////////////////////////////////////////////////////////////////////////
	void LoadCustomBackground();
	void DrawCustomBackground();

	inline std::shared_ptr<GSTexture> s_custom_background_texture;
	inline std::string s_custom_background_path;
	inline bool s_custom_background_enabled = false;

	//////////////////////////////////////////////////////////////////////////
	// Achievements
	//////////////////////////////////////////////////////////////////////////
	void SwitchToAchievementsWindow();
	void SwitchToLeaderboardsWindow();
	void DrawAchievementsLoginWindow();


	//////////////////////////////////////////////////////////////////////////
	// Settings
	//////////////////////////////////////////////////////////////////////////
	static constexpr double INPUT_BINDING_TIMEOUT_SECONDS = 5.0;
	static constexpr u32 NUM_MEMORY_CARD_PORTS = 2;

	void SwitchToSettings();
	void SwitchToGameSettings();
	void SwitchToGameSettings(const std::string& path);
	void SwitchToGameSettings(const GameList::Entry* entry);
	void SwitchToGameSettings(const std::string_view serial, u32 crc);
	void DrawSettingsWindow();
	void DrawSummarySettingsPage();
	void DrawInterfaceSettingsPage();
	void DrawBIOSSettingsPage();
	void DrawEmulationSettingsPage();
	void DrawGraphicsSettingsPage(SettingsInterface* bsi, bool show_advanced_settings);
	void DrawAudioSettingsPage();
	void DrawMemoryCardSettingsPage();
	void DrawNetworkHDDSettingsPage();
	void DrawFoldersSettingsPage();
	void DrawAchievementsSettingsPage(std::unique_lock<std::mutex>& settings_lock);
	void DrawControllerSettingsPage();
	void DrawHotkeySettingsPage();
	void DrawAdvancedSettingsPage();
	void DrawPatchesOrCheatsSettingsPage(bool cheats);
	void DrawGameFixesSettingsPage();

	bool IsEditingGameSettings(SettingsInterface* bsi);
	SettingsInterface* GetEditingSettingsInterface();
	SettingsInterface* GetEditingSettingsInterface(bool game_settings);
	bool ShouldShowAdvancedSettings(SettingsInterface* bsi);
	void SetSettingsChanged(SettingsInterface* bsi);
	bool GetEffectiveBoolSetting(SettingsInterface* bsi, const char* section, const char* key, bool default_value);
	s32 GetEffectiveIntSetting(SettingsInterface* bsi, const char* section, const char* key, s32 default_value);
	void DoCopyGameSettings();
	void DoClearGameSettings();
	void ResetControllerSettings();
	void DoLoadInputProfile();
	void DoSaveInputProfile();
	void DoSaveInputProfile(const std::string& name);
	void DoResetSettings();

	bool DrawToggleSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		bool default_value, bool enabled = true, bool allow_tristate = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT,
		std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	void DrawIntListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		int default_value, const char* const* options, size_t option_count, bool translate_options, int option_offset = 0,
		bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font,
		std::pair<ImFont*, float> summary_font = g_medium_font);
	void DrawIntRangeSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		int default_value, int min_value, int max_value, const char* format = "%d", bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	void DrawIntSpinBoxSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		int default_value, int min_value, int max_value, int step_value, const char* format = "%d", bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	void DrawFloatRangeSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		float default_value, float min_value, float max_value, const char* format = "%f", float multiplier = 1.0f, bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	void DrawFloatSpinBoxSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
		const char* key, float default_value, float min_value, float max_value, float step_value, float multiplier,
		const char* format = "%f", bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT,
		std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	void DrawIntRectSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
		const char* left_key, int default_left, const char* top_key, int default_top, const char* right_key, int default_right,
		const char* bottom_key, int default_bottom, int min_value, int max_value, int step_value, const char* format = "%d",
		bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font,
		std::pair<ImFont*, float> summary_font = g_medium_font);
	void DrawStringListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		const char* default_value, const char* const* options, const char* const* option_values, size_t option_count,
		bool translate_options, bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font,
		std::pair<ImFont*, float> summary_font = g_medium_font, const char* translation_ctx = "FullscreenUI");
	void DrawStringListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		const char* default_value, SettingInfo::GetOptionsCallback options_callback, bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	void DrawIPAddressSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		const char* default_value, bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT,
		std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font,
		IPAddressType ip_type = IPAddressType::Other);
	void DrawFloatListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		float default_value, const char* const* options, const float* option_values, size_t option_count, bool translate_options,
		bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font,
		std::pair<ImFont*, float> summary_font = g_medium_font);
	template <typename DataType, typename SizeType>
	void DrawEnumSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
		const char* key, DataType default_value,
		std::optional<DataType> (*from_string_function)(const char* str),
		const char* (*to_string_function)(DataType value),
		const char* (*to_display_string_function)(DataType value), SizeType option_count,
		bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT,
		std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	void DrawFolderSetting(SettingsInterface* bsi, const char* title, const char* section, const char* key,
		const std::string& runtime_var, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font,
		std::pair<ImFont*, float> summary_font = g_medium_font);
	void DrawPathSetting(SettingsInterface* bsi, const char* title, const char* section, const char* key, const char* default_value,
		bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font,
		std::pair<ImFont*, float> summary_font = g_medium_font);
	void DrawClampingModeSetting(SettingsInterface* bsi, const char* title, const char* summary, int vunum);
	void PopulateGraphicsAdapterList();
	void PopulateGameListDirectoryCache(SettingsInterface* si);
	void PopulatePatchesAndCheatsList(const std::string_view serial, u32 crc);
	void BeginInputBinding(SettingsInterface* bsi, InputBindingInfo::Type type, const std::string_view section,
		const std::string_view key, const std::string_view display_name);
	void DrawInputBindingWindow();
	void DrawInputBindingButton(SettingsInterface* bsi, InputBindingInfo::Type type, const char* section, const char* name, const char* display_name, const char* icon_name, bool show_type = true);
	void ClearInputBindingVariables();
	void StartAutomaticBinding(u32 port);
	void DrawSettingInfoSetting(SettingsInterface* bsi, const char* section, const char* key, const SettingInfo& si,
		const char* translation_ctx);
	void OpenMemoryCardCreateDialog();
	void DoCreateMemoryCard(std::string name, MemoryCardType type, MemoryCardFileType file_type, bool use_ntfs_compression = false);

	inline SettingsPage s_settings_page = SettingsPage::Interface;
	inline std::unique_ptr<INISettingsInterface> s_game_settings_interface;
	inline std::unique_ptr<GameList::Entry> s_game_settings_entry;
	inline std::vector<std::pair<std::string, bool>> s_game_list_directories_cache;
	inline std::vector<GSAdapterInfo> s_graphics_adapter_list_cache;
	inline std::vector<Patch::PatchInfo> s_game_patch_list;
	inline std::vector<std::string> s_enabled_game_patch_cache;
	inline std::vector<Patch::PatchInfo> s_game_cheats_list;
	inline std::vector<std::string> s_enabled_game_cheat_cache;
	inline u32 s_game_cheat_unlabelled_count = 0;
	inline std::vector<const HotkeyInfo*> s_hotkey_list_cache;
	inline std::atomic_bool s_settings_changed{false};
	inline std::atomic_bool s_game_settings_changed{false};
	inline InputBindingInfo::Type s_input_binding_type = InputBindingInfo::Type::Unknown;
	inline std::string s_input_binding_section;
	inline std::string s_input_binding_key;
	inline std::string s_input_binding_display_name;
	inline std::vector<InputBindingKey> s_input_binding_new_bindings;
	inline std::vector<std::pair<InputBindingKey, std::pair<float, float>>> s_input_binding_value_ranges;
	inline Common::Timer s_input_binding_timer;

} // namespace FullscreenUI
