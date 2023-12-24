// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/Assertions.h"
#include "common/FileSystem.h"

#include "ATA.h"
#include "DEV9/DEV9.h"

#if __POSIX__
#define INVALID_HANDLE_VALUE -1
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
	if (FileSystem::FSeek64(hddImage, pos, SEEK_SET) != 0 ||
		std::fread(readBuffer,  512, nsector, hddImage) != static_cast<size_t>(nsector))
	{
		Console.Error("DEV9: ATA: File read error");
		pxAssert(false);
		abort();
	}
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

	const u64 imagePos = entry.sector * 512;
	if (FileSystem::FSeek64(hddImage, imagePos, SEEK_SET) != 0)
	{
		Console.Error("DEV9: ATA: File seek error");
		pxAssert(false);
		abort();
	}
	if (hddSparse)
	{
		u32 written = 0;
		while (written != entry.length)
		{
			IO_SparseCacheUpdateLocation(imagePos + written);
			// Align to sparse block size.
			u32 writeSize = hddSparseBlockSize - ((imagePos + written) % hddSparseBlockSize);
			// Limit to size of write.
			writeSize = std::min(writeSize, entry.length - written);

			pxAssert(writeSize > 0);
			pxAssert(writeSize <= hddSparseBlockSize);
			pxAssert((imagePos + written) >= HddSparseStart);
			pxAssert((imagePos + written) - HddSparseStart + writeSize <= hddSparseBlockSize);

			bool sparseWrite = IsAllZero(&entry.data[written], writeSize);

			if (sparseWrite)
			{
#if defined(PCSX2_DEBUG) || defined(PCSX2_DEVBUILD)
				std::unique_ptr<u8[]> zeroBlock = std::make_unique<u8[]>(writeSize);
				memset(zeroBlock.get(), 0, writeSize);
				pxAssert(memcmp(&entry.data[written], zeroBlock.get(), writeSize) == 0);
#endif

				if (!IO_SparseZero(imagePos + written, writeSize))
				{
					Console.Error("DEV9: ATA: File sparse write error");

					// hddNativeHandle is owned by hddImage.
					// do not close it.
					hddNativeHandle = INVALID_HANDLE_VALUE;

					hddSparse = false;
					hddSparseBlock = nullptr;
					hddSparseBlockValid = false;

					// Fallthough into other if statment.
					sparseWrite = false;
				}
			}

			// Also handles sparse write failures.
			if (!sparseWrite)
			{
#if defined(PCSX2_DEBUG) || defined(PCSX2_DEVBUILD)
				if (hddSparse)
				{
					std::unique_ptr<u8[]> zeroBlock = std::make_unique<u8[]>(writeSize);
					memset(zeroBlock.get(), 0, writeSize);
					pxAssert(memcmp(&entry.data[written], zeroBlock.get(), writeSize) != 0);
				}
#endif
				// Update cache.
				if (hddSparseBlockValid)
					memcpy(&hddSparseBlock[(imagePos + written) - HddSparseStart], &entry.data[written], writeSize);

				if (std::fwrite(&entry.data[written], writeSize, 1, hddImage) != 1 ||
					std::fflush(hddImage) != 0)
				{
					Console.Error("DEV9: ATA: File write error");
					pxAssert(false);
					abort();
				}
			}
			written += writeSize;
			pxAssert(FileSystem::FTell64(hddImage) == (s64)(imagePos + written));
		}
	}
	else
	{
		if (std::fwrite(entry.data, entry.length, 1, hddImage) != 1 || std::fflush(hddImage) != 0)
		{
			Console.Error("DEV9: ATA: File write error");
			pxAssert(false);
			abort();
		}
	}
	delete[] entry.data;
	return true;
}

