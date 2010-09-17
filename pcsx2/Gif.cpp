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
#include "ps2/NewDmac.h"

using namespace EE_DMAC;

void GIF_ArbitratePaths()
{
	pxAssume(gifRegs.stat.APATH == GIF_APATH_IDLE);
	pxAssume(!gifRegs.stat.OPH);

	// GIF Arbitrates with a simple priority logic that goes in order of PATH.  As long
	// as PATH! transfers are queued, it'll keep using those, for example.

	do {
		if (gifRegs.stat.P1Q)
		{
			gifRegs.SetActivePath( GIF_APATH1 );
			gifRegs.stat.P1Q = 0;

			// Transfer XGKICK data
			
		}
		else if (gifRegs.stat.P2Q)
		{
			gifRegs.SetActivePath( GIF_APATH2 );
			gifRegs.stat.P2Q = 0;
		}
		else if (gifRegs.stat.IP3)
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
		}
		else
		{
			// PATH3 can only transfer if it hasn't been masked

			// [Ps2Confirm] Changes to M3R and MSKPATH3 registers likely take effect during
			// arbitration to a new PATH3 transfer only, and should not affect resuming an
			// intermittent transfer.  This is an assumption at this point and has not been
			// confirmed.

			if (gifRegs.mode.M3R || vifProc[1].maskpath3) return;

			if (gifRegs.stat.P3Q)
			{
				gifRegs.SetActivePath( GIF_APATH3 );
				gifRegs.stat.P3Q = 0;
			}
		}
	} while (UseDmaBurstHack);
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

GIF_PathQueueResult GIF_QueuePath1()
{
	if ((gifRegs.stat.APATH == GIF_APATH_IDLE) || GIF_InterruptPath3(GIF_APATH1))
	{
		gifRegs.SetActivePath( GIF_APATH1 );
		GIF_LOG("GIFpath rights acquired by PATH1 (VU1 XGKICK).");
		return GIFpath_Acquired;
	}

	if (gifRegs.stat.P1Q) return GIFpath_Busy;
	
	GIF_LOG("GIFpath queued request for PATH1 (VU1 XGKICK).");
	gifRegs.stat.P1Q = 1;
	return GIFpath_Queued;
}

GIF_PathQueueResult GIF_QueuePath2()
{
	pxAssumeDev( (vif1Regs.code.CMD == VIFcode_DIRECTHL) || (vif1Regs.code.CMD == VIFcode_DIRECT),
		"Invalid VIFcode state encountered while processing PATH2.  Current VIFcode is not DIRECT/DIRECTHL!"
	);

	bool isDirectHL = (vif1Regs.code.CMD == VIFcode_DIRECTHL);

	if (gifRegs.stat.APATH == GIF_APATH_IDLE)
	{
		gifRegs.SetActivePath( GIF_APATH2 );
		GIF_LOG("GIFpath rights acquired by PATH2 (VIF1 DIRECT%s).", isDirectHL ? "HL" : "");
		return GIFpath_Acquired;
	}

	if (!isDirectHL && GIF_InterruptPath3(GIF_APATH2))
	{
		gifRegs.SetActivePath( GIF_APATH2 );
		GIF_LOG("GIFpath rights acquired by PATH2 (VIF1 DIRECT%s).", isDirectHL ? "HL" : "");
		return GIFpath_Acquired;
	}

	//if (!pxAssertDev(gifRegs.stat.APATH == GIF_APATH2 || gifRegs.stat.P2Q, "Possible recursive PATH2 transfer detected!"))
	if (gifRegs.stat.P2Q) return GIFpath_Busy;

	GIF_LOG("GIFpath queued request for PATH2 (VIF1 DIRECT%s).", isDirectHL ? "HL" : "");
	gifRegs.stat.P2Q = 1;
	return GIFpath_Queued;
}

GIF_PathQueueResult GIF_QueuePath3()
{
	if (gifRegs.stat.APATH == GIF_APATH_IDLE)
	{
		gifRegs.SetActivePath( GIF_APATH3 );
		GIF_LOG("GIFpath rights acquired by PATH3 (GIF DMA/FIFO).");
		return GIFpath_Acquired;
	}

	if (gifRegs.stat.P3Q) return GIFpath_Busy;

	GIF_LOG("GIFpath queued request for PATH3 (GIF DMA/FIFO).");
	gifRegs.stat.P3Q = 1;
	return GIFpath_Queued;
}

void SaveStateBase::gifFreeze()
{
	FreezeTag( "GIFdma" );

}
