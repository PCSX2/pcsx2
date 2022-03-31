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

#include "common/WindowInfo.h"
#include "SaveState.h"
#include "Host.h"

s32 PADinit();
void PADshutdown();
s32 PADopen(const WindowInfo& wi);
void PADupdate(int pad);
void PADclose();
u8 PADstartPoll(int pad);
u8 PADpoll(u8 value);
HostKeyEvent* PADkeyEvent();
void PADconfigure();
s32 PADfreeze(FreezeAction mode, freezeData* data);
s32 PADsetSlot(u8 port, u8 slot);

#if defined(__unix__) || defined(__APPLE__)
void PADWriteEvent(HostKeyEvent& evt);
#endif
