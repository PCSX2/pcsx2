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

#include <string.h>
#include <time.h>
#include <math.h>
#include "Common.h"
#include "PsxCommon.h"
#include "GS.h"
#include "VU.h"

u64 profile_starttick = 0;
u64 profile_totalticks = 0;

int gates = 0;
extern u8 psxhblankgate;
Counter counters[6];
u32 nextCounter, nextsCounter;
static void (*s_prevExecuteVU1Block)() = NULL;
LARGE_INTEGER lfreq;

void rcntUpdTarget(int index) {
	counters[index].sCycleT = cpuRegs.cycle;
}

void rcntUpd(int index) {
	counters[index].sCycle = cpuRegs.cycle;
	rcntUpdTarget(index);
}

void rcntReset(int index) {
	counters[index].count = 0;
	rcntUpd(index);
}

// Updates the state of the nextCounter value (if needed) to serve
// any pending events for the given counter.
// Call this method after any modifications to the state of a counter.
static __forceinline void _rcntSet( int i )
{
	u32 c;
	if (!(counters[i].mode & 0x80) || (counters[i].mode & 0x3) == 0x3) return; // Stopped
	
	// nextCounter is relative to the cpuRegs.cycle when rcntUpdate() was last called.
	// However, the current _rcntSet could be called at any cycle count, so we need to take
	// that into account.  Adding the difference from that cycle count to the current one
	// will do the trick!

	c = ((0x10000 - counters[i].count) * counters[i].rate) - (cpuRegs.cycle - counters[i].sCycleT);
	c += cpuRegs.cycle - nextsCounter;		// adjust for time passed since last rcntUpdate();
	if (c < nextCounter) nextCounter = c;

	//if(!(counters[i].mode & 0x100) || counters[i].target > 0xffff) continue;

	c = ((counters[i].target - counters[i].count) * counters[i].rate) - (cpuRegs.cycle - counters[i].sCycleT);
	c += cpuRegs.cycle - nextsCounter;		// adjust for time passed since last rcntUpdate();
	if (c < nextCounter) nextCounter = c;
}

static __forceinline void cpuRcntSet() {
	int i;

	// Calculate our target cycle deltas.
	// This must be done regardless of if the hblank/vblank counters updated since
	// cpuRegs.cycle changes, even if sCycle hasn't!

	u32 counter4CycleT = ( counters[4].mode == MODE_HBLANK ) ? HBLANK_TIME_ : HRENDER_TIME_;
	u32 counter5CycleT = VSYNC_HALF_ - (cpuRegs.cycle - counters[5].sCycle);
	counter4CycleT -= (cpuRegs.cycle - counters[4].sCycle);

	nextCounter = (counter4CycleT < counter5CycleT) ? counter4CycleT : counter5CycleT;

	nextsCounter = cpuRegs.cycle;

	for (i = 0; i < 4; i++)
		_rcntSet( i );
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

	UpdateVSyncRate();

#ifdef _WIN32
    QueryPerformanceFrequency(&lfreq);
#endif

	for (i=0; i<4; i++) rcntUpd(i);
	cpuRcntSet();

	assert(Cpu != NULL && Cpu->ExecuteVU1Block != NULL );
	s_prevExecuteVU1Block = Cpu->ExecuteVU1Block;
}

// debug code, used for stats
int g_nCounters[4];
static int iFrame = 0;	

#ifndef _WIN32
#include <sys/time.h>
#endif

u64 iTicks=0;

u64 GetTickFrequency()
{
#ifdef _WIN32
	return lfreq.QuadPart;
#else
    return 1000000;
#endif
}

u64 GetCPUTicks()
{
#ifdef _WIN32
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return count.QuadPart;
#else
    struct timeval t;
    gettimeofday(&t, NULL);
    return (u64)t.tv_sec*1000000+t.tv_usec;
#endif
}

