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

void ATA::HDD_Smart()
{
	DevCon.WriteLn("DEV9: HDD_Smart");

	if ((regStatus & ATA_STAT_READY) == 0)
		return;

	if (regHcyl != 0xC2 || regLcyl != 0x4F)
	{
		CmdNoDataAbort();
		return;
	}

	if (!fetSmartEnabled && regFeature != 0xD8)
	{
		CmdNoDataAbort();
		return;
	}

	switch (regFeature)
	{
		case 0xD9: //SMART_DISABLE
			SMART_EnableOps(false);
			return;
		case 0xD8: //SMART_ENABLE
			SMART_EnableOps(true);
			return;
		case 0xD2: //SMART_ATTR_AUTOSAVE
			SMART_SetAutoSaveAttribute();
			return;
		case 0xD3: //SMART_ATTR_SAVE
			return;
		case 0xDA: //SMART_STATUS (is fault in disk?)
			SMART_ReturnStatus();
			return;
		case 0xD1: //SMART_READ_THRESH
			Console.Error("DEV9: ATA: SMART_READ_THRESH Not Implemented");
			CmdNoDataAbort();
			return;
		case 0xD0: //SMART_READ_DATA
			Console.Error("DEV9: ATA: SMART_READ_DATA Not Implemented");
			CmdNoDataAbort();
			return;
		case 0xD5: //SMART_READ_LOG
			Console.Error("DEV9: ATA: SMART_READ_LOG Not Implemented");
			CmdNoDataAbort();
			return;
		case 0xD4: //SMART_EXECUTE_OFFLINE
			SMART_ExecuteOfflineImmediate();
			return;
		default:
			Console.Error("DEV9: ATA: Unknown SMART command %x", regFeature);
			CmdNoDataAbort();
			return;
	}
}

void ATA::SMART_SetAutoSaveAttribute()
{
	PreCmd();
	switch (regSector)
	{
		case 0x00:
			smartAutosave = false;
			break;
		case 0xF1:
			smartAutosave = true;
			break;
		default:
			Console.Error("DEV9: ATA: Unknown SMART_ATTR_AUTOSAVE command %s", regSector);
			CmdNoDataAbort();
			return;
	}
	PostCmdNoData();
}

void ATA::SMART_ExecuteOfflineImmediate()
{
	PreCmd();
	[[maybe_unused]] int n = 0;
	switch (regSector)
	{
		case 0: /* off-line routine */
		case 1: /* short self test */
		case 2: /* extended self test */
			smartSelfTestCount++;
			if (smartSelfTestCount > 21)
				smartSelfTestCount = 1;

			n = 2 + (smartSelfTestCount - 1) * 24;
			//s->smart_selftest_data[n] = s->sector;
			//s->smart_selftest_data[n + 1] = 0x00; /* OK and finished */
			//s->smart_selftest_data[n + 2] = 0x34; /* hour count lsb */
			//s->smart_selftest_data[n + 3] = 0x12; /* hour count msb */
			break;
		case 127: /* abort off-line routine */
			break;
		case 129: /* short self test, which holds BSY until complete */
		case 130: /* extended self test, which holds BSY until complete */
			smartSelfTestCount++;
			if (smartSelfTestCount > 21)
			{
				smartSelfTestCount = 1;
			}
			n = 2 + (smartSelfTestCount - 1) * 24;

			SMART_ReturnStatus();
			return;
		default:
			CmdNoDataAbort();
			return;
	}
	PostCmdNoData();
}

void ATA::SMART_EnableOps(bool enable)
{
	PreCmd();
	fetSmartEnabled = enable;
	PostCmdNoData();
}

void ATA::SMART_ReturnStatus()
{
	PreCmd();
	if (!smartErrors)
	{
		regHcyl = 0xC2;
		regLcyl = 0x4F;
	}
	else
	{
		regHcyl = 0x2C;
		regLcyl = 0xF4;
	}
	PostCmdNoData();
}
