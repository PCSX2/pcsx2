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

#include "Hardware.h"

enum DMA_DirectionMode
{
	// Indicates a DMA that transfers from peripheral to memory
	DmaDir_Source,
	
	// Indicates a DMA that transfers from peripheral to memory
	DmaDir_Drain,
	
	// Indicates a DAM that bases its transfer direction on the DIR bit of its CHCR register.
	DmaDir_Both,
};

struct DmaChannelInformation
{
	const wxChar*	name;

	uint			regbaseaddr;
	
	DMA_DirectionMode direction;

	bool			hasDrainStall;
	bool			hasSourceChain;
	bool			hasDestChain;
	bool			hasAddressStack;

	tDMA_CHCR& GetCHCR() const
	{
		return (tDMA_CHCR&)PS2MEM_HW[regbaseaddr];
	}

	tDMA_MADR& GetMADR() const
	{
		return (tDMA_MADR&)PS2MEM_HW[regbaseaddr+0x10];
	}

	tDMA_QWC& GetQWC() const
	{
		return (tDMA_QWC&)PS2MEM_HW[regbaseaddr+0x20];
	}
	
	tDMA_TADR& GetTADR() const
	{
		pxAssert(hasSourceChain || hasDestChain);
		return (tDMA_TADR&)PS2MEM_HW[regbaseaddr+0x30];
	}

	tDMA_ASR& GetASR0() const
	{
		pxAssert(hasAddressStack);
		return (tDMA_ASR&)PS2MEM_HW[regbaseaddr+0x40];
	}

	tDMA_ASR& GetASR1() const
	{
		pxAssert(hasAddressStack);
		return (tDMA_ASR&)PS2MEM_HW[regbaseaddr+0x50];
	}
};

static const DmaChannelInformation DmaChan[] =
{
	//				baseaddr	Direction		Drain	Src/DstChain	A.S.
	{ L"VIF0",		D0_CHCR,	DmaDir_Both,	false,	true,	false,	true,	},
	{ L"VIF1",		D1_CHCR,	DmaDir_Drain,	true,	true,	false,	true,	},
	{ L"GIF",		D2_CHCR,	DmaDir_Drain,	true,	true,	false,	true,	},
	{ L"fromIPU",	D3_CHCR,	DmaDir_Source,	false,	false,	false,	false,	},
	{ L"toIPU",		D4_CHCR,	DmaDir_Drain,	false,	true,	false,	false,	},
	{ L"SIF0",		D5_CHCR,	DmaDir_Source,	false,	false,	true,	false,	},
	{ L"SIF1",		D6_CHCR,	DmaDir_Drain,	true,	true,	false,	false,	},
	{ L"SIF2",		D7_CHCR,	DmaDir_Both,	false,	false,	false,	false,	},
	{ L"fromSPR",	D8_CHCR,	DmaDir_Source,	false,	false,	true,	false,	},
	{ L"toSPR",		D9_CHCR,	DmaDir_Drain,	false,	true,	false,	false,	},

	// A.S  -- Has Address Stack
};


static const wxChar* StallSrcNames[] =
{
	L"None", L"SIF0(5)", L"fromSPR(3)", L"fromIPU(8)"
};

static const wxChar* StallDrainNames[] =
{
	L"None", L"VIF1(1)", L"SIF1(6)", L"GIF(2)"
};

static const wxChar* MfifoDrainNames[] =
{
	L"None", L"Reserved", L"VIF1(1)", L"GIF(2)"
};


static const uint ChannelCount = ArraySize(DmaChan);

// Strict DMA emulation actually requires the DMAC event be run on every other CPU
// cycle, and thus will only be available with interpreters.
static const bool UseStrictDmaTiming = false;

// when enabled, the DMAC bursts through all active and pending DMA transfers that it can
// in each IRQ call.  Ie, it continues to rpocess and update DMA transfers and chains until
// a stall condition or interrupt request forces the DMAC to stop execution; or until all
// DMAs are completed.
static const bool UseDmaBurstHack = true;

