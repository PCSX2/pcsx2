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
#include "NewDmac.h"

#include "NewDmac_Tables.inl"
#include "NewDmac_ChainMode.inl"

static uint round_robin = 1;

using namespace EE_DMAC;

ControllerMetrics dmac_metrics;

// When the DMAC is operating in strict emulation mode, channels only receive arbitration rights
// when their dma_request flag is set.  This is an internal DMAC value that appears to have no
// equivalent hardware register on the PS2, and it is flagged TRUE under the following conditions:
//  * The channel has received a STR=1 command.
//  * The channel's accompanying FIFO has drained, and the channel has STR=1.
//    (GIF FIFO is 16 QWC, VIF FIFOs appear to be 8 QWC).
//
// [TODO] Fully implement strict DMA timing mode.
//
static bool dma_request[NumChannels];

bool EE_DMAC::ChannelState::TestArbitration()
{
	if (!chcr.STR) return false;

	if (UseStrictDmaTiming && !dma_request[round_robin])
	{
		// Strict DMA Timings!
		// In strict mode each DMA channel has built-in timers that monitor their FIFO drain
		// rates (actual DMA channel FIFOs are not emulated, only their burst copy size is
		// enforced, which is typically 16 QWC for GIF and 8 QWC for VIF and SIF).
		// When a peripheral deems its FIFO as full or empty (depending on src/drain flag),
		// the DMA flags a "DREQ" (DMA Request), and at that point it is ready for selection.

		return false;
	}

	// Metrics: It's easier to just assume it gets skipped, and decrement the counter
	// later if, in fact, it isn't skipped. ;)
	if (IsDevBuild) ++dmac_metrics.channel[round_robin].skipped_arbitrations;

	if (dmacRegs.pcr.PCE && !(dmacRegs.pcr.CDE & (1<<round_robin)))
	{
		DMA_LOG("\t%s bypassed due to PCE/CDE%d condition", info.NameA, round_robin);
		return false;
	}

	if (DrainStallActive())
	{
		// this channel has drain stalling enabled.  If the stall condition is already met
		// then we need to skip it by and try another channel.

		// Drain Stalling Rules:
		//  * We can copy data up to STADR (exclusive).
		//  * Arbitration should not granted until at least 8 QWC is available for copy.
		//  * If the source DMA doesn't round out at 8 QWC, then the drain channel will
		//    refuse to transfer a partial QWC and will stall indefinitely (until STR is
		//    manually cleared).
		//
		// The last two "features" are difficult to emulate, and are currently on the
		// [TODO] list.
					
		uint stallAt = dmacRegs.stadr.ADDR;
		uint endAt = madr.ADDR + 8*16;

		if (!madr.SPR)
		{
			if (endAt > stallAt)
			{
				DMAC_LOG("\t%s bypassed due to DRAIN STALL condition (D%d_MADR=0x%08x, STADR=0x%08x",
					info.NameA, madr.ADDR, dmacRegs.stadr.ADDR);

				return false;
			}
		}
		else if (stallAt < Ps2MemSize::Scratch)
		{
			// Assumptions:
			// SPR bit transfers most likely perform automatic memory wrapping/masking on MADR
			// and likely do not automatically mask/wrap STADR (both of these assertions
			// need proper test app confirmations!! -- air)

			if ((madr.ADDR < stallAt) && (endAt > stallAt))
			{
				DMAC_LOG("\t%s bypassed due to DRAIN STALL condition (D%d_MADR=%s, STADR=0x%08x)",
					info.NameA, madr.ToUTF8().data(), dmacRegs.stadr.ADDR);

				return false;
			}
			else
			{
				endAt &= (Ps2MemSize::Scratch-1);
				if ((madr.ADDR >= stallAt) && (endAt > stallAt))
				{
					DMAC_LOG("\t%s bypassed due to DRAIN STALL condition (D%d_MADR=%s, STADR=0x%08x) [SPR memory wrap!]",
						info.NameA, madr.ToUTF8().data(), dmacRegs.stadr.ADDR);

					return false;
				}
			}
		}
	}

	if (UseMFIFOHack && (Id == ChanId_fromSPR) && (dmacRegs.ctrl.MFD != NO_MFD) && (dmacRegs.ctrl.MFD != MFD_RESERVED))
	{
		// When the MFIFO hack is enabled, we ignore fromSPR's side of MFIFO.  VIF1
		// and GIF will drain directly from fromSPR when arbitration is passed to them.

		DMAC_LOG("fromSPR bypassed due to MFIFO/Hack condition.");
		return false;
	}

	if (IsDevBuild) --dmac_metrics.channel[Id].skipped_arbitrations;

	if (UseStrictDmaTiming) dma_request[Id] = false;

	return true;
}

