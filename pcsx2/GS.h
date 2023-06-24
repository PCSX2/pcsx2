/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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
#include "Gif.h"
#include "GS/GS.h"
#include "SingleRegisterTypes.h"

extern double GetVerticalFrequency();
alignas(16) extern u8 g_RealGSMem[Ps2MemSize::GSregs];

enum CSR_FifoState
{
	CSR_FIFO_NORMAL = 0, // Somwhere in between (Neither empty or almost full).
	CSR_FIFO_EMPTY, // Empty
	CSR_FIFO_FULL, // Almost Full
	CSR_FIFO_RESERVED // Reserved / Unused.
};

// --------------------------------------------------------------------------------------
//  tGS_CSR
// --------------------------------------------------------------------------------------
// This is the Control Register for the GS.  It is a dual-instance register that returns
// distinctly different values for most fields when read and written.  In PCSX2 we house
// the written version in the gsRegs buffer, and generate the readback version on-demand
// from various other PCSX2 system statuses.
union tGS_CSR
{
	struct
	{
		// Write:
		//   0 - No action;
		//   1 - Old event is cleared and event is enabled.
		// Read:
		//   0 - No SIGNAL pending.
		//   1 - SIGNAL has been generated.
		u64 SIGNAL : 1;

		// Write:
		//   0 - No action;
		//   1 - FINISH event is enabled.
		// Read:
		//   0 - No FINISH event pending.
		//   1 - FINISH event has been generated.
		u64 FINISH : 1;

		// Hsync Interrupt Control
		// Write:
		//   0 - No action;
		//   1 - Hsync interrupt is enabled.
		// Read:
		//   0 - No Hsync interrupt pending.
		//   1 - Hsync interrupt has been generated.
		u64 HSINT : 1;

		// Vsync Interrupt Control
		// Write:
		//   0 - No action;
		//   1 - Vsync interrupt is enabled.
		// Read:
		//   0 - No Vsync interrupt pending.
		//   1 - Vsync interrupt has been generated.
		u64 VSINT : 1;

		// Rect Area Write Termination Control
		//   0 - No action;
		//   1 - Rect area write interrupt is enabled.
		// Read:
		//   0 - No RAWrite interrupt pending.
		//   1 - RAWrite interrupt has been generated.
		u64 EDWINT : 1;

		u64 _zero1 : 1;
		u64 _zero2 : 1;
		u64 pad1 : 1;

		// FLUSH  (write-only!)
		// Write:
		//   0 - Resume drawing if suspended (?)
		//   1 - Flush the GS FIFO and suspend drawing
		// Read: Always returns 0. (?)
		u64 FLUSH : 1;

		// RESET (write-only!)
		// Write:
		//   0 - Do nothing.
		//   1 - GS soft system reset.  Clears FIFOs and resets IMR to all 1's.
		//       (PCSX2 implementation also clears GIFpaths, though that behavior may differ on real HW).
		// Read: Always returns 0. (?)
		u64 RESET : 1;

		u64 _pad2 : 2;

		// (I have no idea what this reg is-- air)
		// Output value is updated by sampling the VSync. (?)
		u64 NFIELD : 1;

		// Current Field of Display [page flipping] (read-only?)
		//  0 - EVEN
		//  1 - ODD
		u64 FIELD : 1;

		// GS FIFO Status (read-only)
		//  00 - Somewhere in between
		//  01 - Empty
		//  10 - Almost Full
		//  11 - Reserved (unused)
		// Assign values using the CSR_FifoState enum.
		u64 FIFO : 2;

		// Revision number of the GS (fairly arbitrary)
		u64 REV : 8;

		// ID of the GS (also fairly arbitrary)
		u64 ID : 8;
	};

	u64 _u64;

	struct
	{
		u32 _u32; // lower 32 bits (all useful content!)
		u32 _unused32; // upper 32 bits (unused -- should probably be 0)
	};

	void SwapField()
	{
		_u32 ^= 0x2000;
	}

	void SetField()
	{
		_u32 |= 0x2000;
	}

	void Reset()
	{
		FIFO = CSR_FIFO_EMPTY;
		REV = 0x1B; // GS Revision
		ID = 0x55; // GS ID
	}

	bool HasAnyInterrupts() const { return (SIGNAL || FINISH || HSINT || VSINT || EDWINT); }

	u32 GetInterruptMask() const
	{
		return _u32 & 0x1f;
	}

	void SetAllInterrupts(bool value = true)
	{
		SIGNAL = FINISH = HSINT = VSINT = EDWINT = value;
	}

	tGS_CSR(u64 val) { _u64 = val; }
	tGS_CSR(u32 val) { _u32 = val; }
	tGS_CSR() { Reset(); }
};

