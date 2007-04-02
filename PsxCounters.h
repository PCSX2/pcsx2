/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2003  Pcsx2 Team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __PSXCOUNTERS_H__
#define __PSXCOUNTERS_H__

typedef struct {
	u32 count, mode, target;
	u32 rate, interrupt, otarget;
	u32 sCycle, Cycle;
	u32 sCycleT, CycleT;
} psxCounter;

psxCounter psxCounters[8];

u32 psxNextCounter, psxNextsCounter;

void psxRcntInit();
void psxRcntUpdate();
void cntspu2async();
void psxRcntWcount16(int index, u32 value);
void psxRcntWcount32(int index, u32 value);
void psxRcnt0Wmode(u32 value);
void psxRcnt1Wmode(u32 value);
void psxRcnt2Wmode(u32 value);
void psxRcnt3Wmode(u32 value);
void psxRcnt4Wmode(u32 value);
void psxRcnt5Wmode(u32 value);
void psxRcntWtarget16(int index, u32 value);
void psxRcntWtarget32(int index, u32 value);
u16  psxRcntRcount16(int index);
u32  psxRcntRcount32(int index);
u64  psxRcntCycles(int index);
int  psxRcntFreeze(gzFile f, int Mode);

void psxVSyncStart();
void psxVSyncEnd();
void psxCheckStartGate(int counter);
void psxCheckEndGate(int counter);

#endif /* __PSXCOUNTERS_H__ */
