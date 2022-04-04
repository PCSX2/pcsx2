/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"

#include "InputRecordingLogger.h"

#include "DebugTools/Debug.h"
#include "common/Console.h"
#include "GS.h"				// GSosdlog
#include "Host.h"

#include <fmt/core.h>

namespace inputRec
{
	void log(const std::string& log)
	{
		if (log.empty())
			return;

		recordingConLog(fmt::format("[REC]: {}\n", log));

		// NOTE - Color is not currently used for OSD logs
		Host::AddOSDMessage(log, 15.0f);
	}

	void consoleLog(const std::string& log)
	{
		if (log.empty())
			return;

		recordingConLog(fmt::format("[REC]: {}\n", log));
	}

	void consoleMultiLog(const std::vector<std::string>& logs)
	{
		std::string log;
		for (std::string l : logs)
			log.append(fmt::format("[REC]: {}\n", l));

		recordingConLog(log);
	}
} // namespace inputRec
