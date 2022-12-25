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

#include "Frontend/Achievements.h"
#include "Frontend/CommonHost.h"
#include "Frontend/FullscreenUI.h"
#include "Frontend/ImGuiFullscreen.h"
#include "Frontend/ImGuiManager.h"

#include "common/Assertions.h"
#include "common/FileSystem.h"
#include "common/HTTPDownloader.h"
#include "common/Console.h"
#include "common/General.h"
#include "common/MD5Digest.h"
#include "common/Path.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "rc_api_info.h"
#include "rc_api_request.h"
#include "rc_api_runtime.h"
#include "rc_api_user.h"
#include "rcheevos.h"
#include "fmt/core.h"

#include "CDVD/IsoFS/IsoFSCDVD.h"
#include "CDVD/IsoFS/IsoFS.h"
#include "Elfheader.h"
#include "GS.h"
#include "Host.h"
#include "HostSettings.h"
#include "IopMem.h"
#include "Memory.h"
#include "VMManager.h"
#include "vtlb.h"
#include "svnrev.h"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdlib>
#include <limits>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#ifdef ENABLE_RAINTEGRATION
// RA_Interface ends up including windows.h, with its silly macros.
#include "common/RedtapeWindows.h"
#include "RA_Interface.h"
#endif

namespace Achievements
{
	enum : s32
	{
		HTTP_OK = Common::HTTPDownloader::HTTP_OK,

		// Number of seconds between rich presence pings. RAIntegration uses 2 minutes.
		RICH_PRESENCE_PING_FREQUENCY = 2 * 60,
		NO_RICH_PRESENCE_PING_FREQUENCY = RICH_PRESENCE_PING_FREQUENCY * 2,

		// Size of the EE physical memory exposed to RetroAchievements.
		EXPOSED_EE_MEMORY_SIZE = Ps2MemSize::MainRam + Ps2MemSize::Scratch,
	};

	static constexpr const char* INFO_SOUND_NAME = "sounds/achievements/message.wav";
	static constexpr const char* UNLOCK_SOUND_NAME = "sounds/achievements/unlock.wav";
	static constexpr const char* LBSUBMIT_SOUND_NAME = "sounds/achievements/lbsubmit.wav";

	static void FormattedError(const char* format, ...);
	static void LogFailedResponseJSON(const Common::HTTPDownloader::Request::Data& data);
	static void EnsureCacheDirectoriesExist();
	static void CheevosEventHandler(const rc_runtime_event_t* runtime_event);
	static unsigned PeekMemory(unsigned address, unsigned num_bytes, void* ud);
	static unsigned PeekMemoryBlock(unsigned address, unsigned char* buffer, unsigned num_bytes);
	static void PokeMemory(unsigned address, unsigned num_bytes, void* ud, unsigned value);
	static bool IsMastered();
	static void ActivateLockedAchievements();
	static bool ActivateAchievement(Achievement* achievement);
	static void DeactivateAchievement(Achievement* achievement);
	static void SendPing();
	static void SendPlaying();
	static void UpdateRichPresence();
	static Achievement* GetMutableAchievementByID(u32 id);
	static void ClearGameInfo(bool clear_achievements = true, bool clear_leaderboards = true);
	static void ClearGameHash();
	static std::string GetUserAgent();
	static void BeginLoadingScreen(const char* text, bool* was_running_idle);
	static void EndLoadingScreen(bool was_running_idle);
	static void LoginCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data);
	static void LoginASyncCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data);
	static void SendLogin(const char* username, const char* password, Common::HTTPDownloader* http_downloader,
		Common::HTTPDownloader::Request::Callback callback);
	static void DownloadImage(std::string url, std::string cache_filename);
	static void DisplayAchievementSummary();
	static void DisplayMasteredNotification();
	static void UnlockAchievement(u32 achievement_id, bool add_notification = true);
	static void AchievementPrimed(u32 achievement_id);
	static void AchievementUnprimed(u32 achievement_id);
	static void SubmitLeaderboard(u32 leaderboard_id, int value);
	static void GetUserUnlocksCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data);
	static void GetUserUnlocks();
	static void GetPatchesCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data);
	static void GetLbInfoCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data);
	static void GetPatches(u32 game_id);
	static std::string_view GetELFNameForHash(const std::string& elf_path);
	static std::optional<std::vector<u8>> ReadELFFromCurrentDisc(const std::string& elf_path);
	static std::string GetGameHash();
	static void SetChallengeMode(bool enabled);
	static void SendGetGameId();
	static void GetGameIdCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data);
	static void SendPlayingCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data);
	static void UpdateRichPresence();
	static void SendPingCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data);
	static void UnlockAchievementCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data);
	static void SubmitLeaderboardCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data);

	static bool s_active = false;
	static bool s_logged_in = false;
	static bool s_challenge_mode = false;
	static u32 s_game_id = 0;

#ifdef ENABLE_RAINTEGRATION
	static bool s_using_raintegration = false;
#endif

	static std::recursive_mutex s_achievements_mutex;
	static rc_runtime_t s_rcheevos_runtime;
	static std::string s_game_icon_cache_directory;
	static std::string s_achievement_icon_cache_directory;
	static std::unique_ptr<Common::HTTPDownloader> s_http_downloader;

	static std::string s_username;
	static std::string s_api_token;

	static u32 s_last_game_crc;
	static std::string s_game_path;
	static std::string s_game_hash;
	static std::string s_game_title;
	static std::string s_game_icon;
	static std::vector<Achievements::Achievement> s_achievements;
	static std::vector<Achievements::Leaderboard> s_leaderboards;
	static std::atomic<u32> s_primed_achievement_count{0};

	static bool s_has_rich_presence = false;
	static std::string s_rich_presence_string;
	static Common::Timer s_last_ping_time;

	static u32 s_last_queried_lboard = 0;
	static u32 s_submitting_lboard_id = 0;
	static std::optional<std::vector<Achievements::LeaderboardEntry>> s_lboard_entries;

	template <typename T>
	static const char* RAPIStructName();

#define RAPI_STRUCT_NAME(x) \
	template <> \
	const char* RAPIStructName<x>() \
	{ \
		return #x; \
	}

	RAPI_STRUCT_NAME(rc_api_login_request_t);
	RAPI_STRUCT_NAME(rc_api_fetch_image_request_t);
	RAPI_STRUCT_NAME(rc_api_resolve_hash_request_t);
	RAPI_STRUCT_NAME(rc_api_fetch_game_data_request_t);
	RAPI_STRUCT_NAME(rc_api_fetch_user_unlocks_request_t);
	RAPI_STRUCT_NAME(rc_api_start_session_request_t);
	RAPI_STRUCT_NAME(rc_api_ping_request_t);
	RAPI_STRUCT_NAME(rc_api_award_achievement_request_t);
	RAPI_STRUCT_NAME(rc_api_submit_lboard_entry_request_t);
	RAPI_STRUCT_NAME(rc_api_fetch_leaderboard_info_request_t);

	RAPI_STRUCT_NAME(rc_api_login_response_t);
	RAPI_STRUCT_NAME(rc_api_resolve_hash_response_t);
	RAPI_STRUCT_NAME(rc_api_fetch_game_data_response_t);
	RAPI_STRUCT_NAME(rc_api_ping_response_t);
	RAPI_STRUCT_NAME(rc_api_award_achievement_response_t);
	RAPI_STRUCT_NAME(rc_api_submit_lboard_entry_response_t);
	RAPI_STRUCT_NAME(rc_api_start_session_response_t);
	RAPI_STRUCT_NAME(rc_api_fetch_user_unlocks_response_t);
	RAPI_STRUCT_NAME(rc_api_fetch_leaderboard_info_response_t);

	// Unused for now.
	// RAPI_STRUCT_NAME(rc_api_response_t);
	// RAPI_STRUCT_NAME(rc_api_fetch_achievement_info_response_t);
	// RAPI_STRUCT_NAME(rc_api_fetch_games_list_response_t);

