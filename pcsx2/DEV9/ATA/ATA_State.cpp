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

#include "common/Assertions.h"

#include "ATA.h"
#include "DEV9/DEV9.h"
#ifndef PCSX2_CORE
#include "HddCreateWx.h"
#endif

ATA::ATA()
{
	//Power on, Would do self-Diag + Hardware Init
	ResetBegin();
	ResetEnd(true);
}

int ATA::Open(ghc::filesystem::path hddPath)
{
	readBufferLen = 256 * 512;
	readBuffer = new u8[readBufferLen];

	CreateHDDinfo(EmuConfig.DEV9.HddSizeSectors);

	//Open File
	if (!ghc::filesystem::exists(hddPath))
	{
#ifndef PCSX2_CORE
		HddCreateWx hddCreator;
		hddCreator.filePath = hddPath;
		hddCreator.neededSize = ((u64)EmuConfig.DEV9.HddSizeSectors) * 512;
		hddCreator.Start();

		if (hddCreator.errored)
			return -1;
#else
		return -1;
#endif
	}
	hddImage = ghc::filesystem::fstream(hddPath, std::ios::in | std::ios::out | std::ios::binary);

	//Store HddImage size for later check
	hddImage.seekg(0, std::ios::end);
	hddImageSize = hddImage.tellg();

	{
		std::lock_guard ioSignallock(ioMutex);
		ioRead = false;
		ioWrite = false;
	}

	ioThread = std::thread(&ATA::IO_Thread, this);
	ioRunning = true;

	return 0;
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
	if (hddImage.is_open())
		hddImage.close();

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
		sdmaMode = -1;
		mdmaMode = 2;
		udmaMode = -1;
	}
	else
	{
		pioMode = 4;
		if (udmaMode == -1)
		{
			sdmaMode = -1;
			mdmaMode = 2;
		}
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
			regFeature = (u8)value;
			break;
		case ATA_R_NSECTOR:
			//DevCon.WriteLn("DEV9: *ATA_R_NSECTOR 16bit write at address %x, value %x", addr, value);
			ClearHOB();
			regNsectorHOB = regNsector;
			regNsector = (u8)value;
			break;
		case ATA_R_SECTOR:
			//DevCon.WriteLn("DEV9: *ATA_R_SECTOR 16bit write at address %x, value %x", addr, value);
			ClearHOB();
			regSectorHOB = regSector;
			regSector = (u8)value;
			break;
		case ATA_R_LCYL:
			//DevCon.WriteLn("DEV9: *ATA_R_LCYL 16bit write at address %x, value %x", addr, value);
			ClearHOB();
			regLcylHOB = regLcyl;
			regLcyl = (u8)value;
			break;
		case ATA_R_HCYL:
			//DevCon.WriteLn("DEV9: *ATA_R_HCYL 16bit write at address %x, value %x", addr, value);
			ClearHOB();
			regHcylHOB = regHcyl;
			regHcyl = (u8)value;
			break;
		case ATA_R_SELECT:
			//DevCon.WriteLn("DEV9: *ATA_R_SELECT 16bit write at address %x, value %x", addr, value);
			regSelect = (u8)value;
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
	if (!hddImage.is_open())
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
			return ((s64)regHcylHOB << 40) |
				   ((s64)regLcylHOB << 32) |
				   ((s64)regSectorHOB << 24) |
				   ((s64)regHcyl << 16) |
				   ((s64)regLcyl << 8) |
				   regSector;
		}
	}
	else
	{
		regStatus |= (u8)ATA_STAT_ERR;
		regError |= (u8)ATA_ERR_ABORT;

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
			regSelect = (u8)((regSelect & 0xf0) | (int)((sectorNum >> 24) & 0x0f));
			regHcyl = (u8)(sectorNum >> 16);
			regLcyl = (u8)(sectorNum >> 8);
			regSector = (u8)(sectorNum);
		}
		else
		{
			regSector = (u8)sectorNum;
			regLcyl = (u8)(sectorNum >> 8);
			regHcyl = (u8)(sectorNum >> 16);
			regSectorHOB = (u8)(sectorNum >> 24);
			regLcylHOB = (u8)(sectorNum >> 32);
			regHcylHOB = (u8)(sectorNum >> 40);
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
	s64 lba;
	s64 posStart;
	s64 posEnd;
	s64 maxLBA;

	maxLBA = std::min<s64>(EmuConfig.DEV9.HddSizeSectors, hddImageSize / 512) - 1;
	if ((regSelect & 0x40) == 0) //CHS mode
		maxLBA = std::min<s64>(maxLBA, curCylinders * curHeads * curSectors);

	lba = HDD_GetLBA();
	if (lba == -1)
		return false;

	//DevCon.WriteLn("DEV9: LBA :%i", lba);
	posStart = lba;

	if (posStart > maxLBA)
	{
		*sectors = -1;
		return false;
	}

	posEnd = posStart + *sectors;

	if (posEnd > maxLBA)
	{
		const s64 overshoot = posEnd - maxLBA;
		s64 space = *sectors - overshoot;
		*sectors = (int)space;
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
