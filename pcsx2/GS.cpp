/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "PrecompiledHeader.h"

#include <list>

#include "Common.h"
#include "VU.h"
#include "GS.h"
#include "iR5900.h"

using namespace Threading;
using namespace std;

using namespace R5900;

#ifdef DEBUG
#define MTGS_LOG SysPrintf
#else
#define MTGS_LOG 0&&
#endif

static bool m_gsOpened = false;

int g_FFXHack=0;

#ifdef PCSX2_DEVBUILD

// GS Playback
int g_SaveGSStream = 0; // save GS stream; 1 - prepare, 2 - save
int g_nLeftGSFrames = 0; // when saving, number of frames left
gzSavingState* g_fGSSave;

void GSGIFTRANSFER1(u32 *pMem, u32 addr) { 
	if( g_SaveGSStream == 2) { 
		u32 type = GSRUN_TRANS1; 
		u32 size = (0x4000-(addr))/16;
		g_fGSSave->Freeze( type );
		g_fGSSave->Freeze( size );
		g_fGSSave->FreezeMem( ((u8*)pMem)+(addr), size*16 );
	} 
	GSgifTransfer1(pMem, addr); 
}

void GSGIFTRANSFER2(u32 *pMem, u32 size) { 
	if( g_SaveGSStream == 2) { 
		u32 type = GSRUN_TRANS2; 
		u32 _size = size; 
		g_fGSSave->Freeze( type );
		g_fGSSave->Freeze( size );
		g_fGSSave->FreezeMem( pMem, _size*16 );
	} 
	GSgifTransfer2(pMem, size); 
}

void GSGIFTRANSFER3(u32 *pMem, u32 size) { 
	if( g_SaveGSStream == 2 ) { 
		u32 type = GSRUN_TRANS3; 
		u32 _size = size; 
		g_fGSSave->Freeze( type );
		g_fGSSave->Freeze( size );
		g_fGSSave->FreezeMem( pMem, _size*16 );
	} 
	GSgifTransfer3(pMem, size); 
}

__forceinline void GSVSYNC(void) { 
	if( g_SaveGSStream == 2 ) { 
		u32 type = GSRUN_VSYNC; 
		g_fGSSave->Freeze( type ); 
	} 
}
#else

__forceinline void GSGIFTRANSFER1(u32 *pMem, u32 addr) { 
	GSgifTransfer1(pMem, addr); 
}

__forceinline void GSGIFTRANSFER2(u32 *pMem, u32 size) { 
	GSgifTransfer2(pMem, size); 
}

__forceinline void GSGIFTRANSFER3(u32 *pMem, u32 size) { 
	GSgifTransfer3(pMem, size); 
}

__forceinline void GSVSYNC(void) { 
} 
#endif

u32 CSRw;

#ifdef PCSX2_VIRTUAL_MEM
#define gif ((DMACh*)&PS2MEM_HW[0xA000])
#else
#define gif ((DMACh*)&psH[0xA000])
#endif

#ifdef PCSX2_VIRTUAL_MEM
#define PS2GS_BASE(mem) ((PS2MEM_BASE+0x12000000)+(mem&0x13ff))
#else
PCSX2_ALIGNED16( u8 g_RealGSMem[0x2000] );
#define PS2GS_BASE(mem) (g_RealGSMem+(mem&0x13ff))
#endif

extern int m_nCounters[];

// FrameSkipping Stuff
// Yuck, iSlowStart is needed by the MTGS, so can't make it static yet.

u64 m_iSlowStart=0;
static s64 m_iSlowTicks=0;
static bool m_justSkipped = false;
static bool m_StrictSkipping = false;

void _gs_ChangeTimings( u32 framerate, u32 iTicks )
{
	m_iSlowStart = GetCPUTicks();

	u32 frameSkipThreshold = Config.CustomFrameSkip*50;
	if( Config.CustomFrameSkip == 0)
	{
		// default: load the frameSkipThreshold with a value roughly 90% of our current framerate
		frameSkipThreshold = ( framerate * 242 ) / 256;
	}

	m_iSlowTicks = ( GetTickFrequency() * 50 ) / frameSkipThreshold;

	// sanity check against users who set a "minimum" frame that's higher
	// than the maximum framerate.  Also, if framerates are within 1/3300th
	// of a second of each other, assume strict skipping (it's too close,
	// and could cause excessive skipping).

	if( m_iSlowTicks <= (iTicks + ((s64)GetTickFrequency()/3300)) )
	{
		m_iSlowTicks = iTicks;
		m_StrictSkipping = true;
	}
}

static void gsOnModeChanged(  u32 framerate, u32 newTickrate )
{
	if( mtgsThread != NULL )
		mtgsThread->SendSimplePacket( GS_RINGTYPE_MODECHANGE, framerate, newTickrate, 0 );
	else
		_gs_ChangeTimings( framerate, newTickrate );
}

void gsSetVideoRegionType( u32 isPal )
{
	u32 framerate;
	
	if( isPal )
	{
		if( Config.PsxType & 1 ) return;
		Console::WriteLn( "PAL Display Mode Initialized." );
		Config.PsxType |= 1;
		framerate = FRAMERATE_PAL;
	}
	else
	{
		if( !(Config.PsxType & 1 ) ) return;
		Console::WriteLn( "NTSC Display Mode Initialized." );
		Config.PsxType &= ~1;
		framerate = FRAMERATE_NTSC;
	}

	u32 newTickrate = UpdateVSyncRate();
}


// Initializes MultiGS ringbuffer and registers.
// Make sure framelimiter options are in sync with the plugin's capabilities.
void gsInit()
{
	switch(CHECK_FRAMELIMIT)
	{
		case PCSX2_FRAMELIMIT_SKIP:
		case PCSX2_FRAMELIMIT_VUSKIP:
			if( GSsetFrameSkip == NULL )
			{
				Config.Options &= ~PCSX2_FRAMELIMIT_MASK;
				Console::WriteLn("Notice: Disabling frameskip -- GS plugin does not support it.");
			}
		break;
	}
}