void UpdateVSyncRate() {

	counters[4].mode = MODE_HRENDER; // Counter 4 takes care of scanlines, so set the mode to HRENDER (drawing part of scanline)
	counters[4].sCycle = cpuRegs.cycle; // Update Counter 4's Start Cycle to match CPU's cycle
	counters[4].CycleT = HRENDER_TIME_; // Amount of cycles before the counter will be updated

	counters[5].mode = MODE_VRENDER; // Counter 5 takes care of vSync/vBlanks
	counters[5].sCycle = cpuRegs.cycle; // Update Counter 5's Start Cycle to match CPU's cycle
	counters[5].CycleT = VSYNC_HALF_; // Amount of cycles before the counter will be updated


	if (Config.CustomFps > 0) {
		iTicks = GetTickFrequency() / Config.CustomFps;
		SysPrintf("Framelimiter rate updated (UpdateVSyncRate): %d fps\n", Config.CustomFps);
	}
	else if (Config.PsxType & 1) {
		iTicks = (GetTickFrequency() * 100) / 5000;
		SysPrintf("Framelimiter rate updated (UpdateVSyncRate): 50 fps Pal\n");
	}
	else {
		iTicks = (GetTickFrequency() / 5994) * 100;
		SysPrintf("Framelimiter rate updated (UpdateVSyncRate): 59.94 fps NTSC\n");
	}
	cpuRcntSet();
}

void FrameLimiter()
{
	static u64 iStart=0, iEnd=0, iExpectedEnd=0;

	if (iStart==0) iStart = GetCPUTicks();

	iExpectedEnd = iStart + iTicks;
	iEnd = GetCPUTicks();

	if (iEnd>=iExpectedEnd) {

		// Compensation: If the framelate drops too low, reset the 
		// expected value.  This avoids "fast forward" syndrome.
		u64 diff = iEnd-iExpectedEnd;
		if ((diff>>3)>iTicks) iExpectedEnd=iEnd;
	}
	else do {
		_TIMESLICE();
		iEnd = GetCPUTicks();
	} while (iEnd<iExpectedEnd);

	// remember the expected value frame. improves smoothness by encouraging
	// the framelimiter to play a little "catch up" after a slow frame.
	iStart = iExpectedEnd; 
}

extern u32 CSRw;
extern u64 SuperVUGetRecTimes(int clear);
extern u32 vu0time;

extern void DummyExecuteVU1Block(void);

//static u32 lastWasSkip=0;
//extern u32 unpacktotal;

void vSyncDebugStuff() {
#ifdef EE_PROFILING
		if( (iFrame%20) == 0 ) {
			SysPrintf("Profiled Cycles at %d frames %d\n", iFrame, profile_totalticks);
			CLEAR_EE_PROFILE();
        }
#endif

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
}

static __forceinline void frameLimit() 
{
	switch(CHECK_FRAMELIMIT) {
		case PCSX2_FRAMELIMIT_LIMIT:
			FrameLimiter();
			break;

		case PCSX2_FRAMELIMIT_SKIP:
		case PCSX2_FRAMELIMIT_VUSKIP: //Skips a sequence of consecutive frames after a sequence of rendered frames
		{
			// This is the least number of consecutive frames we will render w/o skipping
			#define noSkipFrames (Config.CustomConsecutiveFrames>0) ? Config.CustomConsecutiveFrames : 2
			// This is the number of consecutive frames we will skip				
			#define yesSkipFrames (Config.CustomConsecutiveSkip>0) ? Config.CustomConsecutiveSkip : 2
			static u8 bOkayToSkip = 0;
			static u8 bKeepSkipping = 0;
			static u64 uLastTime = 0;

			// This is some Extra Time to add to our Expected Time to compensate for lack of precision.
			#define extraTimeBuffer 0
			// If uDeltaTime is less than this value, then we can frameskip. (45 & 54 FPS is 90% of fullspeed for Pal & NTSC respectively, the default is to only skip when slower than 90%)
			u64 uExpectedTime = (Config.CustomFrameSkip>0) ? (GetTickFrequency()/Config.CustomFrameSkip + extraTimeBuffer) : ((Config.PsxType&1) ? (GetTickFrequency()/45 + extraTimeBuffer) : (GetTickFrequency()/54 + extraTimeBuffer));
			// This is used for the framelimiter; The user can set a custom FPS limit, if none is specified, used default FPS limit (50fps or 60fps).
			//u64 uLimiterExpectedTime = (Config.CustomFps>0) ? (GetTickFrequency()/Config.CustomFps + extraTimeBuffer) : ((Config.PsxType&1) ? (GetTickFrequency()/50 + extraTimeBuffer) : (GetTickFrequency()/60 + extraTimeBuffer));
			u64 uCurTime = GetCPUTicks();
			u64 uDeltaTime = uCurTime - uLastTime;

			// Don't skip the Very First Frame PCSX2 renders. (This line might not be needed, but was included incase it breaks something.)
			if (uDeltaTime == uCurTime) uDeltaTime = 0;

			if (bOkayToSkip == 0) // If we're done rendering our consecutive frames, its okay to skip.
			{
				if (uDeltaTime > uExpectedTime) // Only skip if running slow.
				{
					//first freeze GS regs THEN send dummy packet
					if( CHECK_MULTIGS ) GSRingBufSimplePacket(GS_RINGTYPE_FRAMESKIP, 1, 0, 0);
					else GSsetFrameSkip(1);
					if( CHECK_FRAMELIMIT == PCSX2_FRAMELIMIT_VUSKIP )
						Cpu->ExecuteVU1Block = DummyExecuteVU1Block;
					bOkayToSkip = noSkipFrames;
					bKeepSkipping = yesSkipFrames;
				}
			}
			else if (bOkayToSkip == noSkipFrames) // If we skipped last frame, unfreeze the GS regs
			{
				if (bKeepSkipping <= 1) {
					//first set VU1 to enabled THEN unfreeze GS regs
					if( CHECK_FRAMELIMIT == PCSX2_FRAMELIMIT_VUSKIP ) 
						Cpu->ExecuteVU1Block = s_prevExecuteVU1Block;
					if( CHECK_MULTIGS ) GSRingBufSimplePacket(GS_RINGTYPE_FRAMESKIP, 0, 0, 0);
					else GSsetFrameSkip(0);
					bOkayToSkip--;
				}
				else {bKeepSkipping--;}
			}
			else {bOkayToSkip--;}

			//Frame Limit so we don't go over the FPS limit
			FrameLimiter();

			uLastTime = GetCPUTicks();

			break;
		}
	}
}

