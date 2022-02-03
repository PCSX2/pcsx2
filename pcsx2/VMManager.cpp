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

#include "VMManager.h"

#include <atomic>
#include <sstream>
#include <mutex>

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"
#include "common/SettingsWrapper.h"
#include "common/Timer.h"
#include "common/Threading.h"
#include "fmt/core.h"

#include "Achievements.h"
#include "Counters.h"
#include "CDVD/CDVD.h"
#include "DEV9/DEV9.h"
#include "Elfheader.h"
#include "FW.h"
#include "GS.h"
#include "GSDumpReplayer.h"
#include "HostDisplay.h"
#include "HostSettings.h"
#include "INISettingsInterface.h"
#include "IopBios.h"
#include "MTVU.h"
#include "MemoryCardFile.h"
#include "Patch.h"
#include "PerformanceMetrics.h"
#include "R5900.h"
#include "SPU2/spu2.h"
#include "DEV9/DEV9.h"
#include "USB/USB.h"
#include "PAD/Host/PAD.h"
#include "Sio.h"
#include "ps2/BiosTools.h"
#include "Recording/InputRecordingControls.h"

#include "DebugTools/MIPSAnalyst.h"
#include "DebugTools/SymbolMap.h"

#include "IconsFontAwesome5.h"

#include "Recording/InputRecording.h"

#include "common/emitter/tools.h"
#ifdef _M_X86
#include "common/emitter/x86_intrin.h"
#endif

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <objbase.h>
#include <timeapi.h>
#endif

namespace VMManager
{
	static void ApplyGameFixes();
	static bool UpdateGameSettingsLayer();
	static void CheckForConfigChanges(const Pcsx2Config& old_config);
	static void CheckForCPUConfigChanges(const Pcsx2Config& old_config);
	static void CheckForGSConfigChanges(const Pcsx2Config& old_config);
	static void CheckForFramerateConfigChanges(const Pcsx2Config& old_config);
	static void CheckForPatchConfigChanges(const Pcsx2Config& old_config);
	static void CheckForSPU2ConfigChanges(const Pcsx2Config& old_config);
	static void CheckForDEV9ConfigChanges(const Pcsx2Config& old_config);
	static void CheckForMemoryCardConfigChanges(const Pcsx2Config& old_config);
	static void EnforceAchievementsChallengeModeSettings();
	static void LogUnsafeSettingsToConsole(const std::string& messages);
	static void WarnAboutUnsafeSettings();

	static bool AutoDetectSource(const std::string& filename);
	static bool ApplyBootParameters(VMBootParameters params, std::string* state_to_load);
	static bool CheckBIOSAvailability();
	static void LoadPatches(const std::string& serial, u32 crc,
		bool show_messages, bool show_messages_when_disabled);
	static void UpdateRunningGame(bool resetting, bool game_starting);

	static std::string GetCurrentSaveStateFileName(s32 slot);
	static bool DoLoadState(const char* filename);
	static bool DoSaveState(const char* filename, s32 slot_for_message, bool zip_on_thread, bool backup_old_state);
	static void ZipSaveState(std::unique_ptr<ArchiveEntryList> elist,
		std::unique_ptr<SaveStateScreenshotData> screenshot, std::string osd_key,
		const char* filename, s32 slot_for_message);
	static void ZipSaveStateOnThread(std::unique_ptr<ArchiveEntryList> elist,
		std::unique_ptr<SaveStateScreenshotData> screenshot, std::string osd_key,
		std::string filename, s32 slot_for_message);

	static void SetTimerResolutionIncreased(bool enabled);
	static void SetHardwareDependentDefaultSettings(SettingsInterface& si);
	static void EnsureCPUInfoInitialized();
	static void SetEmuThreadAffinities();
} // namespace VMManager

static std::unique_ptr<SysMainMemory> s_vm_memory;
static std::unique_ptr<SysCpuProviderPack> s_cpu_provider_pack;
static std::unique_ptr<INISettingsInterface> s_game_settings_interface;
static std::unique_ptr<INISettingsInterface> s_input_settings_interface;

static std::atomic<VMState> s_state{VMState::Shutdown};
static bool s_cpu_implementation_changed = false;
static Threading::ThreadHandle s_vm_thread_handle;

static std::deque<std::thread> s_save_state_threads;
static std::mutex s_save_state_threads_mutex;

static std::recursive_mutex s_info_mutex;
static std::string s_disc_path;
static u32 s_game_crc;
static u32 s_patches_crc;
static std::string s_game_serial;
static std::string s_game_name;
static std::string s_elf_override;
static std::string s_input_profile_name;
static u32 s_active_game_fixes = 0;
static std::vector<u8> s_widescreen_cheats_data;
static bool s_widescreen_cheats_loaded = false;
static std::vector<u8> s_no_interlacing_cheats_data;
static bool s_no_interlacing_cheats_loaded = false;
static s32 s_active_widescreen_patches = 0;
static u32 s_active_no_interlacing_patches = 0;
static u32 s_frame_advance_count = 0;
static u32 s_mxcsr_saved;
static bool s_gs_open_on_initialize = false;

bool VMManager::PerformEarlyHardwareChecks(const char** error)
{
#define COMMON_DOWNLOAD_MESSAGE \
	"PCSX2 builds can be downloaded from https://pcsx2.net/downloads/"

#if defined(_M_X86)
	// On Windows, this gets called as a global object constructor, before any of our objects are constructed.
	// So, we have to put it on the stack instead.
	x86capabilities temp_x86_caps;
	temp_x86_caps.Identify();

	if (!temp_x86_caps.hasStreamingSIMD4Extensions)
	{
		*error = "PCSX2 requires the Streaming SIMD 4 Extensions instruction set, which your CPU does not support.\n\n"
				 "SSE4 is now a minimum requirement for PCSX2. You should either upgrade your CPU, or use an older build such as 1.6.0.\n\n" COMMON_DOWNLOAD_MESSAGE;
		return false;
	}

#if _M_SSE >= 0x0501
	if (!temp_x86_caps.hasAVX || !temp_x86_caps.hasAVX2)
	{
		*error = "This build of PCSX2 requires the Advanced Vector Extensions 2 instruction set, which your CPU does not support.\n\n"
				 "You should download and run the SSE4 build of PCSX2 instead, or upgrade to a CPU that supports AVX2 to use this build.\n\n" COMMON_DOWNLOAD_MESSAGE;
		return false;
	}
#endif
#endif

#undef COMMON_DOWNLOAD_MESSAGE
	return true;
}

VMState VMManager::GetState()
{
	return s_state.load(std::memory_order_acquire);
}

void VMManager::SetState(VMState state)
{
	// Some state transitions aren't valid.
	const VMState old_state = s_state.load(std::memory_order_acquire);
	pxAssert(state != VMState::Initializing && state != VMState::Shutdown);
	SetTimerResolutionIncreased(state == VMState::Running);
	s_state.store(state, std::memory_order_release);

	if (state != VMState::Stopping && (state == VMState::Paused || old_state == VMState::Paused))
	{
		const bool paused = (state == VMState::Paused);
		if (paused)
		{
			if (THREAD_VU1)
				vu1Thread.WaitVU();
			GetMTGS().WaitGS(false);
		}
		else
		{
			PerformanceMetrics::Reset();
			frameLimitReset();
		}

		SPU2SetOutputPaused(state == VMState::Paused);
		if (state == VMState::Paused)
			Host::OnVMPaused();
		else
			Host::OnVMResumed();
	}
}

bool VMManager::HasValidVM()
{
	const VMState state = s_state.load(std::memory_order_acquire);
	return (state == VMState::Running || state == VMState::Paused);
}

std::string VMManager::GetDiscPath()
{
	std::unique_lock lock(s_info_mutex);
	return s_disc_path;
}

u32 VMManager::GetGameCRC()
{
	std::unique_lock lock(s_info_mutex);
	return s_game_crc;
}

std::string VMManager::GetGameSerial()
{
	std::unique_lock lock(s_info_mutex);
	return s_game_serial;
}

std::string VMManager::GetGameName()
{
	std::unique_lock lock(s_info_mutex);
	return s_game_name;
}

bool VMManager::Internal::InitializeGlobals()
{
	// On Win32, we have a bunch of things which use COM (e.g. SDL, XAudio2, etc).
	// We need to initialize COM first, before anything else does, because otherwise they might
	// initialize it in single-threaded/apartment mode, which can't be changed to multithreaded.
#ifdef _WIN32
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		Host::ReportErrorAsync("Error", fmt::format("CoInitializeEx() failed: {:08X}", hr));
		return false;
	}
#endif

	x86caps.Identify();
	x86caps.CountCores();
	x86caps.SIMD_EstablishMXCSRmask();
	x86caps.CalculateMHz();
	SysLogMachineCaps();

	if (GSinit() != 0)
	{
		Host::ReportErrorAsync("Error", "Failed to initialize GS (GSinit()).");
		return false;
	}

	if (!SPU2init())
	{
		Host::ReportErrorAsync("Error", "Failed to initialize SPU2 (SPU2init()).");
		return false;
	}

	if (USBinit() != 0)
	{
		Host::ReportErrorAsync("Error", "Failed to initialize USB (USBinit())");
		return false;
	}

	return true;
}

void VMManager::Internal::ReleaseGlobals()
{
	USBshutdown();
	SPU2shutdown();
	GSshutdown();

#ifdef _WIN32
	CoUninitialize();
#endif
}

bool VMManager::Internal::InitializeMemory()
{
	pxAssert(!s_vm_memory && !s_cpu_provider_pack);

	s_vm_memory = std::make_unique<SysMainMemory>();
	s_cpu_provider_pack = std::make_unique<SysCpuProviderPack>();

	return s_vm_memory->Allocate();
}

void VMManager::Internal::ReleaseMemory()
{
	std::vector<u8>().swap(s_widescreen_cheats_data);
	s_widescreen_cheats_loaded = false;
	std::vector<u8>().swap(s_no_interlacing_cheats_data);
	s_no_interlacing_cheats_loaded = false;

	s_vm_memory.reset();
	s_cpu_provider_pack.reset();
}

SysMainMemory& GetVmMemory()
{
	return *s_vm_memory;
}

