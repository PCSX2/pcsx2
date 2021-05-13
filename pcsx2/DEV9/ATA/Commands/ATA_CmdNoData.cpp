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

void ATA::PostCmdNoData()
{
	regStatus &= ~ATA_STAT_BUSY;

	if (regControlEnableIRQ)
		_DEV9irq(ATA_INTR_INTRQ, 1);
}

void ATA::CmdNoDataAbort()
{
	PreCmd();

	regError |= ATA_ERR_ABORT;
	regStatus |= ATA_STAT_ERR;
	PostCmdNoData();
}

//GENRAL FEATURE SET

void ATA::HDD_FlushCache() //Can't when DRQ set
{
	if (!PreCmd())
		return;
	DevCon.WriteLn("DEV9: HDD_FlushCache");

	awaitFlush = true;
	Async(-1);
}

void ATA::HDD_InitDevParameters()
{
	PreCmd(); //Ignore DRDY bit
	DevCon.WriteLn("DEV9: HDD_InitDevParameters");

	curSectors = regNsector;
	curHeads = (u8)((regSelect & 0x7) + 1);
	PostCmdNoData();
}

void ATA::HDD_ReadVerifySectors(bool isLBA48)
{
	if (!PreCmd())
		return;
	DevCon.WriteLn("DEV9: HDD_ReadVerifySectors");

	IDE_CmdLBA48Transform(isLBA48);

	HDD_CanAssessOrSetError();

	PostCmdNoData();
}

void ATA::HDD_SeekCmd()
{
	if (!PreCmd())
		return;
	DevCon.WriteLn("DEV9: HDD_SeekCmd");

	regStatus &= ~ATA_STAT_SEEK;

	if (HDD_CanSeek())
	{
		regStatus |= ATA_STAT_ERR;
		regError |= ATA_ERR_ID;
	}
	else
		regStatus |= ATA_STAT_SEEK;

	PostCmdNoData();
}

void ATA::HDD_SetFeatures()
{
	if (!PreCmd())
		return;
	DevCon.WriteLn("DEV9: HDD_SetFeatures");

	switch (regFeature)
	{
		case 0x02:
			fetWriteCacheEnabled = true;
			break;
		case 0x82:
			fetWriteCacheEnabled = false;
			awaitFlush = true; //Flush Cache
			return;
		case 0x03: //Set transfer mode
		{
			const u16 xferMode = (u16)regNsector; //Set Transfer mode

			const int mode = xferMode & 0x07;
			switch ((xferMode) >> 3)
			{
				case 0x00: //pio default
					//if mode = 1, disable IORDY
					DevCon.WriteLn("DEV9: PIO Default");
					pioMode = 4;
					sdmaMode = -1;
					mdmaMode = -1;
					udmaMode = -1;
					break;
				case 0x01: //pio mode (3,4)
					DevCon.WriteLn("DEV9: PIO Mode %i", mode);
					pioMode = mode;
					sdmaMode = -1;
					mdmaMode = -1;
					udmaMode = -1;
					break;
				case 0x02: //Single word dma mode (0,1,2)
					DevCon.WriteLn("DEV9: SDMA Mode %i", mode);
					//pioMode = -1;
					sdmaMode = mode;
					mdmaMode = -1;
					udmaMode = -1;
					break;
				case 0x04: //Multi word dma mode (0,1,2)
					DevCon.WriteLn("DEV9: MDMA Mode %i", mode);
					//pioMode = -1;
					sdmaMode = -1;
					mdmaMode = mode;
					udmaMode = -1;
					break;
				case 0x08: //Ulta dma mode (0,1,2,3,4,5,6)
					DevCon.WriteLn("DEV9: UDMA Mode %i", mode);
					//pioMode = -1;
					sdmaMode = -1;
					mdmaMode = -1;
					udmaMode = mode;
					break;
				default:
					Console.Error("DEV9: ATA: Unknown transfer mode");
					CmdNoDataAbort();
					break;
			}
		}
		break;
		default:
			Console.Error("DEV9: ATA: Unknown feature mode");
			break;
	}
	PostCmdNoData();
}

void ATA::HDD_SetMultipleMode()
{
	if (!PreCmd())
		return;
	DevCon.WriteLn("DEV9: HDD_SetMultipleMode");

	curMultipleSectorsSetting = regNsector;

	PostCmdNoData();
}

void ATA::HDD_Nop()
{
	if (!PreCmd())
		return;
	DevCon.WriteLn("DEV9: HDD_Nop");

	if (regFeature == 0)
	{
		//This would abort queues if the
		//PS2 HDD supported them.
	}
	//Always ends in error
	regError |= ATA_ERR_ABORT;
	regStatus |= ATA_STAT_ERR;
	PostCmdNoData();
}

//Other Feature Sets

void ATA::HDD_Idle()
{
	if (!PreCmd())
		return;
	DevCon.WriteLn("DEV9: HDD_Idle");

	long idleTime = 0; //in seconds
	if (regNsector >= 1 && regNsector <= 240)
		idleTime = 5 * regNsector;
	else if (regNsector >= 241 && regNsector <= 251)
		idleTime = 30 * (regNsector - 240) * 60;
	else
	{
		switch (regNsector)
		{
			case 0:
				idleTime = 0;
				break;
			case 252:
				idleTime = 21 * 60;
				break;
			case 253: //bettween 8 and 12 hrs
				idleTime = 10 * 60 * 60;
				break;
			case 254: //reserved
				idleTime = -1;
				break;
			case 255:
				idleTime = 21 * 60 + 15;
				break;
			default:
				idleTime = 0;
				break;
		}
	}

	DevCon.WriteLn("DEV9: HDD_Idle for %is", idleTime);
	PostCmdNoData();
}

void ATA::HDD_IdleImmediate()
{
	if (!PreCmd())
		return;
	DevCon.WriteLn("DEV9: HDD_IdleImmediate");
	PostCmdNoData();
}