static __forceinline void VSyncStart(u32 sCycle) // VSync Start 
{
	vSyncDebugStuff(); // EE Profiling and Debug code
	if ((CSRw & 0x8)) GSCSRr|= 0x8;
	if (!(GSIMR&0x800)) gsIrq(); //GS Irq

	hwIntcIrq(2); // HW Irq
	psxVBlankStart(); // psxCounters vBlank Start

	if (gates) rcntStartGate(0x8, sCycle); // Counters Start Gate code
	if (Config.Patch) applypatch(1); // Apply patches (ToDo: clean up patch code)
}

static __forceinline void VSyncEnd(u32 sCycle) // VSync End
{
	iFrame++;
	*(u32*)(PS2MEM_GS+0x1000) ^= 0x2000; // swap the vsync field

	// wait until GS stops
	if( CHECK_MULTIGS ) GSRingBufSimplePacket(GS_RINGTYPE_VSYNC, (*(u32*)(PS2MEM_GS+0x1000)&0x2000), 0, 0);
	else {
		GSvsync((*(u32*)(PS2MEM_GS+0x1000)&0x2000));
        // update here on single thread mode *OBSOLETE*
        if( PAD1update != NULL ) PAD1update(0);
        if( PAD2update != NULL ) PAD2update(1);
	}

	hwIntcIrq(3);  // HW Irq
	psxVBlankEnd(); // psxCounters vBlank End
	if (gates) rcntEndGate(0x8, sCycle); // Counters End Gate Code
	SysUpdate();  // check for and handle keyevents
	frameLimit(); // limit FPS (also handles frameskip and VUskip)
}

