// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Achievements.h"
#include "CDVD/CDVD.h"
#include "CDVD/IsoReader.h"
#include "Counters.h"
#include "DEV9/DEV9.h"
#include "DebugTools/DebugInterface.h"
#include "DebugTools/MIPSAnalyst.h"
#include "DebugTools/SymbolMap.h"
#include "Elfheader.h"
#include "FW.h"
#include "GS.h"
#include "GS/Renderers/HW/GSTextureReplacements.h"
#include "GSDumpReplayer.h"
#include "GameDatabase.h"
#include "GameList.h"
#include "Host.h"
#include "INISettingsInterface.h"
#include "ImGui/FullscreenUI.h"
#include "ImGui/ImGuiOverlays.h"
#include "Input/InputManager.h"
#include "IopBios.h"
#include "MTGS.h"
#include "MTVU.h"
#include "PINE.h"
#include "Patch.h"
#include "PerformanceMetrics.h"
#include "R3000A.h"
#include "R5900.h"
#include "Recording/InputRecording.h"
#include "Recording/InputRecordingControls.h"
#include "SIO/Memcard/MemoryCardFile.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Sio.h"
#include "SIO/Sio0.h"
#include "SIO/Sio2.h"
#include "SPU2/spu2.h"
#include "USB/USB.h"
#include "VMManager.h"
#include "ps2/BiosTools.h"
#include "svnrev.h"

#include "common/Console.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/FPControl.h"
#include "common/ScopedGuard.h"
#include "common/SettingsWrapper.h"
#include "common/SmallString.h"
#include "common/StringUtil.h"
#include "common/Threading.h"
#include "common/Timer.h"

#include "IconsFontAwesome5.h"
#include "IconsPromptFont.h"
#include "cpuinfo.h"
#include "discord_rpc.h"
#include "fmt/core.h"

#include <atomic>
#include <mutex>
#include <sstream>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <objbase.h>
#include <timeapi.h>
#include <powrprof.h>
#include <wil/com.h>
#include <dxgi.h>
#endif

#ifdef __APPLE__
#include "common/Darwin/DarwinMisc.h"
#endif

#ifdef _M_X86
#include "x86/newVif.h"
#endif

namespace VMManager
{
	static void SetDefaultLoggingSettings(SettingsInterface& si);
	static void UpdateLoggingSettings(SettingsInterface& si);

	static void LogCPUCapabilities();
	static void InitializeCPUProviders();
	static void ShutdownCPUProviders();
	static void UpdateCPUImplementations();

	static void ApplyGameFixes();
	static bool UpdateGameSettingsLayer();
	static void CheckForConfigChanges(const Pcsx2Config& old_config);
	static void CheckForCPUConfigChanges(const Pcsx2Config& old_config);
	static void CheckForGSConfigChanges(const Pcsx2Config& old_config);
	static void CheckForEmulationSpeedConfigChanges(const Pcsx2Config& old_config);
	static void CheckForPatchConfigChanges(const Pcsx2Config& old_config);
	static void CheckForDEV9ConfigChanges(const Pcsx2Config& old_config);
	static void CheckForMemoryCardConfigChanges(const Pcsx2Config& old_config);
	static void CheckForMiscConfigChanges(const Pcsx2Config& old_config);
	static void EnforceAchievementsChallengeModeSettings();
	static void LogUnsafeSettingsToConsole(const std::string& messages);
	static void WarnAboutUnsafeSettings();

	static bool AutoDetectSource(const std::string& filename);
	static void UpdateDiscDetails(bool booting);
	static void ClearDiscDetails();
	static void HandleELFChange(bool verbose_patches_if_changed);
	static void UpdateELFInfo(std::string elf_path);
	static void ClearELFInfo();
	static void ReportGameChangeToHost();
	static bool HasBootedELF();
	static bool HasValidOrInitializingVM();

	static std::string GetCurrentSaveStateFileName(s32 slot);
	static bool DoLoadState(const char* filename);
	static bool DoSaveState(const char* filename, s32 slot_for_message, bool zip_on_thread, bool backup_old_state);
	static void ZipSaveState(std::unique_ptr<ArchiveEntryList> elist,
		std::unique_ptr<SaveStateScreenshotData> screenshot, std::string osd_key, const char* filename,
		s32 slot_for_message);
	static void ZipSaveStateOnThread(std::unique_ptr<ArchiveEntryList> elist,
		std::unique_ptr<SaveStateScreenshotData> screenshot, std::string osd_key, std::string filename,
		s32 slot_for_message);

	static void LoadSettings();
	static void LoadCoreSettings(SettingsInterface* si);
	static void ApplyCoreSettings();
	static void UpdateInhibitScreensaver(bool allow);
	static void AccumulateSessionPlaytime();
	static void ResetResumeTimestamp();
	static void SaveSessionTime(const std::string& prev_serial);
	static void ReloadPINE();

	static float GetTargetSpeedForLimiterMode(LimiterModeType mode);
	static void ResetFrameLimiter();
	static double AdjustToHostRefreshRate(float frame_rate, float target_speed);

	static void SetTimerResolutionIncreased(bool enabled);
	static void SetHardwareDependentDefaultSettings(SettingsInterface& si);
	static void EnsureCPUInfoInitialized();
	static void SetEmuThreadAffinities();

	static void InitializeDiscordPresence();
	static void ShutdownDiscordPresence();
	static void PollDiscordPresence();
} // namespace VMManager

static constexpr u32 SETTINGS_VERSION = 1;

static std::unique_ptr<INISettingsInterface> s_game_settings_interface;
static std::unique_ptr<INISettingsInterface> s_input_settings_interface;

static bool s_log_block_system_console = false;
static bool s_log_force_file_log = false;

static std::atomic<VMState> s_state{VMState::Shutdown};
static bool s_cpu_implementation_changed = false;
static Threading::ThreadHandle s_vm_thread_handle;

static std::deque<std::thread> s_save_state_threads;
static std::mutex s_save_state_threads_mutex;

static std::recursive_mutex s_info_mutex;
static std::string s_disc_serial;
static std::string s_disc_elf;
static std::string s_disc_version;
static std::string s_title;
static std::string s_title_en_search;
static std::string s_title_en_replace;
static u32 s_disc_crc;
static u32 s_current_crc;
static u32 s_elf_entry_point = 0xFFFFFFFFu;
static std::string s_elf_path;
static std::pair<u32, u32> s_elf_text_range;
static bool s_elf_executed = false;
static std::string s_elf_override;
static std::string s_input_profile_name;
static u32 s_frame_advance_count = 0;
static bool s_fast_boot_requested = false;
static bool s_gs_open_on_initialize = false;

static LimiterModeType s_limiter_mode = LimiterModeType::Nominal;
static s64 s_limiter_ticks_per_frame = 0;
static u64 s_limiter_frame_start = 0;
static float s_target_speed = 0.0f;
static bool s_target_speed_synced_to_host = false;
static bool s_use_vsync_for_timing = false;

// Used to track play time. We use a monotonic timer here, in case of clock changes.
static u64 s_session_resume_timestamp = 0;
static u64 s_session_accumulated_playtime = 0;

static bool s_screensaver_inhibited = false;

static bool s_discord_presence_active = false;
static time_t s_discord_presence_time_epoch;

// Making GSDumpReplayer.h dependent on R5900.h is a no-no, since the GS uses it.
extern R5900cpu GSDumpReplayerCpu;

bool VMManager::PerformEarlyHardwareChecks(const char** error)
{
#define COMMON_DOWNLOAD_MESSAGE "PCSX2 builds can be downloaded from https://pcsx2.net/downloads/"

#if defined(_M_X86)
	// On Windows, this gets called as a global object constructor, before any of our objects are constructed.
	// So, we have to put it on the stack instead.
	cpuinfo_initialize();

	if (!cpuinfo_has_x86_sse4_1())
	{
		*error =
			"PCSX2 requires the Streaming SIMD 4.1 Extensions instruction set, which your CPU does not support.\n\n"
			"SSE4.1 is now a minimum requirement for PCSX2. You should either upgrade your CPU, or use an older build "
			"such as 1.6.0.\n\n" COMMON_DOWNLOAD_MESSAGE;
		return false;
	}

#if _M_SSE >= 0x0501
	if (!cpuinfo_has_x86_avx2())
	{
		*error = "This build of PCSX2 requires the Advanced Vector Extensions 2 instruction set, which your CPU does "
				 "not support.\n\n"
				 "You should download and run the SSE4.1 build of PCSX2 instead, or upgrade to a CPU that supports "
				 "AVX2 to use this build.\n\n" COMMON_DOWNLOAD_MESSAGE;
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

void VMManager::AccumulateSessionPlaytime()
{
	s_session_accumulated_playtime += static_cast<u64>(Common::Timer::GetCurrentValue()) - s_session_resume_timestamp;
}

void VMManager::ResetResumeTimestamp()
{
	s_session_resume_timestamp = static_cast<u64>(Common::Timer::GetCurrentValue());
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
			MTGS::WaitGS(false);
			InputManager::PauseVibration();
		}
		else
		{
			PerformanceMetrics::Reset();
			ResetFrameLimiter();
		}

		SPU2::SetOutputPaused(paused);
		Achievements::OnVMPaused(paused);

		UpdateInhibitScreensaver(!paused && EmuConfig.InhibitScreensaver);

		if (paused)
		{
			Host::OnVMPaused();
			AccumulateSessionPlaytime();
		}
		else
		{
			Host::OnVMResumed();
			ResetResumeTimestamp();
		}
	}
	else if (state == VMState::Stopping && old_state == VMState::Running)
	{
		// If stopping, break execution as soon as possible.
		Cpu->ExitExecution();
	}
}

bool VMManager::HasValidVM()
{
	const VMState state = s_state.load(std::memory_order_acquire);
	return (state >= VMState::Running && state <= VMState::Resetting);
}

bool VMManager::HasValidOrInitializingVM()
{
	const VMState state = s_state.load(std::memory_order_acquire);
	return (state >= VMState::Initializing && state <= VMState::Resetting);
}

std::string VMManager::GetDiscPath()
{
	std::unique_lock lock(s_info_mutex);
	return CDVDsys_GetFile(CDVDsys_GetSourceType());
}

std::string VMManager::GetDiscSerial()
{
	std::unique_lock lock(s_info_mutex);
	return s_disc_serial;
}

std::string VMManager::GetDiscELF()
{
	std::unique_lock lock(s_info_mutex);
	return s_disc_elf;
}

std::string VMManager::GetTitle(bool prefer_en)
{
	std::unique_lock lock(s_info_mutex);
	std::string out = s_title;
	if (!s_title_en_search.empty())
	{
		size_t pos = out.find(s_title_en_search);
		if (pos != out.npos)
			out.replace(pos, s_title_en_search.size(), s_title_en_replace);
	}
	return out;
}

u32 VMManager::GetDiscCRC()
{
	std::unique_lock lock(s_info_mutex);
	return s_disc_crc;
}

std::string VMManager::GetDiscVersion()
{
	std::unique_lock lock(s_info_mutex);
	return s_disc_version;
}

u32 VMManager::GetCurrentCRC()
{
	return s_current_crc;
}

const std::string& VMManager::GetCurrentELF()
{
	return s_elf_path;
}

bool VMManager::Internal::CPUThreadInitialize()
{
	Threading::SetNameOfCurrentThread("CPU Thread");
	PerformanceMetrics::SetCPUThread(Threading::ThreadHandle::GetForCallingThread());

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

	// Use the default rounding mode, just in case it differs on some platform.
	FPControlRegister::SetCurrent(FPControlRegister::GetDefault());

	if (!cpuinfo_initialize())
		Console.Error("cpuinfo_initialize() failed.");

	LogCPUCapabilities();

	if (!SysMemory::Allocate())
	{
		Host::ReportErrorAsync("Error", "Failed to allocate VM memory.");
		return false;
	}

	InitializeCPUProviders();

	USBinit();

	// We want settings loaded so we choose the correct renderer for big picture mode.
	// This also sorts out input sources.
	LoadSettings();

	if (EmuConfig.Achievements.Enabled)
		Achievements::Initialize();

	ReloadPINE();

	if (EmuConfig.EnableDiscordPresence)
		InitializeDiscordPresence();

	return true;
}

void VMManager::Internal::CPUThreadShutdown()
{
	ShutdownDiscordPresence();

	PINEServer::Deinitialize();

	Achievements::Shutdown(false);

	InputManager::CloseSources();
	WaitForSaveStateFlush();

	PerformanceMetrics::SetCPUThread(Threading::ThreadHandle());

	USBshutdown();

	MTGS::ShutdownThread();
	GSJoinSnapshotThreads();

	ShutdownCPUProviders();

	SysMemory::Release();

#ifdef _WIN32
	CoUninitialize();
#endif

	// Ensure emulog gets flushed.
	Log::SetFileOutputLevel(LOGLEVEL_NONE, std::string());
}

