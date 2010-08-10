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

enum DMA_StallMode
{
	// No stalling logic is performed (STADR is not read or written)
	DmaStall_None,
	
	// STADR is written with the MADR after data is transfered.
	DmaStall_Source,

	// STADR is read and MADR is not allowed to advance beyond that point.
	DmaStall_Drain,
};

enum DMA_DirectionMode
{
	// Indicates a DMA that transfers from peripheral to memory
	DmaDir_Source,
	
	// Indicates a DMA that transfers from peripheral to memory
	DmaDir_Drain,
	
	// Indicates a DAM that bases its transfer direction on the DIR bit of its CHCR register.
	DmaDir_Both,
};

#define __dmacall __fastcall

// Returns the number of QWC actually transferred.  Return value can be 0, in cases where the
// peripheral has no room to receive data (SIF FIFO is full, or occupied by another channel,
// for example).
typedef uint __dmacall FnType_ToPeripheral(const u128* src, uint qwc);

// Returns the number of QWC actually transferred.  Return value can be 0, in cases where the
// peripheral has no data to provide (SIF FIFO is empty, or occupied by another channel,
// for example).
typedef uint __dmacall FnType_FromPeripheral(u128* dest, uint qwc);

typedef FnType_ToPeripheral*	Fnptr_ToPeripheral;
typedef FnType_FromPeripheral*	Fnptr_FromPeripheral;

// --------------------------------------------------------------------------------------
//  Exception::DmaRaiseIRQ
// --------------------------------------------------------------------------------------
namespace Exception
{
	class DmaRaiseIRQ
	{
	public:
		bool		BusError;
		const char* Cause;

		DmaRaiseIRQ( const char* _cause=NULL, bool buserr = false)
		{
			BusError = buserr;
			Cause = _cause;
		}
	};
}

// --------------------------------------------------------------------------------------
//  DmaChannelInformation
// --------------------------------------------------------------------------------------
struct DmaChannelInformation
{
	const wxChar*	name;

	uint			regbaseaddr;
	
	DMA_StallMode	DmaStall;
	bool			hasSourceChain;
	bool			hasDestChain;
	bool			hasAddressStack;

	Fnptr_ToPeripheral		toFunc;
	Fnptr_FromPeripheral	fromFunc;

	DMA_DirectionMode GetDir() const
	{
		if (toFunc && fromFunc) return DmaDir_Both;
		return toFunc ? DmaDir_Drain : DmaDir_Source;
	}

	tDMA_CHCR& GetCHCR() const
	{
		return (tDMA_CHCR&)PS2MEM_HW[regbaseaddr];
	}

	tDMA_ADDR& GetMADR() const
	{
		return (tDMA_ADDR&)PS2MEM_HW[regbaseaddr+0x10];
	}

	tDMA_QWC& GetQWC() const
	{
		return (tDMA_QWC&)PS2MEM_HW[regbaseaddr+0x20];
	}
	
	tDMA_ADDR& GetTADR() const
	{
		pxAssert(hasSourceChain || hasDestChain);
		return (tDMA_ADDR&)PS2MEM_HW[regbaseaddr+0x30];
	}

	tDMA_ADDR& GetASR0() const
	{
		pxAssert(hasAddressStack);
		return (tDMA_ADDR&)PS2MEM_HW[regbaseaddr+0x40];
	}

	tDMA_ADDR& GetASR1() const
	{
		pxAssert(hasAddressStack);
		return (tDMA_ADDR&)PS2MEM_HW[regbaseaddr+0x50];
	}
};

extern FnType_FromPeripheral fromVIF0;
extern FnType_FromPeripheral fromIPU;
extern FnType_FromPeripheral fromSIF0;
extern FnType_FromPeripheral fromSIF2;
extern FnType_FromPeripheral fromSPR;

extern FnType_ToPeripheral toVIF0;
extern FnType_ToPeripheral toVIF1;
extern FnType_ToPeripheral toGIF;
extern FnType_ToPeripheral toIPU;
extern FnType_ToPeripheral toSIF1;
extern FnType_ToPeripheral toSIF2;
extern FnType_ToPeripheral toSPR;

