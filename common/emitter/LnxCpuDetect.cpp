// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#ifndef _WIN32
#include "common/emitter/tools.h"

#include <unistd.h>

// Note: Apparently this solution is Linux/Solaris only.
// FreeBSD/OsX need something far more complicated (apparently)
void x86capabilities::CountLogicalCores()
{
#ifdef __linux__
	// Note : GetCPUCount uses sysconf( _SC_NPROCESSORS_ONLN ) internally, which can return 1
	// if sysconf info isn't available (a long standing linux bug).  There are no fallbacks or
	// alternatives, apparently.
	LogicalCores = sysconf(_SC_NPROCESSORS_ONLN);
#else
	LogicalCores = 1;
#endif
}

#endif
