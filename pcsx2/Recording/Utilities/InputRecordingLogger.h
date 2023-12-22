// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace InputRec
{
	void log(const std::string& log);
	void consoleLog(const std::string& log);
	void consoleMultiLog(const std::vector<std::string>& logs);
} // namespace inputRec
