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

#pragma once

#include "common/RedtapeWindows.h"
#include "common/emitter/tools.h"

// --------------------------------------------------------------------------------------
//  Thread Affinity Lock
// --------------------------------------------------------------------------------------
// Assign a single CPU/core for this thread's affinity to ensure rdtsc() accuracy.
// (rdtsc for each CPU/core can differ, causing skewed results)

class SingleCoreAffinity
{
protected:
#ifdef _WIN32
	HANDLE s_threadId;
	DWORD_PTR s_oldmask;
#endif

public:
	SingleCoreAffinity();
	virtual ~SingleCoreAffinity();
};
