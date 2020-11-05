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

#include "spu2.h" // temporary until I resolve cyclePtr/TimeUpdate dependencies.

extern u8 callirq;

static FILE* DMA4LogFile = nullptr;
static FILE* DMA7LogFile = nullptr;
static FILE* ADMA4LogFile = nullptr;
static FILE* ADMA7LogFile = nullptr;
static FILE* ADMAOutLogFile = nullptr;

static FILE* REGWRTLogFile[2] = {0, 0};

void DMALogOpen()
{
	if (!DMALog())
		return;
	DMA4LogFile = OpenBinaryLog(DMA4LogFileName);
	DMA7LogFile = OpenBinaryLog(DMA7LogFileName);
	ADMA4LogFile = OpenBinaryLog(L"adma4.raw");
	ADMA7LogFile = OpenBinaryLog(L"adma7.raw");
	ADMAOutLogFile = OpenBinaryLog(L"admaOut.raw");
}

void DMA4LogWrite(void* lpData, u32 ulSize)
{
	if (!DMALog())
		return;
	if (!DMA4LogFile)
		return;
	fwrite(lpData, ulSize, 1, DMA4LogFile);
}

void DMA7LogWrite(void* lpData, u32 ulSize)
{
	if (!DMALog())
		return;
	if (!DMA7LogFile)
		return;
	fwrite(lpData, ulSize, 1, DMA7LogFile);
}

void ADMAOutLogWrite(void* lpData, u32 ulSize)
{
	if (!DMALog())
		return;
	if (!ADMAOutLogFile)
		return;
	fwrite(lpData, ulSize, 1, ADMAOutLogFile);
}

void RegWriteLog(u32 core, u16 value)
{
	if (!DMALog())
		return;
	if (!REGWRTLogFile[core])
		return;
	fwrite(&value, 2, 1, REGWRTLogFile[core]);
}

void DMALogClose()
{
	safe_fclose(DMA4LogFile);
	safe_fclose(DMA7LogFile);
	safe_fclose(REGWRTLogFile[0]);
	safe_fclose(REGWRTLogFile[1]);
	safe_fclose(ADMA4LogFile);
	safe_fclose(ADMA7LogFile);
	safe_fclose(ADMAOutLogFile);
}

void V_Core::LogAutoDMA(FILE* fp)
{
	if (!DMALog() || !fp || !DMAPtr)
		return;
	fwrite(DMAPtr + InputDataProgress, 0x400, 1, fp);
}

void V_Core::AutoDMAReadBuffer(int mode) //mode: 0= split stereo; 1 = do not split stereo
{
	int spos = ((InputPosRead + 0xff) & 0x100); //starting position of the free buffer

	LogAutoDMA(Index ? ADMA7LogFile : ADMA4LogFile);

	// HACKFIX!! DMAPtr can be invalid after a savestate load, so the savestate just forces it
	// to nullptr and we ignore it here.  (used to work in old VM editions of PCSX2 with fixed
	// addressing, but new PCSX2s have dynamic memory addressing).

	if (mode)
	{
		if (DMAPtr != nullptr)
			//memcpy((ADMATempBuffer+(spos<<1)),DMAPtr+InputDataProgress,0x400);
			memcpy(GetMemPtr(0x2000 + (Index << 10) + spos), DMAPtr + InputDataProgress, 0x400);
		MADR += 0x400;
		InputDataLeft -= 0x200;
		InputDataProgress += 0x200;
	}
	else
	{
		if (DMAPtr != nullptr)
			//memcpy((ADMATempBuffer+spos),DMAPtr+InputDataProgress,0x200);
			memcpy(GetMemPtr(0x2000 + (Index << 10) + spos), DMAPtr + InputDataProgress, 0x200);
		MADR += 0x200;
		InputDataLeft -= 0x100;
		InputDataProgress += 0x100;

		if (DMAPtr != nullptr)
			//memcpy((ADMATempBuffer+spos+0x200),DMAPtr+InputDataProgress,0x200);
			memcpy(GetMemPtr(0x2200 + (Index << 10) + spos), DMAPtr + InputDataProgress, 0x200);
		MADR += 0x200;
		InputDataLeft -= 0x100;
		InputDataProgress += 0x100;
	}
	// See ReadInput at mixer.cpp for explanation on the commented out lines
	//
}

