/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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
#include "Global.h"
#include "Dma.h"
#include "IopDma.h"

#include "spu2.h" // required for ENABLE_NEW_IOPDMA_SPU2 define

// Core 0 Input is "SPDIF mode" - Source audio is AC3 compressed.

// Core 1 Input is "CDDA mode" - Source audio data is 32 bits.
// PS2 note:  Very! few PS2 games use this mode.  Some PSX games used it, however no
// *known* PS2 game does since it was likely only available if the game was recorded to CD
// media (ie, not available in DVD mode, which almost all PS2 games use).  Plus PS2 games
// generally prefer to use ADPCM streaming audio since they need as much storage space as
// possible for FMVs and high-def textures.
//
StereoOut32 V_Core::ReadInput_HiFi()
{
	if (psxmode)
		ConLog("ReadInput_HiFi!!!!!\n");

	StereoOut32 retval(
		(s32&)(*GetMemPtr(0x2000 + (Index << 10) + OutPos)),
		(s32&)(*GetMemPtr(0x2200 + (Index << 10) + OutPos)));

	if (Index == 1)
	{
		// CDDA Mode:
		// give 30 bit data (SndOut downsamples the rest of the way)
		// HACKFIX: 28 bits seems better according to rama.  I should take some time and do some
		//    bitcounting on this one.  --air
		retval.Left >>= 4;
		retval.Right >>= 4;
	}

	if ((OutPos == 0xFF || OutPos == 0x1FF))
	{
		if (InputDataLeft >= 0x200)
		{
			int oldOutPos = OutPos;
			OutPos = (OutPos + 1) & 0x1FF;
			AutoDMAReadBuffer(0);
			OutPos = oldOutPos;
			AdmaInProgress = 1;

			if (InputDataLeft < 0x200)
			{
				FileLog("[%10d] %s AutoDMA%c block end.\n", (Index == 1) ? "CDDA" : "SPDIF", Cycles, GetDmaIndexChar());

				if (IsDevBuild)
				{
					if (InputDataLeft > 0)
					{
						if (MsgAutoDMA())
							ConLog("WARNING: adma buffer didn't finish with a whole block!!\n");
					}
				}
				InputDataLeft = 0;
				DMAICounter = 0x200 * 4;
				LastClock = *cyclePtr;
			}
		}
	}
	return retval;
}

StereoOut32 V_Core::ReadInput()
{
	StereoOut32 retval;

	if ((Index != 1) || ((PlayMode & 2) == 0))
	{
		for (int i = 0; i < 2; i++)
			if (Cores[i].IRQEnable && 0x2000 + (Index << 10) + OutPos == (Cores[i].IRQA & 0xfffffdff))
				SetIrqCall(i);

		retval = StereoOut32(
			(s32)(*GetMemPtr(0x2000 + (Index << 10) + OutPos)),
			(s32)(*GetMemPtr(0x2200 + (Index << 10) + OutPos)));
	}

#ifdef PCSX2_DEVBUILD
	DebugCores[Index].admaWaveformL[OutPos % 0x100] = retval.Left;
	DebugCores[Index].admaWaveformR[OutPos % 0x100] = retval.Right;
#endif
	
	if (OutPos == 0x100 || OutPos == 0x0 || OutPos == 0x80 || OutPos == 0x180)
	{
		if (OutPos == 0x100)
			InputPosWrite = 0;
		else if (OutPos == 0)
			InputPosWrite = 0x100;

		if (InputDataLeft >= 0x100)
		{
			AutoDMAReadBuffer(0);
			AdmaInProgress = 1;
			if (InputDataLeft < 0x100)
			{
				if (IsDevBuild)
				{
					FileLog("[%10d] AutoDMA%c block end.\n", Cycles, GetDmaIndexChar());
					if (InputDataLeft > 0)
					{
						if (MsgAutoDMA())
							ConLog("WARNING: adma buffer didn't finish with a whole block!!\n");
					}
				}
				
				InputDataLeft = 0;
				DMAICounter = 0x200 * 4;
				LastClock = *cyclePtr;
			}
		}
		else
			if ((AutoDMACtrl & (Index + 1)))
				AutoDMACtrl |= ~3;
	}
	return retval;
}