#undef RAPI_STRUCT_NAME

	template <typename T, int (*InitFunc)(rc_api_request_t*, const T*)>
	struct RAPIRequest : public T
	{
	private:
		rc_api_request_t api_request;

	public:
		RAPIRequest() { std::memset(this, 0, sizeof(*this)); }

		~RAPIRequest() { rc_api_destroy_request(&api_request); }

		void Send(Common::HTTPDownloader::Request::Callback callback) { Send(s_http_downloader.get(), std::move(callback)); }

		void Send(Common::HTTPDownloader* http_downloader, Common::HTTPDownloader::Request::Callback callback)
		{
			const int error = InitFunc(&api_request, this);
			if (error != RC_OK)
			{
				FormattedError("%s failed: error %d (%s)", RAPIStructName<T>(), error, rc_error_str(error));
				callback(-1, std::string(), Common::HTTPDownloader::Request::Data());
				return;
			}

			if (api_request.post_data)
			{
				// needs to be a post
				http_downloader->CreatePostRequest(api_request.url, api_request.post_data, std::move(callback));
			}
			else
			{
				// get is fine
				http_downloader->CreateRequest(api_request.url, std::move(callback));
			}
		}

		bool DownloadImage(std::string cache_filename)
		{
			const int error = InitFunc(&api_request, this);
			if (error != RC_OK)
			{
				FormattedError("%s failed: error %d (%s)", RAPIStructName<T>(), error, rc_error_str(error));
				return false;
			}

			pxAssertMsg(!api_request.post_data, "Download request does not have POST data");
			Achievements::DownloadImage(api_request.url, std::move(cache_filename));
			return true;
		}

		std::string GetURL()
		{
			const int error = InitFunc(&api_request, this);
			if (error != RC_OK)
			{
				FormattedError("%s failed: error %d (%s)", RAPIStructName<T>(), error, rc_error_str(error));
				return std::string();
			}

			return api_request.url;
		}
	};

	template <typename T, int (*ParseFunc)(T*, const char*), void (*DestroyFunc)(T*)>
	struct RAPIResponse : public T
	{
	private:
		bool initialized = false;

	public:
		RAPIResponse(s32 status_code, Common::HTTPDownloader::Request::Data& data)
		{
			if (status_code != Common::HTTPDownloader::HTTP_OK || data.empty())
			{
				FormattedError("%s failed: empty response and/or status code %d", RAPIStructName<T>(), status_code);
				LogFailedResponseJSON(data);
				return;
			}

			// ensure null termination, rapi needs it
			data.push_back(0);

			const int error = ParseFunc(this, reinterpret_cast<const char*>(data.data()));
			initialized = (error == RC_OK);

			const rc_api_response_t& response = static_cast<T*>(this)->response;
			if (error != RC_OK)
			{
				FormattedError("%s failed: parse function returned %d (%s)", RAPIStructName<T>(), error, rc_error_str(error));
				LogFailedResponseJSON(data);
			}
			else if (!response.succeeded)
			{
				FormattedError("%s failed: %s", RAPIStructName<T>(), response.error_message ? response.error_message : "<no error>");
				LogFailedResponseJSON(data);
			}
		}

		~RAPIResponse()
		{
			if (initialized)
				DestroyFunc(this);
		}

		operator bool() const { return initialized && static_cast<const T*>(this)->response.succeeded; }
	};

} // namespace Achievements

#ifdef ENABLE_RAINTEGRATION
bool Achievements::IsUsingRAIntegration()
{
	return s_using_raintegration;
}
#endif

void Achievements::FormattedError(const char* format, ...)
{
	std::va_list ap;
	va_start(ap, format);
	std::string error(fmt::format("Achievements error: {}", StringUtil::StdStringFromFormatV(format, ap)));
	va_end(ap);

	Console.Error(error);
	Host::AddOSDMessage(std::move(error), 10.0f);
}

void Achievements::LogFailedResponseJSON(const Common::HTTPDownloader::Request::Data& data)
{
	const std::string str_data(reinterpret_cast<const char*>(data.data()), data.size());
	Console.Error("API call failed. Response JSON was:\n%s", str_data.c_str());
}

const Achievements::Achievement* Achievements::GetAchievementByID(u32 id)
{
	for (const Achievement& ach : s_achievements)
	{
		if (ach.id == id)
			return &ach;
	}

	return nullptr;
}

Achievements::Achievement* Achievements::GetMutableAchievementByID(u32 id)
{
	for (Achievement& ach : s_achievements)
	{
		if (ach.id == id)
			return &ach;
	}

	return nullptr;
}

void Achievements::ClearGameInfo(bool clear_achievements, bool clear_leaderboards)
{
	std::unique_lock lock(s_achievements_mutex);
	const bool had_game = (s_game_id != 0);

	if (clear_achievements)
	{
		while (!s_achievements.empty())
		{
			Achievement& ach = s_achievements.back();
			DeactivateAchievement(&ach);
			s_achievements.pop_back();
		}
		s_primed_achievement_count.store(0, std::memory_order_release);
	}
	if (clear_leaderboards)
	{
		while (!s_leaderboards.empty())
		{
			Leaderboard& lb = s_leaderboards.back();
			rc_runtime_deactivate_lboard(&s_rcheevos_runtime, lb.id);
			s_leaderboards.pop_back();
		}

		s_last_queried_lboard = 0;
		s_lboard_entries.reset();
	}

	if (s_achievements.empty() && s_leaderboards.empty())
	{
		// Ready to tear down cheevos completely
		s_game_title = {};
		s_game_icon = {};
		s_rich_presence_string = {};
		s_has_rich_presence = false;
		s_game_id = 0;

#ifdef ENABLE_DISCORD_PRESENCE
		if (EmuConfig.EnableDiscordPresence)
			CommonHost::UpdateDiscordPresence(s_rich_presence_string);
#endif
	}

	if (had_game)
		Host::OnAchievementsRefreshed();
}

void Achievements::ClearGameHash()
{
	s_last_game_crc = 0;
	std::string().swap(s_game_hash);
}

std::string Achievements::GetUserAgent()
{
	std::string ret;
	if (!PCSX2_isReleaseVersion && GIT_TAGGED_COMMIT)
		ret = fmt::format("PCSX2 Nightly - {} ({})", GIT_TAG, GetOSVersionString());
	else if (!PCSX2_isReleaseVersion)
		ret = fmt::format("PCSX2 {} ({})", GIT_REV, GetOSVersionString());
	else
		ret = fmt::format("PCSX2 {}.{}.{}-{} ({})", PCSX2_VersionHi, PCSX2_VersionMid, PCSX2_VersionLo, SVN_REV, GetOSVersionString());

	return ret;
}

void Achievements::BeginLoadingScreen(const char* text, bool* was_running_idle)
{
	GetMTGS().RunOnGSThread(&ImGuiManager::InitializeFullscreenUI);
	ImGuiFullscreen::OpenBackgroundProgressDialog("achievements_loading", text, 0, 0, 0);
}

void Achievements::EndLoadingScreen(bool was_running_idle)
{
	ImGuiFullscreen::CloseBackgroundProgressDialog("achievements_loading");
}

bool Achievements::IsActive()
{
	return s_active;
}

bool Achievements::IsLoggedIn()
{
	return s_logged_in;
}

bool Achievements::ChallengeModeActive()
{
#ifdef ENABLE_RAINTEGRATION
	if (IsUsingRAIntegration())
		return RA_HardcoreModeIsActive() != 0;
#endif

	return s_challenge_mode;
}

bool Achievements::LeaderboardsActive()
{
	return ChallengeModeActive() && EmuConfig.Achievements.Leaderboards;
}

bool Achievements::IsTestModeActive()
{
	return EmuConfig.Achievements.TestMode;
}

bool Achievements::IsUnofficialTestModeActive()
{
	return EmuConfig.Achievements.UnofficialTestMode;
}

bool Achievements::IsRichPresenceEnabled()
{
	return EmuConfig.Achievements.RichPresence;
}

bool Achievements::HasActiveGame()
{
	return s_game_id != 0;
}

u32 Achievements::GetGameID()
{
	return s_game_id;
}

std::unique_lock<std::recursive_mutex> Achievements::GetLock()
{
	return std::unique_lock(s_achievements_mutex);
}

void Achievements::Initialize()
{
	if (IsUsingRAIntegration())
		return;

	std::unique_lock lock(s_achievements_mutex);
	pxAssertRel(EmuConfig.Achievements.Enabled, "Achievements are enabled");

	s_http_downloader = Common::HTTPDownloader::Create(GetUserAgent().c_str());
	if (!s_http_downloader)
	{
		Host::ReportErrorAsync("Achievements Error", "Failed to create HTTPDownloader, cannot use achievements");
		return;
	}

	s_active = true;
	s_challenge_mode = false;
	rc_runtime_init(&s_rcheevos_runtime);
	EnsureCacheDirectoriesExist();

	s_last_ping_time.Reset();
	s_username = Host::GetBaseStringSettingValue("Achievements", "Username");
	s_api_token = Host::GetBaseStringSettingValue("Achievements", "Token");
	s_logged_in = (!s_username.empty() && !s_api_token.empty());

	if (VMManager::HasValidVM())
		GameChanged(VMManager::GetGameCRC());
}

