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

#include "PrecompiledHeader.h"

#include <list>
#include <memory>

#include "GS.h"

#ifdef PCSX2_DEVBUILD

// GS Playback
int g_SaveGSStream = 0; // save GS stream; 1 - prepare, 2 - save
int g_nLeftGSFrames = 0; // when saving, number of frames left
static std::unique_ptr<memSavingState> g_fGSSave;

// fixme - need to take this concept and make it MTGS friendly.
#ifdef _STGS_GSSTATE_CODE
void GSGIFTRANSFER1(u32 *pMem, u32 addr) {
	if( g_SaveGSStream == 2) {
		u32 type = GSRUN_TRANS1;
		u32 size = (0x4000-(addr))/16;
		g_fGSSave->Freeze( type );
		g_fGSSave->Freeze( size );
		g_fGSSave->FreezeMem( ((u8*)pMem)+(addr), size*16 );
	}
	GSgifTransfer1(pMem, addr);
}

void GSGIFTRANSFER2(u32 *pMem, u32 size) {
	if( g_SaveGSStream == 2) {
		u32 type = GSRUN_TRANS2;
		u32 _size = size;
		g_fGSSave->Freeze( type );
		g_fGSSave->Freeze( size );
		g_fGSSave->FreezeMem( pMem, _size*16 );
	}
	GSgifTransfer2(pMem, size);
}

void GSGIFTRANSFER3(u32 *pMem, u32 size) {
	if( g_SaveGSStream == 2 ) {
		u32 type = GSRUN_TRANS3;
		u32 _size = size;
		g_fGSSave->Freeze( type );
		g_fGSSave->Freeze( size );
		g_fGSSave->FreezeMem( pMem, _size*16 );
	}
	GSgifTransfer3(pMem, size);
}

__fi void GSVSYNC(void) {
	if( g_SaveGSStream == 2 ) {
		u32 type = GSRUN_VSYNC;
		g_fGSSave->Freeze( type );
	}
}
#endif

#endif

//////////////////////////////////////////////////////////////////////////////////////////
//
void vSyncDebugStuff( uint frame )
{
#ifdef OLD_TESTBUILD_STUFF
	if( g_TestRun.enabled && g_TestRun.frame > 0 ) {
		if( frame > g_TestRun.frame ) {
			// take a snapshot
			if( g_TestRun.pimagename != NULL && GSmakeSnapshot2 != NULL ) {
				if( g_TestRun.snapdone ) {
					g_TestRun.curimage++;
					g_TestRun.snapdone = 0;
					g_TestRun.frame += 20;
					if( g_TestRun.curimage >= g_TestRun.numimages ) {
						// exit
						g_EmuThread->Cancel();
					}
				}
				else {
					// query for the image
					GSmakeSnapshot2(g_TestRun.pimagename, &g_TestRun.snapdone, g_TestRun.jpgcapture);
				}
			}
			else {
				// exit
				g_EmuThread->Cancel();
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
			Console.WriteLn("Done saving GS stream");
		}
	}
#endif
}
