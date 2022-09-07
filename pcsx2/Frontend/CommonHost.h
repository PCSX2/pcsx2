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
} // namespace CommonHost
