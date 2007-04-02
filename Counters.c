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

#include <string.h>
#include <time.h>
#include <math.h>
#include "Common.h"
#include "PsxCommon.h"

int gates = 0;
u8 eehblankgate = 0;
int cnts = 5;
extern u8 psxhblankgate;
int hblankend = 1;
Counter counters[6];
u32 nextCounter, nextsCounter;

void rcntUpdTarget(int index) {
	counters[index].sCycleT = cpuRegs.cycle;
	counters[index].CycleT  = (counters[index].target - rcntRcount(index)) * counters[index].rate;
}

void rcntUpd(int index) {
	counters[index].sCycle = cpuRegs.cycle;
	counters[index].Cycle  = (0xffff - (rcntRcount(index)&0xffff)) * counters[index].rate;
	rcntUpdTarget(index);
}

void rcntReset(int index) {
	counters[index].count = 0;
	counters[index].mode&= ~0x00400C00;
	rcntUpd(index);
}

void rcntSet() {
	u32 c;
	int i;

	nextCounter = 0xffff;
	nextsCounter = cpuRegs.cycle;

	for (i = 0; i < 4; i++) {
		if ((counters[i].mode & 0x380) < 0x80) continue; // Stopped

		if (counters[i].mode & 0x200){
			c = (0xffff - rcntCycle(i)) * counters[i].rate;
			if (c < nextCounter) {
				nextCounter = c;
			}
		}
		if ((counters[i].mode & 0x500) == 0x100){
			c = (counters[i].target - rcntCycle(i)) * counters[i].rate;
			if (c < nextCounter) {
			nextCounter = c;
			}
		}
	}
	//Calculate HBlank
	c = counters[4].CycleT - (cpuRegs.cycle - counters[4].sCycleT);
	if (c < nextCounter) {
		nextCounter = c;
	}
	c = counters[5].CycleT - (cpuRegs.cycle - counters[5].sCycleT);
	if (c < nextCounter) {
		nextCounter = c;
	}
}

void rcntInit() {
	int i;

	memset(counters, 0, sizeof(counters));

	for (i=0; i<4; i++) {
		counters[i].rate = 2;
		counters[i].target = 0xffff;
	}
	counters[0].interrupt =  9;
	counters[1].interrupt = 10;
	counters[2].interrupt = 11;
	counters[3].interrupt = 12;

	counters[4].mode = 0x3c0; // The VSync counter mode
	//counters[4].count = 0;
	counters[5].mode = 0x3c0;
	UpdateVSyncRate();
	counters[4].sCycleT = cpuRegs.cycle;
	counters[4].sCycle = cpuRegs.cycle;
	counters[4].CycleT  = counters[4].rate;
	counters[4].Cycle  = counters[4].rate;
	counters[5].sCycleT = cpuRegs.cycle;
	counters[5].sCycle = cpuRegs.cycle;
	counters[5].CycleT  = counters[5].rate;
	counters[5].Cycle  = counters[5].rate;
	for (i=0; i<4; i++) rcntUpd(i);
	rcntSet();
}

void UpdateVSyncRate() {
	if (Config.PsxType & 1) {
		counters[4].rate = PS2HBLANK_PAL;
		if(Config.PsxType & 2)counters[5].rate = PS2VBLANK_PAL_INT;
		else counters[5].rate = PS2VBLANK_PAL;
	} else {
		counters[4].rate = PS2HBLANK_NTSC;
		if(Config.PsxType & 2)counters[5].rate = PS2VBLANK_NTSC_INT;
		else counters[5].rate = PS2VBLANK_NTSC;
	}
}


#define NOSTATS

// debug code, used for stats
int g_nCounters[4];
extern u32 s_lastvsync[2];
LARGE_INTEGER lfreq;
static int iFrame = 0;	

void FrameLimiter()
{
	static u32 dwStartTime = 0, dwEndTime = 0;

	// do over 4 frames instead of 1
	if( (iFrame&3) == 0 ) {
		u32 frames = (Config.PsxType&1) ? (4000 / 50 -1) : (4000 / 60 - 1);
		dwEndTime = timeGetTime();

		if( dwEndTime < dwStartTime + frames ) {
			Sleep(frames-(dwEndTime-dwStartTime));
		}

		dwStartTime = timeGetTime();
	}
}

