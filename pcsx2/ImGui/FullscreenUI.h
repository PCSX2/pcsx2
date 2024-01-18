// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/ProgressCallback.h"
#include "common/SmallString.h"

#include <ctime>
#include <string>
#include <memory>

struct Pcsx2Config;

namespace FullscreenUI
{
	bool Initialize();
	bool IsInitialized();
	bool HasActiveWindow();
	void CheckForConfigChanges(const Pcsx2Config& old_config);
	void OnVMStarted();
	void OnVMDestroyed();
	void GameChanged(std::string title, std::string path, std::string serial, u32 disc_crc, u32 crc);
	void OpenPauseMenu();
	bool OpenAchievementsWindow();
	bool OpenLeaderboardsWindow();

	// NOTE: Only call from GS thread.
	bool IsAchievementsWindowOpen();
	bool IsLeaderboardsWindowOpen();
	void ReturnToPreviousWindow();
	void ReturnToMainWindow();

	void Shutdown(bool clear_state);
	void Render();
	void InvalidateCoverCache();
	TinyString TimeToPrintableString(time_t t);
} // namespace FullscreenUI

// Host UI triggers from Big Picture mode.
namespace Host
{
void OnCoverDownloaderOpenRequested();
void OnCreateMemoryCardOpenRequested();
}
