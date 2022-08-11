/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

const u32 ThreadListInstructions[3] =
{
	0xac420000, // sw v0,0x0(v0)
	0x00000000, // no-op
	0x00000000, // no-op
};

struct BiosDebugInformation
{
	u32 threadListAddr;
};

// The following two arrays are used for Qt
[[maybe_unused]] static const char* BiosZoneStrings[]{
	"Japan",
	"USA",
	"Europe",
	"Oceania", // cannot be detected from rom0:ROMVER, can be detected with rom1:DVDVER
	"Asia", // it shares 3 regions: Korea (SCPH-xxx05), Hong Kong (SCPH-xxx06), Taiwan (SCPH-xxx07)
	"Russia", // cannot be detected from rom0:ROMVER, can be detected with rom1:DVDVER
	"China",
	"Mexico", // cannot be detected from rom0:ROMVER, can be detected with rom1:DVDVER
	"T10K", // Japan region actually
	"Test", // Japan region actually
	"Free",
	nullptr,
};

[[maybe_unused]] static const char* BiosZoneBytes[]
{
	"J", "A", "E", "E", "H", "E", "C", "T", "A", "X", "P", nullptr
};

extern BiosDebugInformation CurrentBiosInformation;
extern u32 BiosVersion;		// Used by CDVD
extern u32 BiosRegion;		// Used by CDVD
extern bool NoOSD;			// Used for HLE OSD Config Params
extern bool AllowParams1;
extern bool AllowParams2;
extern u32 BiosChecksum;
extern std::string BiosDescription;
extern std::string BiosZone;
extern std::string BiosPath;
extern bool LoadBIOS();
extern bool IsBIOS(const char* filename, u32& version, std::string& description, u32& region, std::string& zone);
extern bool IsBIOSAvailable(const std::string& full_path);
