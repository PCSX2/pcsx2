// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/Assertions.h"
#include "common/FileSystem.h"
#include "common/StringUtil.h"

#include "ATA.h"
#include "DEV9/DEV9.h"

#if _WIN32
#include "pathcch.h"
#include <io.h>
#elif defined(__POSIX__)
#define INVALID_HANDLE_VALUE -1
#if defined(__APPLE__)
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
static_assert(sizeof(off_t) >= 8, "off_t is not 64bit");
#endif

ATA::ATA()
{
	//Power on, Would do self-Diag + Hardware Init
	ResetBegin();
	ResetEnd(true);
}

ATA::~ATA()
{
	if (hddImage)
		std::fclose(hddImage);
}

int ATA::Open(const std::string& hddPath)
{
	readBufferLen = 256 * 512;
	readBuffer = new u8[readBufferLen];
	memset(sceSec, 0, sizeof(sceSec));

	//Open File
	if (!FileSystem::FileExists(hddPath.c_str()))
		return -1;

	hddImage = FileSystem::OpenCFile(hddPath.c_str(), "r+b");
	const s64 size = hddImage ? FileSystem::FSize64(hddImage) : -1;
	if (!hddImage || size < 0)
	{
		Console.Error("Failed to open HDD image '%s'", hddPath.c_str());
		return -1;
	}

	// Open and read the content of the hddid file
	std::string hddidPath = Path::ReplaceExtension(hddPath, "hddid");
	std::optional<std::vector<u8>> fileContent = FileSystem::ReadBinaryFile(hddidPath.c_str());

	if (fileContent.has_value() && fileContent.value().size() <= sizeof(sceSec))
	{
		// Copy the content to sceSec
		std::copy(fileContent.value().begin(), fileContent.value().end(), sceSec);
	}
	else
	{
		// fill sceSec with default data if hdd id file is not present
		memcpy(sceSec, "Sony Computer Entertainment Inc.", 32); // Always this magic header.
		memcpy(sceSec + 0x20, "SCPH-20401", 10); // sometimes this matches HDD model, the rest 6 bytes filles with zeroes, or sometimes with spaces
		memcpy(sceSec + 0x30, "  40", 4); // or " 120" for PSX DESR, reference for ps2 area size. The rest bytes filled with zeroes

		sceSec[0x40] = 0; // 0x40 - 0x43 - 4-byte HDD internal SCE serial, does not match real HDD serial, currently hardcoded to 0x1000000
		sceSec[0x41] = 0;
		sceSec[0x42] = 0;
		sceSec[0x43] = 0x01;

		// purpose of next 12 bytes is unknown
		sceSec[0x44] = 0; // always zero
		sceSec[0x45] = 0; // always zero
		sceSec[0x46] = 0x1a;
		sceSec[0x47] = 0x01;
		sceSec[0x48] = 0x02;
		sceSec[0x49] = 0x20;
		sceSec[0x4a] = 0; // always zero
		sceSec[0x4b] = 0; // always zero
		// next 4 bytes always these values
		sceSec[0x4c] = 0x01;
		sceSec[0x4d] = 0x03;
		sceSec[0x4e] = 0x11;
		sceSec[0x4f] = 0x01;
		// 0x50 - 0x80 is a random unique block of data
		// 0x80 and up - zero filled
	}

	//Store HddImage size for later use
	hddImageSize = static_cast<u64>(size);
	CreateHDDinfo(hddImageSize / 512);

	InitSparseSupport(hddPath);

	{
		std::lock_guard ioSignallock(ioMutex);
		ioRead = false;
		ioWrite = false;
	}

	ioThread = std::thread(&ATA::IO_Thread, this);
	ioRunning = true;

	return 0;
}