extern u32 CSRw;
extern u32 SuperVUGetRecTimes(int clear);
extern u32 vu0time;

#include "VU.h"
void VSync() {

	//QueryPerformanceFrequency(&lfreq);

	if (counters[5].mode & 0x10000) { // VSync End (22 hsyncs)

		// swap the vsync field
		u32 newfield = (*(u32*)(PS2MEM_GS+0x1000)&0x2000) ? 0 : 0x2000;
		*(u32*)(PS2MEM_GS+0x1000) = (*(u32*)(PS2MEM_GS+0x1000) & ~(1<<13)) | newfield;
		iFrame++;

		// wait until GS stops
		if( CHECK_MULTIGS ) {
			GSRingBufVSync(newfield);
		}
		else {
#ifdef GSCAPTURE
			extern u32 g_gstransnum;
			g_gstransnum = 0;
#endif
			GSvsync(newfield);
		}

		//SysPrintf("c: %x, %x\n", cpuRegs.cycle, *(u32*)&VU1.Micro[16]);
		//if( (iFrame%20) == 0 ) SysPrintf("svu time: %d\n", SuperVUGetRecTimes(1));
//		if( (iFrame%10) == 0 ) {
//			SysPrintf("vu0 time: %d\n", vu0time);
//			vu0time = 0;
//		}


#ifdef PCSX2_DEVBUILD
		if( g_TestRun.enabled && g_TestRun.frame > 0 ) {
			if( iFrame > g_TestRun.frame ) {
				// take a snapshot
				if( g_TestRun.pimagename != NULL && GSmakeSnapshot2 != NULL ) {
					if( g_TestRun.snapdone ) {
						g_TestRun.curimage++;
						g_TestRun.snapdone = 0;
						g_TestRun.frame += 20;
						if( g_TestRun.curimage >= g_TestRun.numimages ) {
							// exit
							SysClose();
							exit(0);
						}
					}
					else {
						// query for the image
						GSmakeSnapshot2(g_TestRun.pimagename, &g_TestRun.snapdone, g_TestRun.jpgcapture);
					}
				}
				else {
					// exit
					SysClose();
					exit(0);
				}
			}
		}
#endif

		if( CHECK_FRAMELIMIT ) FrameLimiter();

		counters[5].mode&= ~0x10000;
		//UpdateVSyncRate();
		GSCSRr |= 8; // signal
		if (CSRw & 0x8){		
			CSRw &= ~8;
			if (!(GSIMR&0x800) )
				gsIrq();
		}
		hwIntcIrq(3);		
		psxVSyncEnd();
#ifndef NOSTATS
		statsVSync();
#endif
		if(gates)rcntEndGate(0x8);
		SysUpdate();
	} else { // VSync Start (240 hsyncs) 
		//UpdateVSyncRateEnd();
		
		//SysPrintf("ctrs: %d %d %d %d\n", g_nCounters[0], g_nCounters[1], g_nCounters[2], g_nCounters[3]);
		//SysPrintf("vif: %d\n", (((LARGE_INTEGER*)g_nCounters)->QuadPart * 1000000) / lfreq.QuadPart);
		//memset(g_nCounters, 0, 16);
		counters[5].mode|= 0x10000;
		//GSCSRr|= 0x8;
		hwIntcIrq(2);
		psxVSyncStart();
		
		if(Config.Patch) applypatch(1);
		if(gates)rcntStartGate(0x8);

//		__Log("%u %u 0\n", cpuRegs.cycle-s_lastvsync[1], timeGetTime()-s_lastvsync[0]);
//		s_lastvsync[0] = timeGetTime();
//		s_lastvsync[1] = cpuRegs.cycle;
	}
}