void Achievements::UpdateSettings(const Pcsx2Config::AchievementsOptions& old_config)
{
	if (IsUsingRAIntegration())
		return;

	if (!EmuConfig.Achievements.Enabled)
	{
		// we're done here
		Shutdown();
		return;
	}

	if (!s_active)
	{
		// we just got enabled
		Initialize();
		return;
	}

	if (EmuConfig.Achievements.ChallengeMode != old_config.ChallengeMode)
	{
		// Hardcore mode can only be enabled through system boot/reset.
		if (s_challenge_mode && !EmuConfig.Achievements.ChallengeMode)
		{
			ResetChallengeMode();
		}
		else if (!s_challenge_mode && EmuConfig.Achievements.ChallengeMode)
		{
			if (HasActiveGame())
				ImGuiFullscreen::ShowToast(std::string(), "Hardcore mode will be enabled on system reset.", 10.0f);
		}
	}

	// We *could* handle these separately, but easier to just flush everything.
	if (EmuConfig.Achievements.TestMode != old_config.TestMode ||
		EmuConfig.Achievements.UnofficialTestMode != old_config.UnofficialTestMode ||
		EmuConfig.Achievements.RichPresence != old_config.RichPresence)
	{
		Shutdown();
		Initialize();
		return;
	}
	else if (EmuConfig.Achievements.Leaderboards != old_config.Leaderboards)
	{
		// If leaderboards were toggled, redisplay the summary (but only when HC mode is active,
		// because otherwise leaderboards are irrelevant anyway).
		if (HasActiveGame() && ChallengeModeActive())
			DisplayAchievementSummary();
	}

	// in case cache directory changed
	EnsureCacheDirectoriesExist();
}

bool Achievements::ConfirmChallengeModeDisable(const char* trigger)
{
#ifdef ENABLE_RAINTEGRATION
	if (IsUsingRAIntegration())
		return (RA_WarnDisableHardcore(trigger) != 0);
#endif

	const bool confirmed =
		Host::ConfirmMessage("Confirm Hardcore Mode", fmt::format("{0} cannot be performed while hardcore mode is active. Do you want to "
																  "disable hardcore mode? {0} will be cancelled if you select No.",
														  trigger));
	if (!confirmed)
		return false;

	DisableChallengeMode();
	return true;
}

void Achievements::DisableChallengeMode()
{
	if (!s_active)
		return;

#ifdef ENABLE_RAINTEGRATION
	if (IsUsingRAIntegration())
	{
		if (RA_HardcoreModeIsActive())
			RA_DisableHardcore();

		return;
	}
#endif

	if (s_challenge_mode)
		SetChallengeMode(false);
}

bool Achievements::ResetChallengeMode()
{
	if (!s_active)
		return false;

	Host::RemoveKeyedOSDMessage("challenge_mode_reset");

	if (s_challenge_mode == EmuConfig.Achievements.ChallengeMode)
		return false;

	SetChallengeMode(EmuConfig.Achievements.ChallengeMode);
	return true;
}

void Achievements::SetChallengeMode(bool enabled)
{
	if (enabled == s_challenge_mode)
		return;

	// new mode
	s_challenge_mode = enabled;

	if (HasActiveGame())
		ImGuiFullscreen::ShowToast(std::string(), enabled ? "Hardcore mode is now enabled." : "Hardcore mode is now disabled.", 10.0f);

	if (HasActiveGame() && !IsTestModeActive())
	{
		// deactivate, but don't clear all achievements (getting unlocks will reactivate them)
		std::unique_lock lock(s_achievements_mutex);
		for (Achievement& achievement : s_achievements)
		{
			DeactivateAchievement(&achievement);
			achievement.locked = true;
		}
		for (Leaderboard& leaderboard : s_leaderboards)
			rc_runtime_deactivate_lboard(&s_rcheevos_runtime, leaderboard.id);
	}

	// re-grab unlocks, this will reactivate what's locked in non-hardcore mode later on
	if (!s_achievements.empty())
		GetUserUnlocks();
}

bool Achievements::Shutdown()
{
#ifdef ENABLE_RAINTEGRATION
	if (IsUsingRAIntegration())
	{
		if (!RA_ConfirmLoadNewRom(true))
			return false;

		RA_SetPaused(false);
		RA_ActivateGame(0);
		return true;
	}
#endif

	if (!s_active)
		return true;

	std::unique_lock lock(s_achievements_mutex);
	s_http_downloader->WaitForAllRequests();

	ClearGameInfo();
	ClearGameHash();
	std::string().swap(s_username);
	std::string().swap(s_api_token);
	s_logged_in = false;
	Host::OnAchievementsRefreshed();

	s_challenge_mode = false;
	s_active = false;
	rc_runtime_destroy(&s_rcheevos_runtime);

	s_http_downloader.reset();
	return true;
}

bool Achievements::OnReset()
{
#ifdef ENABLE_RAINTEGRATION
	if (IsUsingRAIntegration())
	{
		if (!RA_ConfirmLoadNewRom(false))
			return false;

		RA_OnReset();
		return true;
	}
#endif

	if (!s_active)
		return true;

	std::unique_lock lock(s_achievements_mutex);

	// Clear the game out, we'll re-query it once the ELF is loaded.
	ClearGameInfo(true, true);
	ClearGameHash();

	// Reset runtime, there shouldn't be anything left in it though.
	DevCon.WriteLn("Resetting rcheevos state...");
	rc_runtime_reset(&s_rcheevos_runtime);
	return true;
}

void Achievements::OnPaused(bool paused)
{
#ifdef ENABLE_RAINTEGRATION
	if (IsUsingRAIntegration())
		RA_SetPaused(paused);
#endif
}

void Achievements::VSyncUpdate()
{
#ifdef ENABLE_RAINTEGRATION
	if (IsUsingRAIntegration())
	{
		RA_DoAchievementsFrame();
		return;
	}
#endif

	s_http_downloader->PollRequests();

	if (HasActiveGame())
	{
		std::unique_lock lock(s_achievements_mutex);
		rc_runtime_do_frame(&s_rcheevos_runtime, &CheevosEventHandler, &PeekMemory, nullptr, nullptr);
		UpdateRichPresence();

		if (!IsTestModeActive())
		{
			const s32 ping_frequency = EmuConfig.Achievements.RichPresence ? RICH_PRESENCE_PING_FREQUENCY : NO_RICH_PRESENCE_PING_FREQUENCY;
			if (static_cast<s32>(s_last_ping_time.GetTimeSeconds()) >= ping_frequency)
				SendPing();
		}
	}
}

void Achievements::ProcessPendingHTTPRequestsFromGSThread()
{
	// no need to do this if we're running, because VSyncUpdate() will take care of it
	if (VMManager::GetState() == VMState::Running)
		return;

	if (s_http_downloader->HasAnyRequests())
		Host::RunOnCPUThread([]() { s_http_downloader->PollRequests(); });
}

void Achievements::LoadState(const u8* state_data, u32 state_data_size)
{
	if (!s_active)
		return;

	// this assumes that the CRC and ELF name has been loaded prior to the cheevos state (it should be).
	if (ElfCRC != s_last_game_crc)
		GameChanged(ElfCRC);

#ifdef ENABLE_RAINTEGRATION
	if (IsUsingRAIntegration())
	{
		if (state_data_size == 0)
		{
			Console.Warning("State is missing cheevos data, resetting RAIntegration");
			RA_OnReset();
		}
		else
		{
			RA_RestoreState(reinterpret_cast<const char*>(state_data));
		}

		return;
	}
#endif

	// if we're active, make sure we've downloaded and activated all the achievements
	// before deserializing, otherwise that state's going to get lost.
	if (s_http_downloader->HasAnyRequests())
	{
		bool was_running_idle;
		BeginLoadingScreen("Downloading achievements data...", &was_running_idle);
		s_http_downloader->WaitForAllRequests();
		EndLoadingScreen(was_running_idle);
	}

	std::unique_lock lock(s_achievements_mutex);
	if (state_data_size == 0)
	{
		// reset runtime, no data (state might've been created without cheevos)
		Console.Warning("State is missing cheevos data, resetting runtime");
		rc_runtime_reset(&s_rcheevos_runtime);
		return;
	}

	// These routines scare me a bit.. the data isn't bounds checked.
	// Really hope that nobody puts any thing malicious in a save state...
	const int result = rc_runtime_deserialize_progress(&s_rcheevos_runtime, state_data, nullptr);
	if (result != RC_OK)
	{
		Console.Warning("Failed to deserialize cheevos state (%d), resetting", result);
		rc_runtime_reset(&s_rcheevos_runtime);
	}
}

std::vector<u8> Achievements::SaveState()
{
	std::vector<u8> ret;

#ifdef ENABLE_RAINTEGRATION
	if (IsUsingRAIntegration())
	{
		const int size = RA_CaptureState(nullptr, 0);

		const u32 data_size = (size >= 0) ? static_cast<u32>(size) : 0;
		ret.resize(data_size);

		const int result = RA_CaptureState(reinterpret_cast<char*>(ret.data()), static_cast<int>(data_size));
		if (result != static_cast<int>(data_size))
		{
			Console.Warning("Failed to serialize cheevos state from RAIntegration.");
			ret.clear();
		}

		return ret;
	}
#endif

	if (s_active)
	{
		std::unique_lock lock(s_achievements_mutex);

		// internally this happens twice.. not great.
		const int size = rc_runtime_progress_size(&s_rcheevos_runtime, nullptr);

		const u32 data_size = (size >= 0) ? static_cast<u32>(size) : 0;
		ret.resize(data_size);

		const int result = rc_runtime_serialize_progress(ret.data(), &s_rcheevos_runtime, nullptr);
		if (result != RC_OK)
		{
			// set data to zero, effectively serializing nothing
			Console.Warning("Failed to serialize cheevos state (%d)", result);
			ret.clear();
		}
	}

	return ret;
}

