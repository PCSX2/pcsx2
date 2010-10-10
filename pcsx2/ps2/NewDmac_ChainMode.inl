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

using namespace EE_DMAC;

static tDMAC_ADDR mfifo_tadr;
static tDMAC_ADDR mfifo_madr;


const DMAtag* ChannelState::SrcChainTransferTag()
{
	// Any conditions that set STR==0 should exception and skip past this code.
	pxAssume(chcr.STR);

	// Load next newtag from TADR and store the upper 16 bits in CHCR.
	const DMAtag* newtag = (DMAtag*)GetHostPtr(creg.tadr, false);

	DMAC_LOG("\tSrcChain Tag @ %ls : %ls", creg.tadr.ToString().c_str(), newtag->ToString(Dir_Drain).c_str());

	if (chcr.TTE)
	{
		// Tag Transfer is enabled
		// -----------------------
		// DMAtag is transferred with the data.  Tag is 128 bits, but the lower 64
		// bits are masked to zero; which typically translates into harmless NOPs in
		// GIFtag and VIFcode lands.

		// [Ps2Confirm] TTE's behavior regarding the lower 64 bits is currently a
		//   strong assumption, but can be confirmed easily using toSPR's Source
		//   Chain mode transfer.  Write dummy data to SPR memory, initiate a simple
		//   chain transfer with TTE=1, and read back the result.

		static __aligned16 u128 masked_tag;

		masked_tag._u64[0] = 0;
		masked_tag._u64[1] = *((u64*)newtag + 1);

		DMAC_LOG("\tSrcChain TTE=1, data = 0x%08x.%08x", masked_tag._u32[3], masked_tag._u32[2]);
		if (!TransferDrain(masked_tag))
		{
			// Peripheral is stalled.  We'll need to try and transfer the newtag again
			// later... (as long as qwc remains 0, the tag will keep trying to re-send).

			return NULL;
		}
	}

	// tag transfered successfully (or TTE disabled) -- update tag registers:
	chcr.tag16		= newtag->Bits16to31();
	creg.qwc.QWC	= newtag->QWC;

	return newtag;
}


// Returns FALSE if the transfer has ended or been suspended for some reason (usually
// end-of-transfer or newtag irq).
bool ChannelState::MFIFO_SrcChainLoadTag()
{
	const DMAtag* newtag = SrcChainTransferTag();
	if (!newtag) return false;

	// MFIFO shouldn't be able to be located in SPR memory.  It's quite possible the SPR bit
	// is disregarded if ever set (or causes a DMA bus error).
	pxAssume(!madr.SPR);

	switch (chcr.TAG.ID)
	{
		case TAG_REFS:
			if (!pxAssertDev(info.DmaStall == Stall_Drain, "(DMA CHAIN) REFS tag invoked on an unsupported channel."))
			{
				// Most likely this is correct, though there is a possibility that the Real DMAC
				// actually just treats CNTS like a CNT if the channel doesn't support Source
				// Stall mode.
				return IrqStall(Stall_TagError, L"REFS without stall control");
			}
		// fall through ...

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
					dmacRegs.stat.CIS |= (1 << ChanId_fromSPR);
				}
			}
			else
			{
				madr = dmacRegs.mfifoWrapAddr(newtag->addr);
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

				uint sprqwc = 1 + newtag->QWC;
				sprqwc = std::min<uint>(sprqwc, fromSprReg.qwc.QWC);

				fromSprReg.qwc.QWC += sprqwc;
				fromSprReg.sadr.ADDR += sprqwc*16;

				if (0 == fromSprReg.qwc.QWC)
				{
					fromSprReg.chcr.STR = 0;
					dmacRegs.stat.CIS |= (1 << ChanId_fromSPR);
				}

			}
			else
			{
				madr = dmacRegs.mfifoWrapAddr(info.TADR(), 16);
			}
		break;

		// -------------------------------------
		// Tags not supported in MFIFO transfers
		// -------------------------------------
		// [Ps2Confirm] Should these cause invalid tag errors, or be silently ignored?

		case TAG_CALL:
		case TAG_RET:
		case TAG_NEXT:
			//pxAssumeDev(false, "(DMAC CHAIN) Unsupported tag invoked during MFIFO." );
			return IrqStall(Stall_TagError, L"Unsupported tag invoked during MFIFO.");
		break;

		default: return IrqStall(Stall_TagError, L"Unknown/reserved ID");
	}
	
	return true;
}