/* DMAC Hardware Functionality:

Generally speaking, the actual hardware emulation of the DMAC isn't going to be important.
This is because the DMAC is a multi-peripheral interface with FIFOs attached to each
peripheral, and the EE is a multihtreaded environment with exception and interrupt handlers.
So in order to do safe DMA arbitration, programs can make few assumptions about how long
a DMA takes to complete or about DMA status even after starting a transfer immediately.
However, understanding the DMAC hardware can help understand what games are trying to do
with their DMAC registers.

The DMAC works by loading/draining FIFOs attached to each peripheral from/to main memory
using a BURST transfer.  That is, Main Memory -> FIFO is a burst, and then the peripheral
drains the FIFO at its internal bus speed.

Most peripherals (except SPR) operate at half the main bus speed, and incur additional
penalties depending on what's being fed to/from them (the gs for example has several commands
that have 1 cycle penalties).  The SIF transfers are especially slow since the IOP DMAs are
a mere 1/16th fraction the speed of the EE's.  Thus, in order to maximize bus speed its
*always* best to have two or more DMAs running concurrently.  The DMAC will slice between
the DMAs accordingly, filling and draining FIFOs as needed, all while the FIFOs are being
filled/drained by the attached peripherals *in parallel.*

SPR is the Exception!  SPR DMAs can max out the DMAC bandwidth, and as such the toSPR and
fromSPR DMAs are always BURST mode for the entire duration of their transfers, since the
DMAC would have no performance benefit from slicing them with other FIFO-based DMA transfers.

*/

static bool IsSprChannel( uint cid )
{
	return (cid == 8) || (cid == 9);
}

uint round_robin = 1;

// Returns the index of the next DMA channel granted bus rights.
static uint ArbitrateBusRight()
{
	//  * VIF0 has top priority.
	//  * SIF2 has secondary priority.
	//  * All other channels are managed in cyclic arbitration (round robin).

	wxString ActiveDmaMsg;

	const tDMAC_PCR& pcr = (tDMAC_PCR&)PS2MEM_HW[DMAC_PCR];

	// VIF0 is the highest of the high priorities!!
	const tDMA_CHCR& vif0chcr = DmaChan[0].GetCHCR();
	if (vif0chcr.STR)
	{
		if (!pcr.PCE || (pcr.CDE & 2)) return 0;
		DMA_LOG("\tVIF0 bypassed due to PCE/CDE0 condition.");
	}

	// SIF2 is next!!
	const tDMA_CHCR& sif2chcr = DmaChan[7].GetCHCR();
	if (vif0chcr.STR)
	{
		if (!pcr.PCE || (pcr.CDE & 2)) return 0;
		DMA_LOG("\tSIF2 bypassed due to PCE/CDE0 condition.");
	}

	for (uint lopi=0; lopi<ChannelCount; ++lopi)
	{
		round_robin = round_robin+1;
		if (round_robin >= ChannelCount) round_robin = 1;

		const tDMA_CHCR& chcr = DmaChan[round_robin].GetCHCR();
		if (!chcr.STR) continue;
		
		if (pcr.PCE && !(pcr.CDE & (1<<round_robin)))
		{
			DMA_LOG("\t%s bypassed due to PCE/CDE%d condition", DmaChan[round_robin].name, round_robin);
			continue;
		}

		if (DmaChan[round_robin].hasDrainStall)
		{
			// this channel supports drain stalling.  If the stall condition is already met
			// then we need to skip it by and try another channel.
			
			const tDMAC_STADR& stadr = (tDMAC_STADR&)PS2MEM_HW[DMAC_STADR];

			// Unknown: Should stall comparisons factor the SPR bit, ignore SPR bit, or base the
			// comparison on the translated physical address? Implied behavior seems to be that
			// it ignores the SPR bit.
			if ((DmaChan[round_robin].GetMADR().ADDR + 8) >= stadr.ADDR)
			{
				DMA_LOG("\t%s bypassed due to DRAIN STALL condition (D%d_MADR=0x%08x, STADR=0x%08x",
					DmaChan[round_robin].name, DmaChan[round_robin].GetMADR().ADDR, stadr.ADDR);

				continue;
			}
		}

		if (UseStrictDmaTiming)
		{
			// [TODO] Strict DMA Timings!
			// In strict mode each DMA channel has built-in timers that monitor their FIFO drain
			// rates (actual DMA channel FIFOs are not emulated, only their busrt copy size is
			// enforced).  When a peripheral FIFO is deemed full or empty (depending on src/drain
			// flag), the DMA is available for selection.
		}

		return round_robin;
	}

	return -1;
}


