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

#include "ATA.h"
#include "DEV9/DEV9.h"

void ATA::WriteUInt16(u8* data, int* index, u16 value)
{
	*(u16*)&data[*index] = value;
	*index += sizeof(value);
}

void ATA::WriteUInt32(u8* data, int* index, u32 value)
{
	*(u32*)&data[*index] = value;
	*index += sizeof(value);
}

void ATA::WriteUInt64(u8* data, int* index, u64 value)
{
	*(u64*)&data[*index] = value;
	*index += sizeof(value);
}

//No null char
void ATA::WritePaddedString(u8* data, int* index, std::string value, u32 len)
{
	memset(&data[*index], (u8)' ', len);
	memcpy(&data[*index], value.c_str(), value.length() < len ? value.length() : len);
	*index += len;
}

void ATA::CreateHDDinfo(u64 sizeSectors)
{
	const u16 sectorSize = 512;
	DevCon.WriteLn("DEV9: HddSize : %i", sizeSectors * sectorSize / (1024 * 1024));
	const u64 nbSectors = sizeSectors;
	DevCon.WriteLn("DEV9: nbSectors : %i", nbSectors);

	memset(&identifyData, 0, sizeof(identifyData));
	//Defualt CHS translation
	const u16 defHeads = 16;
	const u16 defSectors = 63;
	u64 cylinderslong = std::min<u64>(nbSectors, 16514064) / defHeads / defSectors;
	const u16 defCylinders = (u16)std::min<u64>(cylinderslong, UINT16_MAX);

	//Curent CHS translation
	cylinderslong = std::min<u64>(nbSectors, 16514064) / curHeads / curSectors;
	curCylinders = (u16)std::min<u64>(cylinderslong, UINT16_MAX);

	const int curOldsize = curCylinders * curHeads * curSectors;
	//SET MAX ADDRESS will set the nbSectors reported

	//General configuration bit-significant information:
	/*
	 * 0x848A is for CFA devices
	 * bit 0: Resv                                          (all?)
	 * bit 1: Hard Sectored                                 (ATA-1)
	 * bit 2: Soft Sectored                                 (ATA-1) / Response incomplete (ATA-5,6,7,8)
	 * bit 3: Not MFM encoded                               (ATA-1)
	 * bit 4: Head switch time > 15 usec                    (ATA-1)
	 * bit 5: Spindle motor control option implemented      (ATA-1)
	 * bit 6: Non-Removable (Obsolete)                      (ATA-1,2,3,4,5)
	 * bit 7: Removable                                     (ATA-1,2,3,4,5,6,7,8)
	 * bit 8: disk transfer rate > 10Mbs                    (ATA-1)
	 * bit 9: disk transfer rate > 5Mbs but <= 10Mbs        (ATA-1)
	 * bit 10: disk transfer rate <= 5Mbs                   (ATA-1)
	 * bit 11: rotational speed tolerance is > 0.5%         (ATA-1)
	 * bit 12: data strobe offset option available          (ATA-1)
	 * bit 13: track offset option available                (ATA-1)
	 * bit 14: format speed tolerance gap required          (ATA-1)
	 * bit 15: 0 = ATA dev                                  (All?)
	 */
	int index = 0;
	WriteUInt16(identifyData, &index, 0x0040); //word 0
	//Default Num of cylinders
	WriteUInt16(identifyData, &index, defCylinders); //word 1
	//Specific configuration
	index += 1 * 2; //word 2
	//Default Num of heads (Retired)
	WriteUInt16(identifyData, &index, defHeads); //word 3
	//Number of unformatted bytes per track (Retired)
	WriteUInt16(identifyData, &index, (u16)(sectorSize * defSectors)); //word 4
	//Number of unformatted bytes per sector (Retired)
	WriteUInt16(identifyData, &index, sectorSize); //word 5
	//Default Number of sectors per track (Retired)
	WriteUInt16(identifyData, &index, defSectors); //word 6
	//Reserved for assignment by the CompactFlash™ Association
	index += 2 * 2; //word 7-8
	//Retired
	index += 1 * 2; //word 9
	//Serial number (20 ASCII characters)
	WritePaddedString(identifyData, &index, "PCSX2-DEV9-ATA-HDD", 20); //word 10-19
	//Buffer(cache) type (Retired)
	WriteUInt16(identifyData, &index, /*3*/ 0); //word 20
	//Buffer(cache) size in sectors (Retired)
	WriteUInt16(identifyData, &index, /*512*/ 0); //word 21
	//Number of ECC bytes available on read / write long commands (Obsolete)
	WriteUInt16(identifyData, &index, /*4*/ 0); //word 22
	//Firmware revision (8 ASCII characters)
	WritePaddedString(identifyData, &index, "FIRM100", 8); //word 23-26
	//Model number (40 ASCII characters)
	WritePaddedString(identifyData, &index, "PCSX2-DEV9-ATA-HDD", 40); //word 27-46
	//READ/WRITE MULI max sectors
	WriteUInt16(identifyData, &index, 128 & (0x80 << 8)); //word 47
	//Dword IO supported
	WriteUInt16(identifyData, &index, 1); //word 48
	//Capabilities
	/*
	 * bits 7-0: Retired
	 * bit 8: DMA supported
	 * bit 9: LBA supported
	 * bit 10:IORDY may be disabled
	 * bit 11:IORDY supported
	 * bit 12:Reserved
	 * bit 13:Standby timer values as specified in this standard are supported
	 */
	WriteUInt16(identifyData, &index, ((1 << 11) | (1 << 9) | (1 << 8))); //word 49
	//Capabilities (0-Shall be set to one to indicate a device specific Standby timer value minimum)
	index += 1 * 2; //word 50
	//PIO data transfer cycle timing mode (Obsolete)
	WriteUInt16(identifyData, &index, (u8)((pioMode > 2 ? pioMode : 2) << 8)); //word 51
	//DMA data transfer cycle timing mode (Obsolete)
	WriteUInt16(identifyData, &index, 0); //word 52
	//
	/*
	 * bit 0: Fields in 54:58 are valid (CHS sizes)(Obsolete)
	 * bit 1: Fields in 64:70 are valid (pio3,4 and MWDMA info)
	 * bit 2: Fields in 88 are valid    (UDMA modes)
	 */
	WriteUInt16(identifyData, &index, (1 | (1 << 1) | (1 << 2))); //word 53
	//Number of current cylinders
	WriteUInt16(identifyData, &index, curCylinders); //word 54
	//Number of current heads
	WriteUInt16(identifyData, &index, curHeads); //word 55
	//Number of current sectors per track
	WriteUInt16(identifyData, &index, curSectors); //word 56
	//Current capacity in sectors
	WriteUInt32(identifyData, &index, (u32)curOldsize); //word 57-58
	//PIO READ/WRITE Multiple setting
	/*
	 * bit 7-0: Current setting for number of logical sectors that shall be transferred per DRQ
	 *			data block on READ/WRITE Multiple commands
	 * bit 8: Multiple sector setting is valid
	 */
	WriteUInt16(identifyData, &index, (u16)(curMultipleSectorsSetting | (1 << 8))); //word 59
	//Total number of user addressable logical sectors
	WriteUInt32(identifyData, &index, (u32)(nbSectors < 268435456 ? nbSectors : 268435456)); //word 60-61
	//DMA modes
	/*
	 * bits 0-7: Singleword modes supported (0,1,2)
	 * bits 8-15: Transfer mode active
	 */
	if (sdmaMode > 0)
		WriteUInt16(identifyData, &index, (u16)(0x07 | (1 << (sdmaMode + 8)))); //word 62
	else
		WriteUInt16(identifyData, &index, 0x07); //word 62
	//DMA Modes
	/*
	 * bits 0-7: Multiword modes supported (0,1,2)
	 * bits 8-15: Transfer mode active
	 */
	if (mdmaMode > 0)
		WriteUInt16(identifyData, &index, (u16)(0x07 | (1 << (mdmaMode + 8)))); //word 63
	else
		WriteUInt16(identifyData, &index, 0x07); //word 63
	//Bit 0-7-PIO modes supported (0,1,2,3,4)
	WriteUInt16(identifyData, &index, 0x1F); //word 64 (pio3,4 supported) selection not reported here
	//Minimum Multiword DMA transfer cycle time per word
	WriteUInt16(identifyData, &index, 80); //word 65
	//Manufacturer’s recommended Multiword DMA transfer cycle time
	WriteUInt16(identifyData, &index, 80); //word 66
	//Minimum PIO transfer cycle time without flow control
	WriteUInt16(identifyData, &index, 120); //word 67
	//Minimum PIO transfer cycle time with IORDY flow control
	WriteUInt16(identifyData, &index, 120); //word 68
	//Reserved
	//69-70
	//Reserved
	//71-74
	//Queue depth (4bit, Maximum queue depth - 1)
	//75
	//Reserved
	//76-79
	index = 80 * 2;
	//Major revision number (1-3-Obsolete, 4-7-ATA4-7 supported)
	WriteUInt16(identifyData, &index, 0x70); //word 80
	//Minor revision number
	WriteUInt16(identifyData, &index, 0); //word 81
	//Supported Feature Sets (82)
	/*
	 * bit 0: Smart
	 * bit 1: Security Mode
	 * bit 2: Removable media feature set
	 * bit 3: Power management
	 * bit 4: Packet (the CD features)
	 * bit 5: Write cache
	 * bit 6: Look-ahead
	 * bit 7: Release interrupt
	 * bit 8: SERVICE interrupt
	 * bit 9: DEVICE RESET interrupt
	 * bit 10: Host Protected Area
	 * bit 11: (Obsolete)
	 * bit 12: WRITE BUFFER command
	 * bit 13: READ BUFFER command
	 * bit 14: NOP
	 * bit 15: (Obsolete)
	 */
	WriteUInt16(identifyData, &index, (u16)((1 << 14) | (1 << 5) | /*(1 << 1) | (1 << 10) |*/ 1)); //word 82
	//Supported Feature Sets (83)
	/*
	 * bit 0: DOWNLOAD MICROCODE
	 * bit 1: READ/WRITE DMA QUEUED
	 * bit 2: CFA (Card reader)
	 * bit 3: Advanced Power Management
	 * bit 4: Removable Media Status Notifications
	 * bit 5: Power-Up Standby
	 * bit 6: SET FEATURES required to spin up after power-up
	 * bit 7: ??
	 * bit 8: SET MAX security extension
	 * bit 9: Automatic Acoustic Management
	 * bit 10: 48bit LBA
	 * bit 11: Device Configuration Overlay
	 * bit 12: FLUSH CACHE
	 * bit 13: FLUSH CACHE EXT
	 * bit 14: 1
	 */
	WriteUInt16(identifyData, &index, (u16)((1 << 14) | (1 << 13) | (1 << 12) /*| (1 << 8)*/ | ((lba48Supported ? 1 : 0) << 10))); //word 83
	//Supported Feature Sets (84)
	/*
	 * bit 0: Smart error logging
	 * bit 1: smart self-test
	 * bit 2: Media serial number
	 * bit 3: Media Card Pass Though
	 * bit 4: Streaming feature set
	 * bit 5: General Purpose Logging
	 * bit 6: WRITE DMA FUA EXT & WRITE MULTIPLE FUA EXT
	 * bit 7: WRITE DMA QUEUED FUA EXT
	 * bit 8: 64bit World Wide Name
	 * bit 9: URG bit supported for WRITE STREAM DMA EXT amd WRITE STREAM EXT
	 * bit 10: URG bit supported for READ STREAM DMA EXT amd READ STREAM EXT
	 * bit 13: IDLE IMMEDIATE with UNLOAD FEATURE
	 * bit 14: 1
	 */
	WriteUInt16(identifyData, &index, (u16)((1 << 14) | (1 << 1) | 1)); //word 84
	//Command set/feature enabled/supported (See word 82)
	WriteUInt16(identifyData, &index, (u16)((fetSmartEnabled << 0) | (fetSecurityEnabled << 1) | (fetWriteCacheEnabled << 5) | (fetHostProtectedAreaEnabled << 10) | (1 << 14))); //Fixed      //word 85
	//Command set/feature enabled/supported (See word 83)
	// clang-format off
	WriteUInt16(identifyData, &index, (u16)(
		/*(1 << 8) |						//SET MAX */
		((lba48Supported ? 1 : 0) << 10) |	//Fixed
		(1 << 12) |							//Fixed
		(1 << 13)));						//Fixed      //word 86
	//Command set/feature enabled/supported (See word 84)
	WriteUInt16(identifyData, &index, (u16)((1 << 14) | (1 << 1) | 1));
	WriteUInt16(identifyData, &index, (u16)(
		(1) |								//Fixed
		((1) << 1)));						//Fixed      //word 87
	// clang-format on
	//UDMA modes
	/*
	 * bits 0-7: ultraword modes supported (0,1,2,4,5,6,7)
	 * bits 8-15: Transfer mode active
	 */
	if (udmaMode > 0)
		WriteUInt16(identifyData, &index, (u16)(0x7f | (1 << (udmaMode + 8)))); //word 88
	else
		WriteUInt16(identifyData, &index, 0x7f); //word 88
	//Time required for security erase unit completion
	//89
	//Time required for Enhanced security erase completion
	//90
	//Current advanced power management value
	//91
	//Master Password Identifier
	//92
	//Hardware reset result. The contents of bits (12:0) of this word shall change only during the execution of a hardware reset.
	/*
	 * bit 0: 1
	 * bit 1-2: How Dev0 determined Dev number (11 = unk)
	 * bit 3: Dev 0 Passes Diag
	 * bit 4: Dev 0 Detected assertion of PDIAG
	 * bit 5: Dev 0 Detected assertion of DSAP
	 * bit 6: Dev 0 Responds when Dev1 is selected
	 * bit 7: Reserved
	 * bit 8: 1
	 * bit 9-10: How Dev1 determined Dev number
	 * bit 11: Dev1 asserted 1
	 * bit 12: Reserved
	 * bit 13: Dev detected CBLID above Vih
	 * bit 14: 1
	 */
	index = 93 * 2;
	WriteUInt16(identifyData, &index, (u16)(1 | (1 << 14) | 0x2000)); //word 93
	//Vendor’s recommended acoustic management value.
	//94
	//Stream Minimum Request Size
	//95
	//Streaming Transfer Time - DMA
	//96
	//Streaming Access Latency - DMA and PIO
	//97
	//Streaming Performance Granularity
	//98-99
	//Total Number of User Addressable Sectors for the 48-bit Address feature set.
	index = 100 * 2;
	WriteUInt64(identifyData, &index, (u16)nbSectors);
	index -= 2;
	WriteUInt16(identifyData, &index, 0); //truncate to 48bits
	//Streaming Transfer Time - PIO
	//104
	//Reserved
	//105
	//Physical sector size / Logical Sector Size
	/*
	 * bit 0-3: 2^X logical sectors per physical sector
	 * bit 12: Logical sector longer than 512 bytes
	 * bit 13: multiple logical sectors per physical sector
	 * bit 14: 1
	 */
	index = 106 * 2;
	WriteUInt16(identifyData, &index, (u16)((1 << 14) | 0));
	//Inter-seek delay for ISO-7779acoustic testing in microseconds
	//107
	//WNN
	//108-111
	//Reserved
	//112-115
	//Reserved
	//116
	//Words per Logical Sector
	//117-118
	//Reserved
	//119-126
	//Removable Media Status Notification feature support
	//127
	//Security status
	/*
	 * bit 0: Security supported
	 * bit 1: Security enabled
	 * bit 2: Security locked
	 * bit 3: Security frozen
	 * bit 4: Security count expired
	 * bit 5: Enhanced erase supported
	 * bit 6-7: reserved
	 * bit 8: is Maximum Security
	 */
	//Vendor Specific
	//129-159
	//CFA power mode 1
	//160
	//Reserved for CFA
	//161-175
	//Current media serial number (60 ASCII characters)
	//176-205
	//Reserved
	//206-254
	//Integrity word
	//15:8 Checksum, 7:0 Signature
	CreateHDDinfoCsum();
}
void ATA::CreateHDDinfoCsum() //Is this correct?
{
	u8 counter = 0;

	for (int i = 0; i < (512 - 1); i++)
		counter += identifyData[i];

	counter += 0xA5;

	identifyData[510] = 0xA5;
	identifyData[511] = (u8)(255 - counter + 1);
	counter = 0;

	for (int i = 0; i < (512); i++)
		counter += identifyData[i];

	DevCon.WriteLn("DEV9: %i", counter);
}