void ATA::InitSparseSupport(const std::string& hddPath)
{
#ifdef _WIN32
	hddSparse = false;

	const std::wstring wHddPath(StringUtil::UTF8StringToWideString(hddPath));
	const DWORD fileAttributes = GetFileAttributes(wHddPath.c_str());
	hddSparse = fileAttributes & FILE_ATTRIBUTE_SPARSE_FILE;

	if (!hddSparse)
		return;

	// Get OS specific file handle for spare writing.
	// HANDLE is owned by FILE* hddImage.
	hddNativeHandle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(hddImage)));
	if (hddNativeHandle == INVALID_HANDLE_VALUE)
	{
		Console.Error("DEV9: ATA: Failed to open file for sparse");
		hddSparse = false;
		return;
	}

	// Get sparse block size (Initially assumed as 4096 bytes).
	hddSparseBlockSize = 4096;

	// We need the drive letter for the drive the file actually resides on
	// which means we need to deal with any junction links in the path.
	DWORD len = GetFinalPathNameByHandle(hddNativeHandle, nullptr, 0, FILE_NAME_NORMALIZED);

	if (len != 0)
	{
		std::unique_ptr<TCHAR[]> name = std::make_unique<TCHAR[]>(len);
		len = GetFinalPathNameByHandle(hddNativeHandle, name.get(), len, FILE_NAME_NORMALIZED);
		if (len != 0)
		{
			PCWSTR rootEnd;
			if (PathCchSkipRoot(name.get(), &rootEnd) == S_OK)
			{
				const size_t rootLength = rootEnd - name.get();
				std::wstring finalPath(name.get(), rootLength);

				DWORD sectorsPerCluster;
				DWORD bytesPerSector;
				DWORD temp1, temp2;
				if (GetDiskFreeSpace(finalPath.c_str(), &sectorsPerCluster, &bytesPerSector, &temp1, &temp2) == TRUE)
					hddSparseBlockSize = sectorsPerCluster * bytesPerSector;
				else
					Console.Error("DEV9: ATA: Failed to get sparse block size (GetDiskFreeSpace() returned false)");
			}
			else
				Console.Error("DEV9: ATA: Failed to get sparse block size (PathCchSkipRoot() returned false)");
		}
		else
			Console.Error("DEV9: ATA: Failed to get sparse block size (PathBuildRoot() returned 0)");
	}
	else
		Console.Error("DEV9: ATA: Failed to get sparse block size (GetFinalPathNameByHandle() returned 0)");

	/*  https://askbob.tech/the-ntfs-blog-sparse-and-compressed-file/
	 *  NTFS Sparse Block Size are the same size as a compression unit
	 *  Cluster Size    Compression Unit
	 *  --------------------------------
	 *  512bytes         8kb (0x02000)
	 *    1kb           16kb (0x04000)
	 *    2kb           32kb (0x08000)
	 *    4kb           64kb (0x10000)
	 *    8kb           64kb (0x10000)
	 *   16kb           64kb (0x10000)
	 *   32kb           64kb (0x10000)
	 *   64kb           64kb (0x10000)
	 *  --------------------------------
	 */

	// Get the filesystem type.
	WCHAR fsName[MAX_PATH + 1];
	const BOOL ret = GetVolumeInformationByHandleW(hddNativeHandle, nullptr, 0, nullptr, nullptr, nullptr, fsName, MAX_PATH);
	if (ret == FALSE)
	{
		Console.Error("DEV9: ATA: Failed to get sparse block size (GetVolumeInformationByHandle() returned false)");
		// Assume NTFS.
		wcscpy(fsName, L"NTFS");
	}
	if ((wcscmp(fsName, L"NTFS") == 0))
	{
		switch (hddSparseBlockSize)
		{
			case 512:
				hddSparseBlockSize = 8192;
				break;
			case 1024:
				hddSparseBlockSize = 16384;
				break;
			case 2048:
				hddSparseBlockSize = 32768;
				break;
			case 4096:
			case 8192:
			case 16384:
			case 32768:
			case 65536:
				hddSparseBlockSize = 65536;
				break;
			default:
				break;
		}
	}
	// Otherwise assume SparseBlockSize == block size.

#elif defined(__POSIX__)
	// fd is owned by FILE* hddImage.
	hddNativeHandle = fileno(hddImage);
	hddSparse = false;
	if (hddNativeHandle != -1)
	{
		// No way to check if we can hole punch without trying it
		// so just assume sparse files are supported.
		hddSparse = true;

		// Get sparse block size (Initially assumed as 4096 bytes).
		hddSparseBlockSize = 4096;
		struct stat fileInfo;
		if (fstat(hddNativeHandle, &fileInfo) == 0)
			hddSparseBlockSize = fileInfo.st_blksize;
		else
			Console.Error("DEV9: ATA: Failed to get sparse block size (fstat returned != 0)");
	}
	else
		Console.Error("DEV9: ATA: Failed to open file for sparse");
