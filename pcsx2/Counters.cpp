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

#include "PrecompiledHeader.h"

#include <time.h>
#include <cmath>
#include "Common.h"
#include "PsxCommon.h"
#include "GS.h"
#include "VU.h"

using namespace Threading;

u64 profile_starttick = 0;
u64 profile_totalticks = 0;

int gates = 0;
extern u8 psxhblankgate;

// Counter 4 takes care of scanlines - hSync/hBlanks
// Counter 5 takes care of vSync/vBlanks
Counter counters[6];

u32 nextsCounter;	// records the cpuRegs.cycle value of the last call to rcntUpdate()
s32 nextCounter;	// delta from nextsCounter, in cycles, until the next rcntUpdate() 

// VUSkip Locals and Globals

u32 g_vu1SkipCount;	// number of frames to disable/skip VU1
static void (*s_prevExecuteVU1Block)() = NULL;	// old VU1 block (either Int or Rec)

extern void DummyExecuteVU1Block(void);

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

	for (i=0; i<4; i++) rcntReset(i);
	cpuRcntSet();
}

// debug code, used for stats
int g_nCounters[4];
static int iFrame = 0;	

#ifndef _WIN32
#include <sys/time.h>
#endif

static s64 m_iTicks=0;
static u64 m_iStart=0;

struct vSyncTimingInfo
{
	u32 Framerate;			// frames per second * 100 (so 2500 for PAL and 2997 for NTSC)
	u32 Render;				// time from vblank end to vblank start (cycles)
	u32 Blank;				// time from vblank start to vblank end (cycles)

	u32 hSyncError;			// rounding error after the duration of a rendered frame (cycles)
	u32 hRender;			// time from hblank end to hblank start (cycles)
	u32 hBlank;				// time from hblank start to hblank end (cycles)
	u32 hScanlinesPerFrame;	// number of scanlines per frame (525/625 for NTSC/PAL)
};


static vSyncTimingInfo vSyncInfo;


static __forceinline void vSyncInfoCalc( vSyncTimingInfo* info, u32 framesPerSecond, u32 scansPerFrame )
{
	// Important: Cannot use floats or doubles here.  The emulator changes rounding modes
	// depending on user-set speedhack options, and it can break float/double code
	// (as in returning infinities and junk)

	// NOTE: mgs3 likes a /4 vsync, but many games prefer /2.  This seems to indicate a
	// problem in the counters vsync gates somewhere.

	u64 Frame = ((u64)PS2CLK * 1000000ULL) / framesPerSecond;
	u64 HalfFrame = Frame / 2;
	u64 Blank = HalfFrame / 2;		// two blanks and renders per frame
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


u32 UpdateVSyncRate()
{
	const char *limiterMsg = "Framelimiter rate updated (UpdateVSyncRate): %d.%d fps\n";

	// fixme - According to some docs, progressive-scan modes actually refresh slower than
	// interlaced modes.  But I can't fathom how, since the refresh rate is a function of
	// the television and all the docs I found on TVs made no indication that they ever
	// run anything except their native refresh rate.

	//#define VBLANK_NTSC			((Config.PsxType & 2) ? 59.94 : 59.82) //59.94 is more precise
	//#define VBLANK_PAL			((Config.PsxType & 2) ? 50.00 : 49.76)

	if(Config.PsxType & 1)
	{
		if( vSyncInfo.Framerate != FRAMERATE_PAL )
			vSyncInfoCalc( &vSyncInfo, FRAMERATE_PAL, SCANLINES_TOTAL_PAL );
	}
	else
	{
		if( vSyncInfo.Framerate != FRAMERATE_NTSC )
			vSyncInfoCalc( &vSyncInfo, FRAMERATE_NTSC, SCANLINES_TOTAL_NTSC );
	}

	counters[4].CycleT = vSyncInfo.hRender; // Amount of cycles before the counter will be updated
	counters[5].CycleT = vSyncInfo.Render; // Amount of cycles before the counter will be updated

	if (Config.CustomFps > 0)
	{
		s64 ticks = GetTickFrequency() / Config.CustomFps;
		if( m_iTicks != ticks )
		{
			m_iTicks = ticks;
			SysPrintf( limiterMsg, Config.CustomFps, 0 );
		}
	}
	else
	{
		s64 ticks = (GetTickFrequency() * 50) / vSyncInfo.Framerate;
		if( m_iTicks != ticks )
		{
			m_iTicks = ticks;
			SysPrintf( limiterMsg, vSyncInfo.Framerate/50, (vSyncInfo.Framerate*2)%100 );
		}
	}

	m_iStart = GetCPUTicks();
	cpuRcntSet();

	// Initialize VU Skip Stuff...
	assert(Cpu != NULL && Cpu->ExecuteVU1Block != NULL );
	s_prevExecuteVU1Block = Cpu->ExecuteVU1Block;
	g_vu1SkipCount = 0;

	return (u32)m_iTicks;
}

extern u32 vu0time;


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
			g_fGSSave->gsFreeze();
			
			if (GSfreeze(FREEZE_SIZE, &fP) == -1) {
				safe_delete( g_fGSSave );
				g_SaveGSStream = 0;
			}
			else {
				fP.data = (s8*)malloc(fP.size);
				if (fP.data == NULL) {
					safe_delete( g_fGSSave );
					g_SaveGSStream = 0;
				}
				else {
					if (GSfreeze(FREEZE_SAVE, &fP) == -1) {
						safe_delete( g_fGSSave );
						g_SaveGSStream = 0;
					}
					else {
						g_fGSSave->Freeze( fP.size );
						if (fP.size) {
							g_fGSSave->FreezeMem( fP.data, fP.size );
							free(fP.data);
						}
					}
				}
			}
		}
		else if( g_SaveGSStream == 2 ) {
			
			if( --g_nLeftGSFrames <= 0 ) {
				safe_delete( g_fGSSave );
				g_SaveGSStream = 0;
				Console::WriteLn("Done saving GS stream");
			}
		}
