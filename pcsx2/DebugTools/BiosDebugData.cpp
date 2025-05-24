// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

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

std::vector<IopMod> getIOPModules()
{
	u32 maddr = iopMemRead32(CurrentBiosInformation.iopModListAddr);
	std::vector<IopMod> modlist;
	int i = 0;

	while (maddr != 0)
	{
		IopMod mod;

		if (maddr >= 0x200000)
		{
			// outside of memory
			// change if we ever support IOP ram extension
			return {};
		}

		if (i > 200)
		{
			// 200 modules? unlikely
			return {};
		}

		u32 nstr = iopMemRead32(maddr + 4);
		if (nstr)
		{
			mod.name = iopMemReadString(iopMemRead32(maddr + 4));
		}
		else
		{
			mod.name = "(NULL)";
		}

		mod.version = iopMemRead16(maddr + 8);
		mod.entry = iopMemRead32(maddr + 0x10);
		mod.gp = iopMemRead32(maddr + 0x14);
		mod.text_addr = iopMemRead32(maddr + 0x18);
		mod.text_size = iopMemRead32(maddr + 0x1c);
		mod.data_size = iopMemRead32(maddr + 0x20);
		mod.bss_size = iopMemRead32(maddr + 0x24);

		modlist.push_back(mod);

		maddr = iopMemRead32(maddr);
		i++;
	}

	return modlist;
}