// Opens the gsRingbuffer thread.
s32 gsOpen()
{
	if( m_gsOpened ) return 0;

	// mtgs overrides these as necessary...
	GSsetBaseMem( PS2MEM_GS );
	GSirqCallback( gsIrq );

	//video
	// Only bind the gsIrq if we're not running the MTGS.
	// The MTGS simulates its own gsIrq in order to maintain proper sync.

	m_gsOpened = mtgsOpen();
	if( !m_gsOpened )
	{
		// MTGS failed to init or is disabled.  Try the GS instead!
		// ... and set the memptr again just in case (for switching between GS/MTGS on the fly)

		m_gsOpened = !GSopen((void *)&pDsp, "PCSX2", 0);
	}

	if( m_gsOpened )
	{
		gsOnModeChanged(
			(Config.PsxType & 1) ? FRAMERATE_PAL : FRAMERATE_NTSC,
			UpdateVSyncRate()
		);
	}
	return !m_gsOpened;
}

void gsClose()
{
	if( !m_gsOpened ) return;

	// Throw an assert if our multigs setting and mtgsThread status
	// aren't synched.  It shouldn't break the code anyway but it's a
	// bad coding habit that we should catch and fix early.
	assert( !!CHECK_MULTIGS == (mtgsThread != NULL ) );

	if( mtgsThread != NULL )
	{
		safe_delete( mtgsThread );
	}
	else
		GSclose();

	m_gsOpened = false;
}

void gsReset()
{
	// Sanity check in case the plugin hasn't been initialized...
	if( !m_gsOpened ) return;

	if( mtgsThread != NULL )
		mtgsThread->Reset();
	else
	{
		Console::Notice( "GIF reset");
		GSreset();
		GSsetFrameSkip(0);
	}

	gsOnModeChanged(
		(Config.PsxType & 1) ? FRAMERATE_PAL : FRAMERATE_NTSC,
		UpdateVSyncRate() 
	);

#ifndef PCSX2_VIRTUAL_MEM
	memset(g_RealGSMem, 0, sizeof( g_RealGSMem ));
#endif

	Path3transfer = 0;

	GSCSRr = 0x551B400F;   // Set the FINISH bit to 1 for now
	GSIMR = 0x7f00;
	psHu32(GIF_STAT) = 0;
	psHu32(GIF_CTRL) = 0;
	psHu32(GIF_MODE) = 0;
}

static bool _gsGIFSoftReset( int mask )
{
	if( GSgifSoftReset == NULL )
	{
		static bool warned = false;
		if( !warned )
		{
			Console::Notice( "GIF Warning > Soft reset requested, but the GS plugin doesn't support it!" );
			warned = true;
		}
		return false;
	}

	if( mtgsThread != NULL )
		mtgsThread->GIFSoftReset( mask );
	else
		GSgifSoftReset( mask );

	return true;
}

void gsGIFReset()
{
#ifndef PCSX2_VIRTUAL_MEM
	// fixme - should this be here? (air)
	memset(g_RealGSMem, 0, sizeof( g_RealGSMem ));
#endif

	// perform a soft reset (but do not do a full reset if the soft reset API is unavailable)
	_gsGIFSoftReset( 7 );

	GSCSRr = 0x551B400F;   // Set the FINISH bit to 1 for now
	GSIMR = 0x7f00;
	psHu32(GIF_STAT) = 0;
	psHu32(GIF_CTRL) = 0;
	psHu32(GIF_MODE) = 0;
}

void gsCSRwrite(u32 value)
{
	CSRw |= value & ~0x60;

	if( mtgsThread != NULL )
		mtgsThread->SendSimplePacket( GS_RINGTYPE_WRITECSR, CSRw, 0, 0 );
	else
		GSwriteCSR(CSRw);

	GSCSRr = ((GSCSRr&~value)&0x1f)|(GSCSRr&~0x1f);

	// Our emulated GS has no FIFO...
	/*if( value & 0x100 ) { // FLUSH
		//SysPrintf("GS_CSR FLUSH GS fifo: %x (CSRr=%x)\n", value, GSCSRr);
	}*/

	if (value & 0x200) { // resetGS

		// perform a soft reset -- and fall back to doing a full reset if the plugin doesn't
		// support soft resets.

		if( !_gsGIFSoftReset( 7 ) )
		{
			if( mtgsThread != NULL )
				mtgsThread->SendSimplePacket( GS_RINGTYPE_RESET, 0, 0, 0 );
			else
				GSreset();
		}

		GSCSRr = 0x551B400F;   // Set the FINISH bit to 1 - GS is always at a finish state as we don't have a FIFO(saqib)
		GSIMR = 0x7F00; //This is bits 14-8 thats all that should be 1
	}
}

static void IMRwrite(u32 value) {
	GSIMR = (value & 0x1f00)|0x6000;

	// don't update mtgs mem
}

void gsWrite8(u32 mem, u8 value) {
	switch (mem) {
		case 0x12001000: // GS_CSR
			gsCSRwrite((CSRw & ~0x000000ff) | value); break;
		case 0x12001001: // GS_CSR
			gsCSRwrite((CSRw & ~0x0000ff00) | (value <<  8)); break;
		case 0x12001002: // GS_CSR
			gsCSRwrite((CSRw & ~0x00ff0000) | (value << 16)); break;
		case 0x12001003: // GS_CSR
			gsCSRwrite((CSRw & ~0xff000000) | (value << 24)); break;
		default:
			*PS2GS_BASE(mem) = value;

			if( mtgsThread != NULL )
				mtgsThread->SendSimplePacket(GS_RINGTYPE_MEMWRITE8, mem&0x13ff, value, 0);
	}
	GIF_LOG("GS write 8 at %8.8lx with data %8.8lx\n", mem, value);
}

