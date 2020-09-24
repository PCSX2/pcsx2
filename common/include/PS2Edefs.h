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

#ifndef __PS2EDEFS_H__
#define __PS2EDEFS_H__

/*
 *  PS2E Definitions v0.6.2 (beta)
 *
 *  Author: linuzappz@hotmail.com
 *          shadowpcsx2@yahoo.gr
 *          florinsasu@hotmail.com
 */

/*
 Notes:
 * Since this is still beta things may change.

 * OSflags:
	__linux__ (linux OS)
	_WIN32 (win32 OS)

 * common return values (for ie. GSinit):
	 0 - success
	-1 - error

 * reserved keys:
	F1 to F10 are reserved for the emulator

 * plugins should NOT change the current
   working directory.
   (on win32, add flag OFN_NOCHANGEDIR for
    GetOpenFileName)

*/

#include "Pcsx2Defs.h"

///////////////////////////////////////////////////////////////////////

// freeze modes:
#define FREEZE_LOAD 0
#define FREEZE_SAVE 1
#define FREEZE_SIZE 2

// event values:
#define KEYPRESS 1
#define KEYRELEASE 2

typedef struct
{
    int size;
    s8 *data;
} freezeData;

typedef struct _keyEvent
{
    u32 key;
    u32 evt;
} keyEvent;

///////////////////////////////////////////////////////////////////////

#if defined(GSdefs) || defined(PADdefs) || defined(SIOdefs) ||     \
    defined(DEV9defs) || defined(USBdefs)
#define COMMONdefs
#endif

// PS2EgetLibType returns (may be OR'd)
#define PS2E_LT_GS 0x01
#define PS2E_LT_PAD 0x02 // -=[ OBSOLETE ]=-
#define PS2E_LT_DEV9 0x10
#define PS2E_LT_USB 0x20
#define PS2E_LT_SIO 0x80

// PS2EgetLibVersion2 (high 16 bits)
#define PS2E_GS_VERSION 0x0006
#define PS2E_PAD_VERSION 0x0002 // -=[ OBSOLETE ]=-
#define PS2E_DEV9_VERSION 0x0003
#define PS2E_USB_VERSION 0x0003
#define PS2E_SIO_VERSION 0x0001
#ifdef COMMONdefs

#ifdef __cplusplus
extern "C" {
#endif

u32 CALLBACK PS2EgetLibType(void);
u32 CALLBACK PS2EgetLibVersion2(u32 type);
const char *CALLBACK PS2EgetLibName(void);

#ifdef __cplusplus
}
#endif

#endif

// key values:
/* key values must be OS dependant:
	win32: the VK_XXX will be used (WinUser)
	linux: the XK_XXX will be used (XFree86)
*/

// for 64bit compilers
typedef char __keyEvent_Size__[(sizeof(keyEvent) == 8) ? 1 : -1];

// plugin types
#define SIO_TYPE_PAD 0x00000001
#define SIO_TYPE_MTAP 0x00000004
#define SIO_TYPE_RM 0x00000040
#define SIO_TYPE_MC 0x00000100

typedef int(CALLBACK *SIOchangeSlotCB)(int slot);

typedef void (*DEV9callback)(int cycles);
typedef int (*DEV9handler)(void);

typedef void (*USBcallback)(int cycles);
typedef int (*USBhandler)(void);

typedef struct _GSdriverInfo
{
    char name[8];
    void *common;
} GSdriverInfo;

