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

#include <list>

#include "Gif_Unit.h"
#include "Counters.h"
#include "Config.h"

using namespace Threading;
using namespace R5900;

alignas(16) u8 g_RealGSMem[Ps2MemSize::GSregs];
static bool s_GSRegistersWritten = false;

void gsSetVideoMode(GS_VideoMode mode)
{
	gsVideoMode = mode;
	UpdateVSyncRate();
	CSRreg.FIELD = 1;
}

// Make sure framelimiter options are in sync with GS capabilities.
void gsReset()
{
	GetMTGS().ResetGS(true);

	UpdateVSyncRate();
	memzero(g_RealGSMem);

	CSRreg.Reset();
	GSIMR.reset();
}

void gsUpdateFrequency(Pcsx2Config& config)
{
	if (config.GS.FrameLimitEnable)
	{
		switch (config.LimiterMode)
		{
		case LimiterModeType::Nominal:
			config.GS.LimitScalar = config.Framerate.NominalScalar;
			break;
		case LimiterModeType::Slomo:
			config.GS.LimitScalar = config.Framerate.SlomoScalar;
			break;
		case LimiterModeType::Turbo:
			config.GS.LimitScalar = config.Framerate.TurboScalar;
			break;
		case LimiterModeType::Unlimited:
			config.GS.LimitScalar = 0.0;
			break;
		default:
			pxAssert("Unknown framelimiter mode!");
		}
	}
	else
	{
		config.GS.LimitScalar = 0.0;
	}

	UpdateVSyncRate();
}

static __fi void gsCSRwrite( const tGS_CSR& csr )
{
	if (csr.RESET) {
		GUNIT_WARN("GUNIT_WARN: csr.RESET");
		//Console.Warning( "csr.RESET" );
		//gifUnit.Reset(true); // Don't think gif should be reset...
		gifUnit.gsSIGNAL.queued = false;
		GetMTGS().SendSimplePacket(GS_RINGTYPE_RESET, 0, 0, 0);
		const u32 field = CSRreg.FIELD;
		CSRreg.Reset();
		GSIMR.reset();
		CSRreg.FIELD = field;
	}

	if(csr.FLUSH)
	{
		// Our emulated GS has no FIFO, but if it did, it would flush it here...
		//Console.WriteLn("GS_CSR FLUSH GS fifo: %x (CSRr=%x)", value, GSCSRr);
	}
	
	if(csr.SIGNAL)
	{
		// SIGNAL : What's not known here is whether or not the SIGID register should be updated
		//  here or when the IMR is cleared (below).
		GUNIT_LOG("csr.SIGNAL");
		if (gifUnit.gsSIGNAL.queued) {
			//DevCon.Warning("Firing pending signal");
			GSSIGLBLID.SIGID = (GSSIGLBLID.SIGID & ~gifUnit.gsSIGNAL.data[1])
				        | (gifUnit.gsSIGNAL.data[0]&gifUnit.gsSIGNAL.data[1]);

			if (!GSIMR.SIGMSK) gsIrq();
			CSRreg.SIGNAL  = true; // Just to be sure :p
		}
		else CSRreg.SIGNAL = false;
		gifUnit.gsSIGNAL.queued = false;
		gifUnit.Execute(false, true); // Resume paused transfers
	}
	
	if (csr.FINISH)	{
		CSRreg.FINISH = false; 
		gifUnit.gsFINISH.gsFINISHFired = false; //Clear the previously fired FINISH (YS, Indiecar 2005, MGS3)
	}
	if(csr.HSINT)	CSRreg.HSINT	= false;
	if(csr.VSINT)	CSRreg.VSINT	= false;
	if(csr.EDWINT)	CSRreg.EDWINT	= false;
}

static __fi void IMRwrite(u32 value)
{
	GUNIT_LOG("IMRwrite()");

	if (CSRreg.GetInterruptMask() & (~value & GSIMR._u32) >> 8)
		gsIrq();

	GSIMR._u32 = (value & 0x1f00)|0x6000;
}

__fi void gsWrite8(u32 mem, u8 value)
{
	switch (mem)
	{
		// CSR 8-bit write handlers.
		// I'm quite sure these would just write the CSR portion with the other
		// bits set to 0 (no action).  The previous implementation masked the 8-bit 
		// write value against the previous CSR write value, but that really doesn't
		// make any sense, given that the real hardware's CSR circuit probably has no
		// real "memory" where it saves anything.  (for example, you can't write to
		// and change the GS revision or ID portions -- they're all hard wired.) --air

		case GS_CSR: // GS_CSR
			gsCSRwrite( tGS_CSR((u32)value) );			break;
		case GS_CSR + 1: // GS_CSR
			gsCSRwrite( tGS_CSR(((u32)value) <<  8) );	break;
		case GS_CSR + 2: // GS_CSR
			gsCSRwrite( tGS_CSR(((u32)value) << 16) );	break;
		case GS_CSR + 3: // GS_CSR
			gsCSRwrite( tGS_CSR(((u32)value) << 24) );	break;

		default:
			*PS2GS_BASE(mem) = value;
		break;
	}
	GIF_LOG("GS write 8 at %8.8lx with data %8.8lx", mem, value);
}

