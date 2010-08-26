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

#include "Common.h"
#include "Hardware.h"

#undef dmacRegs
#undef intcRegs

namespace EE_DMAC {

enum StallMode
{
	// No stalling logic is performed (STADR is not read or written)
	Stall_None,
	
	// STADR is written with the MADR after data is transfered.
	Stall_Source,

	// STADR is read and MADR is not allowed to advance beyond that point.
	Stall_Drain,
};

enum DirectionMode
{
	// Indicates a DMA that transfers from peripheral to memory
	Dir_Source,
	
	// Indicates a DMA that transfers from peripheral to memory
	Dir_Drain,
	
	// Indicates a DAM that bases its transfer direction on the DIR bit of its CHCR register.
	Dir_Both,
};

enum ChannelId
{
	ChanId_VIF0 = 0,
	ChanId_VIF1,
	ChanId_GIF,
	ChanId_fromIPU,
	ChanId_toIPU,
	ChanId_SIF0,
	ChanId_SIF1,
	ChanId_SIF2,
	ChanId_fromSPR,
	ChanId_toSPR,

	NumChannels,
	ChanId_None = NumChannels
};


#define __dmacall

// Returns the number of QWC actually transferred.  Return value can be 0, in cases where the
// peripheral has no room to receive data (SIF FIFO is full, or occupied by another channel,
// for example).
typedef uint __dmacall FnType_ToPeripheral(const u128* srcBase, uint srcSize, uint srcStartQwc, uint lenQwc);

// Returns the number of QWC actually transferred.  Return value can be 0, in cases where the
// peripheral has no data to provide (SIF FIFO is empty, or occupied by another channel,
// for example).
typedef uint __dmacall FnType_FromPeripheral(u128* dest, uint destSize, uint destStartQwc, uint lenQwc);

typedef FnType_ToPeripheral*	Fnptr_ToPeripheral;
typedef FnType_FromPeripheral*	Fnptr_FromPeripheral;


// --------------------------------------------------------------------------------------
//  EE_DMAC::ChannelRegisters
// --------------------------------------------------------------------------------------
struct ChannelRegisters
{
	tDMA_CHCR	chcr;
	u32	_null0[3];

	tDMAC_ADDR	madr;
	u32 _null1[3];

	tDMA_QWC	qwc;
	u32 _null2[3];


	tDMAC_ADDR	tadr;
	u32 _null3[3];

	tDMAC_ADDR	asr0;
	u32 _null4[3];

	tDMAC_ADDR	asr1;
	u32 _null5[11];

	tDMA_SADR	sadr;
	u32 _null6[3];
};

// --------------------------------------------------------------------------------------
//  EE_DMAC::DMAtag
// --------------------------------------------------------------------------------------
union DMAtag
{
	struct
	{
		u16 QWC;
		
		struct
		{
			u16 _reserved2	: 10;
			u16 PCE			: 2;
			u16 ID			: 3;
			u16 IRQ			: 1;
		};

		// Upper 32 bits
		tDMAC_ADDR	addr;
	};
	u64 _u64;
	u32 _u32[2];
	u16 _u16[4];

	DMAtag(u64 val) { _u64 = val; }

	// Returns the upper 16 bits of the tag, which is typically stored to the channel's
	// CHCR register during chain mode processing.
	u16 Bits16to31() const { return _u16[1]; }
	
	wxString ToString(DirectionMode dir) const
	{
		const char* label;
		switch(ID)
		{
			case TAG_REFE:
				label = (dir == Dir_Source) ? "CNTS" : "REFE";
			break;

			case TAG_CNT:	label = "CNT";		break;
			case TAG_NEXT:	label = "NEXT";		break;
			case TAG_REF:	label = "REF";		break;
			case TAG_REFS:	label = "REFS";		break;
			case TAG_CALL:	label = "CALL";		break;
			case TAG_RET:	label = "RET";		break;
			case TAG_END:	label = "END";		break;

			default:		label = "????";		break;
		}

		return pxsFmt("%s ADDR=0x%08X%s, QWC=0x%04X, IRQ=%s, PCE=%s",
			addr.ADDR, addr.SPR ? "(SPR)" : "", QWC,
			IRQ ? "on" : "off",
			PCE ? "on" : "off"
		);
	}

