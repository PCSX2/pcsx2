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

// Counter 4 takes care of scanlines - hSync/hBlanks
// Counter 5 takes care of vSync/vBlanks
Counter counters[6];

u32 nextsCounter;	// records the cpuRegs.cycle value of the last call to rcntUpdate()
s32 nextCounter;	// delta from nextsCounter, in cycles, until the next rcntUpdate() 

static void (*s_prevExecuteVU1Block)() = NULL;
LARGE_INTEGER lfreq;

void rcntReset(int index) {
	counters[index].count = 0;
	counters[index].sCycleT = cpuRegs.cycle;
}

// Updates the state of the nextCounter value (if needed) to serve
// any pending events for the given counter.
// Call this method after any modifications to the state of a counter.
static __forceinline void _rcntSet( int i )
{
	s32 c;
	assert( i <= 4 );		// rcntSet isn't valid for h/vsync counters.
		
	// Stopped or special hsync gate?
	if (!(counters[i].mode & 0x80) || (counters[i].mode & 0x3) == 0x3) return;
	
	// nextCounter is relative to the cpuRegs.cycle when rcntUpdate() was last called.
	// However, the current _rcntSet could be called at any cycle count, so we need to take
	// that into account.  Adding the difference from that cycle count to the current one
	// will do the trick!

	c = ((0x10000 - counters[i].count) * counters[i].rate) - (cpuRegs.cycle - counters[i].sCycleT);
	c += cpuRegs.cycle - nextsCounter;		// adjust for time passed since last rcntUpdate();
	if (c < nextCounter) nextCounter = c;

	// Ignore target diff if target is currently disabled.
	// (the overflow is all we care about since it goes first, and then the 
	// target will be turned on afterward).

	if( counters[i].target & 0x10000000 ) return;
	c = ((counters[i].target - counters[i].count) * counters[i].rate) - (cpuRegs.cycle - counters[i].sCycleT);
	c += cpuRegs.cycle - nextsCounter;		// adjust for time passed since last rcntUpdate();
	if (c < nextCounter) nextCounter = c;
}


