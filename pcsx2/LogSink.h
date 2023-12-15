/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

class SettingsInterface;

namespace LogSink
{
	/// Overrides the filename used for the file log.
	void SetFileLogPath(std::string path);

	/// Ensures file log is flushed and closed.
	void CloseFileLog();

	/// Prevents the system console from being displayed.
	void SetBlockSystemConsole(bool block);

	/// Updates the Console handler based on the current configuration.
	void UpdateLogging(SettingsInterface& si);

	/// Initializes early console logging (for printing command line arguments).
	void InitializeEarlyConsole();

	/// Stores default logging settings to the specified file.
	void SetDefaultLoggingSettings(SettingsInterface& si);
}