void VMManager::Internal::SetFileLogPath(std::string path)
{
	s_log_force_file_log = Log::SetFileOutputLevel(LOGLEVEL_DEBUG, std::move(path));
	emuLog = Log::GetFileLogHandle();
}

void VMManager::Internal::SetBlockSystemConsole(bool block)
{
	s_log_block_system_console = block;
}

void VMManager::UpdateLoggingSettings(SettingsInterface& si)
{
#ifdef _DEBUG
	constexpr LOGLEVEL level = LOGLEVEL_DEBUG;
#else
	const LOGLEVEL level = (IsDevBuild || si.GetBoolValue("Logging", "EnableVerbose", false)) ? LOGLEVEL_DEV : LOGLEVEL_INFO;
#endif

	const bool system_console_enabled = !s_log_block_system_console && si.GetBoolValue("Logging", "EnableSystemConsole", false);
	const bool log_window_enabled = !s_log_block_system_console && si.GetBoolValue("Logging", "EnableLogWindow", false);
	const bool file_logging_enabled = s_log_force_file_log || si.GetBoolValue("Logging", "EnableFileLogging", false);

	if (system_console_enabled != Log::IsConsoleOutputEnabled())
		Log::SetConsoleOutputLevel(system_console_enabled ? level : LOGLEVEL_NONE);

	if (file_logging_enabled != Log::IsFileOutputEnabled())
	{
		std::string path = Path::Combine(EmuFolders::Logs, "emulog.txt");
		Log::SetFileOutputLevel(file_logging_enabled ? level : LOGLEVEL_NONE, std::move(path));
	}

	// Debug console only exists on Windows.
#ifdef _WIN32
	const bool debug_console_enabled = IsDebuggerPresent() && si.GetBoolValue("Logging", "EnableDebugConsole", false);
	Log::SetDebugOutputLevel(debug_console_enabled ? level : LOGLEVEL_NONE);
#else
	constexpr bool debug_console_enabled = false;
#endif

	const bool timestamps_enabled = si.GetBoolValue("Logging", "EnableTimestamps", true);
	Log::SetTimestampsEnabled(timestamps_enabled);

	const bool any_logging_sinks = system_console_enabled || log_window_enabled || file_logging_enabled || debug_console_enabled;

	const bool ee_console_enabled = any_logging_sinks && si.GetBoolValue("Logging", "EnableEEConsole", false);
	SysConsole.eeConsole.Enabled = ee_console_enabled;

	SysConsole.iopConsole.Enabled = any_logging_sinks && si.GetBoolValue("Logging", "EnableIOPConsole", false);
	SysTrace.IOP.R3000A.Enabled = true;
	SysTrace.IOP.COP2.Enabled = true;
	SysTrace.IOP.Memory.Enabled = true;
	SysTrace.SIF.Enabled = true;

	// Input Recording Logs
	SysConsole.recordingConsole.Enabled = any_logging_sinks && si.GetBoolValue("Logging", "EnableInputRecordingLogs", true);
	SysConsole.controlInfo.Enabled = any_logging_sinks && si.GetBoolValue("Logging", "EnableControllerLogs", false);
}

void VMManager::SetDefaultLoggingSettings(SettingsInterface& si)
{
	si.SetBoolValue("Logging", "EnableSystemConsole", false);
	si.SetBoolValue("Logging", "EnableFileLogging", false);
	si.SetBoolValue("Logging", "EnableTimestamps", true);
	si.SetBoolValue("Logging", "EnableVerbose", false);
	si.SetBoolValue("Logging", "EnableEEConsole", false);
	si.SetBoolValue("Logging", "EnableIOPConsole", false);
	si.SetBoolValue("Logging", "EnableInputRecordingLogs", true);
	si.SetBoolValue("Logging", "EnableControllerLogs", false);
}

bool VMManager::Internal::CheckSettingsVersion()
{
	SettingsInterface* bsi = Host::Internal::GetBaseSettingsLayer();

	uint settings_version;
	return (bsi->GetUIntValue("UI", "SettingsVersion", &settings_version) && settings_version == SETTINGS_VERSION);
}

void VMManager::Internal::LoadStartupSettings()
{
	SettingsInterface* bsi = Host::Internal::GetBaseSettingsLayer();
	EmuFolders::LoadConfig(*bsi);
	EmuFolders::EnsureFoldersExist();

	// We need to create the console window early, otherwise it appears behind the main window.
	UpdateLoggingSettings(*bsi);

#ifdef ENABLE_RAINTEGRATION
	// RAIntegration switch must happen before the UI is created.
	if (Host::GetBaseBoolSettingValue("Achievements", "UseRAIntegration", false))
		Achievements::SwitchToRAIntegration();
#endif
}

void VMManager::SetDefaultSettings(
	SettingsInterface& si, bool folders, bool core, bool controllers, bool hotkeys, bool ui)
{
	FPControlRegisterBackup fpcr_backup(FPControlRegister::GetDefault());

	if (si.GetUIntValue("UI", "SettingsVersion", 0u) != SETTINGS_VERSION)
		si.SetUIntValue("UI", "SettingsVersion", SETTINGS_VERSION);

	if (folders)
		EmuFolders::SetDefaults(si);
	if (core)
	{
		Pcsx2Config temp_config;
		SettingsSaveWrapper ssw(si);
		temp_config.LoadSave(ssw);

		// Settings not part of the Pcsx2Config struct.
		si.SetBoolValue("EmuCore", "EnableFastBoot", true);

		SetHardwareDependentDefaultSettings(si);
		SetDefaultLoggingSettings(si);
	}
	if (controllers)
	{
		Pad::SetDefaultControllerConfig(si);
		USB::SetDefaultConfiguration(&si);
	}
	if (hotkeys)
		Pad::SetDefaultHotkeyConfig(si);
	if (ui)
		Host::SetDefaultUISettings(si);
}

void VMManager::LoadSettings()
{
	// Switch the rounding mode back to the system default for loading settings.
	// We might have a different mode, because this can be called during setting updates while a VM is active,
	// and the rounding mode has an impact on the conversion of floating-point values to/from strings.
	FPControlRegisterBackup fpcr_backup(FPControlRegister::GetDefault());

	std::unique_lock<std::mutex> lock = Host::GetSettingsLock();
	SettingsInterface* si = Host::GetSettingsInterface();
	LoadCoreSettings(si);
	Pad::LoadConfig(*si);
	Host::LoadSettings(*si, lock);
	InputManager::ReloadSources(*si, lock);
	InputManager::ReloadBindings(*si, *Host::GetSettingsInterfaceForBindings());
	UpdateLoggingSettings(*si);

	if (HasValidOrInitializingVM())
	{
		WarnAboutUnsafeSettings();
		ApplyGameFixes();
	}
}

void VMManager::LoadCoreSettings(SettingsInterface* si)
{
	SettingsLoadWrapper slw(*si);
	EmuConfig.LoadSave(slw);
	Patch::ApplyPatchSettingOverrides();

	// Achievements hardcore mode disallows setting some configuration options.
	EnforceAchievementsChallengeModeSettings();

	// Remove any user-specified hacks in the config (we don't want stale/conflicting values when it's globally disabled).
	EmuConfig.GS.MaskUserHacks();
	EmuConfig.GS.MaskUpscalingHacks();

	// Force MTVU off when playing back GS dumps, it doesn't get used.
	if (GSDumpReplayer::IsReplayingDump())
		EmuConfig.Speedhacks.vuThread = false;
}

void VMManager::ApplyGameFixes()
{
	if (!HasBootedELF() && !GSDumpReplayer::IsReplayingDump())
	{
		// Instant DMA needs to be on for this BIOS (font rendering is broken without it, possible cache issues).
		EmuConfig.Gamefixes.InstantDMAHack = true;

		// Disable user's manual hardware fixes, it might be problematic.
		EmuConfig.GS.ManualUserHacks = false;
		return;
	}

	const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(s_disc_serial);
	if (!game)
		return;

	game->applyGameFixes(EmuConfig, EmuConfig.EnableGameFixes);
	game->applyGSHardwareFixes(EmuConfig.GS);

	// Re-remove upscaling fixes, make sure they don't apply at native res.
	// We do this in LoadCoreSettings(), but game fixes get applied afterwards because of the unsafe warning.
	EmuConfig.GS.MaskUpscalingHacks();
}

void VMManager::ApplySettings()
{
	Console.WriteLn("Applying settings...");

	// If we're running, ensure the threads are synced.
	if (GetState() == VMState::Running)
	{
		if (THREAD_VU1)
			vu1Thread.WaitVU();
		MTGS::WaitGS(false);
	}

	// Reset to a clean Pcsx2Config. Otherwise things which are optional (e.g. gamefixes)
	// do not use the correct default values when loading.
	Pcsx2Config old_config(std::move(EmuConfig));
	EmuConfig = Pcsx2Config();
	EmuConfig.CopyRuntimeConfig(old_config);
	LoadSettings();
	CheckForConfigChanges(old_config);
}

void VMManager::ApplyCoreSettings()
{
	// Lightweight version of above, called when ELF changes. This should not get called without an active VM.
	pxAssertRel(HasValidOrInitializingVM(), "Reloading core settings requires a valid VM.");
	Console.WriteLn("Applying core settings...");

	// If we're running, ensure the threads are synced.
	if (GetState() == VMState::Running)
	{
		if (THREAD_VU1)
			vu1Thread.WaitVU();
		MTGS::WaitGS(false);
	}

	// Reset to a clean Pcsx2Config. Otherwise things which are optional (e.g. gamefixes)
	// do not use the correct default values when loading.
	Pcsx2Config old_config(std::move(EmuConfig));
	EmuConfig = Pcsx2Config();
	EmuConfig.CopyRuntimeConfig(old_config);

	{
		FPControlRegisterBackup fpcr_backup(FPControlRegister::GetDefault());
		std::unique_lock<std::mutex> lock = Host::GetSettingsLock();
		LoadCoreSettings(Host::GetSettingsInterface());
		WarnAboutUnsafeSettings();
		ApplyGameFixes();
	}

	CheckForConfigChanges(old_config);
}

