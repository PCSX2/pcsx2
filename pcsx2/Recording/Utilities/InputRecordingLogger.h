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

#include <memory>
#include <string>

namespace inputRec
{
	namespace
	{
		template <typename... Args>
		static std::string fmtStr(const std::string& format, Args... args)
		{
			size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
			if (size <= 0)
				return std::string("");

			std::unique_ptr<char[]> buf(new char[size]);
			snprintf(buf.get(), size, format.c_str(), args...);
			return std::string(buf.get(), buf.get() + size - 1);
		}
	} // namespace


	template <typename... Args>
	static void log(const std::string& format, Args... args)
	{
		std::string finalStr = fmtStr(format, std::forward<Args>(args)...);
		if (finalStr.empty())
			return;

		recordingConLog("[REC]: " + finalStr + "\n");

		// NOTE - Color is not currently used for OSD logs
		if (GSosdLog)
			GSosdLog(finalStr.c_str(), wxGetApp().GetProgramLog()->GetRGBA(ConsoleColors::Color_StrongMagenta));
	}

	template <typename... Args>
	static void consoleLog(const std::string& format, Args... args)
	{
		std::string finalStr = fmtStr(format, std::forward<Args>(args)...);
		if (finalStr.empty())
			return;

		recordingConLog(finalStr + "\n");
	}

	static void consoleMultiLog(std::vector<std::string> strs)
	{
		std::string finalStr;
		for (std::string s : strs)
			finalStr.append("[REC]: " + s + "\n");

		recordingConLog(finalStr);
	}


	static void consoleMultiLog(std::vector<wxString> strs)
	{
		std::vector<std::string> stdStrs;
		for (wxString s : strs)
			stdStrs.push_back(std::string(s));

		consoleMultiLog(stdStrs);
	}
} // namespace inputRec
