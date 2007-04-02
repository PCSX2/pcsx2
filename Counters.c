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
#include "GS.h"

int gates = 0;
extern u8 psxhblankgate;
int hblankend = 0;
Counter counters[6];
u32 nextCounter, nextsCounter;

// its so it doesnt keep triggering an interrupt once its reached its target
// if it doesnt reset the counter it needs stopping
u32 eecntmask = 0;

void rcntUpdTarget(int index) {
	counters[index].sCycleT = cpuRegs.cycle - (cpuRegs.cycle % counters[index].rate);
}

void rcntUpd(int index) {
	counters[index].sCycle = cpuRegs.cycle - (cpuRegs.cycle % counters[index].rate);
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

	nextCounter = 0xffffffff;
	nextsCounter = cpuRegs.cycle;

	for (i = 0; i < 4; i++) {
		if (!(counters[i].mode & 0x80)) continue; // Stopped

			c = (0xffff - rcntCycle(i)) * counters[i].rate;
			if (c < nextCounter) {
				nextCounter = c;
			}
		
		// the + 10 is just in case of overflow
			if(eecntmask & (1<<i) || !(counters[i].mode & 0x100)) continue;
			 c = (counters[i].target - rcntCycle(i)) * counters[i].rate;
			if (c < nextCounter) {
			nextCounter = c;
			}
	
	}
	//Calculate HBlank
	c = counters[4].CycleT - (cpuRegs.cycle - counters[4].sCycleT);
	if (c < nextCounter) {
		nextCounter = c;
	}
	//Calculate VBlank
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
	counters[5].mode = 0x3c0;

	counters[4].sCycleT = cpuRegs.cycle;
	counters[4].sCycle = cpuRegs.cycle;

	counters[5].sCycleT = cpuRegs.cycle;
	counters[5].sCycle = cpuRegs.cycle;
	UpdateVSyncRate();
	
	for (i=0; i<4; i++) rcntUpd(i);
	rcntSet();
}

void UpdateVSyncRate() {
	if (Config.PsxType & 1) {
		SysPrintf("PAL\n");
		counters[4].rate = PS2HBLANK_PAL;
		if(Config.PsxType & 2)counters[5].rate = PS2VBLANK_PAL_INT;
		else counters[5].rate = PS2VBLANK_PAL;
	} else {
		SysPrintf("NTSC\n");
		counters[4].rate = PS2HBLANK_NTSC;
		if(Config.PsxType & 2)counters[5].rate = PS2VBLANK_NTSC_INT;
		else counters[5].rate = PS2VBLANK_NTSC;
	}	
	counters[4].CycleT  = PS2HBLANKEND;
	counters[4].Cycle  = counters[4].rate-PS2HBLANKEND;
	
	counters[5].CycleT  = PS2VBLANKEND;
	counters[5].Cycle  = counters[5].rate-PS2VBLANKEND;
}

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
		u32 frames = (Config.PsxType&1) ? (4000 / 50 - 4) : (4000 / 60 - 4);
		dwEndTime = timeGetTime();

		if( dwEndTime < dwStartTime + frames ) {
			if( dwEndTime < dwStartTime + frames - 2 )
				Sleep(frames-(dwEndTime-dwStartTime)-2);

			while(dwEndTime < dwStartTime + frames) dwEndTime = timeGetTime();
		}

		dwStartTime = timeGetTime();
	}
}

extern u32 CSRw;
extern u32 SuperVUGetRecTimes(int clear);
extern u32 vu0time;

