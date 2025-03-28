// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Common.h"

/*
Interrupts - values are flag bits.

0x00	 No interrupt
0x01	 Data Ready
0x02	 Command Complete
0x03	 Acknowledge (reserved)
0x04	 End of Data Detected
0x05	 Error Detected
0x06	 Drive Not Ready

In limited experimentation I found that PS2 apps respond actively to use of the
'Data Ready' flag -- in that they'll almost immediately initiate a DMA transfer
after receiving an Irq with that as the cause.  But the question is, of course,
*when* to use it.  Adding it into some locations of CDVD reading only slowed
games down and broke things.

Using Drive Not Ready also invokes basic error handling from the Iop Bios, but
without proper emulation of the cdvd status flag it also tends to break things.

*/

/* Old IRQ structure

enum CdvdIrqId
{
	Irq_None = 0,
	Irq_DataReady = 0,
	Irq_CommandComplete,
	Irq_Acknowledge,
	Irq_EndOfData,
	Irq_Error,
	Irq_NotReady

};
*/

enum CdvdIrqId
{
	Irq_None = 0,
	Irq_CommandComplete = 0,
	Irq_POffReady = 2,
	Irq_Eject,
	Irq_BSPower, //PS1 IRQ not used
};

/* Cdvd.Status bits and their meaning
0x0 = Stop
0x1 = Tray Open
0x2 = Spindle Motor Spinning
0x4 = Reading disc
0x8 = Ready but not reading
0x10 = Seeking
0x20 = Abnormal Termination
*/
enum cdvdStatus
{
	CDVD_STATUS_STOP = 0x00,
	CDVD_STATUS_TRAY_OPEN = 0x01, // confirmed to be tray open
	CDVD_STATUS_SPIN = 0x02,
	CDVD_STATUS_READ = 0x06,
	CDVD_STATUS_PAUSE = 0x0A,
	CDVD_STATUS_SEEK = 0x12,
	CDVD_STATUS_EMERGENCY = 0x20,
};

/* from PS2Tek https://psi-rockin.github.io/ps2tek/#cdvdioports
1F402005h N command status (R)
  0     Error (1=error occurred)
  1     Unknown/unused
  2     DEV9 device connected (1=HDD/network adapter connected)
  3     Unknown/unused
  4     Test mode
  5     Power off ready
  6     Drive status (1=ready)
  7     Busy executing NCMD

*/
enum cdvdready
{
	CDVD_DRIVE_ERROR = 0x01,
	CDVD_DRIVE_DEV9CON = 0x04,
	CDVD_DRIVE_MECHA_INIT = 0x8,
	CDVD_DRIVE_PWOFF = 0x20,
	CDVD_DRIVE_READY = 0x40,
	CDVD_DRIVE_BUSY = 0x80,
};

// Cdvd actions tell the emulator how and when to respond to certain requests.
// Actions are handled by the cdvdInterrupt()
enum cdvdActions
{
	cdvdAction_None = 0,
	cdvdAction_Seek,
	cdvdAction_Standby,
	cdvdAction_Stop,
	cdvdAction_Error,
	cdvdAction_Read // note: not used yet.
};

//////////////////////////////////////////////////////////////////////////////////////////
// Cdvd Block Read Cycle Timings
//
// The PS2 CDVD effectively has two seek modes -- the normal/slow one (est. avg seeks being
// around 120-160ms), and a faster seek which has an estimated seek time of about 35-40ms.
// Fast seeks happen when the destination sector is within a certain range of the starting
// point, such that abs(start-dest) is less than the value in the tbl_FastSeekDelta.
//
// CDVDs also have a secondary seeking method used when the destination is close enough
// that a contiguous sector read can reach the sector faster than initiating a full seek.
// Typically this value is very low.

enum CDVD_MODE_TYPE
{
	MODE_CDROM = 0,
	MODE_DVDROM,
};

static constexpr uint tbl_FastSeekDelta[3] =
{
		4371,  // CD-ROM
		14764, // Single-layer DVD-ROM
		13360  // dual-layer DVD-ROM [currently unused]
};

// if a seek is within this many blocks, read instead of seek.
// These values are arbitrary assumptions.  Not sure what the real PS2 uses.
static constexpr uint tbl_ContigiousSeekDelta[3] =
{
		8,  // CD-ROM
		16, // single-layer DVD-ROM
		16, // dual-layer DVD-ROM [currently unused]
};

static constexpr uint PSX_CD_READSPEED = 153600;   // Bytes per second, rough values from outer CD (CAV).
static constexpr uint PSX_DVD_READSPEED = 1382400; // Bytes per second, rough values from outer DVD (CAV).

