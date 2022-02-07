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

void ATA::DRQCmdPIODataToHost(u8* buff, int buffLen, int buffIndex, int size, bool sendIRQ)
{
	//Data in PIO ready to be sent
	pioPtr = 0;
	pioEnd = size >> 1;

	memcpy(pioBuffer, &buff[buffIndex], size < (buffLen - buffIndex) ? size : (buffLen - buffIndex));

	regStatus &= ~ATA_STAT_BUSY;
	regStatus |= ATA_STAT_DRQ;

	if (regControlEnableIRQ && sendIRQ)
		_DEV9irq(ATA_INTR_INTRQ, 1); //0x6c cycles before
}
void ATA::PostCmdPIODataToHost()
{
	pioPtr = 0;
	pioEnd = 0;
	//AnyMoreData?
	if (pioDRQEndTransferFunc != nullptr)
	{
		regStatus |= ATA_STAT_BUSY;
		regStatus &= ~ATA_STAT_DRQ;
		//Call cmd to retrive more data
		(this->*pioDRQEndTransferFunc)();
	}
	else
		regStatus &= ~ATA_STAT_DRQ;
}

//FromHost
u16 ATA::ATAreadPIO()
{
	//DevCon.WriteLn("DEV9: *ATA_R_DATA 16bit read, pio_count %i,  pio_size %i", pioPtr, pioEnd);
	if (pioPtr < pioEnd)
	{
		const u16 ret = *(u16*)&pioBuffer[pioPtr * 2];
		//DevCon.WriteLn("DEV9: *ATA_R_DATA returned value is  %x", ret);
		pioPtr++;
		if (pioPtr >= pioEnd) //Fnished transfer (Changed from MegaDev9)
			PostCmdPIODataToHost();

		return ret;
	}
	return 0xFF;
}
//ATAwritePIO

void ATA::HDD_IdentifyDevice()
{
	if (!PreCmd())
		return;
	DevCon.WriteLn("DEV9: HddidentifyDevice");

	//IDE transfer start
	CreateHDDinfo(EmuConfig.DEV9.HddSizeSectors);

	pioDRQEndTransferFunc = nullptr;
	DRQCmdPIODataToHost(identifyData, 256 * 2, 0, 256 * 2, true);
}

//Read Buffer

void ATA::HDD_ReadMultiple(bool isLBA48)
{
	sectorsPerInterrupt = curMultipleSectorsSetting;
	HDD_ReadPIO(isLBA48);
}

void ATA::HDD_ReadSectors(bool isLBA48)
{
	sectorsPerInterrupt = 1;
	HDD_ReadPIO(isLBA48);
}

void ATA::HDD_ReadPIO(bool isLBA48)
{
	//Log_Info("HDD_ReadPIO");
	if (!PreCmd())
		return;

	if (sectorsPerInterrupt == 0)
	{
		CmdNoDataAbort();
		return;
	}

	IDE_CmdLBA48Transform(isLBA48);

	if (!HDD_CanSeek())
	{
		regStatus |= ATA_STAT_ERR;
		regError |= ATA_ERR_ID;
		PostCmdNoData();
		return;
	}

	HDD_ReadSync(&ATA::HDD_ReadPIOS2);
}

void ATA::HDD_ReadPIOS2()
{
	//Log_Info("HDD_ReadPIO Stage 2");
	pioDRQEndTransferFunc = &ATA::HDD_ReadPIOEndBlock;
	DRQCmdPIODataToHost(readBuffer, readBufferLen, 0, 256 * 2, true);
}

void ATA::HDD_ReadPIOEndBlock()
{
	//Log_Info("HDD_ReadPIO End Block");
	rdTransferred += 512;
	if (rdTransferred >= nsector * 512)
	{
		//Log_Info("HDD_ReadPIO Done");
		HDD_SetErrorAtTransferEnd();
		regStatus &= ~ATA_STAT_BUSY;
		pioDRQEndTransferFunc = nullptr;
		rdTransferred = 0;
	}
	else
	{
		if ((rdTransferred / 512) % sectorsPerInterrupt == 0)
			DRQCmdPIODataToHost(readBuffer, readBufferLen, rdTransferred, 256 * 2, true);
		else
			DRQCmdPIODataToHost(readBuffer, readBufferLen, rdTransferred, 256 * 2, false);
	}
}

//Write Buffer

//Write Multiple

//Write Sectors

//Download Microcode (Used for FW updates)
