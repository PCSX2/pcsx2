// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "DEV9/ATA/ATA.h"
#include "DEV9/DEV9.h"

void ATA::HDD_SCE()
{
	DevCon.WriteLn("DEV9: HDD_SCE SONY-SPECIFIC SECURITY CONTROL COMMAND %x", regFeature);

	switch (regFeature)
	{
		case 0xF1: // ATA_SCE_SECURITY_SET_PASSWORD
		case 0xF2: // ATA_SCE_SECURITY_UNLOCK
		case 0xF3: // ATA_SCE_SECURITY_ERASE_PREPARE
		case 0xF4: // ATA_SCE_SECURITY_ERASE_UNIT
		case 0xF5: // ATA_SCE_SECURITY_FREEZE_LOCK
		case 0x20: // ATA_SCE_SECURITY_READ_ID
		case 0x30: // ATA_SCE_SECURITY_WRITE_ID
			Console.Error("DEV9: ATA: SCE command %x not implemented", regFeature);
			CmdNoDataAbort();
			break;
		case 0xEC:
			SCE_IDENTIFY_DRIVE();
			break;
		default:
			Console.Error("DEV9: ATA: Unknown SCE command %x", regFeature);
			CmdNoDataAbort();
			return;
	}
}
// All games that have ability to install data into HDD will verify HDD by checking that this command completes successfully. Resident Evil: Outbreak for example
// Only a few games/apps make use of the returned data, see Final Fantasy XI or the HDD Utility disks, neither of which work yet
// Also PSX DESR bioses use this response for HDD encryption and decryption.
// Use of external HDD ID (not implemented) file may be necessary for users with protected titles installed to the SCE HDD and then dumped.
// For example: PS2 BB Navigator, PlayOnline Viewer, Bomberman Online, Nobunaga No Yabou Online, Pop'n Taisen Puzzle-Dama Online

// PS2 ID Dumper can be used as test case
void ATA::SCE_IDENTIFY_DRIVE()
{
	PreCmd();

	// fill sceSec response with default data
	memcpy(sceSec, "Sony Computer Entertainment Inc.", 32); // Always this magic header.
	memcpy(sceSec + 0x20, "SCPH-20401", 10); // sometimes this matches HDD model, the rest 6 bytes filles with zeroes, or sometimes with spaces
	memcpy(sceSec + 0x30, "  40", 4); // or " 120" for PSX DESR, reference for ps2 area size. The rest bytes filled with zeroes

	sceSec[0x40] = 0; // 0x40 - 0x43 - 4-byte HDD internal SCE serial, does not match real HDD serial, currently hardcoded to 0x1000000
	sceSec[0x41] = 0;
	sceSec[0x42] = 0;
	sceSec[0x43] = 0x01;

	// purpose of next 12 bytes is unknown
	sceSec[0x44] = 0; // always zero
	sceSec[0x45] = 0; // always zero
	sceSec[0x46] = 0x1a;
	sceSec[0x47] = 0x01;
	sceSec[0x48] = 0x02;
	sceSec[0x49] = 0x20;
	sceSec[0x4a] = 0; // always zero
	sceSec[0x4b] = 0; // always zero
	// next 4 bytes always these values
	sceSec[0x4c] = 0x01;
	sceSec[0x4d] = 0x03;
	sceSec[0x4e] = 0x11;
	sceSec[0x4f] = 0x01;
	// 0x50 - 0x80 is a random unique block of data
	// 0x80 and up - zero filled

	// TODO: if exists *.hddid file (size - 128-512 bytes) along with HDD image, replace generic sceSec with its content

	pioDRQEndTransferFunc = nullptr;
	DRQCmdPIODataToHost(sceSec, 256 * 2, 0, 256 * 2, true);
}
