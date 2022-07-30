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
#include "common/FileSystem.h"

#include "Config.h"

void ATA::HDD_SCE()
{
	DevCon.WriteLn("DEV9: HDD_SCE SONY-SPECIFIC SECURITY CONTROL COMMAND %x", regFeature);

	switch (regFeature)
	{
		case 0xEC:
			SCE_IDENTIFY_DRIVE();
			break;
		default:
			Console.Error("DEV9: ATA: Unknown SCE command %x", regFeature);
			CmdNoDataAbort();
			return;
	}
}
//Has
//ATA_SCE_IDENTIFY_DRIVE @ 0xEC

//ATA_SCE_SECURITY_ERASE_PREPARE @ 0xF1
//ATA_SCE_SECURITY_ERASE_UNIT
//ATA_SCE_SECURITY_FREEZE_LOCK
//ATA_SCE_SECURITY_SET_PASSWORD
//ATA_SCE_SECURITY_UNLOCK

//ATA_SCE_SECURITY_WRITE_ID @ 0x20
//ATA_SCE_SECURITY_READ_ID @ 0x30

void ATA::SCE_IDENTIFY_DRIVE()
{
	PreCmd();

	// 0x80 byte buffer

	u8 hddId[128] = {0};

	memcpy(hddId, "Sony Computer Entertainment Inc.", 32);
	memcpy(hddId + 0x20, "SCPH-20401", 10); // Version the HDD was created with? I've seen "SCPH-20401" and "CEX-20401J"
	memcpy(hddId + 0x30, "  40", 4);

	hddId[0x40] = 0; // Seems to be some unique value but the 4th byte is always 2?
	hddId[0x41] = 0;
	hddId[0x42] = 0;
	hddId[0x43] = 0x02;

	hddId[0x46] = 0x1a; // I've seen 0x18, 0x19, 0x1a
	hddId[0x47] = 0x01;
	hddId[0x48] = 0x02;
	hddId[0x49] = 0x20;
	hddId[0x4c] = 0x01;
	hddId[0x4d] = 0x03;
	hddId[0x4e] = 0x11;
	hddId[0x4f] = 0x01;
	// 0x50 - 0x80 is a unique block of data

	#ifndef PCSX2_CORE
	// TODO: Rewrite this in a way to not use g_Conf for future Qt support
	auto fp = FileSystem::OpenManagedCFile(EmuConfig.DEV9.HddIdFile.c_str(), "rb");
	if (fp && FileSystem::FSize64(fp.get()) >= 128)
		std::fread(hddId, 1, 128, fp.get());
	#endif

	pioDRQEndTransferFunc = nullptr;
	DRQCmdPIODataToHost(hddId, 128, 0, 128, true);
}
