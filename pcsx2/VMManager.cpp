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
#include <mutex>
#include <wx/mstream.h>

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"
#include "common/SettingsWrapper.h"
#include "common/Timer.h"
#include "fmt/core.h"

#include "Counters.h"
#include "CDVD/CDVD.h"
#include "DEV9/DEV9.h"
#include "Elfheader.h"
#include "FW.h"
#include "GS.h"
#include "GSDumpReplayer.h"
#include "HostDisplay.h"
#include "HostSettings.h"
#include "IopBios.h"
#include "MTVU.h"
#include "MemoryCardFile.h"
#include "Patch.h"
#include "PerformanceMetrics.h"
#include "R5900.h"
#include "SPU2/spu2.h"
#include "DEV9/DEV9.h"
#include "System/SysThreads.h"
#include "USB/USB.h"
#include "PAD/Host/PAD.h"
#include "Sio.h"
#include "ps2/BiosTools.h"

#include "DebugTools/MIPSAnalyst.h"
#include "DebugTools/SymbolMap.h"

#include "Frontend/INISettingsInterface.h"
#include "Frontend/InputManager.h"

#include "common/emitter/tools.h"
#ifdef _M_X86
#include "common/emitter/x86_intrin.h"
#endif

namespace VMManager
{
	static void LoadSettings();
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

	static bool AutoDetectSource(const std::string& filename);
	static bool ApplyBootParameters(const VMBootParameters& params);
	static bool CheckBIOSAvailability();
	static void UpdateRunningGame(bool force);

	static std::string GetCurrentSaveStateFileName(s32 slot);
	static bool DoLoadState(const char* filename);
	static bool DoSaveState(const char* filename, s32 slot_for_message);

	static void SetTimerResolutionIncreased(bool enabled);
	static void SetEmuThreadAffinities(bool force);
} // namespace VMManager

static std::unique_ptr<SysMainMemory> s_vm_memory;
static std::unique_ptr<SysCpuProviderPack> s_cpu_provider_pack;
static std::unique_ptr<INISettingsInterface> s_game_settings_interface;
static u64 s_emu_thread_affinity;

static std::atomic<VMState> s_state{VMState::Shutdown};
static std::atomic_bool s_cpu_implementation_changed{false};

static std::mutex s_info_mutex;
static std::string s_disc_path;
static u32 s_game_crc;
static std::string s_game_serial;
static std::string s_game_name;
static std::string s_elf_override;
static u32 s_active_game_fixes = 0;
static std::vector<u8> s_widescreen_cheats_data;
static bool s_widescreen_cheats_loaded = false;
static s32 s_current_save_slot = 1;
static u32 s_frame_advance_count = 0;
static u32 s_mxcsr_saved;

VMState VMManager::GetState()
{
	return s_state.load();
}