void gsWrite16(u32 mem, u16 value) {
	
	GIF_LOG("GS write 16 at %8.8lx with data %8.8lx\n", mem, value);

	switch (mem) {
		case 0x12000010: // GS_SMODE1
			gsSetVideoRegionType( (value & 0x6000) == 0x6000 );
			break;
			
		case 0x12000020: // GS_SMODE2
			if(value & 0x1) Config.PsxType |= 2; // Interlaced
			else Config.PsxType &= ~2;	// Non-Interlaced
			break;
			
		case 0x12001000: // GS_CSR
			gsCSRwrite( (CSRw&0xffff0000) | value);
			return; // do not write to MTGS memory
		case 0x12001002: // GS_CSR
			gsCSRwrite( (CSRw&0xffff) | ((u32)value<<16));
			return; // do not write to MTGS memory
		case 0x12001010: // GS_IMR
			//SysPrintf("writing to IMR 16\n");
			IMRwrite(value);
			return; // do not write to MTGS memory
	}

	*(u16*)PS2GS_BASE(mem) = value;

	if( mtgsThread != NULL )
		mtgsThread->SendSimplePacket(GS_RINGTYPE_MEMWRITE16, mem&0x13ff, value, 0);
}

void gsWrite32(u32 mem, u32 value)
{
	assert( !(mem&3));
	GIF_LOG("GS write 32 at %8.8lx with data %8.8lx\n", mem, value);

	switch (mem) {
		case 0x12000010: // GS_SMODE1
			gsSetVideoRegionType( (value & 0x6000) == 0x6000 );
		break;

		case 0x12000020: // GS_SMODE2
			if(value & 0x1) Config.PsxType |= 2; // Interlaced
			else Config.PsxType &= ~2;	// Non-Interlaced
			break;
			
		case 0x12001000: // GS_CSR
			gsCSRwrite(value);
			return;

		case 0x12001010: // GS_IMR
			IMRwrite(value);
			return;
	}

	*(u32*)PS2GS_BASE(mem) = value;

	if( mtgsThread != NULL )
		mtgsThread->SendSimplePacket(GS_RINGTYPE_MEMWRITE32, mem&0x13ff, value, 0);
}

void gsWrite64(u32 mem, u64 value) {

	GIF_LOG("GS write 64 at %8.8lx with data %8.8lx_%8.8lx\n", mem, ((u32*)&value)[1], (u32)value);

	switch (mem) {
		case 0x12000010: // GS_SMODE1
			gsSetVideoRegionType( (value & 0x6000) == 0x6000 );
			break;

		case 0x12000020: // GS_SMODE2
			if(value & 0x1) Config.PsxType |= 2; // Interlaced
			else Config.PsxType &= ~2;	// Non-Interlaced
			break;

		case 0x12001000: // GS_CSR
			gsCSRwrite((u32)value);
			return;

		case 0x12001010: // GS_IMR
			IMRwrite((u32)value);
			return;
	}

	*(u64*)PS2GS_BASE(mem) = value;

	if( mtgsThread != NULL )
		mtgsThread->SendSimplePacket(GS_RINGTYPE_MEMWRITE64, mem&0x13ff, (u32)value, (u32)(value>>32));
}

u8 gsRead8(u32 mem)
{
	GIF_LOG("GS read 8 from %8.8lx  value: %8.8lx\n", mem, *(u8*)PS2GS_BASE(mem));
	return *(u8*)PS2GS_BASE(mem);
}

u16 gsRead16(u32 mem)
{
	GIF_LOG("GS read 16 from %8.8lx  value: %8.8lx\n", mem, *(u16*)PS2GS_BASE(mem));
	return *(u16*)PS2GS_BASE(mem);
}

u32 gsRead32(u32 mem) 
{
	GIF_LOG("GS read 32 from %8.8lx  value: %8.8lx\n", mem, *(u32*)PS2GS_BASE(mem));
	return *(u32*)PS2GS_BASE(mem);
}

u64 gsRead64(u32 mem)
{
	GIF_LOG("GS read 64 from %8.8lx  value: %8.8lx_%8.8lx\n", mem, *(u32*)PS2GS_BASE(mem+4), *(u32*)PS2GS_BASE(mem) );
	return *(u64*)PS2GS_BASE(mem);
}

void gsIrq() {
	hwIntcIrq(0);
}

static int gspath3done=0;
int gscycles = 0;

__forceinline void gsInterrupt() {
	GIF_LOG("gsInterrupt: %8.8x\n", cpuRegs.cycle);

	if((gif->chcr & 0x100) == 0){
		//SysPrintf("Eh? why are you still interrupting! chcr %x, qwc %x, done = %x\n", gif->chcr, gif->qwc, done);
		cpuRegs.interrupt &= ~(1 << 2);
		return;
	}
	if(gif->qwc > 0 || gspath3done == 0) {
		if( !(psHu32(DMAC_CTRL) & 0x1) ) {
			SysPrintf("gs dma masked\n");
			return;
		}

		GIFdma();
#ifdef GSPATH3FIX
		if ((vif1Regs->mskpath3 && (vif1ch->chcr & 0x100)) || (psHu32(GIF_MODE) & 0x1)) cpuRegs.interrupt &= ~(1 << 2);
#endif
		return;
	}
	
	gspath3done = 0;
	gscycles = 0;
	Path3transfer = 0;
	gif->chcr &= ~0x100;
	GSCSRr &= ~0xC000; //Clear FIFO stuff
	GSCSRr |= 0x4000;  //FIFO empty
	//psHu32(GIF_MODE)&= ~0x4;
	psHu32(GIF_STAT)&= ~0xE00; // OPH=0 | APATH=0
	psHu32(GIF_STAT)&= ~0x1F000000; // QFC=0
	hwDmacIrq(DMAC_GIF);

	cpuRegs.interrupt &= ~(1 << 2);
}

static u64 s_gstag=0; // used for querying the last tag