#endif
	hddSparseBlock = std::make_unique<u8[]>(hddSparseBlockSize);
	hddSparseBlockValid = false;
}

void ATA::Close()
{
	//Wait for async code to finish
	if (ioRunning)
	{
		ioClose.store(true);
		{
			std::lock_guard ioSignallock(ioMutex);
			ioWrite = true;
		}
		ioReady.notify_all();

		ioThread.join();
		ioRunning = false;
	}

	//verify queue
	if (!writeQueue.IsQueueEmpty())
	{
		Console.Error("DEV9: ATA: Write queue not empty, possible data loss");
		pxAssert(false);
		abort(); //All data must be written at this point
	}

	//Close File Handle
	if (hddSparse)
	{
		// hddNativeHandle is owned by hddImage.
		// It will get closed in fclose(hddImage).
		hddNativeHandle = INVALID_HANDLE_VALUE;

		hddSparse = false;
		hddSparseBlock = nullptr;
		hddSparseBlockValid = false;
	}
	if (hddImage)
	{
		std::fclose(hddImage);
		hddImage = nullptr;
	}

	delete[] readBuffer;
	readBuffer = nullptr;
}

void ATA::ResetBegin()
{
	PreCmdExecuteDeviceDiag();
}
void ATA::ResetEnd(bool hard)
{
	curHeads = 16;
	curSectors = 63;
	curCylinders = 0;
	curMultipleSectorsSetting = 128;

	//UDMA Mode setting is preserved
	//across SRST
	if (hard)
	{
		pioMode = 4;
		mdmaMode = 2;
		udmaMode = -1;
	}
	else
	{
		pioMode = 4;
		if (udmaMode == -1)
			mdmaMode = 2;
	}

	regControlEnableIRQ = false;
	HDD_ExecuteDeviceDiag();
	regControlEnableIRQ = true;
}

void ATA::ATA_HardReset()
{
	//DevCon.WriteLn("DEV9: *ATA_HARD RESET");
	ResetBegin();
	ResetEnd(true);
}

u16 ATA::Read16(u32 addr)
{
	switch (addr)
	{
		case ATA_R_DATA:
			return ATAreadPIO();
		case ATA_R_ERROR:
			//DevCon.WriteLn("DEV9: *ATA_R_ERROR 16bit read at address %x, value %x, Active %s", addr, regError, (GetSelectedDevice() == 0) ? "True" : "False");
			if (GetSelectedDevice() != 0)
				return 0;
			return regError;
		case ATA_R_NSECTOR:
			//DevCon.WriteLn("DEV9: *ATA_R_NSECTOR 16bit read at address %x, value %x, Active %s", addr, nsector, (GetSelectedDevice() == 0) ? "True" : "False");
			if (GetSelectedDevice() != 0)
				return 0;
			if (!regControlHOBRead)
				return regNsector;
			else
				return regNsectorHOB;
		case ATA_R_SECTOR:
			//DevCon.WriteLn("DEV9: *ATA_R_NSECTOR 16bit read at address %x, value %x, Active %s", addr, regSector, (GetSelectedDevice() == 0) ? "True" : "False");
			if (GetSelectedDevice() != 0)
				return 0;
			if (!regControlHOBRead)
				return regSector;
			else
				return regSectorHOB;
		case ATA_R_LCYL:
			//DevCon.WriteLn("DEV9: *ATA_R_LCYL 16bit read at address %x, value %x, Active %s", addr, regLcyl, (GetSelectedDevice() == 0) ? "True" : "False");
			if (GetSelectedDevice() != 0)
				return 0;
			if (!regControlHOBRead)
				return regLcyl;
			else
				return regLcylHOB;
		case ATA_R_HCYL:
			//DevCon.WriteLn("DEV9: *ATA_R_HCYL 16bit read at address % x, value % x, Active %s", addr, regHcyl, (GetSelectedDevice() == 0) ? " True " : " False ");
			if (GetSelectedDevice() != 0)
				return 0;
			if (!regControlHOBRead)
				return regHcyl;
			else
				return regHcylHOB;
		case ATA_R_SELECT:
			//DevCon.WriteLn("DEV9: *ATA_R_SELECT 16bit read at address % x, value % x, Active %s", addr, regSelect, (GetSelectedDevice() == 0) ? " True " : " False ");
			return regSelect;
		case ATA_R_STATUS:
			//DevCon.WriteLn("DEV9: *ATA_R_STATUS (Fallthough to ATA_R_ALT_STATUS)");
			//Clear irqcause
			dev9.irqcause &= ~ATA_INTR_INTRQ;
			[[fallthrough]];
		case ATA_R_ALT_STATUS:
			//DevCon.WriteLn("DEV9: *ATA_R_ALT_STATUS 16bit read at address % x, value % x, Active %s", addr, regStatus, (GetSelectedDevice() == 0) ? " True " : " False ");
			//raise IRQ?
			if (GetSelectedDevice() != 0)
				return 0;
			return regStatus;
		default:
			Console.Error("DEV9: ATA: Unknown 16bit read at address %x", addr);
			return 0xff;
	}
}

