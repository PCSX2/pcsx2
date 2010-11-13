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

#include "Sif.h"
#include "IopHw.h"
#include "ps2/NewDmac.h"
#include "R3000A.h"

/*
SIF Overview:

  The SIF is a connection between the EE and the IOP.  For the most part the SIF is a simple
  design that transfers data in and out of the FIFOs (one 8 QWC FIFO attached to the EE and
  an 8-WORD fifo on the IOP  (where WORDs are 32 bits each).  The SIF differs from the
  other FIFO'd DMAs on the EE in the following ways:
  
   * all three SIF DMA channels share the same FIFO.
   * The SIF FIFO does not appear to be mapped to hardware register (meaning it is only
     accessible via DMA); or if it is, nothing ever seems to attempt to access it directly.
   * There is no way to control SIF FIFO direction (like there is with VIF); the FIFO appears
     to be controlled by DMAC exclusively.

  In spite of SIF DMAs sharing a single FIFO, the DMAs can apparently run relatively
  asynchronously.  The actual method of asynchronous operation is unknown, but is likely
  some form of SFIFO arbitration, where by the FIFO direction is switched in order to
  accommodate incoming data.

  The SIF is the most "uncertain" of all PS2 components, from a programmer's perspective.
  It is a handshake exchange between two entirely separate CPUs, both of which rarely aware
  of the other's current status.  Even if the SFIFO were directly accessible via hardware
  register, it would be either incredibly slow or entirely unsafe to use such a mechanism
  (in order to be safe, both CPUs would need to be forcibly synchronized via SBUS registers
  prior to FIFO use).  Tus, for EE/IOP communications to proceed efficiently, things must
  be executed in as much batch/queue style as possible.  Because of this, "accurate" timing
  is both unnecessary and nearly impossible.

IOP SIF Transfer Mode:

  The IOP SIF0 appears to always be in IOP CHAIN mode operation (a mode of transfer only
  supported by the IOP SIF).  Unlike EE chains, IOP chains are very simple, specifying
  only a start address and length in the TAG.
  
Emulation strategy overview:
 
  Actual emulation of the FIFOs is *not* necessary.  FIFO activity always drains completely
  before the next DMA transfer slice begins, and the SFIFO is only accessible via DMA so there
  is no need to worry with one-at-a-time style FIFO feeds.  EE/IOP DMAs are directly linked
  and perform all transfers in immediate time, with transfers following the basic rules of
  stall and arbitration: if a SIF1 transfer is started on the EE, it will stall until a
  SIF0 transfer on the IOP is also started.
  
  SFIFO emulation is not provided even in a strict/purist emulation sense.  We do not have
  a detailed understanding of how the EE/IOP FIFOs perform bi-directional arbitration,
  so any implementation would be mostly made up abyway.  Nor do the EE or IOP have direct
  control over the SFIFO in any way (which allows us maximum freedom in implementation of
  the system, regardless of how the real hardware performs its own FIFO arbitration
  internally).

*/


#define SFIFO_EMULATED 0

// --------------------------------------------------------------------------------------
//  IopDmaTag
// --------------------------------------------------------------------------------------
// The IOP supports a bastardized version of Source Chain DMA transfer on the SIF channel
// only.  The tag is a quadword, where the lower 64 bits are used for IOP address and transfer
// size information. The upper 64 bits are actually the EE's DMAtag (qwc count and destination
// address).
//
// When transferring from IOP to EE, the copy process looks as such:
//   ee_tag (64 bits)      -> EE
//   NULL   (64 bits)      -> EE
//   addr   (wcnt*4 bytes) -> EE
//
// The NULL bits after the ee_tag are to ensure proper 128-bit alignment of the data after
// the DMAtag, which the EE expects.  The actual data is irrelevant -- zero'd or current stale
// FIFO contents work fine.
//
// The Low 32-bits of the IOP Chain Tag contain the MADR/Address and some various bits
// of information (some of which is unknown at this time).
//   Bits 0->23 are the address.
//   Bit 30 is the End of Chain bit (set to 1 to end the IOP DMA transfer)
//   Bit 31 is the IRQ bit, which suspends the chain transfer.
//
// Both EOC and IRQ cause an interrupt to be generated to host (IOP), though the two may
// imply slightly different TAG and/or TADR behavior (which is unknown at this time).
//
union iDMAtag
{
	struct
	{
		// First 32 bits:

