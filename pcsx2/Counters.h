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
// NTSC Timing Information!!! (some scanline info is guessed)
//------------------------------------------------------------------
#define SCANLINE_NTSC		18743  //(PS2CLK / 15734.25)
#define HRENDER_TIME_NTSC	15528  //time from hblank end to hblank start (PS2CLK / 18991.368423051722991900181367568)
#define HBLANK_TIME_NTSC	3215   //time from hblank start to hblank end (PS2CLK / 91738.91105912572817760653181028)
#define VSYNC_NTSC			(PS2CLK / 59.94)  //hz

#define SCANLINES_TOTAL_NTSC	525 // total number of scanlines
#define SCANLINES_VSYNC_NTSC	3   // scanlines that are used for syncing every half-frame
#define SCANLINES_VRENDER_NTSC	240 // scanlines in a half-frame (because of interlacing)
#define SCANLINES_VBLANK1_NTSC	19  // scanlines used for vblank1 (even interlace)
#define SCANLINES_VBLANK2_NTSC	20  // scanlines used for vblank2 (odd interlace)

//------------------------------------------------------------------
// PAL Timing Information!!! (some scanline info is guessed)
//------------------------------------------------------------------
#define SCANLINE_PAL		18874
#define HRENDER_TIME_PAL	15335  //time from hblank end to hblank start
#define HBLANK_TIME_PAL		3539   //time from hblank start to hblank end
#define VSYNC_PAL			(PS2CLK / 50) //hz

#define SCANLINES_TOTAL_PAL		625 // total number of scanlines
#define SCANLINES_VSYNC_PAL		5   // scanlines that are used for syncing every half-frame
#define SCANLINES_VRENDER_PAL	288 // scanlines in a half-frame (because of interlacing)
#define SCANLINES_VBLANK1_PAL	19  // scanlines used for vblank1 (even interlace)
#define SCANLINES_VBLANK2_PAL	20  // scanlines used for vblank2 (odd interlace)

//------------------------------------------------------------------
// PAL Timing Information!!!
//------------------------------------------------------------------
#define SCANLINE_		(u32)((Config.PsxType&1) ? SCANLINE_PAL : SCANLINE_NTSC)
#define HRENDER_TIME_	(u32)((Config.PsxType&1) ? HRENDER_TIME_PAL : HRENDER_TIME_NTSC)
#define HBLANK_TIME_	(u32)((Config.PsxType&1) ? HBLANK_TIME_PAL : HBLANK_TIME_NTSC)
#define VSYNC_			(u32)((Config.PsxType&1) ? VSYNC_PAL : VSYNC_NTSC)

#define SCANLINES_TOTAL_	(u32)((Config.PsxType&1) ? SCANLINES_TOTAL_PAL : SCANLINES_TOTAL_NTSC)
#define SCANLINES_VSYNC_	(u32)((Config.PsxType&1) ? SCANLINES_VSYNC_PAL : SCANLINES_VSYNC_NTSC)
#define SCANLINES_VRENDER_	(u32)((Config.PsxType&1) ? SCANLINES_VRENDER_PAL : SCANLINES_VRENDER_NTSC)
#define SCANLINES_VBLANK1_	(u32)((Config.PsxType&1) ? SCANLINES_VBLANK1_PAL : SCANLINES_VBLANK1_NTSC)
#define SCANLINES_VBLANK2_	(u32)((Config.PsxType&1) ? SCANLINES_VBLANK2_PAL : SCANLINES_VBLANK2_NTSC)

//------------------------------------------------------------------
// vSync and hBlank Timing Modes
//------------------------------------------------------------------
#define MODE_VRENDER	0x10000	//Set during the Render/Frame Scanlines
#define MODE_VSYNC		0x00000 //Set during the Syncing Scanlines
#define MODE_VBLANK		0x30000	//Set during the Blanking Scanlines
#define MODE_VBLANK2	0x40000	//Set during the Blanking Scanlines (half-frame 2)
//#define MODE_DO_ONCE	0x80000	//Do the code once per change of state
#define MODE_HRENDER	0x00000	//Set for ~5/6 of 1 Scanline
#define MODE_HBLANK		0x10000	//Set for the remaining ~1/6 of 1 Scanline


extern Counter counters[6];
extern u32 nextCounter, nextsCounter;

void rcntInit();
void rcntUpdate();
void rcntStartGate(unsigned int mode);
void rcntEndGate(unsigned int mode);
void rcntWcount(int index, u32 value);
void rcntWmode(int index, u32 value);
void rcntWtarget(int index, u32 value);
void rcntWhold(int index, u32 value);
u32	 rcntRcount(int index);
u32	 rcntCycle(int index);
int  rcntFreeze(gzFile f, int Mode);

void UpdateVSyncRate();

#endif /* __COUNTERS_H__ */
