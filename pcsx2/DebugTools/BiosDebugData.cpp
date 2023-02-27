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
#include "IopMem.h"
#include "Memory.h"


std::vector<std::unique_ptr<BiosThread>> getEEThreads()
{
	std::vector<std::unique_ptr<BiosThread>> threads;

	if (CurrentBiosInformation.eeThreadListAddr <= 0)
		return threads;

	const u32 start = CurrentBiosInformation.eeThreadListAddr & 0x3fffff;

	for (int tid = 0; tid < 256; tid++)
	{

		EEInternalThread* internal = static_cast<EEInternalThread*>(PSM(start + tid * sizeof(EEInternalThread)));
		if (internal->status != (int)ThreadStatus::THS_BAD)
		{
			auto thread = std::make_unique<EEThread>(tid, *internal);
			threads.push_back(std::move(thread));
		}
	}

	return threads;
}

std::vector<std::unique_ptr<BiosThread>> getIOPThreads()
{
	std::vector<std::unique_ptr<BiosThread>> threads;

	if (CurrentBiosInformation.iopThreadListAddr <= 0)
		return threads;

	u32 item = iopMemRead32(CurrentBiosInformation.iopThreadListAddr);

	while (item != 0)
	{
		IOPInternalThread data{};

		u16 tag = iopMemRead16(item + 0x8);
		if (tag != 0x7f01)
		{
			// something went wrong
			return {};
		}

		data.stackTop = iopMemRead32(item + 0x3c);
		data.status = iopMemRead8(item + 0xc);
		data.tid = iopMemRead16(item + 0xa);
		data.entrypoint = iopMemRead32(item + 0x38);
		data.waitstate = iopMemRead16(item + 0xe);
		data.initPriority = iopMemRead16(item + 0x2e);

		data.SavedSP = iopMemRead32(item + 0x10);

		data.PC = iopMemRead32(data.SavedSP + 0x8c);

		auto thread = std::make_unique<IOPThread>(data);
		threads.push_back(std::move(thread));

		item = iopMemRead32(item + 0x24);
	}


	return threads;
}