extern void recExecuteVU1Block(void);
extern void DummyExecuteVU1Block(void);

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
			GSRingBufSimplePacket(GS_RINGTYPE_VSYNC, newfield, 0, 0);
		}
		else {
			GSvsync(newfield);
		}

		counters[5].mode&= ~0x10000;
		hwIntcIrq(3);		
		psxVSyncEnd();
		
		if(gates)rcntEndGate(0x8);
		SysUpdate();
	} else { // VSync Start (240 hsyncs) 
		//UpdateVSyncRateEnd();
		

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

		GSVSYNC();

		if( g_SaveGSStream == 1 ) {
			freezeData fP;

			g_SaveGSStream = 2;
			gsFreeze(g_fGSSave, 1);
			
			if (GSfreeze(FREEZE_SIZE, &fP) == -1) {
				gzclose(g_fGSSave);
				g_SaveGSStream = 0;
			}
			else {
				fP.data = (s8*)malloc(fP.size);
				if (fP.data == NULL) {
					gzclose(g_fGSSave);
					g_SaveGSStream = 0;
				}
				else {
					if (GSfreeze(FREEZE_SAVE, &fP) == -1) {
						gzclose(g_fGSSave);
						g_SaveGSStream = 0;
					}
					else {
						gzwrite(g_fGSSave, &fP.size, sizeof(fP.size));
						if (fP.size) {
							gzwrite(g_fGSSave, fP.data, fP.size);
							free(fP.data);
						}
					}
				}
			}
		}
		else if( g_SaveGSStream == 2 ) {
			
			if( --g_nLeftGSFrames <= 0 ) {
				gzclose(g_fGSSave);
				g_fGSSave = NULL;
				g_SaveGSStream = 0;
				SysPrintf("Done saving GS stream\n");
			}
		}
#endif

		// used to limit frames
		switch(CHECK_FRAMELIMIT) {
			case PCSX2_FRAMELIMIT_LIMIT:
				FrameLimiter();
				break;

			case PCSX2_FRAMELIMIT_SKIP:
			case PCSX2_FRAMELIMIT_VUSKIP:
			{
				// the 6 was after trial and error
				static u32 uPrevTimes[6] = {0}, uNextFrame = 0, uNumFrames = 0, uTotalTime = 0;
				static u32 uLastTime = 0;
				static int nConsecutiveSkip = 0, nConsecutiveRender = 0;
				extern u32 g_bVUSkip;
				static int changed = 0;
				static int nNoSkipFrames = 0;
				
				u32 uExpectedTime;
				u32 uCurTime = timeGetTime();
				u32 uDeltaTime = uCurTime - uLastTime;

				assert( GSsetFrameSkip != NULL );

				if( uLastTime > 0 ) {

					if( uNumFrames == ARRAYSIZE(uPrevTimes) )
						uTotalTime -= uPrevTimes[uNextFrame];

					uPrevTimes[uNextFrame] = uDeltaTime;
					uNextFrame = (uNextFrame + 1) % ARRAYSIZE(uPrevTimes);
					uTotalTime += uDeltaTime;

					if( uNumFrames < ARRAYSIZE(uPrevTimes) )
						++uNumFrames;
				}

				uExpectedTime = (Config.PsxType&1) ? (ARRAYSIZE(uPrevTimes) * 1000 / 50 -1) : (ARRAYSIZE(uPrevTimes) * 1000 / 60 - 1);

				if( nNoSkipFrames > 0 )
					--nNoSkipFrames;

				// hmm... this might be more complicated than it needs to be
				if( changed != 0 ) {
					if( changed > 0 ) {
						++nConsecutiveRender;
						--changed;

						if( nConsecutiveRender > 20 && uTotalTime + 1 < uExpectedTime ) {
							Sleep(uExpectedTime-uTotalTime);
							nNoSkipFrames = ARRAYSIZE(uPrevTimes);
						}
					}
					else {
						++nConsecutiveSkip;
						++changed;
					}
				}
				else {
					if( nNoSkipFrames == 0 && nConsecutiveRender > 3 && nConsecutiveSkip < 20 &&
						(CHECK_MULTIGS? (uTotalTime >= uExpectedTime + uDeltaTime/4 && (uTotalTime >= uExpectedTime + uDeltaTime*3/4 || nConsecutiveSkip==0)) : 
										(uTotalTime >= uExpectedTime + (uDeltaTime/4))) ) {

						if( CHECK_FRAMELIMIT == PCSX2_FRAMELIMIT_VUSKIP ) {
							Cpu->ExecuteVU1Block = DummyExecuteVU1Block;
						}

						if( nConsecutiveSkip == 0 ) {
							if( CHECK_MULTIGS ) GSRingBufSimplePacket(GS_RINGTYPE_FRAMESKIP, 1, 0, 0);
							else GSsetFrameSkip(1);
						}

						changed = -3;
						nConsecutiveSkip++;
					}
					else {

						if( CHECK_FRAMELIMIT == PCSX2_FRAMELIMIT_VUSKIP ) {
							Cpu->ExecuteVU1Block = recExecuteVU1Block;
						}

						if( nConsecutiveSkip ) {
							if( CHECK_MULTIGS ) GSRingBufSimplePacket(GS_RINGTYPE_FRAMESKIP, 0, 0, 0);
							else GSsetFrameSkip(0);

							nConsecutiveRender = 0;
						}

						changed = 3;
						nConsecutiveRender++;
						nConsecutiveSkip = 0;

						if( nConsecutiveRender > 20 && uTotalTime + 1 < uExpectedTime ) {
							Sleep(uExpectedTime-uTotalTime);
							nNoSkipFrames = ARRAYSIZE(uPrevTimes);
						}
					}
				}

				uLastTime = uCurTime;

				break;
			}
		}

		//counters[5].mode&= ~0x10000;
		//UpdateVSyncRate();
		
		//SysPrintf("ctrs: %d %d %d %d\n", g_nCounters[0], g_nCounters[1], g_nCounters[2], g_nCounters[3]);
		//SysPrintf("vif: %d\n", (((LARGE_INTEGER*)g_nCounters)->QuadPart * 1000000) / lfreq.QuadPart);
		//memset(g_nCounters, 0, 16);
		counters[5].mode|= 0x10000;
	
		if (!(GSCSRr & 0x8)){
			GSCSRr|= 0x8;
			
		}
		if (!(GSIMR&0x800) )
				gsIrq();
		hwIntcIrq(2);
		psxVSyncStart();
		
		if(Config.Patch) applypatch(1);
		if(gates)rcntStartGate(0x8);

//		__Log("%u %u 0\n", cpuRegs.cycle-s_lastvsync[1], timeGetTime()-s_lastvsync[0]);
//		s_lastvsync[0] = timeGetTime();
//		s_lastvsync[1] = cpuRegs.cycle;
	}
}