//////////////////////////////////////////////////////////////////////////
// GS Write 16 bit

__fi void gsWrite16(u32 mem, u16 value)
{
	GIF_LOG("GS write 16 at %8.8lx with data %8.8lx", mem, value);

	switch (mem)
	{
		// See note above about CSR 8 bit writes, and handling them as zero'd bits
		// for all but the written parts.
		
		case GS_CSR:
			gsCSRwrite( tGS_CSR((u32)value) );
		return; // do not write to MTGS memory

		case GS_CSR+2:
			gsCSRwrite( tGS_CSR(((u32)value) << 16) );
		return; // do not write to MTGS memory

		case GS_IMR:
			IMRwrite(value);
		return; // do not write to MTGS memory
	}

	*(u16*)PS2GS_BASE(mem) = value;
}

//////////////////////////////////////////////////////////////////////////
// GS Write 32 bit

__fi void gsWrite32(u32 mem, u32 value)
{
	pxAssume( (mem & 3) == 0 );
	GIF_LOG("GS write 32 at %8.8lx with data %8.8lx", mem, value);

	switch (mem)
	{
		case GS_CSR:
			gsCSRwrite(tGS_CSR(value));
		return;

		case GS_IMR:
			IMRwrite(value);
		return;
	}

	*(u32*)PS2GS_BASE(mem) = value;
}

//////////////////////////////////////////////////////////////////////////
// GS Write 64 bit

void gsWrite64_generic( u32 mem, const mem64_t* value )
{
	const u32* const srcval32 = (u32*)value;
	GIF_LOG("GS Write64 at %8.8lx with data %8.8x_%8.8x", mem, srcval32[1], srcval32[0]);

	*(u64*)PS2GS_BASE(mem) = *value;
}

void gsWrite64_page_00( u32 mem, const mem64_t* value )
{
	s_GSRegistersWritten |= (mem == GS_DISPFB1 || mem == GS_DISPFB2 || mem == GS_PMODE);

	gsWrite64_generic( mem, value );
}

void gsWrite64_page_01( u32 mem, const mem64_t* value )
{
	GIF_LOG("GS Write64 at %8.8lx with data %8.8x_%8.8x", mem, (u32*)value[1], (u32*)value[0]);

	switch( mem )
	{
		case GS_BUSDIR:

			gifUnit.stat.DIR = value[0] & 1;
			if (gifUnit.stat.DIR) {      // Assume will do local->host transfer
				gifUnit.stat.OPH = true; // Should we set OPH here?
				gifUnit.FlushToMTGS();   // Send any pending GS Primitives to the GS
				GUNIT_LOG("Busdir - GS->EE Download");
			}
			else {
				GUNIT_LOG("Busdir - EE->GS Upload");
			}

			gsWrite64_generic( mem, value );
		return;

		case GS_CSR:
			gsCSRwrite(tGS_CSR(*value));
		return;

		case GS_IMR:
			IMRwrite((u32)value[0]);
		return;
	}

	gsWrite64_generic( mem, value );
}

//////////////////////////////////////////////////////////////////////////
// GS Write 128 bit

void gsWrite128_page_00( u32 mem, const mem128_t* value )
{
	gsWrite128_generic( mem, value );
}

void gsWrite128_page_01( u32 mem, const mem128_t* value )
{
	switch( mem )
	{
		case GS_CSR:
			gsCSRwrite((u32)value[0]);
		return;

		case GS_IMR:
			IMRwrite((u32)value[0]);
		return;
	}

	gsWrite128_generic( mem, value );
}

void gsWrite128_generic( u32 mem, const mem128_t* value )
{
	const u32* const srcval32 = (u32*)value;

	GIF_LOG("GS Write128 at %8.8lx with data %8.8x_%8.8x_%8.8x_%8.8x", mem,
		srcval32[3], srcval32[2], srcval32[1], srcval32[0]);

	CopyQWC(PS2GS_BASE(mem), value);
}

__fi u8 gsRead8(u32 mem)
{
	GIF_LOG("GS read 8 from %8.8lx  value: %8.8lx", mem, *(u8*)PS2GS_BASE(mem));

	switch (mem & ~0xF)
	{
		case GS_SIGLBLID:
			return *(u8*)PS2GS_BASE(mem);
		default: // Only SIGLBLID and CSR are readable, everything else mirrors CSR
			return *(u8*)PS2GS_BASE(GS_CSR + (mem & 0xF));
	}
}

__fi u16 gsRead16(u32 mem)
{
	GIF_LOG("GS read 16 from %8.8lx  value: %8.8lx", mem, *(u16*)PS2GS_BASE(mem));
	switch (mem & ~0xF)
	{
		case GS_SIGLBLID:
			return *(u16*)PS2GS_BASE(mem);
		default: // Only SIGLBLID and CSR are readable, everything else mirrors CSR
			return *(u16*)PS2GS_BASE(GS_CSR + (mem & 0x7));
	}
}