enum DMA_ChannelId
{
	DmaId_VIF0 = 0,
	DmaId_VIF1,
	DmaId_GIF,
	DmaId_fromIPU,
	DmaId_toIPU,
	DmaId_SIF0,
	DmaId_SIF1,
	DmaId_SIF2,
	DmaId_fromSPR,
	DmaId_toSPR,

	DmaId_None
};

#define _m(v) ((v) & 0xffff)

static const DmaChannelInformation DmaChan[] =
{
	//				baseaddr		D.S.				S.C.	D.C.	A.S.
	{ L"VIF0",		_m(D0_CHCR),	DmaStall_None,		true,	false,	true,	toVIF0,		fromVIF0	},
	{ L"VIF1",		_m(D1_CHCR),	DmaStall_Drain,		true,	false,	true,	toVIF1,		NULL		},
	{ L"GIF",		_m(D2_CHCR),	DmaStall_Drain,		true,	false,	true,	toGIF,		NULL		},
	{ L"fromIPU",	_m(D3_CHCR),	DmaStall_Source,	false,	false,	false,	NULL,		fromIPU		},
	{ L"toIPU",		_m(D4_CHCR),	DmaStall_None,		true,	false,	false,	toIPU,		NULL		},
	{ L"SIF0",		_m(D5_CHCR),	DmaStall_Source,	false,	true,	false,	NULL,		fromSIF0	},
	{ L"SIF1",		_m(D6_CHCR),	DmaStall_Drain,		true,	false,	false,	toSIF1,		NULL		},
	{ L"SIF2",		_m(D7_CHCR),	DmaStall_None,		false,	false,	false,	toSIF2,		fromSIF2	},
	{ L"fromSPR",	_m(D8_CHCR),	DmaStall_Source,	false,	true,	false,	NULL,		fromSPR		},
	{ L"toSPR",		_m(D9_CHCR),	DmaStall_None,		true,	false,	false,	toSPR,		NULL		},

	// Legend:
	//   D.S.  -- DMA Stall
	//   S.C.  -- Source Chain
	//   D.C.  -- Destination Chain
	//   A.S.  -- Has Address Stack
};

#undef _m

static const DMA_ChannelId StallSrcChan[4] =
{
	DmaId_None, DmaId_SIF0, DmaId_fromSPR, DmaId_fromIPU
};

