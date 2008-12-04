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

#ifndef __COUNTERS_H__
#define __COUNTERS_H__

// fixme: Cycle and sCycleT members are unused.
//	      But they can't be removed without making a new savestate version.
typedef struct {
	u32 count, mode, target, hold;
	u32 rate, interrupt;
	u32 Cycle;
	u32 sCycle;					// start cycle of timer
	s32 CycleT;
	u32 sCycleT;		// delta values should be signed.
} Counter;

//------------------------------------------------------------------
// SPEED HACKS!!! (1 is normal) (They have inverse affects, only set 1 at a time)
//------------------------------------------------------------------
#define HBLANK_COUNTER_SPEED	1 //Set to '3' to double the speed of games like KHII
//#define HBLANK_TIMER_SLOWDOWN	1 //Set to '2' to increase the speed of games like God of War (FPS will be less, but game will be faster)

//------------------------------------------------------------------
// NTSC Timing Information!!! (some scanline info is guessed)
//------------------------------------------------------------------
#define FRAMERATE_NTSC			2997// frames per second * 100 (29.97)

#define SCANLINES_TOTAL_NTSC	525 // total number of scanlines
#define SCANLINES_VSYNC_NTSC	3   // scanlines that are used for syncing every half-frame
#define SCANLINES_VRENDER_NTSC	240 // scanlines in a half-frame (because of interlacing)
#define SCANLINES_VBLANK1_NTSC	19  // scanlines used for vblank1 (even interlace)
#define SCANLINES_VBLANK2_NTSC	20  // scanlines used for vblank2 (odd interlace)

#define HSYNC_ERROR_NTSC ((s32)VSYNC_NTSC - (s32)(((HRENDER_TIME_NTSC+HBLANK_TIME_NTSC) * SCANLINES_TOTAL_NTSC)/2) )

//------------------------------------------------------------------
// PAL Timing Information!!! (some scanline info is guessed)
//------------------------------------------------------------------
#define FRAMERATE_PAL			2500// frames per second * 100 (25)

#define SCANLINES_TOTAL_PAL		625 // total number of scanlines per frame
#define SCANLINES_VSYNC_PAL		5   // scanlines that are used for syncing every half-frame
#define SCANLINES_VRENDER_PAL	288 // scanlines in a half-frame (because of interlacing)
#define SCANLINES_VBLANK1_PAL	19  // scanlines used for vblank1 (even interlace)
#define SCANLINES_VBLANK2_PAL	20  // scanlines used for vblank2 (odd interlace)

//------------------------------------------------------------------
// vSync and hBlank Timing Modes
//------------------------------------------------------------------
#define MODE_VRENDER	0x0		//Set during the Render/Frame Scanlines
#define MODE_VBLANK		0x1		//Set during the Blanking Scanlines
#define MODE_VSYNC		0x3		//Set during the Syncing Scanlines
#define MODE_VBLANK1	0x0		//Set during the Blanking Scanlines (half-frame 1)
#define MODE_VBLANK2	0x1		//Set during the Blanking Scanlines (half-frame 2)

#define MODE_HRENDER	0x0		//Set for ~5/6 of 1 Scanline
#define MODE_HBLANK		0x1		//Set for the remaining ~1/6 of 1 Scanline


extern Counter counters[6];
extern s32 nextCounter;		// delta until the next counter event (must be signed)
extern u32 nextsCounter;
extern u32 g_lastVSyncCycle;
extern u32 g_deltaVSyncCycle;

extern void rcntUpdate_hScanline();
extern void rcntUpdate_vSync();
extern void rcntUpdate();

void rcntInit();
void rcntStartGate(unsigned int mode, u32 sCycle);
void rcntEndGate(unsigned int mode, u32 sCycle);
void rcntWcount(int index, u32 value);
void rcntWmode(int index, u32 value);
void rcntWtarget(int index, u32 value);
void rcntWhold(int index, u32 value);
u32	 rcntRcount(int index);
u32	 rcntCycle(int index);
int  rcntFreeze(gzFile f, int Mode);

u32 UpdateVSyncRate();

#endif /* __COUNTERS_H__ */