static void WRITERING_DMA(u32 *pMem, u32 qwc)
{ 
	psHu32(GIF_STAT) |= 0xE00;         

	// Path3 transfer will be set to zero by the GIFtag handler.
	Path3transfer = 1;

	if( mtgsThread != NULL )
	{ 
		int sizetoread = (qwc)<<4; 
		sizetoread = mtgsThread->PrepDataPacket( GIF_PATH_3, pMem, sizetoread );
		u8* pgsmem = mtgsThread->GetDataPacketPtr();

		/* check if page of endmem is valid (dark cloud2) */
		// fixme: this hack makes no sense, because the giftagDummy will
		// process the full length of bytes regardess of how much we copy.
		// So you'd think if we're truncating the copy to prevent DEPs, we 
		// should truncate the gif packet size too.. (air)

		// fixed? PrepDataPacket now returns the actual size of the packet.
		// VIF handles scratchpad wrapping also, so this code shouldn't be needed anymore.

		/*u32 pendmem = (u32)gif->madr + sizetoread; 
		if( dmaGetAddr(pendmem-16) == NULL )
		{ 
			pendmem = ((pendmem-16)&~0xfff)-16; 
			while(dmaGetAddr(pendmem) == NULL)
			{ 
				pendmem = (pendmem&~0xfff)-16; 
			} 
			memcpy_raz_(pgsmem, pMem, pendmem-(u32)gif->madr+16);
		}
		else*/
		memcpy_raz_(pgsmem, pMem, sizetoread); 
		
		mtgsThread->SendDataPacket();
	} 
	else 
	{ 
		GSGIFTRANSFER3(pMem, qwc);
        if( GSgetLastTag != NULL )
		{ 
            GSgetLastTag(&s_gstag); 
            if( s_gstag == 1 )
                Path3transfer = 0; /* fixes SRS and others */ 
        } 
	} 
} 

int  _GIFchain() {
#ifdef GSPATH3FIX
	u32 qwc = (psHu32(GIF_MODE) & 0x4 && vif1Regs->mskpath3) ? min(8, (int)gif->qwc) : gif->qwc;
#else
	u32 qwc = gif->qwc;
#endif
	u32 *pMem;

	//if (gif->qwc == 0) return 0;

	pMem = (u32*)dmaGetAddr(gif->madr);
	if (pMem == NULL) {
		// reset path3, fixes dark cloud 2

		_gsGIFSoftReset(4);

		//must increment madr and clear qwc, else it loops
		gif->madr+= gif->qwc*16;
		gif->qwc = 0;
		SysPrintf("NULL GIFchain\n");
		return -1;
	}
	WRITERING_DMA(pMem, qwc);
	
	//if((psHu32(GIF_MODE) & 0x4)) amount -= qwc;
	gif->madr+= qwc*16;
	gif->qwc -= qwc;
	return (qwc)*2;
}

#define GIFchain() \
	if (gif->qwc) { \
		gscycles+= _GIFchain(); /* guessing */ \
	}

int gscount = 0;
static int prevcycles = 0;
static u32* prevtag = NULL;