bool VMManager::ReloadGameSettings()
{
	if (!UpdateGameSettingsLayer())
		return false;

	// Patches must come first, because they can affect aspect ratio/interlacing.
	Patch::UpdateActivePatches(true, false, true);
	ApplySettings();
	return true;
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
	ElfObject elfo;
	if (!elfo.OpenFile(elf_path, false, nullptr))
		return iso_path;

	const u32 crc = elfo.GetCRC();
	if (crc != 0)
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

std::string VMManager::GetDebuggerSettingsFilePath(const std::string_view& game_serial, u32 game_crc)
{
	std::string path;
	if (!game_serial.empty() && game_crc != 0)
	{
		auto lock = Host::GetSettingsLock();
		return Path::Combine(EmuFolders::DebuggerSettings, fmt::format("{}_{:08X}.json", game_serial, game_crc));
	}
	return path;
}

std::string VMManager::GetDebuggerSettingsFilePathForCurrentGame()
{
	return GetDebuggerSettingsFilePath(s_disc_serial, s_current_crc);
}

void VMManager::Internal::UpdateEmuFolders()
{
	const std::string old_cheats_directory(EmuFolders::Cheats);
	const std::string old_patches_directory(EmuFolders::Patches);
	const std::string old_memcards_directory(EmuFolders::MemoryCards);
	const std::string old_textures_directory(EmuFolders::Textures);
	const std::string old_videos_directory(EmuFolders::Videos);

	auto lock = Host::GetSettingsLock();
	EmuFolders::LoadConfig(*Host::Internal::GetBaseSettingsLayer());
	EmuFolders::EnsureFoldersExist();

	if (VMManager::HasValidVM())
	{
		if (EmuFolders::Cheats != old_cheats_directory || EmuFolders::Patches != old_patches_directory)
			Patch::ReloadPatches(s_disc_serial, s_current_crc, true, false, true, true);

		if (EmuFolders::MemoryCards != old_memcards_directory)
		{
			std::string memcardFilters = "";
			if (const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(s_disc_serial))
			{
				memcardFilters = game->memcardFiltersAsString();
			}

			AutoEject::SetAll();

			if (!GSDumpReplayer::IsReplayingDump())
				FileMcd_Reopen(memcardFilters.empty() ? s_disc_serial : memcardFilters);
		}

		if (EmuFolders::Textures != old_textures_directory)
		{
			MTGS::RunOnGSThread([]() {
				if (VMManager::HasValidVM())
					GSTextureReplacements::ReloadReplacementMap();
			});
		}

		if (EmuFolders::Videos != old_videos_directory)
		{
			if (VMManager::HasValidVM())
				MTGS::RunOnGSThread(&GSEndCapture);
		}
	}
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
			if (GSgetDisplayMode() == GSVideoMode::SDTV_480P)
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
	return s_elf_override.empty() ? std::string(s_disc_serial) : std::string();
}

bool VMManager::UpdateGameSettingsLayer()
{
	std::unique_ptr<INISettingsInterface> new_interface;
	if (s_disc_crc != 0)
	{
		std::string filename(GetGameSettingsPath(GetSerialForGameSettings(), s_disc_crc));
		if (!FileSystem::FileExists(filename.c_str()))
		{
			// try the legacy format (crc.ini)
			filename = GetGameSettingsPath({}, s_disc_crc);
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

		Host::Internal::SetInputSettingsLayer(
			input_interface ? input_interface.get() : Host::Internal::GetBaseSettingsLayer());
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

void VMManager::UpdateDiscDetails(bool booting)
{
	std::string memcardFilters;
	{
		// Only need to protect writes with the mutex.
		std::unique_lock lock(s_info_mutex);
		const std::string old_serial = std::move(s_disc_serial);
		const u32 old_crc = s_disc_crc;
		bool serial_is_valid = false;
		std::string title;

		if (GSDumpReplayer::IsReplayingDump())
		{
			s_disc_serial = GSDumpReplayer::GetDumpSerial();
			s_disc_crc = GSDumpReplayer::GetDumpCRC();
			s_disc_elf = {};
			s_disc_version = {};
			serial_is_valid = !s_disc_serial.empty();
		}
		else if (CDVDsys_GetSourceType() != CDVD_SourceType::NoDisc)
		{
			cdvdGetDiscInfo(&s_disc_serial, &s_disc_elf, &s_disc_version, &s_disc_crc, nullptr);
			serial_is_valid = !s_disc_serial.empty();
		}
		else if (!s_elf_override.empty())
		{
			s_disc_serial = Path::GetFileTitle(s_elf_override);
			s_disc_version = {};
			s_disc_crc = 0; // set below
		}
		else
		{
			s_disc_serial = BiosSerial;
			s_disc_version = {};
			s_disc_crc = 0;
			title = fmt::format(TRANSLATE_FS("VMManager", "PS2 BIOS ({})"), BiosZone);
		}

		// If we're booting an ELF, use its CRC, not the disc (if any).
		if (!s_elf_override.empty())
			s_disc_crc = cdvdGetElfCRC(s_elf_override);

		if (!booting && s_disc_serial == old_serial && s_disc_crc == old_crc)
		{
			Console.WriteLn("Skipping disc details update, no change.");
			return;
		}

		SaveSessionTime(old_serial);

		s_title_en_search.clear();
		s_title_en_replace.clear();
		std::string custom_title = GameList::GetCustomTitleForPath(CDVDsys_GetFile(CDVDsys_GetSourceType()));
		if (serial_is_valid)
		{
			if (const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(s_disc_serial))
			{
				if (!game->name_en.empty())
				{
					s_title_en_search = game->name;
					s_title_en_replace = game->name_en;
				}

				std::string game_title = custom_title.empty() ? game->name : std::move(custom_title);

				// Append the ELF override if we're using it with a disc.
				if (!s_elf_override.empty())
				{
					title = fmt::format(
						"{} [{}]", game_title, Path::GetFileTitle(s_elf_override));
				}
				else
				{
					title = std::move(game_title);
				}

				memcardFilters = game->memcardFiltersAsString();
			}
			else
			{
				Console.Warning(fmt::format("Serial '{}' not found in GameDB.", s_disc_serial));
			}
		}

		if (title.empty())
		{
			title = std::move(custom_title);
		}

		if (title.empty())
		{
			if (!s_disc_serial.empty())
				title = fmt::format("{} [?]", s_disc_serial);
			else if (!s_disc_elf.empty())
				title = fmt::format("{} [?]", Path::GetFileName(IsoReader::RemoveVersionIdentifierFromPath(s_disc_elf)));
			else
				title = TRANSLATE_STR("VMManager", "Unknown Game");
		}

		s_title = std::move(title);
	}

	Console.WriteLn(Color_StrongGreen,
		fmt::format("Disc changed to {}.", Path::GetFileName(CDVDsys_GetFile(CDVDsys_GetSourceType()))));
	Console.WriteLn(Color_StrongGreen, fmt::format("  Name: {}", s_title));
	Console.WriteLn(Color_StrongGreen, fmt::format("  Serial: {}", s_disc_serial));
	Console.WriteLn(Color_StrongGreen, fmt::format("  Version: {}", s_disc_version));
	Console.WriteLn(Color_StrongGreen, fmt::format("  CRC: {:08X}", s_disc_crc));

	UpdateGameSettingsLayer();
	ApplySettings();

	// Patches are game-dependent, thus should get applied after game settings ia loaded.
	Patch::ReloadPatches(s_disc_serial, HasBootedELF() ? s_current_crc : 0, true, true, false, false);

	ReportGameChangeToHost();
	Achievements::GameChanged(s_disc_crc, s_current_crc);
	if (MTGS::IsOpen())
		MTGS::GameChanged();
	ReloadPINE();
	UpdateDiscordPresence(s_state.load(std::memory_order_relaxed) == VMState::Initializing);

	if (!GSDumpReplayer::IsReplayingDump())
		FileMcd_Reopen(memcardFilters.empty() ? s_disc_serial : memcardFilters);
}

void VMManager::ClearDiscDetails()
{
	std::unique_lock lock(s_info_mutex);
	s_disc_crc = 0;
	s_title = {};
	s_disc_version = {};
	s_disc_elf = {};
	s_disc_serial = {};
}

void VMManager::HandleELFChange(bool verbose_patches_if_changed)
{
	// Classic chicken and egg problem here. We don't want to update the running game
	// until the game entry point actually runs, because that can update settings, which
	// can flush the JIT, etc. But we need to apply patches for games where the entry
	// point is in the patch (e.g. WRC 4). So. Gross, but the only way to handle it really.
	const u32 crc_to_report = HasBootedELF() ? s_current_crc : 0;

	ReportGameChangeToHost();
	Achievements::GameChanged(s_disc_crc, crc_to_report);

	Console.WriteLn(Color_StrongOrange, fmt::format("ELF changed, active CRC {:08X} ({})", crc_to_report, s_elf_path));
	Patch::ReloadPatches(s_disc_serial, crc_to_report, false, false, false, verbose_patches_if_changed);
	ApplyCoreSettings();

	MIPSAnalyst::ScanForFunctions(
		R5900SymbolMap, s_elf_text_range.first, s_elf_text_range.first + s_elf_text_range.second, true);
}

void VMManager::UpdateELFInfo(std::string elf_path)
{
	Error error;
	ElfObject elfo;
	if (elf_path.empty() || !cdvdLoadElf(&elfo, elf_path, false, &error))
	{
		if (!elf_path.empty())
			Console.Error(fmt::format("Failed to read ELF being loaded: {}: {}", elf_path, error.GetDescription()));

		s_elf_path = {};
		s_elf_text_range = {};
		s_elf_entry_point = 0xFFFFFFFFu;
		s_current_crc = 0;
		return;
	}

	elfo.LoadHeaders();
	s_current_crc = elfo.GetCRC();
	s_elf_entry_point = elfo.GetEntryPoint();
	s_elf_text_range = elfo.GetTextRange();
	s_elf_path = std::move(elf_path);
}

void VMManager::ClearELFInfo()
{
	s_current_crc = 0;
	s_elf_executed = false;
	s_elf_text_range = {};
	s_elf_path = {};
	s_elf_entry_point = 0xFFFFFFFFu;
}

void VMManager::ReportGameChangeToHost()
{
	const std::string& disc_path = CDVDsys_GetFile(CDVDsys_GetSourceType());
	const u32 crc_to_report = HasBootedELF() ? s_current_crc : 0;
	FullscreenUI::GameChanged(disc_path, s_disc_serial, GetTitle(true), s_disc_crc, crc_to_report);
	Host::OnGameChanged(s_title, s_elf_override, disc_path, s_disc_serial, s_disc_crc, crc_to_report);
}

bool VMManager::HasBootedELF()
{
	return s_current_crc != 0 && s_elf_executed;
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

		if (IsGSDumpFileName(filename))
		{
			CDVDsys_ChangeSource(CDVD_SourceType::NoDisc);
			return GSDumpReplayer::Initialize(filename.c_str());
		}
		else if (IsElfFileName(filename))
		{
			// alternative way of booting an elf, change the elf override, and (optionally) use the disc
			// specified in the game settings.
			std::string disc_path = GetDiscOverrideFromGameSettings(filename);
			if (!disc_path.empty())
			{
				CDVDsys_SetFile(CDVD_SourceType::Iso, std::move(disc_path));
				CDVDsys_ChangeSource(CDVD_SourceType::Iso);
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
			return true;
		}
	}
	else
	{
		// make sure we're not fast booting when we have no filename
		CDVDsys_ChangeSource(CDVD_SourceType::NoDisc);
		return true;
	}
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
	VMManager::Internal::ResetVMHotkeyState();

	ScopedGuard close_state = [] {
		if (GSDumpReplayer::IsReplayingDump())
			GSDumpReplayer::Shutdown();

		s_elf_override = {};
		ClearELFInfo();
		ClearDiscDetails();

		Achievements::GameChanged(0, 0);
		FullscreenUI::GameChanged(s_title, std::string(), s_disc_serial, 0, 0);
		UpdateDiscordPresence(true);
		Host::OnGameChanged(s_title, std::string(), std::string(), s_disc_serial, 0, 0);

		UpdateGameSettingsLayer();
		s_state.store(VMState::Shutdown, std::memory_order_release);
		Host::OnVMDestroyed();
		ApplySettings();
	};

	std::string state_to_load;

	s_elf_override = std::move(boot_params.elf_override);
	if (!boot_params.save_state.empty())
		state_to_load = std::move(boot_params.save_state);

	// if we're loading an indexed save state, we need to get the serial/crc from the disc.
	if (boot_params.state_index.has_value())
	{
		if (boot_params.filename.empty())
		{
			Host::ReportErrorAsync("Error", "Cannot load an indexed save state without a boot filename.");
			return false;
		}

		state_to_load = GetSaveStateFileName(boot_params.filename.c_str(), boot_params.state_index.value());
		if (state_to_load.empty())
		{
			Host::ReportErrorAsync("Error", "Could not resolve path indexed save state load.");
			return false;
		}
	}

	// resolve source type
	if (boot_params.source_type.has_value())
	{
		if (boot_params.source_type.value() == CDVD_SourceType::Iso &&
			!FileSystem::FileExists(boot_params.filename.c_str()))
		{
			Host::ReportErrorAsync(
				"Error", fmt::format("Requested filename '{}' does not exist.", boot_params.filename));
			return false;
		}

		// Use specified source type.
		CDVDsys_SetFile(boot_params.source_type.value(), std::move(boot_params.filename));
		CDVDsys_ChangeSource(boot_params.source_type.value());
	}
	else
	{
		// Automatic type detection of boot parameter based on filename.
		if (!AutoDetectSource(boot_params.filename))
			return false;
	}

	ScopedGuard close_cdvd_files(&CDVDsys_ClearFiles);

	// Playing GS dumps don't need a BIOS.
	if (!GSDumpReplayer::IsReplayingDump())
	{
		Console.WriteLn("Loading BIOS...");
		if (!LoadBIOS())
		{
			Host::ReportErrorAsync(TRANSLATE_SV("VMManager", "Error"),
				TRANSLATE_SV("VMManager",
					"PCSX2 requires a PS2 BIOS in order to run.\n\n"
					"For legal reasons, you *must* obtain a BIOS from an actual PS2 unit that you own (borrowing "
					"doesn't count).\n\n"
					"Once dumped, this BIOS image should be placed in the bios folder within the data directory "
					"(Tools Menu -> Open Data Directory).\n\n"
					"Please consult the FAQs and Guides for further instructions."));
			return false;
		}
	}

	Error error;
	Console.WriteLn("Opening CDVD...");
	if (!DoCDVDopen(&error))
	{
		Host::ReportErrorAsync("Startup Error", fmt::format("Failed to open CDVD '{}': {}.",
													Path::GetFileName(CDVDsys_GetFile(CDVDsys_GetSourceType())),
													error.GetDescription()));
		return false;
	}
	ScopedGuard close_cdvd(&DoCDVDclose);

	// Figure out which game we're running! This also loads game settings.
	UpdateDiscDetails(true);

	ScopedGuard close_memcards(&FileMcd_EmuClose);

	// Read fast boot setting late so it can be overridden per-game.
	// ELFs must be fast booted, and GS dumps are never fast booted.
	s_fast_boot_requested =
		(boot_params.fast_boot.value_or(static_cast<bool>(EmuConfig.EnableFastBoot)) || !s_elf_override.empty()) &&
		(CDVDsys_GetSourceType() != CDVD_SourceType::NoDisc || !s_elf_override.empty()) &&
		!GSDumpReplayer::IsReplayingDump();

	if (!s_elf_override.empty())
	{
		if (!FileSystem::FileExists(s_elf_override.c_str()))
		{
			Host::ReportErrorAsync("Error", fmt::format("Requested boot ELF '{}' does not exist.", s_elf_override));
			return false;
		}

		Hle_SetHostRoot(s_elf_override.c_str());
	}
	else if (CDVDsys_GetSourceType() == CDVD_SourceType::Iso)
	{
		Hle_SetHostRoot(CDVDsys_GetFile(CDVDsys_GetSourceType()).c_str());
	}
	else
	{
		Hle_ClearHostRoot();
	}

	// Check for resuming and 'Boot and Debug' with hardcore mode.
	// Why do we need the boot param? Because we need some way of telling BootSystem() that
	// the user allowed HC mode to be disabled, because otherwise we'll ResetHardcoreMode()
	// and send ourselves into an infinite loop.
	if (boot_params.disable_achievements_hardcore_mode)
		Achievements::DisableHardcoreMode();
	else
		Achievements::ResetHardcoreMode(true);
	if (Achievements::IsHardcoreModeActive())
	{
		auto confirmHardcoreModeDisable = [&boot_params, &state_to_load](const char* trigger) mutable {
			if (FullscreenUI::IsInitialized())
			{
				boot_params.elf_override = std::move(s_elf_override);
				boot_params.save_state = std::move(state_to_load);
				boot_params.disable_achievements_hardcore_mode = true;
				s_elf_override = {};

				Achievements::ConfirmHardcoreModeDisableAsync(trigger,
					[boot_params = std::move(boot_params)](bool approved) mutable {
						if (approved && Initialize(std::move(boot_params)))
							SetState(VMState::Running);
					});

				return false;
			}
			else if (!Achievements::ConfirmHardcoreModeDisable(trigger))
			{
				return false;
			}
			return true;
		};

		if (!state_to_load.empty())
		{
			if (!confirmHardcoreModeDisable(TRANSLATE("VMManager", "Resuming state")))
			{
				return false;
			}
		}
		if (DebugInterface::getPauseOnEntry())
		{
			if (!confirmHardcoreModeDisable(TRANSLATE("VMManager", "Boot and Debug")))
			{
				return false;
			}
		}
	}

	s_limiter_mode = LimiterModeType::Nominal;
	s_target_speed = GetTargetSpeedForLimiterMode(s_limiter_mode);
	s_use_vsync_for_timing = false;

	s_cpu_implementation_changed = false;
	UpdateCPUImplementations();
	memSetExtraMemMode(EmuConfig.Cpu.ExtraMemory);
	Internal::ClearCPUExecutionCaches();
	FPControlRegister::SetCurrent(EmuConfig.Cpu.FPUFPCR);
	memBindConditionalHandlers();
	SysMemory::Reset();
	cpuReset();

	Console.WriteLn("Opening GS...");
	s_gs_open_on_initialize = MTGS::IsOpen();
	if (!s_gs_open_on_initialize && !MTGS::WaitForOpen())
	{
		// we assume GS is going to report its own error
		Console.WriteLn("Failed to open GS.");
		return false;
	}

	ScopedGuard close_gs = []() {
		if (!s_gs_open_on_initialize)
			MTGS::WaitForClose();
	};

	Console.WriteLn("Opening SPU2...");
	if (!SPU2::Open())
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize SPU2.");
		return false;
	}
	ScopedGuard close_spu2(&SPU2::Close);


	Console.WriteLn("Initializing Pad...");
	if (!Pad::Initialize())
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize PAD");
		return false;
	}
	ScopedGuard close_pad = &Pad::Shutdown;

	Console.WriteLn("Initializing SIO2...");
	if (!g_Sio2.Initialize())
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize SIO2");
		return false;
	}
	ScopedGuard close_sio2 = []() {
		g_Sio2.Shutdown();
	};

	Console.WriteLn("Initializing SIO0...");
	if (!g_Sio0.Initialize())
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize SIO0");
		return false;
	}
	ScopedGuard close_sio0 = []() {
		g_Sio0.Shutdown();
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
	ScopedGuard close_usb = []() { USBclose(); };

	Console.WriteLn("Opening FW...");
	if (FWopen() != 0)
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize FW.");
		return false;
	}
	ScopedGuard close_fw = []() { FWclose(); };

	// Don't close when we return
	close_fw.Cancel();
	close_usb.Cancel();
	close_dev9.Cancel();
	close_pad.Cancel();
	close_sio2.Cancel();
	close_sio0.Cancel();
	close_spu2.Cancel();
	close_gs.Cancel();
	close_memcards.Cancel();
	close_cdvd.Cancel();
	close_cdvd_files.Cancel();
	close_state.Cancel();

	hwReset();

	Console.WriteLn("VM subsystems initialized in %.2f ms", init_timer.GetTimeMilliseconds());
	s_state.store(VMState::Paused, std::memory_order_release);
	Host::OnVMStarted();
	FullscreenUI::OnVMStarted();
	UpdateInhibitScreensaver(EmuConfig.InhibitScreensaver);

	SetEmuThreadAffinities();

	// do we want to load state?
	if (!GSDumpReplayer::IsReplayingDump() && !state_to_load.empty())
	{
		if (!DoLoadState(state_to_load.c_str()))
		{
			Shutdown(false);
			return false;
		}
	}

	PerformanceMetrics::Clear();
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
	MTGS::WaitGS();

	if (!GSDumpReplayer::IsReplayingDump() && save_resume_state)
	{
		std::string resume_file_name(GetCurrentSaveStateFileName(-1));
		if (!resume_file_name.empty() && !DoSaveState(resume_file_name.c_str(), -1, true, false))
			Console.Error("Failed to save resume state");
	}

	// end input recording before clearing state
	if (g_InputRecording.isActive())
		g_InputRecording.stop();

	SaveSessionTime(s_disc_serial);
	s_elf_override = {};
	ClearELFInfo();
	CDVDsys_ClearFiles();

	{
		std::unique_lock lock(s_info_mutex);
		ClearDiscDetails();
	}

	Achievements::GameChanged(0, 0);
	FullscreenUI::GameChanged(s_title, std::string(), s_disc_serial, 0, 0);
	UpdateDiscordPresence(true);
	Host::OnGameChanged(s_title, std::string(), std::string(), s_disc_serial, 0, 0);

	s_fast_boot_requested = false;

	UpdateGameSettingsLayer();

	FPControlRegister::SetCurrent(FPControlRegister::GetDefault());

	Patch::UnloadPatches();
	R3000A::ioman::reset();
	vtlb_Shutdown();
	USBclose();
	SPU2::Close();
	Pad::Shutdown();
	g_Sio2.Shutdown();
	g_Sio0.Shutdown();
	MemcardBusy::ClearBusy();
	DEV9close();
	DoCDVDclose();
	FWclose();
	FileMcd_EmuClose();

	// If the fullscreen UI is running, do a hardware reset on the GS
	// so that the texture cache and targets are all cleared.
	if (s_gs_open_on_initialize)
	{
		MTGS::WaitGS(false, false, false);
		MTGS::ResetGS(true);
		MTGS::GameChanged();
	}
	else
	{
		MTGS::WaitForClose();
	}

	DEV9shutdown();

	if (GSDumpReplayer::IsReplayingDump())
		GSDumpReplayer::Shutdown();

	s_state.store(VMState::Shutdown, std::memory_order_release);
	FullscreenUI::OnVMDestroyed();
	SaveStateSelectorUI::Clear();
	UpdateInhibitScreensaver(false);
	Host::OnVMDestroyed();

	// clear out any potentially-incorrect settings from the last game
	LoadSettings();
}