	void Clear() { _u64 = 0; }

};

// --------------------------------------------------------------------------------------
//  EE_DMAC::ControllerRegisters
// --------------------------------------------------------------------------------------
struct ControllerRegisters
{
	tDMAC_CTRL	ctrl;
	u32 _padding[3];

	tDMAC_STAT	stat;
	u32 _padding1[3];

	tDMAC_PCR	pcr;
	u32 _padding2[3];


	tDMAC_SQWC	sqwc;
	u32 _padding3[3];

	tDMAC_RBSR	rbsr;
	u32 _padding4[3];

	tDMAC_RBOR	rbor;
	u32 _padding5[3];

	tDMAC_ADDR	stadr;
	u32 _padding6[3];

	__ri u32 mfifoWrapAddr(u32 offset)
	{
		return (rbor.ADDR + (offset & rbsr.RMSK));
	}

	__ri u32 mfifoRingEnd()
	{
		return rbor.ADDR + rbsr.RMSK;
	}

	static ControllerRegisters& Get()
	{
		return (ControllerRegisters&)psHu8(DMAC_CTRL);
	}
};

// --------------------------------------------------------------------------------------
//  Exception::DmaRaiseIRQ
// --------------------------------------------------------------------------------------
// This is a local exception for doing error/IRQ-related flow control within the context
// of the DMAC.  The exception is not (and should never be) leaked to any external contexts.
namespace Exception
{
	class DmaRaiseIRQ
	{
	public:
		bool			m_BusError;
		bool			m_MFIFOstall;
		bool			m_Verbose;
		const wxChar*	m_Cause;

		DmaRaiseIRQ( const wxChar* _cause=NULL )
		{
			m_BusError		= false;
			m_MFIFOstall	= false;
			m_Verbose		= false;
			m_Cause			= _cause;
		}

		DmaRaiseIRQ& BusError()		{ m_BusError	= true; return *this; }
		DmaRaiseIRQ& MFIFOstall()	{ m_MFIFOstall	= true; return *this; }
		DmaRaiseIRQ& Verbose()		{ m_Verbose		= true; return *this; }
		
	};
}

// --------------------------------------------------------------------------------------
//  EE_DMAC::ChannelMetrics
// --------------------------------------------------------------------------------------
struct ChannelMetrics
{
	// total data copied, categorized by LogicalTransferMode.
	u64			qwc[4];

	// times this DMA channel has skipped its arbitration rights due to
	// some stall condition.
	uint		skipped_arbitrations;

	// total number of transfers started for this DMA channel, categorized
	// by LogicalTransferMode.
	// (counted every time STR is set to 1 while DMA is in CHAIN mode)
	uint		xfers[4];

	// total number of CHAIN mode packets transferred since metric was reset.
	uint		chain_packets[tag_id_count];

	// Counting length of the current series of chains (reset to 0 on each
	// STR=1).
	uint		current_chain;

	// Longest series of chain packets transferred in a single DMA kick (ie, from
	// STR=1 until an END tag.
	uint		longest_chain;


	void Reset()
	{
		memzero( *this );
	}
};

// --------------------------------------------------------------------------------------
//  EE_DMAC::ControllerMetrics
// --------------------------------------------------------------------------------------
struct ControllerMetrics
{
	ChannelMetrics		channel[NumChannels];
	
	// Incremented each time the DMAC event hander is entered.
	uint		events;

	// Incremented for each arbitration check performed.  Since some DMAs give up arbitration
	// during their actual transfer logic, multiple arbitrations can be performed per-event.
	// When the DMA Burst hack is enabled, there will usually be significantly fewer events
	// and significantly more arbitration checks.
	uint		arbitration_checks;

	void Reset()
	{
		memzero( channel );
	}
	
	__fi void RecordXfer( ChannelId dmaId, LogicalTransferMode mode, uint qwc )
	{
		if (!IsDevBuild) return;
		++channel[dmaId].xfers[mode];
		channel[dmaId].qwc[mode] += qwc;
		channel[dmaId].current_chain = 0;
	}

