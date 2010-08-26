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

#pragma once

#include "NewDmac.h"

void EE_DMAC::ChannelState::MFIFO_SrcChainUpdateTADR()
{
	tDMAC_ADDR& tadr = info.TADR();

	switch(chcr.TAG.ID)
	{
		// REFE (Reference and End) - Transfer QWC from the specified ADDR, and End Transfer.
		// END (Continue and End) - Transfer QWC following the tag and end.
		case TAG_REFE:
		case TAG_END:
			// Note that when ending transfers, TADR is *not* updated (Soul Calibur 2 and 3)
			chcr.STR = 0;
		break;

		// CNT (Continue) - Transfer QWC following the tag, and following QWC becomes the new TADR.
		case TAG_CNT:
		{
			const ChannelInformation& fromSPR = ChannelInfo[ChanId_fromSPR];
			ChannelRegisters& fromSprReg = fromSPR.GetRegs();

			if (UseMFIFOHack)
			{
				if (!fromSprReg.chcr.STR)
					throw Exception::DmaRaiseIRQ(L"MFIFO stall").MFIFOstall();

				tadr.ADDR = fromSprReg.sadr.ADDR;
				tadr.SPR = 1;
				tadr.IncrementQWC();
			}
			else
			{
				uint new_tadr = dmacReg.mfifoWrapAddr(madr.ADDR + 16);
				fromSprReg.madr = dmacReg.mfifoWrapAddr(fromSprReg.madr.ADDR);

				if ((new_tadr == fromSprReg.madr.ADDR) && !fromSprReg.chcr.STR)
				{
					// MFIFO Stall condition (!)  The FIFO is drained and the SPR isn't
					// feeding it any more data.  Oddly enough, the drain channel does
					// *not* stop in this situation, though we can't very well keep reading
					// data, so throw an exception:

					throw Exception::DmaRaiseIRQ(L"MFIFO stall").MFIFOstall();
				}
				tadr.ADDR = new_tadr;
			}
		}
		break;

		// REF  (Reference) - Transfer QWC from the ADDR field, and increment the TADR to get the next tag.
		// REFS (Reference and Stall) - ... and check STADR and stall if needed, when reading the QWC.
		case TAG_REFS:
			if (!pxAssertDev(Stall_Drain != info.DmaStall, L"REFS without stall control"))
				throw Exception::DmaRaiseIRQ(L"REFS without stall control").Verbose();

		case TAG_REF:
			if (UseMFIFOHack)
			{
				tadr.IncrementQWC();
			}
			else
			{
				tadr.ADDR = dmacReg.mfifoWrapAddr(tadr.ADDR + 16);
			}
		break;

		// -------------------------------------
		// Tags not supported in MFIFO transfers
		// -------------------------------------
		case TAG_CALL:
		case TAG_RET:
		case TAG_NEXT:
			pxAssumeDev(false, "(DMAC CHAIN) Unsupported tag invoked during MFIFO." );
		break;

		jNO_DEFAULT
	}

	// Source chains perform IRQ checks after updating TADR, but *before* actually loading
	// the new tag into CHCR (the exception skips the loading part)

	if(chcr.TAG.IRQ && chcr.TIE)
		throw Exception::DmaRaiseIRQ(L"Tag IRQ");
}

void EE_DMAC::ChannelState::MFIFO_SrcChainUpdateMADR( const DMAtag& tag )
{
	const tDMAC_ADDR& tadr = info.TADR();

	switch (chcr.TAG.ID)
	{
		case TAG_REFS:
			if (!pxAssertDev(info.DmaStall == Stall_Drain, "(DMA CHAIN) REFS tag invoked on an unsupported channel."))
			{
				// Most likely this is correct, though there is a possibility that the
				// Real DMAC actually just treats CNTS like a CNT if the channel doesn't
				// support Source Stall mode.
				throw Exception::DmaRaiseIRQ(L"REFS without stall control").Verbose();
			}
	
		case TAG_REFE:
		case TAG_REF:
		{
			if (UseMFIFOHack)
			{
				const ChannelInformation& fromSPR = ChannelInfo[ChanId_fromSPR];
				ChannelRegisters& fromSprReg = fromSPR.GetRegs();

				fromSprReg.qwc.QWC += 1;
				fromSprReg.sadr.ADDR += 16;

				if (0 == fromSprReg.qwc.QWC)
				{
					fromSprReg.chcr.STR = 0;
					dmacReg.stat.CIS |= (1 << ChanId_fromSPR);
				}
			}
			else
			{
				madr = tag.addr;
			}
		}
		break;

		case TAG_END:
		case TAG_CNT:
			if (UseMFIFOHack)
			{
				const ChannelInformation& fromSPR = ChannelInfo[ChanId_fromSPR];
				ChannelRegisters& fromSprReg = fromSPR.GetRegs();
				madr.ADDR = fromSprReg.sadr.ADDR;
				madr.SPR = 1;
				madr.IncrementQWC();

				// Increment SPR tadr based on the chain DMAtag.
				// This is part of the MFIFOhack.

				uint sprqwc = 1 + tag.QWC;
				sprqwc = std::min<uint>(sprqwc, fromSprReg.qwc.QWC);

				fromSprReg.qwc.QWC += sprqwc;
				fromSprReg.sadr.ADDR += sprqwc*16;

				if (0 == fromSprReg.qwc.QWC)
				{
					fromSprReg.chcr.STR = 0;
					dmacReg.stat.CIS |= (1 << ChanId_fromSPR);
				}

			}
			else
			{
				madr.ADDR = dmacReg.mfifoWrapAddr(tadr.ADDR + 16);
			}
		break;

		// -------------------------------------
		// Tags not supported in MFIFO transfers
		// -------------------------------------
		case TAG_CALL:
		case TAG_RET:
		case TAG_NEXT:
			pxAssumeDev(false, "(DMAC CHAIN) Unsupported tag invoked during MFIFO." );
		break;

		jNO_DEFAULT
	}
}


