// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
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
#include "DEV9/ATA/HddCreate.h"
#include "DEV9/pcap_io.h"
#include "DEV9/sockets.h"
#ifdef _WIN32
#include "DEV9/Win32/tap.h"
#endif

#include "common/Console.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Image.h"
#include "common/Path.h"
#include "common/SettingsInterface.h"
#include "common/SettingsWrapper.h"
#include "common/SmallString.h"
#include "common/StringUtil.h"
#include "common/Threading.h"
#include "common/Timer.h"

#include "SIO/Memcard/MemoryCardFile.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Sio.h"

#include "IconsFontAwesome6.h"
#include "IconsPromptFont.h"
#include "imgui.h"
#include "imgui_internal.h"

#include "fmt/chrono.h"
#include "fmt/format.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <bitset>
#include <chrono>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#define TR_CONTEXT "FullscreenUI"

namespace
{
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
} // namespace

#define FSUI_ICONSTR(icon, str) IconStackString<256>(icon, str).c_str()
#define FSUI_ICONSTR_S(icon, str, suffix) IconStackString<256>(icon, str, suffix).c_str()
#define FSUI_STR(str) Host::TranslateToString(TR_CONTEXT, str)
#define FSUI_CSTR(str) Host::TranslateToCString(TR_CONTEXT, str)
#define FSUI_VSTR(str) Host::TranslateToStringView(TR_CONTEXT, str)
#define FSUI_FSTR(str) fmt::runtime(Host::TranslateToStringView(TR_CONTEXT, str))
#define FSUI_NSTR(str) str

using ImGuiFullscreen::FocusResetType;
using ImGuiFullscreen::g_large_font;
using ImGuiFullscreen::g_layout_padding_left;
using ImGuiFullscreen::g_layout_padding_top;
using ImGuiFullscreen::g_medium_font;
using ImGuiFullscreen::LAYOUT_FOOTER_HEIGHT;
using ImGuiFullscreen::LAYOUT_LARGE_FONT_SIZE;
using ImGuiFullscreen::LAYOUT_MEDIUM_FONT_SIZE;
using ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT;
using ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY;
using ImGuiFullscreen::LAYOUT_MENU_BUTTON_X_PADDING;
using ImGuiFullscreen::LAYOUT_MENU_BUTTON_Y_PADDING;
using ImGuiFullscreen::LAYOUT_SCREEN_HEIGHT;
using ImGuiFullscreen::LAYOUT_SCREEN_WIDTH;
using ImGuiFullscreen::SvgScaling;
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
using ImGuiFullscreen::ForceKeyNavEnabled;
using ImGuiFullscreen::GetCachedSvgTexture;
using ImGuiFullscreen::GetCachedSvgTextureAsync;
using ImGuiFullscreen::GetCachedTexture;
using ImGuiFullscreen::GetCachedTextureAsync;
using ImGuiFullscreen::GetPlaceholderTexture;
using ImGuiFullscreen::GetQueuedFocusResetType;
using ImGuiFullscreen::HorizontalMenuItem;
using ImGuiFullscreen::HorizontalMenuSvgItem;
using ImGuiFullscreen::IsFocusResetQueued;
using ImGuiFullscreen::IsGamepadInputSource;
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
using ImGuiFullscreen::InputFilterType;
using ImGuiFullscreen::QueueResetFocus;
using ImGuiFullscreen::ResetFocusHere;
using ImGuiFullscreen::RightAlignNavButtons;
using ImGuiFullscreen::SetFullscreenFooterText;
using ImGuiFullscreen::ShowToast;
using ImGuiFullscreen::ThreeWayToggleButton;
using ImGuiFullscreen::ToggleButton;
using ImGuiFullscreen::WantsToCloseMenu;

namespace FullscreenUI
{

	class HddCreateInProgress : public HddCreate
	{
	private:
		std::string m_dialogId;
		std::atomic_bool m_completed{false};
		std::atomic_bool m_success{false};
		int m_reqMiB = 0;
		std::atomic_bool m_dialogClosed{false}; // Check if dialog was already closed

		static std::vector<std::shared_ptr<HddCreateInProgress>> s_activeOperations;
		static std::mutex s_operationsMutex;
		static std::atomic_int s_nextOperationId;

	public:
		HddCreateInProgress(const std::string& dialogId)
			: m_dialogId(dialogId)
		{
		}

		~HddCreateInProgress()
		{
			SafeCloseDialog();
		}

