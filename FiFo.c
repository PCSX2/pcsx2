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

//well that might need a recheck :P
//(shadow bug0r work :P)

//plz, change this file according to FIFO defs in hw.h
/*	u64 VIF0[0x1000];
	u64 VIF1[0x1000];
	u64 GIF[0x1000];
	u64 IPU_out_FIFO[0x160];	// 128bit
	u64 IPU_in_FIFO[0x160];	// 128bit
*/

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "Common.h"
#include "Hw.h"

#include <assert.h>

//////////////////////////////////////////////////////////////////////////
/////////////////////////// Quick & dirty FIFO :D ////////////////////////
//////////////////////////////////////////////////////////////////////////
extern int fifo_wwrite(u32* pMem, int size);
extern void fifo_wread1(void *value);

extern int g_nIPU0Data;
extern u8* g_pIPU0Pointer;

// NOTE: cannot use XMM/MMX regs
void ReadFIFO(u32 mem, u64 *out) {
	if ((mem >= 0x10004000) && (mem < 0x10005000)) {
#ifdef VIF_LOG
		VIF_LOG("ReadFIFO VIF0 0x%08X\n", mem);
#endif
		out[0] = psHu64(mem  );
		out[1] = psHu64(mem+8);
		return;
	} else
	if ((mem >= 0x10005000) && (mem < 0x10006000)) {

#ifdef PCSX_DEVBUILD
		VIF_LOG("ReadFIFO VIF1 0x%08X\n", mem);

		if( vif1Regs->stat & (VIF1_STAT_INT|VIF1_STAT_VSS|VIF1_STAT_VIS|VIF1_STAT_VFS) ) {
			SysPrintf("reading from vif1 fifo when stalled\n");
		}
#endif

		if (vif1Regs->stat & 0x800000) {
			if (--psHu32(D1_QWC) == 0) {
				vif1Regs->stat&= ~0x1f000000;
			} else {
			}
		}
		out[0] = psHu64(mem  );
		out[1] = psHu64(mem+8);
		return;
	} else if( (mem&0xfffff010) == 0x10007000) {
		
		if( g_nIPU0Data > 0 ) {
			out[0] = *(u64*)(g_pIPU0Pointer);
			out[1] = *(u64*)(g_pIPU0Pointer+8);
			g_nIPU0Data--;
			g_pIPU0Pointer += 16;
		}
		return;
	}else if ( (mem&0xfffff010) == 0x10007010) {
		fifo_wread1((void*)out);
		return;
	}
	SysPrintf("ReadFIFO Unknown %x\n", mem);
}

void ConstReadFIFO(u32 mem)
{
	// not done
}

extern HANDLE g_hGsEvent;

void WriteFIFO(u32 mem, u64 *value) {
	int ret;

	if ((mem >= 0x10004000) && (mem < 0x10005000)) {
#ifdef VIF_LOG
		VIF_LOG("WriteFIFO VIF0 0x%08X\n", mem);
#endif
		psHu64(mem  ) = value[0];
		psHu64(mem+8) = value[1];
		ret = VIF0transfer((u32*)value, 4, 0);
		assert(ret == 0 ); // vif stall code not implemented
		FreezeXMMRegs(0);
	}
	else if ((mem >= 0x10005000) && (mem < 0x10006000)) {
#ifdef VIF_LOG
		VIF_LOG("WriteFIFO VIF1 0x%08X\n", mem);
#endif
		psHu64(mem  ) = value[0];
		psHu64(mem+8) = value[1];

#ifdef PCSX2_DEVBUILD
		if(vif1Regs->stat & VIF1_STAT_FDR)
			SysPrintf("writing to fifo when fdr is set!\n");
		if( vif1Regs->stat & (VIF1_STAT_INT|VIF1_STAT_VSS|VIF1_STAT_VIS|VIF1_STAT_VFS) ) {
			SysPrintf("writing to vif1 fifo when stalled\n");
		}
#endif
		ret = VIF1transfer((u32*)value, 4, 0);
		assert(ret == 0 ); // vif stall code not implemented
		FreezeXMMRegs(0);
	}
	else if ((mem >= 0x10006000) && (mem < 0x10007000)) {
		u64* data;
#ifdef GIF_LOG
		GIF_LOG("WriteFIFO GIF 0x%08X\n", mem);
#endif

		psHu64(mem  ) = value[0];
		psHu64(mem+8) = value[1];

		if( CHECK_MULTIGS ) {
			data = (u64*)GSRingBufCopy(NULL, 16, GS_RINGTYPE_P3);

			data[0] = value[0];
			data[1] = value[1];
			GSgifTransferDummy(2, (u32*)data, 1);
			GSRINGBUF_DONECOPY(data, 16);

			if( !CHECK_DUALCORE )
				SetEvent(g_hGsEvent);
		}
		else {
			FreezeXMMRegs(1);
#ifdef GSCAPTURE
			extern u32 g_loggs, g_gstransnum, g_gsfinalnum;

			if( !g_loggs || (g_loggs && g_gstransnum++ < g_gsfinalnum)) {
				GSgifTransfer3((u32*)value, 1-GSgifTransferDummy(2, (u32*)value, 1));
			}
#else
			GSgifTransfer3((u32*)value, 1);
#endif
			FreezeXMMRegs(0);
		}

	} else
	if ((mem&0xfffff010) == 0x10007010) {
#ifdef IPU_LOG
		IPU_LOG("WriteFIFO IPU_in[%d] <- %8.8X_%8.8X_%8.8X_%8.8X\n", (mem - 0x10007010)/8, ((u32*)value)[3], ((u32*)value)[2], ((u32*)value)[1], ((u32*)value)[0]);
#endif
		//commiting every 16 bytes
		while( fifo_wwrite((void*)value, 1) == 0 ) {
			SysPrintf("IPU sleeping\n");
			Sleep(1);
		}
	} else {
		SysPrintf("WriteFIFO Unknown %x\n", mem);
	}
}

void ConstWriteFIFO(u32 mem) {
}