void rcntUpdate() {
	int i;

	for (i=0; i<=3; i++) {
		if ((counters[i].mode & 0x380) < 0x80) continue; // Stopped

		
			
		if (rcntCycle(i) >= counters[i].target) { // Target interrupt
			if ((counters[i].mode & 0x500) == 0x100) { // 0x400 == 0 so target interrupt hasnt happened
			counters[i].mode|= 0x0400; // Target flag
				//counters[i].mode&= ~0x800;
				
			hwIntcIrq(counters[i].interrupt);
//				SysPrintf("counter[%d] target interrupt (%x)\n", i, cpuRegs.cycle);
			}
			if (counters[i].mode & 0x40) { // Reset on target
				counters[i].count = 0;
				rcntUpd(i);
			}
		}
		
		

		if (rcntCycle(i) >= 0xffff) {
			if (counters[i].mode & 0x0200) { // Overflow interrupt
				counters[i].mode|= 0x0800; // Overflow flag
				hwIntcIrq(counters[i].interrupt);
//				SysPrintf("counter[%d] overflow interrupt (%x)\n", i, cpuRegs.cycle);
			}
			counters[i].count = 0;
			rcntUpd(i);
		} 
		
	}

	if ((cpuRegs.cycle - counters[4].sCycleT) >= counters[4].CycleT / 2 && hblankend == 0){
		GSCSRr |= 4; // signal
		if (CSRw & 0x4){
			CSRw &= ~4;
			if (!(GSIMR&0x400) )
				gsIrq();
		}
		if(eehblankgate)rcntEndGate(0);
		if(psxhblankgate)psxCheckEndGate(0);
		hblankend = 1;
	}
	if ((cpuRegs.cycle - counters[4].sCycleT) >= counters[4].CycleT) {
		counters[4].sCycleT = cpuRegs.cycle;
		counters[4].sCycle = cpuRegs.cycle;
		counters[4].CycleT  = counters[4].rate;
		counters[4].Cycle  = counters[4].rate;
		
	    
		if(gates)rcntStartGate(0);
		psxCheckStartGate(0);
		hblankend = 0;
	}
	
	if ((cpuRegs.cycle - counters[5].sCycleT) >= counters[5].CycleT / 2 && (counters[5].mode & 0x10000)) VSync();
		
		if ((cpuRegs.cycle - counters[5].sCycleT) >= counters[5].CycleT) {
			counters[5].sCycleT = cpuRegs.cycle;
			counters[5].sCycle = cpuRegs.cycle;
			counters[5].CycleT  = counters[5].rate;
			counters[5].Cycle  = counters[5].rate;
			VSync();
		}
	rcntSet();
}

void rcntWcount(int index, u32 value) {
	//SysPrintf ("writeCcount[%d] = %x\n", index, value);
#ifdef PSXCNT_LOG
	PSXCNT_LOG("EE count write %d count %x eecycle %x\n", index, counters[index].count, cpuRegs.eCycle);
#endif
	
	 //counters[index].mode &= ~0x00400C00;
	counters[index].count = value;
	rcntUpd(index);
	rcntSet();
}

void rcntWmode(int index, u32 value)  {
//	SysPrintf ("writeCmode[%ld] = %lx\n", index, value);
#ifdef PSXCNT_LOG
	PSXCNT_LOG("EE counter set %d mode %x\n", index, counters[index].mode);
#endif
	if (value & 0xc00) {
		counters[index].mode &= ~(value & 0xc00);
	}
	/*if ((counters[index].mode & 0x37F) == (value & 0x37f)){ //Stop Counting
		if((counters[index].mode & 0x80) != (value & 0x80)){
			SysPrintf("EE Counter %x %s\n", index, (counters[index].mode & 0x80) ? "stopped" : "started");
			if(counters[index].mode & 0x80) counters[index].count = rcntRcount(index);
			counters[index].mode ^= 0x80;
			rcntUpd(index);
		}
		rcntSet();
		return;
	}*/
	if(!(value & 0x80)){
		counters[index].count = rcntRcount(index);
		rcntUpd(index);
	}

counters[index].mode = (value & 0x3ff) | (counters[index].mode & 0xc00);


	switch (value & 0x3) {                        //Clock rate divisers *2, they use BUSCLK speed not PS2CLK
		case 0: counters[index].rate = 2; break;
		case 1: counters[index].rate = 32; break;
		case 2: counters[index].rate = 512; break;
		case 3: counters[index].rate = PS2HBLANK; break;
	}

	if((counters[index].mode & 0xF) == 0x7) {
			gates &= ~(1<<index);
	}else if(counters[index].mode & 0x4){
			gates |= 1<<index;
			SysPrintf("Gate Being set on %x, mode %x\n", index, counters[index].mode & 0xf);
	}
	else gates &= ~(1<<index);
	
	
	rcntSet();

}

