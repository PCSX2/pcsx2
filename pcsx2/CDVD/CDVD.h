// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "CDVDcommon.h"

#include <memory>
#include <string>
#include <string_view>

class Error;
class ElfObject;
class IsoReader;

#define btoi(b) ((b) / 16 * 10 + (b) % 16) /* BCD to u_char */
#define itob(i) ((i) / 10 * 16 + (i) % 10) /* u_char to BCD */

static __fi s32 msf_to_lsn(const u8* Time) noexcept
{
	u32 lsn;

	lsn = Time[2];
	lsn += (Time[1] - 2) * 75;
	lsn += Time[0] * 75 * 60;
	return lsn;
}

static __fi s32 msf_to_lba(const u8 m, const u8 s, const u8 f) noexcept
{
	u32 lsn;
	lsn = f;
	lsn += (s - 2) * 75;
	lsn += m * 75 * 60;
	return lsn;
}

static __fi void lsn_to_msf(u8* Time, s32 lsn) noexcept
{
	u8 m, s, f;

	lsn += 150;
	m = lsn / 4500;       // minuten
	lsn = lsn - m * 4500; // minuten rest
	s = lsn / 75;         // sekunden
	f = lsn - (s * 75);   // sekunden rest
	Time[0] = itob(m);
	Time[1] = itob(s);
	Time[2] = itob(f);
}

static __fi void lba_to_msf(s32 lba, u8* m, u8* s, u8* f) noexcept
{
	lba += 150;
	*m = lba / (60 * 75);
	*s = (lba / 75) % 60;
	*f = lba % 75;
}

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

enum class CDVDDiscType : u8
{
	Other,
	PS1Disc,
	PS2Disc
};

enum TrayStates
{
	CDVD_DISC_ENGAGED,
	CDVD_DISC_DETECTING,
	CDVD_DISC_SEEKING,
	CDVD_DISC_EJECT,
	CDVD_DISC_OPEN
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
	u8 DiscType;
	u8 sCommand;
	u8 sDataIn;
	u8 sDataOut;
	u8 HowTo;

	u8 NCMDParamBuff[16];
	u8 SCMDParamBuff[16];
	u8 SCMDResultBuff[16];

	u8 NCMDParamCnt;
	u8 NCMDParamPos;
	u8 SCMDParamCnt;
	u8 SCMDParamPos;
	u8 SCMDResultCnt;
	u8 SCMDResultPos;

	u8 CBlockIndex;
	u8 COffset;
	u8 CReadWrite;
	u8 CNumBlocks;

	// Calculates the number of Vsyncs and once it reaches a total number of Vsyncs worth a second with respect to
	// the videomode's vertical frequency, it updates the real time clock.
	double RTCcount;
	cdvdRTC RTC;

	u32 CurrentSector;
	int SectorCnt;
	int SeekCompleted;  // change to bool. --arcum42
	int Reading; // same here.
	int WaitingDMA;
	int ReadMode;
	int BlockSize; // Total bytes transfered at 1x speed
	int Speed;
	int RetryCntMax;
	int CurrentRetryCnt;
	int ReadErr;
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
	u32 RotSpeed;     // Rotational Speed
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

extern void cdvdGetDiscInfo(std::string* out_serial, std::string* out_elf_path, std::string* out_version, u32* out_crc,
	CDVDDiscType* out_disc_type);
extern u32 cdvdGetElfCRC(const std::string& path);
extern bool cdvdLoadElf(ElfObject* elfo, const std::string_view elfpath, bool isPSXElf, Error* error);
extern bool cdvdLoadDiscElf(ElfObject* elfo, IsoReader& isor, const std::string_view elfpath, bool isPSXElf, Error* error);

extern s32 cdvdCtrlTrayOpen();
extern s32 cdvdCtrlTrayClose();