		u32		ADDR		: 24;		// source address (loaded into MADR)
		u32		_unknown	: 6;
		u32		EOC			: 1;		// End of Chain
		u32		IRQ			: 1;

		// Second 32 bits:

		u32		WCNT;		// word count (words are 32 bits units on MIPS)
	};

	u64 _u64;
	u32 _u32[2];
	
	void clear() { _u64 = 0; }
};

// --------------------------------------------------------------------------------------
//  SIF_Internals
// --------------------------------------------------------------------------------------
// Because the SIF has no known hardware registers for tracking FIFO status, we have to
// use our own internal data structures for it.
//
struct SIF_Internals
{
	// Unused -- We're not emulating the SIF's SFIFO at all currently
	//uint	FQC;		// # of quadwords in the FIFO
	uint	DIR;		// current FIFO direction (0=Source [SIF0], 1=Drain [SIF1])

	// Unused -- we're storing the info in the IOP's BCR instead.
	//uint	QWC;		// remaining quadwords in the current IOP DMA transfer

	iDMAtag tag;
};

__aligned16 SIF_Internals sifstate;

void sifInit()
{
	memzero(sifstate);
}

void SaveStateBase::sifFreeze()
{
	FreezeTag("SIFdma");
	Freeze(sifstate);
}

// returns the number of qwc actually transferred.
// Destination is typically either EE memory or the SFIFO (if implemented/enabled).
static uint SIF0_Transfer(u128* dest, uint dstQwc)
{
	// If there's no pending DMA transfer on the IOP side to feed us data, then the
	// DMA stalls.
	if (!hw_dma9.chcr.STR) return 0;

	sifstate.DIR = 0;

	// FIXME:  IOP BCR! 
	// If IOP behaves like the EE, then it will attempt to transfer data as per the BCR
	// prior to reading and processing tags.  Likewise, the IOP's DMAC may actually load
	// the BCR with the word count for each transfer in the chain (same as the EE, and
	// would make sense since the IOP DMAC likely relies on such behavior to resume from
	// DMA suspension).
	//
	//pxAssertDev( (hw_dma9.bcr >> 16) == 0 || (hw_dma9.bcr & 0xffff) == 0 );

	uint startQwc = dstQwc;

	uint wcnt = ((hw_dma9.bcr >> 16) * (hw_dma9.bcr & 0xffff));
	uint iopQwc = (wcnt + 3) / 4;

	while (dstQwc)
	{
		if (!iopQwc)
		{
			// Fetch next tag in the chain. IOP's "Source Chain" mode is a 128 bit tag pair, where
			// the first 64 bits is the IOP's DMAtag, and the second 64 bits is the EE's DMAtag).

			const u64* tagPtr = (u64*)iopPhysMem(hw_dma9.tadr);
			Copy64(&sifstate.tag, tagPtr);

			SIF_LOG("IOP/SIF0 Source Chain Tag Loaded @ TADR=0x%06X : MADR=0x%06X, WCNT=0x%04X",
				hw_dma9.tadr, sifstate.tag.ADDR, sifstate.tag.WCNT);

			hw_dma9.tadr += 16;
			hw_dma9.madr = sifstate.tag.ADDR;
			iopQwc = (sifstate.tag.WCNT + 3) / 4;


			// Copy the EE's tag.  The EE expects the tag to be 128 bits, with the upper
			// 64 bits being ineffective (ignored).  Since the IOP's source address isn't
			// 128 bit aligned, we can't use MOVAPS/MOVDQA, but we can use MOVLPS, which is
			// not alignment-restricted (yay).

			Copy64(dest, tagPtr + 1);
			++dest;
			--dstQwc;
			continue;
		}

		// IOP's source address memory is likely not QWC-aligned so we can't use memcpy_qwc here.
		uint transable = std::min(dstQwc, iopQwc);
		memcpy_fast(dest, iopPhysMem(hw_dma9.madr), transable*16);

		hw_dma9.madr += transable * 16;
		iopQwc -= transable;
		dstQwc -= transable;

		// Check for End-of-Chain / IRQ of the current chain tag:

		if (sifstate.tag.IRQ || sifstate.tag.EOC)
		{
			psHu32(SBUS_F240) &= ~0x20;
			psHu32(SBUS_F240) &= ~0x2000;

			hw_dma9.chcr.STR = 0;
			psxDmaInterrupt2(2);
			break;
		}
	}

	// write the IOP's BCR back; we might need it later if this was a partial transfer:
	hw_dma9.bcr = (iopQwc << 16) | 0x4;

	return startQwc-dstQwc;
}

