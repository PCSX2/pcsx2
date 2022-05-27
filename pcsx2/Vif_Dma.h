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

#include "Vif.h"
#include "Vif_Unpack.h"

struct vifCode {
   u32 addr;
   u32 size;
   u32 cmd;
   u16 wl;
   u16 cl;
};

union tBITBLTBUF {
	u64 _u64;
	struct {
		u32 SBP : 14;
		u32 _pad14 : 2;
		u32 SBW : 6;
		u32 _pad22 : 2;
		u32 SPSM : 6;
		u32 _pad30 : 2;
		u32 DBP : 14;
		u32 _pad46 : 2;
		u32 DBW : 6;
		u32 _pad54 : 2;
		u32 DPSM : 6;
		u32 _pad62 : 2;
	};
};

union tTRXPOS {
	u64 _u64;
	struct {
		u32 SSAX : 11;
		u32 _PAD1 : 5;
		u32 SSAY : 11;
		u32 _PAD2 : 5;
		u32 DSAX : 11;
		u32 _PAD3 : 5;
		u32 DSAY : 11;
		u32 DIRY : 1;
		u32 DIRX : 1;
		u32 _PAD4 : 3;
	};
};

union tTRXREG
{
	u64 _u64;
	struct
	{
		u32 RRW : 12;
		u32 _pad12 : 20;
		u32 RRH : 12;
		u32 _pad44 : 20;
	};
};

struct tVIF_CTRL {
   bool enabled;
   u32 value;
};

// NOTE, if debugging vif stalls, use sega classics, spyro, gt4, and taito
struct vifStruct {
	alignas(16) u128 MaskRow;
	alignas(16) u128 MaskCol;

	struct { // These must be together for MTVU
		vifCode tag;
		int cmd;
		int pass;
		int cl;
		u8  usn;
		u8 start_aligned;
		u8  StructEnd; // Address of this is used to calculate end of struct
	};

	int irq;

	bool done;
	tVIF_CTRL vifstalled;
	bool stallontag;
	bool waitforvu;
	int unpackcalls;
	// GS registers used for calculating the size of the last local->host transfer initiated on the GS
	// Transfer size calculation should be restricted to GS emulation in the future
	union
	{
		struct
		{
			tBITBLTBUF BITBLTBUF;
			tTRXPOS    TRXPOS;
			tTRXREG    TRXREG;
		};
		u64 transfer_registers[3];
	};

	u32        GSLastDownloadSize;

	tVIF_CTRL  irqoffset; // 32bit offset where next vif code is
	u32 vifpacketsize;
	u8  inprogress;
	u8  dmamode;

	bool queued_program;
	u32 queued_pc;
	bool queued_gif_wait;
};

alignas(16) extern vifStruct  vif0, vif1;

_vifT extern u32 vifRead32(u32 mem);
_vifT extern bool vifWrite32(u32 mem, u32 value);
extern void vif0Interrupt();
extern void vif0VUFinish();
extern void vif0Reset();

extern void vif1Interrupt();
extern void vif1VUFinish();
extern void vif1Reset();

typedef int FnType_VifCmdHandler(int pass, const u32 *data);
typedef FnType_VifCmdHandler* Fnptr_VifCmdHandler;

alignas(16) extern const Fnptr_VifCmdHandler vifCmdHandler[2][128];

__fi static int _limit(int a, int max)
{
	return ((a > max) ? max : a);
}

enum VifModes
{
	VIF_NORMAL_TO_MEM_MODE = 0,
	VIF_NORMAL_FROM_MEM_MODE = 1,
	VIF_CHAIN_MODE = 2
};

// Generic constants
static const unsigned int VIF0intc = 4;
static const unsigned int VIF1intc = 5;

extern u32 g_vif0Cycles;
extern u32 g_vif1Cycles;

extern void vif0FLUSH();
extern void vif1FLUSH();

extern void vifExecQueue(int idx);
