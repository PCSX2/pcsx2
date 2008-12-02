/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "Common.h"
#include "PsxCommon.h"
#include "GS.h"

#ifdef _WIN32
#pragma warning(disable:4244)
#endif

_GSinit            GSinit;
_GSopen            GSopen;
_GSclose           GSclose;
_GSshutdown        GSshutdown;
_GSvsync           GSvsync;
_GSgifTransfer1    GSgifTransfer1;
_GSgifTransfer2    GSgifTransfer2;
_GSgifTransfer3    GSgifTransfer3;
_GSgetLastTag      GSgetLastTag;
_GSgifSoftReset    GSgifSoftReset;
_GSreadFIFO        GSreadFIFO;
_GSreadFIFO2       GSreadFIFO2;

_GSkeyEvent        GSkeyEvent;
_GSchangeSaveState GSchangeSaveState;
_GSmakeSnapshot	   GSmakeSnapshot;
_GSmakeSnapshot2   GSmakeSnapshot2;
_GSirqCallback 	   GSirqCallback;
_GSprintf      	   GSprintf;
_GSsetBaseMem 	   GSsetBaseMem;
_GSsetGameCRC		GSsetGameCRC;
_GSsetFrameSkip	   GSsetFrameSkip;
_GSsetupRecording GSsetupRecording;
_GSreset		   GSreset;
_GSwriteCSR		   GSwriteCSR;
_GSgetDriverInfo   GSgetDriverInfo;
#ifdef _WIN32
_GSsetWindowInfo   GSsetWindowInfo;
#endif
_GSfreeze          GSfreeze;
_GSconfigure       GSconfigure;
_GStest            GStest;
_GSabout           GSabout;

// PAD1
_PADinit           PAD1init;
_PADopen           PAD1open;
_PADclose          PAD1close;
_PADshutdown       PAD1shutdown;
_PADkeyEvent       PAD1keyEvent;
_PADstartPoll      PAD1startPoll;
_PADpoll           PAD1poll;
_PADquery          PAD1query;
_PADupdate         PAD1update;

_PADgsDriverInfo   PAD1gsDriverInfo;
_PADconfigure      PAD1configure;
_PADtest           PAD1test;
_PADabout          PAD1about;

// PAD2
_PADinit           PAD2init;
_PADopen           PAD2open;
_PADclose          PAD2close;
_PADshutdown       PAD2shutdown;
_PADkeyEvent       PAD2keyEvent;
_PADstartPoll      PAD2startPoll;
_PADpoll           PAD2poll;
_PADquery          PAD2query;
_PADupdate         PAD2update;

_PADgsDriverInfo   PAD2gsDriverInfo;
_PADconfigure      PAD2configure;
_PADtest           PAD2test;
_PADabout          PAD2about;

// SIO[2]
_SIOinit           SIOinit[2][9];
_SIOopen           SIOopen[2][9];
_SIOclose          SIOclose[2][9];
_SIOshutdown       SIOshutdown[2][9];
_SIOstartPoll      SIOstartPoll[2][9];
_SIOpoll           SIOpoll[2][9];
_SIOquery          SIOquery[2][9];

_SIOconfigure      SIOconfigure[2][9];
_SIOtest           SIOtest[2][9];
_SIOabout          SIOabout[2][9];

// SPU2
_SPU2init          SPU2init;
_SPU2open          SPU2open;
_SPU2close         SPU2close;
_SPU2shutdown      SPU2shutdown;
_SPU2write         SPU2write;
_SPU2read          SPU2read;
_SPU2readDMA4Mem   SPU2readDMA4Mem;
_SPU2writeDMA4Mem  SPU2writeDMA4Mem;
_SPU2interruptDMA4 SPU2interruptDMA4;
_SPU2readDMA7Mem   SPU2readDMA7Mem;
_SPU2writeDMA7Mem  SPU2writeDMA7Mem;
_SPU2setDMABaseAddr SPU2setDMABaseAddr;
_SPU2interruptDMA7 SPU2interruptDMA7;
_SPU2ReadMemAddr   SPU2ReadMemAddr;
_SPU2setupRecording SPU2setupRecording;
_SPU2WriteMemAddr   SPU2WriteMemAddr;
_SPU2irqCallback   SPU2irqCallback;

_SPU2setClockPtr   SPU2setClockPtr;
_SPU2setTimeStretcher SPU2setTimeStretcher;

_SPU2async         SPU2async;
_SPU2freeze        SPU2freeze;
_SPU2configure     SPU2configure;
_SPU2test          SPU2test;
_SPU2about         SPU2about;