// returns the number of qwc actually transferred
static uint SIF1_Transfer(const u128* src, uint srcQwc)
{
	// If there's no pending DMA transfer on the IOP side to receive data, the DMA stalls.
	if (!hw_dma10.chcr.STR) return 0;

	sifstate.DIR = 1;

	// Unlike source chain mode, the packets in IOP's destination chain do not need to
	// be QWC-aligned; so we have to process everything in words (32 bits) at a time.  This
	// of course complicates everything neatly. :)

	uint startQwc = srcQwc;
	uint wcnt = ((hw_dma10.bcr >> 16) * (hw_dma10.bcr & 0xffff));
	pxAssumeDev((wcnt & 3) == 0, "IOP SIF1 Unaligned size specified in BCR.");
	uint iopQwc = wcnt / 4;

	while (srcQwc)
	{
		u8* iopdest;

		if (!iopQwc)
		{
			// fetch the next tag from the incoming DMA stream.
			// IOP's "Destination Chain" mode is a 128-bit tag, immediately followed by data
			// to be written to the destination address specified in the tag.  The lower 64 bits
			// is the IOP DMAtag, and the upper 64 bits are ineffective.  Data size indicated by
			// the tag should always be QWC-aligned.

			// source is 128-bit aligned, destBase is likely not, and the upper 64 bits can be discarded.
			// Conclusion: this set of SSE copies should work nicely!

			__m128d copyreg = _mm_load_pd((double*)src);
			_mm_storel_pd((double*)&sifstate.tag, copyreg);		// store back low 64 bits, alignment-safe.
			++src;
			--srcQwc;

			SIF_LOG("IOP/SIF1 Dest Chain Tag Loaded : MADR=0x%06X, WCNT=0x%04X",
				sifstate.tag.ADDR, sifstate.tag.WCNT);

			pxAssumeDev((sifstate.tag.WCNT & 3) == 0, "IOP SIF1 Unaligned size specified in tag.");

			iopQwc = sifstate.tag.WCNT / 4;
			hw_dma10.madr = sifstate.tag.ADDR;
			continue;
		}

		uint transable = std::min(srcQwc, iopQwc);
		iopdest = iopPhysMem(hw_dma10.madr);
		if (!iopdest)
		{
			pxFailDev("Invalid target address for IOP SIF1 (Dma 10)");
			// [TODO] IOP Bus error?  More likely a DMA-local bus error that simply stops
			// the DMA and flags a bit somewhere:

			psHu32(SBUS_F240) &= ~0x40;
			psHu32(SBUS_F240) &= ~0x4000;

			hw_dma10.chcr.STR = 0;
			psxDmaInterrupt2(3);
			break;
		}

		memcpy_fast(iopdest, src, transable * 16);
		psxCpu->Clear(hw_dma10.madr, transable*4);

		hw_dma10.madr += transable * 16;
		iopQwc -= transable;
		srcQwc -= transable;

		if (iopQwc == 0)
		{
			// Check for End-of-Chain / IRQ of the current chain tag:
			// Since it unknown if the IOP has a proper holding area for chain mode
			// TAG information, we have to maintain that stuff ourselves in sifstate.

			if (sifstate.tag.IRQ || sifstate.tag.EOC)
			{
				psHu32(SBUS_F240) &= ~0x40;
				psHu32(SBUS_F240) &= ~0x4000;

				hw_dma10.chcr.STR = 0;
				psxDmaInterrupt2(3);
				break;
			}
		}
	}

	// write the IOP's BCR back; we might need it later if this was a partial transfer:
	hw_dma10.bcr = (iopQwc << 16) | 0x4;

	return startQwc - srcQwc;
}

