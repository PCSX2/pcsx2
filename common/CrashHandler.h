// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <string_view>

#ifndef _WIN32
#include <csignal>
#endif

namespace CrashHandler
{
	bool Install();
	void SetWriteDirectory(std::string_view dump_directory);
	void WriteDumpForCaller();

#ifndef _WIN32
	// Allow crash handler to be invoked from a signal.
	void CrashSignalHandler(int signal, siginfo_t* siginfo, void* ctx);
#endif
} // namespace CrashHandler
