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

#include "Gif.h"
#include "GS.h"
#include "Vif.h"
#include "IPU/IPU.h"
#include "IPU/IPU_Fifo.h"

#include "ps2/NewDmac.h"

using namespace EE_DMAC;

//////////////////////////////////////////////////////////////////////////
/////////////////////////// Quick & dirty FIFO :D ////////////////////////
//////////////////////////////////////////////////////////////////////////

__aligned16 PeripheralFifoPack g_fifo;

// Notes on FIFO implementation
//
// The FIFO consists of four separate pages of HW register memory, each mapped to a
// PS2 device.  They are listed as follows:
//
// 0x4000 - 0x5000 : VIF0  (all registers map to 0x4000)
// 0x5000 - 0x6000 : VIF1  (all registers map to 0x5000)
// 0x6000 - 0x7000 : GS    (all registers map to 0x6000)
// 0x7000 - 0x8000 : IPU   (registers map to 0x7000 and 0x7010, respectively)

void __fastcall ReadFIFO_VIF1(mem128_t* out)
{
	// Notes:
	//  * VIF1 is only readable if the FIFO direction has been reversed.
	//  * FDR should only be reversible if the VIF is "clear" (idle), so there shouldn't
	//    be any stall conditions or other obstructions  (writes to FDR other times are
	//    likely disregarded or yield indeterminate results).

	//if (vif1Regs.stat.test(VIF1_STAT_INT | VIF1_STAT_VSS | VIF1_STAT_VIS | VIF1_STAT_VFS) )
	//	DevCon.Warning( "Reading from vif1 fifo when stalled" );

	// [Ps2Confirm] We're not sure what real hardware does when the VIF1 FIFO is empty or
	// when the direction is pointing the wrong way.  It could either deadlock the EE, return 0,
	// or return some indeterminate value (equivalent to leaving the out unmodified, etc.).
	// Current assumption is indeterminate.

	if (!vif1Regs.stat.FDR)
	{
		VIF_LOG("Read from VIF1 FIFO ignored due to FDR=0.");
		return;
	}

	if (vif1Regs.stat.FQC == 0)
	{
		// FIFO is empty, so queue up 16 qwc from the GS plugin.  Operating in bursts should
		// keep framerates from being too terribly miserable.

		// Note: The only safe way to download data form the GS is to perform a full sync/flush
		// of the GS ringbuffer.  It's slow but absolutely unavoidable.  Fortunately few games
		// rely on this feature (its quite slow on the PS2 as well for the same reason).

		GetMTGS().WaitGS();
		vif1Regs.stat.FQC = GSreadFIFO2((u64*)g_fifo.vif1, FifoSize_Vif1);
		if(!pxAssertDev(vif1Regs.stat.FQC != 0, "Reading from an empty VIF1 FIFO (FQC=0)")) return;
	}

	--vif1Regs.stat.FQC;
	CopyQWC(out, &g_fifo.vif1[vif1Regs.stat.FQC]);

	VIF_LOG("ReadFIFO/VIF1 -> %ls", out->ToString().c_str());
}