void ATA::IO_SparseCacheLoad()
{
	// Reads are bounds checked, but for the sectors read only.
	// Need to bounds check for sparse block, to handle an edge case of a user providing a file with a size that dosn't align with the sparse block size.
	// Normally that won't happen as we generate files of exact Gib size.
	u64 readSize = hddSparseBlockSize;
	const u64 posEnd = HddSparseStart + hddSparseBlockSize;
	if (posEnd > hddImageSize)
	{
		readSize = hddSparseBlockSize - (posEnd - hddImageSize);
		// Zero cache for data beyond end of file.
		memset(&hddSparseBlock[readSize], 0, hddSparseBlockSize - readSize);
	}

	// Flush so that we know what is allocated.
	std::fflush(hddImage);

	// Store file pointer.
	const s64 orgPos = FileSystem::FTell64(hddImage);

#ifdef _WIN32
	// FlushFileBuffers is required, hddSparseBlock differs from actual file without it.
	FlushFileBuffers(hddNativeHandle);
	// Range to be examined (One Sparse block size).
	FILE_ALLOCATED_RANGE_BUFFER queryRange;
	queryRange.FileOffset.QuadPart = HddSparseStart;
	queryRange.Length.QuadPart = hddSparseBlockSize;

	// Allocated areas info.
	FILE_ALLOCATED_RANGE_BUFFER allocRange;
	DWORD dwRetBytes;
	const BOOL ret = DeviceIoControl(hddNativeHandle, FSCTL_QUERY_ALLOCATED_RANGES, &queryRange, sizeof(queryRange), &allocRange, sizeof(allocRange), &dwRetBytes, nullptr);

	if (ret == TRUE && dwRetBytes == 0)
	{
		// We are sparse.
		memset(hddSparseBlock.get(), 0, hddSparseBlockSize);
		hddSparseBlockValid = true;
#if defined(PCSX2_DEBUG) || defined(PCSX2_DEVBUILD)
		ATA::IO_SparseCacheAssertFileZeros(readSize);
#endif
		return;
	}
#elif defined(__POSIX__)
#ifdef SEEK_HOLE
	// Are we in a hole?
	off_t ret = lseek(hddNativeHandle, HddSparseStart, SEEK_HOLE);
	if (ret == (off_t)HddSparseStart)
	{
		// Seek to data.
		ret = lseek(hddNativeHandle, HddSparseStart, SEEK_DATA);
		if (ret >= (off_t)(HddSparseStart + hddSparseBlockSize))
		{
			// We are sparse.
			memset(hddSparseBlock.get(), 0, hddSparseBlockSize);
			hddSparseBlockValid = true;
#if defined(PCSX2_DEBUG) || defined(PCSX2_DEVBUILD)
			ATA::IO_SparseCacheAssertFileZeros(readSize);
#endif
			return;
		}
	}
#endif
#endif

	// Load into cache.
	if (orgPos == -1 ||
		FileSystem::FSeek64(hddImage, HddSparseStart, SEEK_SET) != 0 ||
		std::fread((char*)hddSparseBlock.get(), readSize, 1, hddImage) != 1 ||
		FileSystem::FSeek64(hddImage, orgPos, SEEK_SET) != 0) // Restore file pointer.
	{
		Console.Error("DEV9: ATA: File read error");
		pxAssert(false);
		abort();
	}

	hddSparseBlockValid = true;
}

#if defined(PCSX2_DEBUG) || defined(PCSX2_DEVBUILD)
// Asserts that the region of file indicated by HddSparseStart & hddSparseBlockSizeReadable is all zeros
// Used by IO_SparseCacheLoad to ensure the sparse/allocated apis and FileSystem apis are in sync
void ATA::IO_SparseCacheAssertFileZeros(u64 hddSparseBlockSizeReadable)
{
	const s64 orgPos = FileSystem::FTell64(hddImage);
	pxAssert(orgPos != -1);

	// Load into check buffer.
	FileSystem::FSeek64(hddImage, HddSparseStart, SEEK_SET);

	std::unique_ptr<u8[]> temp = std::make_unique<u8[]>(hddSparseBlockSize);
	memset(temp.get(), 0, hddSparseBlockSize);

	if (FileSystem::FSeek64(hddImage, HddSparseStart, SEEK_SET) != 0 ||
		std::fread((char*)hddSparseBlock.get(), hddSparseBlockSizeReadable, 1, hddImage) != 1)
		pxAssert(false);

	// Restore file pointer.
	if (FileSystem::FSeek64(hddImage, orgPos, SEEK_SET) != 0)
		pxAssert(false);

	bool regionIsZeros = memcmp(hddSparseBlock.get(), temp.get(), hddSparseBlockSize) == 0;

	// Check if file is actully zeros.
	if (!regionIsZeros)
	{
		Console.WriteLn("DEV9: ATA: Sparse area not sparse, BlockStart: %s, BlockEnd: %s",
			std::to_string(HddSparseStart).c_str(), std::to_string(HddSparseStart + hddSparseBlockSize).c_str());
	}
	else
		Console.WriteLn("DEV9: ATA: Sparse area is sparse (Yay), BlockStart: %s, BlockEnd: %s",
			std::to_string(HddSparseStart).c_str(), std::to_string(HddSparseStart + hddSparseBlockSize).c_str());

	pxAssert(regionIsZeros);
}
#endif

