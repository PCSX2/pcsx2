/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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