void GIFdma() 
{
	u32 *ptag;
	u32 id;

	gscycles= prevcycles ? prevcycles: gscycles;

	if( (psHu32(GIF_CTRL) & 8) ) { // temporarily stop
		SysPrintf("Gif dma temp paused?\n");
		return;
	}

	GIF_LOG("dmaGIFstart chcr = %lx, madr = %lx, qwc  = %lx\n tadr = %lx, asr0 = %lx, asr1 = %lx\n", gif->chcr, gif->madr, gif->qwc, gif->tadr, gif->asr0, gif->asr1);

#ifndef GSPATH3FIX
	if ( !(psHu32(GIF_MODE) & 0x4) ) {
		if (vif1Regs->mskpath3 || psHu32(GIF_MODE) & 0x1) {
			gif->chcr &= ~0x100;
			psHu32(GIF_STAT)&= ~0xE00; // OPH=0 | APATH=0
			hwDmacIrq(2);
			return;
		}
	}
#endif

	if ((psHu32(DMAC_CTRL) & 0xC0) == 0x80 && prevcycles != 0) { // STD == GIF
		SysPrintf("GS Stall Control Source = %x, Drain = %x\n MADR = %x, STADR = %x", (psHu32(0xe000) >> 4) & 0x3, (psHu32(0xe000) >> 6) & 0x3, gif->madr, psHu32(DMAC_STADR));

		if( gif->madr + (gif->qwc * 16) > psHu32(DMAC_STADR) ) {
			CPU_INT(2, gscycles);
			gscycles = 0;
			return;
		}
		prevcycles = 0;
		gif->qwc = 0;
	}

	GSCSRr &= ~0xC000;  //Clear FIFO stuff
	GSCSRr |= 0x8000;   //FIFO full
	//psHu32(GIF_STAT)|= 0xE00; // OPH=1 | APATH=3
	psHu32(GIF_STAT)|= 0x10000000; // FQC=31, hack ;)

#ifdef GSPATH3FIX
	if (vif1Regs->mskpath3 || psHu32(GIF_MODE) & 0x1) {
		if(gif->qwc == 0) {
			if((gif->chcr & 0x10e) == 0x104) {
				ptag = (u32*)dmaGetAddr(gif->tadr);  //Set memory pointer to TADR

				if (ptag == NULL) {					 //Is ptag empty?
					psHu32(DMAC_STAT)|= 1<<15;		 //If yes, set BEIS (BUSERR) in DMAC_STAT register 
					return;
				}	
				gscycles += 2;
				gif->chcr = ( gif->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );  //Transfer upper part of tag to CHCR bits 31-15
				id        = (ptag[0] >> 28) & 0x7;		//ID for DmaChain copied from bit 28 of the tag
				gif->qwc  = (u16)ptag[0];			    //QWC set to lower 16bits of the tag
				gif->madr = ptag[1];				    //MADR = ADDR field	
				gspath3done = hwDmacSrcChainWithStack(gif, id);
				GIF_LOG("PTH3 MASK gifdmaChain %8.8x_%8.8x size=%d, id=%d, addr=%lx\n", ptag[1], ptag[0], gif->qwc, id, gif->madr);

				if ((gif->chcr & 0x80) && ptag[0] >> 31) {			 //Check TIE bit of CHCR and IRQ bit of tag
					GIF_LOG("PATH3 MSK dmaIrq Set\n");
					SysPrintf("GIF TIE\n");
					gspath3done |= 1;
				}
			}
		}
		// When MTGS is enabled, Gifchain calls WRITERING_DMA, which calls GSRINGBUF_DONECOPY, which freezes 
		// the registers inside of the FreezeXMMRegs calls here and in the other two below..
		// I'm not really sure that is intentional. --arcum42
		FreezeXMMRegs(1); 
		FreezeMMXRegs(1);
		GIFchain(); 
		FreezeXMMRegs(0); // Theres a comment below that says not to unfreeze the xmm regs, so not sure about this.
		FreezeMMXRegs(0);

		if((gspath3done == 1 || (gif->chcr & 0xc) == 0) && gif->qwc == 0){ 
			if(gif->qwc > 0) SysPrintf("Horray\n");
			gspath3done = 0;
			gif->chcr &= ~0x100;
			//psHu32(GIF_MODE)&= ~0x4;
			GSCSRr &= ~0xC000;
			GSCSRr |= 0x4000;  
			Path3transfer = 0;
			psHu32(GIF_STAT)&= ~0x1F000E00; // OPH=0 | APATH=0 | QFC=0
			hwDmacIrq(DMAC_GIF);
		}
		//Dont unfreeze xmm regs here, Masked PATH3 can only be called by VIF, which is already handling it.
		return;
	}
#endif
	//gscycles = 0;
	// Transfer Dn_QWC from Dn_MADR to GIF
	if ((gif->chcr & 0xc) == 0 || gif->qwc > 0) { // Normal Mode
		//gscount++;
		if ((psHu32(DMAC_CTRL) & 0xC0) == 0x80 && (gif->chcr & 0xc) == 0) { 
			SysPrintf("DMA Stall Control on GIF normal\n");
		}
		FreezeXMMRegs(1);  
		FreezeMMXRegs(1);  
		GIFchain();	//Transfers the data set by the switch
		FreezeXMMRegs(0); 
		FreezeMMXRegs(0);	 
		if(gif->qwc == 0 && (gif->chcr & 0xc) == 0) gspath3done = 1;
	}
	else {
		// Chain Mode
		while (gspath3done == 0 && gif->qwc == 0) {		//Loop if the transfers aren't intermittent
			ptag = (u32*)dmaGetAddr(gif->tadr);  //Set memory pointer to TADR
			if (ptag == NULL) {					 //Is ptag empty?
				psHu32(DMAC_STAT)|= 1<<15;		 //If yes, set BEIS (BUSERR) in DMAC_STAT register
				return;
			}
			gscycles+=2; // Add 1 cycles from the QW read for the tag

			// Transfer dma tag if tte is set
			if (gif->chcr & 0x40) {
				//u32 temptag[4] = {0};
				//SysPrintf("GIF TTE: %x_%x\n", ptag[3], ptag[2]);

				//temptag[0] = ptag[2];
				//temptag[1] = ptag[3];
				//GSGIFTRANSFER3(ptag, 1); 
			}

			gif->chcr = ( gif->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );  //Transfer upper part of tag to CHCR bits 31-15
		
			id        = (ptag[0] >> 28) & 0x7;		//ID for DmaChain copied from bit 28 of the tag
			gif->qwc  = (u16)ptag[0];			    //QWC set to lower 16bits of the tag
			gif->madr = ptag[1];				    //MADR = ADDR field
			
			gspath3done = hwDmacSrcChainWithStack(gif, id);
			GIF_LOG("gifdmaChain %8.8x_%8.8x size=%d, id=%d, addr=%lx\n", ptag[1], ptag[0], gif->qwc, id, gif->madr);

			if ((psHu32(DMAC_CTRL) & 0xC0) == 0x80) { // STD == GIF
				// there are still bugs, need to also check if gif->madr +16*qwc >= stadr, if not, stall
				if(!gspath3done && gif->madr + (gif->qwc * 16) > psHu32(DMAC_STADR) && id == 4) {
					// stalled
					SysPrintf("GS Stall Control Source = %x, Drain = %x\n MADR = %x, STADR = %x", (psHu32(0xe000) >> 4) & 0x3, (psHu32(0xe000) >> 6) & 0x3,gif->madr, psHu32(DMAC_STADR));
					prevcycles = gscycles;
					gif->tadr -= 16;
					hwDmacIrq(13);
					CPU_INT(2, gscycles);
					gscycles = 0;
					return;
				}
			}

			FreezeXMMRegs(1);  
			FreezeMMXRegs(1);  
			GIFchain();	//Transfers the data set by the switch
			FreezeXMMRegs(0); 
			FreezeMMXRegs(0); 

			if ((gif->chcr & 0x80) && ptag[0] >> 31) { //Check TIE bit of CHCR and IRQ bit of tag
				GIF_LOG("dmaIrq Set\n");
				gspath3done = 1;
				//gif->qwc = 0;
			}
		}
	}
	prevtag = NULL;
	prevcycles = 0;
	if (!(vif1Regs->mskpath3 || (psHu32(GIF_MODE) & 0x1)))	{
		CPU_INT(2, gscycles);
		gscycles = 0;
	}
}

void dmaGIF() {
	//if(vif1Regs->mskpath3 || (psHu32(GIF_MODE) & 0x1)){
	//	CPU_INT(2, 48); //Wait time for the buffer to fill, fixes some timing problems in path 3 masking
	//}				//It takes the time of 24 QW for the BUS to become ready - The Punisher, And1 Streetball
	//else
	gspath3done = 0; // For some reason this doesnt clear? So when the system starts the thread, we will clear it :)

	if(gif->qwc > 0 && (gif->chcr & 0x4) == 0x4)
        gspath3done = 1; //Halflife sets a QWC amount in chain mode, no tadr set.

	if ((psHu32(DMAC_CTRL) & 0xC) == 0xC ) { // GIF MFIFO
		//SysPrintf("GIF MFIFO\n");
		gifMFIFOInterrupt();
		return;
	}

	GIFdma();
}

#define spr0 ((DMACh*)&PS2MEM_HW[0xD000])

static unsigned int mfifocycles;
static unsigned int gifqwc = 0;
static unsigned int gifdone = 0;

