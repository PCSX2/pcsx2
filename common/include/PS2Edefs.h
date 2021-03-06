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

// PS2EgetLibType returns (may be OR'd)
#define PS2E_LT_GS 0x01

// PS2EgetLibVersion2 (high 16 bits)
#define PS2E_GS_VERSION 0x0006

// key values:
/* key values must be OS dependant:
	win32: the VK_XXX will be used (WinUser)
	linux: the XK_XXX will be used (XFree86)
*/

// for 64bit compilers
typedef char __keyEvent_Size__[(sizeof(keyEvent) == 8) ? 1 : -1];

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

// Starts recording GS frame data
// returns true if successful
bool CALLBACK GSsetupRecording(std::string& filename);

// Stops recording GS frame data
void CALLBACK GSendRecording();

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
typedef bool(CALLBACK* _GSsetupRecording)(std::string&);
typedef void(CALLBACK* _GSendRecording)();
typedef void(CALLBACK *_GSreset)();
typedef void(CALLBACK *_GSwriteCSR)(u32 value);
typedef bool(CALLBACK *_GSmakeSnapshot)(const char *path);
typedef void(CALLBACK *_GSmakeSnapshot2)(const char *path, int *, int);

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
extern _GSendRecording GSendRecording;
extern _GSreset GSreset;
extern _GSwriteCSR GSwriteCSR;
#endif

#endif

#ifdef __cplusplus
} // End extern "C"
#endif

#endif /* __PS2EDEFS_H__ */
