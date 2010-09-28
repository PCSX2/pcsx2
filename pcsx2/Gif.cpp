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
#include "Common.h"

#include "GS.h"
#include "Gif.h"
#include "Vif.h"		// needed to test for VIF path3 masking
#include "VUmicro.h"

#include "ps2/NewDmac.h"

using namespace EE_DMAC;

static u32 xgkick_queue_addr;

// Parameters:
//   baseMem - pointer to the base of the memory buffer.  If the transfer wraps past the specified
//      memSize, it restarts bask here.
//
//   fragment_size - size of incoming data stream, in qwc (simd128).  If this parameter is 0, then
//      the GIF continues to process until the first EOP condition is met.
//
//   startPos - start position within the buffer.
//   memSize - size of the source memory buffer pointed to by baseMem.  If 0, no wrapping logic is performed.
//
// Returns:
//   Amount of data processed.  Actual processed amount may be less than provided size, depending
//   on GS stalls (caused by SIGNAL or EOP, etc).
//
static __fi uint _uploadTag(const u128* baseMem, uint fragment_size, uint startPos, uint memSize)
{
	GetMTGS().PrepDataPacket(GS_RINGTYPE_PATH, fragment_size ? fragment_size : memSize);
	uint processed = g_gifpath.CopyTag(baseMem, fragment_size, startPos, memSize);
	GetMTGS().SendDataPacket();
	return processed;
}

__fi uint GIF_UploadTag(const u128* baseMem, uint fragment_size, uint startPos, uint memSize)
{
	uint processed = _uploadTag(baseMem, fragment_size, startPos, memSize);
	GIF_ArbitratePaths();
	return processed;
}

static bool GIF_TransferXGKICK( u32 vumem )
{
	uint processed = _uploadTag( (u128*)vuRegs[1].Mem, 0x400, vumem, 0x400 );
	if (gifRegs.GetActivePath() == GIF_APATH1)
	{
		// Transfer stalled for some reason, either due to SIGNAL or due to infinite-wrapping.
		// We'll have to try and finish it later.

		GIF_LOG("GIFpath transfer stall on PATH1 (VU1 XGKICK)");

		//gifRegs.stat.P1Q = 1;
		xgkick_queue_addr = (vumem + processed) & 0x3ff;
		return false;
	}

	return true;
}

void GIF_ArbitratePaths()
{
	//pxAssume(gifRegs.stat.APATH == GIF_APATH_IDLE);
	//pxAssume(!gifRegs.stat.OPH);

	// GIF Arbitrates with a simple priority logic that goes in order of PATH.  As long
	// as PATH1 transfers are queued, it'll keep using those, for example.

	// PATH1 transfers are handled inline; PATH2 and PATH3 transfers are handled by sending
	// a DREQ to the respective DMA channel and then waiting for it to resume the transfer.

	if (gifRegs.stat.P1Q || (gifRegs.GetActivePath() == GIF_APATH1))
	{
		gifRegs.SetActivePath( GIF_APATH1 );
		gifRegs.stat.P1Q = 0;

		if (!GIF_TransferXGKICK(xgkick_queue_addr)) return;

		vu1ResumeXGKICK();
		if (gifRegs.stat.P1Q)
		{
			// Micro-program queued and stalled *AGAIN*  .. dumb thing.
			// (whatever blocking condition will need to be cleared by the EE, so break)
			return;
		}
	}

	if (gifRegs.stat.P2Q)
	{
		gifRegs.SetActivePath( GIF_APATH2 );
		gifRegs.stat.P2Q = 0;

		dmacRequestSlice( ChanId_VIF1 );
	}
	
	if (gifRegs.stat.IP3)
	{
		// Resuming in-progress PATH3 transfer.
		// Reload suspended tag info.

		gifRegs.SetActivePath( GIF_APATH3 );

		gifRegs.stat.IP3	= 0;
		gifRegs.tag0		= gifRegs.p3tag;

		g_gifpath.nloop		= gifRegs.p3cnt.P3CNT;
		g_gifpath.tag.EOP	= gifRegs.p3tag.EOP;
		g_gifpath.tag.NLOOP	= gifRegs.p3tag.LOOPCNT;
		g_gifpath.tag.FLG	= GIF_FLG_IMAGE;

		ProcessFifoEvent();
		dmacRequestSlice(ChanId_GIF);
		return;
	}
	
	if (gifRegs.stat.P3Q)
	{
		// PATH3 can only transfer if it hasn't been masked

		// [Ps2Confirm] Changes to M3R and MSKPATH3 registers likely take effect during
		// arbitration to a new PATH3 transfer only, and should not affect resuming an
		// intermittent transfer.  This is an assumption at this point and has not been
		// confirmed.

		if (GIF_MaskedPath3()) return;

		gifRegs.SetActivePath( GIF_APATH3 );
		gifRegs.stat.P3Q = 0;

		ProcessFifoEvent();
		dmacRequestSlice(ChanId_GIF);
		return;
	}

	// No queued or pending/partial transfers left:  Signal FINISH if needed,

	// IMPORTANT: We only signal FINISH if GIFpath processing stopped (EOP and no nloop),
	// *and* no other transfers are in the queue (including PATH3 interrupted!!).
	// FINISH is typically used to make sure the FIFO for the GIF is clear before
	// switching the TXDIR.

	if (CSRreg.FINISH && !(GSIMR & 0x200))
	{
		gsIrq();
	}
}

