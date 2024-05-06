// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <string_view>

namespace CrashHandler
{
	bool Install();
	void SetWriteDirectory(std::string_view dump_directory);
	void WriteDumpForCaller();
} // namespace CrashHandler