// --------------------------------------------------------------------------------------
//  WriteFIFO Strategies
// --------------------------------------------------------------------------------------
//  * Writes to VIF FIFOs should only occur while the DMA is inactive, since writing the FIFO
//    directly while DMA is running likely yields indeterminate results (corrupted VIFcodes
//    most likely). [assertion checked]
//
// The VIF/GIF could be stalled, in which case the data will be denied; so we *must* emulate
// the fifo so that the EE has a place to queue commands until it deals with the stall
// condition. The FIFO will be automatically drained whenever the VIF STAT register is written
// (which is the only action that can alleviate stalls).
//
// Technically each FIFO should start processing almost immediately (2-4 cycle delay from the
// perspective of the EE).  But since the whole point of the FIFO is to eliminate the need
// for the EE to ever care about such prompt responsiveness, we delay longer.  This gives the
// EE some time to queue up a few commands before we try to process them.
//
// (we could process immediately anyway, but I prefer to keep the hwRegister handlers as
//  simple as possible and defer complicated VIFcode processing until the ee event test-- air).
//
// [Ps2Confirm] If the FIFO is full, we don't know what to do (yet) so we just disregard the
// write for now.
//
void ProcessFifoEvent()
{
	if (vif0Regs.stat.FQC)
	{
		VIF_LOG("Draining VIF0 FIFO (FQC = %u)", vif0Regs.stat.FQC);
		uint remaining = vifTransfer<0>(g_fifo.vif0, vif0Regs.stat.FQC);

		if (remaining)
		{
			// VIF stalled during processing.  Copy remaining QWC to the head of the buffer.
			// For such small ring buffers as the PS2 FIFOs, its faster to use copies than to
			// use actual ringbuffer logic.

			memcpy_qwc(g_fifo.vif0, &g_fifo.vif0[vif0Regs.stat.FQC-remaining], remaining);
			vif0Regs.stat.FQC = remaining;
		}
	}

	if (vif1Regs.stat.FQC && !vif1Regs.stat.FDR)
	{
		// cannot drain the VIF1 FIFO when its in Source (toMemory) mode (FDR=1).  The EE has
		// to read from it or reset it manually.

		VIF_LOG("Draining VIF1 FIFO (FQC = %u)", vif1Regs.stat.FQC);
		uint remaining = vifTransfer<1>(g_fifo.vif1, vif1Regs.stat.FQC);

		if (remaining)
		{
			// VIF stalled during processing.  Copy remaining QWC to the head of the buffer.
			// For such small ring buffers as the PS2 FIFOs, its faster to use copies than to
			// use actual ringbuffer logic.

			memcpy_qwc(g_fifo.vif1, &g_fifo.vif1[vif1Regs.stat.FQC-remaining], remaining);
			vif1Regs.stat.FQC = remaining;
		}
	}

	if (gifRegs.stat.FQC)
	{
		// GIF FIFO uses PATH3 for transfer (same as GIF DMA).
		// So if PATH3 cannot get arbitration rights to the GIF, then the FIFO cannot empty itself.

		GIF_LOG("Draining GIF FIFO (FQC = %u)", gifRegs.stat.FQC);

		uint remaining = GIF_UploadTag(g_fifo.gif, gifRegs.stat.FQC);

		if (remaining)
		{
			// GIF stalled during processing.  Copy remaining QWC to the head of the buffer.
			// For such small ring buffers as the PS2 FIFOs, its faster to use copies than to
			// use actual ringbuffer logic.

			memcpy_qwc(g_fifo.gif, &g_fifo.gif[gifRegs.stat.FQC-remaining], remaining);
			gifRegs.stat.FQC = remaining;
		}
	}
	
	// [TODO] : IPU FIFOs (but those need some work)
}

void __fastcall WriteFIFO_VIF0(const mem128_t* value)
{
	// Devs: please read the WriteFIFO Strategies topic above before modifying.

	pxAssume( !vif0dma.chcr.STR );
	VIF_LOG("WriteFIFO/VIF0 <- %ls (FQC=%u)", value->ToString().c_str(), vif0Regs.stat.FQC);

	if (vif0Regs.stat.FQC >= FifoSize_Vif0) return;

	CopyQWC(&g_fifo.vif0[vif0Regs.stat.FQC], value);
	++vif0Regs.stat.FQC;

	CPU_INT(FIFO_EVENT, 64);
}

void __fastcall WriteFIFO_VIF1(const mem128_t *value)
{
	// Devs: please read the WriteFIFO Strategies topic above before modifying.

	pxAssume( !vif1dma.chcr.STR );
	VIF_LOG("WriteFIFO/VIF1 <- %ls (FQC=%u)", value->ToString().c_str(), vif1Regs.stat.FQC);

	if (vif1Regs.stat.FQC >= FifoSize_Vif1) return;
	
	// Writes to VIF1 FIFO when its in Source mode (VIF->memory transfer direction) are
	// likely disregarded.
	if (vif1Regs.stat.FDR) return;

	CopyQWC(&g_fifo.vif1[vif1Regs.stat.FQC], value);
	++vif1Regs.stat.FQC;

	CPU_INT(FIFO_EVENT, 64);
}

void __fastcall WriteFIFO_GIF(const mem128_t *value)
{
	// Devs: please read the WriteFIFO Strategies topic above before modifying.

	// GIF FIFO uses PATH3 for transfer (same as GIF DMA).

	pxAssume( !gifdma.chcr.STR );
	GIF_LOG("WriteFIFO/GIF <- %ls (FQC=%u)", value->ToString().c_str(), gifRegs.stat.FQC);

	if (gifRegs.stat.FQC >= FifoSize_Gif) return;

	CopyQWC(&g_fifo.gif[gifRegs.stat.FQC], value);
	++gifRegs.stat.FQC;

	CPU_INT(FIFO_EVENT, 64);
}