static __forceinline void hScanlineNextCycle( u32 diff, u32 cyclesAmount )
{
	// This function: Now Unneeded?
	// This code doesn't appear to be run anymore after fixing the CycleT bug
	// and fixing the EE/IOP code execution sync issues (tested on 6 games,
	// with EEx3 hack too).

	// And it makes sense -- bad behavior by the counters would have led
	// to cpuBranchTest being delayed beyond the span of a full hsync.
	// It could still happen in some isolated part of some particular game,
	// but probably we're better off letting that game lose a couple hsyncs
	// once in a while rather than slow everyone else down needlessly.

	u32 scanlineCycles = SCANLINE_;
	diff -= cyclesAmount;
	if (diff >= scanlineCycles)
	{
		u32 increment = diff / scanlineCycles;

		// Counter Optimization:
		// If the time passed is beyond a single scanline, then increment all scanline
		// counters as a set here.

		SysPrintf("Counters Optimization %d\n", diff / scanlineCycles);

		/* if counter's count increases on hblank gate's off signal OR if counter increases every hblank, THEN add to the counter */
		if ( (!(counters[0].mode & 0x30) && (gates & (1<<0))) || (((counters[0].mode & 0x83) == 0x83) && !(gates & (1<<0))) ) counters[0].count += (increment * HBLANK_COUNTER_SPEED);
		if ( (!(counters[1].mode & 0x30) && (gates & (1<<1))) || (((counters[1].mode & 0x83) == 0x83) && !(gates & (1<<1))) ) counters[1].count += (increment * HBLANK_COUNTER_SPEED);
		if ( (!(counters[2].mode & 0x30) && (gates & (1<<2))) || (((counters[2].mode & 0x83) == 0x83) && !(gates & (1<<2))) ) counters[2].count += (increment * HBLANK_COUNTER_SPEED);
		if ( (!(counters[3].mode & 0x30) && (gates & (1<<3))) || (((counters[3].mode & 0x83) == 0x83) && !(gates & (1<<3))) ) counters[3].count += (increment * HBLANK_COUNTER_SPEED);
		counters[4].sCycle += (increment * scanlineCycles);
	}
}

static __forceinline void hScanline() 
{
	u32 difference = (cpuRegs.cycle - counters[4].sCycle);

	if (counters[4].mode & MODE_HBLANK) { //HBLANK Start
		const u32 modeCycles = HBLANK_TIME_;
		if (difference >= modeCycles ) {
			//hScanlineNextCycle(difference, modeCycles);
			counters[4].sCycle += modeCycles;
			rcntStartGate(0, counters[4].sCycle);
			psxCheckStartGate16(0);
			counters[4].mode = MODE_HRENDER;
		}
	}
	else { //HBLANK END / HRENDER Begin
		const u32 modeCycles = HRENDER_TIME_;
		if (difference >= modeCycles) {
			//hScanlineNextCycle(difference, modeCycles);
			counters[4].sCycle += modeCycles;
			if (CSRw & 0x4) GSCSRr |= 4; // signal
			if (!(GSIMR&0x400)) gsIrq();
			if (gates) rcntEndGate(0, counters[4].sCycle);
			if (psxhblankgate) psxCheckEndGate16(0);
			counters[4].mode = MODE_HBLANK;
		}
	}

	/*if(counters[4].CycleT < 0) {
		counters[4].sCycle += -counters[4].CycleT;
		counters[4].CycleT = 0;
	}*/

}

// Only called from one place so might as well inline it.
static __forceinline void vSync()
{
	u32 diff = (cpuRegs.cycle - counters[5].sCycle);

	hScanline();

	if (diff >= (VSYNC_HALF_)) {
		counters[5].sCycle += VSYNC_HALF_; // * (u32)(diff / VSYNC_HALF_));

		if (counters[5].mode == MODE_VSYNC) {
			VSyncEnd(counters[5].sCycle);
			counters[5].mode = MODE_VRENDER;
		}
		else {
			VSyncStart(counters[5].sCycle);
			counters[5].mode = MODE_VSYNC;

			// Accumulate hsync rounding errors:
			counters[4].sCycle += HSYNC_ERROR;

			// Tighten up EE/IOP responsiveness for a wee bit.
			// Games are usually most sensitive to vSync sessions since that's
			// when the hard thinking usually occurs.
			g_eeTightenSync += 2;
		}
		g_nextBranchCycle = cpuRegs.cycle + 384;
	}
}