bool Achievements::SafeHasAchievementsOrLeaderboards()
{
	std::unique_lock lock(s_achievements_mutex);
	return !s_achievements.empty() || s_leaderboards.empty();
}

const std::string& Achievements::GetUsername()
{
	return s_username;
}

const std::string& Achievements::GetRichPresenceString()
{
	return s_rich_presence_string;
}

void Achievements::EnsureCacheDirectoriesExist()
{
	s_game_icon_cache_directory = Path::Combine(EmuFolders::Cache, "achievement_gameicon");
	s_achievement_icon_cache_directory = Path::Combine(EmuFolders::Cache, "achievement_badge");

	if (!FileSystem::DirectoryExists(s_game_icon_cache_directory.c_str()) &&
		!FileSystem::CreateDirectoryPath(s_game_icon_cache_directory.c_str(), false))
	{
		FormattedError("Failed to create cache directory '%s'", s_game_icon_cache_directory.c_str());
	}

	if (!FileSystem::DirectoryExists(s_achievement_icon_cache_directory.c_str()) &&
		!FileSystem::CreateDirectoryPath(s_achievement_icon_cache_directory.c_str(), false))
	{
		FormattedError("Failed to create cache directory '%s'", s_achievement_icon_cache_directory.c_str());
	}
}

void Achievements::LoginCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data)
{
	std::unique_lock lock(s_achievements_mutex);

	// NOTE: The strings here can be null if the server returns literal "null" (see rc_json_get_string()), hence the double-check.
	RAPIResponse<rc_api_login_response_t, rc_api_process_login_response, rc_api_destroy_login_response> response(status_code, data);
	if (!response || !response.username || !response.api_token)
	{
		FormattedError("Login failed. Please check your user name and password, and try again.");
		return;
	}

	std::string username(response.username);
	std::string api_token(response.api_token);

	// save to config
	Host::SetBaseStringSettingValue("Achievements", "Username", username.c_str());
	Host::SetBaseStringSettingValue("Achievements", "Token", api_token.c_str());
	Host::SetBaseStringSettingValue("Achievements", "LoginTimestamp", fmt::format("{}", std::time(nullptr)).c_str());
	Host::CommitBaseSettingChanges();

	if (s_active)
	{
		s_username = std::move(username);
		s_api_token = std::move(api_token);
		s_logged_in = true;

		// If we have a game running, set it up.
		if (!s_game_hash.empty())
			SendGetGameId();
	}
}

void Achievements::LoginASyncCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data)
{
	ImGuiFullscreen::CloseBackgroundProgressDialog("cheevos_async_login");

	LoginCallback(status_code, std::move(content_type), std::move(data));
}

void Achievements::SendLogin(
	const char* username, const char* password, Common::HTTPDownloader* http_downloader, Common::HTTPDownloader::Request::Callback callback)
{
	RAPIRequest<rc_api_login_request_t, rc_api_init_login_request> request;
	request.username = username;
	request.password = password;
	request.api_token = nullptr;
	request.Send(http_downloader, std::move(callback));
}

bool Achievements::LoginAsync(const char* username, const char* password)
{
	s_http_downloader->WaitForAllRequests();

	if (s_logged_in || std::strlen(username) == 0 || std::strlen(password) == 0 || IsUsingRAIntegration())
		return false;

	if (FullscreenUI::IsInitialized())
	{
		ImGuiFullscreen::OpenBackgroundProgressDialog("cheevos_async_login", "Logging in to RetroAchievements...", 0, 0, 0);
	}

	SendLogin(username, password, s_http_downloader.get(), LoginASyncCallback);
	return true;
}

bool Achievements::Login(const char* username, const char* password)
{
	if (s_active)
		s_http_downloader->WaitForAllRequests();

	if (s_logged_in || std::strlen(username) == 0 || std::strlen(password) == 0 || IsUsingRAIntegration())
		return false;

	if (s_active)
	{
		SendLogin(username, password, s_http_downloader.get(), LoginCallback);
		s_http_downloader->WaitForAllRequests();
		return IsLoggedIn();
	}

	// create a temporary downloader if we're not initialized
	pxAssertRel(!s_active, "RetroAchievements is not active on login");
	std::unique_ptr<Common::HTTPDownloader> http_downloader = Common::HTTPDownloader::Create(GetUserAgent().c_str());
	if (!http_downloader)
		return false;

	SendLogin(username, password, http_downloader.get(), LoginCallback);
	http_downloader->WaitForAllRequests();

	return !Host::GetBaseStringSettingValue("Achievements", "Token").empty();
}

void Achievements::Logout()
{
	if (s_active)
	{
		std::unique_lock lock(s_achievements_mutex);
		s_http_downloader->WaitForAllRequests();
		if (s_logged_in)
		{
			ClearGameInfo();
			std::string().swap(s_username);
			std::string().swap(s_api_token);
			s_logged_in = false;
			Host::OnAchievementsRefreshed();
		}
	}

	// remove from config
	Host::RemoveBaseSettingValue("Achievements", "Username");
	Host::RemoveBaseSettingValue("Achievements", "Token");
	Host::RemoveBaseSettingValue("Achievements", "LoginTimestamp");
	Host::CommitBaseSettingChanges();
}

void Achievements::DownloadImage(std::string url, std::string cache_filename)
{
	auto callback = [cache_filename](s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data) {
		if (status_code != HTTP_OK)
			return;

		if (!FileSystem::WriteBinaryFile(cache_filename.c_str(), data.data(), data.size()))
		{
			Console.Error("Failed to write badge image to '%s'", cache_filename.c_str());
			return;
		}

		GetMTGS().RunOnGSThread([cache_filename]() { ImGuiFullscreen::InvalidateCachedTexture(cache_filename); });
	};

	s_http_downloader->CreateRequest(std::move(url), std::move(callback));
}

void Achievements::DisplayAchievementSummary()
{
	if (FullscreenUI::IsInitialized() && EmuConfig.Achievements.Notifications)
	{
		std::string title = s_game_title;
		if (ChallengeModeActive())
			title += " (Hardcore Mode)";

		std::string summary;
		if (GetAchievementCount() > 0)
		{
			summary = StringUtil::StdStringFromFormat("You have earned %u of %u achievements, and %u of %u points.",
				GetUnlockedAchiementCount(), GetAchievementCount(), GetCurrentPointsForGame(), GetMaximumPointsForGame());
		}
		else
		{
			summary = "This game has no achievements.";
		}
		if (GetLeaderboardCount() > 0)
		{
			summary.push_back('\n');
			if (LeaderboardsActive())
				summary.append("Leaderboard submission is enabled.");
		}

		ImGuiFullscreen::AddNotification(10.0f, std::move(title), std::move(summary), s_game_icon);

		// Technically not going through the resource API, but since we're passing this to something else, we can't.
		if (EmuConfig.Achievements.SoundEffects)
			Common::PlaySoundAsync(Path::Combine(EmuFolders::Resources, INFO_SOUND_NAME).c_str());
	}
}

void Achievements::DisplayMasteredNotification()
{
	if (!FullscreenUI::IsInitialized() || !EmuConfig.Achievements.Notifications)
		return;

	std::string title(fmt::format("Mastered {}", s_game_title));
	std::string message(fmt::format(
		"{} achievements, {} points{}", GetAchievementCount(), GetCurrentPointsForGame(), s_challenge_mode ? " (Hardcore Mode)" : ""));

	ImGuiFullscreen::AddNotification(20.0f, std::move(title), std::move(message), s_game_icon);
}

void Achievements::GetUserUnlocksCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data)
{
	RAPIResponse<rc_api_fetch_user_unlocks_response_t, rc_api_process_fetch_user_unlocks_response,
		rc_api_destroy_fetch_user_unlocks_response>
		response(status_code, data);

	std::unique_lock lock(s_achievements_mutex);
	if (!response)
	{
		ClearGameInfo(true, false);
		return;
	}

	// flag achievements as unlocked
	for (u32 i = 0; i < response.num_achievement_ids; i++)
	{
		Achievement* cheevo = GetMutableAchievementByID(response.achievement_ids[i]);
		if (!cheevo)
		{
			Console.Error("Server returned unknown achievement %u", response.achievement_ids[i]);
			continue;
		}

		cheevo->locked = false;
	}

	// start scanning for locked achievements
	ActivateLockedAchievements();
	DisplayAchievementSummary();
	SendPlaying();
	UpdateRichPresence();
	SendPing();
	Host::OnAchievementsRefreshed();
}

void Achievements::GetUserUnlocks()
{
	RAPIRequest<rc_api_fetch_user_unlocks_request_t, rc_api_init_fetch_user_unlocks_request> request;
	request.username = s_username.c_str();
	request.api_token = s_api_token.c_str();
	request.game_id = s_game_id;
	request.hardcore = static_cast<int>(ChallengeModeActive());
	request.Send(GetUserUnlocksCallback);
}

