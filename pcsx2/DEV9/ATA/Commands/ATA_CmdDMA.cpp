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

#include "DEV9/ATA/ATA.h"
#include "DEV9/DEV9.h"

void ATA::DRQCmdDMADataToHost()
{
	//Ready to Start DMA
	regStatus &= ~ATA_STAT_BUSY;
	regStatus |= ATA_STAT_DRQ;
	dmaReady = true;
	_DEV9irq(SPD_INTR_ATA_FIFO_DATA, 1);
	//PCSX2 will Start DMA
}
void ATA::PostCmdDMADataToHost()
{
	//readBuffer = null;
	nsectorLeft = 0;

	regStatus &= ~ATA_STAT_DRQ;
	regStatus &= ~ATA_STAT_BUSY;
	dmaReady = false;

	dev9.irqcause &= ~SPD_INTR_ATA_FIFO_DATA;
	if (regControlEnableIRQ)
		_DEV9irq(ATA_INTR_INTRQ, 1);
	//PCSX2 Will Start DMA
}

void ATA::DRQCmdDMADataFromHost()
{
	//Ready to Start DMA
	if (!HDD_CanAssessOrSetError())
		return;

	nsectorLeft = nsector;
	currentWrite = new u8[nsector * 512];
	currentWriteLength = nsector * 512;
	currentWriteSectors = HDD_GetLBA();


	regStatus &= ~ATA_STAT_BUSY;
	regStatus |= ATA_STAT_DRQ;
	dmaReady = true;
	_DEV9irq(SPD_INTR_ATA_FIFO_DATA, 1);
	//PCSX2 will Start DMA
}
void ATA::PostCmdDMADataFromHost()
{
	WriteQueueEntry entry{0};
	entry.data = currentWrite;
	entry.length = currentWriteLength;
	entry.sector = currentWriteSectors;
	writeQueue.Enqueue(entry);
	currentWrite = nullptr;
	currentWriteLength = 0;
	currentWriteSectors = 0;
	nsectorLeft = 0;

	regStatus &= ~ATA_STAT_DRQ;
	dmaReady = false;

	dev9.irqcause &= ~SPD_INTR_ATA_FIFO_DATA;

	if (fetWriteCacheEnabled)
	{
		regStatus &= ~ATA_STAT_BUSY;
		if (regControlEnableIRQ)
			_DEV9irq(ATA_INTR_INTRQ, 1); //0x6C
	}
	else
		awaitFlush = true;

	Async(-1);
}

void ATA::ATAreadDMA8Mem(u8* pMem, int size)
{
	if ((udmaMode >= 0) &&
		(dev9.if_ctrl & SPD_IF_ATA_DMAEN) != 0)
	{
		if (size == 0)
			return;
		DevCon.WriteLn("DEV9: DMA read, size %i, transferred %i, total size %i", size, rdTransferred, nsector * 512);

		//read
		memcpy(pMem, &readBuffer[rdTransferred], size);

		rdTransferred += size;

		if (rdTransferred >= nsector * 512)
		{
			HDD_SetErrorAtTransferEnd();

			nsector = 0;
			rdTransferred = 0;
			PostCmdDMADataToHost();
		}
	}
}

void ATA::ATAwriteDMA8Mem(u8* pMem, int size)
{
	if ((udmaMode >= 0) &&
		(dev9.if_ctrl & SPD_IF_ATA_DMAEN) != 0)
	{
		DevCon.WriteLn("DEV9: DMA write, size %i, transferred %i, total size %i", size, wrTransferred, nsector * 512);

		//write
		memcpy(&currentWrite[wrTransferred], pMem, size);

		wrTransferred += size;

		if (wrTransferred >= nsector * 512)
		{
			HDD_SetErrorAtTransferEnd();

			nsector = 0;
			wrTransferred = 0;
			PostCmdDMADataFromHost();
		}
	}
}

//GENRAL FEATURE SET

void ATA::HDD_ReadDMA(bool isLBA48)
{
	if (!PreCmd())
		return;
	DevCon.WriteLn("DEV9: HDD_ReadDMA");

	IDE_CmdLBA48Transform(isLBA48);

	if (!HDD_CanSeek())
	{
		regStatus |= ATA_STAT_ERR;
		regError |= ATA_ERR_ID;
		PostCmdNoData();
		return;
	}

	//Do Sync Read
	HDD_ReadSync(&ATA::DRQCmdDMADataToHost);
}

void ATA::HDD_WriteDMA(bool isLBA48)
{
	if (!PreCmd())
		return;
	DevCon.WriteLn("DEV9: HDD_WriteDMA");

	IDE_CmdLBA48Transform(isLBA48);

	if (!HDD_CanSeek())
	{
		regStatus |= ATA_STAT_ERR;
		regError |= ATA_ERR_ID;
		PostCmdNoData();
		return;
	}

	//Do Async write
	DRQCmdDMADataFromHost();
}