static __forceinline void cpuRcntSet()
{
	int i;

	nextsCounter = cpuRegs.cycle;
	nextCounter = (counters[5].sCycle + counters[5].CycleT) - cpuRegs.cycle;

	// if we're running behind, the diff will be negative.
	// (and running behind means we need to branch again ASAP)
	if( nextCounter <= 0 )
	{
		nextCounter = 0;
		return;
	}

	for (i = 0; i < 4; i++)
		_rcntSet( i );

	// sanity check!
	if( nextCounter < 0 ) nextCounter = 0;
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

	counters[4].mode = MODE_HRENDER;
	counters[4].sCycle = cpuRegs.cycle;
	counters[5].mode = MODE_VRENDER; 
	counters[5].sCycle = cpuRegs.cycle;

	UpdateVSyncRate();

#ifdef _WIN32
    QueryPerformanceFrequency(&lfreq);
#endif

	for (i=0; i<4; i++) rcntReset(i);
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

typedef struct
{
	u32 Framerate;			// frames per second * 100 (so 2500 for PAL and 2997 for NTSC)
	u32 Render;				// time from vblank end to vblank start (cycles)
	u32 Blank;				// time from vblank start to vblank end (cycles)

	u32 hSyncError;			// rounding error after the duration of a rendered frame (cycles)
	u32 hRender;			// time from hblank end to hblank start (cycles)
	u32 hBlank;				// time from hblank start to hblank end (cycles)
	u32 hScanlinesPerFrame;	// number of scanlines per frame (525/625 for NTSC/PAL)
} vSyncTimingInfo;


static vSyncTimingInfo vSyncInfo;


static __forceinline void vSyncInfoCalc( vSyncTimingInfo* info, u32 framesPerSecond, u32 scansPerFrame )
{
	// Important: Cannot use floats or doubles here.  The emulator changes rounding modes
	// depending on user-set speedhack options, and it can break float/double code
	// (as in returning infinities and junk)

	u64 Frame = ((u64)PS2CLK * 1000000ULL) / framesPerSecond;
	u64 HalfFrame = Frame / 2;
	u64 Blank = HalfFrame / 4;		// two blanks and renders per frame
	u64 Render = HalfFrame - Blank;	// so use the half-frame value for these...

	// Important!  The hRender/hBlank timers should be 50/50 for best results.
	// In theory a 70%/30% ratio would be more correct but in practice it runs
	// like crap and totally screws audio synchronization and other things.
	
	u64 Scanline = Frame / scansPerFrame;
	u64 hBlank = Scanline / 2;
	u64 hRender = Scanline - hBlank;
	
	info->Framerate = framesPerSecond;
	info->Render = (u32)(Render/10000);
	info->Blank  = (u32)(Blank/10000);

	info->hRender = (u32)(hRender/10000);
	info->hBlank  = (u32)(hBlank/10000);
	info->hScanlinesPerFrame = scansPerFrame;
	
	// Apply rounding:
	if( ( Render - info->Render ) >= 5000 ) info->Render++;
	else if( ( Blank - info->Blank ) >= 5000 ) info->Blank++;

	if( ( hRender - info->hRender ) >= 5000 ) info->hRender++;
	else if( ( hBlank - info->hBlank ) >= 5000 ) info->hBlank++;
	
	// Calculate accumulative hSync rounding error per half-frame:
	{
	u32 hSyncCycles = ((info->hRender + info->hBlank) * scansPerFrame) / 2;
	u32 vSyncCycles = (info->Render + info->Blank);
	info->hSyncError = vSyncCycles - hSyncCycles;
	}

	// Note: In NTSC modes there is some small rounding error in the vsync too,
	// however it would take thousands of frames for it to amount to anything and
	// is thus not worth the effort at this time.
}


void UpdateVSyncRate()
{
	const char *limiterMsg = "Framelimiter rate updated (UpdateVSyncRate): %s fps\n";

	// fixme - According to some docs, progressive-scan modes actually refresh slower than
	// interlaced modes.  But I can't fathom how, since the refresh rate is a function of
	// the television and all the docs I found on TVS made no indication that they ever
	// run anything except their native refresh rate.

	//#define VBLANK_NTSC			((Config.PsxType & 2) ? 59.94 : 59.82) //59.94 is more precise
	//#define VBLANK_PAL			((Config.PsxType & 2) ? 50.00 : 49.76)

	if(Config.PsxType & 1)
	{
		if( vSyncInfo.Framerate != FRAMERATE_PAL )
		{
			SysPrintf( "PCSX2: Switching to PAL display timings.\n" );
			vSyncInfoCalc( &vSyncInfo, FRAMERATE_PAL, SCANLINES_TOTAL_PAL );
		}
	}
	else
	{
		if( vSyncInfo.Framerate != FRAMERATE_NTSC )
		{
			SysPrintf( "PCSX2: Switching to NTSC display timings.\n" );
			vSyncInfoCalc( &vSyncInfo, FRAMERATE_NTSC, SCANLINES_TOTAL_NTSC );
		}
	}

	counters[4].CycleT = vSyncInfo.hRender; // Amount of cycles before the counter will be updated
	counters[5].CycleT = vSyncInfo.Render; // Amount of cycles before the counter will be updated

	if (Config.CustomFps > 0)
	{
		u32 ticks = (u32)(GetTickFrequency() / Config.CustomFps);
		if( iTicks != ticks )
		{
			iTicks = ticks;
			SysPrintf( limiterMsg, Config.CustomFps );
		}
	}
	else
	{
		u32 ticks = (u32)((GetTickFrequency() * 50) / vSyncInfo.Framerate);
		if( iTicks != ticks )
		{
			iTicks = ticks;
			SysPrintf( limiterMsg, (Config.PsxType & 1) ? "50" : "59.94" );
		}
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
		// expected value.  This avoids excessive amounts of
		// "fast forward" syndrome which would occur if we tried to
		// catch up too much.
		
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

extern void GSPostVsyncEnd();

static __forceinline void VSyncEnd(u32 sCycle) // VSync End
{
	iFrame++;

	GSPostVsyncEnd();

	hwIntcIrq(3);  // HW Irq
	psxVBlankEnd(); // psxCounters vBlank End
	if (gates) rcntEndGate(0x8, sCycle); // Counters End Gate Code
	SysUpdate();  // check for and handle keyevents
	frameLimit(); // limit FPS (also handles frameskip and VUskip)
}

#ifndef PCSX2_PUBLIC
static u32 hsc=0;
static int vblankinc = 0;
#endif

__forceinline void rcntUpdate_hScanline()
{
	if( !cpuTestCycle( counters[4].sCycle, counters[4].CycleT ) ) return;

	iopBranchAction = 1;
	if (counters[4].mode & MODE_HBLANK) { //HBLANK Start
		//hScanlineNextCycle(difference, modeCycles);
		rcntStartGate(0, counters[4].sCycle);
		psxCheckStartGate16(0);
		
		// Setup the hRender's start and end cycle information:
		counters[4].sCycle += vSyncInfo.hBlank;		// start  (absolute cycle value)
		counters[4].CycleT = vSyncInfo.hRender;		// endpoint (delta from start value)
		counters[4].mode = MODE_HRENDER;
	}
	else { //HBLANK END / HRENDER Begin
		//hScanlineNextCycle(difference, modeCycles);
		if (CSRw & 0x4) GSCSRr |= 4; // signal
		if (!(GSIMR&0x400)) gsIrq();
		if (gates) rcntEndGate(0, counters[4].sCycle);
		if (psxhblankgate) psxCheckEndGate16(0);

		// set up the hblank's start and end cycle information:
		counters[4].sCycle += vSyncInfo.hRender;	// start (absolute cycle value)
		counters[4].CycleT = vSyncInfo.hBlank;		// endpoint (delta from start value)
		counters[4].mode = MODE_HBLANK;

#		ifndef PCSX2_PUBLIC
		hsc++;
#		endif
	}
}

__forceinline void rcntUpdate_vSync()
{
	s32 diff = (cpuRegs.cycle - counters[5].sCycle);
	if( diff < counters[5].CycleT ) return;

	iopBranchAction = 1;
	if (counters[5].mode == MODE_VSYNC)
	{
		VSyncEnd(counters[5].sCycle);

		counters[5].sCycle += vSyncInfo.Blank;
		counters[5].CycleT = vSyncInfo.Render;
		counters[5].mode = MODE_VRENDER;
	}
	else	// VSYNC end / VRENDER begin
	{
		VSyncStart(counters[5].sCycle);

		counters[5].sCycle += vSyncInfo.Render;
		counters[5].CycleT = vSyncInfo.Blank;
		counters[5].mode = MODE_VSYNC;

		// Accumulate hsync rounding errors:
		counters[4].sCycle += vSyncInfo.hSyncError;

#		ifndef PCSX2_PUBLIC
		vblankinc++;
		if( vblankinc > 1 )
		{
			if( hsc != vSyncInfo.hScanlinesPerFrame )
				SysPrintf( " ** vSync > Abnornal Scanline Count: %d\n", hsc );
			hsc = 0;
			vblankinc = 0;
		}
#		endif

	}
}

static __forceinline void __fastcall _cpuTestTarget( int i )
{
	//counters[i].target &= 0xffff;

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


// forceinline note: this method is called from two locations, but one
// of them is the interpreter, which doesn't count. ;)  So might as
// well forceinline it!
__forceinline void rcntUpdate()
{
	int i;

	rcntUpdate_vSync();

	// Update all counters?
	// This code shouldn't be needed.  Counters are updated as needed when
	// Reads, Writes, and Target/Overflow events occur.  The rest of the
	// time the counters can be left unmodified.

	for (i=0; i<=3; i++) {
		if ( gates & (1<<i) ) continue;
		if ((counters[i].mode & 0x80) && (counters[i].mode & 0x3) != 0x3) {
			
			s32 change = cpuRegs.cycle - counters[i].sCycleT;
			if( change > 0 ) {
				counters[i].count += change / counters[i].rate;
				change -= (change / counters[i].rate) * counters[i].rate;
				counters[i].sCycleT = cpuRegs.cycle - change;
			}
		} 
		else counters[i].sCycleT = cpuRegs.cycle;
	}

	// Check Counter Targets and Overflows:
	
	for (i=0; i<=3; i++)
	{
		if (!(counters[i].mode & 0x80)) continue; // Stopped

		// Target reached?
		if (counters[i].count >= counters[i].target)
			_cpuTestTarget( i );

		if (counters[i].count > 0xffff) {
		
			if (counters[i].mode & 0x0200) { // Overflow interrupt
				EECNT_LOG("EE counter %d overflow mode %x count %x target %x\n", i, counters[i].mode, counters[i].count, counters[i].target);
				counters[i].mode |= 0x0800; // Overflow flag
				hwIntcIrq(counters[i].interrupt);
			}
			counters[i].count -= 0x10000;
			counters[i].target &= 0xffff;

			// Target reached after overflow?
			// It's possible that a Target very near zero (1-10, etc) could have already been reached.
			// Checking for it now 
			//if (counters[i].count >= counters[i].target)
			//	_cpuTestTarget( i );
		}
	}
	cpuRcntSet();
}

void rcntWcount(int index, u32 value) 
{
	EECNT_LOG("EE count write %d count %x with %x target %x eecycle %x\n", index, counters[index].count, value, counters[index].target, cpuRegs.eCycle);
	counters[index].count = value & 0xffff;
	counters[index].target &= 0xffff;	

	if(counters[index].mode & 0x80) {
		if((counters[index].mode & 0x3) != 0x3) {
			s32 change = cpuRegs.cycle - counters[index].sCycleT;
			if( change > 0 ) {
				change -= (change / counters[index].rate) * counters[index].rate;
				counters[index].sCycleT = cpuRegs.cycle - change;
			}
		}
	} 
	else counters[index].sCycleT = cpuRegs.cycle;

	_rcntSet( index );
}

void rcntWmode(int index, u32 value)  
{
	if(counters[index].mode & 0x80) {
		if((counters[index].mode & 0x3) != 0x3) {

			u32 change = cpuRegs.cycle - counters[index].sCycleT;
			if( change > 0 )
			{
				counters[index].count += change / counters[index].rate;
				change -= (change / counters[index].rate) * counters[index].rate;
				counters[index].sCycleT = cpuRegs.cycle - change;
			}
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
		case 3: counters[index].rate = vSyncInfo.hBlank+vSyncInfo.hRender; break;
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
	
	_rcntSet( index );
}

void rcntStartGate(unsigned int mode, u32 sCycle) {
	int i;

	for (i=0; i <=3; i++) { //Gates for counters

		if ((mode == 0) && ((counters[i].mode & 0x83) == 0x83))
			counters[i].count += HBLANK_COUNTER_SPEED; //Update counters using the hblank as the clock
		if (!(gates & (1<<i))) continue;
		if ((counters[i].mode & 0x8) != mode) continue;

		switch (counters[i].mode & 0x30) {
			case 0x00: //Count When Signal is low (off)
			
				// Just set the start cycle (sCycleT) -- counting will be done as needed
				// for events (overflows, targets, mode changes, and the gate off below)
			
				counters[i].mode |= 0x80;
				counters[i].sCycleT = sCycle;
				break;
				
			case 0x20:	// reset and start counting on vsync end
				// this is the vsync start so do nothing.
				break;
				
			case 0x10: //Reset and start counting on Vsync start
			case 0x30: //Reset and start counting on Vsync start and end
				counters[i].mode |= 0x80;
				counters[i].count = 0;
				counters[i].sCycleT = sCycle;
				counters[i].target &= 0xffff;
				break;
		}
	}

	// No need to update actual counts here.  Counts are calculated as needed by reads to
	// rcntRcount().  And so long as sCycleT is set properly, any targets or overflows
	// will be scheduled and handled.

	// Note: No need to set counters here.  They'll get set when control returns to
	// rcntUpdate, since we're being called from there anyway.
}

void rcntEndGate(unsigned int mode, u32 sCycle) {
	int i;

	for(i=0; i <=3; i++) { //Gates for counters
		if (!(gates & (1<<i))) continue;
		if ((counters[i].mode & 0x8) != mode) continue;

		switch (counters[i].mode & 0x30) {
			case 0x00: //Count When Signal is low (off)

				// Set the count here.  Since the timer is being turned off it's
				// important to record its count at this point.		
				counters[i].count = rcntRcount(i);
				counters[i].mode &= ~0x80;
				counters[i].sCycleT = sCycle;

				break;
			case 0x10:	// Reset and start counting on Vsync start
				// this is the vsync end so do nothing
				break;
			case 0x20: //Reset and start counting on Vsync end
			case 0x30: //Reset and start counting on Vsync start and end
				counters[i].mode |= 0x80;
				counters[i].count = 0;
				counters[i].sCycleT = sCycle;
				counters[i].target &= 0xffff;
				break;
		}
	}
	// Note: No need to set counters here.  They'll get set when control returns to
	// rcntUpdate, since we're being called from there anyway.
}

void rcntWtarget(int index, u32 value) {

	EECNT_LOG("EE target write %d target %x value %x\n", index, counters[index].target, value);
	counters[index].target = value & 0xffff;

	// guard against premature (instant) targeting.
	// If the target is behind the current count, set it up so that the counter must
	// overflow first before the target fires:

	if( counters[index].target <= rcntCycle(index) )
		counters[index].target |= 0x10000000;

	_rcntSet( index );
}

void rcntWhold(int index, u32 value) {
	EECNT_LOG("EE hold write %d value %x\n", index, value);
	counters[index].hold = value;
}

u32 rcntRcount(int index) {
	u32 ret;

	// only count if the counter is turned on (0x80) and is not an hsync gate (!0x03)
	if ((counters[index].mode & 0x80) && ((counters[index].mode & 0x3) != 0x3)) 
		ret = counters[index].count + ((cpuRegs.cycle - counters[index].sCycleT) / counters[index].rate);
	else 
		ret = counters[index].count;

	EECNT_LOG("EE count read %d value %x\n", index, ret);
	return ret;
}

u32 rcntCycle(int index) {

	if ((counters[index].mode & 0x80) && ((counters[index].mode & 0x3) != 0x3)) 
		return counters[index].count + ((cpuRegs.cycle - counters[index].sCycleT) / counters[index].rate);
	else 
		return counters[index].count;
}

int rcntFreeze(gzFile f, int Mode) {

	if( Mode == 1 )
	{
		// Temp Hack Fix: Adjust some values so that they'll load properly
		// in the future (this should be removed when a new savestate version
		// is introduced).

		counters[4].sCycle += vSyncInfo.hRender;
		counters[5].sCycle += vSyncInfo.Render;
	}

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

	// Old versions of PCSX2 saved the counters *after* incrementing them.
	// So if we don't roll back here, the counters move past cpuRegs.cycle
	// and everthing explodes!
	// Note: Roll back regardless of load or save, since we roll them forward
	// when saving (above).  It's a hack, but it works.

	counters[4].sCycle -= vSyncInfo.hRender;
	counters[5].sCycle -= vSyncInfo.Render;

	return 0;
}

