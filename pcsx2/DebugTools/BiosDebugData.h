/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2014-2014  PCSX2 Dev Team
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
#include "common/Pcsx2Types.h"
#include "ps2/BiosTools.h"

struct EEInternalThread { // internal struct
	u32 prev;
	u32 next;
	int status;
	u32 entry;
	u32 stack;
	u32 gpReg;
	short currentPriority;
	short initPriority;
	int waitType;
	int semaId;
	int wakeupCount;
	int attr;
	int option;
	u32 entry_init;
	int argc;
	u32 argstring;
	u32 stack_bottom; //FIXME
	int stackSize;
	u32 root;
	u32 heap_base;
};

enum {
	THS_BAD          = 0x00,
	THS_RUN          = 0x01,
	THS_READY        = 0x02,
	THS_WAIT         = 0x04,
	THS_SUSPEND      = 0x08,
	THS_WAIT_SUSPEND = 0x0C,
	THS_DORMANT      = 0x10,
};
 
enum {
	WAIT_NONE       = 0,
	WAIT_WAKEUP_REQ = 1,
	WAIT_SEMA       = 2,
};

struct EEThread
{
	int tid;
	EEInternalThread data;
};

std::vector<EEThread> getEEThreads();