// Returns the index of the next DMA channel granted bus rights.
static ChannelId ArbitrateBusRight()
{
	ControllerRegisters& dmacReg = (ControllerRegisters&)psHu8(DMAC_CTRL);

	//  * VIF0 has top priority.
	//  * SIF2 has secondary priority.
	//  * All other channels are managed in cyclic arbitration (round robin).

	wxString ActiveDmaMsg;

	// VIF0 is the highest of the high priorities!!
	const tDMA_CHCR& vif0chcr = ChannelInfo[ChanId_VIF0].CHCR();
	if (vif0chcr.STR)
	{
		if (!dmacRegs.pcr.PCE || (dmacRegs.pcr.CDE & 2)) return ChanId_VIF0;
		DMA_LOG("\tVIF0 bypassed due to PCE/CDE0 condition.");
		if (IsDevBuild) ++dmac_metrics.channel[ChanId_VIF0].skipped_arbitrations;
	}

	// SIF2 is next!!
	const tDMA_CHCR& sif2chcr = ChannelInfo[ChanId_SIF2].CHCR();
	if (sif2chcr.STR)
	{
		if (!dmacRegs.pcr.PCE || (dmacRegs.pcr.CDE & 2)) return ChanId_SIF2;
		DMA_LOG("\tSIF2 bypassed due to PCE/CDE0 condition.");
		if (IsDevBuild) ++dmac_metrics.channel[ChanId_SIF2].skipped_arbitrations;
	}

	// Everything else is handled round-robin style!
	for (uint lopi=0; lopi<NumChannels; ++lopi)
	{
		++round_robin;
		if (round_robin >= NumChannels) round_robin = 1;

		if (ChannelState( (ChannelId)round_robin ).TestArbitration())
			return (ChannelId)round_robin;
	}

	return ChanId_None;
}

