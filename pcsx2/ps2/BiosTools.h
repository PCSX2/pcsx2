/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#pragma once

#include <string>
#include <vector>

const u32 ThreadListInstructions[3] =
{
	0xac420000, // sw v0,0x0(v0)
	0x00000000, // no-op
	0x00000000, // no-op
};

struct BiosDebugInformation
{
	u32 eeThreadListAddr;
	u32 iopThreadListAddr;
	u32 iopModListAddr;
};

// TODO: namespace this
extern BiosDebugInformation CurrentBiosInformation;
extern u32 BiosVersion;		// Used by CDVD
extern u32 BiosRegion;		// Used by CDVD
extern bool NoOSD;			// Used for HLE OSD Config Params
extern bool AllowParams1;
extern bool AllowParams2;
extern u32 BiosChecksum;
extern std::string BiosDescription;
extern std::string BiosZone;

// This function returns part of EXTINFO data of the BIOS rom
// This module contains information about Sony build environment at offst 0x10
// first 15 symbols is build date/time that is unique per rom and can be used as unique serial
// Example for romver 0160EC20010704
// 20010704-160707,ROMconf,PS20160EC20010704.bin,kuma@rom-server/~/f10k/g/app/rom
// 20010704-160707 can be used as unique ID for Bios
extern std::string BiosSerial;
extern std::string BiosPath;

// Copies of the BIOS ROM. Because the EE can write to the ROM area, we need to copy it on reset.
// If we ever support read-only physical mappings, we can remove this.
extern std::vector<u8> BiosRom;

extern bool IsBIOS(const char* filename, u32& version, std::string& description, u32& region, std::string& zone);
extern bool IsBIOSAvailable(const std::string& full_path);

extern bool LoadBIOS();
extern void CopyBIOSToMemory();
