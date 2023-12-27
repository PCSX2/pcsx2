// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
