// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/ProgressCallback.h"
#include "common/SmallString.h"

#include <ctime>
#include <string>
#include <memory>
#include <optional>

struct Pcsx2Config;

namespace FullscreenUI
{
	bool Initialize();
	bool IsInitialized();
	void ReloadSvgResources();
	bool HasActiveWindow();
	void CheckForConfigChanges(const Pcsx2Config& old_config);
	void OnVMStarted();
	void OnVMDestroyed();
	void GameChanged(std::string title, std::string path, std::string serial, u32 disc_crc, u32 crc);
	void OpenPauseMenu();
	bool OpenAchievementsWindow();
	bool OpenLeaderboardsWindow();
	void ReportStateLoadError(const std::string& message, std::optional<s32> slot, bool backup);
	void ReportStateSaveError(const std::string& message, std::optional<s32> slot);

	// NOTE: Only call from GS thread.
	bool IsAchievementsWindowOpen();
	bool IsLeaderboardsWindowOpen();
	void ReturnToPreviousWindow();
	void ReturnToMainWindow();
	void SetStandardSelectionFooterText(bool back_instead_of_cancel);
	void LocaleChanged();
	void GamepadLayoutChanged();

	void Shutdown(bool clear_state);
	void Render();
	void InvalidateCoverCache();
	TinyString TimeToPrintableString(time_t t);
	
	bool CreateHardDriveWithProgress(const std::string& filePath, int sizeInGB, bool use48BitLBA = true);
	void CancelAllHddOperations();
} // namespace FullscreenUI

// Host UI triggers from Big Picture mode.
namespace Host
{
	/// Requests shut down and exit of the hosting application. This may not actually exit,
	/// if the user cancels the shutdown confirmation.
	void RequestExitApplication(bool allow_confirm);

	/// Requests Big Picture mode to be shut down, returning to the desktop interface.
	void RequestExitBigPicture();

	void OnCoverDownloaderOpenRequested();
	void OnCreateMemoryCardOpenRequested();

	/// Did Playstation in the currently selected locale use circle as confirm
	bool LocaleCircleConfirm();
} // namespace Host