void rcntStartGate(int mode){
	int i;
	
	for(i=0; i <=3; i++){  //Gates for counters
		if(!(gates & (1<<i))) continue;
		if ((counters[i].mode & 0x8) != mode) continue;
		//SysPrintf("Gates init %x\n", i);
		if(mode == 0) eehblankgate |= 1;
	
		switch((counters[i].mode & 0x30) >> 4){
			case 0x0: //Count When Signal is low (off)
				counters[i].count = rcntRcount(i);
				//rcntUpd(i);
				counters[i].mode &= ~0x80;
				break;
			case 0x1: //Reset and start counting on Vsync start
				counters[i].mode |= 0x80;
				rcntReset(i);
				break;
			case 0x2: //Reset and start counting on Vsync end
				//Do Nothing
				break;
			case 0x3: //Reset and start counting on Vsync start and end
				counters[i].mode |= 0x80;
				rcntReset(i);
				break;
			default:
				SysPrintf("EE Start Counter %x Gate error\n", i);
				break;
		}
	}
}
void rcntEndGate(int mode){
	int i;

	for(i=0; i <=3; i++){  //Gates for counters
		if(!(gates & (1<<i))) continue;
		if ((counters[i].mode & 0x8) != mode) continue;
		if(mode == 0) eehblankgate &= ~1;
		//SysPrintf("Gates end init %x\n", i);
		switch((counters[i].mode & 0x30) >> 4){
			case 0x0: //Count When Signal is low (off)
				rcntUpd(i);
				counters[i].mode |= 0x80;
				break;
			case 0x1: //Reset and start counting on Vsync start
				//Do Nothing
				break;
			case 0x2: //Reset and start counting on Vsync end
				rcntReset(i);
				counters[i].mode |= 0x80;
				break;
			case 0x3: //Reset and start  counting on Vsync start and end
				rcntReset(i);
				counters[i].mode |= 0x80;
				break;
			default:
				SysPrintf("EE Start Counter %x Gate error\n", i);
				break;
		}
	}
}
void rcntWtarget(int index, u32 value) {
	//SysPrintf ("writeCtarget[%ld] = %lx\n", index, value);
#ifdef PSXCNT_LOG
	PSXCNT_LOG("EE target write %d target %x eecycle %x\n", index, counters[index].target, cpuRegs.eCycle);
#endif
	counters[index].target = value;
	//counters[index].mode &= ~0x00400400;
	counters[index].CycleT  = (counters[index].target - rcntRcount(index)) * counters[index].rate;
	//rcntUpdTarget(index);
	rcntSet();
}

void rcntWhold(int index, u32 value) {
	//SysPrintf ("writeChold[%ld] = %lx\n", index, value);
	counters[index].hold = value;
	//rcntUpd(index);
	//rcntSet();
}

u16 rcntRcount(int index) {
	u16 ret;
	if ((counters[index].mode & 0x80)) {
	ret = counters[index].count + ((cpuRegs.cycle - counters[index].sCycle) / counters[index].rate);
	}else{
	ret = counters[index].count;
	}
//	SysPrintf("rcntRcount[%d] %x\n", index, ret);
	return (u16)ret;
}

u32 rcntCycle(int index) {
	
	if ((counters[index].mode & 0x80)) {
	return counters[index].count + ((cpuRegs.cycle - counters[index].sCycle) / counters[index].rate);
	}else{
	return counters[index].count;
	}
}

int rcntFreeze(gzFile f, int Mode) {
	gzfreezel(counters);
	gzfreeze(&nextCounter, sizeof(nextCounter));
	gzfreeze(&nextsCounter, sizeof(nextsCounter));

	return 0;
}

