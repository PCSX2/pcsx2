// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <string_view>

namespace CrashHandler
{
	bool Install();
	void SetWriteDirectory(const std::string_view& dump_directory);
	void WriteDumpForCaller();
	void Uninstall();
} // namespace CrashHandler