void EE_DMAC::ChannelState::TransferInterleaveData()
{
	tDMA_SADR& sadr = info.SADR();

	// Interleave Data Transfer notes:
	//  * Interleave is allowed on toSPR and fromSPR only.
	//
	//  * SPR can transfer to itself.  The SPR bit is supposed to be ineffective, however
	//    directly addressing the SPR via 0x70000000 appears to work (but without applied
	//    wrapping logic).

	// Interleave should only be valid for toSPR and fromSPR DMAs only.  Fortunately
	// supporting them is trivial, so although I'm asserting on debug builds, all other
	// builds actually perform the interleaved memcpy to/from SPR ram (this just in case
	// the EE actually supports it in spite of being indicated otherwise).
	pxAssertMsg( info.isSprChannel, "DMAC: Interleave mode specified on Scratchpad channel!" );

	// Interleave should never be used in conjunction with MFIFO.  Most likely the Real
	// DMAC ignores MFIFO settings in this case, and performs a normal SPR<->Memory xfer.
	// (though its also possible the DMAC has an error starting the transfer or some such)
	if (IsDebugBuild)
		pxAssert( dmacRegs.ctrl.MFD <= MFD_RESERVED );
	else
		DevCon.Warning("(DMAC) MFIFO enabled during interleaved transfer (ignored!)");

	DMAC_LOG("\tTQWC=%u, SQWC=%u", dmacRegs.sqwc.TQWC, dmacRegs.sqwc.SQWC );
	pxAssumeDev( (creg.qwc.QWC % dmacRegs.sqwc.TQWC) == 0, "(DMAC INTERLEAVE) QWC is not evenly divisible by TQWC!" );

	uint tqwc = dmacRegs.sqwc.TQWC;
	if (!pxAssert(tqwc!=0))
	{
		// The old PCSX2 DMA code treats a zero-length TQWC as "copy a single row" --
		// TQWC is assumed to be the channel's QWC, and SQWC is applied normally afterward.
		// There doesn't appear to be any indication of this behavior, and the old DMA
		// code does not cite any specific game(s) this fixes.  Hmm!  --air

		tqwc = creg.qwc.QWC;
	}

	if (IsDevBuild && UseDmaBurstHack)
	{
		// Sanity Check:  The burst hack assumes that the Memory side of the transfer
		// will always be within the same mappable page of ram, and that it won't cross
		// some oddball boundary or spill over into unmapped ram.
		
		uint add = (tqwc + dmacRegs.sqwc.SQWC);

		tDMAC_ADDR endaddr = madr;
		endaddr.ADDR += add * 16;

		u128* startmem	= DMAC_GetHostPtr(madr, false);
		u128* endmem	= DMAC_TryGetHostPtr(endaddr, false);
		pxAssertDev( (endmem != NULL) && ((startmem+add) == endmem),
			"(DMAC) Physical memory cross-boundary violation detected on SPR INTERLEAVE transfer!"
		);
	}

	// The following interleave code is optimized for burst hack transfers, which
	// transfer all interleaved data at once; which is why we set up some pre-loop
	// variables and then flush them back to DMAC registers when the transfer is complete.

	uint curqwc = creg.qwc.QWC;
	uint addrtmp = sadr.ADDR / 16;

	if (GetDir() == Dir_Source)
	{
		// fromSPR -> Xfer from SPR to memory.

		u128* writeTo = DMAC_GetHostPtr(madr, true);

		do {

			MemCopy_WrappedSrc(
				(u128*)eeMem->Scratch, addrtmp,
				Ps2MemSize::Scratch/16, writeTo, tqwc
			);
			writeTo	+= tqwc + dmacRegs.sqwc.SQWC;
			curqwc	-= tqwc;
			addrtmp	+= dmacRegs.sqwc.SQWC;
			addrtmp	&= (Ps2MemSize::Scratch / 16) - 1;
		} while(UseDmaBurstHack && curqwc);

		if(dmacRegs.ctrl.STS == STS_fromSPR)
		{
			DMAC_LOG("\tUpdated STADR=%s (prev=%s)", madr.ToUTF8(false), dmacRegs.stadr.ToUTF8(false));
			dmacRegs.stadr = madr;
		}
	}
	else
	{
		// toSPR -> Drain from memory to SPR.
		// DMAC does not perform STADR checks in this direction.

		const u128* readFrom = DMAC_GetHostPtr(madr, false);

		do {
			MemCopy_WrappedDest(
				readFrom, (u128*)eeMem->Scratch,
				addrtmp, Ps2MemSize::Scratch/16, tqwc
			);
			readFrom+= tqwc + dmacRegs.sqwc.SQWC;
			curqwc	-= tqwc;
			addrtmp	+= dmacRegs.sqwc.SQWC;
			addrtmp	&= (Ps2MemSize::Scratch / 16) - 1;
		} while(UseDmaBurstHack && curqwc);
	}

	uint qwc_copied = creg.qwc.QWC - curqwc;
	sadr.ADDR = addrtmp * 16;
	madr.ADDR += qwc_copied;

	dmac_metrics.RecordXfer(Id, INTERLEAVE_MODE, qwc_copied);
}

