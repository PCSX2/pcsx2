/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#define GAMEPAD_NUMBER 2 // numbers of gamepad

#include <wx/string.h>
#include <wx/tokenzr.h>
#include <wx/intl.h>
#include <wx/log.h>
#include <wx/filename.h>
#include "Utilities/pxStreams.h"
#include "Utilities/Console.h"
#include <stdio.h>
#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>

#else
/*
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
*/

#endif

#include <array>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <mutex>
#include <queue>

#define PADdefs

enum PadOptions
{
	PADOPTION_FORCEFEEDBACK = 0x1,
	PADOPTION_REVERSELX = 0x2,
	PADOPTION_REVERSELY = 0x4,
	PADOPTION_REVERSERX = 0x8,
	PADOPTION_REVERSERY = 0x10,
	PADOPTION_MOUSE_L = 0x20,
	PADOPTION_MOUSE_R = 0x40,
};

enum PadCommands
{
	CMD_SET_VREF_PARAM = 0x40,
	CMD_QUERY_DS2_ANALOG_MODE = 0x41,
	CMD_READ_DATA_AND_VIBRATE = 0x42,
	CMD_CONFIG_MODE = 0x43,
	CMD_SET_MODE_AND_LOCK = 0x44,
	CMD_QUERY_MODEL_AND_MODE = 0x45,
	CMD_QUERY_ACT = 0x46, // ??
	CMD_QUERY_COMB = 0x47, // ??
	CMD_QUERY_MODE = 0x4C, // QUERY_MODE ??
	CMD_VIBRATION_TOGGLE = 0x4D,
	CMD_SET_DS2_NATIVE_MODE = 0x4F // SET_DS2_NATIVE_MODE
};

#if defined(__unix__) || defined(__APPLE__)
#include "GamePad.h"
#endif
#include "bitwise.h"
#include "controller.h"
#include "KeyStatus.h"
#include "mt_queue.h"

extern FILE* padLog;
extern void initLogging();

//#define PAD_LOG __Log
//#define PAD_LOG __LogToConsole

extern keyEvent event;
extern MtQueue<keyEvent> g_ev_fifo;

s32 _PADopen(void* pDsp);
void _PADclose();
void PADsetMode(int pad, int mode);

void __LogToConsole(const char* fmt, ...);
void PADLoadConfig();
void PADSaveConfig();

void SysMessage(char* fmt, ...);

s32 PADinit();
void PADshutdown();
s32 PADopen(void* pDsp);
void PADsetLogDir(const char* dir);
void PADclose();
s32 PADsetSlot(u8 port, u8 slot);
s32 PADfreeze(int mode, freezeData* data);
u8 PADstartPoll(int pad);
u8 PADpoll(u8 value);
keyEvent* PADkeyEvent();
void PADupdate(int pad);
void PADconfigure();
void PADDoFreezeOut(void* dest);
void PADDoFreezeIn(pxInputStream& infp);

#if defined(__unix__)
void PADWriteEvent(keyEvent& evt);
#endif
