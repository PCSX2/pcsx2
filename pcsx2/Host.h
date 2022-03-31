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

#include <string>
#include <string_view>
#include <optional>
#include <vector>

struct HostKeyEvent
{
	enum class Type
	{
		NoEvent,
		KeyPressed,
		KeyReleased,
		MousePressed,
		MouseReleased,
		MouseWheelDown,
		MouseWheelUp,
		MouseMove,
		FocusGained,
		FocustLost,
	};

	Type type;
	u32 key;
};

namespace Host
{
	/// Reads a file from the resources directory of the application.
	/// This may be outside of the "normal" filesystem on platforms such as Mac.
	std::optional<std::vector<u8>> ReadResourceFile(const char* filename);

	/// Reads a resource file file from the resources directory as a string.
	std::optional<std::string> ReadResourceFileToString(const char* filename);

	/// Adds OSD messages, duration is in seconds.
	void AddOSDMessage(std::string message, float duration = 2.0f);
	void AddKeyedOSDMessage(std::string key, std::string message, float duration = 2.0f);
	void AddFormattedOSDMessage(float duration, const char* format, ...);
	void AddKeyedFormattedOSDMessage(std::string key, float duration, const char* format, ...);
	void RemoveKeyedOSDMessage(std::string key);
	void ClearOSDMessages();

	/// Displays an asynchronous error on the UI thread, i.e. doesn't block the caller.
	void ReportErrorAsync(const std::string_view& title, const std::string_view& message);
	void ReportFormattedErrorAsync(const std::string_view& title, const char* format, ...);
} // namespace Host