void V_Core::StartADMAWrite(u16* pMem, u32 sz)
{
	int size = (sz) & (~511);

	if (MsgAutoDMA())
		ConLog("* SPU2: DMA%c AutoDMA Transfer of %d bytes to %x (%02x %x %04x).\n",
			   GetDmaIndexChar(), size << 1, TSA, DMABits, AutoDMACtrl, (~Regs.ATTR) & 0x7fff);

	InputDataProgress = 0;
	if ((AutoDMACtrl & (Index + 1)) == 0)
	{
		TSA = 0x2000 + (Index << 10);
		DMAICounter = size;
	}
	else if (size >= 512)
	{
		InputDataLeft = size;
		if (AdmaInProgress == 0)
		{
#ifdef PCM24_S1_INTERLEAVE
			if ((Index == 1) && ((PlayMode & 8) == 8))
			{
				AutoDMAReadBuffer(Index, 1);
			}
			else
			{
				AutoDMAReadBuffer(Index, 0);
			}
#else
			if (((PlayMode & 4) == 4) && (Index == 0))
				Cores[0].InputPosRead = 0;

			AutoDMAReadBuffer(0);
#endif
			// Klonoa 2
			if (size == 512)
				DMAICounter = size;
		}

		AdmaInProgress = 1;
	}
	else
	{
		InputDataLeft = 0;
		DMAICounter = 1;
	}
	TADR = MADR + (size << 1);
}

// HACKFIX: The BIOS breaks if we check the IRQA for both cores when issuing DMA writes.  The
// breakage is a null psxRegs.pc being loaded form some memory address (haven't traced it deeper
// yet).  We get around it by only checking the current core's IRQA, instead of doing the
// *correct* thing and checking both.  This might break some games, but having a working BIOS
// is more important for now, until a proper fix can be uncovered.
//
// This problem might be caused by bad DMA timings in the IOP or a lack of proper IRQ
// handling by the Effects Processor.  After those are implemented, let's hope it gets
// magically fixed?
//
// Note: This appears to affect DMA Writes only, so DMA Read DMAs are left intact (both core
// IRQAs are tested).  Very few games use DMA reads tho, so it could just be a case of "works
// by the grace of not being used."
//
// Update: This hack is no longer needed when we don't do a core reset. Guess the null pc was in spu2 memory?
#define NO_BIOS_HACKFIX 1 // set to 1 to disable the hackfix

