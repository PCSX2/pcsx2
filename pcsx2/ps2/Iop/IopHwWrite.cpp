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
#include "IopHw_Internal.h"
#include "Sif.h"
#include "Sio.h"
#include "FW.h"
#include "CDVD/Ps1CD.h"
#include "SPU2/spu2.h"
#include "DEV9/DEV9.h"
#include "USB/USB.h"
#include "IopCounters.h"
#include "IopSio2.h"
#include "IopDma.h"
#include "R3000A.h"

#include "ps2/pgif.h"
#include "Mdec.h"

namespace IopMemory {

using namespace Internal;

// Template-compatible version of the psxHu macro.  Used for writing.
#define psxHu(mem)	(*(u32*)&iopHw[(mem) & 0xffff])


//////////////////////////////////////////////////////////////////////////////////////////
//
template< typename T >
static __fi void _generic_write( u32 addr, T val )
{
	//int bitsize = (sizeof(T) == 1) ? 8 : ( (sizeof(T) == 2) ? 16 : 32 );
	IopHwTraceLog<T>( addr, val, false );
	psxHu(addr) = val;
}

void iopHwWrite8_generic( u32 addr, mem8_t val )		{ _generic_write<mem8_t>( addr, val ); }
void iopHwWrite16_generic( u32 addr, mem16_t val )	{ _generic_write<mem16_t>( addr, val ); }
void iopHwWrite32_generic( u32 addr, mem32_t val )	{ _generic_write<mem32_t>( addr, val ); }

//////////////////////////////////////////////////////////////////////////////////////////
//
template< typename T >
static __fi T _generic_read( u32 addr )
{
	//int bitsize = (sizeof(T) == 1) ? 8 : ( (sizeof(T) == 2) ? 16 : 32 );

	T ret = psxHu(addr);
	IopHwTraceLog<T>( addr, ret, true );
	return ret;
}

mem8_t iopHwRead8_generic( u32 addr )	{ return _generic_read<mem8_t>( addr ); }
mem16_t iopHwRead16_generic( u32 addr )	{ return _generic_read<mem16_t>( addr ); }
mem32_t iopHwRead32_generic( u32 addr )	{ return _generic_read<mem32_t>( addr ); }


//////////////////////////////////////////////////////////////////////////////////////////
//
void iopHwWrite8_Page1( u32 addr, mem8_t val )
{
	// all addresses are assumed to be prefixed with 0x1f801xxx:
	pxAssert( (addr >> 12) == 0x1f801 );

	u32 masked_addr = pgmsk( addr );

	switch( masked_addr )
	{
		mcase(HW_SIO_DATA): sioWrite8( val ); break;

		// for use of serial port ignore for now
		//case 0x50: serial_write8( val ); break;

		mcase(HW_DEV9_DATA): DEV9write8( addr, val ); break;

		mcase(HW_CDR_DATA0): cdrWrite0( val ); break;
		mcase(HW_CDR_DATA1): cdrWrite1( val ); break;
		mcase(HW_CDR_DATA2): cdrWrite2( val ); break;
		mcase(HW_CDR_DATA3): cdrWrite3( val ); break;

		default:
			if( masked_addr >= 0x100 && masked_addr < 0x130 )
			{
				DbgCon.Warning( "HwWrite8 to Counter16 [ignored] @ addr 0x%08x = 0x%02x", addr, psxHu8(addr) );
				psxHu8( addr ) = val;
			}
			else if( masked_addr >= 0x480 && masked_addr < 0x4a0 )
			{
				DbgCon.Warning( "HwWrite8 to Counter32 [ignored] @ addr 0x%08x = 0x%02x", addr, psxHu8(addr) );
				psxHu8( addr ) = val;
			}
			else if( (masked_addr >= pgmsk(HW_USB_START)) && (masked_addr < pgmsk(HW_USB_END)) )
			{
				USBwrite8( addr, val );
			}
			else
			{
				psxHu8(addr) = val;
			}
		break;
	}

	IopHwTraceLog<mem8_t>( addr, val, false );
}

void iopHwWrite8_Page3( u32 addr, mem8_t val )
{
	// all addresses are assumed to be prefixed with 0x1f803xxx:
	pxAssert( (addr >> 12) == 0x1f803 );

	if( SysConsole.iopConsole.IsActive() && (addr == 0x1f80380c) )	// STDOUT
	{
		static char pbuf[1024];
		static int pidx;
		static bool included_newline = false;

		if (val == '\r')
		{
			included_newline = true;
			pbuf[pidx++] = '\n';
		}
		else if (!included_newline || (val != '\n'))
		{
			included_newline = false;
			pbuf[pidx++] = val;
		}

		if ((pidx == std::size(pbuf)-1) || (pbuf[pidx-1] == '\n'))
		{
			pbuf[pidx] = 0;
			iopConLog( ShiftJIS_ConvertString(pbuf) );
			pidx = 0;
		}
	}

	psxHu8( addr ) = val;
	IopHwTraceLog<mem8_t>( addr, val, false );
}

void iopHwWrite8_Page8( u32 addr, mem8_t val )
{
	// all addresses are assumed to be prefixed with 0x1f808xxx:
	pxAssert( (addr >> 12) == 0x1f808 );

	if( addr == HW_SIO2_DATAIN )	// sio2 serial data feed input
		sio2_serialIn( val );
	else
		psxHu8( addr ) = val;

	IopHwTraceLog<mem8_t>( addr, val, false );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Templated handler for both 32 and 16 bit write operations, to Page 1 registers.
//
template< typename T >
static __fi void _HwWrite_16or32_Page1( u32 addr, T val )
{
	// all addresses are assumed to be prefixed with 0x1f801xxx:
	pxAssert( (addr >> 12) == 0x1f801 );

	// all addresses should be aligned to the data operand size:
	pxAssert(
		( sizeof(T) == 2 && (addr & 1) == 0 ) ||
		( sizeof(T) == 4 && (addr & 3) == 0 )
	);

	u32 masked_addr = addr & 0x0fff;

	// ------------------------------------------------------------------------
	// Counters, 16-bit varieties!
	//
	if( masked_addr >= 0x100 && masked_addr < 0x130 )
	{
		int cntidx = ( masked_addr >> 4 ) & 0xf;
		switch( masked_addr & 0xf )
		{
			case 0x0:
				psxRcntWcount16( cntidx, val );
			break;

			case 0x4:
				psxRcntWmode16( cntidx, val );
			break;

			case 0x8:
				psxRcntWtarget16( cntidx, val );
			break;

			default:
				psxHu(addr) = val;
			break;
		}
	}
	// ------------------------------------------------------------------------
	// Counters, 32-bit varieties!
	//
	else if( masked_addr >= 0x480 && masked_addr < 0x4b0 )
	{
		int cntidx = (( masked_addr >> 4 ) & 0xf) - 5;
		switch( masked_addr & 0xf )
		{
			case 0x0:
				psxRcntWcount32( cntidx, val );
			break;

			case 0x2:	// Count HiWord
				psxRcntWcount32( cntidx, (u32)val << 16 );
			break;

			case 0x4:
				psxRcntWmode32( cntidx, val );
			break;

			case 0x8:
				psxRcntWtarget32( cntidx, val );
			break;

			case 0xa:	// Target HiWord
				psxRcntWtarget32( cntidx, (u32)val << 16);
			break;

			default:
				psxHu(addr) = val;
			break;
		}
	}
	// ------------------------------------------------------------------------
	// USB, with both 16 and 32 bit interfaces
	//
	else if( (masked_addr >= pgmsk(HW_USB_START)) && (masked_addr < pgmsk(HW_USB_END)) )
	{
		if( sizeof(T) == 2 ) USBwrite16( addr, val ); else USBwrite32( addr, val );
	}
	// ------------------------------------------------------------------------
	// SPU2, accessible in 16 bit mode only!
	//
	else if( (masked_addr >= pgmsk(HW_SPU2_START)) && (masked_addr < pgmsk(HW_SPU2_END)) )
	{
		if( sizeof(T) == 2 )
			SPU2write( addr, val );
		else
		{
			DbgCon.Warning( "HwWrite32 to SPU2? @ 0x%08X .. What manner of trickery is this?!", addr );
			//psxHu(addr) = val;
		}
	}
	// ------------------------------------------------------------------------
	// PS1 GPU access
	//
	else if( (masked_addr >= pgmsk(HW_PS1_GPU_START)) && (masked_addr < pgmsk(HW_PS1_GPU_END)) )
	{
		// todo: psx mode: this is new
		if( sizeof(T) == 2 )
			DevCon.Warning( "HwWrite16 to PS1 GPU? @ 0x%08X .. What manner of trickery is this?!", addr );

		pxAssert(sizeof(T) == 4);

		psxDma2GpuW(addr, val);
	}
	else
	{
		switch( masked_addr )
		{
			// ------------------------------------------------------------------------
			mcase(HW_SIO_DATA):
				sioWrite8( val & 0xFF );
				sioWrite8( (val >> 8) & 0xFF );
				if( sizeof(T) == 4 )
				{
					// u32 gets rid of compiler warnings when using the u16 version of this template
					sioWrite8( ((u32)val >> 16) & 0xFF );
					sioWrite8( ((u32)val >> 24) & 0xFF );
				}
			break;

			mcase(HW_SIO_STAT):		// read-only?
				//regname = "SIO_STAT (read-only?)";
				//sio.StatReg;
			break;

			mcase(HW_SIO_MODE):
				sio.ModeReg = (u16)val;
				if( sizeof(T) == 4 )
				{
					// My guess on 32-bit accesses.  Dunno yet what the real hardware does. --air
					sio.CtrlReg = (u16)((u32)val >> 16);
				}
			break;

			mcase(HW_SIO_CTRL):
				//sio.CtrlReg = (u16)val;
				sioWriteCtrl16((u16)val);
			break;

			mcase(HW_SIO_BAUD):
				sio.BaudReg = (u16)val;
			break;

			// ------------------------------------------------------------------------
			//Serial port stuff not support now ;P
			// case 0x050: serial_write16( val ); break;
			//	case 0x054: serial_status_write( val ); break;
			//	case 0x05a: serial_control_write( val ); break;
			//	case 0x05e: serial_baud_write( val ); break;

			mcase(HW_IREG):
				psxHu(addr) &= val;
				if ((val == 0xffffffff) ) {
					psxHu32(addr) |= 1 << 2;
					psxHu32(addr) |= 1 << 3;
				}
			break;

			mcase(HW_IREG+2):
				psxHu(addr) &= val;
			break;

			mcase(HW_IMASK):
				psxHu(addr) = val;
				iopTestIntc();
			break;

			mcase(HW_IMASK+2):
				psxHu(addr) = val;
				iopTestIntc();
			break;

			mcase(HW_ICTRL):
				psxHu(addr) = val;
				iopTestIntc();
			break;

			mcase(HW_ICTRL+2):
				psxHu(addr) = val;
				iopTestIntc();
			break;

			// ------------------------------------------------------------------------
			//
			
			mcase(0x1f801088) :	// DMA0 CHCR -- MDEC IN
				// psx mode
				HW_DMA0_CHCR = val;
				psxDma0(HW_DMA0_MADR, HW_DMA0_BCR, HW_DMA0_CHCR);
			break;

			mcase(0x1f801098):	// DMA1 CHCR -- MDEC OUT
				// psx mode
				HW_DMA1_CHCR = val;
				psxDma1(HW_DMA1_MADR, HW_DMA1_BCR, HW_DMA1_CHCR);
			break;
			mcase(0x1f8010ac):
				DevCon.Warning("SIF2 IOP TADR?? write");
				psxHu(addr) = val;
			break;

			mcase(0x1f8010a8) :	// DMA2 CHCR -- GPU
				// BIOS functions
				// send_gpu_linked_list: [1F8010A8h]=1000401h
				// gpu_abort_dma: [1F8010A8h]=401h
				// gpu_send_dma: [1F8010A8h]=1000201h
				psxHu(addr) = val;
				DmaExec(2);
			break;

			mcase(0x1f8010b8):	// DMA3 CHCR -- CDROM
				psxHu(addr) = val;
				DmaExec(3);
			break;

			mcase(0x1f8010c8):	// DMA4 CHCR -- SPU2 Core 1
				psxHu(addr) = val;
				DmaExec(4);
			break;

			mcase(0x1f8010e8):	// DMA6 CHCR -- OT clear
				psxHu(addr) = val;
				DmaExec(6);
			break;

			mcase(0x1f801508):	// DMA7 CHCR -- SPU2 core 2
				psxHu(addr) = val;
				DmaExec2(7);
			break;

			mcase(0x1f801518):	// DMA8 CHCR -- DEV9
				psxHu(addr) = val;
				DmaExec2(8);
			break;

			mcase(0x1f801528):	// DMA9 CHCR -- SIF0
				psxHu(addr) = val;
				DmaExec2(9);
			break;

			mcase(0x1f801538):	// DMA10 CHCR -- SIF1
				psxHu(addr) = val;
				DmaExec2(10);
			break;


			mcase(0x1f801548):	// DMA11 CHCR -- SIO2 IN
				psxHu(addr) = val;
				DmaExec2(11);
			break;

			mcase(0x1f801558):	// DMA12 CHCR -- SIO2 OUT
				psxHu(addr) = val;
				DmaExec2(12);
			break;

			// ------------------------------------------------------------------------
			// DMA ICR handlers -- General XOR behavior!

			mcase(0x1f8010f4):
			{
				//u32 tmp = (~val) & HW_DMA_ICR;
				//u32 old = ((tmp ^ val) & 0xffffff) ^ tmp;
				///psxHu(addr) = ((tmp ^ val) & 0xffffff) ^ tmp;
				u32 newtmp = (HW_DMA_ICR & 0xff000000) | (val & 0xffffff);
				newtmp &= ~(val & 0x7F000000);
				if (((newtmp >> 15) & 0x1) || (((newtmp >> 23) & 0x1) == 0x1 && (((newtmp & 0x7F000000) >> 8) & (newtmp & 0x7F0000)) != 0)) {
					newtmp |= 0x80000000;
				}
				else {
					newtmp &= ~0x80000000;
				}
				//if (newtmp != old)
				//	DevCon.Warning("ICR Old %x New %x", old, newtmp);
				psxHu(addr) = newtmp;
				if ((HW_DMA_ICR >> 15) & 0x1) {
					DevCon.Warning("Force ICR IRQ!");
					psxRegs.CP0.n.Cause &= ~0x7C;
					iopIntcIrq(3);
				}
				else {
					psxDmaInterrupt(33);
				}
			}				
			break;
			
			mcase(0x1f8010f6):		// ICR_hi (16 bit?) [dunno if it ever happens]
			{
				DevCon.Warning("High ICR Write!!");
				const u32 val2 = (u32)val << 16;
				const u32 tmp = (~val2) & HW_DMA_ICR;
				psxHu(addr) = (((tmp ^ val2) & 0xffffff) ^ tmp) >> 16;
			}
			break;

			mcase(0x1f801574):
			{
				/*u32 tmp = (~val) & HW_DMA_ICR2;
				psxHu(addr) = ((tmp ^ val) & 0xffffff) ^ tmp;*/
				//u32 tmp = (~val) & HW_DMA_ICR2;
				//u32 old = ((tmp ^ val) & 0xffffff) ^ tmp;
				///psxHu(addr) = ((tmp ^ val) & 0xffffff) ^ tmp;
				u32 newtmp = (HW_DMA_ICR2 & 0xff000000) | (val & 0xffffff);
				newtmp &= ~(val & 0x7F000000);
				if (((newtmp >> 15) & 0x1) || (((newtmp >> 23) & 0x1) == 0x1 && (((newtmp & 0x7F000000) >> 8) & (newtmp & 0x7F0000)) != 0)) {
					newtmp |= 0x80000000;
				}
				else {
					newtmp &= ~0x80000000;
				}
				//if (newtmp != old)
				//	DevCon.Warning("ICR2 Old %x New %x", old, newtmp);
				psxHu(addr) = newtmp;
				if ((HW_DMA_ICR2 >> 15) & 0x1) {
					DevCon.Warning("Force ICR2 IRQ!");
					psxRegs.CP0.n.Cause &= ~0x7C;
					iopIntcIrq(3);
				}
				else {
					psxDmaInterrupt2(33);
				}
			}
			break;

			mcase(0x1f801576):		// ICR2_hi (16 bit?) [dunno if it ever happens]
			{
				DevCon.Warning("ICR2 high write!");
				const u32 val2 = (u32)val << 16;
				const u32 tmp = (~val2) & HW_DMA_ICR2;
				psxHu(addr) = (((tmp ^ val2) & 0xffffff) ^ tmp) >> 16;
			}
			break;

			// ------------------------------------------------------------------------
			// Legacy GPU  emulation
			//

			mcase(HW_PS1_GPU_DATA) : // HW_PS1_GPU_DATA = 0x1f801810
				psxHu(addr) = val; // guess
				psxGPUw(addr, val);
			break;
			mcase (HW_PS1_GPU_STATUS): // HW_PS1_GPU_STATUS = 0x1f801814
				psxHu(addr) = val; // guess
				psxGPUw(addr, val);
			break;
			mcase (0x1f801820): // MDEC
				psxHu(addr) = val; // guess
				mdecWrite0(val);
			break;
			mcase (0x1f801824): // MDEC
				psxHu(addr) = val; // guess
				mdecWrite1(val);
			break;

				// ------------------------------------------------------------------------

			mcase(HW_DEV9_DATA):
				DEV9write16( addr, val );
				psxHu(addr) = val;
			break;

			default:
				psxHu(addr) = val;
			break;
		}
	}

	IopHwTraceLog<T>( addr, val, false );
}


//////////////////////////////////////////////////////////////////////////////////////////
//
void iopHwWrite16_Page1( u32 addr, mem16_t val )
{
	_HwWrite_16or32_Page1<mem16_t>( addr, val );
}

void iopHwWrite16_Page3( u32 addr, mem16_t val )
{
	// all addresses are assumed to be prefixed with 0x1f803xxx:
	pxAssert( (addr >> 12) == 0x1f803 );
	psxHu16(addr) = val;
	IopHwTraceLog<mem16_t>( addr, val, false );
}

void iopHwWrite16_Page8( u32 addr, mem16_t val )
{
	// all addresses are assumed to be prefixed with 0x1f808xxx:
	pxAssert( (addr >> 12) == 0x1f808 );
	psxHu16(addr) = val;
	IopHwTraceLog<mem16_t>( addr, val, false );
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void iopHwWrite32_Page1( u32 addr, mem32_t val )
{
	_HwWrite_16or32_Page1<mem32_t >( addr, val );
}

void iopHwWrite32_Page3( u32 addr, mem32_t val )
{
	// all addresses are assumed to be prefixed with 0x1f803xxx:
	pxAssert( (addr >> 12) == 0x1f803 );
	psxHu16(addr) = val;
	IopHwTraceLog<mem32_t>( addr, val, false );
}

void iopHwWrite32_Page8( u32 addr, mem32_t val )
{
	// all addresses are assumed to be prefixed with 0x1f808xxx:
	pxAssert( (addr >> 12) == 0x1f808 );

	u32 masked_addr = addr & 0x0fff;

	if( masked_addr >= 0x200 )
	{
		if( masked_addr < 0x240 )
		{
			const int parm = (masked_addr-0x200) / 4;
			sio2_setSend3( parm, val );
		}
		else if( masked_addr < 0x260 )
		{
			// SIO2 Send commands alternate registers.  First reg maps to Send1, second
			// to Send2, third to Send1, etc.  And the following clever code does this:

			const int parm = (masked_addr-0x240) / 8;
			if(masked_addr & 4) sio2_setSend2( parm, val ); else sio2_setSend1( parm, val );
		}
		else if( masked_addr <= 0x280 )
		{
			switch( masked_addr )
			{
				mcase(HW_SIO2_CTRL):	sio2_setCtrl( val );	break;
				mcase(0x1f808278):		sio2_set8278( val );	break;
				mcase(0x1f80827C):		sio2_set827C( val );	break;
				mcase(HW_SIO2_INTR):	sio2_setIntr( val );	break;

				// Other SIO2 registers are read-only, no-ops on write.
				default:
					psxHu32(addr) = val;
				break;
			}
		}
		else if( masked_addr >= pgmsk(HW_FW_START) && masked_addr <= pgmsk(HW_FW_END) )
		{
			FWwrite32( addr, val );
		}
	}
	else psxHu32(addr) = val;

	IopHwTraceLog<mem32_t>( addr, val, false );
}

}