// called from only one location, so forceinline it:
static __forceinline int mfifoGIFrbTransfer() {
	u32 qwc = (psHu32(GIF_MODE) & 0x4 && vif1Regs->mskpath3) ? min(8, (int)gif->qwc) : gif->qwc;
	int mfifoqwc = min(gifqwc, qwc);
	u32 *src;


	/* Check if the transfer should wrap around the ring buffer */
	if ((gif->madr+mfifoqwc*16) > (psHu32(DMAC_RBOR) + psHu32(DMAC_RBSR)+16)) {
		int s1 = ((psHu32(DMAC_RBOR) + psHu32(DMAC_RBSR)+16) - gif->madr) >> 4;

		// fixme - I don't think these should use WRITERING_DMA, since our source
		// isn't the DmaGetAddr(gif->madr) address that WRITERING_DMA expects.

		/* it does, so first copy 's1' bytes from 'addr' to 'data' */
		src = (u32*)PSM(gif->madr);
		if (src == NULL) return -1;
		WRITERING_DMA(src, s1);

		/* and second copy 's2' bytes from 'maddr' to '&data[s1]' */
		src = (u32*)PSM(psHu32(DMAC_RBOR));
		if (src == NULL) return -1;
		WRITERING_DMA(src, (mfifoqwc - s1));
		
	} else {
		/* it doesn't, so just transfer 'qwc*16' words 
		   from 'gif->madr' to GS */
		src = (u32*)PSM(gif->madr);
		if (src == NULL) return -1;
		
		WRITERING_DMA(src, mfifoqwc);
		gif->madr = psHu32(DMAC_RBOR) + (gif->madr & psHu32(DMAC_RBSR));
	}

	gifqwc -= mfifoqwc;
	gif->qwc -= mfifoqwc;
	gif->madr+= mfifoqwc*16;
	mfifocycles+= (mfifoqwc) * 2; /* guessing */
	

	return 0;
}

// called from only one location, so forceinline it:
static __forceinline int mfifoGIFchain() {	 
	/* Is QWC = 0? if so there is nothing to transfer */

	if (gif->qwc == 0) return 0;
	
	if (gif->madr >= psHu32(DMAC_RBOR) &&
		gif->madr <= (psHu32(DMAC_RBOR)+psHu32(DMAC_RBSR))) {
		if (mfifoGIFrbTransfer() == -1) return -1;
	} else {
		int mfifoqwc = (psHu32(GIF_MODE) & 0x4 && vif1Regs->mskpath3) ? min(8, (int)gif->qwc) : gif->qwc;
		u32 *pMem = (u32*)dmaGetAddr(gif->madr);
		if (pMem == NULL) return -1;

		WRITERING_DMA(pMem, mfifoqwc);
		gif->madr+= mfifoqwc*16;
		gif->qwc -= mfifoqwc;
		mfifocycles+= (mfifoqwc) * 2; /* guessing */
	}

	return 0;
}


void mfifoGIFtransfer(int qwc) {
	u32 *ptag;
	int id;
	u32 temp = 0;
	mfifocycles = 0;
	
	if(qwc > 0 ) {
				gifqwc += qwc;
				if(!(gif->chcr & 0x100))return;
			}
	SPR_LOG("mfifoGIFtransfer %x madr %x, tadr %x\n", gif->chcr, gif->madr, gif->tadr);
		
	if(gif->qwc == 0){
			if(gif->tadr == spr0->madr) {
	#ifdef PCSX2_DEVBUILD
				/*if( gifqwc > 1 )
					SysPrintf("gif mfifo tadr==madr but qwc = %d\n", gifqwc);*/
	#endif
				//hwDmacIrq(14);

				return;
			}
			gif->tadr = psHu32(DMAC_RBOR) + (gif->tadr & psHu32(DMAC_RBSR));
			ptag = (u32*)dmaGetAddr(gif->tadr);
			
			id        = (ptag[0] >> 28) & 0x7;
			gif->qwc  = (ptag[0] & 0xffff);
			gif->madr = ptag[1];
			mfifocycles += 2;
			
			gif->chcr = ( gif->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );
			SPR_LOG("dmaChain %8.8x_%8.8x size=%d, id=%d, madr=%lx, tadr=%lx mfifo qwc = %x spr0 madr = %x\n",
					ptag[1], ptag[0], gif->qwc, id, gif->madr, gif->tadr, gifqwc, spr0->madr);

			gifqwc--;
			switch (id) {
				case 0: // Refe - Transfer Packet According to ADDR field
					gif->tadr = psHu32(DMAC_RBOR) + ((gif->tadr + 16) & psHu32(DMAC_RBSR));
					gifdone = 2;										//End Transfer
					break;

				case 1: // CNT - Transfer QWC following the tag.
					gif->madr = psHu32(DMAC_RBOR) + ((gif->tadr + 16) & psHu32(DMAC_RBSR));						//Set MADR to QW after Tag            
					gif->tadr = psHu32(DMAC_RBOR) + ((gif->madr + (gif->qwc << 4)) & psHu32(DMAC_RBSR));			//Set TADR to QW following the data
					gifdone = 0;
					break;

				case 2: // Next - Transfer QWC following tag. TADR = ADDR
					temp = gif->madr;								//Temporarily Store ADDR
					gif->madr = psHu32(DMAC_RBOR) + ((gif->tadr + 16) & psHu32(DMAC_RBSR)); 					  //Set MADR to QW following the tag
					gif->tadr = temp;								//Copy temporarily stored ADDR to Tag
					gifdone = 0;
					break;

				case 3: // Ref - Transfer QWC from ADDR field
				case 4: // Refs - Transfer QWC from ADDR field (Stall Control) 
					gif->tadr = psHu32(DMAC_RBOR) + ((gif->tadr + 16) & psHu32(DMAC_RBSR));							//Set TADR to next tag
					gifdone = 0;
					break;

				case 7: // End - Transfer QWC following the tag
					gif->madr = psHu32(DMAC_RBOR) + ((gif->tadr + 16) & psHu32(DMAC_RBSR));		//Set MADR to data following the tag
					gif->tadr = psHu32(DMAC_RBOR) + ((gif->madr + (gif->qwc << 4)) & psHu32(DMAC_RBSR));			//Set TADR to QW following the data
					gifdone = 2;										//End Transfer
					break;
				}
				if ((gif->chcr & 0x80) && (ptag[0] >> 31)) {
				SPR_LOG("dmaIrq Set\n");
				gifdone = 2;
			}
	 }
	FreezeXMMRegs(1); 
	FreezeMMXRegs(1);
		if (mfifoGIFchain() == -1) {
			SysPrintf("GIF dmaChain error size=%d, madr=%lx, tadr=%lx\n",
					gif->qwc, gif->madr, gif->tadr);
			gifdone = 1;
		}
	FreezeXMMRegs(0); 
	FreezeMMXRegs(0);
		
	if(gif->qwc == 0 && gifdone == 2) gifdone = 1;
	CPU_INT(11,mfifocycles);
		
	SPR_LOG("mfifoGIFtransfer end %x madr %x, tadr %x\n", gif->chcr, gif->madr, gif->tadr);	
}