__fi u32 gsRead32(u32 mem)
{
	GIF_LOG("GS read 32 from %8.8lx  value: %8.8lx", mem, *(u32*)PS2GS_BASE(mem));

	switch (mem & ~0xF)
	{
		case GS_SIGLBLID:
			return *(u32*)PS2GS_BASE(mem);
		default: // Only SIGLBLID and CSR are readable, everything else mirrors CSR
			return *(u32*)PS2GS_BASE(GS_CSR + (mem & 0xC));
	}
}

__fi u64 gsRead64(u32 mem)
{
	// fixme - PS2GS_BASE(mem+4) = (g_RealGSMem+(mem + 4 & 0x13ff))
	GIF_LOG("GS read 64 from %8.8lx  value: %8.8lx_%8.8lx", mem, *(u32*)PS2GS_BASE(mem+4), *(u32*)PS2GS_BASE(mem) );

	switch (mem & ~0xF)
	{
		case GS_SIGLBLID:
			return *(u64*)PS2GS_BASE(mem);
		default: // Only SIGLBLID and CSR are readable, everything else mirrors CSR
			return *(u64*)PS2GS_BASE(GS_CSR + (mem & 0x8));
	}
}

__fi u128 gsNonMirroredRead(u32 mem)
{
	return *(u128*)PS2GS_BASE(mem);
}

void gsIrq() {
	hwIntcIrq(INTC_GS);
}

// --------------------------------------------------------------------------------------
//  gsFrameSkip
// --------------------------------------------------------------------------------------
// This function regulates the frameskipping status of the GS.  Our new frameskipper for
// 0.9.7 is a very simple logic pattern compared to the old mess.  The goal now is to provide
// the most compatible and efficient frameskip, instead of doing the adaptive logic of
// 0.9.6.  This is almost a necessity because of how many games treat the GS: they upload
// great amounts of data while rendering 2 frames at a time (using double buffering), and
// then use a simple pageswap to display the contents of the second frame for that vsync.
//  (this approach is mostly seen on interlace games; progressive games less so)
// The result is that any skip pattern besides a fully consistent 2on,2off would reuslt in
// tons of missing geometry, rendering frameskip useless.
//
// So instead we use a simple "always skipping" or "never skipping" logic.
//
// EE vs MTGS:
//   This function does not regulate frame limiting, meaning it does no stalling. Stalling
//   functions are performed by the EE, which itself uses thread sleep logic to avoid spin
//   waiting as much as possible (maximizes CPU resource availability for the GS).
static bool s_isSkippingCurrentFrame = false;

__fi void gsFrameSkip()
{
	static int consec_skipped = 0;
	static int consec_drawn = 0;

	if( !EmuConfig.GS.FrameSkipEnable )
	{
		if( s_isSkippingCurrentFrame )
		{
			// Frameskipping disabled on-the-fly .. make sure the GS is restored to non-skip
			// behavior.
			GSsetFrameSkip( false );
			s_isSkippingCurrentFrame = false;
		}
		return;
	}

	GSsetFrameSkip( s_isSkippingCurrentFrame );

	if( s_isSkippingCurrentFrame )
	{
		++consec_skipped;
		if( consec_skipped >= EmuConfig.GS.FramesToSkip )
		{
			consec_skipped = 0;
			s_isSkippingCurrentFrame = false;
		}
	}
	else
	{
		++consec_drawn;
		if( consec_drawn >= EmuConfig.GS.FramesToDraw )
		{
			consec_drawn = 0;
			s_isSkippingCurrentFrame = true;
		}
	}
}

extern bool gsIsSkippingCurrentFrame()
{
	return s_isSkippingCurrentFrame;
}

//These are done at VSync Start.  Drawing is done when VSync is off, then output the screen when Vsync is on
//The GS needs to be told at the start of a vsync else it loses half of its picture (could be responsible for some halfscreen issues)
//We got away with it before i think due to our awful GS timing, but now we have it right (ish)
void gsPostVsyncStart()
{
	//gifUnit.FlushToMTGS();  // Needed for some (broken?) homebrew game loaders
	
	const bool registers_written = s_GSRegistersWritten;
	s_GSRegistersWritten = false;
	GetMTGS().PostVsyncStart(registers_written);
}

void _gs_ResetFrameskip()
{
	GSsetFrameSkip( 0 );
}

// Disables the GS Frameskip at runtime without any racy mess...
void gsResetFrameSkip()
{
	GetMTGS().SendSimplePacket(GS_RINGTYPE_FRAMESKIP, 0, 0, 0);
}

void SaveStateBase::gsFreeze()
{
	FreezeMem(PS2MEM_GS, 0x2000);
	Freeze(gsVideoMode);
}