void rcntUpdate()
{
	int i;
	for (i=0; i<=3; i++) {
		if (!(counters[i].mode & 0x80)) continue; // Stopped
		counters[i].count += (int)((cpuRegs.cycle - counters[i].sCycleT) / counters[i].rate);
		counters[i].sCycleT = cpuRegs.cycle - (cpuRegs.cycle % counters[i].rate);
	}
	for (i=0; i<=3; i++) {
		if (!(counters[i].mode & 0x80)) continue; // Stopped

		
			if ((counters[i].count & ~0x3) == (counters[i].target & ~0x3)) { // Target interrupt
				/*if (rcntCycle(i) != counters[i].target){
					SysPrintf("rcntcycle = %d, target = %d, cyclet = %d\n", rcntCycle(i), counters[i].target, counters[i].sCycleT);
					counters[i].sCycleT += (rcntCycle(i) - counters[i].target) * counters[i].rate;
					SysPrintf("rcntcycle = %d, target = %d, cyclet = %d\n", rcntCycle(i), counters[i].target, counters[i].sCycleT);
				}*/
				//if ((eecntmask & (1 << i)) == 0) {
				counters[i].mode|= 0x0400; // Target flag
				if(counters[i].mode & 0x100) {
					hwIntcIrq(counters[i].interrupt);
					
				}
				//eecntmask |= (1 << i);
				//}
				if (counters[i].mode & 0x40) { // Reset on target
					counters[i].count = 0;
					eecntmask &= ~(1 << i);
					//rcntUpd(i);
				}
			
		}
		
		
		if (counters[i].count >= 0xffff) {
			eecntmask &= ~(1 << i);
			counters[i].mode|= 0x0800; // Overflow flag
			if (counters[i].mode & 0x0200) { // Overflow interrupt
				
				hwIntcIrq(counters[i].interrupt);
//				SysPrintf("counter[%d] overflow interrupt (%x)\n", i, cpuRegs.cycle);
			}
			counters[i].count = 0;
			//rcntUpd(i);
		} 
	//	rcntUpd(i);
	}
	
	if ((cpuRegs.cycle - counters[4].sCycleT) >= counters[4].CycleT && hblankend == 1){
		
		if (!(GSCSRr & 0x4)){
			GSCSRr |= 4; // signal
			
		}
		if (!(GSIMR&0x400) )
				gsIrq();
		if(gates)rcntEndGate(0);
		if(psxhblankgate)psxCheckEndGate(0);
		hblankend = 0;
		counters[4].CycleT  = counters[4].rate;
	}
	if ((cpuRegs.cycle - counters[4].sCycleT) >= counters[4].CycleT) {
		counters[4].sCycleT = cpuRegs.cycle;
		counters[4].sCycle = cpuRegs.cycle;
		counters[4].CycleT  = counters[4].rate-PS2HBLANKEND;
		counters[4].Cycle  = counters[4].rate;
		counters[4].count = 0;
		
		if(gates)rcntStartGate(0);
		if(psxhblankgate)psxCheckStartGate(0);
		hblankend = 1;
	}
	
	if ((cpuRegs.cycle - counters[5].sCycleT)
		>= counters[5].CycleT && (counters[5].mode & 0x10000)){
			counters[5].CycleT  = counters[5].rate;
			VSync();
		}
		
		if ((cpuRegs.cycle - counters[5].sCycleT) >= counters[5].CycleT) {
			counters[5].sCycleT = cpuRegs.cycle;
			counters[5].sCycle = cpuRegs.cycle;
			counters[5].CycleT  = counters[5].rate-PS2VBLANKEND;
			counters[5].Cycle  = counters[5].rate;
			counters[5].count = 0;
			VSync();
		}
	rcntSet();
}

