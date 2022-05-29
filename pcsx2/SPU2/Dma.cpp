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
#include "R3000A.h"
#include "IopHw.h"

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
	DMA4LogFile = OpenBinaryLog(DMA4LogFileName.c_str());
	DMA7LogFile = OpenBinaryLog(DMA7LogFileName.c_str());
	ADMA4LogFile = OpenBinaryLog("adma4.raw");
	ADMA7LogFile = OpenBinaryLog("adma7.raw");
	ADMAOutLogFile = OpenBinaryLog("admaOut.raw");
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
	u32 spos = InputPosWrite & 0x100; // Starting position passed by TSA
	bool leftbuffer = !(InputPosWrite & 0x80);

	if (InputPosWrite == 0xFFFF) // Data request not made yet
		return;

	AutoDMACtrl &= 0x3;

	int size = std::min(InputDataLeft, (u32)0x200);
	if (!leftbuffer)
		size = 0x100;
	LogAutoDMA(Index ? ADMA7LogFile : ADMA4LogFile);
	//ConLog("Refilling ADMA buffer at %x OutPos %x with %x\n", spos, OutPos, size);
	// HACKFIX!! DMAPtr can be invalid after a savestate load, so the savestate just forces it
	// to nullptr and we ignore it here.  (used to work in old VM editions of PCSX2 with fixed
	// addressing, but new PCSX2s have dynamic memory addressing).
	if (DMAPtr == nullptr)
	{
		DMAPtr = (u16*)iopPhysMem(MADR);
		InputDataProgress = 0;
	}

	if (mode)
	{
		if (DMAPtr != nullptr)
			memcpy(GetMemPtr(0x2000 + (Index << 10) + spos), DMAPtr + InputDataProgress, size);
		MADR += size;
		InputDataLeft -= 0x200;
		InputDataProgress += 0x200;
	}
	else
	{
		while (size)
		{
			if (!leftbuffer)
				spos |= 0x200;
			else
				spos &= ~0x200;

			if (DMAPtr != nullptr)
				memcpy(GetMemPtr(0x2000 + (Index << 10) + spos), DMAPtr + InputDataProgress, 0x200);
			InputDataTransferred += 0x200;
			InputDataLeft -= 0x100;
			InputDataProgress += 0x100;
			leftbuffer = !leftbuffer;
			size -= 0x100;
			InputPosWrite += 0x80;
		}
	}
	if (!(InputPosWrite & 0x80))
		InputPosWrite = 0xFFFF;
}

void V_Core::StartADMAWrite(u16* pMem, u32 sz)
{
	int size = sz;

	TimeUpdate(psxRegs.cycle);

	if (MsgAutoDMA())
		ConLog("* SPU2: DMA%c AutoDMA Transfer of %d bytes to %x (%02x %x %04x).OutPos %x\n",
			   GetDmaIndexChar(), size << 1, ActiveTSA, DMABits, AutoDMACtrl, (~Regs.ATTR) & 0xffff, OutPos);

	InputDataProgress = 0;
	TADR = MADR + (size << 1);
	if ((AutoDMACtrl & (Index + 1)) == 0)
	{
		ActiveTSA = 0x2000 + (Index << 10);
		DMAICounter = size * 4;
		LastClock = psxRegs.cycle;
	}
	else if (size >= 256)
	{
		InputDataLeft = size;
		if (InputPosWrite != 0xFFFF)
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
			AutoDMAReadBuffer(0);
#endif
		}
		AdmaInProgress = 1;
	}
	else
	{
		ConLog("ADMA%c Error Size of %x too small\n", GetDmaIndexChar(), size);
		InputDataLeft = 0;
		DMAICounter = size * 4;
		LastClock = psxRegs.cycle;
	}
}

void V_Core::PlainDMAWrite(u16* pMem, u32 size)
{
	if (MsgToConsole())
	{
		// Don't need this anymore. Target may still be good to know though.
		/*if((uptr)pMem & 15)
		{
			ConLog("* SPU2 DMA Write > Misaligned source. Core: %d  IOP: %p  TSA: 0x%x  Size: 0x%x\n", Index, (void*)pMem, TSA, size);
		}*/

		if (ActiveTSA & 7)
		{
			ConLog("* SPU2 DMA Write > Misaligned target. Core: %d  IOP: %p  TSA: 0x%x  Size: 0x%x\n", Index, (void*)DMAPtr, ActiveTSA, ReadSize);
		}
	}

	TimeUpdate(psxRegs.cycle);

	ReadSize = size;
	IsDMARead = false;
	DMAICounter = 0;
	LastClock = psxRegs.cycle;
	Regs.STATX &= ~0x80;
	Regs.STATX |= 0x400;
	TADR = MADR + (size << 1);

	if (MsgDMA())
		ConLog("* SPU2: DMA%c Write Transfer of %d bytes to %x (%02x %x %04x). IRQE = %d IRQA = %x \n",
			GetDmaIndexChar(), size << 1, ActiveTSA, DMABits, AutoDMACtrl, Regs.ATTR & 0xffff,
			Cores[Index].IRQEnable, Cores[Index].IRQA);

	FinishDMAwrite();
}