SysCpuProviderPack& GetCpuProviders()
{
	return *s_cpu_provider_pack;
}

void VMManager::LoadSettings()
{
	std::unique_lock<std::mutex> lock = Host::GetSettingsLock();
	SettingsInterface* si = Host::GetSettingsInterface();
	SettingsLoadWrapper slw(*si);
	EmuConfig.LoadSave(slw);
	PAD::LoadConfig(*si);
	Host::LoadSettings(*si, lock);

	// Achievements hardcore mode disallows setting some configuration options.
	EnforceAchievementsChallengeModeSettings();

	// Remove any user-specified hacks in the config (we don't want stale/conflicting values when it's globally disabled).
	EmuConfig.GS.MaskUserHacks();
	EmuConfig.GS.MaskUpscalingHacks();

	// Disable interlacing if we have no-interlacing patches active.
	if (s_active_no_interlacing_patches > 0 && EmuConfig.GS.InterlaceMode == GSInterlaceMode::Automatic)
		EmuConfig.GS.InterlaceMode = GSInterlaceMode::Off;

	// Switch to 16:9 if widescreen patches are enabled, and AR is auto.
	if (s_active_widescreen_patches > 0 && EmuConfig.GS.AspectRatio == AspectRatioType::RAuto4_3_3_2)
	{
		// Don't change when reloading settings in the middle of a FMV with switch.
		if (EmuConfig.CurrentAspectRatio == EmuConfig.GS.AspectRatio)
			EmuConfig.CurrentAspectRatio = AspectRatioType::R16_9;

		EmuConfig.GS.AspectRatio = AspectRatioType::R16_9;
	}

	// Force MTVU off when playing back GS dumps, it doesn't get used.
	if (GSDumpReplayer::IsReplayingDump())
		EmuConfig.Speedhacks.vuThread = false;

	if (HasValidVM())
	{
		if (EmuConfig.WarnAboutUnsafeSettings)
			WarnAboutUnsafeSettings();

		ApplyGameFixes();
	}
}

void VMManager::ApplyGameFixes()
{
	s_active_game_fixes = 0;

	const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(s_game_serial);
	if (!game)
		return;

	s_active_game_fixes += game->applyGameFixes(EmuConfig, EmuConfig.EnableGameFixes);
	s_active_game_fixes += game->applyGSHardwareFixes(EmuConfig.GS);
}

std::string VMManager::GetGameSettingsPath(const std::string_view& game_serial, u32 game_crc)
{
	std::string sanitized_serial(Path::SanitizeFileName(game_serial));

	return game_serial.empty() ?
			   Path::Combine(EmuFolders::GameSettings, fmt::format("{:08X}.ini", game_crc)) :
			   Path::Combine(EmuFolders::GameSettings, fmt::format("{}_{:08X}.ini", sanitized_serial, game_crc));
}

std::string VMManager::GetDiscOverrideFromGameSettings(const std::string& elf_path)
{
	std::string iso_path;
	if (const u32 crc = cdvdGetElfCRC(elf_path); crc != 0)
	{
		INISettingsInterface si(GetGameSettingsPath(std::string_view(), crc));
		if (si.Load())
		{
			iso_path = si.GetStringValue("EmuCore", "DiscPath");
			if (!iso_path.empty())
				Console.WriteLn(fmt::format("Disc override for ELF at '{}' is '{}'", elf_path, iso_path));
		}
	}

	return iso_path;
}

std::string VMManager::GetInputProfilePath(const std::string_view& name)
{
	return Path::Combine(EmuFolders::InputProfiles, fmt::format("{}.ini", name));
}

void VMManager::RequestDisplaySize(float scale /*= 0.0f*/)
{
	int iwidth, iheight;
	GSgetInternalResolution(&iwidth, &iheight);
	if (iwidth <= 0 || iheight <= 0)
		return;

	// scale x not y for aspect ratio
	float x_scale;
	switch (GSConfig.AspectRatio)
	{
		case AspectRatioType::RAuto4_3_3_2:
			if (GSgetDisplayMode() == GSVideoMode::SDTV_480P || (GSConfig.PCRTCOverscan && GSConfig.PCRTCOffsets))
				x_scale = (3.0f / 2.0f) / (static_cast<float>(iwidth) / static_cast<float>(iheight));
			else
				x_scale = (4.0f / 3.0f) / (static_cast<float>(iwidth) / static_cast<float>(iheight));
			break;
		case AspectRatioType::R4_3:
			x_scale = (4.0f / 3.0f) / (static_cast<float>(iwidth) / static_cast<float>(iheight));
			break;
		case AspectRatioType::R16_9:
			x_scale = (16.0f / 9.0f) / (static_cast<float>(iwidth) / static_cast<float>(iheight));
			break;
		case AspectRatioType::Stretch:
		default:
			x_scale = 1.0f;
			break;
	}

	float width = static_cast<float>(iwidth) * x_scale;
	float height = static_cast<float>(iheight);

	if (scale != 0.0f)
	{
		// unapply the upscaling, then apply the scale
		scale = (1.0f / GSConfig.UpscaleMultiplier) * scale;
		width *= scale;
		height *= scale;
	}

	iwidth = std::max(static_cast<int>(std::lroundf(width)), 1);
	iheight = std::max(static_cast<int>(std::lroundf(height)), 1);

	Host::RequestResizeHostDisplay(iwidth, iheight);
}

std::string VMManager::GetSerialForGameSettings()
{
	// If we're running an ELF, we don't want to use the serial for any ISO override
	// for game settings, since the game settings is where we define the override.
	std::unique_lock lock(s_info_mutex);
	return s_elf_override.empty() ? std::string(s_game_serial) : std::string();
}

bool VMManager::UpdateGameSettingsLayer()
{
	std::unique_ptr<INISettingsInterface> new_interface;
	if (s_game_crc != 0 && Host::GetBaseBoolSettingValue("EmuCore", "EnablePerGameSettings", true))
	{
		std::string filename(GetGameSettingsPath(GetSerialForGameSettings(), s_game_crc));
		if (!FileSystem::FileExists(filename.c_str()))
		{
			// try the legacy format (crc.ini)
			filename = GetGameSettingsPath({}, s_game_crc);
		}

		if (FileSystem::FileExists(filename.c_str()))
		{
			Console.WriteLn("Loading game settings from '%s'...", filename.c_str());
			new_interface = std::make_unique<INISettingsInterface>(std::move(filename));
			if (!new_interface->Load())
			{
				Console.Error("Failed to parse game settings ini '%s'", new_interface->GetFileName().c_str());
				new_interface.reset();
			}
		}
		else
		{
			DevCon.WriteLn("No game settings found (tried '%s')", filename.c_str());
		}
	}

	std::string input_profile_name;
	bool use_game_settings_for_controller = false;
	if (new_interface)
	{
		new_interface->GetBoolValue("Pad", "UseGameSettingsForController", &use_game_settings_for_controller);
		if (!use_game_settings_for_controller)
			new_interface->GetStringValue("EmuCore", "InputProfileName", &input_profile_name);
	}

	if (!s_game_settings_interface && !new_interface && s_input_profile_name == input_profile_name)
		return false;

	Host::Internal::SetGameSettingsLayer(new_interface.get());
	s_game_settings_interface = std::move(new_interface);

	std::unique_ptr<INISettingsInterface> input_interface;
	if (!use_game_settings_for_controller)
	{
		if (!input_profile_name.empty())
		{
			const std::string filename(GetInputProfilePath(input_profile_name));
			if (FileSystem::FileExists(filename.c_str()))
			{
				Console.WriteLn("Loading input profile from '%s'...", filename.c_str());
				input_interface = std::make_unique<INISettingsInterface>(std::move(filename));
				if (!input_interface->Load())
				{
					Console.Error("Failed to parse input profile ini '%s'", input_interface->GetFileName().c_str());
					input_interface.reset();
					input_profile_name = {};
				}
			}
			else
			{
				DevCon.WriteLn("No game settings found (tried '%s')", filename.c_str());
				input_profile_name = {};
			}
		}

		Host::Internal::SetInputSettingsLayer(input_interface ? input_interface.get() : Host::Internal::GetBaseSettingsLayer());
	}
	else
	{
		// using game settings for bindings too
		Host::Internal::SetInputSettingsLayer(s_game_settings_interface.get());
	}

	s_input_settings_interface = std::move(input_interface);
	s_input_profile_name = std::move(input_profile_name);
	return true;
}

