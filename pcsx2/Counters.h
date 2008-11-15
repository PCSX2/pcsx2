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

typedef struct {
	u32 count, mode, target, hold;
	u32 rate, interrupt;
	u32 Cycle, sCycle;
	u32 CycleT, sCycleT;
} Counter;

//------------------------------------------------------------------
// SPEED HACKS!!! (1 is normal) (They have inverse affects, only set 1 at a time)
//------------------------------------------------------------------
#define HBLANK_COUNTER_SPEED	1 //Set to '3' to double the speed of games like KHII
//#define HBLANK_TIMER_SLOWDOWN	1 //Set to '2' to increase the speed of games like God of War (FPS will be less, but game will be faster)

//------------------------------------------------------------------
// NTSC Timing Information!!! (some scanline info is guessed)
//------------------------------------------------------------------
#define SCANLINE_NTSC		(u32)(PS2CLK / 15734.25)//18743  //when using 59.94005994 it rounds to 15734.27 :p (rama)
#define HRENDER_TIME_NTSC	(u32)(SCANLINE_NTSC / 2)//15528  //time from hblank end to hblank start (PS2CLK / 18991.368423051722991900181367568)
#define HBLANK_TIME_NTSC	(u32)(SCANLINE_NTSC / 2)//3215   //time from hblank start to hblank end (PS2CLK / 91738.91105912572817760653181028)
#define VSYNC_NTSC			(u32)(PS2CLK / 59.94)  //hz //59.94005994 is more precise
#define VSYNC_HALF_NTSC		(u32)(VSYNC_NTSC / 2)  //hz

#define SCANLINES_TOTAL_NTSC	525 // total number of scanlines
#define SCANLINES_VSYNC_NTSC	3   // scanlines that are used for syncing every half-frame
#define SCANLINES_VRENDER_NTSC	240 // scanlines in a half-frame (because of interlacing)
#define SCANLINES_VBLANK1_NTSC	19  // scanlines used for vblank1 (even interlace)
#define SCANLINES_VBLANK2_NTSC	20  // scanlines used for vblank2 (odd interlace)

#define HSYNC_ERROR_NTSC ((s32)VSYNC_NTSC - (s32)(((HRENDER_TIME_NTSC+HBLANK_TIME_NTSC) * SCANLINES_TOTAL_NTSC)/2) )

//------------------------------------------------------------------
// PAL Timing Information!!! (some scanline info is guessed)
//------------------------------------------------------------------
#define SCANLINE_PAL		(u32)(PS2CLK / 15625)//18874
#define HRENDER_TIME_PAL	(u32)(SCANLINE_PAL / 2)//15335  //time from hblank end to hblank start
#define HBLANK_TIME_PAL		(u32)(SCANLINE_PAL / 2)//3539   //time from hblank start to hblank end
#define VSYNC_PAL			(u32)(PS2CLK / 50)	//hz
#define VSYNC_HALF_PAL		(u32)(VSYNC_PAL / 2) //hz

#define SCANLINES_TOTAL_PAL		625 // total number of scanlines
#define SCANLINES_VSYNC_PAL		5   // scanlines that are used for syncing every half-frame
#define SCANLINES_VRENDER_PAL	288 // scanlines in a half-frame (because of interlacing)
#define SCANLINES_VBLANK1_PAL	19  // scanlines used for vblank1 (even interlace)
#define SCANLINES_VBLANK2_PAL	20  // scanlines used for vblank2 (odd interlace)

#define HSYNC_ERROR_PAL ((s32)VSYNC_PAL - (s32)((SCANLINE_PAL * SCANLINES_TOTAL_PAL) / 2))

//------------------------------------------------------------------
// Timing (PAL/NTSC) Information!!!
//------------------------------------------------------------------
#define SCANLINE_		((Config.PsxType&1) ? SCANLINE_PAL : SCANLINE_NTSC)
#define HRENDER_TIME_	((Config.PsxType&1) ? HRENDER_TIME_PAL : HRENDER_TIME_NTSC)
#define HBLANK_TIME_	((Config.PsxType&1) ? HBLANK_TIME_PAL : HBLANK_TIME_NTSC)
#define VSYNC_			((Config.PsxType&1) ? VSYNC_PAL : VSYNC_NTSC)
#define VSYNC_HALF_		((Config.PsxType&1) ? VSYNC_HALF_PAL : VSYNC_HALF_NTSC)

#define HSYNC_ERROR		((Config.PsxType&1) ? HSYNC_ERROR_PAL : HSYNC_ERROR_NTSC)

#define SCANLINES_TOTAL_	((Config.PsxType&1) ? SCANLINES_TOTAL_PAL : SCANLINES_TOTAL_NTSC)
#define SCANLINES_VSYNC_	((Config.PsxType&1) ? SCANLINES_VSYNC_PAL : SCANLINES_VSYNC_NTSC)
#define SCANLINES_VRENDER_	((Config.PsxType&1) ? SCANLINES_VRENDER_PAL : SCANLINES_VRENDER_NTSC)
#define SCANLINES_VBLANK1_	((Config.PsxType&1) ? SCANLINES_VBLANK1_PAL : SCANLINES_VBLANK1_NTSC)
#define SCANLINES_VBLANK2_	((Config.PsxType&1) ? SCANLINES_VBLANK2_PAL : SCANLINES_VBLANK2_NTSC)

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
extern u32 nextCounter, nextsCounter;

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

void UpdateVSyncRate();

#endif /* __COUNTERS_H__ */