void EE_DMAC::ChannelState::SrcChainUpdateTADR()
{
	tDMAC_ADDR& tadr = info.TADR();

	// Not really sure what the Real DMACdoes if this happens. >_<
	pxAssumeDev(info.hasSourceChain, "(DMAC) Source chain invoked on unsupported channel.");

	switch(chcr.TAG.ID)
	{
		// REFE (Reference and End) - Transfer QWC from the specified ADDR, and End Transfer.
		// END (Continue and End) - Transfer QWC following the tag and end.
		case TAG_REFE:
		case TAG_END:
			// Note that when ending transfers, TADR is *not* updated (Soul Calibur 2 and 3)
			chcr.STR = 0;
		break;

		// CNT (Continue) - Transfer QWC following the tag, and following QWC becomes the new TADR.
		case TAG_CNT:
			tadr = madr;
			tadr.IncrementQWC();
		break;
		
		// NEXT - Transfer QWC following the tag, and uses ADDR of the old tag as the new TADR.
		case TAG_NEXT:
			tadr._u32 = DMAC_Read32(tadr);
		break;
		
		// REF  (Reference) - Transfer QWC from the ADDR field, and increment the TADR to get the next tag.
		// REFS (Reference and Stall) - ... and check STADR and stall if needed, when reading the QWC.
		case TAG_REFS:
			if (!pxAssertDev(Stall_Drain != info.DmaStall, L"REFS without stall control"))
				throw Exception::DmaRaiseIRQ(L"REFS without stall control").Verbose();

		case TAG_REF:
			tadr.IncrementQWC();
		break;

		// CALL - Transfer QWC following the tag, push MADR onto the ASR stack, and use ADDR
		//        of the old tag as the call target (assigned to TADR).
		case TAG_CALL:
		{
			if (!pxAssertDev(info.hasAddressStack, "(DMAC CHAIN) CALL tag invoked on an unsupported channel (no address stack)." ))
			{
				// Note that this condition should typically be unreachable, since the same
				// check is performed when TADR is read and set.

				// CALL and RET are only supported on VIF0, VIF1, and GIF.  On any other
				// channel the *likely* reaction of the DMAC is to stop the transfer and
				// raise an interrupt:

				throw Exception::DmaRaiseIRQ(L"CALL without address stack").Verbose();
			}
			
			// Stash an address on the address stack pointer.
			// Channels that support the address stack have two addresses available:
			// ASR0 and ASR1.  If both addresses are full, an error occurs and the
			// DMA is stopped.
			
			switch(chcr.ASP)
			{
				case 0: creg.asr0 = madr; break;
				case 1: creg.asr1 = madr; break;

				default:
					throw Exception::DmaRaiseIRQ(L"Callstack Overflow").Verbose();
			}

			++chcr.ASP;
			tadr._u32 = DMAC_Read32(tadr);
		}
		break;
		
		// RET - Transfer QWC following the tag, an pops ASR0/1 into the TADR.
		case TAG_RET:
		{
			if (!pxAssertDev(info.hasAddressStack, "(DMAC CHAIN) RET tag invoked on an unsupported channel (no address stack)." ))
			{
				// Note that this condition should typically be unreachable, since the same
				// check is performed when TADR is read and set.

				// CALL and RET are only supported on VIF0, VIF1, and GIF.  On any other
				// channel the *likely* reaction of the DMAC is to stop the transfer and
				// raise an interrupt:

				throw Exception::DmaRaiseIRQ(L"RET without address stack").Verbose();
			}

			switch(chcr.ASP)
			{
				case 0:
				{
					// Callstack underflow could be a common occurrence in games, since
					// it is a convenient way to end chains.  So lets do the IRQ manually
					// and avoid the overhead of throwing an exception.
					//throw Exception::DmaRaiseIRQ(L"Callstack Underflow");

					chcr.STR = 0;
					dmacReg.stat.CIS |= (1 << Id);
				}

				case 1: creg.asr0 = madr; break;
				case 2: creg.asr1 = madr; break;

				case 3:
					pxFailDev("(DMAC) Invalid address stack pointer value = 3.");
			}
			--chcr.ASP;
		}
		break;

		jNO_DEFAULT
	}

	// Source chains perform IRQ checks after updating TADR, but *before* actually loading
	// the new tag into CHCR (the exception skips the loading part)

	if(chcr.TAG.IRQ && chcr.TIE)
		throw Exception::DmaRaiseIRQ(L"Tag IRQ");
}