void VMManager::LoadPatches(const std::string& serial, u32 crc, bool show_messages, bool show_messages_when_disabled)
{
	const std::string crc_string(fmt::format("{:08X}", crc));
	s_patches_crc = crc;
	s_active_widescreen_patches = 0;
	s_active_no_interlacing_patches = 0;
	ForgetLoadedPatches();

	std::string message;

	int patch_count = 0;
	if (EmuConfig.EnablePatches)
	{
		const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(serial);
		const std::string* patches = game ? game->findPatch(crc) : nullptr;
		if (patches && (patch_count = LoadPatchesFromString(*patches)) > 0)
		{
			PatchesCon->WriteLn(Color_Green, "(GameDB) Patches Loaded: %d", patch_count);
			fmt::format_to(std::back_inserter(message), "{} game patches", patch_count);
		}
	}

	// regular cheat patches
	int cheat_count = 0;
	if (EmuConfig.EnableCheats)
	{
		cheat_count = LoadPatchesFromDir(crc_string, EmuFolders::Cheats, "Cheats", true);
		if (cheat_count > 0)
		{
			PatchesCon->WriteLn(Color_Green, "Cheats Loaded: %d", cheat_count);
			fmt::format_to(std::back_inserter(message), "{}{} cheat patches", (patch_count > 0) ? " and " : "", cheat_count);
		}
	}

	// wide screen patches
	if (EmuConfig.EnableWideScreenPatches && crc != 0)
	{
		if (!Achievements::ChallengeModeActive() && (s_active_widescreen_patches = LoadPatchesFromDir(crc_string, EmuFolders::CheatsWS, "Widescreen hacks", false) > 0))
		{
			Console.WriteLn(Color_Gray, "Found widescreen patches in the cheats_ws folder --> skipping cheats_ws.zip");
		}
		else
		{
			// No ws cheat files found at the cheats_ws folder, try the ws cheats zip file.
			if (!s_widescreen_cheats_loaded)
			{
				s_widescreen_cheats_loaded = true;

				std::optional<std::vector<u8>> data = Host::ReadResourceFile("cheats_ws.zip");
				if (data.has_value())
					s_widescreen_cheats_data = std::move(data.value());
			}

			if (!s_widescreen_cheats_data.empty())
			{
				s_active_widescreen_patches = LoadPatchesFromZip(crc_string, s_widescreen_cheats_data.data(), s_widescreen_cheats_data.size());
				PatchesCon->WriteLn(Color_Green, "(Wide Screen Cheats DB) Patches Loaded: %d", s_active_widescreen_patches);
			}
		}

		if (s_active_widescreen_patches > 0)
		{
			fmt::format_to(std::back_inserter(message), "{}{} widescreen patches", (patch_count > 0 || cheat_count > 0) ? " and " : "", s_active_widescreen_patches);

			// Switch to 16:9 if widescreen patches are enabled, and AR is auto.
			if (EmuConfig.GS.AspectRatio == AspectRatioType::RAuto4_3_3_2)
			{
				// Don't change when reloading settings in the middle of a FMV with switch.
				if (EmuConfig.CurrentAspectRatio == EmuConfig.GS.AspectRatio)
					EmuConfig.CurrentAspectRatio = AspectRatioType::R16_9;

				EmuConfig.GS.AspectRatio = AspectRatioType::R16_9;
			}
		}
	}

	// no-interlacing patches
	if (EmuConfig.EnableNoInterlacingPatches && crc != 0)
	{
		if (!Achievements::ChallengeModeActive() && (s_active_no_interlacing_patches = LoadPatchesFromDir(crc_string, EmuFolders::CheatsNI, "No-interlacing patches", false)) > 0)
		{
			Console.WriteLn(Color_Gray, "Found no-interlacing patches in the cheats_ni folder --> skipping cheats_ni.zip");
		}
		else
		{
			// No ws cheat files found at the cheats_ws folder, try the ws cheats zip file.
			if (!s_no_interlacing_cheats_loaded)
			{
				s_no_interlacing_cheats_loaded = true;

				std::optional<std::vector<u8>> data = Host::ReadResourceFile("cheats_ni.zip");
				if (data.has_value())
					s_no_interlacing_cheats_data = std::move(data.value());
			}

			if (!s_no_interlacing_cheats_data.empty())
			{
				s_active_no_interlacing_patches = LoadPatchesFromZip(crc_string, s_no_interlacing_cheats_data.data(), s_no_interlacing_cheats_data.size());
				PatchesCon->WriteLn(Color_Green, "(No-Interlacing Cheats DB) Patches Loaded: %u", s_active_no_interlacing_patches);
			}
		}

		if (s_active_no_interlacing_patches > 0)
		{
			fmt::format_to(std::back_inserter(message), "{}{} no-interlacing patches", (patch_count > 0 || cheat_count > 0 || s_active_widescreen_patches > 0) ? " and " : "", s_active_no_interlacing_patches);

			// Disable interlacing in GS if active.
			if (EmuConfig.GS.InterlaceMode == GSInterlaceMode::Automatic)
			{
				EmuConfig.GS.InterlaceMode = GSInterlaceMode::Off;
				GetMTGS().ApplySettings();
			}
		}
	}
	else
	{
		s_active_no_interlacing_patches = 0;
	}

	if (show_messages)
	{
		if (cheat_count > 0 || s_active_widescreen_patches > 0 || s_active_no_interlacing_patches > 0)
		{
			message += " are active.";
			Host::AddIconOSDMessage("LoadPatches", ICON_FA_FILE_CODE, message, Host::OSD_INFO_DURATION);
		}
		else if (show_messages_when_disabled)
		{
			Host::AddIconOSDMessage("LoadPatches", ICON_FA_FILE_CODE, "No cheats or patches (widescreen, compatibility or others) are found / enabled.", Host::OSD_INFO_DURATION);
		}
	}
}

void VMManager::UpdateRunningGame(bool resetting, bool game_starting)
{
	// The CRC can be known before the game actually starts (at the bios), so when
	// we have the CRC but we're still at the bios and the settings are changed
	// (e.g. the user presses TAB to speed up emulation), we don't want to apply the
	// settings as if the game is already running (title, loadeding patches, etc).
	u32 new_crc;
	std::string new_serial;
	if (!GSDumpReplayer::IsReplayingDump())
	{
		const bool ingame = (ElfCRC && (g_GameLoading || g_GameStarted));
		new_crc = ingame ? ElfCRC : 0;
		new_serial = ingame ? SysGetDiscID() : SysGetBiosDiscID();
	}
	else
	{
		new_crc = GSDumpReplayer::GetDumpCRC();
		new_serial = GSDumpReplayer::GetDumpSerial();
	}

	if (!resetting && s_game_crc == new_crc && s_game_serial == new_serial)
		return;

	{
		std::unique_lock lock(s_info_mutex);
		s_game_serial = std::move(new_serial);
		s_game_crc = new_crc;
		s_game_name.clear();

		std::string memcardFilters;

		if (const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(s_game_serial))
		{
			if (!s_elf_override.empty())
				s_game_name = Path::GetFileTitle(FileSystem::GetDisplayNameFromPath(s_elf_override));
			else
				s_game_name = game->name;

			memcardFilters = game->memcardFiltersAsString();
		}
		else
		{
			if (s_game_serial.empty() && s_game_crc == 0)
				s_game_name = "Booting PS2 BIOS...";
		}

		sioSetGameSerial(memcardFilters.empty() ? s_game_serial : memcardFilters);

		// If we don't reset the timer here, when using folder memcards the reindex will cause an eject,
		// which a bunch of games don't like since they access the memory card on boot.
		if (game_starting || resetting)
			AutoEject::ClearAll();
	}

	UpdateGameSettingsLayer();
	ApplySettings();

	// Clear the memory card eject notification again when booting for the first time, or starting.
	// Otherwise, games think the card was removed on boot.
	if (game_starting || resetting)
		AutoEject::ClearAll();

	// Check this here, for two cases: dynarec on, and when enable cheats is set per-game.
	if (s_patches_crc != s_game_crc)
		ReloadPatches(game_starting, false);

#ifdef ENABLE_ACHIEVEMENTS
	// Per-game ini enabling of hardcore mode. We need to re-enforce the settings if so.
	if (game_starting && Achievements::ResetChallengeMode())
		ApplySettings();
#endif

	GetMTGS().SendGameCRC(new_crc);

	Host::OnGameChanged(s_disc_path, s_elf_override, s_game_serial, s_game_name, s_game_crc);

	MIPSAnalyst::ScanForFunctions(R5900SymbolMap, ElfTextRange.first, ElfTextRange.first + ElfTextRange.second, true);
	R5900SymbolMap.UpdateActiveSymbols();
	R3000SymbolMap.UpdateActiveSymbols();
}

void VMManager::ReloadPatches(bool verbose, bool show_messages_when_disabled)
{
	LoadPatches(s_game_serial, s_game_crc, verbose, show_messages_when_disabled);
}

static LimiterModeType GetInitialLimiterMode()
{
	return EmuConfig.GS.FrameLimitEnable ? LimiterModeType::Nominal : LimiterModeType::Unlimited;
}

bool VMManager::AutoDetectSource(const std::string& filename)
{
	if (!filename.empty())
	{
		if (!FileSystem::FileExists(filename.c_str()))
		{
			Host::ReportErrorAsync("Error", fmt::format("Requested filename '{}' does not exist.", filename));
			return false;
		}

		const std::string display_name(FileSystem::GetDisplayNameFromPath(filename));
		if (IsGSDumpFileName(display_name))
		{
			CDVDsys_ChangeSource(CDVD_SourceType::NoDisc);
			return GSDumpReplayer::Initialize(filename.c_str());
		}
		else if (IsElfFileName(display_name))
		{
			// alternative way of booting an elf, change the elf override, and (optionally) use the disc
			// specified in the game settings.
			std::string disc_path(GetDiscOverrideFromGameSettings(filename));
			if (!disc_path.empty())
			{
				CDVDsys_SetFile(CDVD_SourceType::Iso, disc_path);
				CDVDsys_ChangeSource(CDVD_SourceType::Iso);
				s_disc_path = std::move(disc_path);
			}
			else
			{
				CDVDsys_ChangeSource(CDVD_SourceType::NoDisc);
			}

			s_elf_override = filename;
			return true;
		}
		else
		{
			// TODO: Maybe we should check if it's a valid iso here...
			CDVDsys_SetFile(CDVD_SourceType::Iso, filename);
			CDVDsys_ChangeSource(CDVD_SourceType::Iso);
			s_disc_path = filename;
			return true;
		}
	}
	else
	{
		// make sure we're not fast booting when we have no filename
		CDVDsys_ChangeSource(CDVD_SourceType::NoDisc);
		EmuConfig.UseBOOT2Injection = false;
		return true;
	}
}

