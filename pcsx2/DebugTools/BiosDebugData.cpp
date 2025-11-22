// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "BiosDebugData.h"
#include "IopMem.h"
#include "Memory.h"
#include "VMManager.h"

std::vector<std::unique_ptr<BiosThread>> getEEThreads()
{
	if (!VMManager::HasValidVM() || CurrentBiosInformation.eeThreadListAddr == 0)
		return {};

	const u32 start = CurrentBiosInformation.eeThreadListAddr & 0x3fffff;

	std::vector<std::unique_ptr<BiosThread>> threads;
	for (int tid = 0; tid < 256; tid++)
	{
		EEInternalThread* internal = static_cast<EEInternalThread*>(
			PSM(start + tid * sizeof(EEInternalThread)));

		if (internal && internal->status != (int)ThreadStatus::THS_BAD)
			threads.emplace_back(std::make_unique<EEThread>(tid, *internal));
	}

	return threads;
}

std::vector<std::unique_ptr<BiosThread>> getIOPThreads()
{
	if (!VMManager::HasValidVM() || CurrentBiosInformation.iopThreadListAddr == 0)
		return {};

	std::vector<std::unique_ptr<BiosThread>> threads;

	u32 item = iopMemRead32(CurrentBiosInformation.iopThreadListAddr);

	for (int i = 0; item != 0; i++)
	{
		if (i > 1000)
			return {};

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

		threads.emplace_back(std::make_unique<IOPThread>(data));

		item = iopMemRead32(item + 0x24);
	}


	return threads;
}

std::vector<IopMod> getIOPModules()
{
	if (!VMManager::HasValidVM() || CurrentBiosInformation.iopModListAddr == 0)
		return {};

	u32 maddr = iopMemRead32(CurrentBiosInformation.iopModListAddr);
	std::vector<IopMod> modlist;

	for (int i = 0; maddr != 0; i++)
	{
		if (maddr >= 0x200000)
		{
			// outside of memory
			// change if we ever support IOP ram extension
			return {};
		}

		if (i > 1000)
			return {};

		IopMod& mod = modlist.emplace_back();

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

		maddr = iopMemRead32(maddr);
	}

	return modlist;
}
