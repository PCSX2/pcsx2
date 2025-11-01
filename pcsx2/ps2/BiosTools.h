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
			/*21*/ u32 timezoneOffset : 11;
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

constexpr const char* TimeZoneLocations[128] = {
    "Kabul",  												// Afghanistan				[0]
    "Tirana", 												// Albania					[1]
    "Algiers", 												// Algeria					[2]	
	"Andorra la Vella", 									// Andorra					[3]
	"Yerevan", 												// Armenia					[4]
	"Perth", "Adelaide", "Sydney", "Lord Howe Island", 	 	// Australia				[5, 6, 7, 8]
	"Vienna",												// Austria					[9]
	"Baku",													// Azerbaijan				[10]
	"Manama",												// Bahrain					[11]
	"Dhaka",												// Bangladesh				[12]
	"Minsk",												// Belarus					[13]
	"Brussels",												// Belgium					[14]
	"Sarajevo",												// Bosnia and Herzegovina	[15]
	"Sofia",												// Bulgaria					[16]
	"Pacific (Canada)", "Mountain (Canada)",
	"Central (Canada)", "Eastern (Canada)",
	"Atlantic (Canada)", "Newfoundland",					// Canada					[17, 18, 19, 20, 21, 22]
	"Praia",												// Cape Verde				[23]
	"Santiago", "Easter Island",							// Chile					[24, 25]
	"Beijing",												// China					[26]
	"Zagreb",												// Croatia					[27]
	"Nicosia",												// Cyprus					[28]
	"Prague",												// Czech Republic			[29]
	"Copenhagen",											// Denmark					[30]
	"Cairo",												// Egypt					[31]
	"Tallinn",												// Estonia					[32]
	"Suva",													// Fiji						[33]
	"Helsinki",												// Finland					[34]
	"Paris",												// France					[35]
	"Tbilisi",												// Georgia					[36]
	"Berlin",												// Germany					[37]
	"Gibraltar",											// Gibraltar				[38]
	"Athens",												// Greece					[39]
	"Northwestern Greenland", "Southwestern Greenland",
	"Eastern Greenland",									// Greenland				[40, 41, 42]
	"Budapest",												// Hungary					[43]
	"Reykjavik",											// Iceland					[44]
	"Calcutta",												// India					[45]
	"Tehran",												// Iran						[46]
	"Baghdad",												// Iraq						[47]
	"Dublin",												// Ireland					[48]
	"Jerusalem",											// Israel					[49]
	"Rome",													// Italy					[50]
	"Tokyo",												// Japan					[51]
	"Amman",												// Jordan					[52]
	"Western Kazakhstan", "Central Kazakhstan",
	"Eastern Kazakhstan",									// Kazakhstan				[53, 54, 55]
	"Kuwait City",											// Kuwait					[56]
	"Bishkek",												// Kyrgyzstan				[57]
	"Riga",													// Latvia					[58]
	"Beirut",												// Lebanon					[59]
	"Vaduz",												// Liechtenstein			[60]
	"Vilnius",												// Lithuania				[61]
	"Luxembourg",											// Luxembourg				[62]
	"Skopje",												// Macedonia				[63]
	"Valletta",												// Malta					[64]
	"Tijuana", "Chihuahua", "Mexico City",					// Mexico					[65, 66, 67]
	"Midway Islands",										// Midway Islands			[68]
	"Monaco",												// Monaco					[69]
	"Casablanca",											// Morocco					[70]
	"Windhoek",												// Namibia					[71]
	"Kathmandu",											// Nepal					[72]
	"Amsterdam",											// Netherlands, The			[73]
	"New Caledonia",										// New Caledonia			[74]
	"Wellington",											// New Zealand				[75]
	"Oslo",													// Norway					[76]
	"Muscat",												// Oman						[77]
	"Karachi",												// Pakistan					[78]
	"Panama City",											// Panama					[79]
	"Warsaw",												// Poland					[80]
	"Azores", "Lisbon",										// Portugal					[81, 82]
	"Puerto Rico",											// Puerto Rico				[83]
	"Reunion",												// Réunion					[84]
	"Bucharest",											// Romania					[85]
	"Kaliningrad", "Moscow", "Izhevsk", "Perm", "Omsk",
	"Norilsk", "Bratsk", "Yakutsk", "Vladivostok",
	"Magadan", "Petropavlovsk-Kamchatsky",					// Russia 					[86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96]
	"Samoa Islands",										// Samoa Islands 			[97]
	"San Marino",											// San Marino 				[98]
	"Riyadh",												// Saudi Arabia 			[99]
	"Bratislava",											// Slovakia 				[100]
	"Ljubljana",											// Slovenia 				[101]
	"Johannesburg",											// South Africa 			[102]
	"Canary Islands", "Madrid",								// Spain 					[103, 104]
	"Stockholm",											// Sweden 					[105]
	"Bern",													// Switzerland 				[106]
	"Damascus",												// Syria					[107]
	"Tunis",												// Tunisia					[108]
	"Istanbul",												// Turkey					[109]
	"Kiev",													// Ukraine					[110]
	"Abu Dhabi",											// United Arab Emirates		[111]
	"London",												// United Kingdom			[112]
	"Hawaii", "Alaska", "Pacific (USA)", "Mountain (USA)",
	"Central (USA)", "Eastern (USA)",						// United States of America [113, 114, 115, 116, 117, 118]
	"Tashkent",												// Uzbekistan				[119]
	"Caracas",												// Venezuela 				[120]
	"Belgrade",												// Yugoslavia 				[121]
	"Bangkok",												// Thailand 				[122]
	"Hong Kong",											// Hong Kong 				[123]
	"Kuala Lumpur",											// Malaysia					[124]
	"Singapore",											// Singapore				[125]
	"Taipei",												// Taiwan					[126]
	"Seoul",												// South Korea				[127]
};
