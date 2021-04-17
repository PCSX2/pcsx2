/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "App.h"
#include "ConsoleLogger.h"
#include "DebugTools/Debug.h"
#include "Utilities/Console.h"
#include "Utilities/StringUtils.h"

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <memory>
#include <string>
#include <vector>

namespace inputRec
{
	static void consoleLog(const std::string log_utf8)
	{
		if (log_utf8.empty())
			return;

#ifdef _WIN32
		// TODO - the console logger does not properly support UTF-8, must widen first for proper displaying on windows
		recordingConLog(StringUtils::UTF8::widen(fmt::format("[REC]: {}\n", log_utf8)));
#else
		recordingConLog(fmt::format("[REC]: {}\n", log_utf8));
#endif
	}

	static void consoleMultiLog(std::vector<std::string> logs_utf8)
	{
		std::string log;
		for (std::string l : logs_utf8)
			log.append(fmt::format("[REC]: {}\n", l));

#ifdef _WIN32
		// TODO - the console logger does not properly support UTF-8, must widen first for proper displaying on windows
		recordingConLog(StringUtils::UTF8::widen(log));
#else
		recordingConLog(log);
#endif
	}

	static void log(const std::string log_utf8)
	{
		if (log_utf8.empty())
			return;

		consoleLog(log_utf8);

		// NOTE - Color is not currently used for OSD logs
		if (GSosdLog)
			GSosdLog(log_utf8.c_str(), wxGetApp().GetProgramLog()->GetRGBA(ConsoleColors::Color_StrongMagenta));
	}
} // namespace inputRec
