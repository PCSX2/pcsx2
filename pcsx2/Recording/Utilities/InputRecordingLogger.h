// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

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
