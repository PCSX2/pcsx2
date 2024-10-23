// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DEV9/ATA/ATA.h"
#include "DEV9/DEV9.h"

void ATA::DRQCmdDMADataToHost()
{
	//Ready to Start DMA
	regStatus &= ~ATA_STAT_BUSY;
	regStatus |= ATA_STAT_DRQ;
	dmaReady = true;
	DEV9runFIFO();
	//PCSX2 will Start DMA
}
void ATA::PostCmdDMADataToHost()
{
	//readBuffer = null;
	nsectorLeft = 0;

	regStatus &= ~ATA_STAT_DRQ;
	regStatus &= ~ATA_STAT_BUSY;
	dmaReady = false;

	pendingInterrupt = true;
	if (regControlEnableIRQ)
		_DEV9irq(ATA_INTR_INTRQ, 1);
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
	DEV9runFIFO();
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

	if (fetWriteCacheEnabled)
	{
		regStatus &= ~ATA_STAT_BUSY;
		pendingInterrupt = true;
		if (regControlEnableIRQ)
			_DEV9irq(ATA_INTR_INTRQ, 1);
	}
	else
		awaitFlush = true;

	Async(-1);
}

int ATA::ReadDMAToFIFO(u8* buffer, int space)
{
	if (udmaMode >= 0 || mdmaMode >= 0)
	{
		if (space == 0 || nsector == -1)
			return 0;

		// Read to FIFO
		const int size = std::min(space, nsector * 512 - rdTransferred);
		memcpy(buffer, &readBuffer[rdTransferred], size);

		rdTransferred += size;

		if (rdTransferred >= nsector * 512)
		{
			HDD_SetErrorAtTransferEnd();

			nsector = 0;
			rdTransferred = 0;
			PostCmdDMADataToHost();
		}

		return size;
	}
	return 0;
}

int ATA::WriteDMAFromFIFO(u8* buffer, int available)
{
	if (udmaMode >= 0 || mdmaMode >= 0)
	{
		if (available == 0 || nsector == -1)
			return 0;

		// Write to FIFO
		const int size = std::min(available, nsector * 512 - wrTransferred);
		memcpy(&currentWrite[wrTransferred], buffer, size);

		wrTransferred += size;

		if (wrTransferred >= nsector * 512)
		{
			HDD_SetErrorAtTransferEnd();

			nsector = 0;
			wrTransferred = 0;
			PostCmdDMADataFromHost();
		}

		return size;
	}
	return 0;
}

//GENRAL FEATURE SET

void ATA::HDD_ReadDMA(bool isLBA48)
{
	if (!PreCmd())
		return;
	DevCon.WriteLn(isLBA48 ? "DEV9: HDD_ReadDMA48" : "DEV9: HDD_ReadDMA");

	IDE_CmdLBA48Transform(isLBA48);

	regStatus &= ~ATA_STAT_SEEK;
	if (!HDD_CanSeek())
	{
		Console.Error("DEV9: ATA: Transfer from invalid LBA %lu", HDD_GetLBA());
		nsector = -1;
		regStatus |= ATA_STAT_ERR;
		regStatusSeekLock = -1;
		regError |= ATA_ERR_ID;
		PostCmdNoData();
		return;
	}
	else
		regStatus |= ATA_STAT_SEEK;

	//Do Sync Read
	HDD_ReadSync(&ATA::DRQCmdDMADataToHost);
}

void ATA::HDD_WriteDMA(bool isLBA48)
{
	if (!PreCmd())
		return;
	DevCon.WriteLn(isLBA48 ? "DEV9: HDD_WriteDMA48" : "DEV9: HDD_WriteDMA");

	IDE_CmdLBA48Transform(isLBA48);

	regStatus &= ~ATA_STAT_SEEK;
	if (!HDD_CanSeek())
	{
		Console.Error("DEV9: ATA: Transfer from invalid LBA %lu", HDD_GetLBA());
		nsector = -1;
		regStatus |= ATA_STAT_ERR;
		regStatusSeekLock = -1;
		regError |= ATA_ERR_ID;
		PostCmdNoData();
		return;
	}
	else
		regStatus |= ATA_STAT_SEEK;

	//Do Async write
	DRQCmdDMADataFromHost();
}
