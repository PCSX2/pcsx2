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

#pragma once

#include "common/Pcsx2Defs.h"
#include <string>
#include <mutex>

#include "Config.h"

class SettingsInterface;

namespace Host
{
	/// Sets host-specific default settings.
	void SetDefaultUISettings(SettingsInterface& si);
} // namespace Host

namespace CommonHost
{
	/// Initializes critical folders (AppRoot, DataRoot, Settings). Call once on startup.
	bool InitializeCriticalFolders();

	/// Checks settings version. Call once on startup. If it returns false, you should prompt the user to reset.
	bool CheckSettingsVersion();

	/// Loads early settings. Call once on startup.
	void LoadStartupSettings();

	/// Sets default settings for the specified categories.
	void SetDefaultSettings(SettingsInterface& si, bool folders, bool core, bool controllers, bool hotkeys, bool ui);

	/// Initializes common host state, called on the CPU thread.
	void CPUThreadInitialize();

	/// Cleans up common host state, called on the CPU thread.
	void CPUThreadShutdown();

	/// Loads common host settings (including input bindings).
	void LoadSettings(SettingsInterface& si, std::unique_lock<std::mutex>& lock);

	/// Called after settings are updated.
	void CheckForSettingsChanges(const Pcsx2Config& old_config);

	/// Called when the VM is starting initialization, but has not been completed yet.
	void OnVMStarting();

	/// Called when the VM is created.
	void OnVMStarted();

	/// Called when the VM is shut down or destroyed.
	void OnVMDestroyed();

	/// Called when the VM is paused.
	void OnVMPaused();

	/// Called when the VM is resumed after being paused.
	void OnVMResumed();

	/// Called when the running executable changes.
	void OnGameChanged(const std::string& disc_path, const std::string& elf_override, const std::string& game_serial,
		const std::string& game_name, u32 game_crc);

	/// Provided by the host; called once per frame at guest vsync.
	void CPUThreadVSync();

	/// Returns the time elapsed in the current play session.
	u64 GetSessionPlayedTime();

#ifdef ENABLE_DISCORD_PRESENCE
	/// Called when the rich presence string, provided by RetroAchievements, changes.
	void UpdateDiscordPresence(const std::string& rich_presence);
#endif

	namespace Internal
	{
		/// Resets any state for hotkey-related VMs, called on VM startup.
		void ResetVMHotkeyState();
	} // namespace Internal
} // namespace CommonHost
