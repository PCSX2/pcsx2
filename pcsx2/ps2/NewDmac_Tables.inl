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

static const ChannelInformation ChannelInfo[NumChannels] =
{
	//							D.S.			S.C.	D.C.	A.S.	SPR
	{ _n(VIF0),		_m(0),	Stall_None,		true,	false,	true,	false,	toVIF0,		fromVIF0	},
	{ _n(VIF1),		_m(1),	Stall_Drain,	true,	false,	true,	false,	toVIF1,		NULL		},
	{ _n(GIF),		_m(2),	Stall_Drain,	true,	false,	true,	false,	toGIF,		NULL		},
	{ _n(fromIPU),	_m(3),	Stall_Source,	false,	false,	false,	false,	NULL,		fromIPU		},
	{ _n(toIPU),	_m(4),	Stall_None,		true,	false,	false,	false,	toIPU,		NULL		},
	{ _n(SIF0),		_m(5),	Stall_Source,	false,	true,	false,	false,	NULL,		fromSIF0	},
	{ _n(SIF1),		_m(6),	Stall_Drain,	true,	false,	false,	false,	toSIF1,		NULL		},
	{ _n(SIF2),		_m(7),	Stall_None,		false,	false,	false,	false,	toSIF2,		fromSIF2	},
	{ _n(fromSPR),	_m(8),	Stall_Source,	false,	true,	false,	true,	NULL,		fromSPR		},
	{ _n(toSPR),	_m(9),	Stall_None,		true,	false,	false,	true,	toSPR,		NULL		},

	// Legend:
	//   D.S.  -- DMA Stall
	//   S.C.  -- Source Chain
	//   D.C.  -- Destination Chain
	//   A.S.  -- Has Address Stack
	//   SPR   -- Scratchpad is the peripheral (uses SADR register for scratchpad src/dest)
};

#undef _m
#undef _n

using namespace EE_DMAC;

static const wxChar* MfifoDrainNames[] =
{
	L"None", L"Reserved", L"VIF1(1)", L"GIF(2)"
};


u128* DMAC_TryGetHostPtr( const tDMAC_ADDR& addrReg, bool writeToMem )
{
	static const uint addr = addrReg.ADDR;

	if (addrReg.SPR) return &psSu128(addr);

	// The DMAC appears to be directly hardwired to various memory banks: Main memory (including
	// ROM), VUs, and the Scratchpad.  It is likely wired to the Hardware Register map as well,
	// since it uses registers internally for accessing some peripherals (such as the GS FIFO
	// regs).

	// Supporting the hardware regs properly is problematic, but fortunately there's no reason
	// a game would likely ever use it, so we don't really support them (the PCSX2 emulated DMA
	// will map to the eeMem->HW memory, but does not invoke any of the indirect read/write handlers).

	if ((addr >= PhysMemMap::Scratchpad) && (addr < PhysMemMap::ScratchpadEnd))
	{
		// Secret scratchpad address for DMA; games typically specify 0x70000000, but chances
		// are the DMAC masks all addresses to MIPS physical memory specification (512MB),
		// which would place SPR between 0x10000000 and 0x10004000.  Unknown yet if that is true
		// so I'm sticking with the 0x70000000 mapping.

		return &psSu128(addr);
	}

	void* result = vtlb_GetPhyPtr(addr);
	if (!result && (addr < _256mb))
	{
		// 256mb (PS2 max memory)
		// Such accesses are not documented as causing bus errors but as the memory does
		// not exist, reads should continue to return 0 and writes should be discarded.
		// (note that IOP has similar behavior on its DMAs and some memory accesses).

		return (u128*)(writeToMem ? eeMem->ZeroWrite : eeMem->ZeroRead);
	}

	return NULL;
}

u128* DMAC_GetHostPtr( const tDMAC_ADDR& addrReg, bool writeToMem )
{
	if (u128* retval = DMAC_TryGetHostPtr(addrReg, writeToMem)) return retval;

	// NULL returned?  Raise a DMA BusError!

	wxString msg;
	msg.Printf( L"DMA address error (BUSERR): 0x%08x", addrReg.ADDR );
	Console.Error(msg);
	pxFailDev(msg);
	throw Exception::DmaRaiseIRQ(L"BusError").BusError();
}

u32 DMAC_Read32( const tDMAC_ADDR& addr )
{
	return *(u32*)DMAC_GetHostPtr(addr, false);
}

}		// End Namespace EE_DMAC

// --------------------------------------------------------------------------------------
//  EE_DMAC::ChannelInformation  (implementations)
// --------------------------------------------------------------------------------------
wxCharBuffer EE_DMAC::ChannelInformation::ToUTF8() const
{
	FastFormatAscii msg;

	if (isSprChannel)
		msg.Write("%s(0x%04x)", NameA, GetRegs().sadr.ADDR);
	else
		msg.Write(NameA);

	return msg.c_str();
}


// --------------------------------------------------------------------------------------
//  EE_DMAC::ChannelState  (implementations)
// --------------------------------------------------------------------------------------
EE_DMAC::ChannelState::ChannelState( ChannelId chanId )
	: Id( chanId )
	, info( ChannelInfo[Id] )
	, creg( info.GetRegs() )
	, chcr( info.CHCR() )
	, madr( info.MADR() )
{
}

DirectionMode EE_DMAC::ChannelState::GetDir() const
{
	const DirectionMode dir = info.GetRawDirection();
	return (dir==Dir_Both) ? (chcr.DIR ? Dir_Drain : Dir_Source) : dir;
}

bool EE_DMAC::ChannelState::DrainStallActive() const
{
	static const ChannelId StallDrainChan[4] = {
		ChanId_None, ChanId_VIF1, ChanId_SIF1, ChanId_GIF
	};

	if (StallDrainChan[dmacRegs.ctrl.STD] != Id) return false;
	if (chcr.MOD == CHAIN_MODE) return (chcr.TAG.ID == TAG_REFS);

	return true;
}

bool EE_DMAC::ChannelState::SourceStallActive() const
{
	static const ChannelId StallSrcChan[4] = {
		ChanId_None, ChanId_SIF0, ChanId_fromSPR, ChanId_fromIPU
	};

	if (StallSrcChan[dmacRegs.ctrl.STD] != Id) return false;
	if (chcr.MOD == CHAIN_MODE) return (chcr.TAG.ID == TAG_CNTS);

	return true;
}

bool EE_DMAC::ChannelState::MFIFOActive() const
{
	static const ChannelId mfifo_chanId[4] = { ChanId_None, ChanId_None, ChanId_VIF1, ChanId_GIF };
	return mfifo_chanId[dmacRegs.ctrl.MFD] == Id;
}

uint EE_DMAC::ChannelState::TransferSource( u128* destMemHost, uint lenQwc, uint destStartQwc, uint destSize ) const
{
	return info.fnptr_xferFrom( destMemHost, destStartQwc, destSize, lenQwc );
}

uint EE_DMAC::ChannelState::TransferDrain( const u128* srcMemHost, uint lenQwc, uint srcStartQwc, uint srcSize ) const
{
	return info.fnptr_xferTo( srcMemHost, srcStartQwc, srcSize, lenQwc );
}

template< typename T >
uint EE_DMAC::ChannelState::TransferDrain( const T& srcBuffer ) const
{
	return info.fnptr_xferTo( (u128*)&srcBuffer, 0, 0, sizeof(T) );
}
