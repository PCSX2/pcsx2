// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "InputRecordingLogger.h"

#include "DebugTools/Debug.h"
#include "common/Console.h"
#include "GS.h"
#include "Host.h"

#include <fmt/core.h>

namespace InputRec
{
	void log(const std::string& log)
	{
		if (!log.empty())
		{
			recordingConLog(fmt::format("[REC]: {}\n", log));
			Host::AddOSDMessage(log, 15.0f);
		}
	}

	void consoleLog(const std::string& log)
	{
		if (!log.empty())
		{
			recordingConLog(fmt::format("[REC]: {}\n", log));
		}
	}

	void consoleMultiLog(const std::vector<std::string>& logs)
	{
		if (!logs.empty())
		{
			std::string log;
			for (std::string l : logs)
			{
				log.append(fmt::format("[REC]: {}\n", l));
			}
			recordingConLog(log);
		}
	}
} // namespace InputRecording
