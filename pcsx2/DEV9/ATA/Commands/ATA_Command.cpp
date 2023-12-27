// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "DEV9/ATA/ATA.h"
#include "DEV9/DEV9.h"

void ATA::IDE_ExecCmd(u16 value)
{
	switch (value)
	{
		case 0x00:
			HDD_Nop();
			break;
		case 0x20:
			HDD_ReadSectors(false);
			break;
			//0x21
		case 0x40:
			HDD_ReadVerifySectors(false);
			break;
			//0x41
		case 0x70:
			HDD_SeekCmd();
			break;
		case 0x90:
			HDD_ExecuteDeviceDiag();
			break;
		case 0x91:
			HDD_InitDevParameters();
			break;
		case 0xB0:
			HDD_Smart();
			break;
		case 0xC4:
			HDD_ReadMultiple(false);
			break;
		case 0xC8:
			HDD_ReadDMA(false);
			break;
			//0xC9
		case 0xCA:
			HDD_WriteDMA(false);
			break;
			//0xCB
			//0x25 = HDDreadDMA48;
			//0x35 = HDDwriteDMA48;*/
		case 0xE1:
			HDD_IdleImmediate();
			break;
		case 0xE3:
			HDD_Idle();
			break;
		case 0xE7:
			HDD_FlushCache();
			break;
			//0xEA = HDDflushCache48
		case 0xEC:
			HDD_IdentifyDevice();
			break;
			//0xA1 = HDDidentifyPktDevice
		case 0xEF:
			HDD_SetFeatures();
			break;

			//0xF1 = HDDsecSetPassword
			//0xF2 = HDDsecUnlock
			//0xF3 = HDDsecErasePrepare;
			//0xF4 = HDDsecEraseUnit;

			/* This command is Sony-specific and isn't part of the IDE standard */
			/* The Sony HDD has a modified firmware that supports this command */
			/* Sending this command to a standard HDD will give an error */
			/* We roughly emulate it to make programs think the HDD is a Sony one */
			/* However, we only send null, if anyting checks the returned data */
			/* it will fail */
		case 0x8E:
			HDD_SCE();
			break;

		default:
			HDD_Unk();
			break;
	}
}

void ATA::HDD_Unk()
{
	Console.Error("DEV9: ATA: Unknown cmd %x", regCommand);

	PreCmd();

	regError |= ATA_ERR_ABORT;
	regStatus |= ATA_STAT_ERR;
	PostCmdNoData();
}

bool ATA::PreCmd()
{
	if ((regStatus & ATA_STAT_READY) == 0)
	{
		//Ignore CMD write except for EXECUTE DEVICE DIAG and INITIALIZE DEVICE PARAMETERS
		return false;
	}
	regStatus |= ATA_STAT_BUSY;

	regStatus &= ~ATA_STAT_WRERR;
	regStatus &= ~ATA_STAT_DRQ;
	regStatus &= ~ATA_STAT_ERR;

	regStatus &= ~ATA_STAT_SEEK;

	regError = 0;

	return true;
}

void ATA::IDE_CmdLBA48Transform(bool islba48)
{
	lba48 = islba48;
	//TODO
	/* handle the 'magic' 0 nsector count conversion here. to avoid
             * fiddling with the rest of the read logic, we just store the
             * full sector count in ->nsector
             */
	if (!lba48)
	{
		if (regNsector == 0)
			nsector = 256;
		else
			nsector = regNsector;
	}
	else
	{
		if (regNsector == 0 && regNsectorHOB == 0)
			nsector = 65536;
		else
		{
			const int lo = regNsector;
			const int hi = regNsectorHOB;

			nsector = (hi << 8) | lo;
		}
	}
}

//OTHER FEATURE SETS BELOW (TODO?)

//CFA ERASE SECTORS
//WRITE MULTIPLE
//SET MULTIPLE

//CFA WRITE MULTIPLE WITHOUT ERASE
//GET MEDIA STATUS
//MEDIA LOCK
//MEDIA UNLOCK
//STANDBY IMMEDIAYTE
//STANBY

//CHECK POWER MODE
//SLEEP

//MEDIA EJECT

//SECURITY SET PASSWORD
//SECURITY UNLOCK
//SECUTIRY ERASE PREPARE
//SECURITY ERASE UNIT
//SECURITY FREEZE LOCK
//SECURITY DIABLE PASSWORD
//READ NATIVE MAX ADDRESS
//SET MAX ADDRESS