void EE_DMAC::ChannelState::SrcChainUpdateMADR( const DMAtag& tag )
{
	// Not really sure what the Real DMACdoes if this happens. >_<
	pxAssumeDev(info.hasSourceChain, "(DMAC) Source chain invoked on unsupported channel.");

	switch (chcr.TAG.ID)
	{
		// --------------------------------------------------------------------------------------
		// These tags all transfer from the ADDR field read from the TADR pointer.
		// --------------------------------------------------------------------------------------
		case TAG_REFS:
			if (!pxAssertDev(info.DmaStall == Stall_Drain, "(DMA CHAIN) REFS tag invoked on an unsupported channel."))
			{
				// Most likely this is correct, though there is a possibility that the
				// Real DMAC actually just treats CNTS like a CNT if the channel doesn't
				// support Source Stall mode.
				throw Exception::DmaRaiseIRQ(L"REFS without stall control").Verbose();
			}
	
		case TAG_REFE:
		case TAG_REF:
			madr = tag.addr;
		break;

		// --------------------------------------------------------------------------------------
		// These Tags all transfer the QWC following the tag.  Only REF tags transfer QWC
		// from another source or address.
		// --------------------------------------------------------------------------------------
		case TAG_CALL:
		case TAG_RET:
			if (!pxAssertDev(info.hasAddressStack, "(DMAC CHAIN) CALL/RET tag invoked on an unsupported channel (no address stack)." ))
			{
				// Note that this condition should typically be unreachable, since the same
				// check is performed when TADR is read and set.

				// CALL and RET are only supported on VIF0, VIF1, and GIF.  On any other
				// channel the *likely* reaction of the DMAC is to stop the transfer and
				// raise an interrupt:

				throw Exception::DmaRaiseIRQ(L"CALL without address stack").Verbose();
			}

		// ...and fall through!

		case TAG_END:
		case TAG_CNT:
		case TAG_NEXT:
			madr = creg.tadr;
			madr.IncrementQWC();
		break;

		jNO_DEFAULT
	}
}

void EE_DMAC::ChannelState::DstChainUpdateTADR()
{
	// Not really sure what the Real DMACdoes if this happens. >_<
	pxAssumeDev(info.hasDestChain, "(DMAC) Destination chain invoked on unsupported channel.");

	// Destination chains perform IRQ checks prior to updating TADR!
	if(chcr.TAG.IRQ && chcr.TIE)
		throw Exception::DmaRaiseIRQ(L"Tag IRQ");

	switch(chcr.TAG.ID)
	{
		// END (Continue and End) - Transfer QWC following the tag and end.
		case TAG_END:
			// Note that when ending transfers, TADR is *not* updated (Soul Calibur 2 and 3)
			chcr.STR = 0;
		break;

		// CNT (Continue) - Transfer QWC following the tag, and following QWC becomes the new TADR.
		// CNT (Continue with Stall) - ... and stall against STADR as needed.
		case TAG_CNTS:
			if (!pxAssertDev(info.DmaStall == Stall_Source, "(DMA CHAIN) CNTS tag invoked on an unsupported channel."))
			{
				// Most likely this is correct, though there is a possibility that the
				// Real DMAC actually just treats CNTS like a CNT if the channel doesn't
				// support Source Stall mode.
				throw Exception::DmaRaiseIRQ(L"CNTS without stall control").Verbose();
			}
			if (SourceStallActive())
				dmacReg.stadr = madr;
			
			// .. and fall through!

		case TAG_CNT:
			creg.tadr = madr;
			creg.tadr.IncrementQWC();
		break;

		jNO_DEFAULT
	}
}

void EE_DMAC::ChannelState::DstChainUpdateMADR()
{
	if (TAG_CNTS == chcr.TAG.ID)
	{
		if (!pxAssertDev(info.DmaStall == Stall_Source, "(DMA CHAIN) CNTS tag invoked on an unsupported channel."))
		{
			// Most likely this is correct, though there is a possibility that the
			// Real DMAC actually just treats CNTS like a CNT if the channel doesn't
			// support Source Stall mode.
			throw Exception::DmaRaiseIRQ(L"CNTS without stall control").Verbose();
		}
	}

	madr = creg.tadr;
	madr.IncrementQWC();
}