// --------------------------------------------------------------------------------------
//  tGS_IMR
// --------------------------------------------------------------------------------------
union tGS_IMR
{
	struct
	{
		u32 _reserved1 : 8;
		u32 SIGMSK : 1; // Signal evevnt interrupt mask
		u32 FINISHMSK : 1; // Finish event interrupt mask
		u32 HSMSK : 1; // HSync interrupt mask
		u32 VSMSK : 1; // VSync interrupt mask
		u32 EDWMSK : 1; // Rectangle write termination interrupt mask
		u32 _undefined : 2; // undefined bits should be set to 1.
		u32 _reserved2 : 17;
	};
	u32 _u32;

	void reset()
	{
		_u32 = 0;
		SIGMSK = FINISHMSK = HSMSK = VSMSK = EDWMSK = true;
		_undefined = 0x3;
	}
	void set(u32 value)
	{
		_u32 = (value & 0x1f00); // Set only the interrupt mask fields.
		_undefined = 0x3; // These should always be set.
	}

	bool masked() const { return (SIGMSK || FINISHMSK || HSMSK || VSMSK || EDWMSK); }
};

// --------------------------------------------------------------------------------------
//  GSRegSMODE1
// --------------------------------------------------------------------------------------
// Previously, the union was used to get the CMOD bit of the SMODE1 register
// Commenting it out as it's unused right now. (Might potentially be useful in the future)
//union GSRegSMODE1
//{
//	struct
//	{
//		u32 RC : 3;
//		u32 LC : 7;
//		u32 T1248 : 2;
//		u32 SLCK : 1;
//		u32 CMOD : 2;
//		u32 EX : 1;
//		u32 PRST : 1;
//		u32 SINT : 1;
//		u32 XPCK : 1;
//		u32 PCK2 : 2;
//		u32 SPML : 4;
//		u32 GCONT : 1;
//		u32 PHS : 1;
//		u32 PVS : 1;
//		u32 PEHS : 1;
//		u32 PEVS : 1;
//		u32 CLKSEL : 2;
//		u32 NVCK : 1;
//		u32 SLCK2 : 1;
//		u32 VCKSEL : 2;
//		u32 VHP : 1;
//		u32 _PAD1 : 27;
//	};
//
//	u64 SMODE1;
//};

// --------------------------------------------------------------------------------------
//  GSRegSIGBLID
// --------------------------------------------------------------------------------------
struct GSRegSIGBLID
{
	u32 SIGID;
	u32 LBLID;
};

#define PS2MEM_GS g_RealGSMem
#define PS2GS_BASE(mem) (PS2MEM_GS + (mem & 0x13ff))

#define CSRreg ((tGS_CSR&)*(PS2MEM_GS + 0x1000))

#define GSCSRr ((u32&)*(PS2MEM_GS + 0x1000))
#define GSIMR ((tGS_IMR&)*(PS2MEM_GS + 0x1010))
#define GSSIGLBLID ((GSRegSIGBLID&)*(PS2MEM_GS + 0x1080))

enum class GS_VideoMode : int
{
	Uninitialized,
	Unknown,
	NTSC,
	PAL,
	VESA,
	SDTV_480P,
	SDTV_576P,
	HDTV_720P,
	HDTV_1080I,
	HDTV_1080P,
	DVD_NTSC,
	DVD_PAL
};

extern GS_VideoMode gsVideoMode;
extern bool gsIsInterlaced;

/////////////////////////////////////////////////////////////////////////////
// Generalized GS Functions and Stuff

extern void gsReset();
extern void gsSetVideoMode(GS_VideoMode mode);
extern void gsPostVsyncStart();
extern void gsUpdateFrequency(Pcsx2Config& config);

extern void gsWrite8(u32 mem, u8 value);
extern void gsWrite16(u32 mem, u16 value);
extern void gsWrite32(u32 mem, u32 value);

extern void gsWrite64_page_00(u32 mem, u64 value);
extern void gsWrite64_page_01(u32 mem, u64 value);
extern void gsWrite64_generic(u32 mem, u64 value);

extern void TAKES_R128 gsWrite128_page_00(u32 mem, r128 value);
extern void TAKES_R128 gsWrite128_page_01(u32 mem, r128 value);
extern void TAKES_R128 gsWrite128_generic(u32 mem, r128 value);

extern u8 gsRead8(u32 mem);
extern u16 gsRead16(u32 mem);
extern u32 gsRead32(u32 mem);
extern u64 gsRead64(u32 mem);
extern u128 gsNonMirroredRead(u32 mem);

void gsIrq();

extern tGS_CSR CSRr;
