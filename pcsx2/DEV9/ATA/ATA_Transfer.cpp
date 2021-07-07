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
#include "DEV9/DEV9.h"

#if _WIN32
#include <Windows.h>
#elif defined(__POSIX__)
#if defined(__APPLE__)
#include <unistd.h>
#endif
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

	u64 imagePos = entry.sector * 512;
	hddImage.seekp(imagePos, std::ios::beg);
	if (hddSparse)
	{
		int written = 0;
		while (written != entry.length)
		{
			IO_SparseCacheUpdateLocation(imagePos + written);
			//Align to sparse block size
			u32 writeSize = hddSparseBlockSize - ((imagePos + written) % hddSparseBlockSize);
			//Limit to size of write
			writeSize = std::min(writeSize, entry.length - written);

			pxAssert(writeSize > 0);
			pxAssert(writeSize <= hddSparseBlockSize);
			pxAssert((imagePos + written) - HddSparseStart >= 0);
			pxAssert((imagePos + written) - HddSparseStart + writeSize <= hddSparseBlockSize);

			if (IsAllZero(&entry.data[written], writeSize))
			{
#ifdef _DEBUG
				std::unique_ptr<u8[]> zeroBlock = std::make_unique<u8[]>(writeSize);
				memset(zeroBlock.get(), 0, writeSize);
				pxAssert(memcmp(&entry.data[written], zeroBlock.get(), writeSize) == 0);
#endif

				if (!IO_SparseZero(imagePos + written, writeSize))
				{
					Console.Error("DEV9: ATA: File sparse write error");

#ifdef _WIN32
					CloseHandle(hddNativeHandle);
#elif defined(__POSIX__)
					close(hddNativeHandle);
#endif
					hddSparse = false;
					hddSparseBlock = nullptr;
					hddSparseBlockValid = false;

					hddImage.write((char*)&entry.data[written], writeSize);
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
#ifdef _DEBUG
				std::unique_ptr<u8[]> zeroBlock = std::make_unique<u8[]>(writeSize);
				memset(zeroBlock.get(), 0, writeSize);
				pxAssert(memcmp(&entry.data[written], zeroBlock.get(), writeSize) != 0);
#endif
				//Update Cache
				if (hddSparseBlockValid)
					memcpy(&hddSparseBlock[(imagePos + written) - HddSparseStart], &entry.data[written], writeSize);

				hddImage.write((char*)&entry.data[written], writeSize);
				if (hddImage.fail())
				{
					Console.Error("DEV9: ATA: File write error");
					pxAssert(false);
					abort();
				}
			}
			written += writeSize;
			pxAssert(hddImage.tellp() == (imagePos + written));
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

void ATA::IO_SparseCacheLoad()
{
	//Reads are bounds checked, but for the sectors read only
	//Need to bounds check for sparse block, to handle an edge case of a user providing a file with a size that dosn't align with the sparse block size
	//Normally that won't happen as we generate files of exact Gib size
	u64 readSize = hddSparseBlockSize;
	const u64 posEnd = HddSparseStart + hddSparseBlockSize;
	if (posEnd > hddImageSize)
	{
		readSize = hddSparseBlockSize - (posEnd - hddImageSize);
		//Zero cache for data beyond end of file
		memset(&hddSparseBlock[readSize], 0, hddSparseBlockSize - readSize);
	}

	//Store file pointer
	std::streampos orgPos = hddImage.tellp();
	//Load into cache
	hddImage.seekg(HddSparseStart, std::ios::beg);
	//Suppress Check buffer boundaries warning
	hddImage.read((char*)hddSparseBlock.get(), readSize); /* Flawfinder: ignore */
	if (hddImage.fail())
	{
		Console.Error("DEV9: ATA: File read error");
		pxAssert(false);
		abort();
	}

	hddSparseBlockValid = true;

	//Restore file pointer
	hddImage.seekp(orgPos, std::ios::beg);
}

void ATA::IO_SparseCacheUpdateLocation(u64 byteOffset)
{
	u64 currentBlockStart = (byteOffset / hddSparseBlockSize) * hddSparseBlockSize;
	if (currentBlockStart != HddSparseStart)
	{
		HddSparseStart = currentBlockStart;
		hddSparseBlockValid = false;
		//Only update cache when we perform a sparse write
	}
}

//Also sets hddImage write ptr
bool ATA::IO_SparseZero(u64 byteOffset, u64 byteSize)
{
	if (hddSparseBlockValid == false)
		IO_SparseCacheLoad();

	//Assert as range check
	pxAssert(byteOffset - HddSparseStart >= 0);
	pxAssert(byteOffset - HddSparseStart + byteSize <= hddSparseBlockSize);

	//Write to cache
	memset(&hddSparseBlock[byteOffset - HddSparseStart], 0, byteSize);

	//Is block non-zero?
	if (!IsAllZero(hddSparseBlock.get(), hddSparseBlockSize))
	{
#ifdef _DEBUG
		std::unique_ptr<u8[]> zeroBlock = std::make_unique<u8[]>(hddSparseBlockSize);
		memset(zeroBlock.get(), 0, hddSparseBlockSize);
		pxAssert(memcmp(hddSparseBlock.get(), zeroBlock.get(), hddSparseBlockSize) != 0);
#endif

		//No, do normal write
		hddImage.write((char*)&hddSparseBlock[byteOffset - HddSparseStart], byteSize);
		if (hddImage.fail())
		{
			Console.Error("DEV9: ATA: File write error");
			pxAssert(false);
			abort();
		}
		hddImage.flush();
		return true;
	}

#ifdef _DEBUG
	std::unique_ptr<u8[]> zeroBlock = std::make_unique<u8[]>(hddSparseBlockSize);
	memset(zeroBlock.get(), 0, hddSparseBlockSize);
	pxAssert(memcmp(hddSparseBlock.get(), zeroBlock.get(), hddSparseBlockSize) == 0);
#endif

	//Yes, try sparse write
#ifdef _WIN32
	FILE_ZERO_DATA_INFORMATION sparseRange;
	sparseRange.FileOffset.QuadPart = HddSparseStart;
	sparseRange.BeyondFinalZero.QuadPart = HddSparseStart + hddSparseBlockSize;
	DWORD dwTemp;
	BOOL ret = DeviceIoControl(hddNativeHandle, FSCTL_SET_ZERO_DATA, &sparseRange, sizeof(sparseRange), nullptr, 0, &dwTemp, nullptr);

	if (ret == FALSE)
		return false;

#elif defined(__linux__)
	const int ret = fallocate(hddNativeHandle, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, HddSparseStart, hddSparseBlockSize);

	if (ret == -1)
		return false;

#elif defined(__APPLE__)
	fpunchhole_t sparseRange{0};
	sparseRange.fp_offset = HddSparseStart;
	sparseRange.fp_length = hddSparseBlockSize;

	const int ret = fcntl(hddNativeHandle, F_PUNCHHOLE, &sparseRange);

	if (ret == -1)
		return false;

#else
	return false;
#endif
	hddImage.seekp(byteOffset + byteSize, std::ios::beg);
	return true;
}

bool ATA::IsAllZero(const void* data, size_t len)
{
	intmax_t* pbi = (intmax_t*)data;
	intmax_t* pbiUpper = ((intmax_t*)(((char*)data) + len)) - 1;
	for (; pbi <= pbiUpper; pbi++)
		if (*pbi)
			return false; /* check with the biggest int available most of the array, but without aligning it */
	for (char* p = (char*)pbi; p < ((char*)data) + len; p++)
		if (*p)
			return false; /* check end of non aligned array */
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