void V_Core::FinishDMAwrite()
{
	if (DMAPtr == nullptr)
	{
		DMAPtr = (u16*)iopPhysMem(MADR);
	}

	DMAICounter = ReadSize;

	if (Index == 0)
		DMA4LogWrite(DMAPtr, ReadSize << 1);
	else
		DMA7LogWrite(DMAPtr, ReadSize << 1);

	u32 buff1end = ActiveTSA + std::min(ReadSize, (u32)0x100 + std::abs(DMAICounter / 4));
	u32 start = ActiveTSA;
	u32 buff2end = 0;
	if (buff1end > 0x100000)
	{
		buff2end = buff1end - 0x100000;
		buff1end = 0x100000;
	}

	const int cacheIdxStart = ActiveTSA / pcm_WordsPerBlock;
	const int cacheIdxEnd = (buff1end + pcm_WordsPerBlock - 1) / pcm_WordsPerBlock;
	PcmCacheEntry* cacheLine = &pcm_cache_data[cacheIdxStart];
	PcmCacheEntry& cacheEnd = pcm_cache_data[cacheIdxEnd];

	do
	{
		cacheLine->Validated = false;
		cacheLine++;
	} while (cacheLine != &cacheEnd);

	//ConLog( "* SPU2: Cache Clear Range!  TSA=0x%x, TDA=0x%x (low8=0x%x, high8=0x%x, len=0x%x)\n",
	//	ActiveTSA, buff1end, flagTSA, flagTDA, clearLen );


	// First Branch needs cleared:
	// It starts at TSA and goes to buff1end.

	const u32 buff1size = (buff1end - ActiveTSA);
	memcpy(GetMemPtr(ActiveTSA), DMAPtr, buff1size * 2);

	u32 TDA;

	if (buff2end > 0)
	{
		// second branch needs copied:
		// It starts at the beginning of memory and moves forward to buff2end

		// endpoint cache should be irrelevant, since it's almost certainly dynamic
		// memory below 0x2800 (registers and such)
		//const u32 endpt2 = (buff2end + roundUp) / indexer_scalar;
		//memset( pcm_cache_flags, 0, endpt2 );
		TDA = buff1end;

		DMAPtr += TDA - ActiveTSA;
		ReadSize -= TDA - ActiveTSA;
		ActiveTSA = 0;
		// Emulation Grayarea: Should addresses wrap around to zero, or wrap around to
		// 0x2800?  Hard to know for sure (almost no games depend on this)
		memcpy(GetMemPtr(0), DMAPtr, buff2end * 2);
		TDA = (buff2end) & 0xfffff;

		// Flag interrupt?  If IRQA occurs between start and dest, flag it.
		// Important: Test both core IRQ settings for either DMA!
		// Note: Because this buffer wraps, we use || instead of &&

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

			if (Cores[i].IRQEnable && (Cores[i].IRQA > start || Cores[i].IRQA <= TDA))
			{
				//ConLog("DMAwrite Core %d: IRQ Called (IRQ passed). IRQA = %x Cycles = %d\n", i, Cores[i].IRQA, Cycles );
				SetIrqCallDMA(i);
			}
		}
	}
	else
	{
		// Buffer doesn't wrap/overflow!
		// Just set the TDA and check for an IRQ...

		TDA = buff1end;

		// Flag interrupt?  If IRQA occurs between start and dest, flag it.
		// Important: Test both core IRQ settings for either DMA!
		for (int i = 0; i < 2; i++)
		{
			if (Cores[i].IRQEnable && (Cores[i].IRQA > ActiveTSA && Cores[i].IRQA <= TDA))
			{
				//ConLog("DMAwrite Core %d: IRQ Called (IRQ passed). IRQA = %x Cycles = %d\n", i, Cores[i].IRQA, Cycles );
				SetIrqCallDMA(i);
			}
		}
	}

	DMAPtr += TDA - ActiveTSA;
	ReadSize -= TDA - ActiveTSA;

	DMAICounter = (DMAICounter - ReadSize) * 4;

	if (((psxCounters[6].sCycleT + psxCounters[6].CycleT) - psxRegs.cycle) > (u32)DMAICounter)
	{
		psxCounters[6].sCycleT = psxRegs.cycle;
		psxCounters[6].CycleT = DMAICounter;

		psxNextCounter -= (psxRegs.cycle - psxNextsCounter);
		psxNextsCounter = psxRegs.cycle;
		if (psxCounters[6].CycleT < psxNextCounter)
			psxNextCounter = psxCounters[6].CycleT;
	}

	ActiveTSA = TDA;
	ActiveTSA &= 0xfffff;
	TSA = ActiveTSA;
}

