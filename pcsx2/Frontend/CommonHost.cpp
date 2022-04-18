/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022 PCSX2 Dev Team
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
#include "common/Assertions.h"
#include "common/CrashHandler.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/Threading.h"
#include "Frontend/CommonHost.h"
#include "Frontend/FullscreenUI.h"
#include "Frontend/GameList.h"
#include "Frontend/LayeredSettingsInterface.h"
#include "Frontend/InputManager.h"
#include "Frontend/LogSink.h"
#include "GS.h"
#include "GS/Renderers/HW/GSTextureReplacements.h"
#include "Host.h"
#include "HostSettings.h"
#include "IconsFontAwesome5.h"
#include "MemoryCardFile.h"
#include "PAD/Host/PAD.h"
#include "PerformanceMetrics.h"
#include "Sio.h"
#include "VMManager.h"

#ifdef ENABLE_ACHIEVEMENTS
#include "Frontend/Achievements.h"
#endif

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <KnownFolders.h>
#include <ShlObj.h>
#endif

static constexpr u32 SETTINGS_VERSION = 1;

namespace CommonHost
{
	static void SetAppRoot();
	static void SetResourcesDirectory();
	static bool ShouldUsePortableMode();
	static void SetDataDirectory();
	static void SetCommonDefaultSettings(SettingsInterface& si);
} // namespace CommonHost

bool CommonHost::InitializeCriticalFolders()
{
	SetAppRoot();
	SetResourcesDirectory();
	SetDataDirectory();

	// logging of directories in case something goes wrong super early
	Console.WriteLn("AppRoot Directory: %s", EmuFolders::AppRoot.c_str());
	Console.WriteLn("DataRoot Directory: %s", EmuFolders::DataRoot.c_str());
	Console.WriteLn("Resources Directory: %s", EmuFolders::Resources.c_str());

	// allow SetDataDirectory() to change settings directory (if we want to split config later on)
	if (EmuFolders::Settings.empty())
		EmuFolders::Settings = Path::Combine(EmuFolders::DataRoot, "inis");

	// Write crash dumps to the data directory, since that'll be accessible for certain.
	CrashHandler::SetWriteDirectory(EmuFolders::DataRoot);

	// the resources directory should exist, bail out if not
	if (!FileSystem::DirectoryExists(EmuFolders::Resources.c_str()))
	{
		Console.Error("Resources directory is missing.");
		return false;
	}

	return true;
}

void CommonHost::SetAppRoot()
{
	std::string program_path(FileSystem::GetProgramPath());
	Console.WriteLn("Program Path: %s", program_path.c_str());

	EmuFolders::AppRoot = Path::Canonicalize(Path::GetDirectory(program_path));
}

void CommonHost::SetResourcesDirectory()
{
#ifndef __APPLE__
	// On Windows/Linux, these are in the binary directory.
	EmuFolders::Resources = Path::Combine(EmuFolders::AppRoot, "resources");
#else
	// On macOS, this is in the bundle resources directory.
	EmuFolders::Resources = Path::Canonicalize(Path::Combine(EmuFolders::AppRoot, "../Resources"));
#endif
}

bool CommonHost::ShouldUsePortableMode()
{
	// Check whether portable.ini exists in the program directory.
	return FileSystem::FileExists(Path::Combine(EmuFolders::AppRoot, "portable.ini").c_str());
}

void CommonHost::SetDataDirectory()
{
	if (ShouldUsePortableMode())
	{
		EmuFolders::DataRoot = EmuFolders::AppRoot;
		return;
	}

#if defined(_WIN32)
	// On Windows, use My Documents\PCSX2 to match old installs.
	PWSTR documents_directory;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &documents_directory)))
	{
		if (std::wcslen(documents_directory) > 0)
			EmuFolders::DataRoot = Path::Combine(StringUtil::WideStringToUTF8String(documents_directory), "PCSX2");
		CoTaskMemFree(documents_directory);
	}