// forceinline note: this method is called from two locations, but one
// of them is the interpreter, which doesn't count. ;)  So might as
// well forceinline it!
__forceinline void rcntUpdate()
{
	int i;

	vSync(); //hBlank and vSync Timers

	for (i=0; i<=3; i++) {
		if ( gates & (1<<i) ) continue;
		if ((counters[i].mode & 0x80) && (counters[i].mode & 0x3) != 0x3) {
			//counters[i].count += (cpuRegs.cycle - counters[i].sCycleT) / counters[i].rate;
			//counters[i].sCycleT = cpuRegs.cycle;
			
			u32 change = cpuRegs.cycle - counters[i].sCycleT;
			counters[i].count += (int)(change / counters[i].rate);
			change -= (change / counters[i].rate) * counters[i].rate;
			counters[i].sCycleT = cpuRegs.cycle - change;
		} 
		else counters[i].sCycleT = cpuRegs.cycle;
	}

	for (i=0; i<=3; i++) {
		if (!(counters[i].mode & 0x80)) continue; // Stopped

		if (counters[i].count >= counters[i].target) { // Target interrupt
				
			counters[i].target &= 0xffff;

			if(counters[i].mode & 0x100) {

				EECNT_LOG("EE counter %d target reached mode %x count %x target %x\n", i, counters[i].mode, counters[i].count, counters[i].target);
				counters[i].mode|= 0x0400; // Equal Target flag
				hwIntcIrq(counters[i].interrupt);

				if (counters[i].mode & 0x40) { //The PS2 only resets if the interrupt is enabled - Tested on PS2
					counters[i].count -= counters[i].target; // Reset on target
				}
				else counters[i].target |= 0x10000000;
			} 
			else counters[i].target |= 0x10000000;
		}

		if (counters[i].count > 0xffff) {
		
			if (counters[i].mode & 0x0200) { // Overflow interrupt
				EECNT_LOG("EE counter %d overflow mode %x count %x target %x\n", i, counters[i].mode, counters[i].count, counters[i].target);
				counters[i].mode |= 0x0800; // Overflow flag
				hwIntcIrq(counters[i].interrupt);
			}
			counters[i].count -= 0x10000;
			counters[i].target &= 0xffff;
		}
	}

	cpuRcntSet();
}

void rcntWcount(int index, u32 value) 
{
	EECNT_LOG("EE count write %d count %x with %x target %x eecycle %x\n", index, counters[index].count, value, counters[index].target, cpuRegs.eCycle);
	counters[index].count = value & 0xffff;
	counters[index].target &= 0xffff;	
	//rcntUpd(index);

	if((counters[index].mode & 0x3) != 0x3) {
		//counters[index].sCycleT = cpuRegs.cycle;
		
		u32 change = cpuRegs.cycle - counters[index].sCycleT;
		change -= (change / counters[index].rate) * counters[index].rate;
		counters[index].sCycleT = cpuRegs.cycle - change;
	}

	_rcntSet( index );
}

void rcntWmode(int index, u32 value)  
{
	if(counters[index].mode & 0x80) {
		if((counters[index].mode & 0x3) != 0x3) {
			//counters[index].count += (cpuRegs.cycle - counters[index].sCycleT) / counters[index].rate;
			//counters[index].sCycleT = cpuRegs.cycle;
			
			u32 change = cpuRegs.cycle - counters[index].sCycleT;
			counters[index].count += (int)(change / counters[index].rate);
			change -= (change / counters[index].rate) * counters[index].rate;
			counters[index].sCycleT = cpuRegs.cycle - change;
		}
	}
	else counters[index].sCycleT = cpuRegs.cycle;

	counters[index].mode &= ~(value & 0xc00); //Clear status flags, the ps2 only clears what is given in the value
	counters[index].mode = (counters[index].mode & 0xc00) | (value & 0x3ff);
	EECNT_LOG("EE counter set %d mode %x count %x\n", index, counters[index].mode, rcntCycle(index));

	switch (value & 0x3) { //Clock rate divisers *2, they use BUSCLK speed not PS2CLK
		case 0: counters[index].rate = 2; break;
		case 1: counters[index].rate = 32; break;
		case 2: counters[index].rate = 512; break;
		case 3: counters[index].rate = SCANLINE_; break;
	}
	
	if((counters[index].mode & 0xF) == 0x7) {
		gates &= ~(1<<index);
		SysPrintf("Counters: Gate Disabled\n");
		//counters[index].mode &= ~0x80;
	}
	else if (counters[index].mode & 0x4) {
		gates |= (1<<index);
		counters[index].mode &= ~0x80;
		rcntReset(index);
	}
	else gates &= ~(1<<index);
	
	/*if ((value & 0x580) == 0x580) { // If we need to compare the target value again, correct the target
		//SysPrintf("EE Correcting target %x after mode write\n", index);
		counters[index].target &= 0xffff;
	}*/

	_rcntSet( index );
}