void ATA::Write16(u32 addr, u16 value)
{
	if (addr != ATA_R_CMD && (regStatus & (ATA_STAT_BUSY | ATA_STAT_DRQ)) != 0)
	{
		Console.Error("DEV9: ATA: DEVICE BUSY, DROPPING WRITE");
		return;
	}
	switch (addr)
	{
		case ATA_R_FEATURE:
			//DevCon.WriteLn("DEV9: *ATA_R_FEATURE 16bit write at address %x, value %x", addr, value);
			ClearHOB();
			regFeatureHOB = regFeature;
			regFeature = static_cast<u8>(value);
			break;
		case ATA_R_NSECTOR:
			//DevCon.WriteLn("DEV9: *ATA_R_NSECTOR 16bit write at address %x, value %x", addr, value);
			ClearHOB();
			regNsectorHOB = regNsector;
			regNsector = static_cast<u8>(value);
			break;
		case ATA_R_SECTOR:
			//DevCon.WriteLn("DEV9: *ATA_R_SECTOR 16bit write at address %x, value %x", addr, value);
			ClearHOB();
			regSectorHOB = regSector;
			regSector = static_cast<u8>(value);
			break;
		case ATA_R_LCYL:
			//DevCon.WriteLn("DEV9: *ATA_R_LCYL 16bit write at address %x, value %x", addr, value);
			ClearHOB();
			regLcylHOB = regLcyl;
			regLcyl = static_cast<u8>(value);
			break;
		case ATA_R_HCYL:
			//DevCon.WriteLn("DEV9: *ATA_R_HCYL 16bit write at address %x, value %x", addr, value);
			ClearHOB();
			regHcylHOB = regHcyl;
			regHcyl = static_cast<u8>(value);
			break;
		case ATA_R_SELECT:
			//DevCon.WriteLn("DEV9: *ATA_R_SELECT 16bit write at address %x, value %x", addr, value);
			regSelect = static_cast<u8>(value);
			//bus->ifs[0].select = (val & ~0x10) | 0xa0;
			//bus->ifs[1].select = (val | 0x10) | 0xa0;
			break;
		case ATA_R_CONTROL:
			//DevCon.WriteLn("DEV9: *ATA_R_CONTROL 16bit write at address %x, value %x", addr, value);
			//dev9Ru16(ATA_R_CONTROL) = value;
			if ((value & 0x2) != 0)
			{
				//Supress all IRQ
				dev9.irqcause &= ~ATA_INTR_INTRQ;
				regControlEnableIRQ = false;
			}
			else
				regControlEnableIRQ = true;

			if ((value & 0x4) != 0)
			{
				DevCon.WriteLn("DEV9: *ATA_R_CONTROL RESET");
				ResetBegin();
				ResetEnd(false);
			}
			if ((value & 0x80) != 0)
				regControlHOBRead = true;

			break;
		case ATA_R_CMD:
			//DevCon.WriteLn("DEV9: *ATA_R_CMD 16bit write at address %x, value %x", addr, value);
			regCommand = value;
			regControlHOBRead = false;
			dev9.irqcause &= ~ATA_INTR_INTRQ;
			IDE_ExecCmd(value);
			break;
		default:
			Console.Error("DEV9: ATA: UNKNOWN 16bit write at address %x, value %x", addr, value);
			break;
	}
}