void Achievements::GetPatchesCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data)
{
	if (!VMManager::HasValidVM())
		return;

	RAPIResponse<rc_api_fetch_game_data_response_t, rc_api_process_fetch_game_data_response, rc_api_destroy_fetch_game_data_response>
		response(status_code, data);

	std::unique_lock lock(s_achievements_mutex);
	ClearGameInfo();
	if (!response || !response.title)
		return;

	// ensure fullscreen UI is ready
	GetMTGS().RunOnGSThread(&ImGuiManager::InitializeFullscreenUI);

	s_game_id = response.id;
	s_game_title = response.title;

	// try for a icon
	if (response.image_name && std::strlen(response.image_name) > 0)
	{
		s_game_icon = Path::Combine(s_game_icon_cache_directory, fmt::format("{}.png", s_game_id));
		if (!FileSystem::FileExists(s_game_icon.c_str()))
		{
			RAPIRequest<rc_api_fetch_image_request_t, rc_api_init_fetch_image_request> request;
			request.image_name = response.image_name;
			request.image_type = RC_IMAGE_TYPE_GAME;
			request.DownloadImage(s_game_icon);
		}
	}

	// parse achievements
	for (u32 i = 0; i < response.num_achievements; i++)
	{
		const rc_api_achievement_definition_t& defn = response.achievements[i];

		// Skip local and unofficial achievements for now, unless "Test Unofficial Achievements" is enabled
		if (defn.category == RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL)
		{
			if (!IsUnofficialTestModeActive())
			{
				Console.Warning("Skipping unofficial achievement %u (%s)", defn.id, defn.title);
				continue;
			}
		}
		// local achievements shouldn't be in this list, but just in case?
		else if (defn.category != RC_ACHIEVEMENT_CATEGORY_CORE)
		{
			continue;
		}

		if (GetMutableAchievementByID(defn.id))
		{
			Console.Error("Achievement %u already exists", defn.id);
			continue;
		}

		if (!defn.definition || !defn.title || !defn.description || !defn.badge_name)
		{
			Console.Error("Incomplete achievement %u", defn.id);
			continue;
		}

		Achievement cheevo;
		cheevo.id = defn.id;
		cheevo.memaddr = defn.definition;
		cheevo.title = defn.title;
		cheevo.description = defn.description;
		cheevo.badge_name = defn.badge_name;
		cheevo.locked = true;
		cheevo.active = false;
		cheevo.primed = false;
		cheevo.points = defn.points;
		cheevo.category = static_cast<AchievementCategory>(defn.category);
		s_achievements.push_back(std::move(cheevo));
	}

	for (u32 i = 0; i < response.num_leaderboards; i++)
	{
		const rc_api_leaderboard_definition_t& defn = response.leaderboards[i];
		if (!defn.title || !defn.description || !defn.definition)
		{
			Console.Error("Incomplete leaderboard %u", defn.id);
			continue;
		}

		Leaderboard lboard;
		lboard.id = defn.id;
		lboard.title = defn.title;
		lboard.description = defn.description;
		lboard.format = defn.format;
		s_leaderboards.push_back(std::move(lboard));

		// Always track the leaderboard, if we don't have leaderboards enabled we just won't submit it.
		// That way if someone activates them later on, current progress will count.
		const int err = rc_runtime_activate_lboard(&s_rcheevos_runtime, defn.id, defn.definition, nullptr, 0);
		if (err != RC_OK)
			Console.Error("Leaderboard %u memaddr parse error: %s", defn.id, rc_error_str(err));
		else
			DevCon.WriteLn("Activated leaderboard %s (%u)", defn.title, defn.id);
	}

	// Parse rich presence.
	if (response.rich_presence_script && std::strlen(response.rich_presence_script) > 0)
	{
		const int res = rc_runtime_activate_richpresence(&s_rcheevos_runtime, response.rich_presence_script, nullptr, 0);
		if (res == RC_OK)
			s_has_rich_presence = true;
		else
			Console.Warning("Failed to activate rich presence: %s", rc_error_str(res));
	}

	Console.WriteLn("Game Title: %s", s_game_title.c_str());
	Console.WriteLn("Achievements: %zu", s_achievements.size());
	Console.WriteLn("Leaderboards: %zu", s_leaderboards.size());

	// We don't want to block saving/loading states when there's no achievements.
	if (s_achievements.empty() && s_leaderboards.empty())
		DisableChallengeMode();

	if (!s_achievements.empty() || s_has_rich_presence)
	{
		if (!IsTestModeActive())
		{
			GetUserUnlocks();
		}
		else
		{
			ActivateLockedAchievements();
			DisplayAchievementSummary();
			Host::OnAchievementsRefreshed();
		}
	}
	else
	{
		DisplayAchievementSummary();
	}

	if (s_achievements.empty() && s_leaderboards.empty() && !s_has_rich_presence)
	{
		ClearGameInfo();
	}
}

void Achievements::GetLbInfoCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data)
{
	if (!VMManager::HasValidVM())
		return;

	RAPIResponse<rc_api_fetch_leaderboard_info_response_t, rc_api_process_fetch_leaderboard_info_response,
		rc_api_destroy_fetch_leaderboard_info_response>
		response(status_code, data);
	if (!response)
		return;

	std::unique_lock lock(s_achievements_mutex);
	if (response.id != s_last_queried_lboard)
	{
		// User has already requested another leaderboard, drop this data
		return;
	}

	const Leaderboard* leaderboard = GetLeaderboardByID(response.id);
	if (!leaderboard)
	{
		Console.Error("Attempting to list unknown leaderboard %u", response.id);
		return;
	}

	s_lboard_entries = std::vector<Achievements::LeaderboardEntry>();
	for (u32 i = 0; i < response.num_entries; i++)
	{
		const rc_api_lboard_info_entry_t& entry = response.entries[i];
		if (!entry.username)
			continue;

		char score[128];
		rc_runtime_format_lboard_value(score, sizeof(score), entry.score, leaderboard->format);

		LeaderboardEntry lbe;
		lbe.user = entry.username;
		lbe.rank = entry.rank;
		lbe.formatted_score = score;
		lbe.submitted = entry.submitted;
		lbe.is_self = lbe.user == s_username;

		s_lboard_entries->push_back(std::move(lbe));
	}
}

void Achievements::GetPatches(u32 game_id)
{
	RAPIRequest<rc_api_fetch_game_data_request_t, rc_api_init_fetch_game_data_request> request;
	request.username = s_username.c_str();
	request.api_token = s_api_token.c_str();
	request.game_id = game_id;
	request.Send(GetPatchesCallback);
}

std::string_view Achievements::GetELFNameForHash(const std::string& elf_path)
{
	std::string::size_type start = elf_path.rfind('\\');
	if (start == std::string::npos)
		start = 0;
	else
		start++; // skip backslash

	std::string::size_type end = elf_path.rfind(';');
	if (end == std::string::npos)
		end = elf_path.size();

	if (end < start)
		end = start;

	return std::string_view(elf_path).substr(start, end - start);
}

std::optional<std::vector<u8>> Achievements::ReadELFFromCurrentDisc(const std::string& elf_path)
{
	// This CDVD stuff is super nasty and full of exceptions..
	std::optional<std::vector<u8>> ret;
	try
	{
		IsoFSCDVD isofs;
		IsoFile file(isofs, elf_path);
		const u32 size = file.getLength();

		ret = std::vector<u8>();
		ret->resize(size);

		if (size > 0)
		{
			const s32 bytes_read = file.read(ret->data(), static_cast<s32>(size));
			if (bytes_read != static_cast<s32>(size))
			{
				Console.Error("(Achievements) Only read %d of %u bytes of ELF '%s'", bytes_read, size, elf_path.c_str());
				ret.reset();
			}
		}
	}
	catch (...)
	{
		Console.Error("(Achievements) Caught exception while trying to read ELF '%s'.", elf_path.c_str());
		ret.reset();
	}

	return ret;
}

std::string Achievements::GetGameHash()
{
	const std::string& elf_path = LastELF;
	if (elf_path.empty())
		return {};

	// this.. really shouldn't be invalid
	const std::string_view name_for_hash(GetELFNameForHash(elf_path));
	if (name_for_hash.empty())
		return {};

	std::optional<std::vector<u8>> elf_data(ReadELFFromCurrentDisc(elf_path));
	if (!elf_data.has_value())
		return {};

	// See rcheevos hash.c - rc_hash_ps2().
	const u32 MAX_HASH_SIZE = 64 * 1024 * 1024;
	const u32 hash_size = std::min<u32>(static_cast<u32>(elf_data->size()), MAX_HASH_SIZE);
	pxAssert(hash_size <= elf_data->size());

	MD5Digest digest;
	if (!name_for_hash.empty())
		digest.Update(name_for_hash.data(), static_cast<u32>(name_for_hash.size()));
	if (hash_size > 0)
		digest.Update(elf_data->data(), hash_size);

	u8 hash[16];
	digest.Final(hash);

	std::string hash_str(
		StringUtil::StdStringFromFormat("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", hash[0], hash[1], hash[2],
			hash[3], hash[4], hash[5], hash[6], hash[7], hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]));

	Console.WriteLn("Hash for '%.*s' (%zu bytes, %u bytes hashed): %s", static_cast<int>(name_for_hash.size()), name_for_hash.data(),
		elf_data->size(), hash_size, hash_str.c_str());
	return hash_str;
}

