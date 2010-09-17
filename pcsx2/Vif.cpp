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
#include "newVif.h"
#include "ps2/NewDmac.h"

__aligned16 VifProcessingUnit vifProc[2];

void vifReset()
{
	memzero(vif0Regs);
	memzero(vif1Regs);

	memzero(vifProc);

	vifProc[0].idx	= 0;
	vifProc[1].idx	= 1;
}

void SaveStateBase::vifFreeze()
{
	FreezeTag("VIFunpack (VPU)");
	Freeze(vifProc);
}

#define vcase(reg)  case (idx ? VIF1_##reg : VIF0_##reg)

_vifT __fi u32 vifRead32(u32 mem) {

	switch (mem) {
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
			VIF_LOG("VIF%d_MARK write32 0x%8.8x", idx, value);
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
					dmacRequestXfer( idx ? EE_DMAC::ChanId_VIF1 : EE_DMAC::ChanId_VIF0);
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

	pxAssumeMsg( (size_qwc & ~15) == 0, "VIFcode transfer size is not a multiple of QWC." );

	vifX.fragment_size	= size_qwc * 4;
	vifX.data			= (u32*)data;

	do {
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
			// some blocking/stall condition has occurred.

			break;
		}

		// Code processed in complete, so check the I-bit status.

		// Okay did some testing with Max Payne, it does this:
		// VifMark  value = 0x666   (i know, evil!)
		// NOP with I Bit
		// VifMark  value = 0
		//
		// If you break after the 2nd Mark has run, the game reports invalid mark 0 and the game dies.
		// So it has to occur here, testing a theory that it only doesn't stall if the command with
		// the iBit IS mark, but still sends the IRQ to let the cpu know the mark is there. (Refraction)
		//
		// --------------------------
		//
		// This is how it probably works: i-bit sets the IRQ flag, and VIF keeps running until it encounters
		// a non-MARK instruction.  This includes the *current* instruction.  ie, execution only continues
		// unimpeded if MARK[i] is specified, and keeps executing unimpeded until any non-MARK command.
		// Any other command with an I bit should stall immediately.
		// Example:
		//
		// VifMark[i] value = 0x321   (with I bit)
		// VifMark    value = 0
		// VifMark    value = 0x333
		// NOP
		//
		// ... the VIF should not stall and raise the interrupt until after the NOP is processed.
		// So the final value for MARK as the game sees it will be 0x333. --air

		if (regs.code.IBIT && !regs.err.MII)
		{
			VifCodeLog("I-Bit IRQ raised (unmasked)");
			regs.stat.INT = 1;
			hwIntcIrq(idx ? INTC_VIF1 : INTC_VIF0);
		}

		// As per understanding outlined above: Stall VIF only if the current instruction is not MARK.
		if (regs.stat.INT && (regs.code.CMD != VIFcode_MARK))
			regs.stat.VIS = 1;

	} while (vifX.fragment_size && !regs.stat.IsStalled());
	
	// Return the amount of data not processed as a function of QWC.
	// (data not processed should always be 128 bit/qwc-aligned).

	pxAssumeDev( (vifX.fragment_size & 0x03) == 0, "Misaligned size after VIFcode stall." );
	return vifX.fragment_size / 4;
}

template size_t vifTransfer<0>(const u128 *data, int size);
template size_t vifTransfer<1>(const u128 *data, int size);

template u32 vifRead32<0>(u32 mem);
template u32 vifRead32<1>(u32 mem);

template bool vifWrite32<0>(u32 mem, u32 value);
template bool vifWrite32<1>(u32 mem, u32 value);