void V_Core::PlainDMAWrite(u16* pMem, u32 size)
{
	// Perform an alignment check.
	// Not really important.  Everything should work regardless,
	// but it could be indicative of an emulation foopah elsewhere.

	if (MsgToConsole())
	{
		// Don't need this anymore. Target may still be good to know though.
		/*if((uptr)pMem & 15)
		{
			ConLog("* SPU2 DMA Write > Misaligned source. Core: %d  IOP: %p  TSA: 0x%x  Size: 0x%x\n", Index, (void*)pMem, TSA, size);
		}*/

		if (TSA & 7)
		{
			ConLog("* SPU2 DMA Write > Misaligned target. Core: %d  IOP: %p  TSA: 0x%x  Size: 0x%x\n", Index, (void*)pMem, TSA, size);
		}
	}

	if (Index == 0)
		DMA4LogWrite(pMem, size << 1);
	else
		DMA7LogWrite(pMem, size << 1);

	TSA &= 0xfffff;

	u32 buff1end = TSA + size;
	u32 buff2end = 0;
	if (buff1end > 0x100000)
	{
		buff2end = buff1end - 0x100000;
		buff1end = 0x100000;
	}

	const int cacheIdxStart = TSA / pcm_WordsPerBlock;
	const int cacheIdxEnd = (buff1end + pcm_WordsPerBlock - 1) / pcm_WordsPerBlock;
	PcmCacheEntry* cacheLine = &pcm_cache_data[cacheIdxStart];
	PcmCacheEntry& cacheEnd = pcm_cache_data[cacheIdxEnd];

	do
	{
		cacheLine->Validated = false;
		cacheLine++;
	} while (cacheLine != &cacheEnd);

	//ConLog( "* SPU2: Cache Clear Range!  TSA=0x%x, TDA=0x%x (low8=0x%x, high8=0x%x, len=0x%x)\n",
	//	TSA, buff1end, flagTSA, flagTDA, clearLen );


	// First Branch needs cleared:
	// It starts at TSA and goes to buff1end.

	const u32 buff1size = (buff1end - TSA);
	memcpy(GetMemPtr(TSA), pMem, buff1size * 2);

	u32 TDA;

	if (buff2end > 0)
	{
		// second branch needs copied:
		// It starts at the beginning of memory and moves forward to buff2end

		// endpoint cache should be irrelevant, since it's almost certainly dynamic
		// memory below 0x2800 (registers and such)
		//const u32 endpt2 = (buff2end + roundUp) / indexer_scalar;
		//memset( pcm_cache_flags, 0, endpt2 );

		// Emulation Grayarea: Should addresses wrap around to zero, or wrap around to
		// 0x2800?  Hard to know for sure (almost no games depend on this)

		memcpy(GetMemPtr(0), &pMem[buff1size], buff2end * 2);
		TDA = (buff2end + 1) & 0xfffff;

		// Flag interrupt?  If IRQA occurs between start and dest, flag it.
		// Important: Test both core IRQ settings for either DMA!
		// Note: Because this buffer wraps, we use || instead of &&

#if NO_BIOS_HACKFIX
		for (int i = 0; i < 2; i++)
		{
			// Start is exclusive and end is inclusive... maybe? The end is documented to be inclusive,
			// which suggests that memory access doesn't trigger interrupts, incrementing registers does
			// (which would mean that if TSA=IRQA an interrupt doesn't fire... I guess?)
			// Chaos Legion uses interrupt addresses set to the beginning of the two buffers in a double
			// buffer scheme and sets LSA of one of the voices to the start of the opposite buffer.
			// However it transfers to the same address right after setting IRQA, which by our previous
			// understanding would trigger the interrupt early causing it to switch buffers again immediately
			// and an interrupt never fires again, leaving the voices looping the same samples forever.

			if (Cores[i].IRQEnable && (Cores[i].IRQA > TSA || Cores[i].IRQA <= TDA))
			{
				//ConLog("DMAwrite Core %d: IRQ Called (IRQ passed). IRQA = %x Cycles = %d\n", i, Cores[i].IRQA, Cycles );
				SetIrqCall(i);
			}
		}
#else
		if ((IRQEnable && (IRQA > TSA || IRQA <= TDA))
		{
			SetIrqCall(Index);
		}
#endif
	}
	else
	{
		// Buffer doesn't wrap/overflow!
		// Just set the TDA and check for an IRQ...

		TDA = (buff1end + 1) & 0xfffff;

		// Flag interrupt?  If IRQA occurs between start and dest, flag it.
		// Important: Test both core IRQ settings for either DMA!

#if NO_BIOS_HACKFIX
		for (int i = 0; i < 2; i++)
		{
			if (Cores[i].IRQEnable && (Cores[i].IRQA > TSA && Cores[i].IRQA <= TDA))
			{
				//ConLog("DMAwrite Core %d: IRQ Called (IRQ passed). IRQA = %x Cycles = %d\n", i, Cores[i].IRQA, Cycles );
				SetIrqCall(i);
			}
		}
#else
		if (IRQEnable && (IRQA > TSA) && (IRQA <= TDA))
		{
			SetIrqCall(Index);
		}
#endif
	}

	TSA = TDA;
	DMAICounter = size;
	TADR = MADR + (size << 1);
}

void V_Core::DoDMAread(u16* pMem, u32 size)
{
	TSA &= 0xfffff;

	u32 buff1end = TSA + size;
	u32 buff2end = 0;
	if (buff1end > 0x100000)
	{
		buff2end = buff1end - 0x100000;
		buff1end = 0x100000;
	}

	const u32 buff1size = (buff1end - TSA);
	memcpy(pMem, GetMemPtr(TSA), buff1size * 2);

	// Note on TSA's position after our copy finishes:
	// IRQA should be measured by the end of the writepos+0x20.  But the TDA
	// should be written back at the precise endpoint of the xfer.

	u32 TDA;

	if (buff2end > 0)
	{
		// second branch needs cleared:
		// It starts at the beginning of memory and moves forward to buff2end

		memcpy(&pMem[buff1size], GetMemPtr(0), buff2end * 2);

		TDA = (buff2end + 0x20) & 0xfffff;

		// Flag interrupt?  If IRQA occurs between start and dest, flag it.
		// Important: Test both core IRQ settings for either DMA!
		// Note: Because this buffer wraps, we use || instead of &&

		for (int i = 0; i < 2; i++)
		{
			if (Cores[i].IRQEnable && (Cores[i].IRQA > TSA || Cores[i].IRQA <= TDA))
			{
				SetIrqCall(i);
			}
		}
	}
	else
	{
		// Buffer doesn't wrap/overflow!
		// Just set the TDA and check for an IRQ...

		TDA = (buff1end + 0x20) & 0xfffff;

		// Flag interrupt?  If IRQA occurs between start and dest, flag it.
		// Important: Test both core IRQ settings for either DMA!

		for (int i = 0; i < 2; i++)
		{
			if (Cores[i].IRQEnable && (Cores[i].IRQA > TSA && Cores[i].IRQA <= TDA))
			{
				SetIrqCall(i);
			}
		}
	}

	TSA = TDA;

	DMAICounter = size;
	Regs.STATX &= ~0x80;
	//Regs.ATTR |= 0x30;
	TADR = MADR + (size << 1);
}

void V_Core::DoDMAwrite(u16* pMem, u32 size)
{
	DMAPtr = pMem;

	if (size < 2)
	{
		Regs.STATX &= ~0x80;
		//Regs.ATTR |= 0x30;
		DMAICounter = 1;

		return;
	}

	if (IsDevBuild)
	{
		DebugCores[Index].lastsize = size;
		DebugCores[Index].dmaFlag = 2;
	}

	if (MsgToConsole())
	{
		if (TSA > 0xfffff)
		{
			ConLog("* SPU2: Transfer Start Address out of bounds. TSA is %x\n", TSA);
		}
	}

	TSA &= 0xfffff;

	bool adma_enable = ((AutoDMACtrl & (Index + 1)) == (Index + 1));

	if (adma_enable)
	{
		TSA &= 0x1fff;
		StartADMAWrite(pMem, size);
	}
	else
	{
		if (MsgDMA())
			ConLog("* SPU2: DMA%c Transfer of %d bytes to %x (%02x %x %04x). IRQE = %d IRQA = %x \n",
				   GetDmaIndexChar(), size << 1, TSA, DMABits, AutoDMACtrl, (~Regs.ATTR) & 0x7fff,
				   Cores[0].IRQEnable, Cores[0].IRQA);

		PlainDMAWrite(pMem, size);
	}
	Regs.STATX &= ~0x80;
	//Regs.ATTR |= 0x30;
}