void VMManager::Reset()
{
	pxAssert(HasValidVM());

	// If we're running, we're probably going to be executing this at event test time,
	// at vsync, which happens in the middle of event handling. Resetting everything
	// immediately here is a bad idea (tm), in fact, it breaks some games (e.g. TC:NYC).
	// So, instead, we tell the rec to exit execution, _then_ reset. Paused is fine here,
	// since the rec won't be running, so it's safe to immediately reset there.
	if (s_state.load(std::memory_order_acquire) == VMState::Running)
	{
		s_state.store(VMState::Resetting, std::memory_order_release);
		return;
	}

	if (!Achievements::ConfirmSystemReset())
		return;

	// Re-enforce hardcode mode constraints if we're now enabling it.
	if (Achievements::ResetHardcoreMode(false))
		ApplySettings();

	vu1Thread.WaitVU();
	vu1Thread.Reset();
	MTGS::WaitGS();

	const bool elf_was_changed = (s_current_crc != 0);
	ClearELFInfo();
	if (elf_was_changed)
		HandleELFChange(false);

	memSetExtraMemMode(EmuConfig.Cpu.ExtraMemory);
	Internal::ClearCPUExecutionCaches();
	memBindConditionalHandlers();
	SysMemory::Reset();
	cpuReset();
	hwReset();

	if (g_InputRecording.isActive())
	{
		g_InputRecording.handleReset();
		MTGS::PresentCurrentFrame();
	}

	ResetFrameLimiter();

	// If we were paused, state won't be resetting, so don't flip back to running.
	if (s_state.load(std::memory_order_acquire) == VMState::Resetting)
		s_state.store(VMState::Running, std::memory_order_release);
}

bool SaveStateBase::vmFreeze()
{
	const u32 prev_crc = s_current_crc;
	const std::string prev_elf = s_elf_path;
	const bool prev_elf_executed = s_elf_executed;
	Freeze(s_current_crc);
	FreezeString(s_elf_path);
	Freeze(s_elf_executed);

	// We have to test all the variables here, because we could be loading a state created during ELF load, after the ELF has loaded.
	if (IsLoading())
	{
		// Might need new ELF info.
		if (s_elf_path != prev_elf)
		{
			if (s_elf_path.empty())
			{
				// Shouldn't have executed a non-existant ELF.. unless you load state created from a deleted ELF override I guess.
				if (s_elf_executed)
					Console.Error("Somehow executed a non-existant ELF");
				VMManager::ClearELFInfo();
			}
			else
			{
				VMManager::UpdateELFInfo(std::move(s_elf_path));
			}
		}

		if (s_current_crc != prev_crc || s_elf_path != prev_elf || s_elf_executed != prev_elf_executed)
			VMManager::HandleELFChange(true);
	}

	return IsOkay();
}

std::string VMManager::GetSaveStateFileName(const char* game_serial, u32 game_crc, s32 slot)
{
	std::string filename;
	if (std::strlen(game_serial) > 0)
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
	if (GameList::GetSerialAndCRCForFilename(filename, &serial, &crc))
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
	return GetSaveStateFileName(s_disc_serial.c_str(), s_disc_crc, slot);
}

bool VMManager::DoLoadState(const char* filename)
{
	if (GSDumpReplayer::IsReplayingDump())
		return false;

	Host::OnSaveStateLoading(filename);

	Error error;
	if (!SaveState_UnzipFromDisk(filename, &error))
	{
		Host::ReportErrorAsync(TRANSLATE_SV("VMManager","Failed to load save state"), error.GetDescription());
		return false;
	}

	Host::OnSaveStateLoaded(filename, true);
	if (g_InputRecording.isActive())
	{
		g_InputRecording.handleLoadingSavestate();
		MTGS::PresentCurrentFrame();
	}

	return true;
}