static const DMA_ChannelId StallDrainChan[4] =
{
	DmaId_None, DmaId_VIF1, DmaId_SIF1, DmaId_GIF
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

static uint round_robin = 1;

// Returns the index of the next DMA channel granted bus rights.
static DMA_ChannelId ArbitrateBusRight()
{
	//  * VIF0 has top priority.
	//  * SIF2 has secondary priority.
	//  * All other channels are managed in cyclic arbitration (round robin).

	wxString ActiveDmaMsg;

	const tDMAC_PCR& pcr = (tDMAC_PCR&)psHu8(DMAC_PCR);

	// VIF0 is the highest of the high priorities!!
	const tDMA_CHCR& vif0chcr = DmaChan[0].GetCHCR();
	if (vif0chcr.STR)
	{
		if (!pcr.PCE || (pcr.CDE & 2)) return DmaId_VIF0;
		DMA_LOG("\tVIF0 bypassed due to PCE/CDE0 condition.");
	}

	// SIF2 is next!!
	const tDMA_CHCR& sif2chcr = DmaChan[7].GetCHCR();
	if (sif2chcr.STR)
	{
		if (!pcr.PCE || (pcr.CDE & 2)) return DmaId_SIF2;
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

		if (DmaChan[round_robin].DmaStall == DmaStall_Drain)
		{
			// this channel supports drain stalling.  If the stall condition is already met
			// then we need to skip it by and try another channel.

			const tDMAC_STADR& stadr = (tDMAC_STADR&)psHu8(DMAC_STADR);

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
			// rates (actual DMA channel FIFOs are not emulated, only their burst copy size is
			// enforced, which is typically 8 QWC tho varies in interleave and chain modes).
			// When a peripheral FIFO is deemed full or empty (depending on src/drain flag),
			// the DMA is available for selection.
		}

		return (DMA_ChannelId)round_robin;
	}

	return DmaId_None;
}

// Two 1 megabyte (max DMA) buffers for reading and writing to high memory (>32MB).
// Such accesses are not documented as causing bus errors but as the memory does
// not exist, reads should continue to return 0 and writes should be discarded.
// Probably.
static __aligned16 u128 highmem[(_1mb * 2) / 16];

u128* DMAC_GetHostPtr( const tDMA_ADDR& addrReg, bool writeToMem )
{
	static const uint addr = addrReg.ADDR;

	if (addrReg.SPR) return &psSu128(addr);

	// The DMAC appears to be directly hardwired to various memory banks: Main memory (including
	// ROM), VUs, and the Scratchpad.  It is likely wired to the Hardware Register map as well,
	// since it uses registers internally for accessing some peripherals (such as the GS FIFO
	// regs).

	// Supporting the hardware regs properly is problematic, but fortunately there's no reason
	// a game would likely ever use it, so we don't really support them (the PCSX2 emulated DMA
	// will map to the psH[] memory, but does not invoke any of the indirect read/write handlers).

	if ((addr >= PhysMemMap::Scratchpad) && (addr < PhysMemMap::ScratchpadEnd))
	{
		// Secret scratchpad address for DMA; games typically specify 0x70000000, but chances
		// are the DMAC masks all addresses to MIPS physical memory specification (512MB),
		// which would place SPR between 0x10000000 and 0x10004000.  Unknown yet if that is true
		// so I'm sticking with the 0x70000000 mapping.

		return &psSu128(addr);
	}

	void* result = vtlb_GetPhyPtr(addr);
	if (!result)
	{
		if (addr < 0x10000000)		// 256mb (PS2 max memory)
		{
			// Such accesses are not documented as causing bus errors but as the memory does
			// not exist, reads should continue to return 0 and writes should be discarded.
			// IOP has similar behavior on its DMAs and some memory accesses.

			return &highmem[writeToMem ? _1mb : 0];
		}
		else
		{
			wxString msg;
			msg.Printf( L"DMA address error: 0x%08x", addr );
			Console.Error(msg);
			pxAssertDev(false, msg);
			throw Exception::DmaRaiseIRQ("BusError", true);
		}
	}

	pxFailDev( "Unreachable code reached (!)" );
	return NULL;
}

u32 DMAC_Read32( DMA_ChannelId chanId, const tDMA_ADDR& addr )
{
	return *(u32*)DMAC_GetHostPtr(addr, false);
}

void eeEvt_UpdateDmac()
{
	tDMAC_CTRL& dmactrl = (tDMAC_CTRL&)psHu8(DMAC_CTRL);
	tDMAC_STAT& dmastat = (tDMAC_STAT&)psHu8(DMAC_STAT);

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

	do {
		DMA_ChannelId chanId = ArbitrateBusRight();

		if (chanId == -1)
		{
			// Do not reschedule the event.  The indirect HW reg handler will reschedule it when
			// the STR bits are written and the DMAC is enabled.

			DMA_LOG("DMAC Arbitration complete.");
			break;
		}

		const DmaChannelInformation& chan = DmaChan[chanId];

		tDMA_CHCR& chcr = chan.GetCHCR();
		tDMA_ADDR& madr = chan.GetMADR();
		tDMA_QWC& qwcreg = chan.GetQWC();

		// Determine Direction!
		// --------------------

		DMA_DirectionMode dir = chan.GetDir();
		const char* SrcDrainMsg = "";

		if (dir == DmaDir_Both)
		{
			dir = chcr.DIR ? DmaDir_Drain : DmaDir_Source;
			SrcDrainMsg = chcr.DIR ? "(Drain)" : "(Source)";
		}

		DMA_LOG("\tBus right granted to %s%s MADR=0x%08X QWC=0x%4x", chan.name, SrcDrainMsg, madr.ADDR, qwcreg.QWC);

		const tDMAC_STADR& stadr = (tDMAC_STADR&)PS2MEM_HW[DMAC_STADR];
		const char* PhysModeStr;

		// Determine MADR and Initial Copyable Length
		// ------------------------------------------
		// The initial length determined here does not take into consideration STADR, which
		// must be applied later once our memory wrapping parameters are known.

		uint qwc = 0;

		try
		{
		switch (chcr.MOD)
		{
			case UNDEFINED_MODE:
				pxAssertDev( false, "DMAC: Undefined physical transfer mode?!" );
			break;

			case NORMAL_MODE:
				PhysModeStr = "NORMAL";
				qwc = qwcreg.QWC;
			break;

			case CHAIN_MODE:
			{
				PhysModeStr = "CHAIN";

				if (qwcreg.QWC == 0)
				{
					// Update TADR!
					// This is done *after* each chain transfer has completed.

					tDMA_ADDR& tadr = chan.GetTADR();
					switch(chcr.TAG.ID)
					{
						// REFE (Reference and End) - Transfer packet according to ADDR field and End Transfer.
						// END (Continue and End) - Transfer QWC following the tag and end.
						case TAG_REFE:
						case TAG_END:
							// Note that when ending transfers, TADR is *not* updated (Soul Calibur 2 and 3)
							chcr.STR = 0;
						break;

						// CNT (Continue) - Transfer QWC following the tag, and following QWC becomes the new TADR.
						case TAG_CNT:
							tadr.ADDR = madr.ADDR + 16;
						break;
						
						// NEXT - Transfer QWC following the tag, and uses ADDR of the old tag as the new TADR.
						case TAG_NEXT:
							tadr._u32 = DMAC_Read32(chanId, tadr);
						break;
						
						// REF  (Reference) - Transfer QWC from the ADDR field, and increment the TADR to get the next tag.
						// REFS (Reference and Stall) - ... and check STADR and stall if needed, when reading the QWC.
						case TAG_REF:
						case TAG_REFS:
							tadr.IncrementQWC();
						break;

						// CALL - Transfer QWC following the tag, pushes MADR onto the ASR stack, and uses ADDR as the
						//        next tag.  (QWC is typically 0).
						case TAG_CALL:
						{
							// Stash an address on the address stack pointer.
							switch(chcr.ASP)
							{
								case 0: //Check if ASR0 is empty
									// Store the succeeding tag in asr0, and mark chcr as having 1 address.
									//dma.asr0 = dma.madr + (dma.qwc << 4);
									//dma.chcr.ASP++;
								break;

								case 1:
									// Store the succeeding tag in asr1, and mark chcr as having 2 addresses.
									//dma.asr1 = dma.madr + (dma.qwc << 4);
									//dma.chcr.ASP++;
								break;

								default:
									Console.Warning("DMA CHAIN callstack overflow.  Transfer stopped.");
									throw Exception::DmaRaiseIRQ();
								break;
							}
						}
						break;
						
						case TAG_RET:
						break;
					}

					if (!chcr.STR)
					{
						// The chain has ended.  Nothing more to do here, and we should pass
						// arbitration to another active DMA since this one didn't transfer any data.
						continue;
					}

					// Load next tag from TADR and store the upper 16 bits in CHCR:
					const tDMA_TAG* tag = (tDMA_TAG*)DMAC_GetHostPtr(tadr, false);
					chcr._tag16 = tag->upper();
					qwcreg.QWC = tag->QWC;
					
				}
			}
			break;

			case INTERLEAVE_MODE:
				PhysModeStr = "INTERLEAVE";

				// Should only be valid for toSPR and fromSPR DMAs only.
				pxAssertDev( IsSprChannel(chanId), "DMAC: Interleave mode specified on Scratchpad channel!" );
			break;
		}

		// Determine Memory Wrapping Parameters (if needed)
		// ------------------------------------------------
		// By default DMAs don't wrap.  SPR transfers do, however; and VU transfers likely wrap
		// as well.

		// Will be assigned only if needed; and will remain zero if no wrapping is performed.
		uint wrapsize = 0;

		if (madr.SPR)
		{
			wrapsize = Ps2MemSize::Scratch;
		}
		else
		{
			if ((madr.ADDR >= PhysMemMap::Scratchpad) && (madr.ADDR < PhysMemMap::ScratchpadEnd))
			{
				// It appears that games can access the SPR directly using 0x70000000, but
				// most just use the SPR bit.  What's unknown is if the direct mapping via
				// 0x70000000 affects stalling behavior or SPR memory wrap-around.

				// For now we're assuming SPR direct mappings wrap --air
				wrapsize = Ps2MemSize::Scratch;
			}

			else if ((madr.ADDR >= PhysMemMap::VUMemStart) && (madr.ADDR < PhysMemMap::VUMemEnd))
			{
				// VU memory can also be accessed directly via DMA
				// (this may only be available on to/fromSPR dmas)

				// For now we're assuming VU direct mappings wrap --air

				wrapsize = (madr.ADDR < PhysMemMap::VU1prog) ? 0x400 : 0x1000;
			}
		}


		/*if (chan.DmaStall == DmaStall_Drain)
		{
			pxAssertDev(stadr.ADDR != addrEnd, "DMAC stall address error detected.");
			if ((stadr.ADDR >= madr.ADDR) && (stadr.ADDR < addrEnd))
			{
				// Transfer is set to stall prior to the end address.  We can only
				// transfer up to the stall position -- the rest will have to come later,
				// once the stall address has been written to.

				addrEnd = stadr.ADDR-8;
			}
		}

		uint qwc = addrEnd - madr.ADDR;*/
		
		if (!pxAssertMsg(qwc < _1mb, "DMAC: QWC is over 1 meg!  Truncating."))
			qwc = _1mb;

		if (dir==DmaDir_Drain)
		{
			DMA_LOG("%s xfer %s->%s (qwc=%x)", PhysModeStr,
				pxsFmt(madr.SPR ? "0x%04X(SPR)" : "0x%08X", madr.ADDR).GetResult(),
				chan.name, qwc
			);

			pxAssume(chan.toFunc);
			//chan.toFunc(  );
		}
		else
		{
			DMA_LOG("%s xfer %s->%s (qwc=%x)", PhysModeStr, chan.name,
				pxsFmt(madr.SPR ? "0x%04X(SPR)" : "0x%08X", madr.ADDR).GetResult(),
				qwc
			);

			pxAssume(chan.fromFunc);
			//chan.fromFunc();
		}

		} catch( Exception::DmaRaiseIRQ& ex )
		{
			chcr.STR = 0;
			dmastat.CIS |= (1 << chanId);

			if (ex.BusError)
			{
				Console.Error(L"BUSERR: %s(%u)", chan.name, chanId);
				dmastat.BEIS = 1;
			}

			DMA_LOG("IRQ Raised on %s(%u), cause=%s", chan.name, chanId, ex.Cause);

			// arbitrate back to the EE for a while?
			//break;
		}

	} while (UseDmaBurstHack);
	
	//


	//wxString StallSrcMsg = StallSrcNames[dmactrl.STS];
	//wxString StallDrainMsg = StallDrainNames[dmactrl.STD];
	//wxString MfifoDrainMsg = StallDrainNames[dmactrl.STD];
	
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


uint __dmacall toVIF0(const u128* src, uint qwc) { return 0; }
uint __dmacall toGIF(const u128* src, uint qwc) { return 0; }
uint __dmacall toVIF1(const u128* src, uint qwc) { return 0; }
uint __dmacall toSIF1(const u128* src, uint qwc) { return 0; }
uint __dmacall toSIF2(const u128* src, uint qwc) { return 0; }
uint __dmacall toIPU(const u128* src, uint qwc) { return 0; }
uint __dmacall toSPR(const u128* src, uint qwc) { return 0; }

uint __dmacall fromIPU(u128* dest, uint qwc) { return 0; }
uint __dmacall fromSPR(u128* dest, uint qwc) { return 0; }
uint __dmacall fromSIF0(u128* dest, uint qwc) { return 0; }
uint __dmacall fromSIF2(u128* dest, uint qwc) { return 0; }
uint __dmacall fromVIF0(u128* dest, uint qwc) { return 0; }
//uint fromIPU(u128& dest, uint qwc) {}
//uint fromIPU(u128& dest, uint qwc) {}