bool VMManager::ApplyBootParameters(VMBootParameters params, std::string* state_to_load)
{
	const bool default_fast_boot = Host::GetBoolSettingValue("EmuCore", "EnableFastBoot", true);
	EmuConfig.UseBOOT2Injection = params.fast_boot.value_or(default_fast_boot);

	s_elf_override = std::move(params.elf_override);
	s_disc_path.clear();
	if (!params.save_state.empty())
		*state_to_load = std::move(params.save_state);

	// if we're loading an indexed save state, we need to get the serial/crc from the disc.
	if (params.state_index.has_value())
	{
		if (params.filename.empty())
		{
			Host::ReportErrorAsync("Error", "Cannot load an indexed save state without a boot filename.");
			return false;
		}

		*state_to_load = GetSaveStateFileName(params.filename.c_str(), params.state_index.value());
		if (state_to_load->empty())
		{
			Host::ReportErrorAsync("Error", "Could not resolve path indexed save state load.");
			return false;
		}
	}

#ifdef ENABLE_ACHIEVEMENTS
	// Check for resuming with hardcore mode.
	Achievements::ResetChallengeMode();
	if (!state_to_load->empty() && Achievements::ChallengeModeActive() &&
		!Achievements::ConfirmChallengeModeDisable("Resuming state"))
	{
		return false;
	}
#endif

	// resolve source type
	if (params.source_type.has_value())
	{
		if (params.source_type.value() == CDVD_SourceType::Iso && !FileSystem::FileExists(params.filename.c_str()))
		{
			Host::ReportErrorAsync("Error", fmt::format("Requested filename '{}' does not exist.", params.filename));
			return false;
		}

		// Use specified source type.
		s_disc_path = std::move(params.filename);
		CDVDsys_SetFile(params.source_type.value(), s_disc_path);
		CDVDsys_ChangeSource(params.source_type.value());
	}
	else
	{
		// Automatic type detection of boot parameter based on filename.
		if (!AutoDetectSource(params.filename))
			return false;
	}

	if (!s_elf_override.empty())
	{
		if (!FileSystem::FileExists(s_elf_override.c_str()))
		{
			Host::ReportErrorAsync("Error", fmt::format("Requested boot ELF '{}' does not exist.", s_elf_override));
			return false;
		}

		Hle_SetElfPath(s_elf_override.c_str());
		EmuConfig.UseBOOT2Injection = true;
	}

	return true;
}

bool VMManager::CheckBIOSAvailability()
{
	if (IsBIOSAvailable(EmuConfig.FullpathToBios()))
		return true;

	// TODO: When we translate core strings, translate this.

	const char* message = "PCSX2 requires a PS2 BIOS in order to run.\n\n"
						  "For legal reasons, you *must* obtain a BIOS from an actual PS2 unit that you own (borrowing doesn't count).\n\n"
						  "Once dumped, this BIOS image should be placed in the bios folder within the data directory (Tools Menu -> Open Data Directory).\n\n"
						  "Please consult the FAQs and Guides for further instructions.";

	Host::ReportErrorAsync("Startup Error", message);
	return false;
}

bool VMManager::Initialize(VMBootParameters boot_params)
{
	const Common::Timer init_timer;
	pxAssertRel(s_state.load(std::memory_order_acquire) == VMState::Shutdown, "VM is shutdown");

	// cancel any game list scanning, we need to use CDVD!
	// TODO: we can get rid of this once, we make CDVD not use globals...
	// (or make it thread-local, but that seems silly.)
	Host::CancelGameListRefresh();

	s_state.store(VMState::Initializing, std::memory_order_release);
	s_vm_thread_handle = Threading::ThreadHandle::GetForCallingThread();
	Host::OnVMStarting();

	ScopedGuard close_state = [] {
		if (GSDumpReplayer::IsReplayingDump())
			GSDumpReplayer::Shutdown();

		s_vm_thread_handle = {};
		s_state.store(VMState::Shutdown, std::memory_order_release);
		Host::OnVMDestroyed();
	};

	std::string state_to_load;
	if (!ApplyBootParameters(std::move(boot_params), &state_to_load))
		return false;

	EmuConfig.LimiterMode = GetInitialLimiterMode();

	// early out if we don't have a bios
	if (!GSDumpReplayer::IsReplayingDump() && !CheckBIOSAvailability())
		return false;

	Console.WriteLn("Opening CDVD...");
	if (!DoCDVDopen())
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize CDVD.");
		return false;
	}
	ScopedGuard close_cdvd = [] { DoCDVDclose(); };

	Console.WriteLn("Opening GS...");
	s_gs_open_on_initialize = GetMTGS().IsOpen();
	if (!s_gs_open_on_initialize && !GetMTGS().WaitForOpen())
	{
		// we assume GS is going to report its own error
		Console.WriteLn("Failed to open GS.");
		return false;
	}

	ScopedGuard close_gs = []() {
		if (!s_gs_open_on_initialize)
			GetMTGS().WaitForClose();
	};

	Console.WriteLn("Opening SPU2...");
	if (!SPU2open())
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize SPU2.");
		return false;
	}
	ScopedGuard close_spu2(&SPU2close);

	Console.WriteLn("Opening PAD...");
	if (PADinit() != 0 || PADopen(g_host_display->GetWindowInfo()) != 0)
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize PAD.");
		return false;
	}
	ScopedGuard close_pad = []() {
		PADclose();
		PADshutdown();
	};

	Console.WriteLn("Opening DEV9...");
	if (DEV9init() != 0 || DEV9open() != 0)
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize DEV9.");
		return false;
	}
	ScopedGuard close_dev9 = []() {
		DEV9close();
		DEV9shutdown();
	};

	Console.WriteLn("Opening USB...");
	if (!USBopen())
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize USB.");
		return false;
	}
	ScopedGuard close_usb = []() {
		USBclose();
	};

	Console.WriteLn("Opening FW...");
	if (FWopen() != 0)
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize FW.");
		return false;
	}
	ScopedGuard close_fw = []() { FWclose(); };

	FileMcd_EmuOpen();

	// Don't close when we return
	close_fw.Cancel();
	close_usb.Cancel();
	close_dev9.Cancel();
	close_pad.Cancel();
	close_spu2.Cancel();
	close_gs.Cancel();
	close_cdvd.Cancel();
	close_state.Cancel();

#if defined(_M_X86)
	s_mxcsr_saved = _mm_getcsr();
#elif defined(_M_ARM64)
	s_mxcsr_saved = static_cast<u32>(a64_getfpcr());
#endif

	s_cpu_implementation_changed = false;
	s_cpu_provider_pack->ApplyConfig();
	SetCPUState(EmuConfig.Cpu.sseMXCSR, EmuConfig.Cpu.sseVUMXCSR);
	SysClearExecutionCache();
	memBindConditionalHandlers();

	ForgetLoadedPatches();
	gsUpdateFrequency(EmuConfig);
	frameLimitReset();
	cpuReset();

	Console.WriteLn("VM subsystems initialized in %.2f ms", init_timer.GetTimeMilliseconds());
	s_state.store(VMState::Paused, std::memory_order_release);
	Host::OnVMStarted();

	UpdateRunningGame(true, false);

	SetEmuThreadAffinities();

	PerformanceMetrics::Clear();

	// do we want to load state?
	if (!GSDumpReplayer::IsReplayingDump() && !state_to_load.empty())
	{
		if (!DoLoadState(state_to_load.c_str()))
		{
			Shutdown(false);
			return false;
		}
	}

	return true;
}

void VMManager::Shutdown(bool save_resume_state)
{
	// we'll probably already be stopping (this is how Qt calls shutdown),
	// but just in case, so any of the stuff we call here knows we don't have a valid VM.
	s_state.store(VMState::Stopping, std::memory_order_release);

	SetTimerResolutionIncreased(false);

	// sync everything
	if (THREAD_VU1)
		vu1Thread.WaitVU();
	GetMTGS().WaitGS();

	if (!GSDumpReplayer::IsReplayingDump() && save_resume_state)
	{
		std::string resume_file_name(GetCurrentSaveStateFileName(-1));
		if (!resume_file_name.empty() && !DoSaveState(resume_file_name.c_str(), -1, true, false))
			Console.Error("Failed to save resume state");
	}
	else if (GSDumpReplayer::IsReplayingDump())
	{
		GSDumpReplayer::Shutdown();
	}

	{
		LastELF.clear();
		DiscSerial.clear();
		ElfCRC = 0;
		ElfEntry = 0;
		ElfTextRange = {};

		std::unique_lock lock(s_info_mutex);
		s_disc_path.clear();
		s_elf_override.clear();
		s_game_crc = 0;
		s_patches_crc = 0;
		s_game_serial.clear();
		s_game_name.clear();
		Host::OnGameChanged(s_disc_path, s_elf_override, s_game_serial, s_game_name, 0);
	}
	s_active_game_fixes = 0;
	s_active_widescreen_patches = 0;
	s_active_no_interlacing_patches = 0;

	UpdateGameSettingsLayer();

	std::string().swap(s_elf_override);

#ifdef _M_X86
	_mm_setcsr(s_mxcsr_saved);
#elif defined(_M_ARM64)
	a64_setfpcr(s_mxcsr_saved);
#endif

	ForgetLoadedPatches();
	R3000A::ioman::reset();
	vtlb_Shutdown();
	USBclose();
	SPU2close();
	PADclose();
	DEV9close();
	DoCDVDclose();
	FWclose();
	FileMcd_EmuClose();

	// If the fullscreen UI is running, do a hardware reset on the GS
	// so that the texture cache and targets are all cleared.
	if (s_gs_open_on_initialize)
	{
		GetMTGS().WaitGS(false, false, false);
		GetMTGS().ResetGS(true);
	}
	else
	{
		GetMTGS().WaitForClose();
	}

	PADshutdown();
	DEV9shutdown();

	s_state.store(VMState::Shutdown, std::memory_order_release);
	Host::OnVMDestroyed();
}

void VMManager::Reset()
{
#ifdef ENABLE_ACHIEVEMENTS
	if (!Achievements::OnReset())
		return;
#endif

	const bool game_was_started = g_GameStarted;

	s_active_game_fixes = 0;
	s_active_widescreen_patches = 0;
	s_active_no_interlacing_patches = 0;

	SysClearExecutionCache();
	memBindConditionalHandlers();
	UpdateVSyncRate();
	frameLimitReset();
	cpuReset();

	// gameid change, so apply settings
	if (game_was_started)
		UpdateRunningGame(true, false);

	if (g_InputRecording.isActive())
	{
		g_InputRecording.handleReset();
		GetMTGS().PresentCurrentFrame();
	}
}

std::string VMManager::GetSaveStateFileName(const char* game_serial, u32 game_crc, s32 slot)
{
	std::string filename;
	if (game_crc != 0)
	{
		if (slot < 0)
			filename = fmt::format("{} ({:08X}).resume.p2s", game_serial, game_crc);
		else
			filename = fmt::format("{} ({:08X}).{:02d}.p2s", game_serial, game_crc, slot);

		filename = Path::Combine(EmuFolders::Savestates, filename);
	}

	return filename;
}