		void SafeCloseDialog()
		{
			bool expected = false;
			if (m_dialogClosed.compare_exchange_strong(expected, true))
			{
				ImGuiFullscreen::CloseProgressDialog(m_dialogId.c_str());
			}
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
					Host::RunOnCPUThread([filePath]() {
						ShowToast(
							fmt::format("{} HDD Creation Failed", ICON_FA_TRIANGLE_EXCLAMATION),
							fmt::format("Failed to delete existing HDD image file '{}'. Please check file permissions and try again.", Path::GetFileName(filePath)),
							5.0f);
					});
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
					Host::RunOnCPUThread([size_gb = static_cast<int>(instance->neededSize / static_cast<u64>(_1gb))]() {
						ShowToast(
							ICON_FA_CIRCLE_CHECK,
							fmt::format("HDD image ({} GB) created successfully.", size_gb),
							3.0f);
					});
				else
					Host::RunOnCPUThread([]() {
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
			{
				operation->SetCanceled();
				operation->SafeCloseDialog();
			}
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

		virtual void SetError() override
		{
			SafeCloseDialog();
			HddCreate::SetError();
		}

		virtual void Cleanup() override
		{
			SafeCloseDialog();
			m_success.store(!errored, std::memory_order_release);
			m_completed.store(true, std::memory_order_release);
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
			Host::RunOnCPUThread([min_size, max_size]() {
				ShowToast(std::string(), fmt::format("Invalid HDD size. Size must be between {} and {} GB.", min_size, max_size).c_str());
			});
			return false;
		}

		return HddCreateInProgress::StartCreation(filePath, sizeInGB, use48BitLBA);
	}

	void CancelAllHddOperations()
	{
		HddCreateInProgress::CancelAllOperations();
	}

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
	static void UpdateGameDetails(std::string path, std::string serial, std::string title, u32 disc_crc, u32 crc);
	static bool AreAnyDialogsOpen();
	static void PauseForMenuOpen(bool set_pause_menu_open);
	static void ClosePauseMenu();
	static void OpenPauseSubMenu(PauseSubMenu submenu);
	static void DrawLandingTemplate(ImVec2* menu_pos, ImVec2* menu_size);
	static void DrawLandingWindow();
	static void DrawStartGameWindow();
	static void DrawExitWindow();
	static void DrawPauseMenu(MainWindowType type);
	static void ExitFullscreenAndOpenURL(const std::string_view url);
	static void CopyTextToClipboard(std::string title, const std::string_view text);
	static void DrawAboutWindow();
	static void OpenAboutWindow();
	static void GetStandardSelectionFooterText(SmallStringBase& dest, bool back_instead_of_cancel);
	static void ApplyLayoutSettings(const SettingsInterface* bsi = nullptr);

	void DrawSvgTexture(GSTexture* padded_texture, ImVec2 unpadded_size);
	void DrawCachedSvgTexture(const std::string& path, ImVec2 size, SvgScaling mode);
	void DrawCachedSvgTextureAsync(const std::string& path, ImVec2 size, SvgScaling mode);
	void DrawListSvgTexture(ImDrawList* drawList, GSTexture* padded_texture, const ImVec2& p_min, const ImVec2& p_unpadded_max);

	static MainWindowType s_current_main_window = MainWindowType::None;
	static PauseSubMenu s_current_pause_submenu = PauseSubMenu::None;
	static bool s_initialized = false;
	static bool s_tried_to_initialize = false;
	static bool s_pause_menu_was_open = false;
	static bool s_was_paused_on_quick_menu_open = false;
	static bool s_about_window_open = false;

	// achievements login dialog state
	static bool s_achievements_login_open = false;
	static bool s_achievements_login_logging_in = false;
	static char s_achievements_login_username[256] = {};
	static char s_achievements_login_password[256] = {};
	static Achievements::LoginRequestReason s_achievements_login_reason = Achievements::LoginRequestReason::UserInitiated;

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
	static bool LoadSvgResources();
	static void DestroyResources();

	static std::array<std::shared_ptr<GSTexture>, static_cast<u32>(GameDatabaseSchema::Compatibility::Perfect)>
		s_game_compatibility_textures;
	static std::shared_ptr<GSTexture> s_banner_texture;
	static std::vector<std::unique_ptr<GSTexture>> s_cleanup_textures;

	//////////////////////////////////////////////////////////////////////////
	// Landing
	//////////////////////////////////////////////////////////////////////////
	static void SwitchToLanding();
	static ImGuiFullscreen::FileSelectorFilters GetOpenFileFilters();
	static ImGuiFullscreen::FileSelectorFilters GetDiscImageFilters();
	static ImGuiFullscreen::FileSelectorFilters GetAudioFileFilters();
	static ImGuiFullscreen::FileSelectorFilters GetImageFileFilters();
	static void DoVMInitialize(const VMBootParameters& boot_params, bool switch_to_landing_on_failure);
	static void DoStartPath(
		const std::string& path, std::optional<s32> state_index = std::nullopt, std::optional<bool> fast_boot = std::nullopt);
	static void DoStartFile();
	static void DoStartBIOS();
	static void DoStartDisc(const std::string& drive);
	static void DoStartDisc();
	static void DoToggleFrameLimit();
	static void DoToggleSoftwareRenderer();
	static void RequestShutdown(bool save_state);
	static void DoShutdown(bool save_state);
	static void RequestReset();
	static void DoReset();
	static void DoChangeDiscFromFile();
	static void RequestChangeDisc();
	static void DoRequestExit();
	static void DoDesktopMode();
	static void DoToggleFullscreen();

	static void ConfirmShutdownIfMemcardBusy(std::function<void(bool)> callback);

	static bool ShouldDefaultToGameList();

	//////////////////////////////////////////////////////////////////////////
	// Settings
	//////////////////////////////////////////////////////////////////////////

	static constexpr double INPUT_BINDING_TIMEOUT_SECONDS = 5.0;
	static constexpr u32 NUM_MEMORY_CARD_PORTS = 2;

	static void SwitchToSettings();
	static void SwitchToGameSettings();
	static void SwitchToGameSettings(const std::string& path);
	static void SwitchToGameSettings(const GameList::Entry* entry);
	static void SwitchToGameSettings(const std::string_view serial, u32 crc);
	static void DrawSettingsWindow();
	static void DrawSummarySettingsPage();
	static void DrawInterfaceSettingsPage();
	static void DrawBIOSSettingsPage();
	static void DrawEmulationSettingsPage();
	static void DrawGraphicsSettingsPage(SettingsInterface* bsi, bool show_advanced_settings);
	static void DrawAudioSettingsPage();
	static void DrawMemoryCardSettingsPage();
	static void DrawNetworkHDDSettingsPage();
	static void DrawFoldersSettingsPage();
	static void DrawAchievementsSettingsPage(std::unique_lock<std::mutex>& settings_lock);
	static void DrawControllerSettingsPage();
	static void DrawHotkeySettingsPage();
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
	static void ResetControllerSettings();
	static void DoLoadInputProfile();
	static void DoSaveInputProfile();
	static void DoSaveInputProfile(const std::string& name);
	static void DoResetSettings();

	static bool DrawToggleSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		bool default_value, bool enabled = true, bool allow_tristate = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT,
		std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	static void DrawIntListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		int default_value, const char* const* options, size_t option_count, bool translate_options, int option_offset = 0,
		bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font,
		std::pair<ImFont*, float> summary_font = g_medium_font);
	static void DrawIntRangeSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		int default_value, int min_value, int max_value, const char* format = "%d", bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	static void DrawIntSpinBoxSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		int default_value, int min_value, int max_value, int step_value, const char* format = "%d", bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	static void DrawFloatRangeSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		float default_value, float min_value, float max_value, const char* format = "%f", float multiplier = 1.0f, bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	static void DrawFloatSpinBoxSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
		const char* key, float default_value, float min_value, float max_value, float step_value, float multiplier,
		const char* format = "%f", bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT,
		std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	static void DrawIntRectSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
		const char* left_key, int default_left, const char* top_key, int default_top, const char* right_key, int default_right,
		const char* bottom_key, int default_bottom, int min_value, int max_value, int step_value, const char* format = "%d",
		bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font,
		std::pair<ImFont*, float> summary_font = g_medium_font);
	static void DrawStringListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		const char* default_value, const char* const* options, const char* const* option_values, size_t option_count,
		bool translate_options, bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font,
		std::pair<ImFont*, float> summary_font = g_medium_font, const char* translation_ctx = TR_CONTEXT);
	static void DrawStringListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		const char* default_value, SettingInfo::GetOptionsCallback options_callback, bool enabled = true,
		float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	static void DrawIPAddressSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		const char* default_value, bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT,
		std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font,
		IPAddressType ip_type = IPAddressType::Other);
	static void DrawFloatListSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section, const char* key,
		float default_value, const char* const* options, const float* option_values, size_t option_count, bool translate_options,
		bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font,
		std::pair<ImFont*, float> summary_font = g_medium_font);
	template <typename DataType, typename SizeType>
	static void DrawEnumSetting(SettingsInterface* bsi, const char* title, const char* summary, const char* section,
		const char* key, DataType default_value,
		std::optional<DataType> (*from_string_function)(const char* str),
		const char* (*to_string_function)(DataType value),
		const char* (*to_display_string_function)(DataType value), SizeType option_count,
		bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT,
		std::pair<ImFont*, float> font = g_large_font, std::pair<ImFont*, float> summary_font = g_medium_font);
	static void DrawFolderSetting(SettingsInterface* bsi, const char* title, const char* section, const char* key,
		const std::string& runtime_var, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font,
		std::pair<ImFont*, float> summary_font = g_medium_font);
	static void DrawPathSetting(SettingsInterface* bsi, const char* title, const char* section, const char* key, const char* default_value,
		bool enabled = true, float height = ImGuiFullscreen::LAYOUT_MENU_BUTTON_HEIGHT, std::pair<ImFont*, float> font = g_large_font,
		std::pair<ImFont*, float> summary_font = g_medium_font);
	static void DrawClampingModeSetting(SettingsInterface* bsi, const char* title, const char* summary, int vunum);
	static void PopulateGraphicsAdapterList();
	static void PopulateGameListDirectoryCache(SettingsInterface* si);
	static void PopulatePatchesAndCheatsList(const std::string_view serial, u32 crc);
	static void BeginInputBinding(SettingsInterface* bsi, InputBindingInfo::Type type, const std::string_view section,
		const std::string_view key, const std::string_view display_name);
	static void DrawInputBindingWindow();
	static void DrawInputBindingButton(SettingsInterface* bsi, InputBindingInfo::Type type, const char* section, const char* name, const char* display_name, const char* icon_name, bool show_type = true);
	static void ClearInputBindingVariables();
	static void StartAutomaticBinding(u32 port);
	static void DrawSettingInfoSetting(SettingsInterface* bsi, const char* section, const char* key, const SettingInfo& si,
		const char* translation_ctx);
	static void OpenMemoryCardCreateDialog();
	static void DoCreateMemoryCard(std::string name, MemoryCardType type, MemoryCardFileType file_type, bool use_ntfs_compression = false);

	static SettingsPage s_settings_page = SettingsPage::Interface;
	static std::unique_ptr<INISettingsInterface> s_game_settings_interface;
	static std::unique_ptr<GameList::Entry> s_game_settings_entry;
	static std::vector<std::pair<std::string, bool>> s_game_list_directories_cache;
	static std::vector<GSAdapterInfo> s_graphics_adapter_list_cache;
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
		SaveStateListEntry* li, const std::string& title, const std::string& serial, u32 crc, s32 slot, bool backup = false);
	static void ClearSaveStateEntryList();
	static u32 PopulateSaveStateListEntries(const std::string& title, const std::string& serial, u32 crc);
	static bool OpenLoadStateSelectorForGame(const std::string& game_path);
	static bool OpenSaveStateSelector(bool is_loading);
	static void CloseSaveStateSelector();
	static void DrawSaveStateSelector(bool is_loading);
	static bool OpenLoadStateSelectorForGameResume(const GameList::Entry* entry);
	static void DrawResumeStateSelector();
	static void DoLoadState(std::string path, std::optional<s32> slot, bool backup);
	static void DoSaveState(s32 slot);

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
	static void DrawGameListSettingsWindow();
	static void SwitchToGameList();
	static void PopulateGameListEntryList();
	static GSTexture* GetTextureForGameListEntryType(GameList::EntryType type, const ImVec2& size, SvgScaling mode = SvgScaling::Stretch);
	static GSTexture* GetGameListCover(const GameList::Entry* entry);
	static void DrawGameCover(const GameList::Entry* entry, const ImVec2& size);
	static void DrawGameCover(const GameList::Entry* entry, ImDrawList* draw_list, const ImVec2& min, const ImVec2& max);
	// For when we have no GameList entry
	static void DrawFallbackCover(const ImVec2& size);
	static void DrawFallbackCover(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max);

	// Lazily populated cover images.
	static std::unordered_map<std::string, std::string> s_cover_image_map;
	static std::vector<const GameList::Entry*> s_game_list_sorted_entries;
	static GameListView s_game_list_view = GameListView::Grid;

	//////////////////////////////////////////////////////////////////////////
	// Background
	//////////////////////////////////////////////////////////////////////////
	static void LoadCustomBackground();
	static void DrawCustomBackground();

	static std::shared_ptr<GSTexture> s_custom_background_texture;
	static std::string s_custom_background_path;
	static bool s_custom_background_enabled = false;

	//////////////////////////////////////////////////////////////////////////
	// Achievements
	//////////////////////////////////////////////////////////////////////////
	static void SwitchToAchievementsWindow();
	static void SwitchToLeaderboardsWindow();
	static void DrawAchievementsLoginWindow();
} // namespace FullscreenUI

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
	std::string current_path = Host::GetBaseStringSettingValue("UI", "GameListBackgroundPath");
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
		s_current_main_window == MainWindowType::Settings) && s_custom_background_enabled && s_custom_background_texture;

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
	std::string path = Host::GetBaseStringSettingValue("UI", "GameListBackgroundPath");

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

	const float opacity = Host::GetBaseFloatSettingValue("UI", "GameListBackgroundOpacity", 100.0f) / 100.0f;
	const std::string mode = Host::GetBaseStringSettingValue("UI", "GameListBackgroundMode", "fit");

	const float tex_width = static_cast<float>(s_custom_background_texture->GetWidth());
	const float tex_height = static_cast<float>(s_custom_background_texture->GetHeight());

	ImVec2 img_min, img_max;

	if (mode == "stretch")
	{
		// stretch to fill entire display (ignores aspect ratio)
		img_min = ImVec2(0.0f, 0.0f);
		img_max = display_size;
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

		img_min = ImVec2(offset_x, offset_y);
		img_max = ImVec2(offset_x + scaled_width, offset_y + scaled_height);
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

		img_min = ImVec2(offset_x, offset_y);
		img_max = ImVec2(offset_x + scaled_width, offset_y + scaled_height);
	}

	// Override the UIBackgroundColor that windows use
	// We need to make windows transparent so our background image shows through
	const ImVec4 transparent_bg = ImVec4(UIBackgroundColor.x, UIBackgroundColor.y, UIBackgroundColor.z, 0.0f);
	ImGuiFullscreen::UIBackgroundColor = transparent_bg;

	ImDrawList* bg_draw_list = ImGui::GetBackgroundDrawList();
	const ImU32 col = IM_COL32(255, 255, 255, static_cast<u8>(opacity * 255.0f));
	bg_draw_list->AddImage(reinterpret_cast<ImTextureID>(s_custom_background_texture->GetNativeHandle()),
		img_min, img_max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), col);
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
	if (ImGui::BeginPopupModal(title, &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
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
				names.push_back(std::move(name));
				options.emplace_back(std::move(display_name), false);
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
	constexpr auto sort_patches = [](Patch::PatchInfoList& list) {
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

	const bool using_custom_bg = s_custom_background_enabled && s_custom_background_texture;
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

	std::string background_path = bsi->GetStringValue("UI", "GameListBackgroundPath", "");
	const bool background_enabled = bsi->GetBoolValue("UI", "GameListBackgroundEnabled", false);

	std::string background_display = FSUI_STR("None");
	if (!background_path.empty() && background_enabled)
	{
		background_display = Path::GetFileName(background_path);
	}

	if (MenuButtonWithValue(FSUI_ICONSTR(ICON_FA_IMAGE, "Background Image"),
			FSUI_CSTR("Select a custom background image to use in Big Picture Mode menus."),
			background_display.c_str()))
	{
		OpenFileSelector(FSUI_ICONSTR(ICON_FA_IMAGE, "Select Background Image"), false,
			[](const std::string& path) {
				if (!path.empty())
				{
					{
						auto lock = Host::GetSettingsLock();
						SettingsInterface* bsi = GetEditingSettingsInterface(false);

						std::string relative_path = Path::MakeRelative(path, EmuFolders::DataRoot);
						bsi->SetStringValue("UI", "GameListBackgroundPath", relative_path.c_str());
						bsi->SetBoolValue("UI", "GameListBackgroundEnabled", true);
						SetSettingsChanged(bsi);
					}

					LoadCustomBackground();
				}
				CloseFileSelector();
			},
			GetImageFileFilters());
	}

	if (MenuButton(FSUI_ICONSTR(ICON_FA_XMARK, "Clear Background Image"),
			FSUI_CSTR("Removes the custom background image.")))
	{
		bsi->DeleteValue("UI", "GameListBackgroundPath");
		bsi->SetBoolValue("UI", "GameListBackgroundEnabled", false);
		SetSettingsChanged(bsi);

		s_custom_background_texture.reset();
		s_custom_background_path.clear();
		s_custom_background_enabled = false;
	}

	DrawIntRangeSetting(bsi, FSUI_ICONSTR(ICON_FA_DROPLET, "Background Opacity"),
		FSUI_CSTR("Sets the transparency of the custom background image."),
		"UI", "GameListBackgroundOpacity", 100, 0, 100, "%d%%");

	static constexpr const char* s_background_mode_names[] = {
		FSUI_NSTR("Fit"),
		FSUI_NSTR("Fill"),
		FSUI_NSTR("Stretch"),
	};
	static constexpr const char* s_background_mode_values[] = {
		"fit",
		"fill",
		"stretch",
	};
	DrawStringListSetting(bsi, FSUI_ICONSTR(ICON_FA_EXPAND, "Background Mode"),
		FSUI_CSTR("Select how to display the background image."),
		"UI", "GameListBackgroundMode", "fit", s_background_mode_names, s_background_mode_values, std::size(s_background_mode_names), true);

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
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
	};

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

	MenuHeading(FSUI_CSTR("Options and Patches"));
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
		FSUI_NSTR("Direct3D 11"),
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

					OpenFileSelector(FSUI_ICONSTR(ICON_FA_HARD_DRIVE, "Select HDD Image File"), false,
						[game_settings](const std::string& path) {
							if (path.empty())
								return;

							auto lock = Host::GetSettingsLock();
							SettingsInterface* bsi = GetEditingSettingsInterface(game_settings);
							bsi->SetStringValue("DEV9/Hdd", "HddFile", path.c_str());
							SetSettingsChanged(bsi);
							ShowToast(std::string(), fmt::format(FSUI_FSTR("Selected HDD image: {}"), Path::GetFileName(path)));
						}, {"*.raw", "*"}, EmuFolders::DataRoot);
				}
				else if (values[index] == "__create__")
				{
					CloseChoiceDialog();

					std::vector<std::pair<std::string, int>> size_options = {
						{"40 GB (Recommended)", 40},
						{"80 GB", 80},
						{"120 GB (Max LBA28)", 120},
						{"200 GB", 200},
						{"Custom...", -1}
					};

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
				if (ImGui::SliderInt("##value", &frequency, 0, 60, FSUI_CSTR("Toggle every %d frames"), ImGuiSliderFlags_NoInput))
				{
					if (frequency == 0)
						bsi->DeleteValue(section, freq_key.c_str());
					else
						bsi->SetIntValue(section, freq_key.c_str(), frequency);

					SetSettingsChanged(bsi);
				}

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

		DrawIntListSetting(bsi, FSUI_CSTR("Rounding Mode"),
			FSUI_CSTR("Determines how the results of floating-point operations are rounded. Some games need specific settings."),
			"EmuCore/CPU", "FPU.Roundmode", static_cast<int>(FPRoundMode::ChopZero), ee_rounding_mode_settings,
			std::size(ee_rounding_mode_settings), true);
		DrawIntListSetting(bsi, FSUI_CSTR("Division Rounding Mode"),
			FSUI_CSTR("Determines how the results of floating-point division is rounded. Some games need specific settings."),
			"EmuCore/CPU", "FPUDiv.Roundmode", static_cast<int>(FPRoundMode::Nearest),
			ee_rounding_mode_settings, std::size(ee_rounding_mode_settings), true);
		DrawClampingModeSetting(bsi, FSUI_CSTR("Clamping Mode"),
			FSUI_CSTR("Determines how out-of-range floating point numbers are handled. Some games need specific settings."), -1);

		DrawToggleSetting(bsi, FSUI_CSTR("Enable EE Recompiler"),
			FSUI_CSTR("Performs just-in-time binary translation of 64-bit MIPS-IV machine code to native code."), "EmuCore/CPU/Recompiler",
			"EnableEE", true);
		DrawToggleSetting(bsi, FSUI_CSTR("Enable EE Cache"), FSUI_CSTR("Enables simulation of the EE's cache. Slow."),
			"EmuCore/CPU/Recompiler", "EnableEECache", false);
		DrawToggleSetting(bsi, FSUI_CSTR("Enable INTC Spin Detection"),
			FSUI_CSTR("Huge speedup for some games, with almost no compatibility side effects."), "EmuCore/Speedhacks", "IntcStat", true);
		DrawToggleSetting(bsi, FSUI_CSTR("Enable Wait Loop Detection"),
			FSUI_CSTR("Moderate speedup for some games, with no known side effects."), "EmuCore/Speedhacks", "WaitLoop", true);
		DrawToggleSetting(bsi, FSUI_CSTR("Enable Fast Memory Access"),
			FSUI_CSTR("Uses backpatching to avoid register flushing on every memory access."), "EmuCore/CPU/Recompiler", "EnableFastmem",
			true);

		MenuHeading(FSUI_CSTR("Vector Units"));
		DrawIntListSetting(bsi, FSUI_CSTR("VU0 Rounding Mode"),
			FSUI_CSTR("Determines how the results of floating-point operations are rounded. Some games need specific settings."),
			"EmuCore/CPU", "VU0.Roundmode", static_cast<int>(FPRoundMode::ChopZero),
			ee_rounding_mode_settings, std::size(ee_rounding_mode_settings), true);
		DrawClampingModeSetting(bsi, FSUI_CSTR("VU0 Clamping Mode"),
			FSUI_CSTR("Determines how out-of-range floating point numbers are handled. Some games need specific settings."), 0);
		DrawIntListSetting(bsi, FSUI_CSTR("VU1 Rounding Mode"),
			FSUI_CSTR("Determines how the results of floating-point operations are rounded. Some games need specific settings."),
			"EmuCore/CPU", "VU1.Roundmode", static_cast<int>(FPRoundMode::ChopZero),
			ee_rounding_mode_settings, std::size(ee_rounding_mode_settings), true);
		DrawClampingModeSetting(bsi, FSUI_CSTR("VU1 Clamping Mode"),
			FSUI_CSTR("Determines how out-of-range floating point numbers are handled. Some games need specific settings."), 1);
		DrawToggleSetting(bsi, FSUI_CSTR("Enable VU0 Recompiler (Micro Mode)"),
			FSUI_CSTR("New Vector Unit recompiler with much improved compatibility. Recommended."), "EmuCore/CPU/Recompiler", "EnableVU0",
			true);
		DrawToggleSetting(bsi, FSUI_CSTR("Enable VU1 Recompiler"),
			FSUI_CSTR("New Vector Unit recompiler with much improved compatibility. Recommended."), "EmuCore/CPU/Recompiler", "EnableVU1",
			true);
		DrawToggleSetting(bsi, FSUI_CSTR("Enable VU Flag Optimization"),
			FSUI_CSTR("Good speedup and high compatibility, may cause graphical errors."), "EmuCore/Speedhacks", "vuFlagHack", true);
		DrawToggleSetting(bsi, FSUI_CSTR("Enable Instant VU1"),
			FSUI_CSTR("Runs VU1 instantly. Provides a modest speed improvement in most games. Safe for most games, but a few games may exhibit graphical errors."),
			"EmuCore/Speedhacks", "vu1Instant", true);

		MenuHeading(FSUI_CSTR("I/O Processor"));
		DrawToggleSetting(bsi, FSUI_CSTR("Enable IOP Recompiler"),
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

	const Patch::PatchInfoList& patch_list = cheats ? s_game_cheats_list : s_game_patch_list;
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
	if (ImGui::BeginPopupModal(FSUI_CSTR("Load Resume State"), &is_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
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

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, LayoutScale(6.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(12.0f, 10.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));

		if (s_achievements_login_logging_in)
			ImGui::BeginDisabled();

		ImGui::SetNextItemWidth(content_width);
		ImGui::InputTextWithHint("##username", FSUI_CSTR("Username"), s_achievements_login_username, sizeof(s_achievements_login_username));

		ImGui::Spacing();

		ImGui::SetNextItemWidth(content_width);
		ImGui::InputTextWithHint("##password", FSUI_CSTR("Password"), s_achievements_login_password, sizeof(s_achievements_login_password), ImGuiInputTextFlags_Password);

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);

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

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, LayoutScale(6.0f));

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Translation String Area
// To avoid having to type T_RANSLATE("FullscreenUI", ...) everywhere, we use the shorter macros at the top
// of the file, then preprocess and generate a bunch of noops here to define the strings. Sadly that means
// the view in Linguist is gonna suck, but you can search the file for the string for more context.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
// TRANSLATION-STRING-AREA-BEGIN
TRANSLATE_NOOP("FullscreenUI", "Error");
TRANSLATE_NOOP("FullscreenUI", "Could not find any CD/DVD-ROM devices. Please ensure you have a drive connected and sufficient permissions to access it.");
TRANSLATE_NOOP("FullscreenUI", "Your memory card is still saving data.\n\nWARNING: Shutting down now can IRREVERSIBLY CORRUPT YOUR MEMORY CARD.\n\nYou are strongly advised to select 'No' and let the save finish.\n\nDo you want to shutdown anyway and IRREVERSIBLY CORRUPT YOUR MEMORY CARD?");
TRANSLATE_NOOP("FullscreenUI", "Use Global Setting");
TRANSLATE_NOOP("FullscreenUI", "Automatic binding failed, no devices are available.");
TRANSLATE_NOOP("FullscreenUI", "Game title copied to clipboard.");
TRANSLATE_NOOP("FullscreenUI", "Game serial copied to clipboard.");
TRANSLATE_NOOP("FullscreenUI", "Game CRC copied to clipboard.");
TRANSLATE_NOOP("FullscreenUI", "Game type copied to clipboard.");
TRANSLATE_NOOP("FullscreenUI", "Game region copied to clipboard.");
TRANSLATE_NOOP("FullscreenUI", "Game compatibility copied to clipboard.");
TRANSLATE_NOOP("FullscreenUI", "Game path copied to clipboard.");
TRANSLATE_NOOP("FullscreenUI", "None");
TRANSLATE_NOOP("FullscreenUI", "Automatic");
TRANSLATE_NOOP("FullscreenUI", "Browse...");
TRANSLATE_NOOP("FullscreenUI", "Create New...");
TRANSLATE_NOOP("FullscreenUI", "Enter custom HDD size in gigabytes (40–2000):");
TRANSLATE_NOOP("FullscreenUI", "Invalid size. Please enter a number between 40 and 2000.");
TRANSLATE_NOOP("FullscreenUI", "HDD size must be between 40 GB and 2000 GB.");
TRANSLATE_NOOP("FullscreenUI", "Enter the name for the new memory card.");
TRANSLATE_NOOP("FullscreenUI", "Memory card name cannot be empty.");
TRANSLATE_NOOP("FullscreenUI", "PS2 (8MB)");
TRANSLATE_NOOP("FullscreenUI", "PS2 (16MB)");
TRANSLATE_NOOP("FullscreenUI", "PS2 (32MB)");
TRANSLATE_NOOP("FullscreenUI", "PS2 (64MB)");
TRANSLATE_NOOP("FullscreenUI", "PS1 (128KB)");
TRANSLATE_NOOP("FullscreenUI", "Folder");
TRANSLATE_NOOP("FullscreenUI", "Yes - Enable NTFS compression");
TRANSLATE_NOOP("FullscreenUI", "No - Disable NTFS compression");
TRANSLATE_NOOP("FullscreenUI", "Failed to create the Memory Card, the log may contain more information.");
TRANSLATE_NOOP("FullscreenUI", "Are you sure you want to restore the default controller configuration?\n\nAll shared bindings and configuration will be lost, but your input profiles will remain.\n\nYou cannot undo this action.");
TRANSLATE_NOOP("FullscreenUI", "Controller settings reset to default.");
TRANSLATE_NOOP("FullscreenUI", "No input profiles available.");
TRANSLATE_NOOP("FullscreenUI", "Custom input profiles are used to override the Shared input profile for specific games.\n\nTo apply a custom input profile to a game, go to its Game Properties, then change the 'Input Profile' on the Summary tab.\n\nEnter the name for the new input profile:");
TRANSLATE_NOOP("FullscreenUI", "Are you sure you want to restore the default settings? Any preferences will be lost.");
TRANSLATE_NOOP("FullscreenUI", "Settings reset to defaults.");
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
TRANSLATE_NOOP("FullscreenUI", "-");
TRANSLATE_NOOP("FullscreenUI", "No Binding");
TRANSLATE_NOOP("FullscreenUI", "Setting %s binding %s.");
TRANSLATE_NOOP("FullscreenUI", "Push a controller button or axis now.");
TRANSLATE_NOOP("FullscreenUI", "Timing out in %.0f seconds...");
TRANSLATE_NOOP("FullscreenUI", "Unknown");
TRANSLATE_NOOP("FullscreenUI", "OK");
TRANSLATE_NOOP("FullscreenUI", "Enter the DNS server address");
TRANSLATE_NOOP("FullscreenUI", "Enter the Gateway address");
TRANSLATE_NOOP("FullscreenUI", "Enter the Subnet Mask");
TRANSLATE_NOOP("FullscreenUI", "Enter the PS2 IP address");
TRANSLATE_NOOP("FullscreenUI", "Enter the IP address");
TRANSLATE_NOOP("FullscreenUI", "Select Device");
TRANSLATE_NOOP("FullscreenUI", "Details");
TRANSLATE_NOOP("FullscreenUI", "The selected input profile will be used for this game.");
TRANSLATE_NOOP("FullscreenUI", "Shared");
TRANSLATE_NOOP("FullscreenUI", "Input Profile");
TRANSLATE_NOOP("FullscreenUI", "Options");
TRANSLATE_NOOP("FullscreenUI", "Copies the current global settings to this game.");
TRANSLATE_NOOP("FullscreenUI", "Clears all settings set for this game.");
TRANSLATE_NOOP("FullscreenUI", "Appearance");
TRANSLATE_NOOP("FullscreenUI", "Selects the color style to be used for Big Picture Mode.");
TRANSLATE_NOOP("FullscreenUI", "When Big Picture mode is started, the game list will be displayed instead of the main menu.");
TRANSLATE_NOOP("FullscreenUI", "Show a save state selector UI when switching slots instead of showing a notification bubble.");
TRANSLATE_NOOP("FullscreenUI", "Background");
TRANSLATE_NOOP("FullscreenUI", "Select a custom background image to use in Big Picture Mode menus.");
TRANSLATE_NOOP("FullscreenUI", "Removes the custom background image.");
TRANSLATE_NOOP("FullscreenUI", "Sets the transparency of the custom background image.");
TRANSLATE_NOOP("FullscreenUI", "Select how to display the background image.");
TRANSLATE_NOOP("FullscreenUI", "Behaviour");
TRANSLATE_NOOP("FullscreenUI", "Prevents the screen saver from activating and the host from sleeping while emulation is running.");
TRANSLATE_NOOP("FullscreenUI", "Pauses the emulator when a game is started.");
TRANSLATE_NOOP("FullscreenUI", "Pauses the emulator when you minimize the window or switch to another application, and unpauses when you switch back.");
TRANSLATE_NOOP("FullscreenUI", "Pauses the emulator when a controller with bindings is disconnected.");
TRANSLATE_NOOP("FullscreenUI", "Pauses the emulator when you open the quick menu, and unpauses when you close it.");
TRANSLATE_NOOP("FullscreenUI", "Determines whether a prompt will be displayed to confirm shutting down the emulator/game when the hotkey is pressed.");
TRANSLATE_NOOP("FullscreenUI", "Automatically saves the emulator state when powering down or exiting. You can then resume directly from where you left off next time.");
TRANSLATE_NOOP("FullscreenUI", "Creates a backup copy of a save state if it already exists when the save is created. The backup copy has a .backup suffix");
TRANSLATE_NOOP("FullscreenUI", "Integration");
TRANSLATE_NOOP("FullscreenUI", "Shows the game you are currently playing as part of your profile on Discord.");
TRANSLATE_NOOP("FullscreenUI", "Game Display");
TRANSLATE_NOOP("FullscreenUI", "Automatically switches to fullscreen mode when a game is started.");
TRANSLATE_NOOP("FullscreenUI", "Switches between full screen and windowed when the window is double-clicked.");
TRANSLATE_NOOP("FullscreenUI", "Hides the mouse pointer/cursor when the emulator is in fullscreen mode.");
TRANSLATE_NOOP("FullscreenUI", "On-Screen Display");
TRANSLATE_NOOP("FullscreenUI", "Determines how large the on-screen messages and monitors are.");
TRANSLATE_NOOP("FullscreenUI", "%d%%");
TRANSLATE_NOOP("FullscreenUI", "Determines where on-screen display messages are positioned.");
TRANSLATE_NOOP("FullscreenUI", "Determines where performance statistics are positioned.");
TRANSLATE_NOOP("FullscreenUI", "Shows the current PCSX2 version.");
TRANSLATE_NOOP("FullscreenUI", "Shows the current emulation speed of the system as a percentage.");
TRANSLATE_NOOP("FullscreenUI", "Shows the number of internal video frames displayed per second by the system.");
TRANSLATE_NOOP("FullscreenUI", "Shows the number of Vsyncs performed per second by the system.");
TRANSLATE_NOOP("FullscreenUI", "Shows the internal resolution of the game.");
TRANSLATE_NOOP("FullscreenUI", "Shows the current system CPU and GPU information.");
TRANSLATE_NOOP("FullscreenUI", "Shows statistics about the emulated GS such as primitives and draw calls.");
TRANSLATE_NOOP("FullscreenUI", "Shows the host's CPU utilization based on threads.");
TRANSLATE_NOOP("FullscreenUI", "Shows the host's GPU utilization.");
TRANSLATE_NOOP("FullscreenUI", "Shows indicators when fast forwarding, pausing, and other abnormal states are active.");
TRANSLATE_NOOP("FullscreenUI", "Shows a visual history of frame times.");
TRANSLATE_NOOP("FullscreenUI", "Shows the current configuration in the bottom-right corner of the display.");
TRANSLATE_NOOP("FullscreenUI", "Shows the amount of currently active patches/cheats on the bottom-right corner of the display.");
TRANSLATE_NOOP("FullscreenUI", "Shows the current controller state of the system in the bottom-left corner of the display.");
TRANSLATE_NOOP("FullscreenUI", "Shows the status of the currently active video capture.");
TRANSLATE_NOOP("FullscreenUI", "Shows the status of the currently active input recording.");
TRANSLATE_NOOP("FullscreenUI", "Shows the number of dumped and loaded texture replacements on the OSD.");
TRANSLATE_NOOP("FullscreenUI", "Displays warnings when settings are enabled which may break games.");
TRANSLATE_NOOP("FullscreenUI", "Operations");
TRANSLATE_NOOP("FullscreenUI", "Resets configuration to defaults (excluding controller settings).");
TRANSLATE_NOOP("FullscreenUI", "BIOS Configuration");
TRANSLATE_NOOP("FullscreenUI", "Changes the BIOS image used to start future sessions.");
TRANSLATE_NOOP("FullscreenUI", "BIOS Selection");
TRANSLATE_NOOP("FullscreenUI", "Options and Patches");
TRANSLATE_NOOP("FullscreenUI", "Skips the intro screen, and bypasses region checks.");
TRANSLATE_NOOP("FullscreenUI", "Speed Control");
TRANSLATE_NOOP("FullscreenUI", "Sets the speed when running without fast forwarding.");
TRANSLATE_NOOP("FullscreenUI", "Sets the speed when using the fast forward hotkey.");
TRANSLATE_NOOP("FullscreenUI", "Sets the speed when using the slow motion hotkey.");
TRANSLATE_NOOP("FullscreenUI", "System Settings");
TRANSLATE_NOOP("FullscreenUI", "Underclocks or overclocks the emulated Emotion Engine CPU.");
TRANSLATE_NOOP("FullscreenUI", "Makes the emulated Emotion Engine skip cycles. Helps a small subset of games like SOTC. Most of the time it's harmful to performance.");
TRANSLATE_NOOP("FullscreenUI", "Generally a speedup on CPUs with 4 or more cores. Safe for most games, but a few are incompatible and may hang.");
TRANSLATE_NOOP("FullscreenUI", "Pins emulation threads to CPU cores to potentially improve performance/frame time variance.");
TRANSLATE_NOOP("FullscreenUI", "Enables loading cheats from pnach files.");
TRANSLATE_NOOP("FullscreenUI", "Enables access to files from the host: namespace in the virtual machine.");
TRANSLATE_NOOP("FullscreenUI", "Fast disc access, less loading times. Not recommended.");
TRANSLATE_NOOP("FullscreenUI", "Loads the disc image into RAM before starting the virtual machine.");
TRANSLATE_NOOP("FullscreenUI", "Frame Pacing/Latency Control");
TRANSLATE_NOOP("FullscreenUI", "Sets the number of frames which can be queued.");
TRANSLATE_NOOP("FullscreenUI", "Synchronize EE and GS threads after each frame. Lowest input latency, but increases system requirements.");
TRANSLATE_NOOP("FullscreenUI", "Synchronizes frame presentation with host refresh.");
TRANSLATE_NOOP("FullscreenUI", "Speeds up emulation so that the guest refresh rate matches the host.");
TRANSLATE_NOOP("FullscreenUI", "Disables PCSX2's internal frame timing, and uses host vsync instead.");
TRANSLATE_NOOP("FullscreenUI", "Graphics API");
TRANSLATE_NOOP("FullscreenUI", "Selects the API used to render the emulated GS.");
TRANSLATE_NOOP("FullscreenUI", "Display");
TRANSLATE_NOOP("FullscreenUI", "Selects the aspect ratio to display the game content at.");
TRANSLATE_NOOP("FullscreenUI", "Selects the aspect ratio for display when a FMV is detected as playing.");
TRANSLATE_NOOP("FullscreenUI", "Selects the algorithm used to convert the PS2's interlaced output to progressive for display.");
TRANSLATE_NOOP("FullscreenUI", "Determines the resolution at which screenshots will be saved.");
TRANSLATE_NOOP("FullscreenUI", "Selects the format which will be used to save screenshots.");
TRANSLATE_NOOP("FullscreenUI", "Selects the quality at which screenshots will be compressed.");
TRANSLATE_NOOP("FullscreenUI", "Increases or decreases the virtual picture size vertically.");
TRANSLATE_NOOP("FullscreenUI", "Crops the image, while respecting aspect ratio.");
TRANSLATE_NOOP("FullscreenUI", "%dpx");
TRANSLATE_NOOP("FullscreenUI", "Enables loading widescreen patches from pnach files.");
TRANSLATE_NOOP("FullscreenUI", "Enables loading no-interlacing patches from pnach files.");
TRANSLATE_NOOP("FullscreenUI", "Smooths out the image when upscaling the console to the screen.");
TRANSLATE_NOOP("FullscreenUI", "Adds padding to the display area to ensure that the ratio between pixels on the host to pixels in the console is an integer number. May result in a sharper image in some 2D games.");
TRANSLATE_NOOP("FullscreenUI", "Enables PCRTC Offsets which position the screen as the game requests.");
TRANSLATE_NOOP("FullscreenUI", "Enables the option to show the overscan area on games which draw more than the safe area of the screen.");
TRANSLATE_NOOP("FullscreenUI", "Enables internal Anti-Blur hacks. Less accurate to PS2 rendering but will make a lot of games look less blurry.");
TRANSLATE_NOOP("FullscreenUI", "Rendering");
TRANSLATE_NOOP("FullscreenUI", "Multiplies the render resolution by the specified factor (upscaling).");
TRANSLATE_NOOP("FullscreenUI", "Selects where bilinear filtering is utilized when rendering textures.");
TRANSLATE_NOOP("FullscreenUI", "Selects where trilinear filtering is utilized when rendering textures.");
TRANSLATE_NOOP("FullscreenUI", "Selects where anisotropic filtering is utilized when rendering textures.");
TRANSLATE_NOOP("FullscreenUI", "Selects the type of dithering applies when the game requests it.");
TRANSLATE_NOOP("FullscreenUI", "Determines the level of accuracy when emulating blend modes not supported by the host graphics API.");
TRANSLATE_NOOP("FullscreenUI", "Enables emulation of the GS's texture mipmapping.");
TRANSLATE_NOOP("FullscreenUI", "Number of threads to use in addition to the main GS thread for rasterization.");
TRANSLATE_NOOP("FullscreenUI", "Force a primitive flush when a framebuffer is also an input texture.");
TRANSLATE_NOOP("FullscreenUI", "Enables emulation of the GS's edge anti-aliasing (AA1).");
TRANSLATE_NOOP("FullscreenUI", "Hardware Fixes");
TRANSLATE_NOOP("FullscreenUI", "Disables automatic hardware fixes, allowing you to set fixes manually.");
TRANSLATE_NOOP("FullscreenUI", "CPU Sprite Render Size");
TRANSLATE_NOOP("FullscreenUI", "Uses software renderer to draw texture decompression-like sprites.");
TRANSLATE_NOOP("FullscreenUI", "CPU Sprite Render Level");
TRANSLATE_NOOP("FullscreenUI", "Determines filter level for CPU sprite render.");
TRANSLATE_NOOP("FullscreenUI", "Software CLUT Render");
TRANSLATE_NOOP("FullscreenUI", "Uses software renderer to draw texture CLUT points/sprites.");
TRANSLATE_NOOP("FullscreenUI", "GPU Target CLUT");
TRANSLATE_NOOP("FullscreenUI", "Try to detect when a game is drawing its own color palette and then renders it on the GPU with special handling.");
TRANSLATE_NOOP("FullscreenUI", "Skip Draw Start");
TRANSLATE_NOOP("FullscreenUI", "Object range to skip drawing.");
TRANSLATE_NOOP("FullscreenUI", "Skip Draw End");
TRANSLATE_NOOP("FullscreenUI", "Auto Flush (Hardware)");
TRANSLATE_NOOP("FullscreenUI", "CPU Framebuffer Conversion");
TRANSLATE_NOOP("FullscreenUI", "Convert 4-bit and 8-bit framebuffer on the CPU instead of the GPU.");
TRANSLATE_NOOP("FullscreenUI", "Disable Depth Conversion");
TRANSLATE_NOOP("FullscreenUI", "Disable the support of depth buffers in the texture cache.");
TRANSLATE_NOOP("FullscreenUI", "Disable Safe Features");
TRANSLATE_NOOP("FullscreenUI", "This option disables multiple safe features.");
TRANSLATE_NOOP("FullscreenUI", "Disable Render Fixes");
TRANSLATE_NOOP("FullscreenUI", "This option disables game-specific render fixes.");
TRANSLATE_NOOP("FullscreenUI", "Preload Frame Data");
TRANSLATE_NOOP("FullscreenUI", "Uploads GS data when rendering a new frame to reproduce some effects accurately.");
TRANSLATE_NOOP("FullscreenUI", "Disable Partial Invalidation");
TRANSLATE_NOOP("FullscreenUI", "Removes texture cache entries when there is any intersection, rather than only the intersected areas.");
TRANSLATE_NOOP("FullscreenUI", "Texture Inside RT");
TRANSLATE_NOOP("FullscreenUI", "Allows the texture cache to reuse as an input texture the inner portion of a previous framebuffer.");
TRANSLATE_NOOP("FullscreenUI", "Read Targets When Closing");
TRANSLATE_NOOP("FullscreenUI", "Flushes all targets in the texture cache back to local memory when shutting down.");
TRANSLATE_NOOP("FullscreenUI", "Estimate Texture Region");
TRANSLATE_NOOP("FullscreenUI", "Attempts to reduce the texture size when games do not set it themselves (e.g. Snowblind games).");
TRANSLATE_NOOP("FullscreenUI", "GPU Palette Conversion");
TRANSLATE_NOOP("FullscreenUI", "When enabled GPU converts colormap-textures, otherwise the CPU will. It is a trade-off between GPU and CPU.");
TRANSLATE_NOOP("FullscreenUI", "Upscaling Fixes");
TRANSLATE_NOOP("FullscreenUI", "Half Pixel Offset");
TRANSLATE_NOOP("FullscreenUI", "Adjusts vertices relative to upscaling.");
TRANSLATE_NOOP("FullscreenUI", "Native Scaling");
TRANSLATE_NOOP("FullscreenUI", "Attempt to do rescaling at native resolution.");
TRANSLATE_NOOP("FullscreenUI", "Round Sprite");
TRANSLATE_NOOP("FullscreenUI", "Adjusts sprite coordinates.");
TRANSLATE_NOOP("FullscreenUI", "Bilinear Dirty Upscale");
TRANSLATE_NOOP("FullscreenUI", "Can smooth out textures due to be bilinear filtered when upscaling. E.g. Brave sun glare.");
TRANSLATE_NOOP("FullscreenUI", "Texture Offset X");
TRANSLATE_NOOP("FullscreenUI", "Adjusts target texture offsets.");
TRANSLATE_NOOP("FullscreenUI", "Texture Offset Y");
TRANSLATE_NOOP("FullscreenUI", "Align Sprite");
TRANSLATE_NOOP("FullscreenUI", "Fixes issues with upscaling (vertical lines) in some games.");
TRANSLATE_NOOP("FullscreenUI", "Merge Sprite");
TRANSLATE_NOOP("FullscreenUI", "Replaces multiple post-processing sprites with a larger single sprite.");
TRANSLATE_NOOP("FullscreenUI", "Force Even Sprite Position");
TRANSLATE_NOOP("FullscreenUI", "Lowers the GS precision to avoid gaps between pixels when upscaling. Fixes the text on Wild Arms games.");
TRANSLATE_NOOP("FullscreenUI", "Unscaled Palette Texture Draws");
TRANSLATE_NOOP("FullscreenUI", "Can fix some broken effects which rely on pixel perfect precision.");
TRANSLATE_NOOP("FullscreenUI", "Texture Replacement");
TRANSLATE_NOOP("FullscreenUI", "Loads replacement textures where available and user-provided.");
TRANSLATE_NOOP("FullscreenUI", "Loads replacement textures on a worker thread, reducing microstutter when replacements are enabled.");
TRANSLATE_NOOP("FullscreenUI", "Preloads all replacement textures to memory. Not necessary with asynchronous loading.");
TRANSLATE_NOOP("FullscreenUI", "Folders");
TRANSLATE_NOOP("FullscreenUI", "Texture Dumping");
TRANSLATE_NOOP("FullscreenUI", "Dumps replaceable textures to disk. Will reduce performance.");
TRANSLATE_NOOP("FullscreenUI", "Includes mipmaps when dumping textures.");
TRANSLATE_NOOP("FullscreenUI", "Allows texture dumping when FMVs are active. You should not enable this.");
TRANSLATE_NOOP("FullscreenUI", "Post-Processing");
TRANSLATE_NOOP("FullscreenUI", "Enables FXAA post-processing shader.");
TRANSLATE_NOOP("FullscreenUI", "Enables FidelityFX Contrast Adaptive Sharpening.");
TRANSLATE_NOOP("FullscreenUI", "Determines the intensity the sharpening effect in CAS post-processing.");
TRANSLATE_NOOP("FullscreenUI", "Filters");
TRANSLATE_NOOP("FullscreenUI", "Enables brightness/contrast/gamma/saturation adjustment.");
TRANSLATE_NOOP("FullscreenUI", "Adjusts brightness. 50 is normal.");
TRANSLATE_NOOP("FullscreenUI", "Adjusts contrast. 50 is normal.");
TRANSLATE_NOOP("FullscreenUI", "Shade Boost Gamma");
TRANSLATE_NOOP("FullscreenUI", "Adjusts gamma. 50 is normal.");
TRANSLATE_NOOP("FullscreenUI", "Adjusts saturation. 50 is normal.");
TRANSLATE_NOOP("FullscreenUI", "Applies a shader which replicates the visual effects of different styles of television set.");
TRANSLATE_NOOP("FullscreenUI", "Advanced");
TRANSLATE_NOOP("FullscreenUI", "Skip Presenting Duplicate Frames");
TRANSLATE_NOOP("FullscreenUI", "Skips displaying frames that don't change in 25/30fps games. Can improve speed, but increase input lag/make frame pacing worse.");
TRANSLATE_NOOP("FullscreenUI", "Disable Mailbox Presentation");
TRANSLATE_NOOP("FullscreenUI", "Forces the use of FIFO over Mailbox presentation, i.e. double buffering instead of triple buffering. Usually results in worse frame pacing.");
TRANSLATE_NOOP("FullscreenUI", "Extended Upscaling Multipliers");
TRANSLATE_NOOP("FullscreenUI", "Displays additional, very high upscaling multipliers dependent on GPU capability.");
TRANSLATE_NOOP("FullscreenUI", "Hardware Download Mode");
TRANSLATE_NOOP("FullscreenUI", "Changes synchronization behavior for GS downloads.");
TRANSLATE_NOOP("FullscreenUI", "Allow Exclusive Fullscreen");
TRANSLATE_NOOP("FullscreenUI", "Overrides the driver's heuristics for enabling exclusive fullscreen, or direct flip/scanout.");
TRANSLATE_NOOP("FullscreenUI", "Override Texture Barriers");
TRANSLATE_NOOP("FullscreenUI", "Forces texture barrier functionality to the specified value.");
TRANSLATE_NOOP("FullscreenUI", "GS Dump Compression");
TRANSLATE_NOOP("FullscreenUI", "Sets the compression algorithm for GS dumps.");
TRANSLATE_NOOP("FullscreenUI", "Disable Framebuffer Fetch");
TRANSLATE_NOOP("FullscreenUI", "Prevents the usage of framebuffer fetch when supported by host GPU.");
TRANSLATE_NOOP("FullscreenUI", "Disable Shader Cache");
TRANSLATE_NOOP("FullscreenUI", "Prevents the loading and saving of shaders/pipelines to disk.");
TRANSLATE_NOOP("FullscreenUI", "Disable Vertex Shader Expand");
TRANSLATE_NOOP("FullscreenUI", "Falls back to the CPU for expanding sprites/lines.");
TRANSLATE_NOOP("FullscreenUI", "Texture Preloading");
TRANSLATE_NOOP("FullscreenUI", "Uploads full textures to the GPU on use, rather than only the utilized regions. Can improve performance in some games.");
TRANSLATE_NOOP("FullscreenUI", "NTSC Frame Rate");
TRANSLATE_NOOP("FullscreenUI", "Determines what frame rate NTSC games run at.");
TRANSLATE_NOOP("FullscreenUI", "PAL Frame Rate");
TRANSLATE_NOOP("FullscreenUI", "Determines what frame rate PAL games run at.");
TRANSLATE_NOOP("FullscreenUI", "Audio Control");
TRANSLATE_NOOP("FullscreenUI", "Controls the volume of the audio played on the host at normal speed.");
TRANSLATE_NOOP("FullscreenUI", "Controls the volume of the audio played on the host when fast forwarding.");
TRANSLATE_NOOP("FullscreenUI", "Prevents the emulator from producing any audible sound.");
TRANSLATE_NOOP("FullscreenUI", "Backend Settings");
TRANSLATE_NOOP("FullscreenUI", "Determines how audio frames produced by the emulator are submitted to the host.");
TRANSLATE_NOOP("FullscreenUI", "Determines how audio is expanded from stereo to surround for supported games.");
TRANSLATE_NOOP("FullscreenUI", "Changes when SPU samples are generated relative to system emulation.");
TRANSLATE_NOOP("FullscreenUI", "Determines the amount of audio buffered before being pulled by the host API.");
TRANSLATE_NOOP("FullscreenUI", "%d ms");
TRANSLATE_NOOP("FullscreenUI", "Determines how much latency there is between the audio being picked up by the host API, and played through speakers.");
TRANSLATE_NOOP("FullscreenUI", "When enabled, the minimum supported output latency will be used for the host API.");
TRANSLATE_NOOP("FullscreenUI", "Settings and Operations");
TRANSLATE_NOOP("FullscreenUI", "Creates a new memory card file or folder.");
TRANSLATE_NOOP("FullscreenUI", "Simulates a larger memory card by filtering saves only to the current game.");
TRANSLATE_NOOP("FullscreenUI", "If not set, this card will be considered unplugged.");
TRANSLATE_NOOP("FullscreenUI", "The selected memory card image will be used for this slot.");
TRANSLATE_NOOP("FullscreenUI", "Removes the current card from the slot.");
TRANSLATE_NOOP("FullscreenUI", "Network Adapter");
TRANSLATE_NOOP("FullscreenUI", "Enables the network adapter for online functionality and LAN play.");
TRANSLATE_NOOP("FullscreenUI", "Determines the simulated Ethernet adapter type.");
TRANSLATE_NOOP("FullscreenUI", "Network adapter to use for PS2 network emulation.");
TRANSLATE_NOOP("FullscreenUI", "When enabled, DHCP packets will be intercepted and replaced with internal responses.");
TRANSLATE_NOOP("FullscreenUI", "Network Configuration");
TRANSLATE_NOOP("FullscreenUI", "IP address for the PS2 virtual network adapter.");
TRANSLATE_NOOP("FullscreenUI", "Automatically determine the subnet mask based on the IP address class.");
TRANSLATE_NOOP("FullscreenUI", "Subnet mask for the PS2 virtual network adapter.");
TRANSLATE_NOOP("FullscreenUI", "Automatically determine the gateway address based on the IP address.");
TRANSLATE_NOOP("FullscreenUI", "Gateway address for the PS2 virtual network adapter.");
TRANSLATE_NOOP("FullscreenUI", "Determines how primary DNS requests are handled.");
TRANSLATE_NOOP("FullscreenUI", "Primary DNS server address for the PS2 virtual network adapter.");
TRANSLATE_NOOP("FullscreenUI", "Determines how secondary DNS requests are handled.");
TRANSLATE_NOOP("FullscreenUI", "Secondary DNS server address for the PS2 virtual network adapter.");
TRANSLATE_NOOP("FullscreenUI", "Internal HDD");
TRANSLATE_NOOP("FullscreenUI", "Enables the internal Hard Disk Drive for expanded storage.");
TRANSLATE_NOOP("FullscreenUI", "Changes the HDD image used for PS2 internal storage.");
TRANSLATE_NOOP("FullscreenUI", "HDD Image Selection");
TRANSLATE_NOOP("FullscreenUI", "Configuration");
TRANSLATE_NOOP("FullscreenUI", "Replaces these settings with a previously saved input profile.");
TRANSLATE_NOOP("FullscreenUI", "Stores the current settings to an input profile.");
TRANSLATE_NOOP("FullscreenUI", "Resets all configuration to defaults (including bindings).");
TRANSLATE_NOOP("FullscreenUI", "Input Sources");
TRANSLATE_NOOP("FullscreenUI", "The SDL input source supports most controllers.");
TRANSLATE_NOOP("FullscreenUI", "Provides vibration and LED control support over Bluetooth.");
TRANSLATE_NOOP("FullscreenUI", "Enable/Disable the Player LED on DualSense controllers.");
TRANSLATE_NOOP("FullscreenUI", "Allow SDL to use raw access to input devices.");
TRANSLATE_NOOP("FullscreenUI", "The XInput source provides support for XBox 360/XBox One/XBox Series controllers.");
TRANSLATE_NOOP("FullscreenUI", "Multitap");
TRANSLATE_NOOP("FullscreenUI", "Enables an additional three controller slots. Not supported in all games.");
TRANSLATE_NOOP("FullscreenUI", "Attempts to map the selected port to a chosen controller.");
TRANSLATE_NOOP("FullscreenUI", "Trigger");
TRANSLATE_NOOP("FullscreenUI", "Toggles the macro when the button is pressed, instead of held.");
TRANSLATE_NOOP("FullscreenUI", "Determines the frequency at which the macro will toggle the buttons on and off (aka auto fire).");
TRANSLATE_NOOP("FullscreenUI", "Determines how much pressure is simulated when macro is active.");
TRANSLATE_NOOP("FullscreenUI", "Determines the pressure required to activate the macro.");
TRANSLATE_NOOP("FullscreenUI", "Toggle every %d frames");
TRANSLATE_NOOP("FullscreenUI", "Clears all bindings for this USB controller.");
TRANSLATE_NOOP("FullscreenUI", "Data Save Locations");
TRANSLATE_NOOP("FullscreenUI", "Changing these options may cause games to become non-functional. Modify at your own risk, the PCSX2 team will not provide support for configurations with these settings changed.");
TRANSLATE_NOOP("FullscreenUI", "Logging");
TRANSLATE_NOOP("FullscreenUI", "Writes log messages to the system console (console window/standard output).");
TRANSLATE_NOOP("FullscreenUI", "Writes log messages to emulog.txt.");
TRANSLATE_NOOP("FullscreenUI", "Writes dev log messages to log sinks.");
TRANSLATE_NOOP("FullscreenUI", "Writes timestamps alongside log messages.");
TRANSLATE_NOOP("FullscreenUI", "Writes debug messages from the game's EE code to the console.");
TRANSLATE_NOOP("FullscreenUI", "Writes debug messages from the game's IOP code to the console.");
TRANSLATE_NOOP("FullscreenUI", "Logs disc reads from games.");
TRANSLATE_NOOP("FullscreenUI", "Emotion Engine");
TRANSLATE_NOOP("FullscreenUI", "Rounding Mode");
TRANSLATE_NOOP("FullscreenUI", "Determines how the results of floating-point operations are rounded. Some games need specific settings.");
TRANSLATE_NOOP("FullscreenUI", "Division Rounding Mode");
TRANSLATE_NOOP("FullscreenUI", "Determines how the results of floating-point division is rounded. Some games need specific settings.");
TRANSLATE_NOOP("FullscreenUI", "Clamping Mode");
TRANSLATE_NOOP("FullscreenUI", "Determines how out-of-range floating point numbers are handled. Some games need specific settings.");
TRANSLATE_NOOP("FullscreenUI", "Enable EE Recompiler");
TRANSLATE_NOOP("FullscreenUI", "Performs just-in-time binary translation of 64-bit MIPS-IV machine code to native code.");
TRANSLATE_NOOP("FullscreenUI", "Enable EE Cache");
TRANSLATE_NOOP("FullscreenUI", "Enables simulation of the EE's cache. Slow.");
TRANSLATE_NOOP("FullscreenUI", "Enable INTC Spin Detection");
TRANSLATE_NOOP("FullscreenUI", "Huge speedup for some games, with almost no compatibility side effects.");
TRANSLATE_NOOP("FullscreenUI", "Enable Wait Loop Detection");
TRANSLATE_NOOP("FullscreenUI", "Moderate speedup for some games, with no known side effects.");
TRANSLATE_NOOP("FullscreenUI", "Enable Fast Memory Access");
TRANSLATE_NOOP("FullscreenUI", "Uses backpatching to avoid register flushing on every memory access.");
TRANSLATE_NOOP("FullscreenUI", "Vector Units");
TRANSLATE_NOOP("FullscreenUI", "VU0 Rounding Mode");
TRANSLATE_NOOP("FullscreenUI", "VU0 Clamping Mode");
TRANSLATE_NOOP("FullscreenUI", "VU1 Rounding Mode");
TRANSLATE_NOOP("FullscreenUI", "VU1 Clamping Mode");
TRANSLATE_NOOP("FullscreenUI", "Enable VU0 Recompiler (Micro Mode)");
TRANSLATE_NOOP("FullscreenUI", "New Vector Unit recompiler with much improved compatibility. Recommended.");
TRANSLATE_NOOP("FullscreenUI", "Enable VU1 Recompiler");
TRANSLATE_NOOP("FullscreenUI", "Enable VU Flag Optimization");
TRANSLATE_NOOP("FullscreenUI", "Good speedup and high compatibility, may cause graphical errors.");
TRANSLATE_NOOP("FullscreenUI", "Enable Instant VU1");
TRANSLATE_NOOP("FullscreenUI", "Runs VU1 instantly. Provides a modest speed improvement in most games. Safe for most games, but a few games may exhibit graphical errors.");
TRANSLATE_NOOP("FullscreenUI", "I/O Processor");
TRANSLATE_NOOP("FullscreenUI", "Enable IOP Recompiler");
TRANSLATE_NOOP("FullscreenUI", "Performs just-in-time binary translation of 32-bit MIPS-I machine code to native code.");
TRANSLATE_NOOP("FullscreenUI", "Save State Management");
TRANSLATE_NOOP("FullscreenUI", "Sets the compression algorithm for savestate.");
TRANSLATE_NOOP("FullscreenUI", "Sets the compression level for savestate.");
TRANSLATE_NOOP("FullscreenUI", "Graphics");
TRANSLATE_NOOP("FullscreenUI", "Enables API-level validation of graphics commands.");
TRANSLATE_NOOP("FullscreenUI", "Enable Cheats");
TRANSLATE_NOOP("FullscreenUI", "No cheats are available for this game.");
TRANSLATE_NOOP("FullscreenUI", "Cheat Codes");
TRANSLATE_NOOP("FullscreenUI", "No patches are available for this game.");
TRANSLATE_NOOP("FullscreenUI", "Game Patches");
TRANSLATE_NOOP("FullscreenUI", "Activating cheats can cause unpredictable behavior, crashing, soft-locks, or broken saved games.");
TRANSLATE_NOOP("FullscreenUI", "Activating game patches can cause unpredictable behavior, crashing, soft-locks, or broken saved games.");
TRANSLATE_NOOP("FullscreenUI", "Use patches at your own risk, the PCSX2 team will provide no support for users who have enabled game patches.");
TRANSLATE_NOOP("FullscreenUI", "Game Fixes");
TRANSLATE_NOOP("FullscreenUI", "Game fixes should not be modified unless you are aware of what each option does and the implications of doing so.");
TRANSLATE_NOOP("FullscreenUI", "FPU Multiply Hack");
TRANSLATE_NOOP("FullscreenUI", "For Tales of Destiny.");
TRANSLATE_NOOP("FullscreenUI", "Use Software Renderer For FMVs");
TRANSLATE_NOOP("FullscreenUI", "Needed for some games with complex FMV rendering.");
TRANSLATE_NOOP("FullscreenUI", "Skip MPEG Hack");
TRANSLATE_NOOP("FullscreenUI", "Skips videos/FMVs in games to avoid game hanging/freezes.");
TRANSLATE_NOOP("FullscreenUI", "Preload TLB Hack");
TRANSLATE_NOOP("FullscreenUI", "To avoid TLB miss on Goemon.");
TRANSLATE_NOOP("FullscreenUI", "EE Timing Hack");
TRANSLATE_NOOP("FullscreenUI", "General-purpose timing hack. Known to affect following games: Digital Devil Saga, SSX.");
TRANSLATE_NOOP("FullscreenUI", "Instant DMA Hack");
TRANSLATE_NOOP("FullscreenUI", "Good for cache emulation problems. Known to affect following games: Fire Pro Wrestling Z.");
TRANSLATE_NOOP("FullscreenUI", "OPH Flag Hack");
TRANSLATE_NOOP("FullscreenUI", "Known to affect following games: Bleach Blade Battlers, Growlanser II and III, Wizardry.");
TRANSLATE_NOOP("FullscreenUI", "Emulate GIF FIFO");
TRANSLATE_NOOP("FullscreenUI", "Correct but slower. Known to affect the following games: FIFA Street 2.");
TRANSLATE_NOOP("FullscreenUI", "DMA Busy Hack");
TRANSLATE_NOOP("FullscreenUI", "Known to affect following games: Mana Khemia 1, Metal Saga, Pilot Down Behind Enemy Lines.");
TRANSLATE_NOOP("FullscreenUI", "Delay VIF1 Stalls");
TRANSLATE_NOOP("FullscreenUI", "For SOCOM 2 HUD and Spy Hunter loading hang.");
TRANSLATE_NOOP("FullscreenUI", "Emulate VIF FIFO");
TRANSLATE_NOOP("FullscreenUI", "Simulate VIF1 FIFO read ahead. Known to affect following games: Test Drive Unlimited, Transformers.");
TRANSLATE_NOOP("FullscreenUI", "Full VU0 Synchronization");
TRANSLATE_NOOP("FullscreenUI", "Forces tight VU0 sync on every COP2 instruction.");
TRANSLATE_NOOP("FullscreenUI", "VU I Bit Hack");
TRANSLATE_NOOP("FullscreenUI", "Avoids constant recompilation in some games. Known to affect the following games: Scarface The World is Yours, Crash Tag Team Racing.");
TRANSLATE_NOOP("FullscreenUI", "VU Add Hack");
TRANSLATE_NOOP("FullscreenUI", "For Tri-Ace Games: Star Ocean 3, Radiata Stories, Valkyrie Profile 2.");
TRANSLATE_NOOP("FullscreenUI", "VU Overflow Hack");
TRANSLATE_NOOP("FullscreenUI", "To check for possible float overflows (Superman Returns).");
TRANSLATE_NOOP("FullscreenUI", "VU Sync");
TRANSLATE_NOOP("FullscreenUI", "Run behind. To avoid sync problems when reading or writing VU registers.");
TRANSLATE_NOOP("FullscreenUI", "VU XGKick Sync");
TRANSLATE_NOOP("FullscreenUI", "Use accurate timing for VU XGKicks (slower).");
TRANSLATE_NOOP("FullscreenUI", "Force Blit Internal FPS Detection");
TRANSLATE_NOOP("FullscreenUI", "Use alternative method to calculate internal FPS to avoid false readings in some games.");
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
TRANSLATE_NOOP("FullscreenUI", "{0}/{1}/{2}/{3}");
TRANSLATE_NOOP("FullscreenUI", "Automatic mapping completed for {}.");
TRANSLATE_NOOP("FullscreenUI", "Automatic mapping failed for {}.");
TRANSLATE_NOOP("FullscreenUI", "Game settings initialized with global settings for '{}'.");
TRANSLATE_NOOP("FullscreenUI", "Game settings have been cleared for '{}'.");
TRANSLATE_NOOP("FullscreenUI", "Uses {} as confirm when using a controller.");
TRANSLATE_NOOP("FullscreenUI", "Swaps both {}/{} (When Swap OK/Cancel is set to automatic) and {}/{} buttons");
TRANSLATE_NOOP("FullscreenUI", "Slot {}");
TRANSLATE_NOOP("FullscreenUI", "{} (Current)");
TRANSLATE_NOOP("FullscreenUI", "{} (Folder)");
TRANSLATE_NOOP("FullscreenUI", "Selected HDD image: {}");
TRANSLATE_NOOP("FullscreenUI", "HDD image '{}' already exists. Do you want to overwrite it?");
TRANSLATE_NOOP("FullscreenUI", "Failed to create the Memory Card, because the name '{}' contains one or more invalid characters.");
TRANSLATE_NOOP("FullscreenUI", "Failed to create the Memory Card, because another card with the name '{}' already exists.");
TRANSLATE_NOOP("FullscreenUI", "Memory Card '{}' created.");
TRANSLATE_NOOP("FullscreenUI", "Failed to load '{}'.");
TRANSLATE_NOOP("FullscreenUI", "Input profile '{}' loaded.");
TRANSLATE_NOOP("FullscreenUI", "Input profile '{}' saved.");
TRANSLATE_NOOP("FullscreenUI", "Failed to save input profile '{}'.");
TRANSLATE_NOOP("FullscreenUI", "Port {} Controller Type");
TRANSLATE_NOOP("FullscreenUI", "Select Macro {} Binds");
TRANSLATE_NOOP("FullscreenUI", "{} Frames");
TRANSLATE_NOOP("FullscreenUI", "Port {} Device");
TRANSLATE_NOOP("FullscreenUI", "Port {} Subtype");
TRANSLATE_NOOP("FullscreenUI", "{} unlabelled patch codes will automatically activate.");
TRANSLATE_NOOP("FullscreenUI", "{} unlabelled patch codes found but not enabled.");
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
TRANSLATE_NOOP("FullscreenUI", "Left: ");
TRANSLATE_NOOP("FullscreenUI", "Top: ");
TRANSLATE_NOOP("FullscreenUI", "Right: ");
TRANSLATE_NOOP("FullscreenUI", "Bottom: ");
TRANSLATE_NOOP("FullscreenUI", "Summary");
TRANSLATE_NOOP("FullscreenUI", "Interface Settings");
TRANSLATE_NOOP("FullscreenUI", "BIOS Settings");
TRANSLATE_NOOP("FullscreenUI", "Emulation Settings");
TRANSLATE_NOOP("FullscreenUI", "Graphics Settings");
TRANSLATE_NOOP("FullscreenUI", "Audio Settings");
TRANSLATE_NOOP("FullscreenUI", "Memory Card Settings");
TRANSLATE_NOOP("FullscreenUI", "Network & HDD Settings");
TRANSLATE_NOOP("FullscreenUI", "Folder Settings");
TRANSLATE_NOOP("FullscreenUI", "Achievements Settings");
TRANSLATE_NOOP("FullscreenUI", "Controller Settings");
TRANSLATE_NOOP("FullscreenUI", "Hotkey Settings");
TRANSLATE_NOOP("FullscreenUI", "Advanced Settings");
TRANSLATE_NOOP("FullscreenUI", "Patches");
TRANSLATE_NOOP("FullscreenUI", "Cheats");
TRANSLATE_NOOP("FullscreenUI", "Dark");
TRANSLATE_NOOP("FullscreenUI", "Light");
TRANSLATE_NOOP("FullscreenUI", "Grey Matter");
TRANSLATE_NOOP("FullscreenUI", "Untouched Lagoon");
TRANSLATE_NOOP("FullscreenUI", "Baby Pastel");
TRANSLATE_NOOP("FullscreenUI", "Pizza Time!");
TRANSLATE_NOOP("FullscreenUI", "PCSX2 Blue");
TRANSLATE_NOOP("FullscreenUI", "Scarlet Devil");
TRANSLATE_NOOP("FullscreenUI", "Violet Angel");
TRANSLATE_NOOP("FullscreenUI", "Cobalt Sky");
TRANSLATE_NOOP("FullscreenUI", "AMOLED");
TRANSLATE_NOOP("FullscreenUI", "Fit");
TRANSLATE_NOOP("FullscreenUI", "Fill");
TRANSLATE_NOOP("FullscreenUI", "Stretch");
TRANSLATE_NOOP("FullscreenUI", "Enabled");
TRANSLATE_NOOP("FullscreenUI", "Disabled");
TRANSLATE_NOOP("FullscreenUI", "Top Left");
TRANSLATE_NOOP("FullscreenUI", "Top Center");
TRANSLATE_NOOP("FullscreenUI", "Top Right");
TRANSLATE_NOOP("FullscreenUI", "Center Left");
TRANSLATE_NOOP("FullscreenUI", "Center");
TRANSLATE_NOOP("FullscreenUI", "Center Right");
TRANSLATE_NOOP("FullscreenUI", "Bottom Left");
TRANSLATE_NOOP("FullscreenUI", "Bottom Center");
TRANSLATE_NOOP("FullscreenUI", "Bottom Right");
TRANSLATE_NOOP("FullscreenUI", "2% [1 FPS (NTSC) / 1 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "10% [6 FPS (NTSC) / 5 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "25% [15 FPS (NTSC) / 12 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "50% [30 FPS (NTSC) / 25 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "75% [45 FPS (NTSC) / 37 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "90% [54 FPS (NTSC) / 45 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "100% [60 FPS (NTSC) / 50 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "110% [66 FPS (NTSC) / 55 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "120% [72 FPS (NTSC) / 60 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "150% [90 FPS (NTSC) / 75 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "175% [105 FPS (NTSC) / 87 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "200% [120 FPS (NTSC) / 100 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "300% [180 FPS (NTSC) / 150 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "400% [240 FPS (NTSC) / 200 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "500% [300 FPS (NTSC) / 250 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "1000% [600 FPS (NTSC) / 500 FPS (PAL)]");
TRANSLATE_NOOP("FullscreenUI", "50% Speed");
TRANSLATE_NOOP("FullscreenUI", "60% Speed");
TRANSLATE_NOOP("FullscreenUI", "75% Speed");
TRANSLATE_NOOP("FullscreenUI", "100% Speed (Default)");
TRANSLATE_NOOP("FullscreenUI", "130% Speed");
TRANSLATE_NOOP("FullscreenUI", "180% Speed");
TRANSLATE_NOOP("FullscreenUI", "300% Speed");
TRANSLATE_NOOP("FullscreenUI", "Normal (Default)");
TRANSLATE_NOOP("FullscreenUI", "Mild Underclock");
TRANSLATE_NOOP("FullscreenUI", "Moderate Underclock");
TRANSLATE_NOOP("FullscreenUI", "Maximum Underclock");
TRANSLATE_NOOP("FullscreenUI", "0 Frames (Hard Sync)");
TRANSLATE_NOOP("FullscreenUI", "1 Frame");
TRANSLATE_NOOP("FullscreenUI", "2 Frames");
TRANSLATE_NOOP("FullscreenUI", "3 Frames");
TRANSLATE_NOOP("FullscreenUI", "Extra + Preserve Sign");
TRANSLATE_NOOP("FullscreenUI", "Full");
TRANSLATE_NOOP("FullscreenUI", "Extra");
TRANSLATE_NOOP("FullscreenUI", "Automatic (Default)");
TRANSLATE_NOOP("FullscreenUI", "Direct3D 11");
TRANSLATE_NOOP("FullscreenUI", "Direct3D 12");
TRANSLATE_NOOP("FullscreenUI", "OpenGL");
TRANSLATE_NOOP("FullscreenUI", "Vulkan");
TRANSLATE_NOOP("FullscreenUI", "Metal");
TRANSLATE_NOOP("FullscreenUI", "Software Renderer");
TRANSLATE_NOOP("FullscreenUI", "Null");
TRANSLATE_NOOP("FullscreenUI", "Off");
TRANSLATE_NOOP("FullscreenUI", "Bilinear (Smooth)");
TRANSLATE_NOOP("FullscreenUI", "Bilinear (Sharp)");
TRANSLATE_NOOP("FullscreenUI", "No Deinterlacing");
TRANSLATE_NOOP("FullscreenUI", "Weave (Top Field First, Sawtooth)");
TRANSLATE_NOOP("FullscreenUI", "Weave (Bottom Field First, Sawtooth)");
TRANSLATE_NOOP("FullscreenUI", "Bob (Top Field First)");
TRANSLATE_NOOP("FullscreenUI", "Bob (Bottom Field First)");
TRANSLATE_NOOP("FullscreenUI", "Blend (Top Field First, Half FPS)");
TRANSLATE_NOOP("FullscreenUI", "Blend (Bottom Field First, Half FPS)");
TRANSLATE_NOOP("FullscreenUI", "Adaptive (Top Field First)");
TRANSLATE_NOOP("FullscreenUI", "Adaptive (Bottom Field First)");
TRANSLATE_NOOP("FullscreenUI", "Native (PS2)");
TRANSLATE_NOOP("FullscreenUI", "2x Native (~720px/HD)");
TRANSLATE_NOOP("FullscreenUI", "3x Native (~1080px/FHD)");
TRANSLATE_NOOP("FullscreenUI", "4x Native (~1440px/QHD)");
TRANSLATE_NOOP("FullscreenUI", "5x Native (~1800px/QHD+)");
TRANSLATE_NOOP("FullscreenUI", "6x Native (~2160px/4K UHD)");
TRANSLATE_NOOP("FullscreenUI", "7x Native (~2520px)");
TRANSLATE_NOOP("FullscreenUI", "8x Native (~2880px/5K UHD)");
TRANSLATE_NOOP("FullscreenUI", "9x Native (~3240px)");
TRANSLATE_NOOP("FullscreenUI", "10x Native (~3600px/6K UHD)");
TRANSLATE_NOOP("FullscreenUI", "11x Native (~3960px)");
TRANSLATE_NOOP("FullscreenUI", "12x Native (~4320px/8K UHD)");
TRANSLATE_NOOP("FullscreenUI", "Nearest");
TRANSLATE_NOOP("FullscreenUI", "Bilinear (Forced)");
TRANSLATE_NOOP("FullscreenUI", "Bilinear (PS2)");
TRANSLATE_NOOP("FullscreenUI", "Bilinear (Forced excluding sprite)");
TRANSLATE_NOOP("FullscreenUI", "Off (None)");
TRANSLATE_NOOP("FullscreenUI", "Trilinear (PS2)");
TRANSLATE_NOOP("FullscreenUI", "Trilinear (Forced)");
TRANSLATE_NOOP("FullscreenUI", "Scaled");
TRANSLATE_NOOP("FullscreenUI", "Unscaled (Default)");
TRANSLATE_NOOP("FullscreenUI", "Force 32bit");
TRANSLATE_NOOP("FullscreenUI", "Minimum");
TRANSLATE_NOOP("FullscreenUI", "Basic (Recommended)");
TRANSLATE_NOOP("FullscreenUI", "Medium");
TRANSLATE_NOOP("FullscreenUI", "High");
TRANSLATE_NOOP("FullscreenUI", "Full (Slow)");
TRANSLATE_NOOP("FullscreenUI", "Maximum (Very Slow)");
TRANSLATE_NOOP("FullscreenUI", "Off (Default)");
TRANSLATE_NOOP("FullscreenUI", "2x");
TRANSLATE_NOOP("FullscreenUI", "4x");
TRANSLATE_NOOP("FullscreenUI", "8x");
TRANSLATE_NOOP("FullscreenUI", "16x");
TRANSLATE_NOOP("FullscreenUI", "Partial");
TRANSLATE_NOOP("FullscreenUI", "Full (Hash Cache)");
TRANSLATE_NOOP("FullscreenUI", "Force Disabled");
TRANSLATE_NOOP("FullscreenUI", "Force Enabled");
TRANSLATE_NOOP("FullscreenUI", "Accurate (Recommended)");
TRANSLATE_NOOP("FullscreenUI", "Disable Readbacks (Synchronize GS Thread)");
TRANSLATE_NOOP("FullscreenUI", "Unsynchronized (Non-Deterministic)");
TRANSLATE_NOOP("FullscreenUI", "Disabled (Ignore Transfers)");
TRANSLATE_NOOP("FullscreenUI", "Display Resolution (Aspect Corrected)");
TRANSLATE_NOOP("FullscreenUI", "Internal Resolution (Aspect Corrected)");
TRANSLATE_NOOP("FullscreenUI", "Internal Resolution (No Aspect Correction)");
TRANSLATE_NOOP("FullscreenUI", "PNG");
TRANSLATE_NOOP("FullscreenUI", "JPEG");
TRANSLATE_NOOP("FullscreenUI", "WebP");
TRANSLATE_NOOP("FullscreenUI", "0 (Disabled)");
TRANSLATE_NOOP("FullscreenUI", "1 (64 Max Width)");
TRANSLATE_NOOP("FullscreenUI", "2 (128 Max Width)");
TRANSLATE_NOOP("FullscreenUI", "3 (192 Max Width)");
TRANSLATE_NOOP("FullscreenUI", "4 (256 Max Width)");
TRANSLATE_NOOP("FullscreenUI", "5 (320 Max Width)");
TRANSLATE_NOOP("FullscreenUI", "6 (384 Max Width)");
TRANSLATE_NOOP("FullscreenUI", "7 (448 Max Width)");
TRANSLATE_NOOP("FullscreenUI", "8 (512 Max Width)");
TRANSLATE_NOOP("FullscreenUI", "9 (576 Max Width)");
TRANSLATE_NOOP("FullscreenUI", "10 (640 Max Width)");
TRANSLATE_NOOP("FullscreenUI", "Sprites Only");
TRANSLATE_NOOP("FullscreenUI", "Sprites/Triangles");
TRANSLATE_NOOP("FullscreenUI", "Blended Sprites/Triangles");
TRANSLATE_NOOP("FullscreenUI", "1 (Normal)");
TRANSLATE_NOOP("FullscreenUI", "2 (Aggressive)");
TRANSLATE_NOOP("FullscreenUI", "Inside Target");
TRANSLATE_NOOP("FullscreenUI", "Merge Targets");
TRANSLATE_NOOP("FullscreenUI", "Normal (Vertex)");
TRANSLATE_NOOP("FullscreenUI", "Special (Texture)");
TRANSLATE_NOOP("FullscreenUI", "Special (Texture - Aggressive)");
TRANSLATE_NOOP("FullscreenUI", "Align to Native");
TRANSLATE_NOOP("FullscreenUI", "Align to Native - with Texture Offset");
TRANSLATE_NOOP("FullscreenUI", "Normal");
TRANSLATE_NOOP("FullscreenUI", "Aggressive");
TRANSLATE_NOOP("FullscreenUI", "Normal (Maintain Upscale)");
TRANSLATE_NOOP("FullscreenUI", "Aggressive (Maintain Upscale)");
TRANSLATE_NOOP("FullscreenUI", "Half");
TRANSLATE_NOOP("FullscreenUI", "Force Bilinear");
TRANSLATE_NOOP("FullscreenUI", "Force Nearest");
TRANSLATE_NOOP("FullscreenUI", "Disabled (Default)");
TRANSLATE_NOOP("FullscreenUI", "Enabled (Sprites Only)");
TRANSLATE_NOOP("FullscreenUI", "Enabled (All Primitives)");
TRANSLATE_NOOP("FullscreenUI", "Enabled (Exact Match)");
TRANSLATE_NOOP("FullscreenUI", "Enabled (Check Inside Target)");
TRANSLATE_NOOP("FullscreenUI", "None (Default)");
TRANSLATE_NOOP("FullscreenUI", "Sharpen Only (Internal Resolution)");
TRANSLATE_NOOP("FullscreenUI", "Sharpen and Resize (Display Resolution)");
TRANSLATE_NOOP("FullscreenUI", "Scanline Filter");
TRANSLATE_NOOP("FullscreenUI", "Diagonal Filter");
TRANSLATE_NOOP("FullscreenUI", "Triangular Filter");
TRANSLATE_NOOP("FullscreenUI", "Wave Filter");
TRANSLATE_NOOP("FullscreenUI", "Lottes CRT");
TRANSLATE_NOOP("FullscreenUI", "4xRGSS");
TRANSLATE_NOOP("FullscreenUI", "NxAGSS");
TRANSLATE_NOOP("FullscreenUI", "Uncompressed");
TRANSLATE_NOOP("FullscreenUI", "LZMA (xz)");
TRANSLATE_NOOP("FullscreenUI", "Zstandard (zst)");
TRANSLATE_NOOP("FullscreenUI", "PS1");
TRANSLATE_NOOP("FullscreenUI", "Manual");
TRANSLATE_NOOP("FullscreenUI", "Auto");
TRANSLATE_NOOP("FullscreenUI", "Internal");
TRANSLATE_NOOP("FullscreenUI", "Negative");
TRANSLATE_NOOP("FullscreenUI", "Positive");
TRANSLATE_NOOP("FullscreenUI", "Chop/Zero (Default)");
TRANSLATE_NOOP("FullscreenUI", "Deflate64");
TRANSLATE_NOOP("FullscreenUI", "Zstandard");
TRANSLATE_NOOP("FullscreenUI", "LZMA2");
TRANSLATE_NOOP("FullscreenUI", "Low (Fast)");
TRANSLATE_NOOP("FullscreenUI", "Medium (Recommended)");
TRANSLATE_NOOP("FullscreenUI", "Very High (Slow, Not Recommended)");
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
TRANSLATE_NOOP("FullscreenUI", "Clear Binding");
TRANSLATE_NOOP("FullscreenUI", "Default");
TRANSLATE_NOOP("FullscreenUI", "Change Page");
TRANSLATE_NOOP("FullscreenUI", "Frequency");
TRANSLATE_NOOP("FullscreenUI", "Return To Game");
TRANSLATE_NOOP("FullscreenUI", "Select State");
TRANSLATE_NOOP("FullscreenUI", "Load/Save State");
TRANSLATE_NOOP("FullscreenUI", "Select Game");
TRANSLATE_NOOP("FullscreenUI", "Change View");
TRANSLATE_NOOP("FullscreenUI", "Launch Options");
TRANSLATE_NOOP("FullscreenUI", "Startup Error");
TRANSLATE_NOOP("FullscreenUI", "Select Disc Image");
TRANSLATE_NOOP("FullscreenUI", "Select Disc Drive");
TRANSLATE_NOOP("FullscreenUI", "WARNING: Memory Card Busy");
TRANSLATE_NOOP("FullscreenUI", "Set Input Binding");
TRANSLATE_NOOP("FullscreenUI", "Region");
TRANSLATE_NOOP("FullscreenUI", "Compatibility Rating");
TRANSLATE_NOOP("FullscreenUI", "Path");
TRANSLATE_NOOP("FullscreenUI", "Disc Path");
TRANSLATE_NOOP("FullscreenUI", "Select Disc Path");
TRANSLATE_NOOP("FullscreenUI", "Cannot show details for games which were not scanned in the game list.");
TRANSLATE_NOOP("FullscreenUI", "Copy Settings");
TRANSLATE_NOOP("FullscreenUI", "Clear Settings");
TRANSLATE_NOOP("FullscreenUI", "Theme");
TRANSLATE_NOOP("FullscreenUI", "Default To Game List");
TRANSLATE_NOOP("FullscreenUI", "Use Save State Selector");
TRANSLATE_NOOP("FullscreenUI", "Background Image");
TRANSLATE_NOOP("FullscreenUI", "Select Background Image");
TRANSLATE_NOOP("FullscreenUI", "Clear Background Image");
TRANSLATE_NOOP("FullscreenUI", "Background Opacity");
TRANSLATE_NOOP("FullscreenUI", "Background Mode");
TRANSLATE_NOOP("FullscreenUI", "Inhibit Screensaver");
TRANSLATE_NOOP("FullscreenUI", "Pause On Start");
TRANSLATE_NOOP("FullscreenUI", "Pause On Focus Loss");
TRANSLATE_NOOP("FullscreenUI", "Pause On Controller Disconnection");
TRANSLATE_NOOP("FullscreenUI", "Pause On Menu");
TRANSLATE_NOOP("FullscreenUI", "Confirm Shutdown");
TRANSLATE_NOOP("FullscreenUI", "Save State On Shutdown");
TRANSLATE_NOOP("FullscreenUI", "Create Save State Backups");
TRANSLATE_NOOP("FullscreenUI", "Swap OK/Cancel in Big Picture Mode");
TRANSLATE_NOOP("FullscreenUI", "Use Legacy Nintendo Layout in Big Picture Mode");
TRANSLATE_NOOP("FullscreenUI", "Enable Discord Presence");
TRANSLATE_NOOP("FullscreenUI", "Start Fullscreen");
TRANSLATE_NOOP("FullscreenUI", "Double-Click Toggles Fullscreen");
TRANSLATE_NOOP("FullscreenUI", "Hide Cursor In Fullscreen");
TRANSLATE_NOOP("FullscreenUI", "OSD Scale");
TRANSLATE_NOOP("FullscreenUI", "OSD Messages Position");
TRANSLATE_NOOP("FullscreenUI", "OSD Performance Position");
TRANSLATE_NOOP("FullscreenUI", "Show PCSX2 Version");
TRANSLATE_NOOP("FullscreenUI", "Show Speed");
TRANSLATE_NOOP("FullscreenUI", "Show FPS");
TRANSLATE_NOOP("FullscreenUI", "Show VPS");
TRANSLATE_NOOP("FullscreenUI", "Show Resolution");
TRANSLATE_NOOP("FullscreenUI", "Show Hardware Info");
TRANSLATE_NOOP("FullscreenUI", "Show GS Statistics");
TRANSLATE_NOOP("FullscreenUI", "Show CPU Usage");
TRANSLATE_NOOP("FullscreenUI", "Show GPU Usage");
TRANSLATE_NOOP("FullscreenUI", "Show Status Indicators");
TRANSLATE_NOOP("FullscreenUI", "Show Frame Times");
TRANSLATE_NOOP("FullscreenUI", "Show Settings");
TRANSLATE_NOOP("FullscreenUI", "Show Patches");
TRANSLATE_NOOP("FullscreenUI", "Show Inputs");
TRANSLATE_NOOP("FullscreenUI", "Show Video Capture Status");
TRANSLATE_NOOP("FullscreenUI", "Show Input Recording Status");
TRANSLATE_NOOP("FullscreenUI", "Show Texture Replacement Status");
TRANSLATE_NOOP("FullscreenUI", "Warn About Unsafe Settings");
TRANSLATE_NOOP("FullscreenUI", "Reset Settings");
TRANSLATE_NOOP("FullscreenUI", "Change Search Directory");
TRANSLATE_NOOP("FullscreenUI", "Fast Boot");
TRANSLATE_NOOP("FullscreenUI", "Normal Speed");
TRANSLATE_NOOP("FullscreenUI", "Fast Forward Speed");
TRANSLATE_NOOP("FullscreenUI", "Slow Motion Speed");
TRANSLATE_NOOP("FullscreenUI", "EE Cycle Rate");
TRANSLATE_NOOP("FullscreenUI", "EE Cycle Skipping");
TRANSLATE_NOOP("FullscreenUI", "Enable MTVU (Multi-Threaded VU1)");
TRANSLATE_NOOP("FullscreenUI", "Thread Pinning");
TRANSLATE_NOOP("FullscreenUI", "Enable Host Filesystem");
TRANSLATE_NOOP("FullscreenUI", "Enable Fast CDVD");
TRANSLATE_NOOP("FullscreenUI", "Enable CDVD Precaching");
TRANSLATE_NOOP("FullscreenUI", "Maximum Frame Latency");
TRANSLATE_NOOP("FullscreenUI", "Optimal Frame Pacing");
TRANSLATE_NOOP("FullscreenUI", "Vertical Sync (VSync)");
TRANSLATE_NOOP("FullscreenUI", "Sync to Host Refresh Rate");
TRANSLATE_NOOP("FullscreenUI", "Use Host VSync Timing");
TRANSLATE_NOOP("FullscreenUI", "Aspect Ratio");
TRANSLATE_NOOP("FullscreenUI", "FMV Aspect Ratio Override");
TRANSLATE_NOOP("FullscreenUI", "Deinterlacing");
TRANSLATE_NOOP("FullscreenUI", "Screenshot Size");
TRANSLATE_NOOP("FullscreenUI", "Screenshot Format");
TRANSLATE_NOOP("FullscreenUI", "Screenshot Quality");
TRANSLATE_NOOP("FullscreenUI", "Vertical Stretch");
TRANSLATE_NOOP("FullscreenUI", "Crop");
TRANSLATE_NOOP("FullscreenUI", "Enable Widescreen Patches");
TRANSLATE_NOOP("FullscreenUI", "Enable No-Interlacing Patches");
TRANSLATE_NOOP("FullscreenUI", "Bilinear Upscaling");
TRANSLATE_NOOP("FullscreenUI", "Integer Upscaling");
TRANSLATE_NOOP("FullscreenUI", "Screen Offsets");
TRANSLATE_NOOP("FullscreenUI", "Show Overscan");
TRANSLATE_NOOP("FullscreenUI", "Anti-Blur");
TRANSLATE_NOOP("FullscreenUI", "Internal Resolution");
TRANSLATE_NOOP("FullscreenUI", "Bilinear Filtering");
TRANSLATE_NOOP("FullscreenUI", "Trilinear Filtering");
TRANSLATE_NOOP("FullscreenUI", "Anisotropic Filtering");
TRANSLATE_NOOP("FullscreenUI", "Dithering");
TRANSLATE_NOOP("FullscreenUI", "Blending Accuracy");
TRANSLATE_NOOP("FullscreenUI", "Mipmapping");
TRANSLATE_NOOP("FullscreenUI", "Software Rendering Threads");
TRANSLATE_NOOP("FullscreenUI", "Auto Flush (Software)");
TRANSLATE_NOOP("FullscreenUI", "Edge AA (AA1)");
TRANSLATE_NOOP("FullscreenUI", "Manual Hardware Fixes");
TRANSLATE_NOOP("FullscreenUI", "Load Textures");
TRANSLATE_NOOP("FullscreenUI", "Asynchronous Texture Loading");
TRANSLATE_NOOP("FullscreenUI", "Precache Replacements");
TRANSLATE_NOOP("FullscreenUI", "Replacements Directory");
TRANSLATE_NOOP("FullscreenUI", "Dump Textures");
TRANSLATE_NOOP("FullscreenUI", "Dump Mipmaps");
TRANSLATE_NOOP("FullscreenUI", "Dump FMV Textures");
TRANSLATE_NOOP("FullscreenUI", "FXAA");
TRANSLATE_NOOP("FullscreenUI", "Contrast Adaptive Sharpening");
TRANSLATE_NOOP("FullscreenUI", "CAS Sharpness");
TRANSLATE_NOOP("FullscreenUI", "Shade Boost");
TRANSLATE_NOOP("FullscreenUI", "Shade Boost Brightness");
TRANSLATE_NOOP("FullscreenUI", "Shade Boost Contrast");
TRANSLATE_NOOP("FullscreenUI", "Shade Boost Saturation");
TRANSLATE_NOOP("FullscreenUI", "TV Shaders");
TRANSLATE_NOOP("FullscreenUI", "Standard Volume");
TRANSLATE_NOOP("FullscreenUI", "Fast Forward Volume");
TRANSLATE_NOOP("FullscreenUI", "Mute All Sound");
TRANSLATE_NOOP("FullscreenUI", "Audio Backend");
TRANSLATE_NOOP("FullscreenUI", "Expansion");
TRANSLATE_NOOP("FullscreenUI", "Synchronization");
TRANSLATE_NOOP("FullscreenUI", "Buffer Size");
TRANSLATE_NOOP("FullscreenUI", "Output Latency");
TRANSLATE_NOOP("FullscreenUI", "Minimal Output Latency");
TRANSLATE_NOOP("FullscreenUI", "Create Memory Card");
TRANSLATE_NOOP("FullscreenUI", "Memory Card Directory");
TRANSLATE_NOOP("FullscreenUI", "Folder Memory Card Filter");
TRANSLATE_NOOP("FullscreenUI", "Enable Network Adapter");
TRANSLATE_NOOP("FullscreenUI", "Ethernet Device Type");
TRANSLATE_NOOP("FullscreenUI", "Ethernet Device");
TRANSLATE_NOOP("FullscreenUI", "Intercept DHCP");
TRANSLATE_NOOP("FullscreenUI", "Address");
TRANSLATE_NOOP("FullscreenUI", "Auto Subnet Mask");
TRANSLATE_NOOP("FullscreenUI", "Subnet Mask");
TRANSLATE_NOOP("FullscreenUI", "Auto Gateway");
TRANSLATE_NOOP("FullscreenUI", "Gateway Address");
TRANSLATE_NOOP("FullscreenUI", "DNS1 Mode");
TRANSLATE_NOOP("FullscreenUI", "DNS1 Address");
TRANSLATE_NOOP("FullscreenUI", "DNS2 Mode");
TRANSLATE_NOOP("FullscreenUI", "DNS2 Address");
TRANSLATE_NOOP("FullscreenUI", "Enable HDD");
TRANSLATE_NOOP("FullscreenUI", "Select HDD Image File");
TRANSLATE_NOOP("FullscreenUI", "Select HDD Size");
TRANSLATE_NOOP("FullscreenUI", "Custom HDD Size");
TRANSLATE_NOOP("FullscreenUI", "Create");
TRANSLATE_NOOP("FullscreenUI", "File Already Exists");
TRANSLATE_NOOP("FullscreenUI", "Memory Card Type");
TRANSLATE_NOOP("FullscreenUI", "Use NTFS Compression?");
TRANSLATE_NOOP("FullscreenUI", "Reset Controller Settings");
TRANSLATE_NOOP("FullscreenUI", "Load Profile");
TRANSLATE_NOOP("FullscreenUI", "Save Profile");
TRANSLATE_NOOP("FullscreenUI", "Enable SDL Input Source");
TRANSLATE_NOOP("FullscreenUI", "SDL DualShock 4 / DualSense Enhanced Mode");
TRANSLATE_NOOP("FullscreenUI", "SDL DualSense Player LED");
TRANSLATE_NOOP("FullscreenUI", "SDL Raw Input");
TRANSLATE_NOOP("FullscreenUI", "Enable XInput Input Source");
TRANSLATE_NOOP("FullscreenUI", "Enable Console Port 1 Multitap");
TRANSLATE_NOOP("FullscreenUI", "Enable Console Port 2 Multitap");
TRANSLATE_NOOP("FullscreenUI", "Controller Port {}{}");
TRANSLATE_NOOP("FullscreenUI", "Controller Port {}");
TRANSLATE_NOOP("FullscreenUI", "Controller Type");
TRANSLATE_NOOP("FullscreenUI", "Automatic Mapping");
TRANSLATE_NOOP("FullscreenUI", "Controller Port {}{} Macros");
TRANSLATE_NOOP("FullscreenUI", "Controller Port {} Macros");
TRANSLATE_NOOP("FullscreenUI", "Macro Button {}");
TRANSLATE_NOOP("FullscreenUI", "Buttons");
TRANSLATE_NOOP("FullscreenUI", "Press To Toggle");
TRANSLATE_NOOP("FullscreenUI", "Pressure");
TRANSLATE_NOOP("FullscreenUI", "Deadzone");
TRANSLATE_NOOP("FullscreenUI", "Controller Port {}{} Settings");
TRANSLATE_NOOP("FullscreenUI", "Controller Port {} Settings");
TRANSLATE_NOOP("FullscreenUI", "USB Port {}");
TRANSLATE_NOOP("FullscreenUI", "Device Type");
TRANSLATE_NOOP("FullscreenUI", "Device Subtype");
TRANSLATE_NOOP("FullscreenUI", "{} Bindings");
TRANSLATE_NOOP("FullscreenUI", "Clear Bindings");
TRANSLATE_NOOP("FullscreenUI", "{} Settings");
TRANSLATE_NOOP("FullscreenUI", "Cache Directory");
TRANSLATE_NOOP("FullscreenUI", "Covers Directory");
TRANSLATE_NOOP("FullscreenUI", "Snapshots Directory");
TRANSLATE_NOOP("FullscreenUI", "Save States Directory");
TRANSLATE_NOOP("FullscreenUI", "Game Settings Directory");
TRANSLATE_NOOP("FullscreenUI", "Input Profile Directory");
TRANSLATE_NOOP("FullscreenUI", "Cheats Directory");
TRANSLATE_NOOP("FullscreenUI", "Patches Directory");
TRANSLATE_NOOP("FullscreenUI", "Texture Replacements Directory");
TRANSLATE_NOOP("FullscreenUI", "Video Dumping Directory");
TRANSLATE_NOOP("FullscreenUI", "Show Advanced Settings");
TRANSLATE_NOOP("FullscreenUI", "System Console");
TRANSLATE_NOOP("FullscreenUI", "File Logging");
TRANSLATE_NOOP("FullscreenUI", "Verbose Logging");
TRANSLATE_NOOP("FullscreenUI", "Log Timestamps");
TRANSLATE_NOOP("FullscreenUI", "EE Console");
TRANSLATE_NOOP("FullscreenUI", "IOP Console");
TRANSLATE_NOOP("FullscreenUI", "CDVD Verbose Reads");
TRANSLATE_NOOP("FullscreenUI", "Compression Method");
TRANSLATE_NOOP("FullscreenUI", "Compression Level");
TRANSLATE_NOOP("FullscreenUI", "Use Debug Device");
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
TRANSLATE_NOOP("FullscreenUI", "Failed to Load State");
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
TRANSLATE_NOOP("FullscreenUI", "Memory Card Enabled");
TRANSLATE_NOOP("FullscreenUI", "Card Name");
TRANSLATE_NOOP("FullscreenUI", "Eject Card");
// TRANSLATION-STRING-AREA-END
#endif
