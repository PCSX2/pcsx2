// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
	bool disable_achievements_hardcore_mode = false;
};

namespace VMManager
{
	/// The number of usable save state slots.
	static constexpr s32 NUM_SAVE_STATE_SLOTS = 10;

	/// The stack size to use for threads running recompilers
	static constexpr std::size_t EMU_THREAD_STACK_SIZE = 2 * 1024 * 1024; // µVU likes recursion

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

	/// Returns the serial of the disc currently running.
	std::string GetDiscSerial();

	/// Returns the path of the main ELF of the disc currently running.
	std::string GetDiscELF();

	/// Returns the name of the disc/executable currently running.
	std::string GetTitle(bool prefer_en);

	/// Returns the CRC for the main ELF of the disc currently running.
	u32 GetDiscCRC();

	/// Returns the version of the disc currently running.
	std::string GetDiscVersion();

	/// Returns the crc of the executable currently running.
	u32 GetCurrentCRC();

	/// Returns the path to the ELF which is currently running. Only safe to read on the EE thread.
	const std::string& GetCurrentELF();

	/// Initializes all system components.
	bool Initialize(VMBootParameters boot_params);

	/// Destroys all system components.
	void Shutdown(bool save_resume_state);

	/// Resets all subsystems to a cold boot.
	void Reset();

	/// Runs the VM until the CPU execution is canceled.
	void Execute();

	/// Polls input, updates subsystems which are present while paused/inactive.
	void IdlePollUpdate();

	/// Changes the pause state of the VM, resetting anything needed when unpausing.
	void SetPaused(bool paused);

	/// Reloads settings, and applies any changes present.
	void ApplySettings();

	/// Reloads game specific settings, and applys any changes present.
	bool ReloadGameSettings();

	/// Reloads game patches.
	void ReloadPatches(bool reload_files, bool reload_enabled_list, bool verbose, bool verbose_if_changed);

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

	/// Returns the target speed, based on the limiter mode.
	float GetTargetSpeed();

	/// Ensures the target speed reflects the current configuration. Call if you change anything in
	/// EmuConfig.EmulationSpeed without going through the usual config apply.
	void UpdateTargetSpeed();

	/// Returns the current frame rate of the virtual machine.
	float GetFrameRate();

	/// Runs the virtual machine for the specified number of video frames, and then automatically pauses.
	void FrameAdvance(u32 num_frames = 1);

	/// Changes the disc in the virtual CD/DVD drive. Passing an empty will remove any current disc.
	/// Returns false if the new disc can't be opened.
	bool ChangeDisc(CDVD_SourceType source, std::string path);

	/// Changes the ELF to boot ("ELF override"). The VM will be reset.
	bool SetELFOverride(std::string path);

	/// Changes the current GS dump being played back.
	bool ChangeGSDump(const std::string& path);

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

	/// Returns the path for the debugger settings json file for the specified game serial and CRC.
	std::string GetDebuggerSettingsFilePath(const std::string_view& game_serial, u32 game_crc);

	/// Returns the path for the debugger settings json file for the current game.
	std::string GetDebuggerSettingsFilePathForCurrentGame();

	/// Resizes the render window to the display size, with an optional scale.
	/// If the scale is set to 0, the internal resolution will be used, otherwise it is treated as a multiplier to 1x.
	void RequestDisplaySize(float scale = 0.0f);

	/// Initializes default configuration in the specified file for the specified categories.
	void SetDefaultSettings(SettingsInterface& si, bool folders, bool core, bool controllers, bool hotkeys, bool ui);

	/// Returns a list of processors in the system, and their corresponding affinity mask.
	/// This list is ordered by most performant to least performant for pinning threads to.
	const std::vector<u32>& GetSortedProcessorList();

	/// Returns the time elapsed in the current play session.
	u64 GetSessionPlayedTime();

	/// Called when the rich presence string, provided by RetroAchievements, changes.
	void UpdateDiscordPresence(bool update_session_time);

	/// Internal callbacks, implemented in the emu core.
	namespace Internal
	{
		/// Checks settings version. Call once on startup. If it returns false, you should prompt the user to reset.
		bool CheckSettingsVersion();

		/// Loads early settings. Call once on startup.
		void LoadStartupSettings();

		/// Overrides the filename used for the file log.
		void SetFileLogPath(std::string path);

		/// Prevents the system console from being displayed.
		void SetBlockSystemConsole(bool block);

		/// Initializes common host state, called on the CPU thread.
		bool CPUThreadInitialize();

		/// Cleans up common host state, called on the CPU thread.
		void CPUThreadShutdown();

		/// Resets any state for hotkey-related VMs, called on VM startup.
		void ResetVMHotkeyState();

		/// Updates the variables in the EmuFolders namespace, reloading subsystems if needed.
		void UpdateEmuFolders();

		/// Returns true if fast booting is active (requested but ELF not started).
		bool IsFastBootInProgress();

		/// Disables fast boot if it was requested, and found to be incompatible.
		void DisableFastBoot();

		/// Returns true if the current ELF has started executing.
		bool HasBootedELF();

		/// Returns the PC of the currently-executing ELF's entry point.
		u32 GetCurrentELFEntryPoint();

		/// Called when the internal frame rate changes.
		void FrameRateChanged();

		/// Throttles execution, or limits the frame rate.
		void Throttle();

		/// Resets/clears all execution/code caches.
		void ClearCPUExecutionCaches();

		const std::string& GetELFOverride();
		bool IsExecutionInterrupted();
		void ELFLoadingOnCPUThread(std::string elf_path);
		void EntryPointCompilingOnCPUThread();
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

	/// Called when a save state is loading, before the file is processed.
	void OnSaveStateLoading(const std::string_view& filename);

	/// Called after a save state is successfully loaded. If the save state was invalid, was_successful will be false.
	void OnSaveStateLoaded(const std::string_view& filename, bool was_successful);

	/// Called when a save state is being created/saved. The compression/write to disk is asynchronous, so this callback
	/// just signifies that the save has started, not necessarily completed.
	void OnSaveStateSaved(const std::string_view& filename);

	/// Provided by the host; called when the running executable changes.
	void OnGameChanged(const std::string& title, const std::string& elf_override, const std::string& disc_path,
		const std::string& disc_serial, u32 disc_crc, u32 current_crc);

	/// Provided by the host; called once per frame at guest vsync.
	void VSyncOnCPUThread();
} // namespace Host