#elif defined(__linux__)
	// Use $XDG_CONFIG_HOME/PCSX2 if it exists.
	const char* xdg_config_home = getenv("XDG_CONFIG_HOME");
	if (xdg_config_home && Path::IsAbsolute(xdg_config_home))
	{
		EmuFolders::DataRoot = Path::Combine(xdg_config_home, "PCSX2");
	}
	else
	{
		// Use ~/PCSX2 for non-XDG, and ~/.config/PCSX2 for XDG.
		// Maybe we should drop the former when Qt goes live.
		const char* home_dir = getenv("HOME");
		if (home_dir)
		{
#ifndef XDG_STD
			EmuFolders::DataRoot = Path::Combine(home_dir, "PCSX2");
#else
			// ~/.config should exist, but just in case it doesn't and this is a fresh profile..
			const std::string config_dir(Path::Combine(home_dir, ".config"));
			if (!FileSystem::DirectoryExists(config_dir.c_str()))
				FileSystem::CreateDirectoryPath(config_dir.c_str(), false);

			EmuFolders::DataRoot = Path::Combine(config_dir, "PCSX2");
#endif
		}
	}
#elif defined(__APPLE__)
	static constexpr char MAC_DATA_DIR[] = "Library/Application Support/PCSX2";
	const char* home_dir = getenv("HOME");
	if (home_dir)
		EmuFolders::DataRoot = Path::Combine(home_dir, MAC_DATA_DIR);
#endif

	// make sure it exists
	if (!EmuFolders::DataRoot.empty() && !FileSystem::DirectoryExists(EmuFolders::DataRoot.c_str()))
	{
		// we're in trouble if we fail to create this directory... but try to hobble on with portable
		if (!FileSystem::CreateDirectoryPath(EmuFolders::DataRoot.c_str(), false))
			EmuFolders::DataRoot.clear();
	}

	// couldn't determine the data directory? fallback to portable.
	if (EmuFolders::DataRoot.empty())
		EmuFolders::DataRoot = EmuFolders::AppRoot;
}

bool CommonHost::CheckSettingsVersion()
{
	SettingsInterface* bsi = Host::Internal::GetBaseSettingsLayer();

	uint settings_version;
	return (bsi->GetUIntValue("UI", "SettingsVersion", &settings_version) && settings_version == SETTINGS_VERSION);
}

void CommonHost::LoadStartupSettings()
{
	SettingsInterface* bsi = Host::Internal::GetBaseSettingsLayer();
	EmuFolders::LoadConfig(*bsi);
	EmuFolders::EnsureFoldersExist();
	UpdateLogging(*bsi);

#ifdef ENABLE_RAINTEGRATION
	// RAIntegration switch must happen before the UI is created.
	if (Host::GetBaseBoolSettingValue("Achievements", "UseRAIntegration", false))
		Achievements::SwitchToRAIntegration();
#endif
}

void CommonHost::SetDefaultSettings(SettingsInterface& si, bool folders, bool core, bool controllers, bool hotkeys, bool ui)
{
	if (si.GetUIntValue("UI", "SettingsVersion", 0u) != SETTINGS_VERSION)
		si.SetUIntValue("UI", "SettingsVersion", SETTINGS_VERSION);

	if (folders)
		EmuFolders::SetDefaults(si);
	if (core)
	{
		VMManager::SetDefaultSettings(si);
		SetCommonDefaultSettings(si);
	}
	if (controllers)
		PAD::SetDefaultControllerConfig(si);
	if (hotkeys)
		PAD::SetDefaultHotkeyConfig(si);
	if (ui)
		Host::SetDefaultUISettings(si);
}

void CommonHost::SetCommonDefaultSettings(SettingsInterface& si)
{
	SetDefaultLoggingSettings(si);
}

