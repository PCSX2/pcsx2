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

#define _m(v) ((D##v##_CHCR) & 0xffff)
#define _n(v) #v, wxT(#v)

namespace EE_DMAC
{

namespace Exception
{
	// simple class used to break DMA transfer logic for the current channel when a bus error
	// or critical tag error occurs.  Standard stall conditions such as EndOfTransfer detection
	// are handled inline using return codes (exception stack walking is a bit too slow for
	// them, especially in the context of the debugger).
	class DmaBusError { };
}


static const ChannelInformation ChannelInfo[NumChannels] =
{
	//							D.S.		S.C.	D.C.	A.S.
	{ _n(VIF0),		_m(0),	Stall_None,		true,	false,	true,	toVIF0,		fromVIF0	},
	{ _n(VIF1),		_m(1),	Stall_Drain,	true,	false,	true,	toVIF1,		NULL		},
	{ _n(GIF),		_m(2),	Stall_Drain,	true,	false,	true,	toGIF,		NULL		},
	{ _n(fromIPU),	_m(3),	Stall_Source,	false,	false,	false,	NULL,		fromIPU		},
	{ _n(toIPU),	_m(4),	Stall_None,		true,	false,	false,	toIPU,		NULL		},
	{ _n(SIF0),		_m(5),	Stall_Source,	false,	true,	false,	NULL,		fromSIF0	},
	{ _n(SIF1),		_m(6),	Stall_Drain,	true,	false,	false,	toSIF1,		NULL		},
	{ _n(SIF2),		_m(7),	Stall_None,		false,	false,	false,	toSIF2,		fromSIF2	},
	{ _n(fromSPR),	_m(8),	Stall_Source,	false,	true,	false,	NULL,		fromSPR		},
	{ _n(toSPR),	_m(9),	Stall_None,		true,	false,	false,	toSPR,		NULL		},

	// Legend:
	//   D.S.  -- DMA Stall
	//   S.C.  -- Source Chain
	//   D.C.  -- Destination Chain
	//   A.S.  -- Has Address Stack
	//   SPR   -- Scratchpad is the peripheral (uses SADR register for scratchpad src/dest)
};

bool ChannelInformation::isSprChannel() const
{
	return (_m(8) == regbaseaddr) || (_m(9) == regbaseaddr);
}

bool ChannelInformation::isSifChannel() const
{
	return (_m(5) == regbaseaddr) || (_m(6) == regbaseaddr) || (_m(7) == regbaseaddr);
}

// VIF, GIF, and IPu honor the SPR bit on addresses.  SPR and SIF transfers ignore it.
bool ChannelInformation::HonorsSprBit() const
{
	return regbaseaddr <= _m(4);
}

static const ChannelId StallSrcChan[4] = {
	ChanId_None, ChanId_SIF0, ChanId_fromSPR, ChanId_fromIPU
};

static const ChannelId StallDrainChan[4] = {
	ChanId_None, ChanId_VIF1, ChanId_SIF1, ChanId_GIF
};

static const ChannelId mfifo_DrainChanTable[4] = {
	ChanId_None, ChanId_None, ChanId_VIF1, ChanId_GIF
};


#undef _m
#undef _n

static const wxChar* MfifoDrainNames[] =
{
	L"None", L"Reserved", L"VIF1(1)", L"GIF(2)"
};


using namespace EE_DMAC;

// --------------------------------------------------------------------------------------
//  EE_DMAC::ChannelInformation  (implementations)
// --------------------------------------------------------------------------------------
wxString ChannelInformation::ToString() const
{
	FastFormatUnicode msg;

	if (isSprChannel())
		msg.Write("%s(0x%04x)", NameA, GetRegs().sadr.ADDR);
	else
		msg.Write(NameA);

	return msg;
}


// --------------------------------------------------------------------------------------
//  EE_DMAC::ChannelState  (implementations)
// --------------------------------------------------------------------------------------
ChannelState::ChannelState( ChannelId chanId )
	: Id( chanId )
	, info( ChannelInfo[Id] )
	, creg( info.GetRegs() )
	, chcr( info.CHCR() )
	, madr( info.MADR() )
{
}

DirectionMode ChannelState::GetDir() const
{
	const DirectionMode dir = info.GetRawDirection();
	return (dir==Dir_Both) ? (chcr.DIR ? Dir_Drain : Dir_Source) : dir;
}

bool ChannelState::DrainStallActive() const
{
	if (StallDrainChan[dmacRegs.ctrl.STD] != Id) return false;
	if (chcr.MOD == CHAIN_MODE) return (chcr.TAG.ID == TAG_REFS);

	return true;
}

bool ChannelState::SourceStallActive() const
{
	if (StallSrcChan[dmacRegs.ctrl.STS] != Id) return false;
	if (chcr.MOD == CHAIN_MODE) return (chcr.TAG.ID == TAG_CNTS);

	return true;
}

bool ChannelState::MFIFOActive() const
{
	return mfifo_DrainChanTable[dmacRegs.ctrl.MFD] == Id;
}

uint ChannelState::TransferSource( u128* destMemHost, uint lenQwc, uint destStartQwc, uint destSize ) const
{
	return info.fnptr_xferFrom( destMemHost, destStartQwc, destSize, lenQwc );
}

uint ChannelState::TransferDrain( const u128* srcMemHost, uint lenQwc, uint srcStartQwc, uint srcSize ) const
{
	return info.fnptr_xferTo( srcMemHost, srcStartQwc, srcSize, lenQwc );
}

template< typename T >
uint ChannelState::TransferSource( T& destBuffer ) const
{
	pxAssume((sizeof(T) & 15) == 0);		// quadwords only please!!
	return info.fnptr_xferFrom( (u128*)&destBuffer, 0, 0, sizeof(T) / 16 );
}

template< typename T >
uint ChannelState::TransferDrain( const T& srcBuffer ) const
{
	pxAssume((sizeof(T) & 15) == 0);		// quadwords only please!!
	return info.fnptr_xferTo( (u128*)&srcBuffer, 0, 0, sizeof(T) / 16 );
}

// --------------------------------------------------------------------------------------
// EmotionEngine Programmable DMA Controller Address Resolution
// --------------------------------------------------------------------------------------
// 
//  * All channels are hard-wired main memory (including ROM).
//
//  * VIF, GIF, and IPU channels are hard-wired to SPR; both via the SPR bit and via
//    direct mapping at 0x70000000 (the former supports automatic memory wrapping while
//    the latter likely generates BUSERR if the DMA exceeds the SPRAM's 16kb range).
//
//  * toSPR and fromSPR are hard-wired to VU data memory (but not VU micro memory?).
//
//  * SIF channels are *not* hard-wired to SPR or VU memory.  They can transfer to/from
//    main memory only (this is likely because the SIF is typically *very* slow).
//
//  * Channels may be wired to hardware registers as well, but no known PS2 software
//    depends on such functionality, so we don't have explicit confirmation yet.
//    (implementing DMA access to hardware regs is exceptionally difficult and slow anyway,
//     and will likely never be done.)
//


u128* ChannelState::TryGetHostPtr( const tDMAC_ADDR& addrReg, bool writeToMem )
{
	const uint addr = addrReg.ADDR;

	if (info.HonorsSprBit())
	{
		// Secret scratchpad address for DMA; games typically specify 0x70000000, but chances
		// are the DMAC masks all addresses to MIPS physical memory specification (512MB),
		// which would place SPR between 0x10000000 and 0x10004000.  Unknown yet if that is true
		// so I'm sticking with the 0x70000000 mapping.
		
		// [Ps2Confirm] The secret scratchpad support for 0x7000000 appears to only apply to
		// channels that also honor the SPR bit (VIF, GIF, IPU).  SIF and SPR channels should
		// be tested on real hardware to determine behavior.

		if (addrReg.SPR || ((addr >= PhysMemMap::Scratchpad) && (addr < PhysMemMap::ScratchpadEnd)) )
		{
			return &psSu128(addr);
		}
	}

	u128* result = (u128*)vtlb_GetPhyPtr(addr & 0x1fffffff);
	if (!result)
	{
		if(addr < _256mb)
		{
			// 256mb (PS2 max memory)
			// Such accesses are not documented as causing bus errors but as the memory does
			// not exist, reads should continue to return 0 and writes should be discarded.
			// (note that IOP has similar behavior on its DMAs and some memory accesses).

			return (u128*)(writeToMem ? eeMem->ZeroWrite : eeMem->ZeroRead);
		}
		return NULL;
	}

	return result;
}

u128* ChannelState::GetHostPtr( const tDMAC_RBOR& addrReg, bool writeToMem )
{
	// RBOR is allowed to address physical ram ONLY.

	uint addr = addrReg.ADDR;

	u128* result = (u128*)vtlb_GetPhyPtr(addr & 0x1fffffff);
	if (!result)
	{
		if(addr < _256mb)
		{
			// 256mb (PS2 max memory)
			// Such accesses are not documented as causing bus errors but as the memory does
			// not exist, reads should continue to return 0 and writes should be discarded.
			// (note that IOP has similar behavior on its DMAs and some memory accesses).

			return (u128*)(writeToMem ? eeMem->ZeroWrite : eeMem->ZeroRead);
		}

		wxString msg;
		msg.Printf( L"DMA address error (BUSERR) during MFIFO: 0x%08x [%s]", addrReg.ADDR, writeToMem ? "write" : "read" );
		pxFailDev(msg);
		IrqStall(Stall_BusError);
		throw Exception::DmaBusError();
	}
	return result;
}

u128* ChannelState::GetHostPtr( const tDMAC_ADDR& addrReg, bool writeToMem )
{
	if (u128* retval = TryGetHostPtr(addrReg, writeToMem)) return retval;

	// NULL returned?  Raise a DMA BusError!

	wxString msg;
	msg.Printf( L"DMA address error (BUSERR): 0x%08x", addrReg.ADDR, writeToMem ? "write" : "read" );
	pxFailDev(msg);
	IrqStall(Stall_BusError);
	throw Exception::DmaBusError();

	//return NULL;	// technically unreachable
}

u32 ChannelState::Read32( const tDMAC_ADDR& addr )
{
	return *(u32*)GetHostPtr(addr, false);
}

}		// End Namespace EE_DMAC