// CDVD
_CDVDinit          CDVDinit;
_CDVDopen          CDVDopen;
_CDVDclose         CDVDclose;
_CDVDshutdown      CDVDshutdown;
_CDVDreadTrack     CDVDreadTrack;
_CDVDgetBuffer     CDVDgetBuffer;
_CDVDreadSubQ      CDVDreadSubQ;
_CDVDgetTN         CDVDgetTN;
_CDVDgetTD         CDVDgetTD;
_CDVDgetTOC        CDVDgetTOC;
_CDVDgetDiskType   CDVDgetDiskType;
_CDVDgetTrayStatus CDVDgetTrayStatus;
_CDVDctrlTrayOpen  CDVDctrlTrayOpen;
_CDVDctrlTrayClose CDVDctrlTrayClose;

_CDVDconfigure     CDVDconfigure;
_CDVDtest          CDVDtest;
_CDVDabout         CDVDabout;
_CDVDnewDiskCB     CDVDnewDiskCB;

// DEV9
_DEV9init          DEV9init;
_DEV9open          DEV9open;
_DEV9close         DEV9close;
_DEV9shutdown      DEV9shutdown;
_DEV9read8         DEV9read8;
_DEV9read16        DEV9read16;
_DEV9read32        DEV9read32;
_DEV9write8        DEV9write8;
_DEV9write16       DEV9write16;
_DEV9write32       DEV9write32;
_DEV9readDMA8Mem   DEV9readDMA8Mem;
_DEV9writeDMA8Mem  DEV9writeDMA8Mem;
_DEV9irqCallback   DEV9irqCallback;
_DEV9irqHandler    DEV9irqHandler;

_DEV9configure     DEV9configure;
_DEV9freeze        DEV9freeze;
_DEV9test          DEV9test;
_DEV9about         DEV9about;

// USB
_USBinit           USBinit;
_USBopen           USBopen;
_USBclose          USBclose;
_USBshutdown       USBshutdown;
_USBread8          USBread8;
_USBread16         USBread16;
_USBread32         USBread32;
_USBwrite8         USBwrite8;
_USBwrite16        USBwrite16;
_USBwrite32        USBwrite32;
_USBasync          USBasync;

_USBirqCallback    USBirqCallback;
_USBirqHandler     USBirqHandler;
_USBsetRAM         USBsetRAM;

_USBconfigure      USBconfigure;
_USBfreeze         USBfreeze;
_USBtest           USBtest;
_USBabout          USBabout;

// FW
_FWinit            FWinit;
_FWopen            FWopen;
_FWclose           FWclose;
_FWshutdown        FWshutdown;
_FWread32          FWread32;
_FWwrite32         FWwrite32;
_FWirqCallback     FWirqCallback;

_FWconfigure       FWconfigure;
_FWfreeze          FWfreeze;
_FWtest            FWtest;
_FWabout           FWabout;

#define CheckErr(func) \
    err = SysLibError(); \
    if (err != NULL) { SysMessage (_("%s: Error loading %s: %s"), filename, func, err); return -1; }

#define LoadSym(dest, src, name, checkerr) \
    dest = (src) SysLoadSym(drv, name); if (checkerr == 1) CheckErr(name); \
    if (checkerr == 2) { err = SysLibError(); if (err != NULL) errval = 1; }