void Achievements::GetGameIdCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data)
{
	if (!VMManager::HasValidVM())
		return;

	RAPIResponse<rc_api_resolve_hash_response_t, rc_api_process_resolve_hash_response, rc_api_destroy_resolve_hash_response> response(
		status_code, data);
	if (!response)
		return;

	const u32 game_id = response.game_id;
	Console.WriteLn("Server returned GameID %u", game_id);
	if (game_id == 0)
	{
		// We don't want to block saving/loading states when there's no achievements.
		DisableChallengeMode();
		return;
	}

	GetPatches(game_id);
}

void Achievements::GameChanged(u32 crc)
{
	std::unique_lock lock(s_achievements_mutex);
	if (!s_active)
		return;

	// avoid reading+hashing the executable if the crc hasn't changed
	if (s_last_game_crc == crc)
		return;

	std::string game_hash(GetGameHash());
	if (s_game_hash == game_hash)
		return;

	if (!IsUsingRAIntegration() && s_http_downloader->HasAnyRequests())
	{
		bool was_running_idle;
		BeginLoadingScreen("Downloading achievements data...", &was_running_idle);
		s_http_downloader->WaitForAllRequests();
		EndLoadingScreen(was_running_idle);
	}

	ClearGameInfo();
	ClearGameHash();
	s_last_game_crc = crc;
	s_game_hash = std::move(game_hash);

#ifdef ENABLE_RAINTEGRATION
	if (IsUsingRAIntegration())
	{
		RAIntegration::GameChanged();
		return;
	}
#endif

	if (s_game_hash.empty())
	{
		// when we're booting the bios, or shutting down, this will fail
		if (crc != 0)
		{
			Host::AddKeyedOSDMessage("retroachievements_disc_read_failed", "Failed to read executable from disc. Achievements disabled.",
				Host::OSD_CRITICAL_ERROR_DURATION);
		}

		s_last_game_crc = 0;
		return;
	}

	if (IsLoggedIn())
		SendGetGameId();
}

void Achievements::SendGetGameId()
{
	RAPIRequest<rc_api_resolve_hash_request_t, rc_api_init_resolve_hash_request> request;
	request.username = s_username.c_str();
	request.api_token = s_api_token.c_str();
	request.game_hash = s_game_hash.c_str();
	request.Send(GetGameIdCallback);
}

void Achievements::SendPlayingCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data)
{
	if (!VMManager::HasValidVM())
		return;

	RAPIResponse<rc_api_start_session_response_t, rc_api_process_start_session_response, rc_api_destroy_start_session_response> response(
		status_code, data);
	if (!response)
		return;

	Console.WriteLn("Playing game updated to %u (%s)", s_game_id, s_game_title.c_str());
}

void Achievements::SendPlaying()
{
	if (!HasActiveGame())
		return;

	RAPIRequest<rc_api_start_session_request_t, rc_api_init_start_session_request> request;
	request.username = s_username.c_str();
	request.api_token = s_api_token.c_str();
	request.game_id = s_game_id;
	request.Send(SendPlayingCallback);
}

void Achievements::UpdateRichPresence()
{
	if (!s_has_rich_presence)
		return;

	char buffer[512];
	const int res = rc_runtime_get_richpresence(&s_rcheevos_runtime, buffer, sizeof(buffer), PeekMemory, nullptr, nullptr);

	std::unique_lock lock(s_achievements_mutex);
	const bool had_rich_presence = !s_rich_presence_string.empty();
	if (res <= 0)
	{
		if (!had_rich_presence)
			return;

		s_rich_presence_string.clear();
	}
	else
	{
		if (s_rich_presence_string == buffer)
			return;

		s_rich_presence_string.assign(buffer);
	}

	Host::OnAchievementsRefreshed();

#ifdef ENABLE_DISCORD_PRESENCE
	if (EmuConfig.EnableDiscordPresence)
		CommonHost::UpdateDiscordPresence(s_rich_presence_string);
#endif
}

void Achievements::SendPingCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data)
{
	if (!VMManager::HasValidVM())
		return;

	RAPIResponse<rc_api_ping_response_t, rc_api_process_ping_response, rc_api_destroy_ping_response> response(status_code, data);
}

void Achievements::SendPing()
{
	if (!HasActiveGame())
		return;

	s_last_ping_time.Reset();

	RAPIRequest<rc_api_ping_request_t, rc_api_init_ping_request> request;
	request.api_token = s_api_token.c_str();
	request.username = s_username.c_str();
	request.game_id = s_game_id;
	request.rich_presence = s_rich_presence_string.c_str();
	request.Send(SendPingCallback);
}

const std::string& Achievements::GetGameTitle()
{
	return s_game_title;
}

const std::string& Achievements::GetGameIcon()
{
	return s_game_icon;
}

bool Achievements::EnumerateAchievements(std::function<bool(const Achievement&)> callback)
{
	for (const Achievement& cheevo : s_achievements)
	{
		if (!callback(cheevo))
			return false;
	}

	return true;
}

u32 Achievements::GetUnlockedAchiementCount()
{
	u32 count = 0;
	for (const Achievement& cheevo : s_achievements)
	{
		if (!cheevo.locked)
			count++;
	}

	return count;
}

u32 Achievements::GetAchievementCount()
{
	return static_cast<u32>(s_achievements.size());
}

u32 Achievements::GetMaximumPointsForGame()
{
	u32 points = 0;
	for (const Achievement& cheevo : s_achievements)
		points += cheevo.points;

	return points;
}

u32 Achievements::GetCurrentPointsForGame()
{
	u32 points = 0;
	for (const Achievement& cheevo : s_achievements)
	{
		if (!cheevo.locked)
			points += cheevo.points;
	}

	return points;
}

bool Achievements::EnumerateLeaderboards(std::function<bool(const Leaderboard&)> callback)
{
	for (const Leaderboard& lboard : s_leaderboards)
	{
		if (!callback(lboard))
			return false;
	}

	return true;
}

std::optional<bool> Achievements::TryEnumerateLeaderboardEntries(u32 id, std::function<bool(const LeaderboardEntry&)> callback)
{
	if (id == s_last_queried_lboard)
	{
		if (s_lboard_entries)
		{
			for (const LeaderboardEntry& entry : *s_lboard_entries)
			{
				if (!callback(entry))
					return false;
			}
			return true;
		}
	}
	else
	{
		s_last_queried_lboard = id;
		s_lboard_entries.reset();

		// TODO: Add paging? For now, stick to defaults
		RAPIRequest<rc_api_fetch_leaderboard_info_request_t, rc_api_init_fetch_leaderboard_info_request> request;
		request.username = s_username.c_str();
		request.leaderboard_id = id;
		request.first_entry = 0;

		// Just over what a single page can store, should be a reasonable amount for now
		request.count = 15;

		request.Send(GetLbInfoCallback);
	}

	return std::nullopt;
}

const Achievements::Leaderboard* Achievements::GetLeaderboardByID(u32 id)
{
	for (const Leaderboard& lb : s_leaderboards)
	{
		if (lb.id == id)
			return &lb;
	}

	return nullptr;
}

u32 Achievements::GetLeaderboardCount()
{
	return static_cast<u32>(s_leaderboards.size());
}

bool Achievements::IsLeaderboardTimeType(const Leaderboard& leaderboard)
{
	return leaderboard.format != RC_FORMAT_SCORE && leaderboard.format != RC_FORMAT_VALUE;
}

bool Achievements::IsMastered()
{
	for (const Achievement& cheevo : s_achievements)
	{
		if (cheevo.locked)
			return false;
	}

	return true;
}

void Achievements::ActivateLockedAchievements()
{
	for (Achievement& cheevo : s_achievements)
	{
		if (cheevo.locked)
			ActivateAchievement(&cheevo);
	}
}

bool Achievements::ActivateAchievement(Achievement* achievement)
{
	if (achievement->active)
		return true;

	const int err = rc_runtime_activate_achievement(&s_rcheevos_runtime, achievement->id, achievement->memaddr.c_str(), nullptr, 0);
	if (err != RC_OK)
	{
		Console.Error("Achievement %u memaddr parse error: %s", achievement->id, rc_error_str(err));
		return false;
	}

	achievement->active = true;

	DevCon.WriteLn("Activated achievement %s (%u)", achievement->title.c_str(), achievement->id);
	return true;
}

