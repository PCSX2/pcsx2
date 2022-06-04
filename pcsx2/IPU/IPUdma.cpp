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
#include "IPU.h"
#include "IPU/IPUdma.h"
#include "mpeg2lib/Mpeg.h"

IPUStatus IPU1Status;
bool CommandExecuteQueued;

void ipuDmaReset()
{
	IPU1Status.InProgress	= false;
	IPU1Status.DMAFinished	= true;
	CommandExecuteQueued	= false;
}

void SaveStateBase::ipuDmaFreeze()
{
	FreezeTag( "IPUdma" );
	Freeze(IPU1Status);
	Freeze(CommandExecuteQueued);
}

static __fi int IPU1chain() {

	int totalqwc = 0;

	int qwc = ipu1ch.qwc;
	u32 *pMem;

	pMem = (u32*)dmaGetAddr(ipu1ch.madr, false);

	if (pMem == NULL)
	{
		Console.Error("ipu1dma NULL!");
		return totalqwc;
	}

	//Write our data to the fifo
	qwc = ipu_fifo.in.write(pMem, qwc);
	ipu1ch.madr += qwc << 4;
	ipu1ch.qwc -= qwc;
	totalqwc += qwc;

	//Update TADR etc
	hwDmacSrcTadrInc(ipu1ch);

	if (!ipu1ch.qwc)
		IPU1Status.InProgress = false;

	return totalqwc;
}

void IPU1dma()
{
	int ipu1cycles = 0;
	int totalqwc = 0;

	if(!ipu1ch.chcr.STR || ipu1ch.chcr.MOD == 2)
	{
		//We MUST stop the IPU from trying to fill the FIFO with more data if the DMA has been suspended
		//if we don't, we risk causing the data to go out of sync with the fifo and we end up losing some!
		//This is true for Dragons Quest 8 and probably others which suspend the DMA.
		DevCon.Warning("IPU1 running when IPU1 DMA disabled! CHCR %x QWC %x", ipu1ch.chcr._u32, ipu1ch.qwc);
		return;
	}

	if (IPU1Status.DataRequested == false)
	{
		// IPU isn't expecting any data, so put it in to wait mode.
		cpuRegs.eCycle[4] = 0x9999;
		return;
	}

	IPU_LOG("IPU1 DMA Called QWC %x Finished %d In Progress %d tadr %x", ipu1ch.qwc, IPU1Status.DMAFinished, IPU1Status.InProgress, ipu1ch.tadr);
	if (!IPU1Status.InProgress)
	{
		if (IPU1Status.DMAFinished)
			DevCon.Warning("IPU1 DMA Somehow reading tag when finished??");

		tDMA_TAG* ptag = dmaGetAddr(ipu1ch.tadr, false);  //Set memory pointer to TADR

		if (!ipu1ch.transfer("IPU1", ptag))
		{
			return;
		}
		ipu1ch.madr = ptag[1]._u32;

		ipu1cycles += 1; // Add 1 cycles from the QW read for the tag

		if (ipu1ch.chcr.TTE) DevCon.Warning("TTE?");

		IPU1Status.DMAFinished = hwDmacSrcChain(ipu1ch, ptag->ID);

		IPU_LOG("dmaIPU1 dmaChain %8.8x_%8.8x size=%d, addr=%lx, fifosize=%x",
			ptag[1]._u32, ptag[0]._u32, ipu1ch.qwc, ipu1ch.madr, 8 - g_BP.IFC);

		if (ipu1ch.chcr.TIE && ptag->IRQ) //Tag Interrupt is set, so schedule the end/interrupt
			IPU1Status.DMAFinished = true;

		if (ipu1ch.qwc)
			IPU1Status.InProgress = true;
	}

	if (IPU1Status.InProgress)
		totalqwc += IPU1chain();

	//Do this here to prevent double settings on Chain DMA's
	if(totalqwc == 0 || (IPU1Status.DMAFinished && !IPU1Status.InProgress))
	{
		totalqwc = std::max(4, totalqwc);
		IPU_INT_TO(totalqwc * BIAS);
	}
	else
	{
		IPU1Status.DataRequested = false;

		if (!(IPU1Status.DMAFinished && !IPU1Status.InProgress))
		{
			cpuRegs.eCycle[4] = 0x9999;//IPU_INT_TO(2048);
		}
		else
		{
			IPU_INT_TO(totalqwc * BIAS);
		}
	}


	CommandExecuteQueued = true;
	CPU_INT(IPU_PROCESS, totalqwc * BIAS);

	IPU_LOG("Completed Call IPU1 DMA QWC Remaining %x Finished %d In Progress %d tadr %x", ipu1ch.qwc, IPU1Status.DMAFinished, IPU1Status.InProgress, ipu1ch.tadr);
}

