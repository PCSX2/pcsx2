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

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <string>
#include <string_view>
#include <optional>

#include "common/Pcsx2Defs.h"

#include "Config.h"

enum class CDVD_SourceType : uint8_t;

enum class VMState
{
	Shutdown,
	Initializing,
	Running,
	Paused,
	Resetting,
	Stopping,
};

struct VMBootParameters
{
	std::string filename;
	std::string elf_override;
	std::string save_state;
	std::optional<s32> state_index;
	std::optional<CDVD_SourceType> source_type;

	std::optional<bool> fast_boot;
	std::optional<bool> fullscreen;
};

namespace VMManager
{
	/// The number of usable save state slots.
	static constexpr s32 NUM_SAVE_STATE_SLOTS = 10;

	/// Makes sure that AVX2 is available if we were compiled with it.
	bool PerformEarlyHardwareChecks(const char** error);

	/// Returns the current state of the VM.
	VMState GetState();

	/// Alters the current state of the VM.
	void SetState(VMState state);

	/// Returns true if there is an active virtual machine.
	bool HasValidVM();

	/// Returns the path of the disc currently running.
	std::string GetDiscPath();

	/// Returns the crc of the executable currently running.
	u32 GetGameCRC();

	/// Returns the serial of the disc/executable currently running.
	std::string GetGameSerial();

	/// Returns the name of the disc/executable currently running.
	std::string GetGameName();

	/// Loads global settings (i.e. EmuConfig).
	void LoadSettings();

	/// Initializes all system components.
	bool Initialize(VMBootParameters boot_params);

	/// Destroys all system components.
	void Shutdown(bool save_resume_state);

	/// Resets all subsystems to a cold boot.
	void Reset();

	/// Runs the VM until the CPU execution is canceled.
	void Execute();

	/// Changes the pause state of the VM, resetting anything needed when unpausing.
	void SetPaused(bool paused);

	/// Reloads settings, and applies any changes present.
	void ApplySettings();

	/// Reloads game specific settings, and applys any changes present.
	bool ReloadGameSettings();

	/// Reloads cheats/patches. If verbose is set, the number of patches loaded will be shown in the OSD.
	void ReloadPatches(bool verbose, bool show_messages_when_disabled);

	/// Returns the save state filename for the given game serial/crc.
	std::string GetSaveStateFileName(const char* game_serial, u32 game_crc, s32 slot);

	/// Returns the path to save state for the specified disc/elf.
	std::string GetSaveStateFileName(const char* filename, s32 slot);

	/// Returns true if there is a save state in the specified slot.
	bool HasSaveStateInSlot(const char* game_serial, u32 game_crc, s32 slot);

	/// Loads state from the specified file.
	bool LoadState(const char* filename);

	/// Loads state from the specified slot.
	bool LoadStateFromSlot(s32 slot);

	/// Saves state to the specified filename.
	bool SaveState(const char* filename, bool zip_on_thread = true, bool backup_old_state = false);

	/// Saves state to the specified slot.
	bool SaveStateToSlot(s32 slot, bool zip_on_thread = true);

	/// Waits until all compressing save states have finished saving to disk.
	void WaitForSaveStateFlush();

	/// Removes all save states for the specified serial and crc. Returns the number of files deleted.
	u32 DeleteSaveStates(const char* game_serial, u32 game_crc, bool also_backups = true);

	/// Returns the current limiter mode.
	LimiterModeType GetLimiterMode();

	/// Updates the host vsync state, as well as timer frequencies. Call when the speed limiter is adjusted.
	void SetLimiterMode(LimiterModeType type);

	/// Runs the virtual machine for the specified number of video frames, and then automatically pauses.
	void FrameAdvance(u32 num_frames = 1);

	/// Changes the disc in the virtual CD/DVD drive. Passing an empty will remove any current disc.
	/// Returns false if the new disc can't be opened.
	bool ChangeDisc(CDVD_SourceType source, std::string path);

	/// Returns true if the specified path is an ELF.
	bool IsElfFileName(const std::string_view& path);

	/// Returns true if the specified path is a blockdump.
	bool IsBlockDumpFileName(const std::string_view& path);

	/// Returns true if the specified path is a GS Dump.
	bool IsGSDumpFileName(const std::string_view& path);

	/// Returns true if the specified path is a save state.
	bool IsSaveStateFileName(const std::string_view& path);

	/// Returns true if the specified path is a disc image.
	bool IsDiscFileName(const std::string_view& path);

	/// Returns true if the specified path is a disc/elf/etc.
	bool IsLoadableFileName(const std::string_view& path);

	/// Returns the serial to use when computing the game settings path for the current game.
	std::string GetSerialForGameSettings();

	/// Returns the path for the game settings ini file for the specified CRC.
	std::string GetGameSettingsPath(const std::string_view& game_serial, u32 game_crc);

	/// Returns the ISO override for an ELF via gamesettings.
	std::string GetDiscOverrideFromGameSettings(const std::string& elf_path);

	/// Returns the path for the input profile ini file with the specified name (may not exist).
	std::string GetInputProfilePath(const std::string_view& name);

	/// Resizes the render window to the display size, with an optional scale.
	/// If the scale is set to 0, the internal resolution will be used, otherwise it is treated as a multiplier to 1x.
	void RequestDisplaySize(float scale = 0.0f);

	/// Initializes default configuration in the specified file.
	void SetDefaultSettings(SettingsInterface& si);

	/// Returns a list of processors in the system, and their corresponding affinity mask.
	/// This list is ordered by most performant to least performant for pinning threads to.
	const std::vector<u32>& GetSortedProcessorList();

	/// Internal callbacks, implemented in the emu core.
	namespace Internal
	{
		/// Performs early global initialization.
		bool InitializeGlobals();

		/// Releases resources allocated in InitializeGlobals().
		void ReleaseGlobals();

		/// Reserves memory for the virtual machines.
		bool InitializeMemory();

		/// Completely releases all memory for the virtual machine.
		void ReleaseMemory();

		const std::string& GetElfOverride();
		bool IsExecutionInterrupted();
		void EntryPointCompilingOnCPUThread();
		void GameStartingOnCPUThread();
		void VSyncOnCPUThread();
	} // namespace Internal
} // namespace VMManager


namespace Host
{
	/// Called with the settings lock held, when system settings are being loaded (should load input sources, etc).
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

	/// Called when performance metrics are updated, approximately once a second.
	void OnPerformanceMetricsUpdated();

	/// Looks up the serial and CRC for a game in the most efficient manner possible.
	/// Implemented in the host because it may have a game list cache.
	bool GetSerialAndCRCForFilename(const char* filename, std::string* serial, u32* crc);

	/// Called when a save state is loading, before the file is processed.
	void OnSaveStateLoading(const std::string_view& filename);

	/// Called after a save state is successfully loaded. If the save state was invalid, was_successful will be false.
	void OnSaveStateLoaded(const std::string_view& filename, bool was_successful);

	/// Called when a save state is being created/saved. The compression/write to disk is asynchronous, so this callback
	/// just signifies that the save has started, not necessarily completed.
	void OnSaveStateSaved(const std::string_view& filename);

	/// Provided by the host; called when the running executable changes.
	void OnGameChanged(const std::string& disc_path, const std::string& elf_override, const std::string& game_serial,
		const std::string& game_name, u32 game_crc);

	/// Provided by the host; called once per frame at guest vsync.
	void CPUThreadVSync();
} // namespace Host