void Achievements::DeactivateAchievement(Achievement* achievement)
{
	if (!achievement->active)
		return;

	rc_runtime_deactivate_achievement(&s_rcheevos_runtime, achievement->id);
	achievement->active = false;

	if (achievement->primed)
	{
		achievement->primed = false;
		s_primed_achievement_count.fetch_sub(std::memory_order_acq_rel);
	}

	DevCon.WriteLn("Deactivated achievement %s (%u)", achievement->title.c_str(), achievement->id);
}

void Achievements::UnlockAchievementCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data)
{
	if (!VMManager::HasValidVM())
		return;

	RAPIResponse<rc_api_award_achievement_response_t, rc_api_process_award_achievement_response, rc_api_destroy_award_achievement_response>
		response(status_code, data);
	if (!response)
		return;

	Console.WriteLn("Successfully unlocked achievement %u, new score %u", response.awarded_achievement_id, response.new_player_score);
}

void Achievements::SubmitLeaderboardCallback(s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data)
{
	if (!VMManager::HasValidVM())
		return;

	RAPIResponse<rc_api_submit_lboard_entry_response_t, rc_api_process_submit_lboard_entry_response,
		rc_api_destroy_submit_lboard_entry_response>
		response(status_code, data);
	if (!response)
		return;

	// Force the next leaderboard query to repopulate everything, just in case the user wants to see their new score
	s_last_queried_lboard = 0;

	// RA API doesn't send us the leaderboard ID back.. hopefully we don't submit two at once :/
	if (s_submitting_lboard_id == 0)
		return;

	const Leaderboard* lb = GetLeaderboardByID(std::exchange(s_submitting_lboard_id, 0u));
	if (!lb || !FullscreenUI::IsInitialized() || !EmuConfig.Achievements.Notifications)
		return;

	char submitted_score[128];
	char best_score[128];
	rc_runtime_format_lboard_value(submitted_score, sizeof(submitted_score), response.submitted_score, lb->format);
	rc_runtime_format_lboard_value(best_score, sizeof(best_score), response.best_score, lb->format);

	std::string summary = fmt::format(
		"Your Score: {} (Best: {})\nLeaderboard Position: {} of {}", submitted_score, best_score, response.new_rank, response.num_entries);

	ImGuiFullscreen::AddNotification(10.0f, lb->title, std::move(summary), s_game_icon);
}

void Achievements::UnlockAchievement(u32 achievement_id, bool add_notification /* = true*/)
{
	std::unique_lock lock(s_achievements_mutex);
	Achievement* achievement = GetMutableAchievementByID(achievement_id);
	if (!achievement)
	{
		Console.Error("Attempting to unlock unknown achievement %u", achievement_id);
		return;
	}
	else if (!achievement->locked)
	{
		Console.Warning("Achievement %u for game %u is already unlocked", achievement_id, s_game_id);
		return;
	}

	achievement->locked = false;
	DeactivateAchievement(achievement);

	Console.WriteLn("Achievement %s (%u) for game %u unlocked", achievement->title.c_str(), achievement_id, s_game_id);

	if (FullscreenUI::IsInitialized() && EmuConfig.Achievements.Notifications)
	{
		std::string title;
		switch (achievement->category)
		{
			case AchievementCategory::Local:
				title = fmt::format("{} (Local)", achievement->title);
				break;
			case AchievementCategory::Unofficial:
				title = fmt::format("{} (Unofficial)", achievement->title);
				break;
			case AchievementCategory::Core:
			default:
				title = achievement->title;
				break;
		}

		ImGuiFullscreen::AddNotification(15.0f, std::move(title), achievement->description, GetAchievementBadgePath(*achievement));
		if (EmuConfig.Achievements.SoundEffects)
			Common::PlaySoundAsync(Path::Combine(EmuFolders::Resources, UNLOCK_SOUND_NAME).c_str());
	}

	if (IsMastered())
		DisplayMasteredNotification();

	if (IsTestModeActive())
	{
		Console.Warning("Skipping sending achievement %u unlock to server because of test mode.", achievement_id);
		return;
	}

	if (achievement->category != AchievementCategory::Core)
	{
		Console.Warning("Skipping sending achievement %u unlock to server because it's not from the core set.", achievement_id);
		return;
	}

	RAPIRequest<rc_api_award_achievement_request_t, rc_api_init_award_achievement_request> request;
	request.username = s_username.c_str();
	request.api_token = s_api_token.c_str();
	request.game_hash = s_game_hash.c_str();
	request.achievement_id = achievement_id;
	request.hardcore = static_cast<int>(ChallengeModeActive());
	request.Send(UnlockAchievementCallback);
}

void Achievements::AchievementPrimed(u32 achievement_id)
{
	std::unique_lock lock(s_achievements_mutex);
	Achievement* achievement = GetMutableAchievementByID(achievement_id);
	if (!achievement || achievement->primed)
		return;

	achievement->primed = true;
	s_primed_achievement_count.fetch_add(std::memory_order_acq_rel);
}

void Achievements::AchievementUnprimed(u32 achievement_id)
{
	std::unique_lock lock(s_achievements_mutex);
	Achievement* achievement = GetMutableAchievementByID(achievement_id);
	if (!achievement || !achievement->primed)
		return;

	achievement->primed = false;
	s_primed_achievement_count.fetch_sub(std::memory_order_acq_rel);
}

void Achievements::SubmitLeaderboard(u32 leaderboard_id, int value)
{
	if (IsTestModeActive())
	{
		Console.Warning("Skipping sending leaderboard %u result to server because of test mode.", leaderboard_id);
		return;
	}

	if (!ChallengeModeActive())
	{
		Console.Warning("Skipping sending leaderboard %u result to server because Challenge mode is off.", leaderboard_id);
		return;
	}

	if (!LeaderboardsActive())
	{
		Console.Warning("Skipping sending leaderboard %u result to server because leaderboards are disabled.", leaderboard_id);
		return;
	}

	std::unique_lock lock(s_achievements_mutex);
	s_submitting_lboard_id = leaderboard_id;

	RAPIRequest<rc_api_submit_lboard_entry_request_t, rc_api_init_submit_lboard_entry_request> request;
	request.username = s_username.c_str();
	request.api_token = s_api_token.c_str();
	request.game_hash = s_game_hash.c_str();
	request.leaderboard_id = leaderboard_id;
	request.score = value;
	request.Send(SubmitLeaderboardCallback);

	// Technically not going through the resource API, but since we're passing this to something else, we can't.
	if (EmuConfig.Achievements.SoundEffects)
		Common::PlaySoundAsync(Path::Combine(EmuFolders::Resources, LBSUBMIT_SOUND_NAME).c_str());
}

std::pair<u32, u32> Achievements::GetAchievementProgress(const Achievement& achievement)
{
	std::pair<u32, u32> result;
	rc_runtime_get_achievement_measured(&s_rcheevos_runtime, achievement.id, &result.first, &result.second);
	return result;
}

std::string Achievements::GetAchievementProgressText(const Achievement& achievement)
{
	char buf[256];
	rc_runtime_format_achievement_measured(&s_rcheevos_runtime, achievement.id, buf, std::size(buf));
	return buf;
}

const std::string& Achievements::GetAchievementBadgePath(const Achievement& achievement, bool download_if_missing, bool force_unlocked_icon)
{
	const bool use_locked = (achievement.locked && !force_unlocked_icon);
	std::string& badge_path = use_locked ? achievement.locked_badge_path : achievement.unlocked_badge_path;
	if (!badge_path.empty() || achievement.badge_name.empty())
		return badge_path;

	// well, this comes from the internet.... :)
	std::string clean_name(Path::SanitizeFileName(achievement.badge_name));
	badge_path = Path::Combine(s_achievement_icon_cache_directory, fmt::format("{}{}.png", clean_name, use_locked ? "_lock" : ""));
	if (FileSystem::FileExists(badge_path.c_str()))
		return badge_path;

	// need to download it
	if (download_if_missing)
	{
		RAPIRequest<rc_api_fetch_image_request_t, rc_api_init_fetch_image_request> request;
		request.image_name = achievement.badge_name.c_str();
		request.image_type = use_locked ? RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED : RC_IMAGE_TYPE_ACHIEVEMENT;
		request.DownloadImage(badge_path);
	}

	return badge_path;
}

std::string Achievements::GetAchievementBadgeURL(const Achievement& achievement)
{
	RAPIRequest<rc_api_fetch_image_request_t, rc_api_init_fetch_image_request> request;
	request.image_name = achievement.badge_name.c_str();
	request.image_type = achievement.locked ? RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED : RC_IMAGE_TYPE_ACHIEVEMENT;
	return request.GetURL();
}

u32 Achievements::GetPrimedAchievementCount()
{
	// Relaxed is fine here, worst that happens is we draw the triggers one frame late.
	return s_primed_achievement_count.load(std::memory_order_relaxed);
}