void EE_DMAC::ChannelState::TransferNormalAndChainData()
{
	const ChannelInformation& fromSPR = ChannelInfo[ChanId_fromSPR];
	ChannelRegisters& fromSprReg = fromSPR.GetRegs();

	// Step 1 : Determine MADR and Copyable Length

	const DirectionMode dir = GetDir();
	uint qwc = creg.qwc.QWC;

	try
	{
		// CHAIN modes arbitrate per-packet regardless of slice or burst modes.
		// NORMAL modes arbitrate at 8 QWC slices.

		if (NORMAL_MODE == chcr.MOD)
		{
			if (!UseDmaBurstHack && IsSliced())
				qwc = std::min<uint>(creg.qwc.QWC, 8);
		}
		else // CHAIN_MODE
		{
			if (UseMFIFOHack && ((creg.chcr.TAG.ID == TAG_CNT) || (creg.chcr.TAG.ID == TAG_END)))
			{
				// MFIFOhack: We can't let the peripheral out-strip SPR.
				//  (REFx tags copy from sources other than our SPRdma, which is why we
				//   exclude them above).
				qwc = std::min<uint>(creg.qwc.QWC, fromSprReg.qwc.QWC);
			}
		}

		if (DrainStallActive())
		{
			// this channel has drain stalling enabled.  If the stall condition is already met
			// then we need to skip it by and try another channel.

			// Drain Stalling Rules:
			//  * We can copy data up to STADR (exclusive).
			//  * Arbitration is not granted until at least 8 QWC is available for copy.
			//  
			// Furthermore, there must be at *least* 8 QWCs available for transfer or the
			// DMA stalls.  Stall-control DMAs do not transfer partial QWCs at the edge of
			// STADR.  Translation: If the source DMA (the one writing to STADR) doesn't
			// transfer an even 8-qwc block, the drain DMA will actually deadlock until
			// the PS2 app manually writes 0 to STR!  (and this is correct!) --air

			uint stallAt = dmacRegs.stadr.ADDR;
			uint endAt = madr.ADDR + qwc*16;

			if (!madr.SPR)
			{
				if (endAt > stallAt)
				{
					qwc = (stallAt - madr.ADDR) / 16;
					DMAC_LOG("\tDRAIN STALL condition! (STADR=%s, newQWC=%u)", dmacRegs.stadr.ToUTF8(false), qwc);
				}
			}
			else if (stallAt < Ps2MemSize::Scratch)
			{
				// Assumptions:
				// SPR bit transfers most likely perform automatic memory wrapping/masking on MADR
				// and likely do not automatically mask/wrap STADR (both of these assertions
				// need proper test app confirmations!! -- air)

				if ((madr.ADDR < stallAt) && (endAt > stallAt))
				{
					qwc = (stallAt - madr.ADDR) / 16;
					DMAC_LOG("\tDRAIN STALL condition! (STADR=%s, newQWC=%u)", dmacRegs.stadr.ToUTF8(false), qwc);
				}
				else
				{
					endAt &= (Ps2MemSize::Scratch-1);
					if ((madr.ADDR >= stallAt) && (endAt > stallAt))
					{
						// Copy from madr->ScratchEnd and from ScratchStart->StallAt
						qwc = ((Ps2MemSize::Scratch - madr.ADDR) + stallAt) / 16;
						DMAC_LOG("\tDRAIN STALL condition (STADR=%s, newQWC=%u) [SPR memory wrap]", dmacRegs.stadr.ToUTF8(false), qwc);
					}
				}
			}
		}
		
		// The real hardware has undefined behavior for this, but PCSX2 supports it.
 		pxAssertMsg(creg.qwc.QWC < _1mb, "DMAC: QWC is over 1 meg!");

		// -----------------------------------
		// DO THAT MOVEMENT OF DATA.  NOOOOOW!		
		// -----------------------------------

		uint qwc_xfer = (dir == Dir_Source)
			? TransferSource(DMAC_GetHostPtr(madr,true), qwc)
			: TransferDrain(DMAC_GetHostPtr(madr,false), qwc);

		// Peripherals have the option to stall transfers on their end, usually due to
		// specific conditions that can arise, such as tag errors or IRQs.

		if (qwc_xfer != qwc)
		{
			DMAC_LOG( "\tPartial transfer %s peripheral (qwc=%u, xfer=%u)",
				(dir==Dir_Drain) ? "to" : "from",
				qwc, qwc_xfer
			);
		}

		creg.qwc.QWC -= qwc_xfer;
		if (0 == creg.qwc.QWC)
		{
			// NORMAL MODE: STR becomes 0 when transfer ends (qwc==0)
			// CHAIN MODE: hop to the next link in the chain!

			if (NORMAL_MODE == creg.chcr.MOD)
			{
				creg.chcr.STR = 0;
				dmacRegs.stat.CIS |= (1 << Id);
			}
			else // (CHAIN_MODE == creg.chcr.MOD)
			{
				// In order to process chains correctly, we must update TADR and MADR
				// in separate passes.  This mimics the real DMAC behavior, which itself
				// does not update/advance the TADR until after the current chain's transfer
				// has completed successfully.
				//
				// After TADR is established, the new TAG is loaded into the channel's CHCR,
				// and then the new MADR established.

				const DMAtag* tag;

				if (MFIFOActive())
				{
					pxAssumeDev(NORMAL_MODE == fromSprReg.chcr.MOD, "MFIFO error: fromSPR is not in NORMAL mode.");
					if (fromSprReg.chcr.STR)
						pxAssumeDev(fromSprReg.qwc.QWC >= 1, "(MFIFO) fromSPR is running but has a QWC of zero!?");

					// MFIFO Enabled on this channel.
					// Based on the UseMFIFOHack, there are two approaches here:
					//  1. Hack Enabled: Copy data directly to/from SPR and the MFD peripheral.
					//  2. Hack Disabled: Copy data to/from the ringbuffer specified by the
					//     RBOR and RBSR registers (requires lots of wrapped memcpys).

					MFIFO_SrcChainUpdateTADR();

					if (!chcr.STR)
						return;

					// Load next tag from TADR and store the upper 16 bits in CHCR.
					tag = (DMAtag*)DMAC_GetHostPtr(creg.tadr, false);
					chcr._tag16 = tag->Bits16to31();
					creg.qwc.QWC = tag->QWC;

					MFIFO_SrcChainUpdateMADR(*tag);
				}
				else
				{
					if (dir == Dir_Drain)
						SrcChainUpdateTADR();
					else
						DstChainUpdateTADR();

					if (!chcr.STR)
						return;

					// Load next tag from TADR and store the upper 16 bits in CHCR.
					tag = (DMAtag*)DMAC_GetHostPtr(creg.tadr, false);
					chcr._tag16 = tag->Bits16to31();
					creg.qwc.QWC = tag->QWC;

					if (dir == Dir_Drain)
						SrcChainUpdateMADR(*tag);
					else
						DstChainUpdateMADR();
				}

				if (chcr.TTE && (dir == Dir_Drain))
				{
					// Tag Transfer is enabled
					// -----------------------
					// DMAtag is transferred with the data.  Tag is 128 bits, but the lower 64
					// bits are masked to zero; which typically translates into harmless NOPs in
					// GIFtag and VIFcode lands.
 
					// * TTE's behavior regarding the lower 64 bits is currently a strong assumption,
					//   but can be confirmed easily using toSPR's Source Chain mode transfer.  Write
					//   dummy data to SPR memory, initiate a simple chain transfer with TTE=1, and
					//   read back the result.

					static __aligned16 u64 masked_tag[2] = {0,0};
					masked_tag[1] = tag->_u64;
					TransferDrain(masked_tag);
				}
			}
		}

	} catch( Exception::DmaRaiseIRQ& ex )
	{
		// Standard IRQ behavior for all channel errors is to stop the DMA (STR=0)
		// and set the CIS bit corresponding to the DMA channel.  Bus Errors set
		// the BEIS bit additionally.

		if (!ex.m_MFIFOstall)
		{
			chcr.STR = 0;
			dmacRegs.stat.CIS |= (1 << Id);
			dmacRegs.stat.BEIS = ex.m_BusError;
		}

		dmacRegs.stat.MEIS = ex.m_MFIFOstall;

		if (ex.m_Verbose)
		{
			Console.Warning(L"(DMAC) IRQ raised on %s(%u), cause=%s", info.NameW, Id, ex.m_Cause);
		}

		DMAC_LOG("IRQ Raised on %s(%u), cause=%s", info.NameA, Id, wxString(ex.m_Cause).ToUTF8().data());

		// arbitrate back to the EE for a while?
		//break;
	}

}