void ATA::IO_SparseCacheUpdateLocation(u64 byteOffset)
{
	const u64 currentBlockStart = (byteOffset / hddSparseBlockSize) * hddSparseBlockSize;
	if (currentBlockStart != HddSparseStart)
	{
		HddSparseStart = currentBlockStart;
		hddSparseBlockValid = false;
		// Only update cache when we perform a sparse write.
	}
}

// Also sets hddImage write ptr.
bool ATA::IO_SparseZero(u64 byteOffset, u64 byteSize)
{
	if (hddSparseBlockValid == false)
		IO_SparseCacheLoad();

	//Assert as range check
	pxAssert(byteOffset >= HddSparseStart);
	pxAssert(byteOffset - HddSparseStart + byteSize <= hddSparseBlockSize);

	//Write to cache
	memset(&hddSparseBlock[byteOffset - HddSparseStart], 0, byteSize);

	//Is block non-zero?
	if (!IsAllZero(hddSparseBlock.get(), hddSparseBlockSize))
	{
#if defined(PCSX2_DEBUG) || defined(PCSX2_DEVBUILD)
		std::unique_ptr<u8[]> zeroBlock = std::make_unique<u8[]>(hddSparseBlockSize);
		memset(zeroBlock.get(), 0, hddSparseBlockSize);
		pxAssert(memcmp(hddSparseBlock.get(), zeroBlock.get(), hddSparseBlockSize) != 0);
#endif

		//No, do normal write
		if (std::fwrite((char*)&hddSparseBlock[byteOffset - HddSparseStart], byteSize, 1, hddImage) != 1 ||
			std::fflush(hddImage) != 0)
		{
			Console.Error("DEV9: ATA: File write error");
			pxAssert(false);
			abort();
		}
		return true;
	}

#if defined(PCSX2_DEBUG) || defined(PCSX2_DEVBUILD)
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
	const BOOL ret = DeviceIoControl(hddNativeHandle, FSCTL_SET_ZERO_DATA, &sparseRange, sizeof(sparseRange), nullptr, 0, &dwTemp, nullptr);

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
	Console.Error("DEV9: ATA: Hole punching not supported on current OS");
	return false;
#endif
	if (FileSystem::FSeek64(hddImage, byteOffset + byteSize, SEEK_SET) != 0)
	{
		Console.Error("DEV9: ATA: File seek error");
		pxAssert(false);
		abort();
	}
	return true;
}

bool ATA::IsAllZero(const void* data, size_t len)
{
	intmax_t* pbi = (intmax_t*)data;
	intmax_t* pbiUpper = ((intmax_t*)(((char*)data) + len)) - 1;
	for (; pbi <= pbiUpper; pbi++)
		if (*pbi)
			return false; // Check with the biggest int available most of the array, but without aligning it.
	for (char* p = (char*)pbi; p < ((char*)data) + len; p++)
		if (*p)
			return false; // Check end of non aligned array.
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
		regStatus |= static_cast<u8>(ATA_STAT_ERR);
		regError |= static_cast<u8>(ATA_ERR_ID);
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
		Console.Error("DEV9: ATA: Transfer from invalid LBA %lu", currSect);
	}
}
