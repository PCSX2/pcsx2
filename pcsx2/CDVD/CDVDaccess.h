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

typedef struct _cdvdSubQ
{
	u8 ctrl : 4;   // control and mode bits
	u8 mode : 4;   // control and mode bits
	u8 trackNum;   // current track number (1 to 99)
	u8 trackIndex; // current index within track (0 to 99)
	u8 trackM;     // current minute location on the disc (BCD encoded)
	u8 trackS;     // current sector location on the disc (BCD encoded)
	u8 trackF;     // current frame location on the disc (BCD encoded)
	u8 pad;        // unused
	u8 discM;      // current minute offset from first track (BCD encoded)
	u8 discS;      // current sector offset from first track (BCD encoded)
	u8 discF;      // current frame offset from first track (BCD encoded)
} cdvdSubQ;

typedef struct _cdvdTD
{ // NOT bcd coded
	u32 lsn;
	u8 type;
} cdvdTD;

typedef struct _cdvdTN
{
	u8 strack; //number of the first track (usually 1)
	u8 etrack; //number of the last track
} cdvdTN;

// SpindleCtrl Masks
#define CDVD_SPINDLE_SPEED 0x7  // Speed ranges from 0-3 (1, 2, 3, 4x for DVD) and 0-5 (1, 2, 4, 12, 24x for CD)
#define CDVD_SPINDLE_NOMINAL 0x40 // Changes the speed to be constant (CLV) based on current speed
#define CDVD_SPINDLE_CAV 0x80  // CAV/CLV selector

// CDVDreadTrack mode values:
#define CDVD_MODE_2352 0 // full 2352 bytes
#define CDVD_MODE_2340 1 // skip sync (12) bytes
#define CDVD_MODE_2328 2 // skip sync+head+sub (24) bytes
#define CDVD_MODE_2048 3 // skip sync+head+sub (24) bytes
#define CDVD_MODE_2368 4 // full 2352 bytes + 16 subq

// CDVDgetDiskType returns:
#define CDVD_TYPE_ILLEGAL 0xff   // Illegal Disc
#define CDVD_TYPE_DVDV 0xfe      // DVD Video
#define CDVD_TYPE_CDDA 0xfd      // Audio CD
#define CDVD_TYPE_PS2DVD 0x14    // PS2 DVD
#define CDVD_TYPE_PS2CDDA 0x13   // PS2 CD (with audio)
#define CDVD_TYPE_PS2CD 0x12     // PS2 CD
#define CDVD_TYPE_PSCDDA 0x11    // PS CD (with audio)
#define CDVD_TYPE_PSCD 0x10      // PS CD
#define CDVD_TYPE_UNKNOWN 0x05   // Unknown
#define CDVD_TYPE_DETCTDVDD 0x04 // Detecting Dvd Dual Sided
#define CDVD_TYPE_DETCTDVDS 0x03 // Detecting Dvd Single Sided
#define CDVD_TYPE_DETCTCD 0x02   // Detecting Cd
#define CDVD_TYPE_DETCT 0x01     // Detecting
#define CDVD_TYPE_NODISC 0x00    // No Disc

// CDVDgetTrayStatus returns:
#define CDVD_TRAY_CLOSE 0x00
#define CDVD_TRAY_OPEN 0x01

// cdvdTD.type (track types for cds)
#define CDVD_AUDIO_TRACK 0x01
#define CDVD_MODE1_TRACK 0x41
#define CDVD_MODE2_TRACK 0x61

#define CDVD_AUDIO_MASK 0x00
#define CDVD_DATA_MASK 0x40
//	CDROM_DATA_TRACK	0x04	//do not enable this! (from linux kernel)

// CDVD
// NOTE: The read/write functions CANNOT use XMM/MMX regs
// If you want to use them, need to save and restore current ones
typedef s32(CALLBACK* _CDVDopen)(const char* pTitleFilename);

// Initiates an asynchronous track read operation.
// Returns -1 on error (invalid track)
// Returns 0 on success.
typedef s32(CALLBACK* _CDVDreadTrack)(u32 lsn, int mode);

// Copies loaded data to the target buffer.
// Returns -2 if the asynchronous read is still pending.
// Returns -1 if the asyncronous read failed.
// Returns 0 on success.
typedef s32(CALLBACK* _CDVDgetBuffer)(u8* buffer);

typedef s32(CALLBACK* _CDVDreadSubQ)(u32 lsn, cdvdSubQ* subq);
typedef s32(CALLBACK* _CDVDgetTN)(cdvdTN* Buffer);
typedef s32(CALLBACK* _CDVDgetTD)(u8 Track, cdvdTD* Buffer);
typedef s32(CALLBACK* _CDVDgetTOC)(void* toc);
typedef s32(CALLBACK* _CDVDgetDiskType)();
typedef s32(CALLBACK* _CDVDgetTrayStatus)();
typedef s32(CALLBACK* _CDVDctrlTrayOpen)();
typedef s32(CALLBACK* _CDVDctrlTrayClose)();
typedef s32(CALLBACK* _CDVDreadSector)(u8* buffer, u32 lsn, int mode);
typedef s32(CALLBACK* _CDVDgetDualInfo)(s32* dualType, u32* _layer1start);

typedef void(CALLBACK* _CDVDnewDiskCB)(void (*callback)());

enum class CDVD_SourceType : uint8_t
{
	Iso,    // use built in ISO api
	Disc,   // use built in Disc api
	NoDisc, // use built in CDVDnull
};

struct CDVD_API
{
	void(CALLBACK* close)();

	// Don't need init or shutdown.  iso/nodisc have no init/shutdown.

	_CDVDopen open;
	_CDVDreadTrack readTrack;
	_CDVDgetBuffer getBuffer;
	_CDVDreadSubQ readSubQ;
	_CDVDgetTN getTN;
	_CDVDgetTD getTD;
	_CDVDgetTOC getTOC;
	_CDVDgetDiskType getDiskType;
	_CDVDgetTrayStatus getTrayStatus;
	_CDVDctrlTrayOpen ctrlTrayOpen;
	_CDVDctrlTrayClose ctrlTrayClose;
	_CDVDnewDiskCB newDiskCB;

	// special functions, not in external interface yet
	_CDVDreadSector readSector;
	_CDVDgetDualInfo getDualInfo;
};

// ----------------------------------------------------------------------------
//   Multiple interface system for CDVD. 
// ----------------------------------------------------------------------------

extern CDVD_API* CDVD; // currently active CDVD access mode api (either Iso, NoDisc, or Disc)

extern CDVD_API CDVDapi_Iso;
extern CDVD_API CDVDapi_Disc;
extern CDVD_API CDVDapi_NoDisc;

extern void CDVDsys_ChangeSource(CDVD_SourceType type);
extern void CDVDsys_SetFile(CDVD_SourceType srctype, std::string newfile);
extern const std::string& CDVDsys_GetFile(CDVD_SourceType srctype);
extern CDVD_SourceType CDVDsys_GetSourceType();

extern bool DoCDVDopen();
extern void DoCDVDclose();
extern s32 DoCDVDreadSector(u8* buffer, u32 lsn, int mode);
extern s32 DoCDVDreadTrack(u32 lsn, int mode);
extern s32 DoCDVDgetBuffer(u8* buffer);
extern s32 DoCDVDdetectDiskType();
extern void DoCDVDresetDiskTypeCache();