std::string VMManager::GetSaveStateFileName(const char* filename, s32 slot)
{
	pxAssertRel(!HasValidVM(), "Should not have a VM when calling the non-gamelist GetSaveStateFileName()");

	std::string ret;
	std::string serial;
	u32 crc;
	if (Host::GetSerialAndCRCForFilename(filename, &serial, &crc))
		ret = GetSaveStateFileName(serial.c_str(), crc, slot);

	return ret;
}

bool VMManager::HasSaveStateInSlot(const char* game_serial, u32 game_crc, s32 slot)
{
	std::string filename(GetSaveStateFileName(game_serial, game_crc, slot));
	return (!filename.empty() && FileSystem::FileExists(filename.c_str()));
}

std::string VMManager::GetCurrentSaveStateFileName(s32 slot)
{
	std::unique_lock lock(s_info_mutex);
	return GetSaveStateFileName(s_game_serial.c_str(), s_game_crc, slot);
}

bool VMManager::DoLoadState(const char* filename)
{
	if (GSDumpReplayer::IsReplayingDump())
		return false;

	try
	{
		Host::OnSaveStateLoading(filename);
		SaveState_UnzipFromDisk(filename);
		UpdateRunningGame(false, false);
		Host::OnSaveStateLoaded(filename, true);
		if (g_InputRecording.isActive())
		{
			g_InputRecording.handleLoadingSavestate();
			GetMTGS().PresentCurrentFrame();
		}
		return true;
	}
	catch (Exception::BaseException& e)
	{
		Host::ReportErrorAsync("Failed to load save state", e.UserMsg());
		Host::OnSaveStateLoaded(filename, false);
		return false;
	}
}

bool VMManager::DoSaveState(const char* filename, s32 slot_for_message, bool zip_on_thread, bool backup_old_state)
{
	if (GSDumpReplayer::IsReplayingDump())
		return false;

	std::string osd_key(fmt::format("SaveStateSlot{}", slot_for_message));

	try
	{
		std::unique_ptr<ArchiveEntryList> elist(SaveState_DownloadState());
		std::unique_ptr<SaveStateScreenshotData> screenshot(SaveState_SaveScreenshot());

		if (FileSystem::FileExists(filename) && backup_old_state)
		{
			const std::string backup_filename(fmt::format("{}.backup", filename));
			Console.WriteLn(fmt::format("Creating save state backup {}...", backup_filename));
			if (!FileSystem::RenamePath(filename, backup_filename.c_str()))
			{
				Host::AddIconOSDMessage(std::move(osd_key), ICON_FA_EXCLAMATION_TRIANGLE,
					fmt::format("Failed to back up old save state {}.", Path::GetFileName(filename)), Host::OSD_ERROR_DURATION);
			}
		}

		if (zip_on_thread)
		{
			// lock order here is important; the thread could exit before we resume here.
			std::unique_lock lock(s_save_state_threads_mutex);
			s_save_state_threads.emplace_back(&VMManager::ZipSaveStateOnThread,
				std::move(elist), std::move(screenshot), std::move(osd_key), std::string(filename),
				slot_for_message);
		}
		else
		{
			ZipSaveState(std::move(elist), std::move(screenshot), std::move(osd_key), filename, slot_for_message);
		}

		Host::OnSaveStateSaved(filename);
		return true;
	}
	catch (Exception::BaseException& e)
	{
		Host::AddIconOSDMessage(std::move(osd_key), ICON_FA_EXCLAMATION_TRIANGLE, fmt::format("Failed to save save state: {}.", e.DiagMsg()),
			Host::OSD_ERROR_DURATION);
		return false;
	}
}

void VMManager::ZipSaveState(std::unique_ptr<ArchiveEntryList> elist,
	std::unique_ptr<SaveStateScreenshotData> screenshot, std::string osd_key,
	const char* filename, s32 slot_for_message)
{
	Common::Timer timer;

	if (SaveState_ZipToDisk(std::move(elist), std::move(screenshot), filename))
	{
		if (slot_for_message >= 0 && VMManager::HasValidVM())
			Host::AddIconOSDMessage(std::move(osd_key), ICON_FA_SAVE, fmt::format("State saved to slot {}.", slot_for_message),
				Host::OSD_QUICK_DURATION);
	}
	else
	{
		Host::AddIconOSDMessage(std::move(osd_key), ICON_FA_EXCLAMATION_TRIANGLE, fmt::format("Failed to save save state to slot {}.", slot_for_message),
			Host::OSD_ERROR_DURATION);
	}

	DevCon.WriteLn("Zipping save state to '%s' took %.2f ms", filename, timer.GetTimeMilliseconds());

	Host::InvalidateSaveStateCache();
}

void VMManager::ZipSaveStateOnThread(std::unique_ptr<ArchiveEntryList> elist, std::unique_ptr<SaveStateScreenshotData> screenshot,
	std::string osd_key, std::string filename, s32 slot_for_message)
{
	ZipSaveState(std::move(elist), std::move(screenshot), std::move(osd_key), filename.c_str(), slot_for_message);

	// remove ourselves from the thread list. if we're joining, we might not be in there.
	const auto this_id = std::this_thread::get_id();
	std::unique_lock lock(s_save_state_threads_mutex);
	for (auto it = s_save_state_threads.begin(); it != s_save_state_threads.end(); ++it)
	{
		if (it->get_id() == this_id)
		{
			it->detach();
			s_save_state_threads.erase(it);
			break;
		}
	}
}

void VMManager::WaitForSaveStateFlush()
{
	std::unique_lock lock(s_save_state_threads_mutex);
	while (!s_save_state_threads.empty())
	{
		// take a thread from the list and join with it. it won't self detatch then, but that's okay,
		// since we're joining with it here.
		std::thread save_thread(std::move(s_save_state_threads.front()));
		s_save_state_threads.pop_front();
		lock.unlock();
		save_thread.join();
		lock.lock();
	}
}

u32 VMManager::DeleteSaveStates(const char* game_serial, u32 game_crc, bool also_backups /* = true */)
{
	WaitForSaveStateFlush();

	u32 deleted = 0;
	for (s32 i = -1; i <= NUM_SAVE_STATE_SLOTS; i++)
	{
		std::string filename(GetSaveStateFileName(game_serial, game_crc, i));
		if (FileSystem::FileExists(filename.c_str()) && FileSystem::DeleteFilePath(filename.c_str()))
			deleted++;

		if (also_backups)
		{
			filename += ".backup";
			if (FileSystem::FileExists(filename.c_str()) && FileSystem::DeleteFilePath(filename.c_str()))
				deleted++;
		}
	}

	return deleted;
}

bool VMManager::LoadState(const char* filename)
{
#ifdef ENABLE_ACHIEVEMENTS
	if (Achievements::ChallengeModeActive() &&
		!Achievements::ConfirmChallengeModeDisable("Loading state"))
	{
		return false;
	}
#endif

	// TODO: Save the current state so we don't need to reset.
	if (DoLoadState(filename))
		return true;

	Reset();
	return false;
}

bool VMManager::LoadStateFromSlot(s32 slot)
{
	const std::string filename(GetCurrentSaveStateFileName(slot));
	if (filename.empty())
	{
		Host::AddIconOSDMessage("LoadStateFromSlot", ICON_FA_EXCLAMATION_TRIANGLE, fmt::format("There is no save state in slot {}.", slot), 5.0f);
		return false;
	}

#ifdef ENABLE_ACHIEVEMENTS
	if (Achievements::ChallengeModeActive() &&
		!Achievements::ConfirmChallengeModeDisable("Loading state"))
	{
		return false;
	}
#endif

	Host::AddIconOSDMessage("LoadStateFromSlot", ICON_FA_FOLDER_OPEN, fmt::format("Loading state from slot {}...", slot), Host::OSD_QUICK_DURATION);
	return DoLoadState(filename.c_str());
}

bool VMManager::SaveState(const char* filename, bool zip_on_thread, bool backup_old_state)
{
	return DoSaveState(filename, -1, zip_on_thread, backup_old_state);
}

bool VMManager::SaveStateToSlot(s32 slot, bool zip_on_thread)
{
	const std::string filename(GetCurrentSaveStateFileName(slot));
	if (filename.empty())
		return false;

	// if it takes more than a minute.. well.. wtf.
	Host::AddIconOSDMessage(fmt::format("SaveStateSlot{}", slot), ICON_FA_SAVE, fmt::format("Saving state to slot {}...", slot), 60.0f);
	return DoSaveState(filename.c_str(), slot, zip_on_thread, EmuConfig.BackupSavestate);
}

LimiterModeType VMManager::GetLimiterMode()
{
	return EmuConfig.LimiterMode;
}

void VMManager::SetLimiterMode(LimiterModeType type)
{
	if (EmuConfig.LimiterMode == type)
		return;

	EmuConfig.LimiterMode = type;
	gsUpdateFrequency(EmuConfig);
}

void VMManager::FrameAdvance(u32 num_frames /*= 1*/)
{
	if (!HasValidVM())
		return;

#ifdef ENABLE_ACHIEVEMENTS
	if (Achievements::ChallengeModeActive() && !Achievements::ConfirmChallengeModeDisable("Frame advancing"))
		return;
#endif

	s_frame_advance_count = num_frames;
	SetState(VMState::Running);
}

bool VMManager::ChangeDisc(CDVD_SourceType source, std::string path)
{
	const CDVD_SourceType old_type = CDVDsys_GetSourceType();
	const std::string old_path(CDVDsys_GetFile(old_type));

	const std::string display_name((source != CDVD_SourceType::Iso) ? path : FileSystem::GetDisplayNameFromPath(path));
	CDVDsys_ChangeSource(source);
	if (!path.empty())
		CDVDsys_SetFile(source, std::move(path));

	const bool result = DoCDVDopen();
	if (result)
	{
		if (source == CDVD_SourceType::NoDisc)
			Host::AddIconOSDMessage("ChangeDisc", ICON_FA_COMPACT_DISC, "Disc removed.", Host::OSD_INFO_DURATION);
		else
			Host::AddIconOSDMessage("ChangeDisc", ICON_FA_COMPACT_DISC, fmt::format("Disc changed to '{}'.", display_name), Host::OSD_INFO_DURATION);
	}
	else
	{
		Host::AddIconOSDMessage("ChangeDisc", ICON_FA_COMPACT_DISC, fmt::format("Failed to open new disc image '{}'. Reverting to old image.", display_name),
			Host::OSD_ERROR_DURATION);
		CDVDsys_ChangeSource(old_type);
		if (!old_path.empty())
			CDVDsys_SetFile(old_type, std::move(old_path));
		if (!DoCDVDopen())
		{
			Host::AddIconOSDMessage("ChangeDisc", ICON_FA_COMPACT_DISC, "Failed to switch back to old disc image. Removing disc.", Host::OSD_CRITICAL_ERROR_DURATION);
			CDVDsys_ChangeSource(CDVD_SourceType::NoDisc);
			DoCDVDopen();
		}
	}

	cdvdCtrlTrayOpen();
	return result;
}