bool VMManager::DoSaveState(const char* filename, s32 slot_for_message, bool zip_on_thread, bool backup_old_state)
{
	if (GSDumpReplayer::IsReplayingDump())
		return false;

	std::string osd_key(fmt::format("SaveStateSlot{}", slot_for_message));
	Error error;

	std::unique_ptr<ArchiveEntryList> elist = SaveState_DownloadState(&error);
	if (!elist)
	{
		Host::AddIconOSDMessage(std::move(osd_key), ICON_FA_EXCLAMATION_TRIANGLE,
			fmt::format(TRANSLATE_FS("VMManager", "Failed to save save state: {}."), error.GetDescription()),
			Host::OSD_ERROR_DURATION);
		return false;
	}

	std::unique_ptr<SaveStateScreenshotData> screenshot = SaveState_SaveScreenshot();

	if (FileSystem::FileExists(filename) && backup_old_state)
	{
		const std::string backup_filename(fmt::format("{}.backup", filename));
		Console.WriteLn(fmt::format("Creating save state backup {}...", backup_filename));
		if (!FileSystem::RenamePath(filename, backup_filename.c_str()))
		{
			Host::AddIconOSDMessage(std::move(osd_key), ICON_FA_EXCLAMATION_TRIANGLE,
				fmt::format(
					TRANSLATE_FS("VMManager", "Failed to back up old save state {}."), Path::GetFileName(filename)),
				Host::OSD_ERROR_DURATION);
		}
	}

	if (zip_on_thread)
	{
		// lock order here is important; the thread could exit before we resume here.
		std::unique_lock lock(s_save_state_threads_mutex);
		s_save_state_threads.emplace_back(&VMManager::ZipSaveStateOnThread, std::move(elist), std::move(screenshot),
			std::move(osd_key), std::string(filename), slot_for_message);
	}
	else
	{
		ZipSaveState(std::move(elist), std::move(screenshot), std::move(osd_key), filename, slot_for_message);
	}

	Host::OnSaveStateSaved(filename);
	return true;
}

void VMManager::ZipSaveState(std::unique_ptr<ArchiveEntryList> elist,
	std::unique_ptr<SaveStateScreenshotData> screenshot, std::string osd_key, const char* filename,
	s32 slot_for_message)
{
	Common::Timer timer;

	if (SaveState_ZipToDisk(std::move(elist), std::move(screenshot), filename))
	{
		if (slot_for_message >= 0 && VMManager::HasValidVM())
		{
			Host::AddIconOSDMessage(std::move(osd_key), ICON_FA_SAVE,
				fmt::format(TRANSLATE_FS("VMManager", "State saved to slot {}."), slot_for_message),
				Host::OSD_QUICK_DURATION);
		}
	}
	else
	{
		Host::AddIconOSDMessage(std::move(osd_key), ICON_FA_EXCLAMATION_TRIANGLE,
			fmt::format(TRANSLATE_FS("VMManager", "Failed to save save state to slot {}."), slot_for_message,
				Host::OSD_ERROR_DURATION));
	}

	DevCon.WriteLn("Zipping save state to '%s' took %.2f ms", filename, timer.GetTimeMilliseconds());
}

void VMManager::ZipSaveStateOnThread(std::unique_ptr<ArchiveEntryList> elist,
	std::unique_ptr<SaveStateScreenshotData> screenshot, std::string osd_key, std::string filename,
	s32 slot_for_message)
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
	if (Achievements::IsHardcoreModeActive())
	{
		Achievements::ConfirmHardcoreModeDisableAsync(TRANSLATE("VMManager", "Loading state"),
			[filename = std::string(filename)](bool approved) {
				if (approved)
					LoadState(filename.c_str());
			});
		return false;
	}

	if (MemcardBusy::IsBusy())
	{
		Host::AddIconOSDMessage("LoadStateFromSlot", ICON_FA_EXCLAMATION_TRIANGLE,
			fmt::format(TRANSLATE_FS("VMManager", "Failed to load state (Memory card is busy)")),
			Host::OSD_QUICK_DURATION);
		return false;
	}

	// TODO: Save the current state so we don't need to reset.
	if (DoLoadState(filename))
		return true;

	Reset();
	return false;
}

bool VMManager::LoadStateFromSlot(s32 slot)
{
	const std::string filename = GetCurrentSaveStateFileName(slot);
	if (filename.empty() || !FileSystem::FileExists(filename.c_str()))
	{
		Host::AddIconOSDMessage("LoadStateFromSlot", ICON_FA_EXCLAMATION_TRIANGLE,
			fmt::format(TRANSLATE_FS("VMManager", "There is no save state in slot {}."), slot),
			Host::OSD_QUICK_DURATION);
		return false;
	}

	if (Achievements::IsHardcoreModeActive())
	{
		Achievements::ConfirmHardcoreModeDisableAsync(TRANSLATE("VMManager", "Loading state"),
			[slot](bool approved) {
				if (approved)
					LoadStateFromSlot(slot);
			});
		return false;
	}

	if (MemcardBusy::IsBusy())
	{
		Host::AddIconOSDMessage("LoadStateFromSlot", ICON_FA_EXCLAMATION_TRIANGLE,
			fmt::format(TRANSLATE_FS("VMManager", "Failed to load state from slot {} (Memory card is busy)"), slot),
			Host::OSD_QUICK_DURATION);
		return false;
	}

	Host::AddIconOSDMessage("LoadStateFromSlot", ICON_FA_FOLDER_OPEN,
		fmt::format(TRANSLATE_FS("VMManager", "Loading state from slot {}..."), slot), Host::OSD_QUICK_DURATION);
	return DoLoadState(filename.c_str());
}

bool VMManager::SaveState(const char* filename, bool zip_on_thread, bool backup_old_state)
{
	if (MemcardBusy::IsBusy())
	{
		Host::AddIconOSDMessage("LoadStateFromSlot", ICON_FA_EXCLAMATION_TRIANGLE,
			fmt::format(TRANSLATE_FS("VMManager", "Failed to save state (Memory card is busy)")),
			Host::OSD_QUICK_DURATION);
		return false;
	}

	return DoSaveState(filename, -1, zip_on_thread, backup_old_state);
}

bool VMManager::SaveStateToSlot(s32 slot, bool zip_on_thread)
{
	const std::string filename(GetCurrentSaveStateFileName(slot));
	if (filename.empty())
		return false;

	if (MemcardBusy::IsBusy())
	{
		Host::AddIconOSDMessage("LoadStateFromSlot", ICON_FA_EXCLAMATION_TRIANGLE,
			fmt::format(TRANSLATE_FS("VMManager", "Failed to save state to slot {} (Memory card is busy)"), slot),
			Host::OSD_QUICK_DURATION);
		return false;
	}

	// if it takes more than a minute.. well.. wtf.
	Host::AddIconOSDMessage(fmt::format("SaveStateSlot{}", slot), ICON_FA_SAVE,
		fmt::format(TRANSLATE_FS("VMManager", "Saving state to slot {}..."), slot), 60.0f);
	return DoSaveState(filename.c_str(), slot, zip_on_thread, EmuConfig.BackupSavestate);
}

LimiterModeType VMManager::GetLimiterMode()
{
	return s_limiter_mode;
}

void VMManager::SetLimiterMode(LimiterModeType type)
{
	if (s_limiter_mode == type)
		return;

	s_limiter_mode = type;
	UpdateTargetSpeed();
}

float VMManager::GetTargetSpeed()
{
	return s_target_speed;
}

double VMManager::AdjustToHostRefreshRate(float frame_rate, float target_speed)
{
	if (!EmuConfig.EmulationSpeed.SyncToHostRefreshRate || target_speed != 1.0f)
	{
		s_target_speed_synced_to_host = false;
		s_use_vsync_for_timing = false;
		return target_speed;
	}

	float host_refresh_rate;
	if (!GSGetHostRefreshRate(&host_refresh_rate))
	{
		Console.Warning("Cannot sync to host refresh since the query failed.");
		s_target_speed_synced_to_host = false;
		s_use_vsync_for_timing = false;
		return target_speed;
	}

	const float ratio = host_refresh_rate / frame_rate;
	const bool syncing_to_host = (ratio >= 0.95f && ratio <= 1.05f);
	s_target_speed_synced_to_host = syncing_to_host;
	s_use_vsync_for_timing = (syncing_to_host && !EmuConfig.GS.SkipDuplicateFrames && EmuConfig.GS.VsyncEnable != VsyncMode::Off);
	Console.WriteLn("Refresh rate: Host=%fhz Guest=%fhz Ratio=%f - %s %s", host_refresh_rate, frame_rate, ratio,
		syncing_to_host ? "can sync" : "can't sync", s_use_vsync_for_timing ? "and using vsync for pacing" : "and using sleep for pacing");

	return syncing_to_host ? ratio : target_speed;
}

float VMManager::GetTargetSpeedForLimiterMode(LimiterModeType mode)
{
	if (EmuConfig.EnableFastBootFastForward && VMManager::Internal::IsFastBootInProgress())
		return 0.0f;

	switch (s_limiter_mode)
	{
		case LimiterModeType::Nominal:
			return EmuConfig.EmulationSpeed.NominalScalar;

		case LimiterModeType::Slomo:
			return EmuConfig.EmulationSpeed.SlomoScalar;

		case LimiterModeType::Turbo:
			return EmuConfig.EmulationSpeed.TurboScalar;

		case LimiterModeType::Unlimited:
			return 0.0f;

		default:
			ASSUME(false);
	}
}

void VMManager::UpdateTargetSpeed()
{
	const float frame_rate = GetFrameRate();
	const float target_speed = AdjustToHostRefreshRate(frame_rate, GetTargetSpeedForLimiterMode(s_limiter_mode));
	const float target_frame_rate = frame_rate * target_speed;

	s_limiter_ticks_per_frame =
		static_cast<s64>(static_cast<double>(GetTickFrequency()) / static_cast<double>(std::max(frame_rate * target_speed, 1.0f)));

	DevCon.WriteLn(fmt::format("Frame rate: {}, target speed: {}, target frame rate: {}, ticks per frame: {}", frame_rate, target_speed,
		target_frame_rate, s_limiter_ticks_per_frame));

	if (s_target_speed != target_speed)
	{
		s_target_speed = target_speed;

		MTGS::UpdateVSyncMode();
		SPU2::OnTargetSpeedChanged();
		ResetFrameLimiter();
	}
}

bool VMManager::IsTargetSpeedAdjustedToHost()
{
	return s_target_speed_synced_to_host;
}

float VMManager::GetFrameRate()
{
	return GetVerticalFrequency();
}

void VMManager::ResetFrameLimiter()
{
	s_limiter_frame_start = GetCPUTicks();
}

void VMManager::Internal::Throttle()
{
	if (s_target_speed == 0.0f || s_use_vsync_for_timing)
		return;

	const u64 uExpectedEnd =
		s_limiter_frame_start +
		s_limiter_ticks_per_frame; // Compute when we would expect this frame to end, assuming everything goes perfectly perfect.
	const u64 iEnd = GetCPUTicks(); // The current tick we actually stopped on.
	const s64 sDeltaTime = iEnd - uExpectedEnd; // The diff between when we stopped and when we expected to.

	// If frame ran too long...
	if (sDeltaTime >= s_limiter_ticks_per_frame)
	{
		// ... Fudge the next frame start over a bit. Prevents fast forward zoomies.
		s_limiter_frame_start += (sDeltaTime / s_limiter_ticks_per_frame) * s_limiter_ticks_per_frame;
		return;
	}

	// Conversion of delta from CPU ticks (microseconds) to milliseconds
	const s32 msec = static_cast<s32>((sDeltaTime * -1000) / static_cast<s64>(GetTickFrequency()));

	// If any integer value of milliseconds exists, sleep it off.
	// Prior comments suggested that 1-2 ms sleeps were inaccurate on some OSes;
	// further testing suggests instead that this was utter bullshit.
	if (msec > 1)
	{
		Threading::Sleep(msec - 1);
	}

	// Conversion to milliseconds loses some precision; after sleeping off whole milliseconds,
	// spin the thread without sleeping until we finally reach our expected end time.
	while (GetCPUTicks() < uExpectedEnd)
	{
	}

	// Finally, set our next frame start to when this one ends
	s_limiter_frame_start = uExpectedEnd;
}

void VMManager::Internal::FrameRateChanged()
{
	UpdateTargetSpeed();
}

void VMManager::FrameAdvance(u32 num_frames /*= 1*/)
{
	if (!HasValidVM())
		return;

	if (Achievements::IsHardcoreModeActive())
	{
		Achievements::ConfirmHardcoreModeDisableAsync(TRANSLATE("VMManager", "Frame advancing"),
			[num_frames](bool approved) {
				if (approved)
					FrameAdvance(num_frames);
			});

		return;
	}

	s_frame_advance_count = num_frames;
	SetState(VMState::Running);
}