static constexpr uint CD_SECTORS_PERSECOND = 75;
static constexpr uint DVD_SECTORS_PERSECOND = 675;

// Rotations per minute.
static constexpr uint CD_MIN_ROTATION_X1 = 214;
static constexpr uint CD_MAX_ROTATION_X1 = 497;

static constexpr uint DVD_MIN_ROTATION_X1 = 570;
static constexpr uint DVD_MAX_ROTATION_X1 = 1515;

// Legacy Note: FullSeek timing causes many games to load very slow, but it likely not the real problem.
// Games breaking with it set to PSXCLK*40 : "wrath unleashed" and "Shijou Saikyou no Deshi Kenichi".

static constexpr uint Cdvd_FullSeek_Cycles = (36864000UL * 100UL) / 1000UL; // average number of cycles per fullseek (100ms)
static constexpr uint Cdvd_FastSeek_Cycles = (36864000UL * 30UL) / 1000UL;  // average number of cycles per fastseek (37ms)
bool trayState = 0; // Used to check if the CD tray status has changed since the last time

static const char* mg_zones[8] = {"Japan", "USA", "Europe", "Oceania", "Asia", "Russia", "China", "Mexico"};

static const char* nCmdName[0x100] = {
	"CdSync",
	"CdNop",
	"CdStandby",
	"CdStop",
	"CdPause",
	"CdSeek",
	"CdRead",
	"CdReadCDDA",
	"CdReadDVDV",
	"CdGetToc",
	"",
	"NCMD_B",
	"CdReadKey",
	"",
	"sceCdReadXCDDA",
	"sceCdChgSpdlCtrl",
};

enum nCmds
{
	N_CD_NOP = 0x00,           // CdNop
	N_CD_RESET = 0x01,         // CdReset
	N_CD_STANDBY = 0x02,       // CdStandby
	N_CD_STOP = 0x03,          // CdStop
	N_CD_PAUSE = 0x04,         // CdPause
	N_CD_SEEK = 0x05,          // CdSeek
	N_CD_READ = 0x06,          // CdRead
	N_CD_READ_CDDA = 0x07,     // CdReadCDDA
	N_DVD_READ = 0x08,         // DvdRead
	N_CD_GET_TOC = 0x09,       // CdGetToc & cdvdman_call19
	N_CMD_B = 0x0B,            // CdReadKey
	N_CD_READ_KEY = 0x0C,      // CdReadKey
	N_CD_READ_XCDDA = 0x0E,    // CdReadXCDDA
	N_CD_CHG_SPDL_CTRL = 0x0F, // CdChgSpdlCtrl
};

static const char* sCmdName[0x100] = {
	"", "sceCdGetDiscType", "sceCdReadSubQ", "subcommands", //sceCdGetMecaconVersion, read/write console id, read renewal date
	"", "sceCdTrayState", "sceCdTrayCtrl", "",
	"sceCdReadClock", "sceCdWriteClock", "sceCdReadNVM", "sceCdWriteNVM",
	"sceCdSetHDMode", "", "", "sceCdPowerOff",
	"", "", "sceCdReadILinkID", "sceCdWriteILinkID", /*10*/
	"sceAudioDigitalOut", "sceForbidDVDP", "sceAutoAdjustCtrl", "sceCdReadModelNumber",
	"sceWriteModelNumber", "sceCdForbidCD", "sceCdBootCertify", "sceCdCancelPOffRdy",
	"sceCdBlueLEDCtl", "", "sceRm2Read", "sceRemote2_7",               //Rm2PortGetConnection?
	"sceRemote2_6", "sceCdWriteWakeUpTime", "sceCdReadWakeUpTime", "", /*20*/
	"sceCdRcBypassCtl", "", "", "",
	"", "sceCdNoticeGameStart", "", "",
	"sceCdXBSPowerCtl", "sceCdXLEDCtl", "sceCdBuzzerCtl", "",
	"", "sceCdSetMediumRemoval", "sceCdGetMediumRemoval", "sceCdXDVRPReset", /*30*/
	"", "", "__sceCdReadRegionParams", "__sceCdReadMAC",
	"__sceCdWriteMAC", "", "", "",
	"", "", "__sceCdWriteRegionParams", "",
	"sceCdOpenConfig", "sceCdReadConfig", "sceCdWriteConfig", "sceCdCloseConfig", /*40*/
	"", "", "", "",
	"", "", "", "",
	"", "", "", "",
	"", "", "", "", /*50*/
	"", "", "", "",
	"", "", "", "",
	"", "", "", "",
	"", "", "", "", /*60*/
	"", "", "", "",
	"", "", "", "",
	"", "", "", "",
	"", "", "", "", /*70*/
	"", "", "", "",
	"", "", "", "",
	"", "", "", "",
	"mechacon_auth_0x80", "mechacon_auth_0x81", "mechacon_auth_0x82", "mechacon_auth_0x83", /*80*/
	"mechacon_auth_0x84", "mechacon_auth_0x85", "mechacon_auth_0x86", "mechacon_auth_0x87",
	"mechacon_auth_0x88", "", "", "",
	"", "sceMgWriteData", "sceMgReadData", "mechacon_auth_0x8F",
	"sceMgWriteHeaderStart", "sceMgReadBITLength", "sceMgWriteDatainLength", "sceMgWriteDataoutLength", /*90*/
	"sceMgReadKbit", "sceMgReadKbit2", "sceMgReadKcon", "sceMgReadKcon2",
	"sceMgReadIcvPs2", "", "", "",
	"", "", "", "",
	/*A0, no sCmds above?*/
};