bool GIF_InterruptPath3( gif_active_path apath )
{
	pxAssumeDev((apath != GIF_APATH1) && (apath != GIF_APATH2),
		"Invalid parameter; please specify either PATH1 or PATH2.");

	if (gifRegs.stat.APATH != GIF_APATH3) return false;
	if (gifRegs.tag1.FLG != GIF_FLG_IMAGE) return false;

	pxAssume( g_gifpath.nloop < 0x8000 );		// max range of P3CNT

	// currently active path is in the midst of an IMAGE mode transfer.  Technically we
	// should only perform arbitration to PATH2 when IMAGE data transfer crosses an
	// 8 QWC slice; but such a fine detail is likely *not* important .. ever.  --air

	gifRegs.stat.IP3	= 1;
	gifRegs.stat.APATH	= apath;
	gifRegs.p3cnt.P3CNT = g_gifpath.nloop;
	gifRegs.p3tag		= gifRegs.tag0;
	
	pxAssumeDev(gifRegs.p3cnt.P3CNT != 0, "Resuming PATH3 is already finished? (p3cnt==0)");

	GIF_LOG( "PATH3 interrupted by PATH%u (P3CNT=%x)", gifRegs.p3cnt );
	return true;
}


// returns FALSE if the transfer is blocked; returns true if the transfer proceeds unimpeeded
// and/or is queued.
//
// (called from recompilers, must remain __fastcall!)
bool __fastcall GIF_QueuePath1( u32 vumem )
{
	vumem &= 0x3ff;
	if ((gifRegs.GetActivePath() == GIF_APATH_IDLE) || GIF_InterruptPath3(GIF_APATH1))
	{
		gifRegs.SetActivePath( GIF_APATH1 );
		GIF_LOG("GIFpath rights acquired by PATH1 (VU1 XGKICK).");
		GIF_TransferXGKICK(vumem);

		// Always return true -- if the transfer stalled it'll queue (next XGKICK will stall instead).
		return true;
	}

	if (gifRegs.stat.P1Q) return false;

	GIF_LOG("GIFpath queued request for PATH1 (VU1 XGKICK) @ 0x%04x", vumem);
	gifRegs.stat.P1Q = 1;
	xgkick_queue_addr = vumem;

	return true;
}

bool GIF_QueuePath2()
{
	pxAssume(!gifRegs.stat.P2Q);
	
	pxAssumeDev( (vif1Regs.code.CMD == VIFcode_DIRECTHL) || (vif1Regs.code.CMD == VIFcode_DIRECT),
		"Invalid VIFcode state encountered while processing PATH2.  Current VIFcode is not DIRECT/DIRECTHL!"
	);

	bool isDirectHL = (vif1Regs.code.CMD == VIFcode_DIRECTHL);

	if (gifRegs.stat.APATH == GIF_APATH_IDLE)
	{
		gifRegs.SetActivePath( GIF_APATH2 );
		GIF_LOG("GIFpath rights acquired by PATH2 (VIF1 DIRECT%s).", isDirectHL ? "HL" : "");
		return true;
	}

	if (!isDirectHL && GIF_InterruptPath3(GIF_APATH2))
	{
		gifRegs.SetActivePath( GIF_APATH2 );
		GIF_LOG("GIFpath rights acquired by PATH2 (VIF1 DIRECT%s).", isDirectHL ? "HL" : "");
		return true;
	}

	GIF_LOG("GIFpath queued request for PATH2 (VIF1 DIRECT%s).", isDirectHL ? "HL" : "");
	gifRegs.stat.P2Q = 1;
	return false;
}

bool GIF_ClaimPath3()
{
	if (gifRegs.stat.APATH == GIF_APATH3) return true;
	if (gifRegs.stat.P3Q)
	{
		pxAssumeDev(gifRegs.stat.APATH != GIF_APATH_IDLE, "Invalid GIFpath state during PATH3 arbitration request.");
		return false;
	}

	if (gifRegs.stat.APATH == GIF_APATH_IDLE)
	{
		if (GIF_MaskedPath3())
		{
			GIF_LOG("PATH3 Masked!  (arbitration denied)");
			gifRegs.stat.P3Q = 1;
			return false;
		}

		gifRegs.SetActivePath( GIF_APATH3 );
		GIF_LOG("GIFpath rights acquired by PATH3 (GIF DMA/FIFO).");
		return true;
	}

	GIF_LOG("GIFpath queued request for PATH3 (GIF DMA/FIFO).");
	gifRegs.stat.P3Q = 1;
	return false;
}

bool GIF_MaskedPath3()
{
	return (gifRegs.mode.M3R || vifProc[1].maskpath3);
}

__fi u32 gifRead32(u32 mem)
{
	switch (mem) {
		case GIF_STAT:
			ProcessFifoEvent();
			gifRegs.stat.FQC = g_fifo.gif.qwc;
			return gifRegs.stat._u32;
	}
	return psHu32(mem);
}

uint __dmacall EE_DMAC::toGIF(const u128* srcBase, uint srcSize, uint srcStartQwc, uint lenQwc)
{
	// DMA transfers to GIF use PATH3.
	// PATH3 can be disabled/masked by various conditions.  In such cases the DMA acts as a stall:

	if ((gifRegs.stat.APATH != GIF_APATH3) && !GIF_ClaimPath3()) return 0;

	return GIF_UploadTag(srcBase, lenQwc, srcStartQwc, srcSize);
}

void SaveStateBase::gifFreeze()
{
	FreezeTag( "GIFdma" );

}
