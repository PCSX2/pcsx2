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
#include "Vif.h"
#include "Gif.h"
#include "newVif.h"
#include "ps2/NewDmac.h"

#include "FiFo.inl"


__aligned16 VifProcessingUnit vifProc[2];

void vifReset()
{
	memzero(vif0Regs);
	memzero(vif1Regs);

	memzero(vifProc);

	#ifdef PCSX2_DEVBUILD
	vifProc[0].idx	= 0;
	vifProc[1].idx	= 1;
	#endif
}

void SaveStateBase::vifFreeze()
{
	FreezeTag("VIFunpack (VPU)");
	Freeze(vifProc);
}

#define vcase(reg)  case (idx ? VIF1_##reg : VIF0_##reg)

_vifT __fi u32 vifRead32(u32 mem)
{
	switch (mem) {
		vcase(STAT):
			ProcessFifoEvent();
			GetVifXregs.stat.FQC = idx ? g_fifo.vif1.qwc : g_fifo.vif0.qwc;
			return GetVifXregs.stat._u32;
	
		vcase(ROW0): return vifProc[idx].MaskRow._u32[0];
		vcase(ROW1): return vifProc[idx].MaskRow._u32[1];
		vcase(ROW2): return vifProc[idx].MaskRow._u32[2];
		vcase(ROW3): return vifProc[idx].MaskRow._u32[3];

		vcase(COL0): return vifProc[idx].MaskCol._u32[0];
		vcase(COL1): return vifProc[idx].MaskCol._u32[1];
		vcase(COL2): return vifProc[idx].MaskCol._u32[2];
		vcase(COL3): return vifProc[idx].MaskCol._u32[3];
	}

	return psHu32(mem);
}

// returns FALSE if no writeback is needed (or writeback is handled internally)
// returns TRUE if the caller should writeback the value to the eeHw register map.
_vifT __fi bool vifWrite32(u32 mem, u32 value) {

	VIFregisters&	vifXRegs	= GetVifXregs;

	switch (mem) {
		vcase(MARK):
			vifXRegs.stat.MRK = 0;
		break;

		vcase(FBRST):
		{
			// IMPORTANT:  VIF resets have *nothing* to do with VIF DMAs!!  Issuing a reset to
			// VIF while a DMA is in progress does not reset or modify the DMA status in any way.
			// (though technically it is probably an error to try to reset the VIF while a DMA is
			// in progress as it would yield indeterminate results, ie errors).

			tVIF_FBRST& fbrst = (tVIF_FBRST&)value;
	
			if (fbrst.RST)
			{
				// Docs specify specific flags of STAT that should be cleared, however all other
				// flags are implicitly cleared as part of the reset operation; so just clear the
				// whole mess.  Other regs below are indicated as being set to 0, and anything
				// else should either be unmodified or indeterminate.

				vifXRegs.stat.reset();
				vifXRegs.num	= 0;
				vifXRegs.mark	= 0;
				vifXRegs.err.reset();
				
				memzero(vifProc[idx]);
				
				if (idx)	g_fifo.vif1.Clear();
				else		g_fifo.vif0.Clear();
				
				// Clearing the VIF releases VIF-side PATH3 masking, GIF arbitration may be able
				// to resume transfer:
				GIF_ArbitratePaths();
			}

			if (fbrst.FBK)
			{
				// Forcebreak is mostly a VU command -- meant to break execution of the VU processor
				// associated with the VIF interface (0/1).  It is only be used for debugging
				// purposes since a VU microprogram cannot be safely resumed after ForceBreak.
				//  (translation: very likely no games use this)

				// FIXME: VUs currently do not support forcebreak.
				vifXRegs.stat.VFS = true;
			}
			
			if (fbrst.STP)
			{
				// Signals that the VIF should stop as soon as the current VIFcode finishes processing.
				// At the conclusion of every VIFcode, this bit is checked, and if it is 1, the VIF
				// stalls until the app cancels the stall.
				vifXRegs.stat.VSS = true;
			}
			
			if (fbrst.STC)
			{
				// Cancel VIF stall, clear stall/error flags, and request DMA transfer resume if
				// a DMA is pending:

				if (vifXRegs.IsStalled())
				{
					dmacRequestSlice( idx ? EE_DMAC::ChanId_VIF1 : EE_DMAC::ChanId_VIF0);
				}

			}
			return false;
		}

		vcase(STAT):
		{
			if (!idx) return false;

			// VIF1's FDR bit controls the VIF1 FIFO direction, and is the only writable bit
			// in STAT.  VIF0 has no writable bits, so writes to it are disregarded.
			
			// Note that transferring data from GS to host (EE) is a complex task that requires
			// an app to completely clear out all GIF and GIFpath activity, switch GS trans-
			// mission direction, and switch VIF transmission direction.  The GIF/VIF direction
			// changes can be done in any order; if the VIF is switched first the DMA will
			// stall until the GS starts filling its FIFO.  If the GS is switched first, its
			// transfer stalls until the VIF starts draining its FIFO.

			tVIF_STAT& stat = (tVIF_STAT&)value;
			vifXRegs.stat.FDR = stat.FDR;
			return false;
		}

		vcase(ERR):
		vcase(MODE):
			// standard register writes -- handled by caller.
			break;

		vcase(ROW0): vifProc[idx].MaskRow._u32[0] = value; return false;
		vcase(ROW1): vifProc[idx].MaskRow._u32[1] = value; return false;
		vcase(ROW2): vifProc[idx].MaskRow._u32[2] = value; return false;
		vcase(ROW3): vifProc[idx].MaskRow._u32[3] = value; return false;

		vcase(COL0): vifProc[idx].MaskCol._u32[0] = value; return false;
		vcase(COL1): vifProc[idx].MaskCol._u32[1] = value; return false;
		vcase(COL2): vifProc[idx].MaskCol._u32[2] = value; return false;
		vcase(COL3): vifProc[idx].MaskCol._u32[3] = value; return false;
	}

	// fall-through case: issue standard writeback behavior.
	return true;
}