void EE_DMAC::ChannelState::TransferData()
{
	const char* const SrcDrainMsg = GetDir() ? "<-" : "->";

	DMAC_LOG("\tBus right granted to %s%s%s QWC=0x%4x MODE=%s",
		info.ToUTF8().data(), SrcDrainMsg, creg.madr.ToUTF8(),
		creg.qwc.QWC, chcr.ModeToUTF8()
	);

	// Interleave mode has special accelerated handling in burst mode, and a lot of
	// checks and assertions, so lets handle it from its own function to help maintain
	// programmer sanity.

	if (chcr.MOD == INTERLEAVE_MODE)
		TransferInterleaveData();
	else
		TransferNormalAndChainData();
}

void eeEvt_UpdateDmac()
{
	ControllerRegisters& dmacReg = (ControllerRegisters&)psHu8(DMAC_CTRL);

	DMA_LOG("(UpdateDMAC Event) D_CTRL=0x%08X", dmacRegs.ctrl._u32);

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

	if ((psHu32(DMAC_ENABLER) & 0x10000) || dmacRegs.ctrl.DMAE)
	{
		// Do not reschedule the event.  The indirect HW reg handler will reschedule it when
		// the DMAC register(s) are written and the DMAC is fully re-enabled.
		DMA_LOG("DMAC disabled, no actions performed. (DMAE=%d, ENABLER=0x%08x", dmacRegs.ctrl.DMAE, psHu32(DMAC_ENABLER));
		return;
	}

	do {
		ChannelId chanId = ArbitrateBusRight();

		if (chanId == -1)
		{
			// Do not reschedule the event.  The indirect HW reg handler will reschedule it when
			// the STR bits are written and the DMAC is enabled.

			DMA_LOG("DMAC Arbitration complete.");
			break;
		}

		ChannelState cstate( chanId );
		cstate.TransferData();

	} while (UseDmaBurstHack);
	
	// Furthermore, the SPR DMAs are "Burst" mode DMAs that behave very predictably
	// when the RELE bit is cleared (no cycle stealing): all other DMAs are stalled
	// and the SPR DMA transfers all its data in one shot.

	wxString CycStealMsg;
	if (dmacRegs.ctrl.RELE)
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
		
		CycStealMsg = wxsFormat(L"On/%d",8<<dmacRegs.ctrl.RCYC);
	}
	else
	{
		CycStealMsg = L"Off";
	}

	if (dmacRegs.ctrl.MFD)
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
	
	if (dmacRegs.ctrl.STS)
	{
		/* Stall Control Source Channel
		
		This function must be be emulated at all times.  It essentially causes the DMAC to
		write the MADR of the specified channel to STADR.  While this isn't always needed
		by our own DMAC (which can avoid stalling by doing complete BURST style transfers
		for all unchained DMAs), apps could still rely on STADR for their own internal
		logic.
		*/
	}

	if (dmacRegs.ctrl.STD)
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

	for( uint i=0; i<NumChannels; ++i )
	{
		
	}
}