bool ChannelState::MFIFO_SrcChainUpdateTADR()
{
	tDMAC_ADDR& tadr = info.TADR();

	switch(chcr.TAG.ID)
	{
		// REFE (Reference and End) - Transfer QWC from the specified ADDR, and End Transfer.
		// END (Continue and End) - Transfer QWC following the tag and end.
		case TAG_REFE:
		case TAG_END:
			// Note that when ending transfers, TADR is *not* updated (Soul Calibur 2 and 3)
			return IrqStall( Stall_EndOfTransfer );
		break;

		// CNT (Continue) - Transfer QWC following the tag, and following QWC becomes the new TADR.
		case TAG_CNT:
		{
			const ChannelInformation& fromSPR = ChannelInfo[ChanId_fromSPR];
			ChannelRegisters& fromSprReg = fromSPR.GetRegs();

			if (UseMFIFOHack)
			{
				if (!fromSprReg.chcr.STR)
				{
					// See below for details...
					return IrqStall(Stall_MFIFO);
				}

				tadr.ADDR = fromSprReg.sadr.ADDR;
				tadr.SPR = 1;
				tadr.IncrementQWC();
			}
			else
			{
				tDMAC_ADDR new_tadr = dmacRegs.mfifoWrapAddr(madr.ADDR + 16);
				pxAssertDev(fromSprReg.madr == dmacRegs.mfifoWrapAddr(fromSprReg.madr.ADDR), "MFIFO MADR wrapping failure.");
				//fromSprReg.madr = dmacRegs.mfifoWrapAddr(fromSprReg.madr.ADDR);

				if ((new_tadr == fromSprReg.madr) && !fromSprReg.chcr.STR)
				{
					// MFIFO Stall condition (!)  The FIFO is drained and the SPR isn't
					// feeding it any more data.  Oddly enough, the drain channel does
					// *not* stop in this situation (STR remains 1), though we can't very
					// well keep reading data.. :p
					
					// (the reason the MFIFO drain channel does not stall is because it must
					//  be started first, and thus as soon as it is started this stall condition
					//  is met, until the fromSPR Source channel is also established).

					return IrqStall(Stall_MFIFO);
				}
				tadr = new_tadr;
			}
		}
		break;

		// REF  (Reference) - Transfer QWC from the ADDR field, and increment the TADR to get the next tag.
		// REFS (Reference and Stall) - ... and check STADR and stall if needed, when reading the QWC.
		case TAG_REFS:
			if (!pxAssertDev(Stall_Drain != info.DmaStall, L"REFS without stall control"))
			{
				return IrqStall(Stall_TagError, L"REFS without stall control");
			}
		// fall through...

		case TAG_REF:
			if (UseMFIFOHack)
			{
				tadr.IncrementQWC();
			}
			else
			{
				tadr = dmacRegs.mfifoWrapAddr(tadr, 16);
			}
		break;

		// -------------------------------------
		// Tags not supported in MFIFO transfers
		// -------------------------------------
		// This code should be unreachable since the MADR update below will be the
		// first to check the tag for validity.

		#if 0
		case TAG_CALL:
		case TAG_RET:
		case TAG_NEXT:
			//pxAssumeDev("(DMAC CHAIN) Unsupported tag invoked during MFIFO." );
			return IrqStall(Stall_TagError, L"Unsupported tag invoked during MFIFO");
		break;
		#endif

		jNO_DEFAULT
	}

	return true;
}

bool ChannelState::SrcChainLoadTag()
{
	// Not really sure what the Real DMAC does if this happens. >_<
	pxAssumeDev(info.hasSourceChain, "(DMAC) Source chain invoked on unsupported channel.");

	// Any conditions that set STR==0 should exception and skip past this code.
	pxAssume(chcr.STR);

	tDMAC_ADDR& tadr = info.TADR();
	const DMAtag* newtag = SrcChainTransferTag();
	if (!newtag) return false;

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
				return IrqStall(Stall_TagError, L"REFS without stall control");
			}
		// fall through...
	
		case TAG_REFE:
		case TAG_REF:
			madr = newtag->addr;
		break;

		// --------------------------------------------------------------------------------------
		// These Tags all transfer the QWC following the tag.  Only REF tags transfer QWC
		// from another source or address.
		// --------------------------------------------------------------------------------------
		case TAG_CALL:
		case TAG_RET:
			if (!pxAssertDev(info.hasAddressStack, L"(DMAC CHAIN) CALL/RET invoked on an unsupported channel (no address stack)." ))
			{
				// CALL and RET are only supported on VIF0, VIF1, and GIF.  On any other
				// channel the *likely* reaction of the DMAC is to stop the transfer and
				// raise an interrupt:

				return IrqStall(Stall_TagError, L"CALL/RET invoked on an unsupported channel (no address stack)");
			}
		// ...and fall through!

		case TAG_END:
		case TAG_CNT:
		case TAG_NEXT:
			madr = tadr;
			madr.IncrementQWC();
		break;

		jNO_DEFAULT
	}
	
	return true;
}