// Processes as many commands in the packet as possible.
_vifT static size_t vifTransferLoop(const u128* data, uint size_qwc) {
}

// size should always be a multiple of 128 bits (16 bytes) [assertion checked]
// Returns the number of QWC not processed (non-zero typically means the VIF stalled
// due to IRQ or MARK).
_vifT size_t vifTransfer(const u128* data, int size_qwc) {
	VifProcessingUnit&	vifX	= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;

	pxAssume(size_qwc != 0);

	vifX.fragment_size	= (size_qwc * 4) - vifX.stallpos;
	vifX.data			= (u32*)data + vifX.stallpos;

	while (vifX.fragment_size && !regs.stat.IsStalled())
	{
		if (regs.stat.VPS == VPS_IDLE)
		{
			// Fetch and dispatch a new VIFcode.
			regs.code = *vifX.data;
			++vifX.data;
			--vifX.fragment_size;
		}

		vifCmdHandler[idx][regs.code.CMD]();

		if (regs.stat.VPS != VPS_IDLE)
		{
			// Command was unable to finish processing, meaning it needs more data or that
			// some blocking/stall condition has occurred.  Break execution without checking
			// iBit or mask status (since those only trigger once the command has completed).

			break;
		}

		// Code processed in complete, so check the I-bit status:
		//  * i-bit stalls on all instructions *except* mark.  Use Max Payne for testing (it issues
		//    a series of MARK/NOP/MARK instructions)

		if (regs.code.IBIT && !regs.err.MII)
		{
			VIF_LOG("I-Bit IRQ raised (unmasked)");
			regs.stat.INT = 1;
			hwIntcIrq(idx ? INTC_VIF1 : INTC_VIF0);

			if (regs.code.CMD != VIFcode_MARK)
				regs.stat.VIS = 1;
		}
	}
	
	// Return the amount of data not processed as a function of QWC.  If the VIF stalled mid-
	// transfer, record the stall position and don't process the current QWC until later.

	vifX.stallpos = (4 - (vifX.fragment_size & 0x03)) & 0x03;
	return (vifX.fragment_size+3) / 4;
}

template size_t vifTransfer<0>(const u128 *data, int size);
template size_t vifTransfer<1>(const u128 *data, int size);

template u32 vifRead32<0>(u32 mem);
template u32 vifRead32<1>(u32 mem);

template bool vifWrite32<0>(u32 mem, u32 value);
template bool vifWrite32<1>(u32 mem, u32 value);

uint __dmacall EE_DMAC::fromVIF1(u128* destBase, uint destSize, uint destStartQwc, uint lenQwc)
{
	// Note: The only safe way to download data from the GS is to perform a full sync/flush
	// of the GS ringbuffer.  It's slow but absolutely unavoidable.  Fortunately few games
	// rely on this feature (its quite slow on the PS2 as well for the same reason).

	GetMTGS().WaitGS();

	if (destSize)
	{
		// destination buffer has a size -- so wrapped transfer logic needed:

		uint fullLen = lenQwc;
		uint firstcopylen = destSize - destStartQwc;
		uint remainder = GSreadFIFO2((u64*)(destBase+destStartQwc), firstcopylen);

		lenQwc -= (firstcopylen - remainder);

		if (lenQwc)
		{
			lenQwc = GSreadFIFO2((u64*)destBase, firstcopylen);
		}

		return fullLen - lenQwc;
	}
	else
	{
		uint remainder = GSreadFIFO2((u64*)(destBase+destStartQwc), lenQwc);
		return lenQwc - remainder;
	}
}


// Returns the number of QWC actually transferred  (confusing because VIF functions return the # of QWC
// *not* transferred).
uint __dmacall EE_DMAC::toVIF0	(const u128* srcBase, uint srcSize, uint srcStartQwc, uint lenQwc)
{
	// [TODO] : Optimization...
	// VIF may be better to have built in wrapping logic similar to GIF.

	uint endpos = srcStartQwc + lenQwc;

	if (srcSize == 0 || endpos < srcSize)
	{
		uint remainder = vifTransfer<0>(srcBase+srcStartQwc, lenQwc);
		return lenQwc - remainder;
	}
	else
	{
		uint fullLen = lenQwc;
		uint firstcopylen = srcSize - srcStartQwc;
		uint remainder = vifTransfer<0>(srcBase+srcStartQwc, firstcopylen);

		lenQwc -= (firstcopylen - remainder);

		if (lenQwc)
		{
			lenQwc = vifTransfer<0>(srcBase, lenQwc);
		}

		return fullLen - lenQwc;
	}
}

uint __dmacall EE_DMAC::toVIF1	(const u128* srcBase, uint srcSize, uint srcStartQwc, uint lenQwc)
{
	uint endpos = srcStartQwc + lenQwc;

	if (srcSize == 0 || endpos < srcSize)
	{
		uint remainder = vifTransfer<1>(srcBase+srcStartQwc, lenQwc);
		return lenQwc - remainder;
	}
	else
	{
		uint fullLen = lenQwc;
		uint firstcopylen = srcSize - srcStartQwc;
		uint remainder = vifTransfer<1>(srcBase+srcStartQwc, firstcopylen);

		lenQwc -= (firstcopylen - remainder);

		if (lenQwc)
		{
			lenQwc = vifTransfer<1>(srcBase, lenQwc);
		}

		return fullLen - lenQwc;
	}
}
