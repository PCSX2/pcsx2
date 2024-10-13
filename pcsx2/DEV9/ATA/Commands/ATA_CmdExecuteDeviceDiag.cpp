// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DEV9/ATA/ATA.h"
#include "DEV9/DEV9.h"

void ATA::PreCmdExecuteDeviceDiag()
{
	regStatus |= ATA_STAT_BUSY;
	regStatus &= ~ATA_STAT_READY;
	pendingInterrupt = false;
	dev9.irqcause &= ~ATA_INTR_INTRQ;
	//dev9.spd.regIntStat &= unchecked((UInt16)~DEV9Header.ATA_INTR_DMA_RDY); //Is this correct?
}

void ATA::PostCmdExecuteDeviceDiag(bool sendIRQ)
{
	regStatus &= ~ATA_STAT_BUSY;
	regStatus |= ATA_STAT_READY;

	SetSelectedDevice(0);

	// If Device Diagnostics is performed as part of a reset
	// then we don't raise an IRQ or set pending interrupt
	if (sendIRQ)
	{
		pendingInterrupt = true;
		if (regControlEnableIRQ)
			_DEV9irq(ATA_INTR_INTRQ, 1);
	}
}

//GENRAL FEATURE SET

void ATA::HDD_ExecuteDeviceDiag(bool sendIRQ)
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

	PostCmdExecuteDeviceDiag(sendIRQ);
}
