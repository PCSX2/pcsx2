// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "DEV9/ATA/ATA.h"
#include "DEV9/DEV9.h"

void ATA::PreCmdExecuteDeviceDiag()
{
	regStatus |= ATA_STAT_BUSY;
	regStatus &= ~ATA_STAT_READY;
	dev9.irqcause &= ~ATA_INTR_INTRQ;
	//dev9.spd.regIntStat &= unchecked((UInt16)~DEV9Header.ATA_INTR_DMA_RDY); //Is this correct?
}

void ATA::PostCmdExecuteDeviceDiag()
{
	regStatus &= ~ATA_STAT_BUSY;
	regStatus |= ATA_STAT_READY;

	SetSelectedDevice(0);

	if (regControlEnableIRQ)
		_DEV9irq(ATA_INTR_INTRQ, 1);
}

//GENRAL FEATURE SET

void ATA::HDD_ExecuteDeviceDiag()
{
	PreCmdExecuteDeviceDiag();
	//Perform Self Diag
	//Log_Error("ExecuteDeviceDiag");
	//Would check both drives, but the PS2 would only have 1
	regError &= ~ATA_ERR_ICRC;
	//Passed self-Diag
	regError = (0x01 | (regError & ATA_ERR_ICRC));

	regNsector = 1;
	regSector = 1;
	regLcyl = 0;
	regHcyl = 0;

	regStatus &= ~ATA_STAT_DRQ;
	regStatus &= ~ATA_STAT_ECC;
	regStatus &= ~ATA_STAT_ERR;

	PostCmdExecuteDeviceDiag();
}