void gifMFIFOInterrupt()
{
	if(!(gif->chcr & 0x100)) { SysPrintf("WTF GIFMFIFO\n");cpuRegs.interrupt &= ~(1 << 11); return ; }
	
	if(gifdone != 1) {
		if(gifqwc <= 0) {
		//SysPrintf("Empty\n");
			psHu32(GIF_STAT)&= ~0xE00; // OPH=0 | APATH=0
			hwDmacIrq(14);
			cpuRegs.interrupt &= ~(1 << 11); 
			return;
		}
		mfifoGIFtransfer(0);
		return;
	}
#ifdef PCSX2_DEVBUILD
	if(gifdone == 0 || gif->qwc > 0) {
		SysPrintf("gifMFIFO Panic > Shouldnt go here!\n");
		cpuRegs.interrupt &= ~(1 << 11);
		return;
	}
#endif
	//if(gifqwc > 0)SysPrintf("GIF MFIFO ending with stuff in it %x\n", gifqwc);
	gifqwc = 0;
	gifdone = 0;
	gif->chcr &= ~0x100;
	hwDmacIrq(DMAC_GIF);
	GSCSRr &= ~0xC000; //Clear FIFO stuff
	GSCSRr |= 0x4000;  //FIFO empty
	//psHu32(GIF_MODE)&= ~0x4;
	psHu32(GIF_STAT)&= ~0xE00; // OPH=0 | APATH=0
	psHu32(GIF_STAT)&= ~0x1F000000; // QFC=0
	cpuRegs.interrupt &= ~(1 << 11);
}

void gsSyncLimiterLostTime( s32 deltaTime )
{
	// This sync issue applies only to configs that are trying to maintain
	// a perfect "specific" framerate (where both min and max fps are the same)
	// any other config will eventually equalize out.

	if( !m_StrictSkipping ) return;

	//SysPrintf("LostTime on the EE!\n");

	if( mtgsThread != NULL )
	{
		mtgsThread->SendSimplePacket(
			GS_RINGTYPE_STARTTIME,
			deltaTime,
			0,
			0
		);
	}
	else
	{
		m_iSlowStart += deltaTime;
		//m_justSkipped = false;
	}
}

// FrameSkipper - Measures delta time between calls and issues frameskips
// it the time is too long.  Also regulates the status of the EE's framelimiter.

// This function does not regulate frame limiting, meaning it does no stalling.
// Stalling functions are performed by the EE: If the MTGS were throtted and not
// the EE, the EE would fill the ringbuffer while the MTGS regulated frames -- 
// fine for most situations but could result in literally dozens of frames queued
// up in the ringbuffer durimg some game menu screens; which in turn would result
// in a half-second lag of keystroke actions becoming visible to the user (bad!).

// Alternative: Instead of this, I could have everything regulated here, and then
// put a framecount limit on the MTGS ringbuffer.  But that seems no less complicated
// and would also mean that aforementioned menus would still be laggy by whatever
// frame count threshold.  This method is more responsive.