void rcntStartGate(unsigned int mode, u32 sCycle) {
	int i;

	for (i=0; i <=3; i++) { //Gates for counters

		if ((mode == 0) && ((counters[i].mode & 0x83) == 0x83)) counters[i].count += HBLANK_COUNTER_SPEED; //Update counters using the hblank as the clock
		if (!(gates & (1<<i))) continue;
		if ((counters[i].mode & 0x8) != mode) continue;

		switch (counters[i].mode & 0x30) {
			case 0x00: //Count When Signal is low (off)
				counters[i].mode |= 0x80;
				counters[i].count += (int)((cpuRegs.cycle - counters[i].sCycleT) / counters[i].rate);
				counters[i].sCycle = sCycle;
				counters[i].sCycleT = sCycle;
				break;
			case 0x20:
				counters[i].count = rcntRcount(i);
				break;
			case 0x10: //Reset and start counting on Vsync start
			case 0x30: //Reset and start counting on Vsync start and end
				counters[i].mode |= 0x80;
				counters[i].count = 0;
				counters[i].sCycle = sCycle;
				counters[i].sCycleT = sCycle;
				counters[i].target &= 0xffff;
				break;
		}
	}
	// Note: No need to set counters here.
	// They'll get set later on in rcntUpdate, since we're
	// being called from there anyway.
}

void rcntEndGate(unsigned int mode, u32 sCycle) {
	int i;

	for(i=0; i <=3; i++) { //Gates for counters
		if (!(gates & (1<<i))) continue;
		if ((counters[i].mode & 0x8) != mode) continue;

		switch (counters[i].mode & 0x30) {
			case 0x00: //Count When Signal is low (off)
				counters[i].mode &= ~0x80;
				counters[i].sCycle = sCycle;
				counters[i].sCycleT = sCycle;
				break;
			case 0x10:
				counters[i].count = rcntRcount(i);
				break;	// skip the _rcntSet
			case 0x20: //Reset and start counting on Vsync end
			case 0x30: //Reset and start counting on Vsync start and end
				counters[i].mode |= 0x80;
				counters[i].count = 0;
				counters[i].sCycle = sCycle;
				counters[i].sCycleT = sCycle;
				counters[i].target &= 0xffff;
				break;
		}
	}
	// Note: No need to set counters here.
	// They'll get set later on in rcntUpdate, since we're
	// being called from there anyway.
}

void rcntWtarget(int index, u32 value) {

	EECNT_LOG("EE target write %d target %x value %x\n", index, counters[index].target, value);
	counters[index].target = value & 0xffff;

	if (counters[index].target <= rcntCycle(index)/* && counters[index].target != 0*/) {
		//SysPrintf("EE Saving target %d from early trigger, target = %x, count = %x\n", index, counters[index].target, rcntCycle(index));
		counters[index].target |= 0x10000000;
	}
	_rcntSet( index );
}

void rcntWhold(int index, u32 value) {
	EECNT_LOG("EE hold write %d value %x\n", index, value);
	counters[index].hold = value;
}

u32 rcntRcount(int index) {
	u32 ret;

	if ((counters[index].mode & 0x80)) 
		ret = counters[index].count + (int)((cpuRegs.cycle - counters[index].sCycleT) / counters[index].rate);
	else 
		ret = counters[index].count;

	EECNT_LOG("EE count read %d value %x\n", index, ret);
	return ret;
}

u32 rcntCycle(int index) {

	if ((counters[index].mode & 0x80)) 
		return (u32)counters[index].count + (int)((cpuRegs.cycle - counters[index].sCycleT) / counters[index].rate);
	else 
		return (u32)counters[index].count;
}

int rcntFreeze(gzFile f, int Mode) {
	gzfreezel(counters);
	gzfreeze(&nextCounter, sizeof(nextCounter));
	gzfreeze(&nextsCounter, sizeof(nextsCounter));

	if( Mode == 0 )
	{
		// Sanity check for loading older savestates:
		if( counters[4].sCycle == 0 )
			counters[4].sCycle = cpuRegs.cycle;

		if( counters[5].sCycle == 0 )
			counters[5].sCycle = cpuRegs.cycle;
	}

	return 0;
}

