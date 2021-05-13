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