#define TestPS2Esyms(type) { \
	_PS2EgetLibVersion2 PS2EgetLibVersion2; \
	SysLoadSym(drv, "PS2EgetLibType"); CheckErr("PS2EgetLibType"); \
	PS2EgetLibVersion2 = (_PS2EgetLibVersion2) SysLoadSym(drv, "PS2EgetLibVersion2"); CheckErr("PS2EgetLibVersion2"); \
	SysLoadSym(drv, "PS2EgetLibName"); CheckErr("PS2EgetLibName"); \
	if( ((PS2EgetLibVersion2(PS2E_LT_##type) >> 16)&0xff) != PS2E_##type##_VERSION) { \
		SysMessage (_("Can't load '%s', wrong PS2E version (%x != %x)"), filename, (PS2EgetLibVersion2(PS2E_LT_##type) >> 16)&0xff, PS2E_##type##_VERSION); return -1; \
	} \
}

static char *err;
static int errval;

void *GSplugin;

void CALLBACK GS_printf(int timeout, char *fmt, ...) {
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	SysPrintf(msg);
}

s32  CALLBACK GS_freeze(int mode, freezeData *data) { data->size = 0; return 0; }
void CALLBACK GS_keyEvent(keyEvent *ev) {}
void CALLBACK GS_makeSnapshot(char *path) {}
void CALLBACK GS_irqCallback(void (*callback)()) {}
void CALLBACK GS_configure() {}
void CALLBACK GS_about() {}
long CALLBACK GS_test() { return 0; }

#define LoadGSsym1(dest, name) \
	LoadSym(GS##dest, _GS##dest, name, 1);

#define LoadGSsym0(dest, name) \
	LoadSym(GS##dest, _GS##dest, name, 0); \
	if (GS##dest == NULL) GS##dest = (_GS##dest) GS_##dest;

#define LoadGSsymN(dest, name) \
	LoadSym(GS##dest, _GS##dest, name, 0);

int LoadGSplugin(char *filename) {
	void *drv;

	GSplugin = SysLoadLibrary(filename);
	if (GSplugin == NULL) { SysMessage (_("Could Not Load GS Plugin '%s': %s"), filename, SysLibError()); return -1; }
	drv = GSplugin;
	TestPS2Esyms(GS);
	LoadGSsym1(init,         "GSinit");
	LoadGSsym1(shutdown,     "GSshutdown");
	LoadGSsym1(open,         "GSopen");
	LoadGSsym1(close,        "GSclose");
	LoadGSsym1(gifTransfer1, "GSgifTransfer1");
	LoadGSsym1(gifTransfer2, "GSgifTransfer2");
	LoadGSsym1(gifTransfer3, "GSgifTransfer3");
	LoadGSsym1(readFIFO,     "GSreadFIFO");
    LoadGSsymN(getLastTag,   "GSgetLastTag");
	LoadGSsymN(readFIFO2,    "GSreadFIFO2"); // optional
	LoadGSsym1(vsync,        "GSvsync");

	LoadGSsym0(keyEvent,     "GSkeyEvent");
	LoadGSsymN(changeSaveState, "GSchangeSaveState");
	LoadGSsymN(gifSoftReset, "GSgifSoftReset");
	LoadGSsym0(makeSnapshot, "GSmakeSnapshot");
	LoadGSsym0(irqCallback,  "GSirqCallback");
	LoadGSsym0(printf,       "GSprintf");
	LoadGSsym1(setBaseMem,	"GSsetBaseMem");
	LoadGSsymN(setGameCRC,	"GSsetGameCRC");
	LoadGSsym1(reset,       "GSreset");
	LoadGSsym1(writeCSR,       "GSwriteCSR");
	LoadGSsymN(makeSnapshot2,"GSmakeSnapshot2");
	LoadGSsymN(getDriverInfo,"GSgetDriverInfo");

	LoadGSsymN(setFrameSkip, "GSsetFrameSkip");
    LoadGSsymN(setupRecording, "GSsetupRecording");

#ifdef _WIN32
	LoadGSsymN(setWindowInfo,"GSsetWindowInfo");
#endif
	LoadGSsym0(freeze,       "GSfreeze");
	LoadGSsym0(configure,    "GSconfigure");
	LoadGSsym0(about,        "GSabout");
	LoadGSsym0(test,         "GStest");

	return 0;
}

void *PAD1plugin;

void CALLBACK PAD1_configure() {}
void CALLBACK PAD1_about() {}
long CALLBACK PAD1_test() { return 0; }

#define LoadPAD1sym1(dest, name) \
	LoadSym(PAD1##dest, _PAD##dest, name, 1);

#define LoadPAD1sym0(dest, name) \
	LoadSym(PAD1##dest, _PAD##dest, name, 0); \
	if (PAD1##dest == NULL) PAD1##dest = (_PAD##dest) PAD1_##dest;

#define LoadPAD1symN(dest, name) \
	LoadSym(PAD1##dest, _PAD##dest, name, 0);

int LoadPAD1plugin(char *filename) {
	void *drv;

	PAD1plugin = SysLoadLibrary(filename);
	if (PAD1plugin == NULL) { SysMessage (_("Could Not Load PAD1 Plugin '%s': %s"), filename, SysLibError()); return -1; }
	drv = PAD1plugin;
	TestPS2Esyms(PAD);
	LoadPAD1sym1(init,         "PADinit");
	LoadPAD1sym1(shutdown,     "PADshutdown");
	LoadPAD1sym1(open,         "PADopen");
	LoadPAD1sym1(close,        "PADclose");
	LoadPAD1sym1(keyEvent,     "PADkeyEvent");
	LoadPAD1sym1(startPoll,    "PADstartPoll");
	LoadPAD1sym1(poll,         "PADpoll");
	LoadPAD1sym1(query,        "PADquery");
    LoadPAD1symN(update,       "PADupdate");

	LoadPAD1symN(gsDriverInfo, "PADgsDriverInfo");
	LoadPAD1sym0(configure,    "PADconfigure");
	LoadPAD1sym0(about,        "PADabout");
	LoadPAD1sym0(test,         "PADtest");

	return 0;
}

void *PAD2plugin;

void CALLBACK PAD2_configure() {}
void CALLBACK PAD2_about() {}
long CALLBACK PAD2_test() { return 0; }

#define LoadPAD2sym1(dest, name) \
	LoadSym(PAD2##dest, _PAD##dest, name, 1);

#define LoadPAD2sym0(dest, name) \
	LoadSym(PAD2##dest, _PAD##dest, name, 0); \
	if (PAD2##dest == NULL) PAD2##dest = (_PAD##dest) PAD2_##dest;

#define LoadPAD2symN(dest, name) \
	LoadSym(PAD2##dest, _PAD##dest, name, 0);

int LoadPAD2plugin(char *filename) {
	void *drv;

	PAD2plugin = SysLoadLibrary(filename);
	if (PAD2plugin == NULL) { SysMessage (_("Could Not Load PAD2 Plugin '%s': %s"), filename, SysLibError()); return -1; }
	drv = PAD2plugin;
	TestPS2Esyms(PAD);
	LoadPAD2sym1(init,         "PADinit");
	LoadPAD2sym1(shutdown,     "PADshutdown");
	LoadPAD2sym1(open,         "PADopen");
	LoadPAD2sym1(close,        "PADclose");
	LoadPAD2sym1(keyEvent,     "PADkeyEvent");
	LoadPAD2sym1(startPoll,    "PADstartPoll");
	LoadPAD2sym1(poll,         "PADpoll");
	LoadPAD2sym1(query,        "PADquery");
    LoadPAD2symN(update,       "PADupdate");

	LoadPAD2symN(gsDriverInfo, "PADgsDriverInfo");
	LoadPAD2sym0(configure,    "PADconfigure");
	LoadPAD2sym0(about,        "PADabout");
	LoadPAD2sym0(test,         "PADtest");

	return 0;
}

void *SPU2plugin;

s32  CALLBACK SPU2_freeze(int mode, freezeData *data) { data->size = 0; return 0; }
void CALLBACK SPU2_configure() {}
void CALLBACK SPU2_about() {}
s32  CALLBACK SPU2_test() { return 0; }

#define LoadSPU2sym1(dest, name) \
	LoadSym(SPU2##dest, _SPU2##dest, name, 1);

#define LoadSPU2sym0(dest, name) \
	LoadSym(SPU2##dest, _SPU2##dest, name, 0); \
	if (SPU2##dest == NULL) SPU2##dest = (_SPU2##dest) SPU2_##dest;

#define LoadSPU2symN(dest, name) \
	LoadSym(SPU2##dest, _SPU2##dest, name, 0);

int LoadSPU2plugin(char *filename) {
	void *drv;

	SPU2plugin = SysLoadLibrary(filename);
	if (SPU2plugin == NULL) { SysMessage (_("Could Not Load SPU2 Plugin '%s': %s"), filename, SysLibError()); return -1; }
	drv = SPU2plugin;
	TestPS2Esyms(SPU2);
	LoadSPU2sym1(init,         "SPU2init");
	LoadSPU2sym1(shutdown,     "SPU2shutdown");
	LoadSPU2sym1(open,         "SPU2open");
	LoadSPU2sym1(close,        "SPU2close");
	LoadSPU2sym1(write,        "SPU2write");
	LoadSPU2sym1(read,         "SPU2read");
    LoadSPU2sym1(readDMA4Mem,  "SPU2readDMA4Mem");     
    LoadSPU2sym1(writeDMA4Mem, "SPU2writeDMA4Mem");   
	LoadSPU2sym1(interruptDMA4,"SPU2interruptDMA4");
    LoadSPU2sym1(readDMA7Mem,  "SPU2readDMA7Mem");     
    LoadSPU2sym1(writeDMA7Mem, "SPU2writeDMA7Mem");  
	LoadSPU2sym1(interruptDMA7,"SPU2interruptDMA7");
    LoadSPU2symN(setDMABaseAddr, "SPU2setDMABaseAddr");
	LoadSPU2sym1(ReadMemAddr,  "SPU2ReadMemAddr");
	LoadSPU2sym1(WriteMemAddr,  "SPU2WriteMemAddr");
	LoadSPU2sym1(irqCallback,  "SPU2irqCallback");

    LoadSPU2symN(setClockPtr, "SPU2setClockPtr");

	LoadSPU2symN(setupRecording, "SPU2setupRecording");

	LoadSPU2sym0(freeze,       "SPU2freeze");
	LoadSPU2sym0(configure,    "SPU2configure");
	LoadSPU2sym0(about,        "SPU2about");
	LoadSPU2sym0(test,         "SPU2test");
	LoadSPU2symN(async,        "SPU2async");

	return 0;
}

void *CDVDplugin;

void CALLBACK CDVD_configure() {}
void CALLBACK CDVD_about() {}
long CALLBACK CDVD_test() { return 0; }

#define LoadCDVDsym1(dest, name) \
	LoadSym(CDVD##dest, _CDVD##dest, name, 1);

#define LoadCDVDsym0(dest, name) \
	LoadSym(CDVD##dest, _CDVD##dest, name, 0); \
	if (CDVD##dest == NULL) CDVD##dest = (_CDVD##dest) CDVD_##dest;

#define LoadCDVDsymN(dest, name) \
	LoadSym(CDVD##dest, _CDVD##dest, name, 0); \

int LoadCDVDplugin(char *filename) {
	void *drv;

	CDVDplugin = SysLoadLibrary(filename);
	if (CDVDplugin == NULL) { SysMessage (_("Could Not Load CDVD Plugin '%s': %s"), filename, SysLibError()); return -1; }
	drv = CDVDplugin;
	TestPS2Esyms(CDVD);
	LoadCDVDsym1(init,          "CDVDinit");
	LoadCDVDsym1(shutdown,      "CDVDshutdown");
	LoadCDVDsym1(open,          "CDVDopen");
	LoadCDVDsym1(close,         "CDVDclose");
	LoadCDVDsym1(readTrack,     "CDVDreadTrack");
	LoadCDVDsym1(getBuffer,     "CDVDgetBuffer");
	LoadCDVDsym1(readSubQ,      "CDVDreadSubQ");
	LoadCDVDsym1(getTN,         "CDVDgetTN");
	LoadCDVDsym1(getTD,         "CDVDgetTD");
	LoadCDVDsym1(getTOC,        "CDVDgetTOC");
	LoadCDVDsym1(getDiskType,   "CDVDgetDiskType");
	LoadCDVDsym1(getTrayStatus, "CDVDgetTrayStatus");
	LoadCDVDsym1(ctrlTrayOpen,  "CDVDctrlTrayOpen");
	LoadCDVDsym1(ctrlTrayClose, "CDVDctrlTrayClose");

	LoadCDVDsym0(configure,     "CDVDconfigure");
	LoadCDVDsym0(about,         "CDVDabout");
	LoadCDVDsym0(test,          "CDVDtest");
	LoadCDVDsymN(newDiskCB,     "CDVDnewDiskCB");

	return 0;
}

void *DEV9plugin;

s32  CALLBACK DEV9_freeze(int mode, freezeData *data) { data->size = 0; return 0; }
void CALLBACK DEV9_configure() {}
void CALLBACK DEV9_about() {}
long CALLBACK DEV9_test() { return 0; }

#define LoadDEV9sym1(dest, name) \
	LoadSym(DEV9##dest, _DEV9##dest, name, 1);

#define LoadDEV9sym0(dest, name) \
	LoadSym(DEV9##dest, _DEV9##dest, name, 0); \
	if (DEV9##dest == NULL) DEV9##dest = (_DEV9##dest) DEV9_##dest;

int LoadDEV9plugin(char *filename) {
	void *drv;

	DEV9plugin = SysLoadLibrary(filename);
	if (DEV9plugin == NULL) { SysMessage (_("Could Not Load DEV9 Plugin '%s': %s"), filename, SysLibError()); return -1; }
	drv = DEV9plugin;
	TestPS2Esyms(DEV9);
	LoadDEV9sym1(init,          "DEV9init");
	LoadDEV9sym1(shutdown,      "DEV9shutdown");
	LoadDEV9sym1(open,          "DEV9open");
	LoadDEV9sym1(close,         "DEV9close");
	LoadDEV9sym1(read8,         "DEV9read8");
	LoadDEV9sym1(read16,        "DEV9read16");
	LoadDEV9sym1(read32,        "DEV9read32");
	LoadDEV9sym1(write8,        "DEV9write8");
	LoadDEV9sym1(write16,       "DEV9write16");
	LoadDEV9sym1(write32,       "DEV9write32");
	LoadDEV9sym1(readDMA8Mem,   "DEV9readDMA8Mem");
	LoadDEV9sym1(writeDMA8Mem,  "DEV9writeDMA8Mem");
	LoadDEV9sym1(irqCallback,   "DEV9irqCallback");
	LoadDEV9sym1(irqHandler,    "DEV9irqHandler");

	LoadDEV9sym0(freeze,        "DEV9freeze");
	LoadDEV9sym0(configure,     "DEV9configure");
	LoadDEV9sym0(about,         "DEV9about");
	LoadDEV9sym0(test,          "DEV9test");

	return 0;
}

void *USBplugin;

s32  CALLBACK USB_freeze(int mode, freezeData *data) { data->size = 0; return 0; }
void CALLBACK USB_configure() {}
void CALLBACK USB_about() {}
long CALLBACK USB_test() { return 0; }

#define LoadUSBsym1(dest, name) \
	LoadSym(USB##dest, _USB##dest, name, 1);

#define LoadUSBsym0(dest, name) \
	LoadSym(USB##dest, _USB##dest, name, 0); \
	if (USB##dest == NULL) USB##dest = (_USB##dest) USB_##dest;

#define LoadUSBsymX(dest, name) \
	LoadSym(USB##dest, _USB##dest, name, 0); \

int LoadUSBplugin(char *filename) {
	void *drv;

	USBplugin = SysLoadLibrary(filename);
	if (USBplugin == NULL) { SysMessage (_("Could Not Load USB Plugin '%s': %s"), filename, SysLibError()); return -1; }
	drv = USBplugin;
	TestPS2Esyms(USB);
	LoadUSBsym1(init,          "USBinit");
	LoadUSBsym1(shutdown,      "USBshutdown");
	LoadUSBsym1(open,          "USBopen");
	LoadUSBsym1(close,         "USBclose");
	LoadUSBsym1(read8,         "USBread8");
	LoadUSBsym1(read16,        "USBread16");
	LoadUSBsym1(read32,        "USBread32");
	LoadUSBsym1(write8,        "USBwrite8");
	LoadUSBsym1(write16,       "USBwrite16");
	LoadUSBsym1(write32,       "USBwrite32");
	LoadUSBsym1(irqCallback,   "USBirqCallback");
	LoadUSBsym1(irqHandler,    "USBirqHandler");
	LoadUSBsym1(setRAM,        "USBsetRAM");
	
	LoadUSBsymX(async,         "USBasync");

	LoadUSBsym0(freeze,        "USBfreeze");
	LoadUSBsym0(configure,     "USBconfigure");
	LoadUSBsym0(about,         "USBabout");
	LoadUSBsym0(test,          "USBtest");

	return 0;
}
void *FWplugin;

s32  CALLBACK FW_freeze(int mode, freezeData *data) { data->size = 0; return 0; }
void CALLBACK FW_configure() {}
void CALLBACK FW_about() {}
long CALLBACK FW_test() { return 0; }

#define LoadFWsym1(dest, name) \
	LoadSym(FW##dest, _FW##dest, name, 1);

#define LoadFWsym0(dest, name) \
	LoadSym(FW##dest, _FW##dest, name, 0); \
	if (FW##dest == NULL) FW##dest = (_FW##dest) FW_##dest;

int LoadFWplugin(char *filename) {
	void *drv;

	FWplugin = SysLoadLibrary(filename);
	if (FWplugin == NULL) { SysMessage (_("Could Not Load FW Plugin '%s': %s"), filename, SysLibError()); return -1; }
	drv = FWplugin;
	TestPS2Esyms(FW);
	LoadFWsym1(init,          "FWinit");
	LoadFWsym1(shutdown,      "FWshutdown");
	LoadFWsym1(open,          "FWopen");
	LoadFWsym1(close,         "FWclose");
	LoadFWsym1(read32,        "FWread32");
	LoadFWsym1(write32,       "FWwrite32");
	LoadFWsym1(irqCallback,   "FWirqCallback");

	LoadFWsym0(freeze,        "FWfreeze");
	LoadFWsym0(configure,     "FWconfigure");
	LoadFWsym0(about,         "FWabout");
	LoadFWsym0(test,          "FWtest");

	return 0;
}
static int loadp=0;

int InitPlugins() {
	int ret;

	if( GSsetBaseMem ) {

		if( CHECK_MULTIGS ) {
			extern u8 g_MTGSMem[];
			GSsetBaseMem(g_MTGSMem);
		}
		else {
			GSsetBaseMem(PS2MEM_GS);
		}
	}

	ret = GSinit();
	if (ret != 0) { SysMessage (_("GSinit error: %d"), ret); return -1; }
	ret = PAD1init(1);
	if (ret != 0) { SysMessage (_("PAD1init error: %d"), ret); return -1; }
	ret = PAD2init(2);
	if (ret != 0) { SysMessage (_("PAD2init error: %d"), ret); return -1; }
	ret = SPU2init();
	if (ret != 0) { SysMessage (_("SPU2init error: %d"), ret); return -1; }
	ret = CDVDinit();
	if (ret != 0) { SysMessage (_("CDVDinit error: %d"), ret); return -1; }
	ret = DEV9init();
	if (ret != 0) { SysMessage (_("DEV9init error: %d"), ret); return -1; }
	ret = USBinit();
	if (ret != 0) { SysMessage (_("USBinit error: %d"), ret); return -1; }
	ret = FWinit();
	if (ret != 0) { SysMessage (_("FWinit error: %d"), ret); return -1; }
	return 0;
}

void ShutdownPlugins() {
	GSshutdown();
	PAD1shutdown();
	PAD2shutdown();
	SPU2shutdown();
	CDVDshutdown();
	DEV9shutdown();
	USBshutdown();
    FWshutdown();
}

int LoadPlugins() {
	char Plugin[g_MaxPath];

	
	CombinePaths( Plugin, Config.PluginsDir, Config.GS );
	if (LoadGSplugin(Plugin) == -1) return -1;

	CombinePaths( Plugin, Config.PluginsDir, Config.PAD1 );
	if (LoadPAD1plugin(Plugin) == -1) return -1;

	CombinePaths( Plugin, Config.PluginsDir, Config.PAD2);
	if (LoadPAD2plugin(Plugin) == -1) return -1;

	CombinePaths( Plugin, Config.PluginsDir, Config.SPU2);
	if (LoadSPU2plugin(Plugin) == -1) return -1;

	CombinePaths( Plugin, Config.PluginsDir, Config.CDVD);
	if (LoadCDVDplugin(Plugin) == -1) return -1;

	CombinePaths( Plugin, Config.PluginsDir, Config.DEV9);
	if (LoadDEV9plugin(Plugin) == -1) return -1;

	CombinePaths( Plugin, Config.PluginsDir, Config.USB);
	if (LoadUSBplugin(Plugin) == -1) return -1;

    CombinePaths( Plugin, Config.PluginsDir, Config.FW);
	if (LoadFWplugin(Plugin) == -1) return -1;

	if( g_Error_PathTooLong ) return -1;
	if (InitPlugins() == -1) return -1;

	loadp=1;

	return 0;
}

uptr pDsp;
extern void spu2DMA4Irq();
extern void spu2DMA7Irq();
extern void spu2Irq();

typedef struct _PluginOpenStatusFlags
{
	u8	GS : 1
	,	CDVD : 1
	,	DEV9 : 1
	,	USB : 1
	,	SPU2 : 1
	,	PAD1 : 1
	,	PAD2 : 1
	,	FW : 1;

} PluginOpenStatusFlags;

static PluginOpenStatusFlags OpenStatus;

int OpenPlugins(const char* pTitleFilename) {
	GSdriverInfo info;
	int ret;

	if (loadp == 0) return -1;

#ifndef _WIN32
    // change dir so that CDVD can find its config file
    char file[255], pNewTitle[255];
    getcwd(file, ARRAYSIZE(file));
    chdir(Config.PluginsDir);

    if( pTitleFilename != NULL && pTitleFilename[0] != '/' ) {
        // because we are changing the dir, we have to set a new title if it is a relative dir
        sprintf(pNewTitle, "%s/%s", file, pTitleFilename);
        pTitleFilename = pNewTitle;
    }
#endif

	//first we need the data
	if (CDVDnewDiskCB) CDVDnewDiskCB(cdvdNewDiskCB);

    ret = CDVDopen(pTitleFilename);

	if (ret != 0) { SysMessage (_("Error Opening CDVD Plugin")); goto OpenError; }
	OpenStatus.CDVD = 1;
	cdvdNewDiskCB();

	//video
	// Only bind the gsIrq if we're not running the MTGS.
	// The MTGS simulates its own gsIrq in order to maintain proper sync.
	GSirqCallback( CHECK_MULTIGS ? NULL : gsIrq );

	// GS isn't closed during emu pauses, so only open it once per instance.
	if( !OpenStatus.GS ) {
		ret = gsOpen();
		if (ret != 0) { SysMessage (_("Error Opening GS Plugin")); goto OpenError; }
		OpenStatus.GS = 1;
	}

	//then the user input
	if (GSgetDriverInfo) {
		GSgetDriverInfo(&info);
		if (PAD1gsDriverInfo) PAD1gsDriverInfo(&info);
		if (PAD2gsDriverInfo) PAD2gsDriverInfo(&info);
	}
	ret = PAD1open((void *)&pDsp);
	if (ret != 0) { SysMessage (_("Error Opening PAD1 Plugin")); goto OpenError; }
	OpenStatus.PAD1 = 1;
	ret = PAD2open((void *)&pDsp);
	if (ret != 0) { SysMessage (_("Error Opening PAD2 Plugin")); goto OpenError; }
	OpenStatus.PAD2 = 1;

	//the sound

	SPU2irqCallback(spu2Irq,spu2DMA4Irq,spu2DMA7Irq);
    if( SPU2setDMABaseAddr != NULL )
        SPU2setDMABaseAddr((uptr)PSXM(0));

	if(SPU2setClockPtr != NULL)
		SPU2setClockPtr(&psxRegs.cycle);

	ret = SPU2open((void*)&pDsp);
	if (ret != 0) { SysMessage (_("Error Opening SPU2 Plugin")); goto OpenError; }
	OpenStatus.SPU2 = 1;

	//and last the dev9
	DEV9irqCallback(dev9Irq);
	dev9Handler = DEV9irqHandler();
	ret = DEV9open(&(psxRegs.pc)); //((void *)&pDsp);
	if (ret != 0) { SysMessage (_("Error Opening DEV9 Plugin")); goto OpenError; }
	OpenStatus.DEV9 = 1;

	USBirqCallback(usbIrq);
	usbHandler = USBirqHandler();
	USBsetRAM(psxM);
	ret = USBopen((void *)&pDsp);
	if (ret != 0) { SysMessage (_("Error Opening USB Plugin")); goto OpenError; }
	OpenStatus.USB = 1;

	FWirqCallback(fwIrq);
	ret = FWopen((void *)&pDsp);
	if (ret != 0) { SysMessage (_("Error Opening FW Plugin")); goto OpenError; }
	OpenStatus.FW = 1;

#ifdef __LINUX__
    chdir(file);
#endif
	return 0;

OpenError:
	ClosePlugins();
#ifdef __LINUX__
    chdir(file);
#endif

    return -1;
}


#define CLOSE_PLUGIN( name ) \
	if( OpenStatus.name ) { \
		name##close(); \
		OpenStatus.name = 0; \
	}


void ClosePlugins()
{
	// GS plugin is special and is not closed during emulation pauses.

	if( OpenStatus.GS )
		gsWaitGS();

	CLOSE_PLUGIN( CDVD );
	CLOSE_PLUGIN( DEV9 );
	CLOSE_PLUGIN( USB );
	CLOSE_PLUGIN( FW );
	CLOSE_PLUGIN( SPU2 );
	CLOSE_PLUGIN( PAD1 );
	CLOSE_PLUGIN( PAD2 );
}

void ResetPlugins() {
	gsWaitGS();

	ShutdownPlugins();
	InitPlugins();
}

void ReleasePlugins() {
	if (loadp == 0) return;

	if (GSplugin   == NULL || PAD1plugin == NULL || PAD2plugin == NULL ||
		SPU2plugin == NULL || CDVDplugin == NULL || DEV9plugin == NULL ||
		USBplugin  == NULL || FWplugin == NULL) return;

	ShutdownPlugins();

	SysCloseLibrary(GSplugin);   GSplugin = NULL;
	SysCloseLibrary(PAD1plugin); PAD1plugin = NULL;
	SysCloseLibrary(PAD2plugin); PAD2plugin = NULL;
	SysCloseLibrary(SPU2plugin); SPU2plugin = NULL;
	SysCloseLibrary(CDVDplugin); CDVDplugin = NULL;
	SysCloseLibrary(DEV9plugin); DEV9plugin = NULL;
	SysCloseLibrary(USBplugin);  USBplugin = NULL;
	SysCloseLibrary(FWplugin);   FWplugin = NULL;
	loadp=0;
}