// NVM (eeprom) layout info
struct NVMLayout
{
	u32 biosVer;   // bios version that this eeprom layout is for
	s32 config0;   // offset of 1st config block
	s32 config1;   // offset of 2nd config block
	s32 config2;   // offset of 3rd config block
	s32 consoleId; // offset of console id (?)
	s32 ilinkId;   // offset of ilink id (ilink mac address)
	s32 modelNum;  // offset of ps2 model number (eg "SCPH-70002")
	s32 regparams; // offset of RegionParams for PStwo
	s32 mac;       // offset of the value written to 0xFFFE0188 and 0xFFFE018C on PStwo
};

#define NVM_FORMAT_MAX 2
static constexpr NVMLayout nvmlayouts[NVM_FORMAT_MAX] =
	{
		{0x000, 0x280, 0x300, 0x200, 0x1C8, 0x1C0, 0x1A0, 0x180, 0x198}, // eeproms from bios v0.00 and up
		{0x146, 0x270, 0x2B0, 0x200, 0x1F0, 0x1E0, 0x1B0, 0x180, 0x198}, // eeproms from bios v1.70 and up
};

static constexpr u8 PStwoRegionDefaults[13][12] =
	{
		{0x4a, 0x4a, 0x6a, 0x70, 0x6e, 0x4a, 0x4a, 0x00, 0x00, 0x00, 0x00, 0x00}, // JJjpnJJ - Japan
		{0x41, 0x41, 0x65, 0x6e, 0x67, 0x41, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00}, // AAengAU - USA
		{0x45, 0x45, 0x65, 0x6e, 0x67, 0x45, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00}, // EEengEE - Europe
		{0x45, 0x45, 0x65, 0x6e, 0x67, 0x45, 0x4f, 0x00, 0x00, 0x00, 0x00, 0x00}, // EEengEO - Oceania
		{0x48, 0x48, 0x65, 0x6e, 0x67, 0x4a, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00}, // HHengJA - Asia
		{0x45, 0x52, 0x65, 0x6e, 0x67, 0x45, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00}, // ERengER - Russia
		{0x43, 0x43, 0x73, 0x63, 0x68, 0x4A, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00}, // CCschJC - China
		{0x41, 0x41, 0x73, 0x70, 0x61, 0x41, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x00}, // AAspaAM - Mexico
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // T10K (does not exist)
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Test (does not exist)
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Free (does not exist)
		{0x48, 0x4b, 0x6b, 0x6f, 0x72, 0x4a, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00}, // HKkorJA - Korea
		{0x48, 0x48, 0x74, 0x63, 0x68, 0x4a, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00}, // HHtchJA - Taiwan
};

static constexpr u8 biosLangDefaults[11][16] =
	{
		{0x20, 0x20, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30}, // Japan (Japanese)
		{0x30, 0x21, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41}, // USA (English)
		{0x30, 0x21, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41}, // Europe (English)
		{0x30, 0x21, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41}, // Oceania (English)
		{0x30, 0x21, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41}, // Asia (English)
		{0x30, 0x21, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41}, // Russia (English)
		{0x30, 0x2B, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4B}, // China (Simplified Chinese)
		{0x30, 0x21, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41}, // Mexico (English)
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // T10K (Japanese, generally gets overridden)
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Test (Japanese, as above)
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Free (Japanese, no examples to use)
};