bool VMManager::IsElfFileName(const std::string_view& path)
{
	return StringUtil::EndsWithNoCase(path, ".elf");
}

bool VMManager::IsBlockDumpFileName(const std::string_view& path)
{
	return StringUtil::EndsWithNoCase(path, ".dump");
}

bool VMManager::IsGSDumpFileName(const std::string_view& path)
{
	return (StringUtil::EndsWithNoCase(path, ".gs") ||
			StringUtil::EndsWithNoCase(path, ".gs.xz") ||
			StringUtil::EndsWithNoCase(path, ".gs.zst"));
}

bool VMManager::IsSaveStateFileName(const std::string_view& path)
{
	return StringUtil::EndsWithNoCase(path, ".p2s");
}

bool VMManager::IsDiscFileName(const std::string_view& path)
{
	static const char* extensions[] = {".iso", ".bin", ".img", ".mdf", ".gz", ".cso", ".chd"};

	for (const char* test_extension : extensions)
	{
		if (StringUtil::EndsWithNoCase(path, test_extension))
			return true;
	}

	return false;
}

bool VMManager::IsLoadableFileName(const std::string_view& path)
{
	return IsDiscFileName(path) || IsElfFileName(path) || IsGSDumpFileName(path) || IsBlockDumpFileName(path);
}

void VMManager::Execute()
{
	// Check for interpreter<->recompiler switches.
	if (std::exchange(s_cpu_implementation_changed, false))
	{
		// We need to switch the cpus out, and reset the new ones if so.
		s_cpu_provider_pack->ApplyConfig();
		SysClearExecutionCache();
		vtlb_ResetFastmem();
	}

	// Execute until we're asked to stop.
	Cpu->Execute();
}

void VMManager::SetPaused(bool paused)
{
	if (!HasValidVM())
		return;

	Console.WriteLn(paused ? "(VMManager) Pausing..." : "(VMManager) Resuming...");
	SetState(paused ? VMState::Paused : VMState::Running);
}

const std::string& VMManager::Internal::GetElfOverride()
{
	return s_elf_override;
}

bool VMManager::Internal::IsExecutionInterrupted()
{
	return s_state.load(std::memory_order_relaxed) != VMState::Running || s_cpu_implementation_changed;
}

void VMManager::Internal::EntryPointCompilingOnCPUThread()
{
	// Classic chicken and egg problem here. We don't want to update the running game
	// until the game entry point actually runs, because that can update settings, which
	// can flush the JIT, etc. But we need to apply patches for games where the entry
	// point is in the patch (e.g. WRC 4). So. Gross, but the only way to handle it really.
	LoadPatches(SysGetDiscID(), ElfCRC, true, false);
	ApplyLoadedPatches(PPT_ONCE_ON_LOAD);
}

void VMManager::Internal::GameStartingOnCPUThread()
{
	UpdateRunningGame(false, true);
	ApplyLoadedPatches(PPT_ONCE_ON_LOAD);
	ApplyLoadedPatches(PPT_COMBINED_0_1);
}

void VMManager::Internal::VSyncOnCPUThread()
{
	// TODO: Move frame limiting here to reduce CPU usage after sleeping...
	ApplyLoadedPatches(PPT_CONTINUOUSLY);
	ApplyLoadedPatches(PPT_COMBINED_0_1);

	// Frame advance must be done *before* pumping messages, because otherwise
	// we'll immediately reduce the counter we just set.
	if (s_frame_advance_count > 0)
	{
		s_frame_advance_count--;
		if (s_frame_advance_count == 0)
		{
			// auto pause at the end of frame advance
			SetState(VMState::Paused);
		}
	}

	Host::CPUThreadVSync();

	if (EmuConfig.EnableRecordingTools)
	{
		// This code is called _before_ Counter's vsync end, and _after_ vsync start
		if (g_InputRecording.isActive())
		{
			// Process any outstanding recording actions (ie. toggle mode, stop the recording, etc)
			g_InputRecording.processRecordQueue();
			g_InputRecording.getControls().processControlQueue();
			// Increment our internal frame counter, used to keep track of when we hit the end, etc.
			g_InputRecording.incFrameCounter();
			g_InputRecording.handleExceededFrameCounter();
		}
		// At this point, the PAD data has been read from the user for the current frame
		// so we can either read from it, or overwrite it!
		g_InputRecording.handleControllerDataUpdate();
	}
}

void VMManager::CheckForCPUConfigChanges(const Pcsx2Config& old_config)
{
	if (EmuConfig.Cpu == old_config.Cpu &&
		EmuConfig.Gamefixes == old_config.Gamefixes &&
		EmuConfig.Speedhacks == old_config.Speedhacks &&
		EmuConfig.Profiler == old_config.Profiler)
	{
		return;
	}

	Console.WriteLn("Updating CPU configuration...");
	SetCPUState(EmuConfig.Cpu.sseMXCSR, EmuConfig.Cpu.sseVUMXCSR);
	SysClearExecutionCache();
	memBindConditionalHandlers();

	if (EmuConfig.Cpu.Recompiler.EnableFastmem != old_config.Cpu.Recompiler.EnableFastmem)
		vtlb_ResetFastmem();

	// did we toggle recompilers?
	if (EmuConfig.Cpu.CpusChanged(old_config.Cpu))
	{
		// This has to be done asynchronously, since we're still executing the
		// cpu when this function is called. Break the execution as soon as
		// possible and reset next time we're called.
		s_cpu_implementation_changed = true;
	}

	if (EmuConfig.Cpu.AffinityControlMode != old_config.Cpu.AffinityControlMode ||
		EmuConfig.Speedhacks.vuThread != old_config.Speedhacks.vuThread)
	{
		SetEmuThreadAffinities();
	}
}

void VMManager::CheckForGSConfigChanges(const Pcsx2Config& old_config)
{
	if (EmuConfig.GS == old_config.GS)
		return;

	Console.WriteLn("Updating GS configuration...");

	if (EmuConfig.GS.FrameLimitEnable != old_config.GS.FrameLimitEnable)
		EmuConfig.LimiterMode = GetInitialLimiterMode();

	gsUpdateFrequency(EmuConfig);
	UpdateVSyncRate();
	frameLimitReset();
	GetMTGS().ApplySettings();
}

void VMManager::CheckForFramerateConfigChanges(const Pcsx2Config& old_config)
{
	if (EmuConfig.Framerate == old_config.Framerate)
		return;

	Console.WriteLn("Updating frame rate configuration");
	gsUpdateFrequency(EmuConfig);
	UpdateVSyncRate();
	frameLimitReset();
}

void VMManager::CheckForPatchConfigChanges(const Pcsx2Config& old_config)
{
	if (EmuConfig.EnableCheats == old_config.EnableCheats &&
		EmuConfig.EnableWideScreenPatches == old_config.EnableWideScreenPatches &&
		EmuConfig.EnablePatches == old_config.EnablePatches)
	{
		return;
	}

	ReloadPatches(true, true);
}

void VMManager::CheckForSPU2ConfigChanges(const Pcsx2Config& old_config)
{
	if (EmuConfig.SPU2 == old_config.SPU2)
		return;

	// TODO: Don't reinit on volume changes.

	Console.WriteLn("Updating SPU2 configuration");

	// kinda lazy, but until we move spu2 over...
	freezeData fd = {};
	if (SPU2freeze(FreezeAction::Size, &fd) != 0)
	{
		Console.Error("(CheckForSPU2ConfigChanges) Failed to get SPU2 freeze size");
		return;
	}

	std::unique_ptr<u8[]> fd_data = std::make_unique<u8[]>(fd.size);
	fd.data = fd_data.get();
	if (SPU2freeze(FreezeAction::Save, &fd) != 0)
	{
		Console.Error("(CheckForSPU2ConfigChanges) Failed to freeze SPU2");
		return;
	}

	const bool psxmode = SPU2IsRunningPSXMode();

	SPU2close();
	if (!SPU2open(psxmode ? PS2Modes::PSX : PS2Modes::PS2))
	{
		Console.Error("(CheckForSPU2ConfigChanges) Failed to reopen SPU2, we'll probably crash :(");
		return;
	}

	if (SPU2freeze(FreezeAction::Load, &fd) != 0)
	{
		Console.Error("(CheckForSPU2ConfigChanges) Failed to unfreeze SPU2");
		return;
	}
}

void VMManager::CheckForDEV9ConfigChanges(const Pcsx2Config& old_config)
{
	if (EmuConfig.DEV9 == old_config.DEV9)
		return;

	DEV9CheckChanges(old_config);
}

void VMManager::CheckForMemoryCardConfigChanges(const Pcsx2Config& old_config)
{
	bool changed = false;

	for (size_t i = 0; i < std::size(EmuConfig.Mcd); i++)
	{
		if (EmuConfig.Mcd[i].Enabled != old_config.Mcd[i].Enabled ||
			EmuConfig.Mcd[i].Filename != old_config.Mcd[i].Filename)
		{
			changed = true;
			break;
		}
	}

	changed |= (EmuConfig.McdEnableEjection != old_config.McdEnableEjection);
	changed |= (EmuConfig.McdFolderAutoManage != old_config.McdFolderAutoManage);

	if (!changed)
		return;

	Console.WriteLn("Updating memory card configuration");

	FileMcd_EmuClose();
	FileMcd_EmuOpen();

	// force card eject when files change
	for (u32 port = 0; port < 2; port++)
	{
		for (u32 slot = 0; slot < 4; slot++)
		{
			const uint index = FileMcd_ConvertToSlot(port, slot);
			if (EmuConfig.Mcd[index].Enabled != old_config.Mcd[index].Enabled ||
				EmuConfig.Mcd[index].Filename != old_config.Mcd[index].Filename)
			{
				Console.WriteLn("Ejecting memory card %u (port %u slot %u) due to source change", index, port, slot);
				AutoEject::Set(port, slot);
			}
		}
	}

	// force reindexing, mc folder code is janky
	std::string sioSerial;
	{
		std::unique_lock lock(s_info_mutex);
		if (const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(s_game_serial))
			sioSerial = game->memcardFiltersAsString();
		if (sioSerial.empty())
			sioSerial = s_game_serial;
	}
	sioSetGameSerial(sioSerial);
}

