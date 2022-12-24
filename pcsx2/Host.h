/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include <ctime>
#include <functional>
#include <string>
#include <string_view>
#include <optional>
#include <vector>

namespace Host
{
	/// Typical durations for OSD messages.
	static constexpr float OSD_CRITICAL_ERROR_DURATION = 20.0f;
	static constexpr float OSD_ERROR_DURATION = 15.0f;
	static constexpr float OSD_WARNING_DURATION = 10.0f;
	static constexpr float OSD_INFO_DURATION = 5.0f;
	static constexpr float OSD_QUICK_DURATION = 2.5f;

	/// Reads a file from the resources directory of the application.
	/// This may be outside of the "normal" filesystem on platforms such as Mac.
	std::optional<std::vector<u8>> ReadResourceFile(const char* filename);

	/// Reads a resource file file from the resources directory as a string.
	std::optional<std::string> ReadResourceFileToString(const char* filename);

	/// Returns the modified time of a resource.
	std::optional<std::time_t> GetResourceFileTimestamp(const char* filename);

	/// Adds OSD messages, duration is in seconds.
	void AddOSDMessage(std::string message, float duration = 2.0f);
	void AddKeyedOSDMessage(std::string key, std::string message, float duration = 2.0f);
	void AddIconOSDMessage(std::string key, const char* icon, const std::string_view& message, float duration = 2.0f);
	void AddFormattedOSDMessage(float duration, const char* format, ...);
	void AddKeyedFormattedOSDMessage(std::string key, float duration, const char* format, ...);
	void RemoveKeyedOSDMessage(std::string key);
	void ClearOSDMessages();

	/// Displays an asynchronous error on the UI thread, i.e. doesn't block the caller.
	void ReportErrorAsync(const std::string_view& title, const std::string_view& message);
	void ReportFormattedErrorAsync(const std::string_view& title, const char* format, ...);

	/// Displays a synchronous confirmation on the UI thread, i.e. blocks the caller.
	bool ConfirmMessage(const std::string_view& title, const std::string_view& message);
	bool ConfirmFormattedMessage(const std::string_view& title, const char* format, ...);

	/// Opens a URL, using the default application.
	void OpenURL(const std::string_view& url);

	/// Copies the provided text to the host's clipboard, if present.
	bool CopyTextToClipboard(const std::string_view& text);

	/// Requests settings reset. Can be called from any thread, will call back and apply on the CPU thread.
	bool RequestResetSettings(bool folders, bool core, bool controllers, bool hotkeys, bool ui);

	/// Requests a specific display window size.
	void RequestResizeHostDisplay(s32 width, s32 height);

	/// Safely executes a function on the VM thread.
	void RunOnCPUThread(std::function<void()> function, bool block = false);

	/// Asynchronously starts refreshing the game list.
	void RefreshGameListAsync(bool invalidate_cache);

	/// Cancels game list refresh, if there is one in progress.
	void CancelGameListRefresh();

	/// Requests shut down and exit of the hosting application. This may not actually exit,
	/// if the user cancels the shutdown confirmation.
	void RequestExit(bool save_state_if_running);

	/// Requests shut down of the current virtual machine.
	void RequestVMShutdown(bool allow_confirm, bool allow_save_state, bool default_save_state);

	/// Returns true if the hosting application is currently fullscreen.
	bool IsFullscreen();

	/// Alters fullscreen state of hosting application.
	void SetFullscreen(bool enabled);
} // namespace Host
