// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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

// Obained from the Open PS2SDK here https://github.com/ps2dev/ps2sdk/blob/master/ee/kernel/include/osd_config.h
// Only used for HLEing ConfigParam Syscalls in fast boot
typedef struct
{
	union
	{
		struct
		{
			/** 0=enabled, 1=disabled */
			/*00*/ u32 spdifMode : 1;
			/** 0=4:3, 1=fullscreen, 2=16:9 */
			/*01*/ u32 screenType : 2;
			/** 0=rgb(scart), 1=component */
			/*03*/ u32 videoOutput : 1;
			/** 0=japanese, 1=english(non-japanese) */
			/*04*/ u32 japLanguage : 1;
			/** Playstation driver settings. */
			/*05*/ u32 ps1drvConfig : 8;
			/** 0 = early Japanese OSD, 1 = OSD2, 2 = OSD2 with extended languages. Early kernels cannot retain the value set in this field (Hence always 0). */
			/*13*/ u32 version : 3;
			/** LANGUAGE_??? value */
			/*16*/ u32 language : 5;
			/** timezone minutes offset from gmt */
			/*21*/ u32 timezoneOffset : 11;
		};

		u8  UC[4];
		u16 US[2];
		u32 UL[1];
	};
} ConfigParam;

typedef struct
{
	union
	{
		struct
		{
			// This value is unknown, seems to be set to zero by default
			/*00*/ u8 format;

			/*00*/ u8 reserved : 4;
			/** 0=standard(winter), 1=daylight savings(summer) */
			/*04*/ u8 daylightSaving : 1;
			/** 0=24 hour, 1=12 hour */
			/*05*/ u8 timeFormat : 1;
			/** 0=YYYYMMDD, 1=MMDDYYYY, 2=DDMMYYYY */
			/*06*/ u8 dateFormat : 2;

			// Only used if ConfigParam.version = 2
			/** Set to 2 */
			/*00*/ u8 version;
			/** The true language, unlike the one from ConfigParam */
			/*00*/ u8 language;
		};

		u8  UC[4];
		u16 US[2];
		u32 UL[1];
	};
} Config2Param;
// End of OpenSDK struct

// TODO: namespace this
extern BiosDebugInformation CurrentBiosInformation;
extern u32 BiosVersion;		// Used by CDVD
extern u32 BiosRegion;		// Used by CDVD
extern bool NoOSD;			// Used for HLE OSD Config Params
extern ConfigParam configParams1;
extern Config2Param configParams2;
extern bool ParamsRead;
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

extern void ReadOSDConfigParames();

extern bool IsBIOS(const char* filename, u32& version, std::string& description, u32& region, std::string& zone);
extern bool IsBIOSAvailable(const std::string& full_path);

extern bool LoadBIOS();
extern void CopyBIOSToMemory();
