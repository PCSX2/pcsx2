/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "ATA.h"
#include "../DEV9.h"

void ATA::IO_Thread()
{
	std::unique_lock ioWaitHandle(ioMutex);
	ioThreadIdle_bool = false;
	ioWaitHandle.unlock();

	while (true)
	{
		ioWaitHandle.lock();
		ioThreadIdle_bool = true;
		ioThreadIdle_cv.notify_all();

		ioReady.wait(ioWaitHandle, [&] { return ioRead | ioWrite; });
		ioThreadIdle_bool = false;

		int ioType = -1;
		if (ioRead)
			ioType = 0;
		else if (ioWrite)
			ioType = 1;

		ioWaitHandle.unlock();

		//Read or Write
		if (ioType == 0)
			IO_Read();
		else if (ioType == 1)
		{
			if (!IO_Write())
			{
				if (ioClose.load())
				{
					ioClose.store(false);
					ioWaitHandle.lock();
					ioThreadIdle_bool = true;
					ioWaitHandle.unlock();
					return;
				}
			}
		}
	}
}

void ATA::IO_Read()
{
	const s64 lba = HDD_GetLBA();

	if (lba == -1)
	{
		Console.Error("ATA: Invalid LBA");
		pxAssert(false);
		abort();
	}

	const u64 pos = lba * 512;
	hddImage.seekg(pos, std::ios::beg);
	if (hddImage.fail())
	{
		Console.Error("ATA: File read error");
		pxAssert(false);
		abort();
	}
	hddImage.read((char*)readBuffer, (u64)nsector * 512);
	{
		std::lock_guard ioSignallock(ioMutex);
		ioRead = false;
	}
}

bool ATA::IO_Write()
{
	u64 sector = 0;
	u8* data = nullptr;
	u32 len = 0;
	if (!DequeueWrite(&sector, &data, &len))
	{
		std::lock_guard ioSignallock(ioMutex);
		ioWrite = false;
		return false;
	}

	hddImage.seekp(sector * 512, std::ios::beg);
	hddImage.write((char*)data, len);
	if (hddImage.fail())
	{
		Console.Error("ATA: File write error");
		pxAssert(false);
		abort();
	}
	hddImage.flush();
	delete[] data;
	return true;
}

void ATA::HDD_ReadAsync(void (ATA::*drqCMD)())
{
	nsectorLeft = 0;

	if (!HDD_CanAssessOrSetError())
		return;

	nsectorLeft = nsector;
	if (readBufferLen < nsector * 512)
	{
		delete readBuffer;
		readBuffer = new u8[nsector * 512];
		readBufferLen = nsector * 512;
	}
	waitingCmd = drqCMD;

	{
		std::lock_guard ioSignallock(ioMutex);
		ioRead = true;
	}
	ioReady.notify_all();
}

//Note, we don't expect both Async & Sync Reads
//Do one of the other
void ATA::HDD_ReadSync(void (ATA::*drqCMD)())
{
	//unique_lock instead of lock_guard as also used for cv
	std::unique_lock ioWaitHandle(ioMutex);
	//Set ioWrite false to prevent reading & writing at the same time
	const bool ioWritePaused = ioWrite;
	ioWrite = false;

	//wait until thread waiting
	ioThreadIdle_cv.wait(ioWaitHandle, [&] { return ioThreadIdle_bool; });
	ioWaitHandle.unlock();

	nsectorLeft = 0;

	if (!HDD_CanAssessOrSetError())
	{
		if (ioWritePaused)
		{
			ioWaitHandle.lock();
			ioWrite = true;
			ioWaitHandle.unlock();
			ioReady.notify_all();
		}
		return;
	}

	nsectorLeft = nsector;
	if (readBufferLen < nsector * 512)
	{
		delete[] readBuffer;
		readBuffer = new u8[nsector * 512];
		readBufferLen = nsector * 512;
	}

	IO_Read();

	if (ioWritePaused)
	{
		ioWaitHandle.lock();
		ioWrite = true;
		ioWaitHandle.unlock();
		ioReady.notify_all();
	}

	(this->*drqCMD)();
}

bool ATA::HDD_CanAssessOrSetError()
{
	if (!HDD_CanAccess(&nsector))
	{
		//Read what we can
		regStatus |= (u8)ATA_STAT_ERR;
		regError |= (u8)ATA_ERR_ID;
		if (nsector == -1)
		{
			PostCmdNoData();
			return false;
		}
	}
	return true;
}
void ATA::HDD_SetErrorAtTransferEnd()
{
	u64 currSect = HDD_GetLBA();
	currSect += nsector;
	if ((regStatus & ATA_STAT_ERR) != 0)
	{
		//Error condition
		//Write errored sector to LBA
		currSect++;
		HDD_SetLBA(currSect);
	}
}

//Used by EE thread only
void ATA::QueueWrite(u64 sector, u8* data, u32 length)
{
	WriteQueueEntry* newEntry = head;
	newEntry->data = data;
	newEntry->length = length;
	newEntry->sector = sector;

	//Allocate Next entry
	newEntry->next = new WriteQueueEntry();
	head = newEntry->next;
	//Set ready
	newEntry->ready.store(true);
}

//Used by IO thread only
bool ATA::DequeueWrite(u64* sector, u8** data, u32* length)
{
	if (!tail->ready.load())
		return false;

	WriteQueueEntry* entry = tail;
	tail = entry->next;

	*sector = entry->sector;
	*data = entry->data;
	*length = entry->length;
	delete entry;
	return true;
}

//Used by EE thread only
bool ATA::IsQueueEmpty()
{
	return !tail->ready.load();
}