void VMManager::CheckForConfigChanges(const Pcsx2Config& old_config)
{
	if (HasValidVM())
	{
		CheckForCPUConfigChanges(old_config);
		CheckForFramerateConfigChanges(old_config);
		CheckForPatchConfigChanges(old_config);
		CheckForSPU2ConfigChanges(old_config);
		CheckForDEV9ConfigChanges(old_config);
		CheckForMemoryCardConfigChanges(old_config);
		USB::CheckForConfigChanges(old_config);

		if (EmuConfig.EnableCheats != old_config.EnableCheats ||
			EmuConfig.EnableWideScreenPatches != old_config.EnableWideScreenPatches ||
			EmuConfig.EnableNoInterlacingPatches != old_config.EnableNoInterlacingPatches)
		{
			VMManager::ReloadPatches(true, true);
		}
	}

	// For the big picture UI, we still need to update GS settings, since it's running,
	// and we don't update its config when we start the VM.
	if (HasValidVM() || GetMTGS().IsOpen())
		CheckForGSConfigChanges(old_config);

	Host::CheckForSettingsChanges(old_config);
}

void VMManager::ApplySettings()
{
	Console.WriteLn("Applying settings...");

	// if we're running, ensure the threads are synced
	const bool running = (s_state.load(std::memory_order_acquire) == VMState::Running);
	if (running)
	{
		if (THREAD_VU1)
			vu1Thread.WaitVU();
		GetMTGS().WaitGS(false);
	}

	// Reset to a clean Pcsx2Config. Otherwise things which are optional (e.g. gamefixes)
	// do not use the correct default values when loading.
	Pcsx2Config old_config(std::move(EmuConfig));
	EmuConfig = Pcsx2Config();
	EmuConfig.CopyRuntimeConfig(old_config);
	LoadSettings();
	CheckForConfigChanges(old_config);
}

bool VMManager::ReloadGameSettings()
{
	if (!UpdateGameSettingsLayer())
		return false;

	ApplySettings();
	return true;
}

void VMManager::SetDefaultSettings(SettingsInterface& si)
{
	{
		Pcsx2Config temp_config;
		SettingsSaveWrapper ssw(si);
		temp_config.LoadSave(ssw);
	}

	// Settings not part of the Pcsx2Config struct.
	si.SetBoolValue("EmuCore", "EnableFastBoot", true);

	SetHardwareDependentDefaultSettings(si);
}

void VMManager::EnforceAchievementsChallengeModeSettings()
{
	if (!Achievements::ChallengeModeActive())
		return;

	static constexpr auto ClampSpeed = [](float& rate) {
		if (rate > 0.0f && rate < 1.0f)
			rate = 1.0f;
	};

	// Can't use slow motion.
	ClampSpeed(EmuConfig.Framerate.NominalScalar);
	ClampSpeed(EmuConfig.Framerate.TurboScalar);
	ClampSpeed(EmuConfig.Framerate.SlomoScalar);

	// Can't use cheats.
	if (EmuConfig.EnableCheats)
	{
		Host::AddKeyedOSDMessage("ChallengeDisableCheats", "Cheats have been disabled due to achievements hardcore mode.", Host::OSD_WARNING_DURATION);
		EmuConfig.EnableCheats = false;
	}

	// Input recording/playback is probably an issue.
	EmuConfig.EnableRecordingTools = false;
	EmuConfig.EnablePINE = false;

	// Framerates should be at default.
	EmuConfig.GS.FramerateNTSC = Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_NTSC;
	EmuConfig.GS.FrameratePAL = Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_PAL;

	// You can overclock, but not underclock (since that might slow down the game and make it easier).
	EmuConfig.Speedhacks.EECycleRate = std::max<decltype(EmuConfig.Speedhacks.EECycleRate)>(EmuConfig.Speedhacks.EECycleRate, 0);
	EmuConfig.Speedhacks.EECycleSkip = 0;
}

void VMManager::LogUnsafeSettingsToConsole(const std::string& messages)
{
	// a not-great way of getting rid of the icons for the console message
	std::string console_messages(messages);
	for (;;)
	{
		const std::string::size_type pos = console_messages.find("\xef");
		if (pos != std::string::npos)
		{
			console_messages.erase(pos, pos + 3);
			console_messages.insert(pos, "[Unsafe Settings]");
		}
		else
		{
			break;
		}
	}
	Console.Warning(console_messages);
}

void VMManager::WarnAboutUnsafeSettings()
{
	std::string messages;

	if (EmuConfig.Speedhacks.fastCDVD)
		messages += ICON_FA_COMPACT_DISC " Fast CDVD is enabled, this may break games.\n";
	if (EmuConfig.Speedhacks.EECycleRate != 0 || EmuConfig.Speedhacks.EECycleSkip != 0)
		messages += ICON_FA_TACHOMETER_ALT " Cycle rate/skip is not at default, this may crash or make games run too slow.\n";
	if (EmuConfig.SPU2.SynchMode != Pcsx2Config::SPU2Options::SynchronizationMode::TimeStretch)
		messages += ICON_FA_VOLUME_MUTE " Audio is not using time stretch synchronization, this may break FMVs.\n";
	if (EmuConfig.GS.UpscaleMultiplier < 1.0f)
		messages += ICON_FA_TV " Upscale multiplier is below native, this will break rendering.\n";
	if (EmuConfig.GS.HWMipmap != HWMipmapLevel::Automatic)
		messages += ICON_FA_IMAGES " Mipmapping is not set to automatic. This may break rendering in some games.\n";
	if (EmuConfig.GS.TextureFiltering != BiFiltering::PS2)
		messages += ICON_FA_FILTER " Texture filtering is not set to Bilinear (PS2). This will break rendering in some games.\n";
	if (EmuConfig.GS.TriFilter != TriFiltering::Automatic)
		messages += ICON_FA_PAGER " Trilinear filtering is not set to automatic. This may break rendering in some games.\n";
	if (EmuConfig.GS.AccurateBlendingUnit <= AccBlendLevel::Minimum)
		messages += ICON_FA_BLENDER " Blending is below basic, this may break effects in some games.\n";
	if (EmuConfig.GS.CRCHack != CRCHackLevel::Automatic)
		messages += ICON_FA_FIRST_AID " CRC Fix Level is not set to default, this may break effects in some games.\n";
	if (EmuConfig.GS.HWDownloadMode != GSHardwareDownloadMode::Enabled)
		messages += ICON_FA_DOWNLOAD " Hardware Download Mode is not set to Accurate, this may break rendering in some games.\n";
	if (EmuConfig.Cpu.sseMXCSR.GetRoundMode() != SSEround_Chop || EmuConfig.Cpu.sseVUMXCSR.GetRoundMode() != SSEround_Chop)
		messages += ICON_FA_MICROCHIP " EE FPU Round Mode is not set to default, this may break some games.\n";
	if (!EmuConfig.Cpu.Recompiler.fpuOverflow || EmuConfig.Cpu.Recompiler.fpuExtraOverflow || EmuConfig.Cpu.Recompiler.fpuFullMode)
		messages += ICON_FA_MICROCHIP " EE FPU Clamp Mode is not set to default, this may break some games.\n";
	if (EmuConfig.Cpu.sseVUMXCSR.GetRoundMode() != SSEround_Chop)
		messages += ICON_FA_MICROCHIP " VU Round Mode is not set to default, this may break some games.\n";
	if (!EmuConfig.Cpu.Recompiler.vuOverflow || EmuConfig.Cpu.Recompiler.vuExtraOverflow || EmuConfig.Cpu.Recompiler.vuSignOverflow)
		messages += ICON_FA_MICROCHIP " VU Clamp Mode is not set to default, this may break some games.\n";
	if (!EmuConfig.EnableGameFixes)
		messages += ICON_FA_GAMEPAD " Game Fixes are not enabled. Compatibility with some games may be affected.\n";
	if (!EmuConfig.EnablePatches)
		messages += ICON_FA_GAMEPAD " Compatibility Patches are not enabled. Compatibility with some games may be affected.\n";
	if (EmuConfig.GS.FramerateNTSC != Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_NTSC)
		messages += ICON_FA_TV " Frame rate for NTSC is not default. This may break some games.\n";
	if (EmuConfig.GS.FrameratePAL != Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_PAL)
		messages += ICON_FA_TV " Frame rate for PAL is not default. This may break some games.\n";

	if (!messages.empty())
	{
		if (messages.back() == '\n')
			messages.pop_back();

		LogUnsafeSettingsToConsole(messages);
		Host::AddKeyedOSDMessage("unsafe_settings_warning", std::move(messages), Host::OSD_WARNING_DURATION);
	}
	else
	{
		Host::RemoveKeyedOSDMessage("unsafe_settings_warning");
	}

	messages.clear();
	if (!EmuConfig.Cpu.Recompiler.EnableEE)
		messages += ICON_FA_EXCLAMATION_CIRCLE " EE Recompiler is not enabled, this will significantly reduce performance.\n";
	if (!EmuConfig.Cpu.Recompiler.EnableVU0)
		messages += ICON_FA_EXCLAMATION_CIRCLE " VU0 Recompiler is not enabled, this will significantly reduce performance.\n";
	if (!EmuConfig.Cpu.Recompiler.EnableVU1)
		messages += ICON_FA_EXCLAMATION_CIRCLE " VU1 Recompiler is not enabled, this will significantly reduce performance.\n";
	if (!EmuConfig.Cpu.Recompiler.EnableIOP)
		messages += ICON_FA_EXCLAMATION_CIRCLE " IOP Recompiler is not enabled, this will significantly reduce performance.\n";
	if (EmuConfig.Cpu.Recompiler.EnableEECache)
		messages += ICON_FA_EXCLAMATION_CIRCLE " EE Cache is enabled, this will significantly reduce performance.\n";
	if (!EmuConfig.Speedhacks.WaitLoop)
		messages += ICON_FA_EXCLAMATION_CIRCLE " EE Wait Loop Detection is not enabled, this may reduce performance.\n";
	if (!EmuConfig.Speedhacks.IntcStat)
		messages += ICON_FA_EXCLAMATION_CIRCLE " INTC Spin Detection is not enabled, this may reduce performance.\n";
	if (!EmuConfig.Speedhacks.vu1Instant)
		messages += ICON_FA_EXCLAMATION_CIRCLE " Instant VU1 is disabled, this may reduce performance.\n";
	if (!EmuConfig.Speedhacks.vuFlagHack)
		messages += ICON_FA_EXCLAMATION_CIRCLE " mVU Flag Hack is not enabled, this may reduce performance.\n";
	if (EmuConfig.GS.GPUPaletteConversion)
		messages += ICON_FA_EXCLAMATION_CIRCLE " GPU Palette Conversion is enabled, this may reduce performance.\n";
	if (EmuConfig.GS.TexturePreloading != TexturePreloadingLevel::Full)
		messages += ICON_FA_EXCLAMATION_CIRCLE " Texture Preloading is not Full, this may reduce performance.\n";

	if (!messages.empty())
	{
		if (messages.back() == '\n')
			messages.pop_back();

		LogUnsafeSettingsToConsole(messages);
		Host::AddKeyedOSDMessage("performance_settings_warning", std::move(messages), Host::OSD_WARNING_DURATION);
	}
	else
	{
		Host::RemoveKeyedOSDMessage("performance_settings_warning");
	}
}