void IPU0dma()
{
	if(!ipuRegs.ctrl.OFC)
	{
		if(!CommandExecuteQueued)
			IPUProcessInterrupt();
		return;
	}

	int readsize;
	tDMA_TAG* pMem;

	if ((!(ipu0ch.chcr.STR) || (cpuRegs.interrupt & (1 << DMAC_FROM_IPU))) || (ipu0ch.qwc == 0))
	{
		DevCon.Warning("How??");
		return;
	}

	pxAssert(!(ipu0ch.chcr.TTE));

	IPU_LOG("dmaIPU0 chcr = %lx, madr = %lx, qwc  = %lx",
	        ipu0ch.chcr._u32, ipu0ch.madr, ipu0ch.qwc);

	pxAssert(ipu0ch.chcr.MOD == NORMAL_MODE);

	pMem = dmaGetAddr(ipu0ch.madr, true);

	readsize = std::min(ipu0ch.qwc, (u32)ipuRegs.ctrl.OFC);
	ipu_fifo.out.read(pMem, readsize);

	ipu0ch.madr += readsize << 4;
	ipu0ch.qwc -= readsize;
	
	if (dmacRegs.ctrl.STS == STS_fromIPU)   // STS == fromIPU
	{
		//DevCon.Warning("fromIPU Stall Control");
		dmacRegs.stadr.ADDR = ipu0ch.madr;
	}

	IPU_INT_FROM( readsize * BIAS );

	if (ipu0ch.qwc > 0 && !CommandExecuteQueued)
	{
		CommandExecuteQueued = true;
		CPU_INT(IPU_PROCESS, 4);
	}
}

__fi void dmaIPU0() // fromIPU
{
	//if (dmacRegs.ctrl.STS == STS_fromIPU) DevCon.Warning("DMA Stall enabled on IPU0");

	if (dmacRegs.ctrl.STS == STS_fromIPU)   // STS == fromIPU - Initial settings
		dmacRegs.stadr.ADDR = ipu0ch.madr;

	// Note: This should probably be a very small value, however anything lower than this will break Mana Khemia
	// This is because the game sends bad DMA information, starts an IDEC, then sets it to the correct values
	// but because our IPU is too quick, it messes up the sync between the DMA and IPU.
	// So this will do until (if) we sort the timing out of IPU, shouldn't cause any problems for games for now.
	//IPU_INT_FROM( 160 );
	// Update 22/12/2021 - Doesn't seem to need this now after fixing some FIFO/DMA behaviour
	IPU0dma();

	// Explanation of this:
	// The DMA logic on a NORMAL transfer is generally a "transfer first, ask questions later" so when it's sent
	// QWC == 0 (which we change to 0x10000) it transfers, causing an underflow, then asks if it's reached 0
	// since IPU_FROM is beholden to the OUT FIFO, if there's nothing to transfer, it will stay at 0 and won't underflow
	// so the DMA will end.
	if (ipu0ch.qwc == 0x10000)
	{
		ipu0ch.qwc = 0;
		ipu0ch.chcr.STR = false;
		hwDmacIrq(DMAC_FROM_IPU);
		DMA_LOG("IPU0 DMA End");
	}
}

__fi void dmaIPU1() // toIPU
{
	IPU_LOG("IPU1DMAStart QWC %x, MADR %x, CHCR %x, TADR %x", ipu1ch.qwc, ipu1ch.madr, ipu1ch.chcr._u32, ipu1ch.tadr);

	if (ipu1ch.chcr.MOD == CHAIN_MODE)  //Chain Mode
	{
		IPU_LOG("Setting up IPU1 Chain mode");
		if(ipu1ch.qwc == 0)
		{
			IPU1Status.InProgress = false;
			IPU1Status.DMAFinished = false;
		}
		else // Attempting to continue a previous chain
		{
			IPU_LOG("Resuming DMA TAG %x", (ipu1ch.chcr.TAG >> 12));
			IPU1Status.InProgress = true;
			if ((ipu1ch.chcr.tag().ID == TAG_REFE) || (ipu1ch.chcr.tag().ID == TAG_END) || (ipu1ch.chcr.tag().IRQ && ipu1ch.chcr.TIE))
			{
				IPU1Status.DMAFinished = true;
			}
			else
			{
				IPU1Status.DMAFinished = false;
			}
		}

		if(IPU1Status.DataRequested)
			IPU1dma();
		else
			cpuRegs.eCycle[4] = 0x9999;
	}
	else // Normal Mode
	{
			IPU_LOG("Setting up IPU1 Normal mode");
			IPU1Status.InProgress = true;
			IPU1Status.DMAFinished = true;

			if (IPU1Status.DataRequested)
				IPU1dma();
			else
				cpuRegs.eCycle[4] = 0x9999;
	}
}

void ipuCMDProcess()
{
	CommandExecuteQueued = false;
	IPUProcessInterrupt();
}

void ipu0Interrupt()
{
	IPU_LOG("ipu0Interrupt: %x", cpuRegs.cycle);

	if(ipu0ch.qwc > 0)
	{
		IPU0dma();
		return;
	}

	ipu0ch.chcr.STR = false;
	hwDmacIrq(DMAC_FROM_IPU);
	DMA_LOG("IPU0 DMA End");
}

__fi void ipu1Interrupt()
{
	IPU_LOG("ipu1Interrupt %x:", cpuRegs.cycle);

	if(!IPU1Status.DMAFinished || IPU1Status.InProgress)  //Sanity Check
	{
		IPU1dma();
		return;
	}

	DMA_LOG("IPU1 DMA End");
	ipu1ch.chcr.STR = false;
	hwDmacIrq(DMAC_TO_IPU);
}
