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

#if _WIN32
#include <Windows.h>
#elif defined(__linux__)
#include <fcntl.h>
#elif defined(__APPLE__)
#include <unistd.h>
#include <fcntl.h>
#endif

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
		Console.Error("DEV9: ATA: Invalid LBA");
		pxAssert(false);
		abort();
	}

	const u64 pos = lba * 512;
	hddImage.seekg(pos, std::ios::beg);
	if (hddImage.fail())
	{
		Console.Error("DEV9: ATA: File read error");
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
	WriteQueueEntry entry;
	if (!writeQueue.Dequeue(&entry))
	{
		std::lock_guard ioSignallock(ioMutex);
		ioWrite = false;
		return false;
	}

	hddImage.seekp(entry.sector * 512, std::ios::beg);
	if (hddSparse)
	{
		//What size block is best (must fit evenly into 512 bytes)?
		const int sparseSize = 512;
		const static u8 zeroBlock[sparseSize]{0};
		for (size_t i = 0; i < entry.length; i += sparseSize)
		{
			if (memcmp(&entry.data[i], zeroBlock, sparseSize) == 0)
			{
				if (!IO_SparseWrite(entry.sector * 512 + i, sparseSize))
				{
					Console.Error("DEV9: ATA: File sparse write error");

#ifdef _WIN32
					CloseHandle(hddNativeHandle);
#elif defined(__POSIX__)
					close(hddNativeHandle);
#endif
					hddSparse = false;

					hddImage.write((char*)&entry.data[i], sparseSize);
					if (hddImage.fail())
					{
						Console.Error("DEV9: ATA: File write error");
						pxAssert(false);
						abort();
					}
				}
			}
			else
			{
				hddImage.write((char*)&entry.data[i], sparseSize);
				if (hddImage.fail())
				{
					Console.Error("DEV9: ATA: File write error");
					pxAssert(false);
					abort();
				}
			}
		}
	}
	else
	{
		hddImage.write((char*)entry.data, entry.length);
		if (hddImage.fail())
		{
			Console.Error("DEV9: ATA: File write error");
			pxAssert(false);
			abort();
		}
	}
	hddImage.flush();
	delete[] entry.data;
	return true;
}

//Also sets hddImage write ptr
bool ATA::IO_SparseWrite(u64 byteOffset, u64 byteSize)
{
#ifdef _WIN32
	FILE_ZERO_DATA_INFORMATION sparseRange;
	sparseRange.FileOffset.QuadPart = byteOffset;
	sparseRange.BeyondFinalZero.QuadPart = byteOffset + byteSize;
	DWORD dwTemp;
	const BOOL ret = DeviceIoControl(hddNativeHandle, FSCTL_SET_ZERO_DATA, &sparseRange, sizeof(sparseRange), nullptr, 0, &dwTemp, nullptr);

	if (ret == 0)
		return false;

#elif defined(__linux__)
	const int ret = fallocate(hddNativeHandle, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, byteOffset, byteSize);

	if (ret == -1)
		return false;

#elif defined(__APPLE__)
	fpunchhole_t sparseRange{0};
	sparseRange.fp_offset = byteOffset;
	sparseRange.fp_length = byteSize;

	int ret = fcntl(hddNativeHandle, F_PUNCHHOLE, &sparseRange);

	if (ret == -1)
		return false;

#else
	return false;
#endif
	hddImage.seekp(byteOffset + byteSize, std::ios::beg);
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