void eeEvt_UpdateDmac()
{
	tDMAC_CTRL& dmactrl = (tDMAC_CTRL&)PS2MEM_HW[DMAC_CTRL];

	DMA_LOG("(UpdateDMAC Event) D_CTRL=0x%08X", dmactrl._u32);

	/* DMAC_ENABLER / DMAC_ENABLEW
	
	These registers regulate the master DMAC transfer status for all DMAs.  On the real hardware
	it acts as an immediate stoppage of all DMA activity, so that all hardware register accesses
	by DMAC are suspended and the EE is allowed to safely write to DMA registers, without
	the possibility of DMAC register writebacks interfering with results (DMAC and EE are
	concurrent processes, and if both access a register at the same time, either of their
	writes could be ignored and result in rather unpredictable behavior).

	Chances are, the real hardware uses the upper 16 bits of this register to store some status
	info needed to safely resume the DMA transfer where it left off.  It may use the bottom
	bits as well, or they may be some sort of safety check ID.  PS2 BIOs seems to expect the
	bottom 16 bits to always be 0x1201, for example.
	
	In any case, our DMAC is *not* a concurrent process, and all our status vars are already
	in order (mostly stored in DMA registers) so there isn't much we need to do except adhere
	to the program's wishes and suspend all transfers. :)
	
	Note: ENABLEW is a write-to reg only, and it automatically updates DMAC_ENABLER when handled
	from the indirect memop handlers.
	*/

	if ((psHu32(DMAC_ENABLER) & 0x10000) || dmactrl.DMAE)
	{
		// Do not reschedule the event.  The indirect HW reg handler will reschedule it when
		// the DMAC register(s) are written and the DMAC is fully re-enabled.
		DMA_LOG("DMAC disabled, no actions performed. (DMAE=%d, ENABLER=0x%08x", dmactrl.DMAE, psHu32(DMAC_ENABLER));
		return;
	}


	do
	{
		int chanId = ArbitrateBusRight();
		
		if (chanId == -1)
		{
			// Do not reschedule the event.  The indirect HW reg handler will reschedule it when
			// the STR bits are written and the DMAC is enabled.

			DMA_LOG("DMAC Arbitration complete.");
			break;
		}

		const DmaChannelInformation& chan = DmaChan[chanId];

		tDMA_CHCR& chcr	= chan.GetCHCR();
		tDMA_MADR& madr	= chan.GetMADR();
		tDMA_QWC& qwc	= chan.GetQWC();

		DMA_DirectionMode dir = chan.direction;

		const char* SrcDrainMsg = "";

		if (dir == DmaDir_Both)
		{
			dir = chcr.DIR ? DmaDir_Drain : DmaDir_Source;
			SrcDrainMsg = chcr.DIR ? "(Drain)" : "(Source)";
		}

		DMA_LOG("\tBus right granted to %s%s MADR=0x%08X QWC=0x%4x", chan.name, SrcDrainMsg, madr.ADDR, qwc.QWC);

		// Determine copyable length of this DMA.

		const tDMAC_STADR& stadr = (tDMAC_STADR&)PS2MEM_HW[DMAC_STADR];
		
		switch (chcr.MOD)
		{
			case NORMAL_MODE:
			{
				// By default DMAs don't wrap.  SPR and VU transfers do, however.
				uint wrapspot = 0;
				uint stallspot = 0;
				uint endaddr = madr.ADDR + qwc.QWC;		// estimated end address of the transfer.

				if (madr.SPR)
				{
					wrapspot = Ps2MemSize::Scratch;
				}
				else
				{
					if (madr.ADDR >= 0x70000000)
					{
						// It appears that games can access the SPR directly using 0x70000000, but
						// most just use the SPR bit.  What's unknown is if the direct mapping via
						// 0x70000000 affects stalling behavior or SPR memory wrap-around (behavior
						// for direct SPR mapping should match that of direct VU mappings, however).

						wrapspot = 0x70000000;
					}

					// toSPR/fromSPR have special behavior

					if ((madr.ADDR >= 0x11004000) && (madr.ADDR < 0x11010000))
					{
					
					}

					if (IsSprChannel(chanId))
					{

					}
				}

				if (wrapspot)
				{
					pxAssertDev(stadr.ADDR != wrapspot, "DMAC wrapspot logic error detected.");
					if ((stadr.ADDR >= madr.ADDR) && (stadr.ADDR < wrapspot))
					{
						// Transfer is set to stall prior to the wrap spot.  We can only
						// transfer up to the stall position.
						
						stallspot = stadr.ADDR-8;
					}
				}
				
				
			}
			break;
			
			case CHAIN_MODE:
			break;
			
			case INTERLEAVE_MODE:
				// Should only be valid for toSPR and fromSPR DMAs only.
				pxAssertDev( IsSprChannel(chanId), "DMAC: Interleave mode specified on Scratchpad channel!" );
			break;
		}
		
	} while (UseDmaBurstHack);


	//


	wxString StallSrcMsg = StallSrcNames[dmactrl.STS];
	wxString StallDrainMsg = StallDrainNames[dmactrl.STD];
	wxString MfifoDrainMsg = StallDrainNames[dmactrl.STD];
	
	// Furthermore, the SPR DMAs are "Burst" mode DMAs that behave very predictably
	// when the RELE bit is cleared (no cycle stealing): all other DMAs are stalled
	// and the SPR DMA transfers all its data in one shot.

	wxString CycStealMsg;
	if (dmactrl.RELE)
	{
		/* Cycle Stealing Enabled?

		This feature of the DMAC exists mainly for PS2 hardware performance reasons,
		though it is possible that a game could disable cycle stealing in a way to
		time the EE to dma activity:
		
		With cycle stealing disabled, any direct physical memory access by the EE 
		(anything not scratchpad and not a hardware register, basically) will cause
		the EE to stall until the entire DMA completes.  Thusly, a game *could* start
		a DMA, write or read physical memory, and then know with certainty that the
		very next EE instruction executed will be run only after the DMAC has completed
		its tasks.
		
		(unless the EE uses scratchpad for stackframe, this would mean the EE can't
		 respond to IRQs either until the transfer finishes -- hmm)
		*/
		
		CycStealMsg = wxsFormat(L"On/%d",8<<dmactrl.RCYC);
	}
	else
	{
		CycStealMsg = L"Off";
	}

	if (dmactrl.MFD)
	{
		/* Memory FIFO Drain Channel (MFIFO)
		
		The MFIFO is a provisioned alternative to stall control, for transferring data
		to/from SPR and VIF/GIF.  Since all three DMAs can only transfer to/from physical
		memory, connecting them requires a memory buffer.  Using stall control leads to
		lots of lost DMA cycles.  Using an MFIFO allows for the memory transfer bursts
		to be much larger and thusly much more efficient.
		 
		While that's all well and good, the actual likeliness of this process being important
		to the running application is slim.  Apps can, however, completely suspend the DMAC
		and read back the current SADR and TADR addresses of the MFIFO; perhaps for debugging
		or what-not.  Even in this case, apps should function fine (though not necessarily
		"as expected") so long as SADR and TADR are set to the same address in memory (see
		below).
		 
		Since the functionality is likely unneeded for games, support for MFIFO is implemented
		conditionally.  When disabled, transfers are passed directly from SPR to the per-
		ipheral, without having to write to some memory buffer as a go-between.  In case
		some games use the SADR/TADR progress for some sort of timing or logic, the SADR
		and TADR registers are updated at each chain segment, but always reflect a fully
		drained buffer.  (also necessary because they need to raise IRQs when drained).
		*/

	}
	
	if (dmactrl.STS)
	{
		/* Stall Control Source Channel
		
		This function must be be emulated at all times.  It essentially causes the DMAC to
		write the MADR of the specified channel to STADR.  While this isn't always needed
		by our own DMAC (which can avoid stalling by doing complete BURST style transfers
		for all unchained DMAs), apps could still rely on STADR for their own internal
		logic.
		*/
	}

	if (dmactrl.STD)
	{
		/* Stall Control Drain Channel
		
		The most important aspect of this function is that certain conditions can result in
		a DMA request being completely disregarded.  This value must also be respected when
		the DMAC is running in purist mode (slicing and arbitrating at 8 QWC boundaries).

		When DMAs are in Normal mode, using BURST transfers for all DMAs should eliminate
		any need for advanced stall control logic.  However, there *is* a potential cavet:
		when the drain channel is in Source Chain mode, stall control is apparently performed
		on each REFS tag; meaning that each chain of the DMA must perform stall checks and
		arbitration on the referenced address, which can jump all around memory.  
		 
		It is possible that BURSTing through entire chains will also negate the need for
		stall control logic.  But its also possible for an app to micro-manage the STADR
		register.
		*/
	}

	wxsFormat(L"[CycSteal:%s]", CycStealMsg);

	for( uint i=0; i<ChannelCount; ++i )
	{
		
	}
}