uint __dmacall EE_DMAC::fromSIF0(u128* destBase, uint destSize, uint destStartQwc, uint lenQwc)
{
	// SIF transfers do not support wrapping, since they do not honor the SPR bit,
	// cannot be used in conjunction with the MFIFO; and we don't emulate the SFIFO either.

	pxAssume(destSize==0 && destStartQwc==0);

	#if SFIFO_EMULATED
	if (sifstate.FQC)
	{
		// Need to drain the FIFO we can transfer the requested data packet.
		// We can only drain it if its already pointing in the right direction, though.
		// If the direction is wrong, we'll have to skip arbitration until the other
		// direction drains the FIFO properly.

		if (sifstate.DIR != 0) return 0;

		uint xferqwc = SIF0_Transfer(g_fifo.sif, sifstate.FQC);
		sifstate.FQC -= xferqwc;
		if (sifstate.FQC) return 0;
	}
	#endif
	
	return SIF0_Transfer( destBase, lenQwc );
}

uint __dmacall EE_DMAC::toSIF1(const u128* srcBase, uint srcSize, uint srcStartQwc, uint lenQwc)
{
	// SIF transfers do not support wrapping, since they do not honor the SPR bit,
	// cannot be used in conjunction with the MFIFO; and we don't emulate the SFIFO either.

	pxAssume(srcSize==0 && srcStartQwc==0);

	#if SFIFO_EMULATED
	if (sifstate.FQC)
	{
		// Need to drain the FIFO before we can transfer the requested data packet.
		// We can only drain it if its already pointing in the right direction, though.
		// If the direction is wrong, we'll have to skip arbitration until the other
		// direction drains the FIFO properly.

		if (sifstate.DIR != 1) return 0;

		uint xferqwc = SIF1_Transfer(g_fifo.sif, sifstate.FQC);
		sifstate.FQC -= xferqwc;
		if (sifstate.FQC) return 0;
	}
	#endif

	return SIF1_Transfer( srcBase, lenQwc );
}


void psxDma9(iDMA_CHCR newchcr)
{
	// We ignore the write if:
	//  * if STR is already set (log/assert if game is trying to clear it)
	// 	* is STR is not being changed (old == new)

	if (!hw_dma9.chcr.STR)
	{
		if (newchcr.STR)
		{
			// PCR is the Priority Control Register.  This shouldn't be disabling transfers outright;
			// it should only be changing the arbitration order (delaying a channel's transfer until
			// another has completed).  Since we have no priority system, let's ignore it for now.

			//uint bitmess = 8 << ((9-7) * 4);
			//if ((HW_DMA_PCR2 & bitmess) == 0) return;

			SIF_LOG("IOP dmaSIF0(9): chcr = %08x, madr = %08x, bcr = %08x, tadr = %08x",
				newchcr._u32, hw_dma9.madr, hw_dma9.bcr, hw_dma9.tadr);

			psHu32(SBUS_F240) |= 0x2000;

			// The EE is responsible for performing all SIF transfers, so the IOP can only signal
			// to the EE that it is ready for transfer (DREQ); and then continue on its merry way.

			sifstate.tag.clear();
			EE_DMAC::dmacRequestSlice(EE_DMAC::ChanId_SIF0);
		}
	}
	else if(newchcr.STR)
	{
		pxFailDev("Attempted manual clearing of IOP SIF0 STR bit.");
	}

	hw_dma9.chcr = newchcr;
}

void psxDma10(iDMA_CHCR newchcr)
{
	// We ignore the write if:
	//  * if STR is already set (log/assert if game is trying to clear it)
	// 	* is STR is not being changed (old == new)

	if (!hw_dma10.chcr.STR)
	{
		if (newchcr.STR)
		{
			SIF_LOG("IOP dmaSIF1(10): chcr = %08x, madr = %08x, bcr = %08x",
				newchcr._u32, hw_dma10.madr, hw_dma10.bcr);

			psHu32(SBUS_F240) |= 0x4000;

			// The EE is responsible for performing all SIF transfers, so the IOP can only signal
			// to the EE that it is ready for transfer (DREQ); and then continue on its merry way.

			// See SIF0 above.
			//uint bitmess = 8 << ((10-7) * 4);
			//if ((HW_DMA_PCR2 & bitmess) == 0) return;

			sifstate.tag.clear();
			EE_DMAC::dmacRequestSlice(EE_DMAC::ChanId_SIF1);
		}
	}
	else if(newchcr.STR)
	{
		pxFailDev("Attempted manual clearing of IOP SIF1 STR bit.");
	}

	hw_dma10.chcr = newchcr;
}