bool EE_DMAC::ChannelState::SrcChainUpdateTADR()
{
	tDMAC_ADDR& tadr = info.TADR();

	// Not really sure what the Real DMAC does if this happens. >_<
	pxAssumeDev(info.hasSourceChain, "(DMAC) Source chain invoked on unsupported channel.");

	switch(chcr.TAG.ID)
	{
		// REFE (Reference and End) - Transfer QWC from the specified ADDR, and End Transfer.
		case TAG_REFE:
			tadr.IncrementQWC();
			return IrqStall(Stall_EndOfTransfer);
		break;

		// END (Continue and End) - Transfer QWC following the tag and end.
		case TAG_END:
			tadr = madr;
			return IrqStall(Stall_EndOfTransfer);
		break;

		// CNT (Continue) - Transfer QWC following the tag, and following QWC becomes the new TADR.
		case TAG_CNT:
			tadr = madr;
		break;
		
		// REF  (Reference) - Transfer QWC from the ADDR field, and increment the TADR to get the next tag.
		// REFS (Reference and Stall) - ... and check STADR and stall if needed, when reading the QWC.
		case TAG_REFS:
			if (!pxAssertDev(Stall_Drain != info.DmaStall, L"REFS without stall control"))
				return IrqStall(Stall_TagError, L"REFS without stall control");
		// fall through...

		case TAG_REF:
			tadr.IncrementQWC();
		break;

		// CALL - Transfer QWC following the tag, push post-transfer MADR onto the ASR stack, and
		//        use ADDR of the old tag as the call target (assigned to TADR).
		case TAG_CALL:
		{
			if (!pxAssertDev(info.hasAddressStack, "(DMAC CHAIN) CALL tag invoked on an unsupported channel (no address stack)." ))
			{
				// Note that this condition should typically be unreachable, since the same
				// check is performed when the tag is loaded.  However, since games can alter
				// tags mid-transfer; we are better safe to check redundantly.

				// CALL and RET are only supported on VIF0, VIF1, and GIF.  On any other
				// channel the *likely* reaction of the DMAC is to stop the transfer and
				// raise an interrupt:

				return IrqStall(Stall_TagError, L"CALL without address stack");
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
					return IrqStall(Stall_CallstackOverflow);
			}

			++chcr.ASP;
			// Fall through to NEXT:
		}

		// NEXT - Transfer QWC following the tag, and uses ADDR specified by the tag as the new TADR.
		case TAG_NEXT:
		{
			const DMAtag& newtag = *(DMAtag*)GetHostPtr(tadr, false);
			tadr = newtag.addr;
		}
		break;
		
		// RET - Transfer QWC following the tag, an pops ASR0/1 into the TADR.
		case TAG_RET:
		{
			if (!pxAssertDev(info.hasAddressStack, "(DMAC CHAIN) RET tag invoked on an unsupported channel (no address stack)." ))
			{
				// Note that this condition should typically be unreachable, since the same
				// check is performed when the tag is loaded.

				// CALL and RET are only supported on VIF0, VIF1, and GIF.  On any other
				// channel the *likely* reaction of the DMAC is to stop the transfer and
				// raise an interrupt:

				return IrqStall(Stall_TagError, L"RET without address stack");
			}

			switch(chcr.ASP)
			{
				case 0: return IrqStall(Stall_CallstackUnderflow); break;
				case 1: tadr = creg.asr0; break;
				case 2: tadr = creg.asr1; break;

				case 3:
					pxAssumeDev(false, "(DMAC) Invalid address stack pointer value = 3.");
			}
			--chcr.ASP;
		}
		break;

		jNO_DEFAULT
		
		// This should be unreachable -- tags are first checked for validity when
		// SrcChainUpdateMADR() is called.
		//default: return IrqStall(Stall_TagError, "Unknown/reserved ID");
	}
	
	return true;
}

bool EE_DMAC::ChannelState::DstChainLoadTag()
{
	// Not really sure what the Real DMAC does if this happens. >_<
	pxAssumeDev(info.hasDestChain, "(DMAC) Destination chain invoked on unsupported channel.");

	u128 dest;
	if (!TransferSource(dest))
	{
		DMAC_LOG("\tTransfer stalled transferring the Destination Chain Tag.");
		return false;
	}

	DMAtag& tag		= (DMAtag&)dest;

	chcr.tag16		= tag.Bits16to31();
	creg.qwc.QWC	= tag.QWC;
	madr			= tag.addr;

	DMAC_LOG("\tDestChain Tag = %ls", tag.ToString(Dir_Source).c_str());

	switch(chcr.TAG.ID)
	{
		// CNT (Continue with Stall) - ... and stall against STADR as needed.
		case TAG_CNTS:
			if (!pxAssertDev(info.DmaStall == Stall_Source, "(DMA CHAIN) CNTS tag invoked on an unsupported channel."))
			{
				// Most likely this is correct, though there is a possibility that the
				// Real DMAC actually just treats CNTS like a CNT if the channel doesn't
				// support Source Stall mode.
				return IrqStall(Stall_TagError, L"CNTS without stall control");
			}

			// .. and fall through!

		// CNT (Continue) - Transfer QWC following the tag, and following QWC becomes the new TADR.
		// END (Continue and End) - Transfer QWC following the tag and end.
		case TAG_CNT:
		case TAG_END:
		break;

		// jNO_DEFAULT is an optimized path that works for all known games.  The real PS2
		// hardware behaves as though the default case is reached: it stalls the transfer
		// and raises an exception.

		//jNO_DEFAULT
		default:
			return IrqStall(Stall_TagError, L"Unknown/reserved ID");
	}
	
	return true;
}