#ifdef _WIN32

#include "common/RedtapeWindows.h"

static bool s_timer_resolution_increased = false;

void VMManager::SetTimerResolutionIncreased(bool enabled)
{
	if (s_timer_resolution_increased == enabled)
		return;

	if (enabled)
	{
		s_timer_resolution_increased = (timeBeginPeriod(1) == TIMERR_NOERROR);
	}
	else if (s_timer_resolution_increased)
	{
		timeEndPeriod(1);
		s_timer_resolution_increased = false;
	}
}

#else

void VMManager::SetTimerResolutionIncreased(bool enabled)
{
}

#endif

static std::vector<u32> s_processor_list;
static std::once_flag s_processor_list_initialized;

#if defined(__linux__) || defined(_WIN32)

#include "cpuinfo.h"

static u32 GetProcessorIdForProcessor(const cpuinfo_processor* proc)
{
#if defined(__linux__)
	return static_cast<u32>(proc->linux_id);
#elif defined(_WIN32)
	return static_cast<u32>(proc->windows_processor_id);
#else
	return 0;
#endif
}

static void InitializeCPUInfo()
{
	if (!cpuinfo_initialize())
	{
		Console.Error("Failed to initialize cpuinfo");
		return;
	}

	const u32 cluster_count = cpuinfo_get_clusters_count();
	if (cluster_count == 0)
	{
		Console.Error("Invalid CPU count returned");
		return;
	}

	Console.WriteLn(Color_StrongYellow, "Processor count: %u cores, %u processors", cpuinfo_get_cores_count(), cpuinfo_get_processors_count());
	Console.WriteLn(Color_StrongYellow, "Cluster count: %u", cluster_count);

	static std::vector<const cpuinfo_processor*> ordered_processors;
	for (u32 i = 0; i < cluster_count; i++)
	{
		const cpuinfo_cluster* cluster = cpuinfo_get_cluster(i);
		for (u32 j = 0; j < cluster->processor_count; j++)
		{
			const cpuinfo_processor* proc = cpuinfo_get_processor(cluster->processor_start + j);
			if (!proc)
				continue;

			ordered_processors.push_back(proc);
		}
	}
	// find the large and small clusters based on frequency
	// this is assuming the large cluster is always clocked higher
	// sort based on core, so that hyperthreads get pushed down
	std::sort(ordered_processors.begin(), ordered_processors.end(), [](const cpuinfo_processor* lhs, const cpuinfo_processor* rhs) {
		return (lhs->core->frequency > rhs->core->frequency || lhs->smt_id < rhs->smt_id);
	});

	s_processor_list.reserve(ordered_processors.size());
	std::stringstream ss;
	ss << "Ordered processor list: ";
	for (const cpuinfo_processor* proc : ordered_processors)
	{
		if (proc != ordered_processors.front())
			ss << ", ";

		const u32 procid = GetProcessorIdForProcessor(proc);
		ss << procid;
		if (proc->smt_id != 0)
			ss << "[SMT " << proc->smt_id << "]";

		s_processor_list.push_back(procid);
	}
	Console.WriteLn(ss.str());
}

static void SetMTVUAndAffinityControlDefault(SettingsInterface& si)
{
	VMManager::EnsureCPUInfoInitialized();

	const u32 cluster_count = cpuinfo_get_clusters_count();
	if (cluster_count == 0)
	{
		Console.Error("Invalid CPU count returned");
		return;
	}

	Console.WriteLn("Cluster count: %u", cluster_count);

	for (u32 i = 0; i < cluster_count; i++)
	{
		const cpuinfo_cluster* cluster = cpuinfo_get_cluster(i);
		Console.WriteLn("  Cluster %u: %u cores and %u processors at %u MHz",
			i, cluster->core_count, cluster->processor_count, static_cast<u32>(cluster->frequency /* / 1000000u*/));
	}

	const bool has_big_little = cluster_count > 1;
	Console.WriteLn("Big-Little: %s", has_big_little ? "yes" : "no");

	const u32 big_cores = cpuinfo_get_cluster(0)->core_count + ((cluster_count > 2) ? cpuinfo_get_cluster(1)->core_count : 0u);
	Console.WriteLn("Guessing we have %u big/medium cores...", big_cores);

	if (big_cores >= 3)
	{
		Console.WriteLn("  So enabling MTVU.");
		si.SetBoolValue("EmuCore/Speedhacks", "vuThread", true);
	}
	else
	{
		Console.WriteLn("  So disabling MTVU.");
		si.SetBoolValue("EmuCore/Speedhacks", "vuThread", false);
	}
}

#else

static void InitializeCPUInfo()
{
	DevCon.WriteLn("(VMManager) InitializeCPUInfo() not implemented.");
}

static void SetMTVUAndAffinityControlDefault(SettingsInterface& si)
{
}

#endif

void VMManager::EnsureCPUInfoInitialized()
{
	std::call_once(s_processor_list_initialized, InitializeCPUInfo);
}

void VMManager::SetEmuThreadAffinities()
{
	EnsureCPUInfoInitialized();

	if (s_processor_list.empty())
	{
		// not supported on this platform
		return;
	}

	if (EmuConfig.Cpu.AffinityControlMode == 0 ||
		s_processor_list.size() < (EmuConfig.Speedhacks.vuThread ? 3 : 2))
	{
		if (EmuConfig.Cpu.AffinityControlMode != 0)
			Console.Error("Insufficient processors for affinity control.");

		GetMTGS().GetThreadHandle().SetAffinity(0);
		vu1Thread.GetThreadHandle().SetAffinity(0);
		s_vm_thread_handle.SetAffinity(0);
		return;
	}

	static constexpr u8 processor_assignment[7][2][3] = {
		//EE xx GS  EE VU GS
		{{0, 2, 1}, {0, 1, 2}}, // Disabled
		{{0, 2, 1}, {0, 1, 2}}, // EE > VU > GS
		{{0, 2, 1}, {0, 2, 1}}, // EE > GS > VU
		{{0, 2, 1}, {1, 0, 2}}, // VU > EE > GS
		{{1, 2, 0}, {2, 0, 1}}, // VU > GS > EE
		{{1, 2, 0}, {1, 2, 0}}, // GS > EE > VU
		{{1, 2, 0}, {2, 1, 0}}, // GS > VU > EE
	};

	// steal vu's thread if mtvu is off
	const u8* this_proc_assigment = processor_assignment[EmuConfig.Cpu.AffinityControlMode][EmuConfig.Speedhacks.vuThread];
	const u32 ee_index = s_processor_list[this_proc_assigment[0]];
	const u32 vu_index = s_processor_list[this_proc_assigment[1]];
	const u32 gs_index = s_processor_list[this_proc_assigment[2]];
	Console.WriteLn("Processor order assignment: EE=%u, VU=%u, GS=%u",
		this_proc_assigment[0], this_proc_assigment[1], this_proc_assigment[2]);

	const u64 ee_affinity = static_cast<u64>(1) << ee_index;
	Console.WriteLn(Color_StrongGreen, "EE thread is on processor %u (0x%llx)", ee_index, ee_affinity);
	s_vm_thread_handle.SetAffinity(ee_affinity);

	if (EmuConfig.Speedhacks.vuThread)
	{
		const u64 vu_affinity = static_cast<u64>(1) << vu_index;
		Console.WriteLn(Color_StrongGreen, "VU thread is on processor %u (0x%llx)", vu_index, vu_affinity);
		vu1Thread.GetThreadHandle().SetAffinity(vu_affinity);
	}
	else
	{
		vu1Thread.GetThreadHandle().SetAffinity(0);
	}

	const u64 gs_affinity = static_cast<u64>(1) << gs_index;
	Console.WriteLn(Color_StrongGreen, "GS thread is on processor %u (0x%llx)", gs_index, gs_affinity);
	GetMTGS().GetThreadHandle().SetAffinity(gs_affinity);
}

void VMManager::SetHardwareDependentDefaultSettings(SettingsInterface& si)
{
	SetMTVUAndAffinityControlDefault(si);
}

const std::vector<u32>& VMManager::GetSortedProcessorList()
{
	EnsureCPUInfoInitialized();
	return s_processor_list;
}
