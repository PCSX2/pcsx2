// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Global.h"
#include "Dma.h"
#include "IopDma.h"
#include "IopHw.h"

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
	if (SPU2::IsRunningPSXMode() && SPU2::MsgToConsole())
		SPU2::ConLog("ReadInput_HiFi!!!!!\n");

	const u16 ReadIndex = (OutPos * 2) & 0x1FF;

	StereoOut32 retval(
		(s32&)(*GetMemPtr(0x2000 + (Index << 10) + ReadIndex)),
		(s32&)(*GetMemPtr(0x2200 + (Index << 10) + ReadIndex)));

	if (Index == 1)
	{
		retval.Left >>= 16;
		retval.Right >>= 16;
	}

	// Simulate MADR increase, GTA VC tracks the MADR address for calculating a certain point in the buffer
	if (InputDataTransferred)
	{
		u32 amount = std::min(InputDataTransferred, (u32)0x180);

		InputDataTransferred -= amount;
		MADR += amount;
		// Because some games watch the MADR to see when it reaches the end we need to end the DMA here
		// Tom & Jerry War of the Whiskers is one such game, the music will skip
		if (!InputDataTransferred && !InputDataLeft)
		{
			if (Index == 0)
				spu2DMA4Irq();
			else
				spu2DMA7Irq();
		}
	}

	if (ReadIndex == 0x100 || ReadIndex == 0x0 || ReadIndex == 0x80 || ReadIndex == 0x180)
	{
		if (ReadIndex == 0x100)
			InputPosWrite = 0;
		else if (ReadIndex == 0)
			InputPosWrite = 0x100;

		if (InputDataLeft >= 0x100)
		{
			AutoDMAReadBuffer(0);
			AdmaInProgress = 1;
			if (InputDataLeft < 0x100)
			{
				if (IsDevBuild)
				{
					SPU2::FileLog("[%10d] AutoDMA%c block end.\n", Cycles, GetDmaIndexChar());
					if (InputDataLeft > 0)
					{
						if (SPU2::MsgAutoDMA())
							SPU2::ConLog("WARNING: adma buffer didn't finish with a whole block!!\n");
					}
				}

				InputDataLeft = 0;
			}
		}
		else if ((AutoDMACtrl & (Index + 1)))
			AutoDMACtrl |= ~3;
	}
	return retval;
}

StereoOut32 V_Core::ReadInput()
{
	StereoOut32 retval;
	u16 ReadIndex = OutPos;

	for (int i = 0; i < 2; i++)
		if (Cores[i].IRQEnable && (0x2000 + (Index << 10) + ReadIndex) == (Cores[i].IRQA & 0xfffffdff))
			SetIrqCall(i);

	// PlayMode & 2 is Bypass Mode, so it doesn't go through the SPU
	if ((Index == 1) || !(Index == 0 && (PlayMode & 2) != 0))
	{
		retval = StereoOut32(
			(s32)(*GetMemPtr(0x2000 + (Index << 10) + ReadIndex)),
			(s32)(*GetMemPtr(0x2200 + (Index << 10) + ReadIndex)));
	}

#ifdef PCSX2_DEVBUILD
	DebugCores[Index].admaWaveformL[OutPos % 0x100] = retval.Left;
	DebugCores[Index].admaWaveformR[OutPos % 0x100] = retval.Right;
#endif

	// Simulate MADR increase, GTA VC tracks the MADR address for calculating a certain point in the buffer
	if (InputDataTransferred)
	{
		u32 amount = std::min(InputDataTransferred, (u32)0x180);

		InputDataTransferred -= amount;
		MADR += amount;
		// Because some games watch the MADR to see when it reaches the end we need to end the DMA here
		// Tom & Jerry War of the Whiskers is one such game, the music will skip
		if (!InputDataTransferred && !InputDataLeft)
		{
			if (Index == 0)
				spu2DMA4Irq();
			else
				spu2DMA7Irq();
		}
	}

	if (PlayMode == 2 && Index == 0) //Bitstream bypass refills twice as quickly (GTA VC)
		ReadIndex = (ReadIndex * 2) & 0x1FF;

	if (ReadIndex == 0x100 || ReadIndex == 0x0 || ReadIndex == 0x80 || ReadIndex == 0x180)
	{
		if (ReadIndex == 0x100)
			InputPosWrite = 0;
		else if (ReadIndex == 0)
			InputPosWrite = 0x100;

		if (InputDataLeft >= 0x100)
		{
			AutoDMAReadBuffer(0);
			AdmaInProgress = 1;
			if (InputDataLeft < 0x100)
			{
				if (IsDevBuild)
				{
					SPU2::FileLog("[%10d] AutoDMA%c block end.\n", Cycles, GetDmaIndexChar());
					if (InputDataLeft > 0)
					{
						if (SPU2::MsgAutoDMA())
							SPU2::ConLog("WARNING: adma buffer didn't finish with a whole block!!\n");
					}
				}

				InputDataLeft = 0;
			}
		}
		else if ((AutoDMACtrl & (Index + 1)))
			AutoDMACtrl |= ~3;
	}
	return retval;
}
