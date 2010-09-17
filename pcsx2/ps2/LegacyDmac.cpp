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

#include "ps2/HwInternal.h"
#include "DmacLegacy.h"

bool DMACh::transfer(const char *s, tDMA_TAG* ptag)
{
	if (ptag == NULL)  					 // Is ptag empty?
	{
		throwBusError(s);
		return false;
	}
    chcrTransfer(ptag);

    qwcTransfer(ptag);
    return true;
}

void DMACh::unsafeTransfer(tDMA_TAG* ptag)
{
    chcrTransfer(ptag);
    qwcTransfer(ptag);
}

tDMA_TAG *DMACh::getAddr(u32 addr, u32 num, bool write)
{
	tDMA_TAG *ptr = dmaGetAddr(addr, write);
	if (ptr == NULL)
	{
		throwBusError("dmaGetAddr");
		setDmacStat(num);
		chcr.STR = false;
	}

	return ptr;
}

tDMA_TAG *DMACh::DMAtransfer(u32 addr, u32 num)
{
	tDMA_TAG *tag = getAddr(addr, num, false);

	if (tag == NULL) return NULL;

    chcrTransfer(tag);
    qwcTransfer(tag);
    return tag;
}

wxString DMACh::cmq_to_str() const
{
	return wxsFormat(L"chcr = %lx, madr = %lx, qwc  = %lx", chcr._u32, madr, qwc);
}

wxString DMACh::cmqt_to_str() const
{
	return wxsFormat(L"chcr = %lx, madr = %lx, qwc  = %lx, tadr = %1x", chcr._u32, madr, qwc, tadr);
}

__fi void throwBusError(const char *s)
{
    Console.Error("%s BUSERR", s);
    dmacRegs.stat.BEIS = true;
}

__fi void setDmacStat(u32 num)
{
	dmacRegs.stat.set_flags(1 << num);
}

// Note: Dma addresses are guaranteed to be aligned to 16 bytes (128 bits)
__fi tDMA_TAG *SPRdmaGetAddr(u32 addr, bool write)
{
	// if (addr & 0xf) { DMA_LOG("*PCSX2*: DMA address not 128bit aligned: %8.8x", addr); }

	//For some reason Getaway references SPR Memory from itself using SPR0, oh well, let it i guess...
	if((addr & 0x70000000) == 0x70000000)
	{
		return (tDMA_TAG*)&eeMem->Scratch[addr & 0x3ff0];
	}

	// FIXME: Why??? DMA uses physical addresses
	addr &= 0x1ffffff0;

	if (addr < Ps2MemSize::Base)
	{
		return (tDMA_TAG*)&eeMem->Main[addr];
	}
	else if (addr < 0x10000000)
	{
		return (tDMA_TAG*)(write ? eeMem->ZeroWrite : eeMem->ZeroRead);
	}
	else if ((addr >= 0x11004000) && (addr < 0x11010000))
	{
		//Access for VU Memory
		return (tDMA_TAG*)vtlb_GetPhyPtr(addr & 0x1FFFFFF0);
	}
	else
	{
		Console.Error( "*PCSX2*: DMA error: %8.8x", addr);
		return NULL;
	}
}

// Note: Dma addresses are guaranteed to be aligned to 16 bytes (128 bits)
__ri tDMA_TAG *dmaGetAddr(u32 addr, bool write)
{
	// if (addr & 0xf) { DMA_LOG("*PCSX2*: DMA address not 128bit aligned: %8.8x", addr); }
	if (DMA_TAG(addr).SPR) return (tDMA_TAG*)&eeMem->Scratch[addr & 0x3ff0];

	// FIXME: Why??? DMA uses physical addresses
	addr &= 0x1ffffff0;

	if (addr < Ps2MemSize::Base)
	{
		return (tDMA_TAG*)&eeMem->Main[addr];
	}
	else if (addr < 0x10000000)
	{
		return (tDMA_TAG*)(write ? eeMem->ZeroWrite : eeMem->ZeroRead);
	}
	else if (addr < 0x10004000)
	{
		// Secret scratchpad address for DMA = end of maximum main memory?
		//Console.Warning("Writing to the scratchpad without the SPR flag set!");
		return (tDMA_TAG*)&eeMem->Scratch[addr & 0x3ff0];
	}
	else
	{
		Console.Error( "*PCSX2*: DMA error: %8.8x", addr);
		return NULL;
	}
}


// Returns true if the DMA is enabled and executed successfully.  Returns false if execution
// was blocked (DMAE or master DMA enabler).
static bool QuickDmaExec( void (*func)(), u32 mem)
{
	bool ret = false;
    DMACh& reg = (DMACh&)psHu32(mem);

	if (reg.chcr.STR && dmacRegs.ctrl.DMAE && !psHu8(DMAC_ENABLER+2))
	{
		func();
		ret = true;
	}

	return ret;
}