bool VMManager::ChangeDisc(CDVD_SourceType source, std::string path)
{
	const CDVD_SourceType old_type = CDVDsys_GetSourceType();
	const std::string old_path(CDVDsys_GetFile(old_type));

	CDVDsys_ChangeSource(source);
	if (!path.empty())
		CDVDsys_SetFile(source, std::move(path));

	Error error;
	const bool result = DoCDVDopen(&error);
	if (result)
	{
		if (source == CDVD_SourceType::NoDisc)
		{
			Host::AddIconOSDMessage("ChangeDisc", ICON_FA_COMPACT_DISC, TRANSLATE_SV("VMManager", "Disc removed."),
				Host::OSD_INFO_DURATION);
		}
		else
		{
			Host::AddIconOSDMessage("ChangeDisc", ICON_FA_COMPACT_DISC,
				fmt::format(TRANSLATE_FS("VMManager", "Disc changed to '{}'."),
					Path::GetFileName(CDVDsys_GetFile(CDVDsys_GetSourceType()))),
				Host::OSD_INFO_DURATION);
		}
	}
	else
	{
		Host::AddIconOSDMessage("ChangeDisc", ICON_FA_COMPACT_DISC,
			fmt::format(
				TRANSLATE_FS("VMManager", "Failed to open new disc image '{}'. Reverting to old image.\nError was: {}"),
				Path::GetFileName(path), error.GetDescription()),
			Host::OSD_ERROR_DURATION);
		CDVDsys_ChangeSource(old_type);
		if (!old_path.empty())
			CDVDsys_SetFile(old_type, std::move(old_path));
		if (!DoCDVDopen(&error))
		{
			Host::AddIconOSDMessage("ChangeDisc", ICON_FA_COMPACT_DISC,
				fmt::format(TRANSLATE_FS("VMManager", "Failed to switch back to old disc image. Removing disc.\nError was: {}"),
					error.GetDescription()),
				Host::OSD_CRITICAL_ERROR_DURATION);
			CDVDsys_ChangeSource(CDVD_SourceType::NoDisc);
			DoCDVDopen(nullptr);
		}
	}
	cdvd.Tray.cdvdActionSeconds = 1;
	cdvd.Tray.trayState = CDVD_DISC_OPEN;
	UpdateDiscDetails(false);
	return result;
}

bool VMManager::SetELFOverride(std::string path)
{
	if (!HasValidVM() || (!path.empty() && !FileSystem::FileExists(path.c_str())))
		return false;

	s_elf_override = std::move(path);
	UpdateDiscDetails(false);

	s_fast_boot_requested = !s_elf_override.empty() || EmuConfig.EnableFastBoot;
	if (s_elf_override.empty())
		Hle_ClearHostRoot();
	else
		Hle_SetHostRoot(s_elf_override.c_str());

	Reset();
	return true;
}

bool VMManager::ChangeGSDump(const std::string& path)
{
	if (!HasValidVM() || !GSDumpReplayer::IsReplayingDump() || !IsGSDumpFileName(path))
		return false;

	if (!GSDumpReplayer::ChangeDump(path.c_str()))
		return false;

	UpdateDiscDetails(false);
	return true;
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
	return (StringUtil::EndsWithNoCase(path, ".gs") || StringUtil::EndsWithNoCase(path, ".gs.xz") ||
			StringUtil::EndsWithNoCase(path, ".gs.zst"));
}

bool VMManager::IsSaveStateFileName(const std::string_view& path)
{
	return StringUtil::EndsWithNoCase(path, ".p2s");
}

bool VMManager::IsDiscFileName(const std::string_view& path)
{
	static const char* extensions[] = {".iso", ".bin", ".img", ".mdf", ".gz", ".cso", ".zso", ".chd"};

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

#ifdef _WIN32
inline void LogUserPowerPlan()
{
	wil::unique_any<GUID*, decltype(&::LocalFree), ::LocalFree> pPwrGUID;
	DWORD ret = PowerGetActiveScheme(NULL, pPwrGUID.put());
	if (ret != ERROR_SUCCESS)
		return;

	UCHAR aBuffer[2048];
	DWORD aBufferSize = sizeof(aBuffer);
	ret = PowerReadFriendlyName(NULL, pPwrGUID.get(), &NO_SUBGROUP_GUID, NULL, aBuffer, &aBufferSize);
	std::string friendlyName(StringUtil::WideStringToUTF8String((wchar_t*)aBuffer));
	if (ret != ERROR_SUCCESS)
		return;

	DWORD acMax = 0, acMin = 0, dcMax = 0, dcMin = 0;

	if (PowerReadACValueIndex(NULL, pPwrGUID.get(), &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MAXIMUM, &acMax) ||
		PowerReadACValueIndex(NULL, pPwrGUID.get(), &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MINIMUM, &acMin) ||
		PowerReadDCValueIndex(NULL, pPwrGUID.get(), &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MAXIMUM, &dcMax) ||
		PowerReadDCValueIndex(NULL, pPwrGUID.get(), &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_THROTTLE_MINIMUM, &dcMin))
		return;

	Console.WriteLnFmt(
		"  Power Profile    = '{}'\n"
		"  Power States (min/max)\n"
		"    AC             = {}% / {}%\n"
		"    Battery        = {}% / {}%\n", friendlyName.c_str(), acMin, acMax, dcMin, dcMax);
}
#endif

#if 0
#if defined(__linux__) || defined(_WIN32)
void LogGPUCapabilities()
{
	Console.WriteLn(Color_StrongBlack, "Graphics Adapters Detected:");
#if defined(_WIN32)
	IDXGIFactory1* pFactory = nullptr;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory)))
		return;

	UINT i = 0;
	IDXGIAdapter* pAdapter = nullptr;
	while (pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		LARGE_INTEGER umdver;
		if (SUCCEEDED(pAdapter->GetDesc(&desc)) && SUCCEEDED(pAdapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umdver)))
		{
			Console.WriteLnFmt(
				"  GPU              = {}\n"
				"  Driver Version   = {}.{}.{}.{}\n",
				StringUtil::WideStringToUTF8String(desc.Description),
				umdver.QuadPart >> 48,
				(umdver.QuadPart >> 32) & 0xFFFF,
				(umdver.QuadPart >> 16) & 0xFFFF,
				umdver.QuadPart & 0xFFFF);

			i++;
			pAdapter->Release();
			pAdapter = nullptr;
		}
		else
		{
			pAdapter->Release();
			pAdapter = nullptr;

			break;
		}
	}

	if (pAdapter)
		pAdapter->Release();
	pFactory->Release();
#else
	// Credits to neofetch for the following (modified) script
	std::string gpu_script = R"gpu_script(
	lspci -mm |
	awk -F '\"|\" \"|\\(' \
	'/"Display|"3D|"VGA/ {
		a[$0] = $1 "" $3 " " ($(NF-1) ~ /^$|^Device [[:xdigit:]]+$/ ? $4 : $(NF-1))
	}
	END { for (i in a) {
	if (!seen[a[i]]++) {
		sub("^[^ ]+ ", "", a[i]);
		print a[i]
	}
	}}'
	)gpu_script";

	FILE* f = popen(gpu_script.c_str(), "r");
	if (f)
	{
		char buffer[1024];
		while (fgets(buffer, sizeof(buffer), f))
		{
			std::string card(buffer);
			std::string version = "Unknown\n";
			if (card.find("NVIDIA") != std::string::npos)
			{
				// Assumes that all NVIDIA cards use the same driver
				FILE* fnsmi = popen("nvidia-smi --query-gpu=driver_version --format=csv,noheader", "r");
				if (fnsmi)
				{
					if (fgets(buffer, sizeof(buffer), fnsmi))
					{
						version = std::string(buffer);
					}
					pclose(fnsmi);
				}
			}
			else
			{
				// Assuming non-NVIDIA cards are using the mesa driver
				FILE* fglxinfo = popen("glxinfo | sed -n 's/OpenGL version string:/OGL/p'", "r");
				if (fglxinfo)
				{
					if (fgets(buffer, sizeof(buffer), fglxinfo))
					{
						version = std::string(buffer);

						// This path is taken if the card was Intel or AMD
						// If glxinfo is reporting NVIDIA, then it's likely that this is a multi-gpu system
						// and that this version doesn't apply to this AMD / Intel device
						// Alternatively we could use vulkaninfo, which allows us to select the device
						// But I was unable to get that to work on my system
						// So for now, we cannot get the iGPU version on NVIDIA dGPU systems
						if (version.find("NVIDIA") != std::string::npos)
						{
							version = "Unknown (OpenGL default is NVIDIA)\n";
						}
					}
					pclose(fglxinfo);
				}
			}
			Console.WriteLnFmt(
				"  GPU              = {}"
				"  Driver  Version  = {}",
				card, version);
		}
		pclose(f);
	}
#endif
}
#endif
#endif

void VMManager::LogCPUCapabilities()
{
	Console.WriteLn(Color_StrongGreen, "PCSX2 " GIT_REV);
	Console.WriteLnFmt("Savestate version: 0x{:x}\n", g_SaveVersion);
	Console.WriteLn();

	Console.WriteLn(Color_StrongBlack, "Host Machine Init:");

	Console.WriteLnFmt(
		"  Operating System = {}\n"
		"  Physical RAM     = {} MB",
		GetOSVersionString(),
		GetPhysicalMemory() / _1mb);

	Console.WriteLnFmt("  Processor        = {}", cpuinfo_get_package(0)->name);
	Console.WriteLnFmt("  Core Count       = {} cores", cpuinfo_get_cores_count());
	Console.WriteLnFmt("  Thread Count     = {} threads", cpuinfo_get_processors_count());
#ifdef _WIN32
	LogUserPowerPlan();
#endif

	std::string features;
	if (cpuinfo_has_x86_avx())
		features += "AVX ";
	if (cpuinfo_has_x86_avx2())
		features += "AVX2 ";

	StringUtil::StripWhitespace(&features);

	Console.WriteLn(Color_StrongBlack, "x86 Features Detected:");
	Console.WriteLnFmt("  {}", features);
	Console.WriteLn();

#if 0
	LogGPUCapabilities();
#endif
}


void VMManager::InitializeCPUProviders()
{
	recCpu.Reserve();
	psxRec.Reserve();

	CpuMicroVU0.Reserve();
	CpuMicroVU1.Reserve();

	VifUnpackSSE_Init();
}

void VMManager::ShutdownCPUProviders()
{
	if (newVifDynaRec)
	{
		dVifRelease(1);
		dVifRelease(0);
	}

	CpuMicroVU1.Shutdown();
	CpuMicroVU0.Shutdown();

	psxRec.Shutdown();
	recCpu.Shutdown();
}

void VMManager::UpdateCPUImplementations()
{
	if (GSDumpReplayer::IsReplayingDump())
	{
		Cpu = &GSDumpReplayerCpu;
		psxCpu = &psxInt;
		CpuVU0 = &CpuIntVU0;
		CpuVU1 = &CpuIntVU1;
		return;
	}

	Cpu = CHECK_EEREC ? &recCpu : &intCpu;
	psxCpu = CHECK_IOPREC ? &psxRec : &psxInt;

	CpuVU0 = &CpuIntVU0;
	CpuVU1 = &CpuIntVU1;

	if (EmuConfig.Cpu.Recompiler.EnableVU0)
		CpuVU0 = &CpuMicroVU0;

	if (EmuConfig.Cpu.Recompiler.EnableVU1)
		CpuVU1 = &CpuMicroVU1;
}

void VMManager::Internal::ClearCPUExecutionCaches()
{
	Cpu->Reset();
	psxCpu->Reset();

	// mVU's VU0 needs to be properly initialized for macro mode even if it's not used for micro mode!
	if (CHECK_EEREC && !EmuConfig.Cpu.Recompiler.EnableVU0)
		CpuMicroVU0.Reset();

	CpuVU0->Reset();
	CpuVU1->Reset();

	if constexpr (newVifDynaRec)
	{
		dVifReset(0);
		dVifReset(1);
	}
}

void VMManager::Execute()
{
	// Check for interpreter<->recompiler switches.
	if (std::exchange(s_cpu_implementation_changed, false))
	{
		// We need to switch the cpus out, and reset the new ones if so.
		UpdateCPUImplementations();
		Internal::ClearCPUExecutionCaches();
		vtlb_ResetFastmem();
	}

	// Execute until we're asked to stop.
	Cpu->Execute();
}

void VMManager::IdlePollUpdate()
{
	Achievements::IdleUpdate();

	PollDiscordPresence();

	InputManager::PollSources();
}

void VMManager::SetPaused(bool paused)
{
	if (!HasValidVM())
		return;

	Console.WriteLn(paused ? "(VMManager) Pausing..." : "(VMManager) Resuming...");
	SetState(paused ? VMState::Paused : VMState::Running);
}

VsyncMode Host::GetEffectiveVSyncMode()
{
	const bool has_vm = VMManager::GetState() != VMState::Shutdown;

	// Force vsync off when not running at 100% speed.
	if (has_vm && (s_target_speed != 1.0f && !s_use_vsync_for_timing))
		return VsyncMode::Off;

	// Otherwise use the config setting.
	return EmuConfig.GS.VsyncEnable;
}