void ATA::Async(uint cycles)
{
	if (!hddImage)
		return;

	if ((regStatus & (ATA_STAT_BUSY | ATA_STAT_DRQ)) == 0 ||
		awaitFlush || (waitingCmd != nullptr))
	{
		{
			std::lock_guard ioSignallock(ioMutex);
			if (ioRead || ioWrite)
				//IO Running
				return;
		}

		//Note, ioThread may still be working.
		if (waitingCmd != nullptr) //Are we waiting to continue a command?
		{
			//Log_Info("Running waiting command");
			void (ATA::*cmd)() = waitingCmd;
			waitingCmd = nullptr;
			(this->*cmd)();
		}
		else if (!writeQueue.IsQueueEmpty()) //Flush cache
		{
			//Log_Info("Starting async write");
			{
				std::lock_guard ioSignallock(ioMutex);
				ioWrite = true;
			}
			ioReady.notify_all();
		}
		else if (awaitFlush) //Fire IRQ on flush completion?
		{
			//Log_Info("Flush done, raise IRQ");
			awaitFlush = false;
			PostCmdNoData();
		}
	}
}

s64 ATA::HDD_GetLBA()
{
	if ((regSelect & 0x40) != 0)
	{
		if (!lba48)
		{
			return (regSector |
					(regLcyl << 8) |
					(regHcyl << 16) |
					((regSelect & 0x0f) << 24));
		}
		else
		{
			return (static_cast<s64>(regHcylHOB) << 40) |
				   (static_cast<s64>(regLcylHOB) << 32) |
				   (static_cast<s64>(regSectorHOB) << 24) |
				   (static_cast<s64>(regHcyl) << 16) |
				   (static_cast<s64>(regLcyl) << 8) |
				   regSector;
		}
	}
	else
	{
		regStatus |= static_cast<u8>(ATA_STAT_ERR);
		regError |= static_cast<u8>(ATA_ERR_ABORT);

		Console.Error("DEV9: ATA: Tried to get LBA address while LBA mode disabled");
		//(c.Nh + h).Ns+(s-1)
		//s64 CHSasLBA = ((regLcyl + (regHcyl << 8)) * curHeads + (regSelect & 0x0F)) * curSectors + (regSector - 1);
		return -1;
	}
}

void ATA::HDD_SetLBA(s64 sectorNum)
{
	if ((regSelect & 0x40) != 0)
	{
		if (!lba48)
		{
			regSelect = static_cast<u8>((regSelect & 0xf0) | static_cast<int>((sectorNum >> 24) & 0x0f));
			regHcyl = static_cast<u8>(sectorNum >> 16);
			regLcyl = static_cast<u8>(sectorNum >> 8);
			regSector = static_cast<u8>(sectorNum);
		}
		else
		{
			regSector = static_cast<u8>(sectorNum);
			regLcyl = static_cast<u8>(sectorNum >> 8);
			regHcyl = static_cast<u8>(sectorNum >> 16);
			regSectorHOB = static_cast<u8>(sectorNum >> 24);
			regLcylHOB = static_cast<u8>(sectorNum >> 32);
			regHcylHOB = static_cast<u8>(sectorNum >> 40);
		}
	}
	else
	{
		regStatus |= ATA_STAT_ERR;
		regError |= ATA_ERR_ABORT;

		Console.Error("DEV9: ATA: Tried to set LBA address while LBA mode disabled");
	}
}

bool ATA::HDD_CanSeek()
{
	int sectors = 0;
	return HDD_CanAccess(&sectors);
}

bool ATA::HDD_CanAccess(int* sectors)
{
	s64 maxLBA = hddImageSize / 512 - 1;
	if ((regSelect & 0x40) == 0) //CHS mode
		maxLBA = std::min<s64>(maxLBA, curCylinders * curHeads * curSectors);

	const s64 posStart = HDD_GetLBA();
	if (posStart == -1)
		return false;

	//DevCon.WriteLn("DEV9: LBA :%i", lba);
	if (posStart > maxLBA)
	{
		*sectors = -1;
		return false;
	}

	const s64 posEnd = posStart + *sectors;
	if (posEnd > maxLBA)
	{
		const s64 overshoot = posEnd - maxLBA;
		s64 space = *sectors - overshoot;
		*sectors = static_cast<int>(space);
		return false;
	}

	return true;
}

//QEMU stuff
void ATA::ClearHOB()
{
	/* any write clears HOB high bit of device control register */
	regControlHOBRead = false;
}