__forceinline void gsFrameSkip( bool forceskip )
{
	static u8 FramesToRender = 0;
	static u8 FramesToSkip = 0;

	if( CHECK_FRAMELIMIT != PCSX2_FRAMELIMIT_SKIP && 
		CHECK_FRAMELIMIT != PCSX2_FRAMELIMIT_VUSKIP ) return;

	// FrameSkip and VU-Skip Magic!
	// Skips a sequence of consecutive frames after a sequence of rendered frames

	// This is the least number of consecutive frames we will render w/o skipping
	const int noSkipFrames = ((Config.CustomConsecutiveFrames>0) ? Config.CustomConsecutiveFrames : 1);
	// This is the number of consecutive frames we will skip				
	const int yesSkipFrames = ((Config.CustomConsecutiveSkip>0) ? Config.CustomConsecutiveSkip : 1);

	const u64 iEnd = GetCPUTicks();
	const s64 uSlowExpectedEnd = m_iSlowStart + m_iSlowTicks;
	const s64 sSlowDeltaTime = iEnd - uSlowExpectedEnd;

	m_iSlowStart = uSlowExpectedEnd;

	if( forceskip )
	{
		if( !FramesToSkip )
		{
			//SysPrintf( "- Skipping some VUs!\n" );

			GSsetFrameSkip( 1 );
			FramesToRender = noSkipFrames;
			FramesToSkip = 1;	// just set to 1

			// We're already skipping, so FramesToSkip==1 will just restore the gsFrameSkip
			// setting and reset our delta times as needed.
		}
		return;
	}
	
	// if we've already given the EE a skipcount assignment then don't do anything more.
	// Otherwise we could start compounding the issue and skips would be too long.
	if( g_vu1SkipCount > 0 )
	{
		//SysPrintf("- Already Assigned a Skipcount.. %d\n", g_vu1SkipCount );
		return;
	}

	if( FramesToRender == 0 )
	{
		// -- Standard operation section --
		// Means neither skipping frames nor force-rendering consecutive frames.

		if( sSlowDeltaTime > 0 ) 
		{
			// The game is running below the minimum framerate.
			// But don't start skipping yet!  That would be too sensitive.
			// So the skipping code is only engaged if the SlowDeltaTime falls behind by
			// a full frame, or if we're already skipping (in which case we don't care
			// to avoid errant skips).
			
			// Note: The MTGS can go out of phase from the EE, which means that the
			// variance for a "nominal" framerate can range from 0 to m_iSlowTicks.
			// We also check for that here.

			if( (m_justSkipped && (sSlowDeltaTime > m_iSlowTicks)) || 
				(sSlowDeltaTime > m_iSlowTicks*2) )
			{
				//SysPrintf( "Frameskip Initiated! Lateness: %d\n", (int)( (sSlowDeltaTime*100) / m_iSlowTicks ) );
				
				if( CHECK_FRAMELIMIT == PCSX2_FRAMELIMIT_VUSKIP )
				{
					// For best results we have to wait for the EE to
					// tell us when to skip, so that VU skips are synched with GS skips.
					AtomicExchangeAdd( g_vu1SkipCount, yesSkipFrames+1 );
				}
				else
				{
					GSsetFrameSkip(1);
					FramesToRender = noSkipFrames+1;
					FramesToSkip = yesSkipFrames;
				}
			}
		}
		else
		{
			// Running at or above full speed, so reset the StartTime since the Limiter
			// will muck things up.  (special case: if skip and limit fps are equal then
			// we don't reset starttime since it would cause desyncing.  We let the EE
			// regulate it via calls to gsSyncLimiterStartTime).

			if( !m_StrictSkipping )
				m_iSlowStart = iEnd;
		}
		m_justSkipped = false;
		return;
	}
	else if( FramesToSkip > 0 )
	{
		// -- Frames-a-Skippin' Section --

		FramesToSkip--;

		if( FramesToSkip == 0 )
		{
			// Skipped our last frame, so restore non-skip behavior

			GSsetFrameSkip(0);

			// Note: If we lag behind by 250ms then it's time to give up on the idea
			// of catching up.  Force the game to slow down by resetting iStart to
			// something closer to iEnd.

			if( sSlowDeltaTime > (m_iSlowTicks + ((s64)GetTickFrequency() / 4)) )
			{
				//SysPrintf( "Frameskip couldn't skip enough -- had to lose some time!\n" );
				m_iSlowStart = iEnd - m_iSlowTicks;
			}

			m_justSkipped = true;
		}
		else
			return;
	}

	//SysPrintf( "Consecutive Frames -- Lateness: %d\n", (int)( sSlowDeltaTime / m_iSlowTicks ) );

	// -- Consecutive frames section --
	// Force-render consecutive frames without skipping.

	FramesToRender--;

	if( sSlowDeltaTime < 0 )
	{
		m_iSlowStart = iEnd;
	}
}

// updategs - if FALSE the gs will skip the frame.
void gsPostVsyncEnd( bool updategs )
{
	*(u32*)(PS2MEM_GS+0x1000) ^= 0x2000; // swap the vsync field

	if( mtgsThread != NULL ) 
	{
		mtgsThread->SendSimplePacket( GS_RINGTYPE_VSYNC,
			(*(u32*)(PS2MEM_GS+0x1000)&0x2000), updategs, 0);

		// No need to freeze MMX/XMM registers here since this
		// code is always called from the context of a BranchTest.
		mtgsThread->SetEvent();
	}
	else
	{
		GSvsync((*(u32*)(PS2MEM_GS+0x1000)&0x2000));

		// update here on single thread mode *OBSOLETE*
		if( PAD1update != NULL ) PAD1update(0);
		if( PAD2update != NULL ) PAD2update(1);

		gsFrameSkip( !updategs );
	}
}

void _gs_ResetFrameskip()
{
	g_vu1SkipCount = 0;		// set to 0 so that EE will re-enable the VU at the next vblank.
	GSsetFrameSkip( 0 );
}

// Disables the GS Frameskip at runtime without any racy mess...
void gsResetFrameSkip()
{
	if( mtgsThread != NULL )
		mtgsThread->SendSimplePacket(GS_RINGTYPE_FRAMESKIP, 0, 0, 0);
	else
		_gs_ResetFrameskip();
}

void gsDynamicSkipEnable()
{
	if( !m_StrictSkipping ) return;

	mtgsWaitGS();
	m_iSlowStart = GetCPUTicks();	
	frameLimitReset();
}

void SaveState::gsFreeze()
{
	FreezeMem(PS2MEM_GS, 0x2000);
	Freeze(CSRw);
	mtgsFreeze();
}

#ifdef PCSX2_DEVBUILD

struct GSStatePacket
{
	u32 type;
	vector<u8> mem;
};

// runs the GS
void RunGSState( gzLoadingState& f )
{
	u32 newfield;
	list< GSStatePacket > packets;

	while( !f.Finished() )
	{
		int type, size;
		f.Freeze( type );

		if( type != GSRUN_VSYNC ) f.Freeze( size );

		packets.push_back(GSStatePacket());
		GSStatePacket& p = packets.back();

		p.type = type;

		if( type != GSRUN_VSYNC ) {
			p.mem.resize(size*16);
			f.FreezeMem( &p.mem[0], size*16 );
		}
	}

	list<GSStatePacket>::iterator it = packets.begin();
	g_SaveGSStream = 3;

	int skipfirst = 1;

	// first extract the data
	while(1) {

		switch(it->type) {
			case GSRUN_TRANS1:
				GSgifTransfer1((u32*)&it->mem[0], 0);
				break;
			case GSRUN_TRANS2:
				GSgifTransfer2((u32*)&it->mem[0], it->mem.size()/16);
				break;
			case GSRUN_TRANS3:
				GSgifTransfer3((u32*)&it->mem[0], it->mem.size()/16);
				break;
			case GSRUN_VSYNC:
				// flip
				newfield = (*(u32*)(PS2MEM_GS+0x1000)&0x2000) ? 0 : 0x2000;
				*(u32*)(PS2MEM_GS+0x1000) = (*(u32*)(PS2MEM_GS+0x1000) & ~(1<<13)) | newfield;

				GSvsync(newfield);
				SysUpdate();

				if( g_SaveGSStream != 3 )
					return;
				break;

			jNO_DEFAULT
		}

		++it;
		if( it == packets.end() )
			it = packets.begin();
	}
}

#endif

#undef GIFchain
