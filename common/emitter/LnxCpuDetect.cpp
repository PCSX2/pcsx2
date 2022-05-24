/*  Cpudetection lib
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#ifndef _WIN32
#include "common/emitter/cpudetect_internal.h"

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

// Not implemented yet for linux (see cpudetect_internal.h for details)
SingleCoreAffinity::SingleCoreAffinity() = default;
SingleCoreAffinity::~SingleCoreAffinity() = default;
#endif
