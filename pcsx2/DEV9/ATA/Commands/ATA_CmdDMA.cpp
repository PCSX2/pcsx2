// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
	if ((udmaMode >= 0 || mdmaMode >= 0) &&
		(dev9.if_ctrl & SPD_IF_ATA_DMAEN) != 0)
	{
		if (size == 0 || nsector == -1)
			return;
		DevCon.WriteLn("DEV9: DMA read, size %i, transferred %i, total size %i", size, rdTransferred, nsector * 512);

		//read
		size = std::min(size, nsector * 512 - rdTransferred);
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
	if ((udmaMode >= 0 || mdmaMode >= 0) &&
		(dev9.if_ctrl & SPD_IF_ATA_DMAEN) != 0)
	{
		if (nsector == -1)
			return;
		DevCon.WriteLn("DEV9: DMA write, size %i, transferred %i, total size %i", size, wrTransferred, nsector * 512);

		//write
		size = std::min(size, nsector * 512 - wrTransferred);
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
		Console.Error("DEV9: ATA: Transfer from invalid LBA %lu", HDD_GetLBA());
		nsector = -1;
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
		Console.Error("DEV9: ATA: Transfer from invalid LBA %lu", HDD_GetLBA());
		nsector = -1;
		regStatus |= ATA_STAT_ERR;
		regError |= ATA_ERR_ID;
		PostCmdNoData();
		return;
	}

	//Do Async write
	DRQCmdDMADataFromHost();
}