void V_Core::FinishDMAread()
{
	u32 buff1end = ActiveTSA + std::min(ReadSize, (u32)0x100 + std::abs(DMAICounter / 4));
	u32 start = ActiveTSA;
	u32 buff2end = 0;

	if (buff1end > 0x100000)
	{
		buff2end = buff1end - 0x100000;
		buff1end = 0x100000;
	}

	if (DMAPtr == nullptr)
	{
		DMAPtr = (u16*)iopPhysMem(MADR);
	}

	const u32 buff1size = (buff1end - ActiveTSA);
	memcpy(DMARPtr, GetMemPtr(ActiveTSA), buff1size * 2);
	// Note on TSA's position after our copy finishes:
	// IRQA should be measured by the end of the writepos+0x20.  But the TDA
	// should be written back at the precise endpoint of the xfer.
	u32 TDA;

	if (buff2end > 0)
	{

		TDA = buff1end;

		DMARPtr += TDA - ActiveTSA;
		ReadSize -= TDA - ActiveTSA;
		ActiveTSA = 0;

		// second branch needs cleared:
		// It starts at the beginning of memory and moves forward to buff2end
		memcpy(DMARPtr, GetMemPtr(0), buff2end * 2);

		TDA = (buff2end) & 0xfffff;

		// Flag interrupt?  If IRQA occurs between start and dest, flag it.
		// Important: Test both core IRQ settings for either DMA!
		// Note: Because this buffer wraps, we use || instead of &&

		for (int i = 0; i < 2; i++)
		{
			if (Cores[i].IRQEnable && (Cores[i].IRQA > start || Cores[i].IRQA <= TDA))
			{
				SetIrqCallDMA(i);
			}
		}
	}
	else
	{
		// Buffer doesn't wrap/overflow!
		// Just set the TDA and check for an IRQ...

		TDA = buff1end;

		// Flag interrupt?  If IRQA occurs between start and dest, flag it.
		// Important: Test both core IRQ settings for either DMA!

		for (int i = 0; i < 2; i++)
		{
			if (Cores[i].IRQEnable && (Cores[i].IRQA > ActiveTSA && Cores[i].IRQA <= TDA))
			{
				SetIrqCallDMA(i);
			}
		}
	}

	DMARPtr += TDA - ActiveTSA;
	ReadSize -= TDA - ActiveTSA;

	// DMA Reads are done AFTER the delay, so to get the timing right we need to scheule one last DMA to catch IRQ's
	if (ReadSize)
		DMAICounter = std::min(ReadSize, (u32)0x100) * 4;
	else
		DMAICounter = 4;

	if (((psxCounters[6].sCycleT + psxCounters[6].CycleT) - psxRegs.cycle) > (u32)DMAICounter)
	{
		psxCounters[6].sCycleT = psxRegs.cycle;
		psxCounters[6].CycleT = DMAICounter;

		psxNextCounter -= (psxRegs.cycle - psxNextsCounter);
		psxNextsCounter = psxRegs.cycle;
		if (psxCounters[6].CycleT < psxNextCounter)
			psxNextCounter = psxCounters[6].CycleT;
	}

	ActiveTSA = TDA;
	ActiveTSA &= 0xfffff;
	TSA = ActiveTSA;
}

void V_Core::DoDMAread(u16* pMem, u32 size)
{
	TimeUpdate(psxRegs.cycle);

	DMARPtr = pMem;
	ActiveTSA = TSA & 0xfffff;
	ReadSize = size;
	IsDMARead = true;
	LastClock = psxRegs.cycle;
	DMAICounter = std::min(ReadSize, (u32)0x100) * 4;
	Regs.STATX &= ~0x80;
	Regs.STATX |= 0x400;
	//Regs.ATTR |= 0x30;
	TADR = MADR + (size << 1);

	if (((psxCounters[6].sCycleT + psxCounters[6].CycleT) - psxRegs.cycle) > (u32)DMAICounter)
	{
		psxCounters[6].sCycleT = psxRegs.cycle;
		psxCounters[6].CycleT = DMAICounter;

		psxNextCounter -= (psxRegs.cycle - psxNextsCounter);
		psxNextsCounter = psxRegs.cycle;
		if (psxCounters[6].CycleT < psxNextCounter)
			psxNextCounter = psxCounters[6].CycleT;
	}

	if (MsgDMA())
		ConLog("* SPU2: DMA%c Read Transfer of %d bytes from %x (%02x %x %04x). IRQE = %d IRQA = %x \n",
			GetDmaIndexChar(), size << 1, ActiveTSA, DMABits, AutoDMACtrl, Regs.ATTR & 0xffff,
			Cores[Index].IRQEnable, Cores[Index].IRQA);
}

void V_Core::DoDMAwrite(u16* pMem, u32 size)
{
	DMAPtr = pMem;

	if (size < 2)
	{
		Regs.STATX &= ~0x80;
		//Regs.ATTR |= 0x30;
		DMAICounter = 1 * 4;
		LastClock = psxRegs.cycle;
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

	ActiveTSA = TSA & 0xfffff;

	bool adma_enable = ((AutoDMACtrl & (Index + 1)) == (Index + 1));

	if (adma_enable)
	{
		StartADMAWrite(pMem, size);
	}
	else
	{
		PlainDMAWrite(pMem, size);
		Regs.STATX &= ~0x80;
		Regs.STATX |= 0x400;
	}
}
