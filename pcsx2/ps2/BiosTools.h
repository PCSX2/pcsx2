// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

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
			/*21*/ s32 timezoneOffset : 11;
 			/** timezone location ID */
			/*32*/ u8 timeZoneID : 7;
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
			/*04*/ u8 daylightSavings : 1;
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
extern u32 BiosVersion;		// Used by CDVD; third octet is major version, fourth is minor version.
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

// Values in [][0] are the names on BIOSes v1.70 and below.
// Values in [][1] are the names on BIOSes v1.90 and above.
// ID 127 ("Seoul" in v01.90 and above) appears to have no corresponding value ("South Korea") on v01.70 and below.
// clang-format off
constexpr const char* TimeZoneLocations[128][2] = {
	{"Afghanistan", "Kabul"},  														// [0]
	{"Albania", "Tirana"}, 															// [1]
	{"Algeria", "Algiers"}, 														// [2]
	{"Andorra", "Andorra la Vella"}, 												// [3]
	{"Armenia", "Yerevan"}, 														// [4]
	{"Australia – Perth", "Perth"},													// [5]
	{"Australia – Adelaide", "Adelaide"},											// [6]
	{"Australia – Sydney", "Sydney"},												// [7]
	{"Australia – Lord Howe Island", "Lord Howe Island"},							// [8]
	{"Austria", "Vienna"},															// [9]
	{"Azerbaijan", "Baku"},															// [10]
	{"Bahrain", "Manama"},															// [11]
	{"Bangladesh", "Dhaka"},														// [12]
	{"Belarus", "Minsk"},															// [13]
	{"Belgium", "Brussels"},														// [14]
	{"Bosnia and Herzegovina", "Sarajevo"},											// [15]
	{"Bulgaria", "Sofia"},															// [16]
	{"Canada – Pacific, Yukon", "Pacific (Canada)"},								// [17]
	{"Canada – Mountain", "Mountain (Canada)"},										// [18]
	{"Canada – Central", "Central (Canada)"},										// [19]
	{"Canada – Eastern", "Eastern (Canada)"},										// [20]
	{"Canada – Atlantic", "Atlantic (Canada)"},										// [21]
	{"Canada – Newfoundland", "Newfoundland"},										// [22]
	{"Cape Verde", "Praia"},														// [23]
	{"Chile – Santiago", "Santiago"},												// [24]
	{"Chile – Easter Island", "Easter Island"},										// [25]
	{"China", "Beijing"},															// [26]
	{"Croatia", "Zagreb"},															// [27]
	{"Cyprus", "Nicosia"},															// [28]
	{"Czech Republic", "Prague"},													// [29]
	{"Denmark", "Copenhagen"},														// [30]
	{"Egypt", "Cairo"},																// [31]
	{"Estonia", "Tallinn"},															// [32]
	{"Fiji", "Suva"},																// [33]
	{"Finland", "Helsinki"},														// [34]
	{"France", "Paris"},															// [35]
	{"Georgia", "Tbilisi"},															// [36]
	{"Germany", "Berlin"},															// [37]
	{"Gibraltar", "Gibraltar"},														// [38]
	{"Greece", "Athens"},															// [39]
	{"Greenland – Pituffik", "Northwestern Greenland"},								// [40]
	{"Greenland – Greenland", "Southwestern Greenland"},							// [41]
	{"Greenland – Ittoqqortoormiit", "Eastern Greenland"},							// [42]
	{"Hungary", "Budapest"},														// [43]
	{"Iceland", "Reykjavik"},														// [44]
	{"India", "Calcutta"},															// [45]
	{"Iran", "Tehran"},																// [46]
	{"Iraq", "Baghdad"},															// [47]
	{"Ireland", "Dublin"},															// [48]
	{"Israel", "Jerusalem"},														// [49]
	{"Italy", "Rome"},																// [50]
	{"Japan", "Tokyo"},																// [51]
	{"Jordan", "Amman"},															// [52]
	{"Kazakhstan – Western", "Western Kazakhstan"},									// [53]
	{"Kazakhstan – Central", "Central Kazakhstan"},									// [54]
	{"Kazakhstan – Eastern", "Eastern Kazakhstan"},									// [55]
	{"Kuwait", "Kuwait City"},														// [56]
	{"Kyrgyzstan", "Bishkek"},														// [57]
	{"Latvia", "Riga"},																// [58]
	{"Lebanon", "Beirut"},															// [59]
	{"Liechtenstein", "Vaduz"},														// [60]
	{"Lithuania", "Vilnius"},														// [61]
	{"Luxembourg", "Luxembourg"},													// [62]
	{"Macedonia", "Skopje"},														// [63]
	{"Malta", "Valletta"},															// [64]
	{"Mexico – Tijuana", "Tijuana"},												// [65]
	{"Mexico – Chihuahua", "Chihuahua"},											// [66]
	{"Mexico – Mexico City", "Mexico City"},										// [67]
	{"Midway Islands", "Midway Islands"},											// [68]
	{"Monaco", "Monaco"},															// [69]
	{"Morocco", "Casablanca"},														// [70]
	{"Namibia", "Windhoek"},														// [71]
	{"Nepal", "Kathmandu"},															// [72]
	{"Netherlands", "Amsterdam"},													// [73]
	{"New Caledonia", "New Caledonia"},												// [74]
	{"New Zealand", "Wellington"},													// [75]
	{"Norway", "Oslo"},																// [76]
	{"Oman", "Muscat"},																// [77]
	{"Pakistan", "Karachi"},														// [78]
	{"Panama", "Panama City"},														// [79]
	{"Poland", "Warsaw"},															// [80]
	{"Portugal – Azores", "Azores"},												// [81]
	{"Portugal – Lisbon", "Lisbon"},												// [82]
	{"Puerto Rico", "Puerto Rico"},													// [83]
	{"Reunion", "Reunion"},															// [84]
	{"Romania", "Bucharest"},														// [85]
	{"Russian Federation – Kaliningrad", "Kaliningrad"},							// [86]
	{"Russian Federation – Moscow", "Moscow"},										// [87]
	{"Russian Federation – Izhevsk", "Izhevsk"},									// [88]
	{"Russian Federation – Perm", "Perm"},											// [89]
	{"Russian Federation – Omsk", "Omsk"},											// [90]
	{"Russian Federation – Norilsk", "Norilsk"},									// [91]
	{"Russian Federation – Bratsk", "Bratsk"},										// [92]
	{"Russian Federation – Yakutsk", "Yakutsk"},									// [93]
	{"Russian Federation – Vladivostok", "Vladivostok"},							// [94]
	{"Russian Federation – Magadan", "Magadan"},									// [95]
	{"Russian Federation – Petropavlovsk-Kamchatsky", "Petropavlovsk-Kamchatsky"},	// [96]
	{"Samoa", "Samoa Islands"},														// [97]
	{"San Marino", "San Marino"},													// [98]
	{"Saudi Arabia", "Riyadh"},														// [99]
	{"Slovakia", "Bratislava"},														// [100]
	{"Slovenia", "Ljubljana"},														// [101]
	{"South Africa", "Johannesburg"},												// [102]
	{"Spain – Canary Islands", "Canary Islands"},									// [103]
	{"Spain – Madrid", "Madrid"},													// [104]
	{"Sweden", "Stockholm"},														// [105]
	{"Switzerland", "Bern"},														// [106]
	{"Syria", "Damascus"},															// [107]
	{"Tunisia", "Tunis"},															// [108]
	{"Turkey", "Istanbul"},															// [109]
	{"Ukraine", "Kiev"},															// [110]
	{"United Arab Emirates", "Abu Dhabi"},											// [111]
	{"United Kingdom", "London"},													// [112]
	{"United States – Hawaii", "Hawaii"},											// [113]
	{"United States – Alaska", "Alaska"},											// [114]
	{"United States – Pacific", "Pacific (USA)"},									// [115]
	{"United States – Mountain", "Mountain (USA)"},									// [116]
	{"United States – Central", "Central (USA)"},									// [117]
	{"United States – Eastern", "Eastern (USA)"},									// [118]
	{"Uzbekistan", "Tashkent"},														// [119]
	{"Venezuela", "Caracas"},														// [120]
	{"Yugoslavia", "Belgrade"},														// [121]
	{"Thailand", "Bangkok"},														// [122]
	{"Hong Kong", "Hong Kong"},														// [123]
	{"Malaysia", "Kuala Lumpur"},													// [124]
	{"Singapore", "Singapore"},														// [125]
	{"Taiwan", "Taipei"},															// [126]
	{"South Korea", "Seoul"},														// [127]
};
// clang-format on