#endif
}

void frameLimitReset()
{
	m_iStart = GetCPUTicks();
}

// Framelimiter - Measures the delta time between calls and stalls until a
// certain amount of time passes if such time hasn't passed yet.
// See the GS FrameSkip function for details on why this is here and not in the GS.
static __forceinline void frameLimit()
{
	s64 sDeltaTime;
	u64 uExpectedEnd;
	u64 iEnd;

	if( CHECK_FRAMELIMIT == PCSX2_FRAMELIMIT_NORMAL ) return;
	if( Config.CustomFps >= 999 ) return;	// means the user would rather just have framelimiting turned off...

	uExpectedEnd = m_iStart + m_iTicks;
	iEnd = GetCPUTicks();

	sDeltaTime = iEnd - uExpectedEnd;

	// If the framerate drops too low, reset the expected value.  This avoids
	// excessive amounts of "fast forward" syndrome which would occur if we
	// tried to catch up too much.
	
	if( sDeltaTime > m_iTicks*8 )
	{
		m_iStart = iEnd - m_iTicks;

		// Let the GS Skipper know we lost time.
		// Keeps the GS skipper from trying to catch up to a framerate
		// that the limiter already gave up on.

		gsSyncLimiterLostTime( (s32)(m_iStart - uExpectedEnd) );
		return;
	}

	// use the expected frame completion time as our starting point.
	// improves smoothness by making the framelimiter more adaptive to the
	// imperfect TIMESLICE() wait, and allows it to speed up a wee bit after
	// slow frames to "catch up."

	m_iStart = uExpectedEnd;

	while( sDeltaTime < 0 )
	{
		Timeslice();
		iEnd = GetCPUTicks();
		sDeltaTime = iEnd - uExpectedEnd;
	}
}

static __forceinline void VSyncStart(u32 sCycle)
{
	vSyncDebugStuff(); // EE Profiling and Debug code

	if ((CSRw & 0x8)) GSCSRr|= 0x8;
	if (!(GSIMR&0x800)) gsIrq();

	// HACK : For some inexplicable reason, having the IntcIrq(2) handled during the
	// current Event Test breaks some games (Grandia 2 at bootup).  I can't fathom why.
	// To fix I fool the Intc handler into thinking that we're not in an event test, so
	// that it schedules the handler into the future by 4 cycles. (air)

	//eeEventTestIsActive = false;
	hwIntcIrq(2);
	//eeEventTestIsActive = true;

	psxVBlankStart();

	if (gates) rcntStartGate(0x8, sCycle); // Counters Start Gate code
	if (Config.Patch) applypatch(1); // Apply patches (ToDo: clean up patch code)

	cpuRegs.eCycle[30] = 8;
}

static __forceinline void VSyncEnd(u32 sCycle)
{
	iFrame++;

	if( g_vu1SkipCount > 0 )
	{
		gsPostVsyncEnd( false );
		AtomicDecrement( g_vu1SkipCount );
		Cpu->ExecuteVU1Block = DummyExecuteVU1Block;
	}
	else
	{
		gsPostVsyncEnd( true );
		Cpu->ExecuteVU1Block = s_prevExecuteVU1Block;
	}

	hwIntcIrq(3);  // HW Irq
	psxVBlankEnd(); // psxCounters vBlank End
	if (gates) rcntEndGate(0x8, sCycle); // Counters End Gate Code
	frameLimit(); // limit FPS
}

//#define VSYNC_DEBUG		// Uncomment this to enable some vSync Timer debugging features.
#ifdef VSYNC_DEBUG
static u32 hsc=0;
static int vblankinc = 0;
#endif