#ifdef __cplusplus
extern "C" {
#endif

/* GS plugin API */

// if this file is included with this define
// the next api will not be skipped by the compiler
#if defined(GSdefs) || defined(BUILTIN_GS_PLUGIN)

// basic funcs

s32 CALLBACK GSinit();
s32 CALLBACK GSopen(void *pDsp, const char *Title, int multithread);
void CALLBACK GSclose();
void CALLBACK GSshutdown();
void CALLBACK GSsetSettingsDir(const char *dir);
void CALLBACK GSsetLogDir(const char *dir);

void CALLBACK GSvsync(int field);
void CALLBACK GSgifTransfer(const u32 *pMem, u32 addr);
void CALLBACK GSgifTransfer1(u32 *pMem, u32 addr);
void CALLBACK GSgifTransfer2(u32 *pMem, u32 size);
void CALLBACK GSgifTransfer3(u32 *pMem, u32 size);
void CALLBACK GSgetLastTag(u64 *ptag); // returns the last tag processed (64 bits)
void CALLBACK GSgifSoftReset(u32 mask);
void CALLBACK GSreadFIFO(u64 *mem);
void CALLBACK GSinitReadFIFO(u64 *mem);
void CALLBACK GSreadFIFO2(u64 *mem, int qwc);
void CALLBACK GSinitReadFIFO2(u64 *mem, int qwc);

// extended funcs

// GSkeyEvent gets called when there is a keyEvent from the PAD plugin
void CALLBACK GSkeyEvent(keyEvent *ev);
void CALLBACK GSchangeSaveState(int, const char *filename);
void CALLBACK GSmakeSnapshot(char *path);
void CALLBACK GSmakeSnapshot2(char *pathname, int *snapdone, int savejpg);
void CALLBACK GSirqCallback(void (*callback)());
void CALLBACK GSsetBaseMem(void *);
void CALLBACK GSsetGameCRC(int crc, int gameoptions);

// controls frame skipping in the GS, if this routine isn't present, frame skipping won't be done
void CALLBACK GSsetFrameSkip(int frameskip);

// if start is 1, starts recording spu2 data, else stops
// returns a non zero value if successful
// for now, pData is not used
int CALLBACK GSsetupRecording(int start, void *pData);

void CALLBACK GSreset();
//deprecated: GSgetTitleInfo was used in PCSX2 but no plugin supported it prior to r4070:
//void CALLBACK GSgetTitleInfo( char dest[128] );
void CALLBACK GSgetTitleInfo2(char *dest, size_t length);
void CALLBACK GSwriteCSR(u32 value);
s32 CALLBACK GSfreeze(int mode, freezeData *data);
void CALLBACK GSconfigure();
void CALLBACK GSabout();
s32 CALLBACK GStest();

#endif

/* PAD plugin API -=[ OBSOLETE ]=- */

// if this file is included with this define
// the next api will not be skipped by the compiler
#if defined(PADdefs) || defined(BUILTIN_PAD_PLUGIN)

// basic funcs

s32 CALLBACK PADinit(u32 flags);
s32 CALLBACK PADopen(void *pDsp);
void CALLBACK PADclose();
void CALLBACK PADshutdown();
void CALLBACK PADsetSettingsDir(const char *dir);
void CALLBACK PADsetLogDir(const char *dir);

// PADkeyEvent is called every vsync (return NULL if no event)
keyEvent *CALLBACK PADkeyEvent();
u8 CALLBACK PADstartPoll(int pad);
u8 CALLBACK PADpoll(u8 value);
// returns: 1 if supported pad1
//			2 if supported pad2
//			3 if both are supported
u32 CALLBACK PADquery();

// call to give a hint to the PAD plugin to query for the keyboard state. A
// good plugin will query the OS for keyboard state ONLY in this function.
// This function is necessary when multithreading because otherwise
// the PAD plugin can get into deadlocks with the thread that really owns
// the window (and input). Note that PADupdate can be called from a different
// thread than the other functions, so mutex or other multithreading primitives
// have to be added to maintain data integrity.
void CALLBACK PADupdate(int pad);

// Send a key event from wx-gui to pad
// Note: On linux GSOpen2, wx-gui and pad share the same event buffer. Wx-gui reads and deletes event
// before the pad saw them. So the gui needs to send them back to the pad.
void CALLBACK PADWriteEvent(keyEvent &evt);

// extended funcs

void CALLBACK PADgsDriverInfo(GSdriverInfo *info);
void CALLBACK PADconfigure();
void CALLBACK PADabout();
s32 CALLBACK PADtest();

#endif

/* DEV9 plugin API */

// if this file is included with this define
// the next api will not be skipped by the compiler
#if defined(DEV9defs) || defined(BUILTIN_DEV9_PLUGIN)

// basic funcs

// NOTE: The read/write functions CANNOT use XMM/MMX regs
// If you want to use them, need to save and restore current ones
s32 CALLBACK DEV9init();
s32 CALLBACK DEV9open(void *pDsp);
void CALLBACK DEV9close();
void CALLBACK DEV9shutdown();
void CALLBACK DEV9setSettingsDir(const char *dir);
void CALLBACK DEV9setLogDir(const char *dir);

u8 CALLBACK DEV9read8(u32 addr);
u16 CALLBACK DEV9read16(u32 addr);
u32 CALLBACK DEV9read32(u32 addr);
void CALLBACK DEV9write8(u32 addr, u8 value);
void CALLBACK DEV9write16(u32 addr, u16 value);
void CALLBACK DEV9write32(u32 addr, u32 value);
void CALLBACK DEV9readDMA8Mem(u32 *pMem, int size);
void CALLBACK DEV9writeDMA8Mem(u32 *pMem, int size);

// cycles = IOP cycles before calling callback,
// if callback returns 1 the irq is triggered, else not
void CALLBACK DEV9irqCallback(DEV9callback callback);
DEV9handler CALLBACK DEV9irqHandler(void);

// extended funcs

s32 CALLBACK DEV9freeze(int mode, freezeData *data);
void CALLBACK DEV9configure();
void CALLBACK DEV9about();
s32 CALLBACK DEV9test();

#endif

/* USB plugin API */

// if this file is included with this define
// the next api will not be skipped by the compiler
#if defined(USBdefs) || defined(BUILTIN_USB_PLUGIN)

// basic funcs

s32 CALLBACK USBinit();
s32 CALLBACK USBopen(void *pDsp);
void CALLBACK USBclose();
void CALLBACK USBshutdown();
void CALLBACK USBsetSettingsDir(const char *dir);
void CALLBACK USBsetLogDir(const char *dir);

u8 CALLBACK USBread8(u32 addr);
u16 CALLBACK USBread16(u32 addr);
u32 CALLBACK USBread32(u32 addr);
void CALLBACK USBwrite8(u32 addr, u8 value);
void CALLBACK USBwrite16(u32 addr, u16 value);
void CALLBACK USBwrite32(u32 addr, u32 value);
void CALLBACK USBasync(u32 cycles);

// cycles = IOP cycles before calling callback,
// if callback returns 1 the irq is triggered, else not
void CALLBACK USBirqCallback(USBcallback callback);
USBhandler CALLBACK USBirqHandler(void);
void CALLBACK USBsetRAM(void *mem);

// extended funcs

s32 CALLBACK USBfreeze(int mode, freezeData *data);
void CALLBACK USBconfigure();
void CALLBACK USBabout();
s32 CALLBACK USBtest();

#endif

// might be useful for emulators
#ifdef PLUGINtypedefs

typedef u32(CALLBACK *_PS2EgetLibType)(void);
typedef u32(CALLBACK *_PS2EgetLibVersion2)(u32 type);
typedef char *(CALLBACK *_PS2EgetLibName)(void);

// GS
// NOTE: GSreadFIFOX/GSwriteCSR functions CANNOT use XMM/MMX regs
// If you want to use them, need to save and restore current ones
typedef void(CALLBACK *_GSosdLog)(const char *utf8, u32 color);
typedef void(CALLBACK *_GSosdMonitor)(const char *key, const char *value, u32 color);
typedef s32(CALLBACK *_GSopen)(void *pDsp, const char *Title, int multithread);
typedef s32(CALLBACK *_GSopen2)(void *pDsp, u32 flags);
typedef void(CALLBACK *_GSvsync)(int field);
typedef void(CALLBACK *_GSgifTransfer)(const u32 *pMem, u32 size);
typedef void(CALLBACK *_GSgifTransfer1)(u32 *pMem, u32 addr);
typedef void(CALLBACK *_GSgifTransfer2)(u32 *pMem, u32 size);
typedef void(CALLBACK *_GSgifTransfer3)(u32 *pMem, u32 size);
typedef void(CALLBACK *_GSgifSoftReset)(u32 mask);
typedef void(CALLBACK *_GSreadFIFO)(u64 *pMem);
typedef void(CALLBACK *_GSreadFIFO2)(u64 *pMem, int qwc);
typedef void(CALLBACK *_GSinitReadFIFO)(u64 *pMem);
typedef void(CALLBACK *_GSinitReadFIFO2)(u64 *pMem, int qwc);

typedef void(CALLBACK *_GSchangeSaveState)(int, const char *filename);
typedef void(CALLBACK *_GSgetTitleInfo2)(char *dest, size_t length);
typedef void(CALLBACK *_GSirqCallback)(void (*callback)());
typedef void(CALLBACK *_GSsetBaseMem)(void *);
typedef void(CALLBACK *_GSsetGameCRC)(int, int);
typedef void(CALLBACK *_GSsetFrameSkip)(int frameskip);
typedef void(CALLBACK *_GSsetVsync)(int enabled);
typedef void(CALLBACK *_GSsetExclusive)(int isExclusive);
typedef std::wstring*(CALLBACK *_GSsetupRecording)(int);
typedef void(CALLBACK *_GSreset)();
typedef void(CALLBACK *_GSwriteCSR)(u32 value);
typedef void(CALLBACK *_GSmakeSnapshot)(const char *path);
typedef void(CALLBACK *_GSmakeSnapshot2)(const char *path, int *, int);

// PAD
typedef s32(CALLBACK *_PADinit)(u32 flags);
typedef s32(CALLBACK *_PADopen)(void *pDsp);
typedef u8(CALLBACK *_PADstartPoll)(int pad);
typedef u8(CALLBACK *_PADpoll)(u8 value);
typedef u32(CALLBACK *_PADquery)(int pad);
typedef void(CALLBACK *_PADupdate)(int pad);
typedef keyEvent *(CALLBACK *_PADkeyEvent)();
typedef void(CALLBACK *_PADgsDriverInfo)(GSdriverInfo *info);
typedef s32(CALLBACK *_PADsetSlot)(u8 port, u8 slot);
typedef s32(CALLBACK *_PADqueryMtap)(u8 port);
typedef void(CALLBACK *_PADWriteEvent)(keyEvent &evt);

// DEV9
// NOTE: The read/write functions CANNOT use XMM/MMX regs
// If you want to use them, need to save and restore current ones
typedef s32(CALLBACK *_DEV9open)(void *pDsp);
typedef u8(CALLBACK *_DEV9read8)(u32 mem);
typedef u16(CALLBACK *_DEV9read16)(u32 mem);
typedef u32(CALLBACK *_DEV9read32)(u32 mem);
typedef void(CALLBACK *_DEV9write8)(u32 mem, u8 value);
typedef void(CALLBACK *_DEV9write16)(u32 mem, u16 value);
typedef void(CALLBACK *_DEV9write32)(u32 mem, u32 value);
typedef void(CALLBACK *_DEV9readDMA8Mem)(u32 *pMem, int size);
typedef void(CALLBACK *_DEV9writeDMA8Mem)(u32 *pMem, int size);
typedef void(CALLBACK *_DEV9irqCallback)(DEV9callback callback);
typedef DEV9handler(CALLBACK *_DEV9irqHandler)(void);
typedef void(CALLBACK *_DEV9async)(u32 cycles);

// USB
// NOTE: The read/write functions CANNOT use XMM/MMX regs
// If you want to use them, need to save and restore current ones
typedef s32(CALLBACK *_USBopen)(void *pDsp);
typedef u8(CALLBACK *_USBread8)(u32 mem);
typedef u16(CALLBACK *_USBread16)(u32 mem);
typedef u32(CALLBACK *_USBread32)(u32 mem);
typedef void(CALLBACK *_USBwrite8)(u32 mem, u8 value);
typedef void(CALLBACK *_USBwrite16)(u32 mem, u16 value);
typedef void(CALLBACK *_USBwrite32)(u32 mem, u32 value);
typedef void(CALLBACK *_USBasync)(u32 cycles);

typedef void(CALLBACK *_USBirqCallback)(USBcallback callback);
typedef USBhandler(CALLBACK *_USBirqHandler)(void);
typedef void(CALLBACK *_USBsetRAM)(void *mem);
#endif

#ifdef PLUGINfuncs

// GS
#ifndef BUILTIN_GS_PLUGIN
extern _GSosdLog GSosdLog;
extern _GSosdMonitor GSosdMonitor;
extern _GSopen GSopen;
extern _GSopen2 GSopen2;
extern _GSvsync GSvsync;
extern _GSgifTransfer GSgifTransfer;
extern _GSgifTransfer1 GSgifTransfer1;
extern _GSgifTransfer2 GSgifTransfer2;
extern _GSgifTransfer3 GSgifTransfer3;
extern _GSgifSoftReset GSgifSoftReset;
extern _GSreadFIFO GSreadFIFO;
extern _GSinitReadFIFO GSinitReadFIFO;
extern _GSreadFIFO2 GSreadFIFO2;
extern _GSinitReadFIFO2 GSinitReadFIFO2;

extern _GSchangeSaveState GSchangeSaveState;
extern _GSgetTitleInfo2 GSgetTitleInfo2;
extern _GSmakeSnapshot GSmakeSnapshot;
extern _GSmakeSnapshot2 GSmakeSnapshot2;
extern _GSirqCallback GSirqCallback;
extern _GSsetBaseMem GSsetBaseMem;
extern _GSsetGameCRC GSsetGameCRC;
extern _GSsetFrameSkip GSsetFrameSkip;
extern _GSsetVsync GSsetVsync;
extern _GSsetupRecording GSsetupRecording;
extern _GSreset GSreset;
extern _GSwriteCSR GSwriteCSR;
#endif

// PAD
#ifndef BUILTIN_PAD_PLUGIN
extern _PADopen PADopen;
extern _PADstartPoll PADstartPoll;
extern _PADpoll PADpoll;
extern _PADquery PADquery;
extern _PADupdate PADupdate;
extern _PADkeyEvent PADkeyEvent;
extern _PADgsDriverInfo PADgsDriverInfo;
extern _PADsetSlot PADsetSlot;
extern _PADqueryMtap PADqueryMtap;
extern _PADWriteEvent PADWriteEvent;
#endif

// DEV9
#ifndef BUILTIN_DEV9_PLUGIN
extern _DEV9open DEV9open;
extern _DEV9read8 DEV9read8;
extern _DEV9read16 DEV9read16;
extern _DEV9read32 DEV9read32;
extern _DEV9write8 DEV9write8;
extern _DEV9write16 DEV9write16;
extern _DEV9write32 DEV9write32;
extern _DEV9readDMA8Mem DEV9readDMA8Mem;
extern _DEV9writeDMA8Mem DEV9writeDMA8Mem;
extern _DEV9irqCallback DEV9irqCallback;
extern _DEV9irqHandler DEV9irqHandler;
extern _DEV9async DEV9async;
#endif

// USB
#ifndef BUILTIN_USB_PLUGIN
extern _USBopen USBopen;
extern _USBread8 USBread8;
extern _USBread16 USBread16;
extern _USBread32 USBread32;
extern _USBwrite8 USBwrite8;
extern _USBwrite16 USBwrite16;
extern _USBwrite32 USBwrite32;
extern _USBasync USBasync;

extern _USBirqCallback USBirqCallback;
extern _USBirqHandler USBirqHandler;
extern _USBsetRAM USBsetRAM;
#endif

#endif

#ifdef __cplusplus
} // End extern "C"
#endif

#endif /* __PS2EDEFS_H__ */