void VMManager::SetState(VMState state)
{
	// Some state transitions aren't valid.
	const VMState old_state = s_state.load();
	pxAssert(state != VMState::Initializing && state != VMState::Shutdown);
	SetTimerResolutionIncreased(state == VMState::Running);
	s_state.store(state);

	if (state != VMState::Stopping && (state == VMState::Paused || old_state == VMState::Paused))
	{
		if (state == VMState::Paused)
		{
			if (THREAD_VU1)
				vu1Thread.WaitVU();
			GetMTGS().WaitGS(false);
			InputManager::PauseVibration();
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
	const VMState state = s_state.load();
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
	x86caps.Identify();
	x86caps.CountCores();
	x86caps.SIMD_EstablishMXCSRmask();
	x86caps.CalculateMHz();
	SysLogMachineCaps();

	return true;
}

bool VMManager::Internal::InitializeMemory()
{
	pxAssert(!s_vm_memory && !s_cpu_provider_pack);

	s_vm_memory = std::make_unique<SysMainMemory>();
	s_cpu_provider_pack = std::make_unique<SysCpuProviderPack>();

	s_vm_memory->ReserveAll();
	return true;
}

void VMManager::Internal::ReleaseMemory()
{
	std::vector<u8>().swap(s_widescreen_cheats_data);
	s_widescreen_cheats_loaded = false;

	s_vm_memory->DecommitAll();
	s_vm_memory->ReleaseAll();
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
	auto lock = Host::GetSettingsLock();
	SettingsInterface* si = Host::GetSettingsInterface();
	SettingsLoadWrapper slw(*si);
	EmuConfig.LoadSave(slw);
	PAD::LoadConfig(*si);
	InputManager::ReloadSources(*si);
	InputManager::ReloadBindings(*si);

	// Remove any user-specified hacks in the config (we don't want stale/conflicting values when it's globally disabled).
	EmuConfig.GS.MaskUserHacks();
	EmuConfig.GS.MaskUpscalingHacks();

	// Force MTVU off when playing back GS dumps, it doesn't get used.
	if (GSDumpReplayer::IsReplayingDump())
		EmuConfig.Speedhacks.vuThread = false;

	if (HasValidVM())
		ApplyGameFixes();
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

std::string VMManager::GetGameSettingsPath(u32 game_crc)
{
	return Path::CombineStdString(EmuFolders::GameSettings, StringUtil::StdStringFromFormat("%08X.ini", game_crc));
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
		scale = (1.0f / static_cast<float>(GSConfig.UpscaleMultiplier)) * scale;
		width *= scale;
		height *= scale;
	}

	iwidth = std::max(static_cast<int>(std::lroundf(width)), 1);
	iheight = std::max(static_cast<int>(std::lroundf(height)), 1);

	Host::RequestResizeHostDisplay(iwidth, iheight);
}

bool VMManager::UpdateGameSettingsLayer()
{
	std::unique_ptr<INISettingsInterface> new_interface;
	if (s_game_crc != 0)
	{
		const std::string filename(GetGameSettingsPath(s_game_crc));
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

	if (!s_game_settings_interface && !new_interface)
		return false;

	Host::Internal::SetGameSettingsLayer(new_interface.get());
	s_game_settings_interface = std::move(new_interface);
	return true;
}

static void LoadPatches(const std::string& crc_string, bool show_messages, bool show_messages_when_disabled)
{
	FastFormatAscii message;

	int patch_count = 0;
	if (EmuConfig.EnablePatches)
	{
		const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(s_game_serial);
		if (game && (patch_count = LoadPatchesFromGamesDB(crc_string, *game)) > 0)
		{
			PatchesCon->WriteLn(Color_Green, "(GameDB) Patches Loaded: %d", patch_count);
			message.Write("%u game patches", patch_count);
		}
	}

	// regular cheat patches
	int cheat_count = 0;
	if (EmuConfig.EnableCheats)
	{
		cheat_count = LoadPatchesFromDir(StringUtil::UTF8StringToWxString(crc_string), EmuFolders::Cheats, L"Cheats");
		if (cheat_count > 0)
		{
			PatchesCon->WriteLn(Color_Green, "Cheats Loaded: %d", cheat_count);
			message.Write("%s%u cheat patches", (patch_count > 0) ? " and " : "", cheat_count);
		}
	}

	// wide screen patches
	int ws_patch_count = 0;
	if (EmuConfig.EnableWideScreenPatches && s_game_crc != 0)
	{
		if (ws_patch_count = LoadPatchesFromDir(StringUtil::UTF8StringToWxString(crc_string), EmuFolders::CheatsWS, L"Widescreen hacks"))
		{
			Console.WriteLn(Color_Gray, "Found widescreen patches in the cheats_ws folder --> skipping cheats_ws.zip");
		}
		else
		{
			// No ws cheat files found at the cheats_ws folder, try the ws cheats zip file.
			if (!s_widescreen_cheats_loaded)
			{
				std::optional<std::vector<u8>> data = Host::ReadResourceFile("cheats_ws.zip");
				if (data.has_value())
					s_widescreen_cheats_data = std::move(data.value());
			}

			if (!s_widescreen_cheats_data.empty())
			{
				ws_patch_count = LoadPatchesFromZip(StringUtil::UTF8StringToWxString(crc_string), wxT("cheats_ws.zip"), new wxMemoryInputStream(s_widescreen_cheats_data.data(), s_widescreen_cheats_data.size()));
				PatchesCon->WriteLn(Color_Green, "(Wide Screen Cheats DB) Patches Loaded: %d", ws_patch_count);
			}
		}

		if (ws_patch_count > 0)
			message.Write("%s%u widescreen patches", (patch_count > 0 || cheat_count > 0) ? " and " : "", ws_patch_count);
	}

	if (show_messages)
	{
		if (cheat_count > 0 || ws_patch_count > 0)
		{
			message.Write(" are active.");
			Host::AddKeyedOSDMessage("LoadPatches", message.GetString().ToStdString(), 5.0f);
		}
		else if (show_messages_when_disabled)
		{
			Host::AddKeyedOSDMessage("LoadPatches", "No cheats or patches (widescreen, compatibility or others) are found / enabled.", 8.0f);
		}
	}
}

void VMManager::UpdateRunningGame(bool force)
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
		new_serial = ingame ? SysGetDiscID().ToStdString() : SysGetBiosDiscID().ToStdString();
	}
	else
	{
		new_crc = GSDumpReplayer::GetDumpCRC();
		new_serial = GSDumpReplayer::GetDumpSerial();
	}

	if (!force && s_game_crc == new_crc && s_game_serial == new_serial)
		return;

	{
		std::unique_lock lock(s_info_mutex);
		s_game_serial = std::move(new_serial);
		s_game_crc = new_crc;
		s_game_name.clear();

		std::string memcardFilters;

		if (const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(s_game_serial))
		{
			s_game_name = game->name;
			memcardFilters = game->memcardFiltersAsString();
		}
		else
		{
			if (s_game_serial.empty() && s_game_crc == 0)
				s_game_name = "Booting PS2 BIOS...";
		}

		sioSetGameSerial(StringUtil::UTF8StringToWxString(memcardFilters.empty() ? s_game_serial : memcardFilters));
	}

	UpdateGameSettingsLayer();
	ApplySettings();

	ForgetLoadedPatches();
	LoadPatches(StringUtil::StdStringFromFormat("%08X", new_crc), true, false);
	GetMTGS().SendGameCRC(new_crc);

	Host::OnGameChanged(s_disc_path, s_game_serial, s_game_name, s_game_crc);
}

void VMManager::ReloadPatches(bool verbose)
{
	ForgetLoadedPatches();
	LoadPatches(StringUtil::StdStringFromFormat("%08X", s_game_crc), verbose, verbose);
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
			Host::ReportFormattedErrorAsync("Error", "Requested filename '%s' does not exist.", filename.c_str());
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
			// alternative way of booting an elf, change the elf override, and use no disc.
			CDVDsys_ChangeSource(CDVD_SourceType::NoDisc);
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

bool VMManager::ApplyBootParameters(const VMBootParameters& params)
{
	const bool default_fast_boot = Host::GetBoolSettingValue("EmuCore", "EnableFastBoot", true);
	EmuConfig.UseBOOT2Injection = params.fast_boot.value_or(default_fast_boot);
	s_elf_override = params.elf_override;
	s_disc_path.clear();

	if (params.source_type.has_value())
	{
		if (params.source_type.value() == CDVD_SourceType::Iso && !FileSystem::FileExists(params.filename.c_str()))
		{
			Host::ReportFormattedErrorAsync("Error", "Requested filename '%s' does not exist.", params.filename.c_str());
			return false;
		}

		// Use specified source type.
		CDVDsys_SetFile(params.source_type.value(), params.filename);
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
			Host::ReportFormattedErrorAsync("Error", "Requested boot ELF '%s' does not exist.", s_elf_override.c_str());
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

bool VMManager::Initialize(const VMBootParameters& boot_params)
{
	const Common::Timer init_timer;
	pxAssertRel(s_state.load() == VMState::Shutdown, "VM is shutdown");
	s_state.store(VMState::Initializing);
	Host::OnVMStarting();

	ScopedGuard close_state = [] {
		if (GSDumpReplayer::IsReplayingDump())
			GSDumpReplayer::Shutdown();

		s_state.store(VMState::Shutdown);
		Host::OnVMDestroyed();
	};

	LoadSettings();

	if (!ApplyBootParameters(boot_params))
		return false;

	EmuConfig.LimiterMode = GetInitialLimiterMode();

	// early out if we don't have a bios
	if (!GSDumpReplayer::IsReplayingDump() && !CheckBIOSAvailability())
		return false;

	Console.WriteLn("Allocating memory map...");
	s_vm_memory->CommitAll();

	Console.WriteLn("Opening CDVD...");
	if (!DoCDVDopen())
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize CDVD.");
		return false;
	}
	ScopedGuard close_cdvd = [] { DoCDVDclose(); };

	Console.WriteLn("Opening GS...");

	// TODO: Get rid of thread state nonsense and just make it a "normal" thread.
	static bool gs_initialized = false;
	if (!gs_initialized)
	{
		if (GSinit() != 0)
		{
			Console.WriteLn("Failed to initialize GS.");
			return false;
		}

		gs_initialized = true;
	}
	if (!GetMTGS().WaitForOpen())
	{
		// we assume GS is going to report its own error
		Console.WriteLn("Failed to open GS.");
		return false;
	}

	ScopedGuard close_gs = []() { GetMTGS().Suspend(); };

	Console.WriteLn("Opening SPU2...");
	if (SPU2init() != 0 || SPU2open() != 0)
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize SPU2.");
		SPU2shutdown();
		return false;
	}
	ScopedGuard close_spu2 = []() {
		SPU2close();
		SPU2shutdown();
	};

	Console.WriteLn("Opening PAD...");
	if (PADinit() != 0 || PADopen(Host::GetHostDisplay()->GetWindowInfo()) != 0)
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
	if (USBinit() != 0 || USBopen(Host::GetHostDisplay()->GetWindowInfo()) != 0)
	{
		Host::ReportErrorAsync("Startup Error", "Failed to initialize USB.");
		return false;
	}
	ScopedGuard close_usb = []() {
		USBclose();
		USBshutdown();
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

	s_cpu_implementation_changed.store(false);
	s_cpu_provider_pack->ApplyConfig();
	SetCPUState(EmuConfig.Cpu.sseMXCSR, EmuConfig.Cpu.sseVUMXCSR);
	SysClearExecutionCache();
	memBindConditionalHandlers();

	ForgetLoadedPatches();
	gsUpdateFrequency(EmuConfig);
	frameLimitReset();
	cpuReset();

	Console.WriteLn("VM subsystems initialized in %.2f ms", init_timer.GetTimeMilliseconds());
	s_state.store(VMState::Paused);
	Host::OnVMStarted();

	UpdateRunningGame(true);

	SetEmuThreadAffinities(true);

	PerformanceMetrics::Clear();

	// do we want to load state?
	if (!GSDumpReplayer::IsReplayingDump() && !boot_params.save_state.empty())
	{
		if (!DoLoadState(boot_params.save_state.c_str()))
		{
			Shutdown();
			return false;
		}
	}

	return true;
}

void VMManager::Shutdown(bool allow_save_resume_state /* = true */)
{
	SetTimerResolutionIncreased(false);

	// sync everything
	if (THREAD_VU1)
		vu1Thread.WaitVU();
	GetMTGS().WaitGS();

	if (!GSDumpReplayer::IsReplayingDump() && allow_save_resume_state && ShouldSaveResumeState())
	{
		std::string resume_file_name(GetCurrentSaveStateFileName(-1));
		if (!resume_file_name.empty() && !DoSaveState(resume_file_name.c_str(), -1))
			Console.Error("Failed to save resume state");
	}
	else if (GSDumpReplayer::IsReplayingDump())
	{
		GSDumpReplayer::Shutdown();
	}

	{
		std::unique_lock lock(s_info_mutex);
		s_disc_path.clear();
		s_game_crc = 0;
		s_game_serial.clear();
		s_game_name.clear();
		Host::OnGameChanged(s_disc_path, s_game_serial, s_game_name, 0);
	}
	UpdateGameSettingsLayer();

	std::string().swap(s_elf_override);

#ifdef _M_X86
	_mm_setcsr(s_mxcsr_saved);
#elif defined(_M_ARM64)
	a64_setfpcr(s_mxcsr_saved);
#endif

	ForgetLoadedPatches();
	R3000A::ioman::reset();
	USBclose();
	SPU2close();
	PADclose();
	DEV9close();
	DoCDVDclose();
	FWclose();
	FileMcd_EmuClose();
	GetMTGS().Suspend();
	USBshutdown();
	SPU2shutdown();
	PADshutdown();
	DEV9shutdown();

	// GS mess here...
	GetMTGS().Cancel();
	GSshutdown();

	s_vm_memory->DecommitAll();

	s_state.store(VMState::Shutdown);
	Host::OnVMDestroyed();
}

void VMManager::Reset()
{
	const bool game_was_started = g_GameStarted;

	SysClearExecutionCache();
	memBindConditionalHandlers();
	UpdateVSyncRate();
	frameLimitReset();
	cpuReset();

	// gameid change, so apply settings
	if (game_was_started)
		UpdateRunningGame(true);
}

bool VMManager::ShouldSaveResumeState()
{
	return Host::GetBoolSettingValue("EmuCore", "AutoStateLoadSave", false);
}

std::string VMManager::GetSaveStateFileName(const char* game_serial, u32 game_crc, s32 slot)
{
	if (!game_serial || game_serial[0] == '\0')
		return std::string();

	std::string filename;
	if (slot < 0)
		filename = StringUtil::StdStringFromFormat("%s (%08X).resume.p2s", game_serial, game_crc);
	else
		filename = StringUtil::StdStringFromFormat("%s (%08X).%02d.p2s", game_serial, game_crc, slot);

	return Path::CombineStdString(EmuFolders::Savestates, filename);
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
		SaveState_UnzipFromDisk(wxString::FromUTF8(filename));
		UpdateRunningGame(false);
		Host::OnSaveStateLoaded(filename, true);
		return true;
	}
	catch (Exception::BaseException& e)
	{
		Host::ReportErrorAsync("Failed to load save state", static_cast<const char*>(e.UserMsg().c_str()));
		Host::OnSaveStateLoaded(filename, false);
		return false;
	}
}

bool VMManager::DoSaveState(const char* filename, s32 slot_for_message)
{
	if (GSDumpReplayer::IsReplayingDump())
		return false;

	try
	{
		std::unique_ptr<ArchiveEntryList> elist = std::make_unique<ArchiveEntryList>(new VmStateBuffer(L"Zippable Savestate"));
		SaveState_DownloadState(elist.get());
		SaveState_ZipToDisk(elist.release(), SaveState_SaveScreenshot(), wxString::FromUTF8(filename), slot_for_message);
		Host::InvalidateSaveStateCache();
		Host::OnSaveStateSaved(filename);
		return true;
	}
	catch (Exception::BaseException& e)
	{
		Host::AddFormattedOSDMessage(15.0f, "Failed to save save state: %s", static_cast<const char*>(e.DiagMsg().c_str()));
		return false;
	}
}

bool VMManager::LoadState(const char* filename)
{
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
		Host::AddKeyedFormattedOSDMessage("LoadStateFromSlot", 5.0f, "There is no save state in slot %d.", slot);
		return false;
	}

	Host::AddKeyedFormattedOSDMessage("LoadStateFromSlot", 5.0f, "Loading state from slot %d...", slot);
	return DoLoadState(filename.c_str());
}

bool VMManager::SaveState(const char* filename)
{
	return DoSaveState(filename, -1);
}

bool VMManager::SaveStateToSlot(s32 slot)
{
	const std::string filename(GetCurrentSaveStateFileName(slot));
	if (filename.empty())
		return false;

	// if it takes more than a minute.. well.. wtf.
	Host::AddKeyedFormattedOSDMessage(StringUtil::StdStringFromFormat("SaveStateSlot%d", slot), 60.0f, "Saving state to slot %d...", slot);
	return DoSaveState(filename.c_str(), slot);
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
	GetMTGS().SetVSync(EmuConfig.GetEffectiveVsyncMode());
}

void VMManager::FrameAdvance(u32 num_frames /*= 1*/)
{
	if (!HasValidVM())
		return;

	s_frame_advance_count = num_frames;
	SetState(VMState::Running);
}

bool VMManager::ChangeDisc(std::string path)
{
	std::string old_path(CDVDsys_GetFile(CDVD_SourceType::Iso));
	CDVD_SourceType old_type = CDVDsys_GetSourceType();

	const std::string display_name(path.empty() ? std::string() : FileSystem::GetDisplayNameFromPath(path));
	CDVDsys_ChangeSource(path.empty() ? CDVD_SourceType::NoDisc : CDVD_SourceType::Iso);
	if (!path.empty())
		CDVDsys_SetFile(CDVD_SourceType::Iso, std::move(path));

	const bool result = DoCDVDopen();
	if (result)
	{
		Host::AddFormattedOSDMessage(5.0f, "Disc changed to '%s'.", display_name.c_str());
	}
	else
	{
		Host::AddFormattedOSDMessage(20.0f, "Failed to open new disc image '%s'. Reverting to old image.", display_name.c_str());
		CDVDsys_ChangeSource(old_type);
		if (!old_path.empty())
			CDVDsys_SetFile(old_type, std::move(old_path));
		if (!DoCDVDopen())
		{
			Host::AddFormattedOSDMessage(20.0f, "Failed to switch back to old disc image. Removing disc.");
			CDVDsys_ChangeSource(CDVD_SourceType::NoDisc);
			DoCDVDopen();
		}
	}

	cdvdCtrlTrayOpen();
	return result;
}

bool VMManager::IsElfFileName(const std::string& path)
{
	return StringUtil::EndsWithNoCase(path, ".elf");
}

bool VMManager::IsGSDumpFileName(const std::string& path)
{
	return (StringUtil::EndsWithNoCase(path, ".gs") || StringUtil::EndsWithNoCase(path, ".gs.xz"));
}

void VMManager::Execute()
{
	// Check for interpreter<->recompiler switches.
	if (s_cpu_implementation_changed.exchange(false))
	{
		// We need to switch the cpus out, and reset the new ones if so.
		s_cpu_provider_pack->ApplyConfig();
		SysClearExecutionCache();
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
	return s_state.load() != VMState::Running || s_cpu_implementation_changed.load();
}

void VMManager::Internal::GameStartingOnCPUThread()
{
	GetMTGS().SendGameCRC(ElfCRC);

	MIPSAnalyst::ScanForFunctions(R5900SymbolMap, ElfTextRange.first, ElfTextRange.first + ElfTextRange.second, true);
	R5900SymbolMap.UpdateActiveSymbols();
	R3000SymbolMap.UpdateActiveSymbols();

	UpdateRunningGame(false);
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

	Host::PumpMessagesOnCPUThread();
	InputManager::PollSources();
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

	// did we toggle recompilers?
	if (EmuConfig.Cpu.CpusChanged(old_config.Cpu))
	{
		// This has to be done asynchronously, since we're still executing the
		// cpu when this function is called. Break the execution as soon as
		// possible and reset next time we're called.
		s_cpu_implementation_changed.store(true);
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
	GetMTGS().SetVSync(EmuConfig.GetEffectiveVsyncMode());
}

void VMManager::CheckForFramerateConfigChanges(const Pcsx2Config& old_config)
{
	if (EmuConfig.Framerate == old_config.Framerate)
		return;

	Console.WriteLn("Updating frame rate configuration");
	gsUpdateFrequency(EmuConfig);
	UpdateVSyncRate();
	frameLimitReset();
	GetMTGS().SetVSync(EmuConfig.GetEffectiveVsyncMode());
}

void VMManager::CheckForPatchConfigChanges(const Pcsx2Config& old_config)
{
	if (EmuConfig.EnableCheats == old_config.EnableCheats &&
		EmuConfig.EnableWideScreenPatches == old_config.EnableWideScreenPatches &&
		EmuConfig.EnablePatches == old_config.EnablePatches)
	{
		return;
	}

	ReloadPatches(true);
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

	SPU2close();
	SPU2shutdown();
	if (SPU2init() != 0 || SPU2open() != 0)
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

	// force reindexing, mc folder code is janky
	std::string sioSerial;
	{
		std::unique_lock lock(s_info_mutex);
		if (const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(s_game_serial))
			sioSerial = game->memcardFiltersAsString();
		if (sioSerial.empty())
			sioSerial = s_game_serial;
	}
	sioSetGameSerial(StringUtil::UTF8StringToWxString(sioSerial));
}

void VMManager::CheckForConfigChanges(const Pcsx2Config& old_config)
{
	CheckForCPUConfigChanges(old_config);
	CheckForGSConfigChanges(old_config);
	CheckForFramerateConfigChanges(old_config);
	CheckForPatchConfigChanges(old_config);
	CheckForSPU2ConfigChanges(old_config);
	CheckForDEV9ConfigChanges(old_config);
	CheckForMemoryCardConfigChanges(old_config);

	if (EmuConfig.EnableCheats != old_config.EnableCheats || EmuConfig.EnableWideScreenPatches != old_config.EnableWideScreenPatches)
		VMManager::ReloadPatches(true);
}

void VMManager::ApplySettings()
{
	Console.WriteLn("Applying settings...");

	// if we're running, ensure the threads are synced
	const bool running = (s_state.load() == VMState::Running);
	if (running)
	{
		if (THREAD_VU1)
			vu1Thread.WaitVU();
		GetMTGS().WaitGS(false);
	}

	const Pcsx2Config old_config(EmuConfig);
	LoadSettings();

	if (HasValidVM())
	{
		CheckForConfigChanges(old_config);
		SetEmuThreadAffinities(false);
	}
}

bool VMManager::ReloadGameSettings()
{
	if (!UpdateGameSettingsLayer())
		return false;

	ApplySettings();
	return true;
}

static void HotkeyAdjustTargetSpeed(double delta)
{
	EmuConfig.Framerate.NominalScalar = EmuConfig.GS.LimitScalar + delta;
	VMManager::SetLimiterMode(LimiterModeType::Nominal);
	gsUpdateFrequency(EmuConfig);
	GetMTGS().SetVSync(EmuConfig.GetEffectiveVsyncMode());
	Host::AddKeyedFormattedOSDMessage("SpeedChanged", 5.0f, "Target speed set to %.0f%%.", std::round(EmuConfig.Framerate.NominalScalar * 100.0));
}

static constexpr s32 CYCLE_SAVE_STATE_SLOTS = 10;

static void HotkeyCycleSaveSlot(s32 delta)
{
	// 1..10
	s_current_save_slot = ((s_current_save_slot - 1) + delta);
	if (s_current_save_slot < 0)
		s_current_save_slot = CYCLE_SAVE_STATE_SLOTS;
	else
		s_current_save_slot = (s_current_save_slot % CYCLE_SAVE_STATE_SLOTS) + 1;

	const std::string filename(VMManager::GetSaveStateFileName(s_game_serial.c_str(), s_game_crc, s_current_save_slot));
	FILESYSTEM_STAT_DATA sd;
	if (!filename.empty() && FileSystem::StatFile(filename.c_str(), &sd))
	{
		char date_buf[128] = {};
#ifdef _WIN32
		ctime_s(date_buf, std::size(date_buf), &sd.ModificationTime);
#else
		ctime_r(&sd.ModificationTime, date_buf);
#endif

		// remove terminating \n
		size_t len = std::strlen(date_buf);
		if (len > 0 && date_buf[len - 1] == '\n')
			date_buf[len - 1] = 0;

		Host::AddKeyedFormattedOSDMessage("CycleSaveSlot", 5.0f, "Save slot %d selected (last save: %s).", s_current_save_slot, date_buf);
	}
	else
	{
		Host::AddKeyedFormattedOSDMessage("CycleSaveSlot", 5.0f, "Save slot %d selected (no save yet).", s_current_save_slot);
	}
}

BEGIN_HOTKEY_LIST(g_vm_manager_hotkeys)
DEFINE_HOTKEY("ToggleFrameLimit", "System", "Toggle Frame Limit", [](bool pressed) {
	if (!pressed)
	{
		VMManager::SetLimiterMode((EmuConfig.LimiterMode != LimiterModeType::Unlimited) ?
                                      LimiterModeType::Unlimited :
                                      LimiterModeType::Nominal);
	}
})
DEFINE_HOTKEY("ToggleTurbo", "System", "Toggle Turbo", [](bool pressed) {
	if (!pressed)
	{
		VMManager::SetLimiterMode((EmuConfig.LimiterMode != LimiterModeType::Turbo) ?
                                      LimiterModeType::Turbo :
                                      LimiterModeType::Nominal);
	}
})
DEFINE_HOTKEY("ToggleSlowMotion", "System", "Toggle Slow Motion", [](bool pressed) {
	if (!pressed)
	{
		VMManager::SetLimiterMode((EmuConfig.LimiterMode != LimiterModeType::Slomo) ?
                                      LimiterModeType::Slomo :
                                      LimiterModeType::Nominal);
	}
})
DEFINE_HOTKEY("IncreaseSpeed", "System", "Increase Target Speed", [](bool pressed) {
	if (!pressed)
		HotkeyAdjustTargetSpeed(0.1);
})
DEFINE_HOTKEY("DecreaseSpeed", "System", "Decrease Target Speed", [](bool pressed) {
	if (!pressed)
		HotkeyAdjustTargetSpeed(-0.1);
})
DEFINE_HOTKEY("ResetVM", "System", "Reset Virtual Machine", [](bool pressed) {
	if (!pressed && VMManager::HasValidVM())
		VMManager::Reset();
})
DEFINE_HOTKEY("FrameAdvance", "System", "Frame Advance", [](bool pressed) {
	if (!pressed)
		VMManager::FrameAdvance(1);
})

DEFINE_HOTKEY("PreviousSaveStateSlot", "Save States", "Select Previous Save Slot", [](bool pressed) {
	if (!pressed)
		HotkeyCycleSaveSlot(-1);
})
DEFINE_HOTKEY("NextSaveStateSlot", "Save States", "Select Next Save Slot", [](bool pressed) {
	if (!pressed)
		HotkeyCycleSaveSlot(1);
})
DEFINE_HOTKEY("SaveStateToSlot", "Save States", "Save State To Selected Slot", [](bool pressed) {
	if (!pressed)
		VMManager::SaveStateToSlot(s_current_save_slot);
})
DEFINE_HOTKEY("LoadStateFromSlot", "Save States", "Load State From Selected Slot", [](bool pressed) {
	if (!pressed)
		VMManager::LoadStateFromSlot(s_current_save_slot);
})
END_HOTKEY_LIST()

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

void VMManager::SetEmuThreadAffinities(bool force)
{
	Console.Error("(SetEmuThreadAffinities) Not implemented");
}