bool VMManager::Internal::IsFastBootInProgress()
{
	return s_fast_boot_requested && !HasBootedELF();
}

void VMManager::Internal::DisableFastBoot()
{
	if (!s_fast_boot_requested)
		return;

	s_fast_boot_requested = false;

	// Stop fast forwarding boot if enabled.
	if (EmuConfig.EnableFastBootFastForward && !s_elf_executed)
		UpdateTargetSpeed();
}

bool VMManager::Internal::HasBootedELF()
{
	return VMManager::HasBootedELF();
}

u32 VMManager::Internal::GetCurrentELFEntryPoint()
{
	return s_elf_entry_point;
}

const std::string& VMManager::Internal::GetELFOverride()
{
	return s_elf_override;
}

bool VMManager::Internal::IsExecutionInterrupted()
{
	return s_state.load(std::memory_order_relaxed) != VMState::Running || s_cpu_implementation_changed;
}

void VMManager::Internal::ELFLoadingOnCPUThread(std::string elf_path)
{
	const bool was_running_bios = (s_current_crc == 0);

	UpdateELFInfo(std::move(elf_path));
	Console.WriteLn(Color_StrongBlue, fmt::format("ELF Loading: {}, Game CRC = {:08X}, EntryPoint = 0x{:08X}",
										  s_elf_path, s_current_crc, s_elf_entry_point));
	s_elf_executed = false;

	// Remove patches, if we're changing games, we don't want to be applying the patch for the old game while it's loading.
	if (!was_running_bios)
	{
		Patch::ReloadPatches(s_disc_serial, 0, false, false, false, true);
		ApplyCoreSettings();
	}
}

void VMManager::Internal::EntryPointCompilingOnCPUThread()
{
	if (s_elf_executed)
		return;

	const bool reset_speed_limiter = (EmuConfig.EnableFastBootFastForward && IsFastBootInProgress());

	Console.WriteLn(
		Color_StrongGreen, fmt::format("ELF {} with entry point at 0x{:08X} is executing.", s_elf_path, s_elf_entry_point));
	s_elf_executed = true;

	if (reset_speed_limiter)
	{
		UpdateTargetSpeed();
		PerformanceMetrics::Reset();
	}

	HandleELFChange(true);

	Patch::ApplyLoadedPatches(Patch::PPT_ONCE_ON_LOAD);
	// If the config changes at this point, it's a reset, so the game doesn't currently know about the memcard
	// so there's no need to leave the eject running.
	FileMcd_CancelEject();

	// Toss all the recs, we're going to be executing new code.
	mmap_ResetBlockTracking();
	ClearCPUExecutionCaches();
}

void VMManager::Internal::VSyncOnCPUThread()
{
	Pad::UpdateMacroButtons();

	Patch::ApplyLoadedPatches(Patch::PPT_CONTINUOUSLY);
	Patch::ApplyLoadedPatches(Patch::PPT_COMBINED_0_1);

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

	Achievements::FrameUpdate();

	PollDiscordPresence();
}

void VMManager::Internal::PollInputOnCPUThread()
{
	Host::PumpMessagesOnCPUThread();
	InputManager::PollSources();

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
	if (EmuConfig.Cpu == old_config.Cpu && EmuConfig.Gamefixes == old_config.Gamefixes &&
		EmuConfig.Speedhacks == old_config.Speedhacks && EmuConfig.Profiler == old_config.Profiler)
	{
		return;
	}

	Console.WriteLn("Updating CPU configuration...");
	FPControlRegister::SetCurrent(EmuConfig.Cpu.FPUFPCR);
	Internal::ClearCPUExecutionCaches();
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

	// We could just check whichever NTSC or PAL is appropriate for our current mode,
	// but people _really_ shouldn't be screwing with framerate, so whatever.
	if (EmuConfig.GS.FramerateNTSC != old_config.GS.FramerateNTSC ||
		EmuConfig.GS.FrameratePAL != old_config.GS.FrameratePAL)
	{
		UpdateVSyncRate(false);
		UpdateTargetSpeed();
	}
	else if (EmuConfig.GS.VsyncEnable != old_config.GS.VsyncEnable)
	{
		// Still need to update target speed, because of sync-to-host-refresh.
		UpdateTargetSpeed();
	}

	MTGS::ApplySettings();
}

void VMManager::CheckForEmulationSpeedConfigChanges(const Pcsx2Config& old_config)
{
	if (EmuConfig.EmulationSpeed == old_config.EmulationSpeed)
		return;

	Console.WriteLn("Updating emulation speed configuration");
	UpdateTargetSpeed();
}

void VMManager::CheckForPatchConfigChanges(const Pcsx2Config& old_config)
{
	if (EmuConfig.EnableCheats == old_config.EnableCheats &&
		EmuConfig.EnableWideScreenPatches == old_config.EnableWideScreenPatches &&
		EmuConfig.EnableNoInterlacingPatches == old_config.EnableNoInterlacingPatches &&
		EmuConfig.EnablePatches == old_config.EnablePatches)
	{
		return;
	}

	Patch::UpdateActivePatches(true, false, true);

	// This is a bit messy, because the patch config update happens after the settings are loaded,
	// if we disable widescreen patches, we have to reload the original settings again.
	if (Patch::ReloadPatchAffectingOptions())
		MTGS::ApplySettings();
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

	changed |= (EmuConfig.McdFolderAutoManage != old_config.McdFolderAutoManage);

	if (!changed)
		return;

	Console.WriteLn("Updating memory card configuration");

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
		if (const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(s_disc_serial))
			sioSerial = game->memcardFiltersAsString();
		if (sioSerial.empty())
			sioSerial = s_disc_serial;
	}

	if (!GSDumpReplayer::IsReplayingDump())
		FileMcd_Reopen(sioSerial);
}

void VMManager::CheckForMiscConfigChanges(const Pcsx2Config& old_config)
{
	if (EmuConfig.EnableFastBootFastForward && !old_config.EnableFastBootFastForward &&
		VMManager::Internal::IsFastBootInProgress())
	{
		UpdateTargetSpeed();
	}

	if (EmuConfig.InhibitScreensaver != old_config.InhibitScreensaver)
		UpdateInhibitScreensaver(EmuConfig.InhibitScreensaver && VMManager::GetState() == VMState::Running);

	if (EmuConfig.EnableDiscordPresence != old_config.EnableDiscordPresence)
	{
		if (EmuConfig.EnableDiscordPresence)
			InitializeDiscordPresence();
		else
			ShutdownDiscordPresence();
	}
}

void VMManager::CheckForConfigChanges(const Pcsx2Config& old_config)
{
	if (HasValidVM())
	{
		CheckForCPUConfigChanges(old_config);
		CheckForEmulationSpeedConfigChanges(old_config);
		CheckForPatchConfigChanges(old_config);
		SPU2::CheckForConfigChanges(old_config);
		CheckForDEV9ConfigChanges(old_config);
		CheckForMemoryCardConfigChanges(old_config);
		USB::CheckForConfigChanges(old_config);
	}

	// For the big picture UI, we still need to update GS settings, since it's running,
	// and we don't update its config when we start the VM.
	if (HasValidVM() || MTGS::IsOpen())
		CheckForGSConfigChanges(old_config);

	if (EmuConfig.Achievements != old_config.Achievements)
		Achievements::UpdateSettings(old_config.Achievements);

	FullscreenUI::CheckForConfigChanges(old_config);

	CheckForMiscConfigChanges(old_config);

	Host::CheckForSettingsChanges(old_config);
}

void VMManager::ReloadPatches(bool reload_files, bool reload_enabled_list, bool verbose, bool verbose_if_changed)
{
	if (!HasValidVM())
		return;

	Patch::ReloadPatches(s_disc_serial, HasBootedELF() ? s_current_crc : 0, reload_files, reload_enabled_list, verbose, verbose_if_changed);

	// Might change widescreen mode.
	if (Patch::ReloadPatchAffectingOptions())
		ApplyCoreSettings();
}