void CommonHost::CPUThreadInitialize()
{
	Threading::SetNameOfCurrentThread("CPU Thread");
	PerformanceMetrics::SetCPUThread(Threading::ThreadHandle::GetForCallingThread());

	// neither of these should ever fail.
	if (!VMManager::Internal::InitializeGlobals() || !VMManager::Internal::InitializeMemory())
		pxFailRel("Failed to allocate memory map");

	// We want settings loaded so we choose the correct renderer for big picture mode.
	// This also sorts out input sources.
	VMManager::LoadSettings();

#ifdef ENABLE_ACHIEVEMENTS
	if (EmuConfig.Achievements.Enabled)
		Achievements::Initialize();
#endif
}

void CommonHost::CPUThreadShutdown()
{
#ifdef ENABLE_ACHIEVEMENTS
	Achievements::Shutdown();
#endif

	InputManager::CloseSources();
	VMManager::WaitForSaveStateFlush();
	VMManager::Internal::ReleaseMemory();
	VMManager::Internal::ReleaseGlobals();
	PerformanceMetrics::SetCPUThread(Threading::ThreadHandle());
}

void CommonHost::LoadSettings(SettingsInterface& si, std::unique_lock<std::mutex>& lock)
{
	SettingsInterface* binding_si = Host::GetSettingsInterfaceForBindings();
	InputManager::ReloadSources(si, lock);
	InputManager::ReloadBindings(si, *binding_si);

	UpdateLogging(si);
}

void CommonHost::CheckForSettingsChanges(const Pcsx2Config& old_config)
{
#ifdef ENABLE_ACHIEVEMENTS
	if (EmuConfig.Achievements != old_config.Achievements)
		Achievements::UpdateSettings(old_config.Achievements);
#endif

	FullscreenUI::CheckForConfigChanges(old_config);
}

void CommonHost::OnVMStarting()
{
	CommonHost::Internal::ResetVMHotkeyState();
}

void CommonHost::OnVMStarted()
{
	FullscreenUI::OnVMStarted();
}

void CommonHost::OnVMDestroyed()
{
	FullscreenUI::OnVMDestroyed();
}

void CommonHost::OnVMPaused()
{
	InputManager::PauseVibration();
	FullscreenUI::OnVMPaused();

#ifdef ENABLE_ACHIEVEMENTS
	Achievements::OnPaused(true);
#endif
}

void CommonHost::OnVMResumed()
{
	FullscreenUI::OnVMResumed();

#ifdef ENABLE_ACHIEVEMENTS
	Achievements::OnPaused(false);
#endif
}

void CommonHost::OnGameChanged(const std::string& disc_path, const std::string& game_serial, const std::string& game_name, u32 game_crc)
{
	if (FullscreenUI::IsInitialized())
	{
		GetMTGS().RunOnGSThread([disc_path, game_serial, game_name, game_crc]() {
			FullscreenUI::OnRunningGameChanged(std::move(disc_path), std::move(game_serial), std::move(game_name), game_crc);
		});
	}

#ifdef ENABLE_ACHIEVEMENTS
	Achievements::GameChanged(game_crc);
#endif
}

void CommonHost::CPUThreadVSync()
{
#ifdef ENABLE_ACHIEVEMENTS
	if (Achievements::IsActive())
		Achievements::VSyncUpdate();
#endif

	InputManager::PollSources();
}

bool Host::GetSerialAndCRCForFilename(const char* filename, std::string* serial, u32* crc)
{
	{
		auto lock = GameList::GetLock();
		if (const GameList::Entry* entry = GameList::GetEntryForPath(filename); entry)
		{
			*serial = entry->serial;
			*crc = entry->crc;
			return true;
		}
	}

	// Just scan it.. hopefully it'll come back okay.
	GameList::Entry temp_entry;
	if (GameList::PopulateEntryFromPath(filename, &temp_entry))
	{
		*serial = std::move(temp_entry.serial);
		*crc = temp_entry.crc;
		return true;
	}

	return false;
}
