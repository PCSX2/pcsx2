/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2014-2014  PCSX2 Dev Team
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
#include "BiosDebugData.h"
#include "Memory.h"

std::vector<EEThread> getEEThreads()
{
	std::vector<EEThread> threads;

	if (CurrentBiosInformation.threadListAddr <= 0)
		return threads;

	const u32 start = CurrentBiosInformation.threadListAddr & 0x3fffff;

	for (int tid = 0; tid < 256; tid++)
	{
		EEThread thread;

		EEInternalThread* internal = static_cast<EEInternalThread*>(PSM(start + tid * sizeof(EEInternalThread)));
		if (internal->status != THS_BAD)
		{
			thread.tid = tid;
			thread.data = *internal;
			threads.push_back(thread);
		}
	}

	return threads;
}