void rcntWcount(int index, u32 value) {
	//SysPrintf ("writeCcount[%d] = %x\n", index, value);
#ifdef EECNT_LOG
	EECNT_LOG("EE count write %d count %x with %x target %x eecycle %x\n", index, counters[index].count, value, counters[index].target, cpuRegs.eCycle);
#endif
	//if((u16)value < counters[index].target)
	//eecntmask &= ~(1 << index);
	counters[index].count = value & 0xffff;
	rcntUpd(index);
	rcntSet();
}

void rcntWmode(int index, u32 value)  
{


	if (value & 0xc00) { //Clear status flags, the ps2 only clears what is given in the value
		eecntmask &= ~(1 << index);
		counters[index].mode &= ~((value & 0xc00));
	}

		
	//if((value & 0x3ff) != (counters[index].mode & 0x3ff))eecntmask &= ~(1 << index);
	counters[index].mode = (counters[index].mode & 0xc00) | (value & 0x3ff);

#ifdef EECNT_LOG
	EECNT_LOG("EE counter set %d mode %x\n", index, counters[index].mode);
#endif

	switch (value & 0x3) {                        //Clock rate divisers *2, they use BUSCLK speed not PS2CLK
		case 0: counters[index].rate = 2; break;
		case 1: counters[index].rate = 32; break;
		case 2: counters[index].rate = 512; break;
		case 3: counters[index].rate = PS2HBLANK; break;
	}

	if((counters[index].mode & 0xF) == 0x7) {
			gates &= ~(1<<index);
			counters[index].mode &= ~0x80;
	}else if(counters[index].mode & 0x4){
		    SysPrintf("Gate enable on counter %x mode %x\n", index, counters[index].mode);
			gates |= 1<<index;
			counters[index].mode &= ~0x80;
			rcntReset(index);
	}
	else gates &= ~(1<<index);
	//counters[index].count = 0;
	rcntSet();

}

void rcntStartGate(int mode){
	int i;
	
	for(i=0; i <=3; i++){  //Gates for counters
		if(!(gates & (1<<i))) continue;
		if ((counters[i].mode & 0x8) != mode) continue;
		//SysPrintf("Gate %d mode %d Start\n", i, (counters[i].mode & 0x30) >> 4);
		switch((counters[i].mode & 0x30) >> 4){
			case 0x0: //Count When Signal is low (off)
				counters[i].count = rcntRcount(i);
				rcntUpd(i);
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
		//SysPrintf("Gate %d mode %d End\n", i, (counters[i].mode & 0x30) >> 4);
		switch((counters[i].mode & 0x30) >> 4){
			case 0x0: //Count When Signal is low (off)
				rcntUpd(i);
				counters[i].mode |= 0x80;
				break;
			case 0x1: //Reset and start counting on Vsync start
				//Do Nothing
				break;
			case 0x2: //Reset and start counting on Vsync end
				counters[i].mode |= 0x80;
				rcntReset(i);
				break;
			case 0x3: //Reset and start  counting on Vsync start and end
				counters[i].mode |= 0x80;
				rcntReset(i);				
				break;
			default:
				SysPrintf("EE Start Counter %x Gate error\n", i);
				break;
		}
	}
}
void rcntWtarget(int index, u32 value) {

	eecntmask &= ~(1 << index);
	counters[index].target = value & 0xffff;
	
#ifdef EECNT_LOG
	EECNT_LOG("EE target write %d target %x value %x\n", index, counters[index].target, value);
#endif
	rcntSet();
}

void rcntWhold(int index, u32 value) {
#ifdef EECNT_LOG
	EECNT_LOG("EE hold write %d value %x\n", index, value);
#endif
	counters[index].hold = value;
}

u16 rcntRcount(int index) {
	u16 ret;

	if ((counters[index].mode & 0x80)) {
		ret = counters[index].count + (int)((cpuRegs.cycle - counters[index].sCycleT) / counters[index].rate);
	}else{
		ret = counters[index].count;
	}

	return (u16)ret;
}

u32 rcntCycle(int index) {

	if ((counters[index].mode & 0x80)) {
		return (u32)counters[index].count + (int)((cpuRegs.cycle - counters[index].sCycleT) / counters[index].rate);
	}else{
		return (u32)counters[index].count;
	}
}

int rcntFreeze(gzFile f, int Mode) {
	gzfreezel(counters);
	gzfreeze(&nextCounter, sizeof(nextCounter));
	gzfreeze(&nextsCounter, sizeof(nextsCounter));

	return 0;
}

