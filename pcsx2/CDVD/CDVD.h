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

#include "CDVDcommon.h"

#include <string>
#include <string_view>

struct cdvdRTC
{
	u8 status;
	u8 second;
	u8 minute;
	u8 hour;
	u8 pad;
	u8 day;
	u8 month;
	u8 year;
};

enum TrayStates
{
	CDVD_DISC_ENGAGED,
	CDVD_DISC_DETECTING,
	CDVD_DISC_SEEKING,
	CDVD_DISC_EJECT
};

struct cdvdTrayTimer
{
	u32 cdvdActionSeconds;
	TrayStates trayState;
};

struct cdvdStruct
{
	u8 nCommand;
	u8 Ready;
	u8 Error;
	u8 IntrStat;
	u8 Status;
	u8 StatusSticky;
	u8 Type;
	u8 sCommand;
	u8 sDataIn;
	u8 sDataOut;
	u8 HowTo;

	u8 NCMDParam[16];
	u8 SCMDParam[16];
	u8 SCMDResult[16];

	u8 NCMDParamC;
	u8 NCMDParamP;
	u8 SCMDParamC;
	u8 SCMDParamP;
	u8 SCMDResultC;
	u8 SCMDResultP;

	u8 CBlockIndex;
	u8 COffset;
	u8 CReadWrite;
	u8 CNumBlocks;

	// Calculates the number of Vsyncs and once it reaches a total number of Vsyncs worth a second with respect to
	// the videomode's vertical frequency, it updates the real time clock.
	int RTCcount;
	cdvdRTC RTC;

	cdvdSubQ subq;

	u32 Sector;
	int nSectors;
	int Readed;  // change to bool. --arcum42
	int Reading; // same here.
	int WaitingDMA;
	int ReadMode;
	int BlockSize; // Total bytes transfered at 1x speed
	int Speed;
	int RetryCnt;
	int RetryCntP;
	int RErr;
	int SpindlCtrl;

	u8 Key[16];
	u8 KeyXor;
	u8 decSet;

	u8 mg_buffer[65536];
	int mg_size;
	int mg_maxsize;
	int mg_datatype; //0-data(encrypted); 1-header
	u8 mg_kbit[16];  //last BIT key 'seen'
	u8 mg_kcon[16];  //last content key 'seen'

	u8 TrayTimeout;
	u8 Action;        // the currently scheduled emulated action
	u32 SeekToSector; // Holds the destination sector during seek operations.
	u32 MaxSector;    // Current disc max sector.
	u32 ReadTime;     // Avg. time to read one block of data (in Iop cycles)
	bool Spinning;    // indicates if the Cdvd is spinning or needs a spinup delay
	cdvdTrayTimer Tray;
	u8 nextSectorsBuffered;
	bool AbortRequested;
};

extern cdvdStruct cdvd;

extern void cdvdReadLanguageParams(u8* config);

extern void cdvdReset();
extern void cdvdVsync();
extern void cdvdActionInterrupt();
extern void cdvdSectorReady();
extern void cdvdReadInterrupt();

// We really should not have a function with the exact same name as a callback except for case!
extern void cdvdNewDiskCB();
extern u8 cdvdRead(u8 key);
extern void cdvdWrite(u8 key, u8 rt);

extern void cdvdReloadElfInfo(std::string elfoverride = std::string());
extern s32 cdvdCtrlTrayOpen();
extern s32 cdvdCtrlTrayClose();

extern std::string DiscSerial;