__forceinline void rcntUpdate_hScanline()
{
	if( !cpuTestCycle( counters[4].sCycle, counters[4].CycleT ) ) return;

	iopBranchAction = 1;
	if (counters[4].mode & MODE_HBLANK) { //HBLANK Start
		rcntStartGate(0, counters[4].sCycle);
		psxCheckStartGate16(0);
		
		// Setup the hRender's start and end cycle information:
		counters[4].sCycle += vSyncInfo.hBlank;		// start  (absolute cycle value)
		counters[4].CycleT = vSyncInfo.hRender;		// endpoint (delta from start value)
		counters[4].mode = MODE_HRENDER;
	}
	else { //HBLANK END / HRENDER Begin
		if (CSRw & 0x4) GSCSRr |= 4; // signal
		if (!(GSIMR&0x400)) gsIrq();
		if (gates) rcntEndGate(0, counters[4].sCycle);
		if (psxhblankgate) psxCheckEndGate16(0);

		// set up the hblank's start and end cycle information:
		counters[4].sCycle += vSyncInfo.hRender;	// start (absolute cycle value)
		counters[4].CycleT = vSyncInfo.hBlank;		// endpoint (delta from start value)
		counters[4].mode = MODE_HBLANK;

#		ifdef VSYNC_DEBUG
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
		
		SysUpdate();  // check for and handle keyevents
	}
	else	// VSYNC end / VRENDER begin
	{
		VSyncStart(counters[5].sCycle);

		counters[5].sCycle += vSyncInfo.Render;
		counters[5].CycleT = vSyncInfo.Blank;
		counters[5].mode = MODE_VSYNC;

		// Accumulate hsync rounding errors:
		counters[4].sCycle += vSyncInfo.hSyncError;

#		ifdef VSYNC_DEBUG
		vblankinc++;
		if( vblankinc > 1 )
		{
			if( hsc != vSyncInfo.hScanlinesPerFrame )
				SysPrintf( " ** vSync > Abnormal Scanline Count: %d\n", hsc );
			hsc = 0;
			vblankinc = 0;
		}
#		endif

	}
}

static __forceinline void __fastcall _cpuTestTarget( int i )
{
	if (counters[i].count < counters[i].target) return;

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

static __forceinline void _cpuTestOverflow( int i )
{
	if (counters[i].count <= 0xffff) return;
	
	if (counters[i].mode & 0x0200) { // Overflow interrupt
		EECNT_LOG("EE counter %d overflow mode %x count %x target %x\n", i, counters[i].mode, counters[i].count, counters[i].target);
		counters[i].mode |= 0x0800; // Overflow flag
		hwIntcIrq(counters[i].interrupt);
	}
	
	// wrap counter back around zero, and enable the future target:
	counters[i].count -= 0x10000;
	counters[i].target &= 0xffff;
}


// forceinline note: this method is called from two locations, but one
// of them is the interpreter, which doesn't count. ;)  So might as
// well forceinline it!
__forceinline void rcntUpdate()
{
	int i;

	rcntUpdate_vSync();

	// Update counters so that we can perform overflow and target tests.
	
	for (i=0; i<=3; i++) {
		
		// We want to count gated counters (except the hblank which exclude below, and are
		// counted by the hblank timer instead)

		//if ( gates & (1<<i) ) continue;
		
		if (!(counters[i].mode & 0x80)) continue;

		if((counters[i].mode & 0x3) != 0x3)	// don't count hblank sources
		{
			s32 change = cpuRegs.cycle - counters[i].sCycleT;
			if( change < 0 ) change = 0;	// sanity check!

			counters[i].count += change / counters[i].rate;
			change -= (change / counters[i].rate) * counters[i].rate;
			counters[i].sCycleT = cpuRegs.cycle - change;

			// Check Counter Targets and Overflows:
			_cpuTestTarget( i );
			_cpuTestOverflow( i );
		} 
		else counters[i].sCycleT = cpuRegs.cycle;
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

static void _rcntSetGate( int index )
{
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
	
	_rcntSetGate( index );
	_rcntSet( index );
}

// mode - 0 means hblank source, 8 means vblank source.
void rcntStartGate(unsigned int mode, u32 sCycle) {
	int i;

	for (i=0; i <=3; i++) { //Gates for counters

		if ((mode == 0) && ((counters[i].mode & 0x83) == 0x83))
		{
			counters[i].count += HBLANK_COUNTER_SPEED; //Update counters using the hblank as the clock
			_cpuTestTarget( i );
			_cpuTestOverflow( i );
		}
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

// mode - 0 means hblank signal, 8 means vblank signal.
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

void SaveState::rcntFreeze()
{
	Freeze(counters);
	Freeze(nextCounter);
	Freeze(nextsCounter);

	// New in version 11 -- save the PAL/NTSC info!
	if( GetVersion() > 0x10 )
	{
		Freeze( Config.PsxType );
	}

	if( IsLoading() )
	{
		UpdateVSyncRate();

#ifdef PCSX2_VIRTUAL_MEM
		// Sanity check for loading older savestates:

		if( counters[4].sCycle == 0 )
			counters[4].sCycle = cpuRegs.cycle;

		if( counters[5].sCycle == 0 )
			counters[5].sCycle = cpuRegs.cycle;
#endif
	
		// make sure the gate flags are set based on the counter modes...
		for( int i=0; i<4; i++ )
			_rcntSetGate( i );

		iopBranchAction = 1;	// probably not needed but won't hurt anything either.
	}
}