	__fi void RecordChainPacket( ChannelId dmaId, tag_id tag, uint qwc )
	{
		if (!IsDevBuild) return;
		++channel[dmaId].chain_packets[tag];
		++channel[dmaId].current_chain;
		channel[dmaId].qwc[CHAIN_MODE] += qwc;
	}

	__fi void RecordChainEnd( ChannelId dmaId )
	{
		if (!IsDevBuild) return;
		if (channel[dmaId].current_chain >= channel[dmaId].longest_chain)
			channel[dmaId].longest_chain = channel[dmaId].current_chain;
	}

	u64 GetQWC(LogicalTransferMode mode) const;
	u64 GetQWC() const;

	uint GetTransferCount(LogicalTransferMode mode) const;
	uint GetTransferCount() const;
};

// --------------------------------------------------------------------------------------
//  EE_DMAC::ChannelInformation
// --------------------------------------------------------------------------------------
struct ChannelInformation
{
	const char*		NameA;
	const wxChar*	NameW;

	uint			regbaseaddr;
	
	StallMode		DmaStall;
	bool			hasSourceChain;
	bool			hasDestChain;
	bool			hasAddressStack;
	bool			isSprChannel;

	// (Drain) Non-Null for channels that can xfer from main memory to peripheral.
	Fnptr_ToPeripheral		fnptr_xferTo;

	// (Source) Non-Null for channels that can xfer from peripheral to main memory.
	Fnptr_FromPeripheral	fnptr_xferFrom;

	DirectionMode GetRawDirection() const
	{
		if (fnptr_xferTo && fnptr_xferFrom) return Dir_Both;
		return fnptr_xferTo ? Dir_Drain : Dir_Source;
	}

	ChannelRegisters& GetRegs() const
	{
		return (ChannelRegisters&)eeMem->HW[regbaseaddr];
	}

	tDMA_CHCR& CHCR() const
	{
		return GetRegs().chcr;
	}

	tDMAC_ADDR& MADR() const
	{
		return GetRegs().madr;
	}

	tDMA_QWC& QWC() const
	{
		return GetRegs().qwc;
	}
	
	tDMAC_ADDR& TADR() const
	{
		pxAssert(hasSourceChain || hasDestChain);
		return GetRegs().tadr;
	}

	tDMAC_ADDR& ASR0() const
	{
		pxAssert(hasAddressStack);
		return GetRegs().asr0;
	}

	tDMAC_ADDR& ASR1() const
	{
		pxAssert(hasAddressStack);
		return GetRegs().asr1;
	}
	
	tDMA_SADR& SADR() const
	{
		pxAssert(isSprChannel);
		return GetRegs().sadr;
		
	}
	
	wxCharBuffer ToUTF8() const;
};

// --------------------------------------------------------------------------------------
//  EE_DMAC::ChannelState
// --------------------------------------------------------------------------------------
class ChannelState
{
protected:
	const ChannelId				Id;
	ControllerRegisters&		dmacReg;
	const ChannelInformation&	info;
	ChannelRegisters&			creg;
	tDMA_CHCR&					chcr;
	tDMAC_ADDR&					madr;

public:
	ChannelState( ChannelId chanId );

	DirectionMode GetDir() const;
	bool DrainStallActive() const;
	bool SourceStallActive() const;
	bool MFIFOActive() const;
	bool TestArbitration();
	void TransferData();

	bool IsSliced()
	{
		return (Id < ChanId_fromSPR);
	}

	bool IsBurst()
	{
		return (Id >= ChanId_fromSPR);
	}

	uint TransferSource(u128* destMemHost, uint lenQwc, uint destStartQwc=0, uint destSize=0) const;
	uint TransferDrain(const u128* srcMemHost, uint lenQwc, uint srcStartQwc=0, uint srcSize=0) const;
	template< typename T >
	uint TransferDrain( const T& srcBuffer ) const;

protected:
	void TransferInterleaveData();
	void TransferNormalAndChainData();

	void MFIFO_SrcChainUpdateTADR();
	void MFIFO_SrcChainUpdateMADR( const DMAtag& tag );

	void SrcChainUpdateTADR();
	void SrcChainUpdateMADR( const DMAtag& tag );

	void DstChainUpdateTADR();
	void DstChainUpdateMADR();
};

}		// namespace EE_DMAC