void Achievements::CheevosEventHandler(const rc_runtime_event_t* runtime_event)
{
	static const char* events[] = {"RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED", "RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED",
		"RC_RUNTIME_EVENT_ACHIEVEMENT_RESET", "RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED", "RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED",
		"RC_RUNTIME_EVENT_LBOARD_STARTED", "RC_RUNTIME_EVENT_LBOARD_CANCELED", "RC_RUNTIME_EVENT_LBOARD_UPDATED",
		"RC_RUNTIME_EVENT_LBOARD_TRIGGERED", "RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED", "RC_RUNTIME_EVENT_LBOARD_DISABLED"};
	const char* event_text = ((unsigned)runtime_event->type >= std::size(events)) ? "unknown" : events[(unsigned)runtime_event->type];
	DevCon.WriteLn("Cheevos Event %s for %u", event_text, runtime_event->id);

	switch (runtime_event->type)
	{
		case RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED:
			UnlockAchievement(runtime_event->id);
			break;

		case RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED:
			AchievementPrimed(runtime_event->id);
			break;

		case RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED:
			AchievementUnprimed(runtime_event->id);
			break;

		case RC_RUNTIME_EVENT_LBOARD_TRIGGERED:
			SubmitLeaderboard(runtime_event->id, runtime_event->value);
			break;

		default:
			break;
	}
}

unsigned Achievements::PeekMemory(unsigned address, unsigned num_bytes, void* ud)
{
	if ((static_cast<u64>(address) + num_bytes) >= EXPOSED_EE_MEMORY_SIZE)
	{
		DevCon.Warning("[Achievements] Ignoring out of bounds memory peek of %u bytes at %08X.", num_bytes, address);
		return 0u;
	}

	switch (num_bytes)
	{
		case 1:
		{
			u8 value;
			std::memcpy(&value, reinterpret_cast<u8*>(eeMem) + address, sizeof(value));
			return static_cast<unsigned>(value);
		}

		case 2:
		{
			u16 value;
			std::memcpy(&value, reinterpret_cast<u8*>(eeMem) + address, sizeof(value));
			return static_cast<unsigned>(value);
		}

		case 4:
		{
			u32 value;
			std::memcpy(&value, reinterpret_cast<u8*>(eeMem) + address, sizeof(value));
			return static_cast<unsigned>(value);
		}

		default:
			return 0u;
	}
}

unsigned Achievements::PeekMemoryBlock(unsigned address, unsigned char* buffer, unsigned num_bytes)
{
	if ((address >= EXPOSED_EE_MEMORY_SIZE))
	{
		DevCon.Warning("[Achievements] Ignoring out of bounds block memory read for %u bytes at %08X.", num_bytes, address);
		return 0u;
	}

	const unsigned read_byte_count = std::min<unsigned>(EXPOSED_EE_MEMORY_SIZE - address, num_bytes);
	std::memcpy(buffer, reinterpret_cast<u8*>(eeMem) + address, read_byte_count);
	return read_byte_count;
}

void Achievements::PokeMemory(unsigned address, unsigned num_bytes, void* ud, unsigned value)
{
	if ((static_cast<u64>(address) + num_bytes) >= EXPOSED_EE_MEMORY_SIZE)
	{
		DevCon.Warning("[Achievements] Ignoring out of bounds memory poke of %u bytes at %08X (value %08X).", num_bytes, address, value);
		return;
	}

	switch (num_bytes)
	{
		case 1:
		{
			const u8 value8 = static_cast<u8>(value);
			std::memcpy(reinterpret_cast<u8*>(eeMem) + address, &value8, sizeof(value8));
		}
		break;

		case 2:
		{
			const u16 value16 = static_cast<u16>(value);
			std::memcpy(reinterpret_cast<u8*>(eeMem) + address, &value16, sizeof(value16));
		}
		break;

		case 4:
		{
			const u32 value32 = static_cast<u32>(value);
			std::memcpy(reinterpret_cast<u8*>(eeMem) + address, &value32, sizeof(value32));
		}
		break;

		default:
			break;
	}
}

#ifdef ENABLE_RAINTEGRATION

#include "RA_Consoles.h"

namespace Achievements::RAIntegration
{
	static void InitializeRAIntegration(void* main_window_handle);

	static int RACallbackIsActive();
	static void RACallbackCauseUnpause();
	static void RACallbackCausePause();
	static void RACallbackRebuildMenu();
	static void RACallbackEstimateTitle(char* buf);
	static void RACallbackResetEmulator();
	static void RACallbackLoadROM(const char* unused);
	static unsigned char RACallbackReadMemory(unsigned int address);
	static unsigned int RACallbackReadBlock(unsigned int address, unsigned char* buffer, unsigned int bytes);
	static void RACallbackWriteMemory(unsigned int address, unsigned char value);

	static bool s_raintegration_initialized = false;
} // namespace Achievements::RAIntegration

void Achievements::SwitchToRAIntegration()
{
	s_using_raintegration = true;
	s_active = true;

	// Not strictly the case, but just in case we gate anything by IsLoggedIn().
	s_logged_in = true;
}

void Achievements::RAIntegration::InitializeRAIntegration(void* main_window_handle)
{
	RA_InitClient((HWND)main_window_handle, "PCSX2", GIT_TAG);
	RA_SetUserAgentDetail(Achievements::GetUserAgent().c_str());

	RA_InstallSharedFunctions(RACallbackIsActive, RACallbackCauseUnpause, RACallbackCausePause, RACallbackRebuildMenu,
		RACallbackEstimateTitle, RACallbackResetEmulator, RACallbackLoadROM);
	RA_SetConsoleID(PlayStation2);

	// EE physical memory and scratchpad are currently exposed (matching direct rcheevos implementation).
	RA_InstallMemoryBank(0, RACallbackReadMemory, RACallbackWriteMemory, EXPOSED_EE_MEMORY_SIZE);
	RA_InstallMemoryBankBlockReader(0, RACallbackReadBlock);

	// Fire off a login anyway. Saves going into the menu and doing it.
	RA_AttemptLogin(0);

	s_raintegration_initialized = true;

	// this is pretty lame, but we may as well persist until we exit anyway
	std::atexit(RA_Shutdown);
}

void Achievements::RAIntegration::MainWindowChanged(void* new_handle)
{
	if (s_raintegration_initialized)
	{
		RA_UpdateHWnd((HWND)new_handle);
		return;
	}

	InitializeRAIntegration(new_handle);
}

void Achievements::RAIntegration::GameChanged()
{
	s_game_id = s_game_hash.empty() ? 0 : RA_IdentifyHash(s_game_hash.c_str());
	RA_ActivateGame(s_game_id);
}

std::vector<std::tuple<int, std::string, bool>> Achievements::RAIntegration::GetMenuItems()
{
	std::array<RA_MenuItem, 64> items;
	const int num_items = RA_GetPopupMenuItems(items.data());

	std::vector<std::tuple<int, std::string, bool>> ret;
	ret.reserve(static_cast<u32>(num_items));

	for (int i = 0; i < num_items; i++)
	{
		const RA_MenuItem& it = items[i];
		if (!it.sLabel)
		{
			// separator
			ret.emplace_back(0, std::string(), false);
		}
		else
		{
			// option, maybe checkable
			ret.emplace_back(static_cast<int>(it.nID), StringUtil::WideStringToUTF8String(it.sLabel), it.bChecked);
		}
	}

	return ret;
}

void Achievements::RAIntegration::ActivateMenuItem(int item)
{
	RA_InvokeDialog(item);
}

int Achievements::RAIntegration::RACallbackIsActive()
{
	return static_cast<int>(HasActiveGame());
}

void Achievements::RAIntegration::RACallbackCauseUnpause()
{
	if (VMManager::HasValidVM())
		VMManager::SetState(VMState::Running);
}

void Achievements::RAIntegration::RACallbackCausePause()
{
	if (VMManager::HasValidVM())
		VMManager::SetState(VMState::Paused);
}

void Achievements::RAIntegration::RACallbackRebuildMenu()
{
	// unused, we build the menu on demand
}

void Achievements::RAIntegration::RACallbackEstimateTitle(char* buf)
{
	std::string title(fmt::format("{0} ({1}) [{2:08X}]", VMManager::GetGameName(), VMManager::GetGameSerial(), VMManager::GetGameCRC()));
	StringUtil::Strlcpy(buf, title, 256);
}

void Achievements::RAIntegration::RACallbackResetEmulator()
{
	if (VMManager::HasValidVM())
		VMManager::Reset();
}

void Achievements::RAIntegration::RACallbackLoadROM(const char* unused)
{
	// unused
	UNREFERENCED_PARAMETER(unused);
}

unsigned char Achievements::RAIntegration::RACallbackReadMemory(unsigned int address)
{
	return static_cast<unsigned char>(PeekMemory(address, sizeof(unsigned char), nullptr));
}

unsigned int Achievements::RAIntegration::RACallbackReadBlock(unsigned int address, unsigned char* buffer, unsigned int bytes)
{
	return PeekMemoryBlock(address, buffer, bytes);
}

void Achievements::RAIntegration::RACallbackWriteMemory(unsigned int address, unsigned char value)
{
	PokeMemory(address, sizeof(value), nullptr, static_cast<unsigned>(value));
}

#endif