void VMManager::EnforceAchievementsChallengeModeSettings()
{
	if (!Achievements::IsHardcoreModeActive())
	{
		Host::RemoveKeyedOSDMessage("ChallengeDisableCheats");
		return;
	}

	static constexpr auto ClampSpeed = [](float& rate) {
		if (rate > 0.0f && rate < 1.0f)
			rate = 1.0f;
	};

	// Can't use slow motion.
	ClampSpeed(EmuConfig.EmulationSpeed.NominalScalar);
	ClampSpeed(EmuConfig.EmulationSpeed.TurboScalar);
	ClampSpeed(EmuConfig.EmulationSpeed.SlomoScalar);

	// Can't use cheats.
	if (EmuConfig.EnableCheats)
	{
		Host::AddKeyedOSDMessage("ChallengeDisableCheats",
			TRANSLATE_STR("VMManager", "Cheats have been disabled due to achievements hardcore mode."),
			Host::OSD_WARNING_DURATION);
		EmuConfig.EnableCheats = false;
	}

	// Input recording/playback is probably an issue.
	EmuConfig.EnableRecordingTools = false;
	EmuConfig.EnablePINE = false;

	// Framerates should be at default.
	EmuConfig.GS.FramerateNTSC = Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_NTSC;
	EmuConfig.GS.FrameratePAL = Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_PAL;

	// You can overclock, but not underclock (since that might slow down the game and make it easier).
	EmuConfig.Speedhacks.EECycleRate =
		std::max<decltype(EmuConfig.Speedhacks.EECycleRate)>(EmuConfig.Speedhacks.EECycleRate, 0);
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
	if (!EmuConfig.WarnAboutUnsafeSettings)
		return;

	std::string messages;
	auto append = [&messages](const char* icon, const std::string_view& msg) {
		messages += icon;
		messages += ' ';
		messages += msg;
		messages += '\n';
	};

	if (EmuConfig.Speedhacks.fastCDVD)
		append(ICON_FA_COMPACT_DISC, TRANSLATE_SV("VMManager", "Fast CDVD is enabled, this may break games."));
	if (EmuConfig.Speedhacks.EECycleRate != 0 || EmuConfig.Speedhacks.EECycleSkip != 0)
	{
		append(ICON_FA_TACHOMETER_ALT,
			TRANSLATE_SV("VMManager", "Cycle rate/skip is not at default, this may crash or make games run too slow."));
	}
	if (EmuConfig.GS.UpscaleMultiplier < 1.0f)
		append(ICON_FA_TV, TRANSLATE_SV("VMManager", "Upscale multiplier is below native, this will break rendering."));
	if (EmuConfig.GS.HWMipmap != HWMipmapLevel::Automatic)
	{
		append(ICON_FA_IMAGES,
			TRANSLATE_SV("VMManager", "Mipmapping is not set to automatic. This may break rendering in some games."));
	}
	if (EmuConfig.GS.TextureFiltering != BiFiltering::PS2)
	{
		append(ICON_FA_FILTER,
			TRANSLATE_SV("VMManager",
				"Texture filtering is not set to Bilinear (PS2). This will break rendering in some games."));
	}
	if (EmuConfig.GS.TriFilter != TriFiltering::Automatic)
	{
		append(
			ICON_FA_PAGER, TRANSLATE_SV("VMManager",
							   "Trilinear filtering is not set to automatic. This may break rendering in some games."));
	}
	if (EmuConfig.GS.AccurateBlendingUnit <= AccBlendLevel::Minimum)
	{
		append(ICON_FA_PAINT_BRUSH,
			TRANSLATE_SV("VMManager", "Blending Accuracy is below Basic, this may break effects in some games."));
	}
	if (EmuConfig.GS.HWDownloadMode != GSHardwareDownloadMode::Enabled)
	{
		append(ICON_FA_DOWNLOAD,
			TRANSLATE_SV(
				"VMManager", "Hardware Download Mode is not set to Accurate, this may break rendering in some games."));
	}
	if (EmuConfig.Cpu.FPUFPCR.GetRoundMode() != FPRoundMode::ChopZero)
	{
		append(ICON_PF_MICROCHIP,
			TRANSLATE_SV("VMManager", "EE FPU Round Mode is not set to default, this may break some games."));
	}
	if (!EmuConfig.Cpu.Recompiler.fpuOverflow || EmuConfig.Cpu.Recompiler.fpuExtraOverflow ||
		EmuConfig.Cpu.Recompiler.fpuFullMode)
	{
		append(ICON_PF_MICROCHIP,
			TRANSLATE_SV("VMManager", "EE FPU Clamp Mode is not set to default, this may break some games."));
	}
	if (EmuConfig.Cpu.VU0FPCR.GetRoundMode() != FPRoundMode::ChopZero)
	{
		append(ICON_PF_MICROCHIP,
			TRANSLATE_SV("VMManager", "VU0 Round Mode is not set to default, this may break some games."));
	}
	if (EmuConfig.Cpu.VU1FPCR.GetRoundMode() != FPRoundMode::ChopZero)
	{
		append(ICON_PF_MICROCHIP,
			TRANSLATE_SV("VMManager", "VU1 Round Mode is not set to default, this may break some games."));
	}
	if (!EmuConfig.Cpu.Recompiler.vu0Overflow || EmuConfig.Cpu.Recompiler.vu0ExtraOverflow ||
		EmuConfig.Cpu.Recompiler.vu0SignOverflow || !EmuConfig.Cpu.Recompiler.vu1Overflow ||
		EmuConfig.Cpu.Recompiler.vu1ExtraOverflow || EmuConfig.Cpu.Recompiler.vu1SignOverflow)
	{
		append(ICON_PF_MICROCHIP,
			TRANSLATE_SV("VMManager", "VU Clamp Mode is not set to default, this may break some games."));
	}
	if (EmuConfig.Cpu.ExtraMemory)
	{
		append(ICON_PF_MICROCHIP,
			TRANSLATE_SV("VMManager", "128MB RAM is enabled. Compatibility with some games may be affected."));
	}
	if (!EmuConfig.EnableGameFixes)
	{
		append(ICON_FA_GAMEPAD,
			TRANSLATE_SV("VMManager", "Game Fixes are not enabled. Compatibility with some games may be affected."));
	}
	if (!EmuConfig.EnablePatches)
	{
		append(ICON_FA_GAMEPAD,
			TRANSLATE_SV(
				"VMManager", "Compatibility Patches are not enabled. Compatibility with some games may be affected."));
	}
	if (EmuConfig.GS.FramerateNTSC != Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_NTSC)
		append(ICON_FA_TV, TRANSLATE_SV("VMManager", "Frame rate for NTSC is not default. This may break some games."));
	if (EmuConfig.GS.FrameratePAL != Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_PAL)
		append(ICON_FA_TV, TRANSLATE_SV("VMManager", "Frame rate for PAL is not default. This may break some games."));

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
	{
		append(ICON_FA_EXCLAMATION_CIRCLE,
			TRANSLATE_SV("VMManager", "EE Recompiler is not enabled, this will significantly reduce performance."));
	}
	if (!EmuConfig.Cpu.Recompiler.EnableVU0)
	{
		append(ICON_FA_EXCLAMATION_CIRCLE,
			TRANSLATE_SV("VMManager", "VU0 Recompiler is not enabled, this will significantly reduce performance."));
	}
	if (!EmuConfig.Cpu.Recompiler.EnableVU1)
	{
		append(ICON_FA_EXCLAMATION_CIRCLE,
			TRANSLATE_SV("VMManager", "VU1 Recompiler is not enabled, this will significantly reduce performance."));
	}
	if (!EmuConfig.Cpu.Recompiler.EnableIOP)
	{
		append(ICON_FA_EXCLAMATION_CIRCLE,
			TRANSLATE_SV("VMManager", "IOP Recompiler is not enabled, this will significantly reduce performance."));
	}
	if (EmuConfig.Cpu.Recompiler.EnableEECache)
	{
		append(ICON_FA_EXCLAMATION_CIRCLE,
			TRANSLATE_SV("VMManager", "EE Cache is enabled, this will significantly reduce performance."));
	}
	if (!EmuConfig.Speedhacks.WaitLoop)
	{
		append(ICON_FA_EXCLAMATION_CIRCLE,
			TRANSLATE_SV("VMManager", "EE Wait Loop Detection is not enabled, this may reduce performance."));
	}
	if (!EmuConfig.Speedhacks.IntcStat)
	{
		append(ICON_FA_EXCLAMATION_CIRCLE,
			TRANSLATE_SV("VMManager", "INTC Spin Detection is not enabled, this may reduce performance."));
	}
	if (!EmuConfig.Speedhacks.vu1Instant)
	{
		append(ICON_FA_EXCLAMATION_CIRCLE,
			TRANSLATE_SV("VMManager", "Instant VU1 is disabled, this may reduce performance."));
	}
	if (!EmuConfig.Speedhacks.vuFlagHack)
	{
		append(ICON_FA_EXCLAMATION_CIRCLE,
			TRANSLATE_SV("VMManager", "mVU Flag Hack is not enabled, this may reduce performance."));
	}
	if (EmuConfig.GS.GPUPaletteConversion)
	{
		append(ICON_FA_EXCLAMATION_CIRCLE,
			TRANSLATE_SV("VMManager", "GPU Palette Conversion is enabled, this may reduce performance."));
	}
	if (EmuConfig.GS.TexturePreloading != TexturePreloadingLevel::Full)
	{
		append(ICON_FA_EXCLAMATION_CIRCLE,
			TRANSLATE_SV("VMManager", "Texture Preloading is not Full, this may reduce performance."));
	}
	if (EmuConfig.GS.UserHacks_EstimateTextureRegion)
	{
		append(ICON_FA_EXCLAMATION_CIRCLE,
			TRANSLATE_SV("VMManager", "Estimate texture region is enabled, this may reduce performance."));
	}
	if (EmuConfig.GS.DumpReplaceableTextures)
	{
		append(ICON_FA_EXCLAMATION_CIRCLE,
			TRANSLATE_SV("VMManager", "Texture dumping is enabled, this will continually dump textures to disk."));
	}

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

void VMManager::UpdateInhibitScreensaver(bool inhibit)
{
	if (s_screensaver_inhibited == inhibit)
		return;

	WindowInfo wi;
	auto top_level_wi = Host::GetTopLevelWindowInfo();
	if (top_level_wi.has_value())
		wi = top_level_wi.value();

	s_screensaver_inhibited = inhibit;
	if (!WindowInfo::InhibitScreensaver(wi, inhibit) && inhibit)
		Console.Warning("Failed to inhibit screen saver.");
}

void VMManager::SaveSessionTime(const std::string& prev_serial)
{
	// Don't save time when running dumps, just messes up your list.
	if (GSDumpReplayer::IsReplayingDump())
		return;

	if (!prev_serial.empty())
	{
		// round up to seconds
		const std::time_t etime =
			static_cast<std::time_t>(std::round(Common::Timer::ConvertValueToSeconds(std::exchange(s_session_accumulated_playtime, 0))));
		const std::time_t wtime = std::time(nullptr);
		GameList::AddPlayedTimeForSerial(prev_serial, wtime, etime);
	}
}

u64 VMManager::GetSessionPlayedTime()
{
	return static_cast<u64>(std::round(Common::Timer::ConvertValueToSeconds(s_session_accumulated_playtime)));
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

static void InitializeProcessorList()
{
	const u32 cluster_count = cpuinfo_get_clusters_count();
	if (cluster_count == 0)
	{
		Console.Error("Invalid CPU count returned");
		return;
	}

	Console.WriteLn(Color_StrongYellow, "Processor count: %u cores, %u processors", cpuinfo_get_cores_count(),
		cpuinfo_get_processors_count());
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
	std::sort(ordered_processors.begin(), ordered_processors.end(),
		[](const cpuinfo_processor* lhs, const cpuinfo_processor* rhs) {
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

	const u32 core_count = cpuinfo_get_cores_count();
	if (core_count == 0)
	{
		Console.Error("Invalid CPU count returned");
		return;
	}

	Console.WriteLn(fmt::format("CPU cores count: {}", core_count));

	if (core_count >= 3)
	{
		Console.WriteLn("  Enabling MTVU.");
		si.SetBoolValue("EmuCore/Speedhacks", "vuThread", true);
	}
	else
	{
		Console.WriteLn("  Disabling MTVU.");
		si.SetBoolValue("EmuCore/Speedhacks", "vuThread", false);
	}

	const int extra_threads = (core_count > 3) ? 3 : 2;
	Console.WriteLn(fmt::format("  Setting Extra Software Rendering Threads to {}.", extra_threads));
	si.SetIntValue("EmuCore/GS", "extrathreads", extra_threads);
}

#elif defined(__APPLE__)

static u32 s_big_cores;
static u32 s_small_cores;

static void InitializeProcessorList()
{
	s_big_cores = 0;
	s_small_cores = 0;
	std::vector<DarwinMisc::CPUClass> classes = DarwinMisc::GetCPUClasses();
	for (size_t i = 0; i < classes.size(); i++)
	{
		const DarwinMisc::CPUClass& cls = classes[i];
		const bool is_big = i == 0 || i < classes.size() - 1; // Assume only one group is small
		DevCon.WriteLn("(VMManager) Found %u physical cores and %u logical cores in perf level %u (%s), assuming %s",
			cls.num_physical, cls.num_logical, i, cls.name.c_str(), is_big ? "big" : "small");
		(is_big ? s_big_cores : s_small_cores) += cls.num_physical;
	}
}

static void SetMTVUAndAffinityControlDefault(SettingsInterface& si)
{
	VMManager::EnsureCPUInfoInitialized();

	Console.WriteLn("Detected %u big cores", s_big_cores);

	if (s_big_cores >= 3)
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

static void InitializeProcessorList()
{
	DevCon.WriteLn("(VMManager) InitializeCPUInfo() not implemented.");
}

static void SetMTVUAndAffinityControlDefault(SettingsInterface& si)
{
}

#endif

void VMManager::EnsureCPUInfoInitialized()
{
	std::call_once(s_processor_list_initialized, InitializeProcessorList);
}

void VMManager::SetEmuThreadAffinities()
{
	EnsureCPUInfoInitialized();

	if (s_processor_list.empty())
	{
		// not supported on this platform
		return;
	}

	if (EmuConfig.Cpu.AffinityControlMode == 0 || s_processor_list.size() < (EmuConfig.Speedhacks.vuThread ? 3 : 2))
	{
		if (EmuConfig.Cpu.AffinityControlMode != 0)
			Console.Error("Insufficient processors for affinity control.");

		MTGS::GetThreadHandle().SetAffinity(0);
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
	const u8* this_proc_assigment =
		processor_assignment[EmuConfig.Cpu.AffinityControlMode][EmuConfig.Speedhacks.vuThread];
	const u32 ee_index = s_processor_list[this_proc_assigment[0]];
	const u32 vu_index = s_processor_list[this_proc_assigment[1]];
	const u32 gs_index = s_processor_list[this_proc_assigment[2]];
	Console.WriteLn("Processor order assignment: EE=%u, VU=%u, GS=%u", this_proc_assigment[0], this_proc_assigment[1],
		this_proc_assigment[2]);

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
	MTGS::GetThreadHandle().SetAffinity(gs_affinity);
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

void VMManager::ReloadPINE()
{
	const bool needs_reinit = (EmuConfig.EnablePINE != PINEServer::IsInitialized() ||
							   PINEServer::GetSlot() != EmuConfig.PINESlot);
	if (!needs_reinit)
		return;

	PINEServer::Deinitialize();

	if (EmuConfig.EnablePINE)
		PINEServer::Initialize();
}

void VMManager::InitializeDiscordPresence()
{
	if (s_discord_presence_active)
		return;

	DiscordEventHandlers handlers = {};
	Discord_Initialize("1025789002055430154", &handlers, 0, nullptr);
	s_discord_presence_active = true;

	UpdateDiscordPresence(true);
}

void VMManager::ShutdownDiscordPresence()
{
	if (!s_discord_presence_active)
		return;

	Discord_ClearPresence();
	Discord_RunCallbacks();
	Discord_Shutdown();
	s_discord_presence_active = false;
}

void VMManager::UpdateDiscordPresence(bool update_session_time)
{
	if (!s_discord_presence_active)
		return;

	if (update_session_time)
		s_discord_presence_time_epoch = std::time(nullptr);

	// https://discord.com/developers/docs/rich-presence/how-to#updating-presence-update-presence-payload-fields
	DiscordRichPresence rp = {};
	rp.largeImageKey = "4k-pcsx2";
	rp.largeImageText = "PCSX2 Emulator";
	rp.startTimestamp = s_discord_presence_time_epoch;
	rp.details = s_title.empty() ? "No Game Running" : s_title.c_str();

	std::string state_string;
	if (Achievements::HasRichPresence())
	{
		state_string = StringUtil::Ellipsise(Achievements::GetRichPresenceString(), 128);
		rp.state = state_string.c_str();
	}

	Discord_UpdatePresence(&rp);
	Discord_RunCallbacks();
}

void VMManager::PollDiscordPresence()
{
	if (!s_discord_presence_active)
		return;

	Discord_RunCallbacks();
}