uint __dmacall EE_DMAC::toVIF0	(const u128* srcBase, uint srcSize, uint srcStartQwc, uint lenQwc) { return 0; }
uint __dmacall EE_DMAC::toGIF	(const u128* srcBase, uint srcSize, uint srcStartQwc, uint lenQwc) { return 0; }
uint __dmacall EE_DMAC::toVIF1	(const u128* srcBase, uint srcSize, uint srcStartQwc, uint lenQwc) { return 0; }
uint __dmacall EE_DMAC::toSIF1	(const u128* srcBase, uint srcSize, uint srcStartQwc, uint lenQwc) { return 0; }
uint __dmacall EE_DMAC::toSIF2	(const u128* srcBase, uint srcSize, uint srcStartQwc, uint lenQwc) { return 0; }
uint __dmacall EE_DMAC::toIPU	(const u128* srcBase, uint srcSize, uint srcStartQwc, uint lenQwc) { return 0; }
uint __dmacall EE_DMAC::toSPR	(const u128* srcBase, uint srcSize, uint srcStartQwc, uint lenQwc) { return 0; }

uint __dmacall EE_DMAC::fromIPU	(u128* dest, uint destSize, uint destStartQwc, uint lenQwc) { return 0; }
uint __dmacall EE_DMAC::fromSPR	(u128* dest, uint destSize, uint destStartQwc, uint lenQwc) { return 0; }
uint __dmacall EE_DMAC::fromSIF0(u128* dest, uint destSize, uint destStartQwc, uint lenQwc) { return 0; }
uint __dmacall EE_DMAC::fromSIF2(u128* dest, uint destSize, uint destStartQwc, uint lenQwc) { return 0; }
uint __dmacall EE_DMAC::fromVIF0(u128* dest, uint destSize, uint destStartQwc, uint lenQwc) { return 0; }


// --------------------------------------------------------------------------------------
//  EE_DMAC::ControllerMetrics  (implementations)
// --------------------------------------------------------------------------------------
u64 EE_DMAC::ControllerMetrics::GetQWC(LogicalTransferMode mode) const
{
	u64 result = 0;
	for( uint i=0; i<NumChannels; ++i )
		result += channel[i].qwc[mode];
	return result;
}

u64 EE_DMAC::ControllerMetrics::GetQWC() const
{
	u64 result = 0;
	for( uint i=0; i<NumChannels; ++i )
	for( uint m=0; m<4; ++m )
		result += channel[i].qwc[m];
	return result;
}

uint EE_DMAC::ControllerMetrics::GetTransferCount(LogicalTransferMode mode) const
{
	uint result = 0;
	for( uint i=0; i<NumChannels; ++i )
		result += channel[i].xfers[mode];
	return result;
}

uint EE_DMAC::ControllerMetrics::GetTransferCount() const
{
	uint result = 0;
	for( uint i=0; i<NumChannels; ++i )
		for( uint m=0; m<4; ++m )
			result += channel[i].xfers[m];
	return result;
}