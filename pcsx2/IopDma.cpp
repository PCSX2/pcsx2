// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "R3000A.h"
#include "Common.h"
#include "SPU2/spu2.h"
#include "IopCounters.h"
#include "IopHw.h"
#include "IopDma.h"
#include "SIO/Sio2.h"

#include "Sif.h"
#include "DEV9/DEV9.h"

using namespace R3000A;

// Dma0/1   in Mdec.c
// Dma3     in CdRom.c
// Dma8     in PsxSpd.c
// Dma11/12 in PsxSio2.c

static void psxDmaGeneric(u32 madr, u32 bcr, u32 chcr, u32 spuCore)
{
	const char dmaNum = spuCore ? 7 : 4;

	/*if (chcr & 0x400) DevCon.Status("SPU 2 DMA %c linked list chain mode! chcr = %x madr = %x bcr = %x\n", dmaNum, chcr, madr, bcr);
	if (chcr & 0x40000000) DevCon.Warning("SPU 2 DMA %c Unusual bit set on 'to' direction chcr = %x madr = %x bcr = %x\n", dmaNum, chcr, madr, bcr);
	if ((chcr & 0x1) == 0) DevCon.Status("SPU 2 DMA %c loading from spu2 memory chcr = %x madr = %x bcr = %x\n", dmaNum, chcr, madr, bcr);*/

	const int size = (bcr >> 16) * (bcr & 0xFFFF);

	// Update the spu2 to the current cycle before initiating the DMA

	SPU2async();
	//Console.Status("cycles sent to SPU2 %x\n", psxRegs.cycle - psxCounters[6].sCycleT);

	psxCounters[6].sCycleT = psxRegs.cycle;
	psxCounters[6].CycleT = size * 4;

	psxNextCounter -= (psxRegs.cycle - psxNextsCounter);
	psxNextsCounter = psxRegs.cycle;
	if (psxCounters[6].CycleT < psxNextCounter)
		psxNextCounter = psxCounters[6].CycleT;

	if ((psxRegs.iopNextEventCycle - psxNextsCounter) > (u32)psxNextCounter)
	{
		//DevCon.Warning("SPU2async Setting new counter branch, old %x new %x ((%x - %x = %x) > %x delta)", g_iopNextEventCycle, psxNextsCounter + psxNextCounter, g_iopNextEventCycle, psxNextsCounter, (g_iopNextEventCycle - psxNextsCounter), psxNextCounter);
		psxRegs.iopNextEventCycle = psxNextsCounter + psxNextCounter;
	}

	switch (chcr)
	{
		case 0x01000201: //cpu to spu2 transfer
			PSXDMA_LOG("*** DMA %d - mem2spu *** %x addr = %x size = %x", dmaNum, chcr, madr, bcr);
			if (dmaNum == 7)
				SPU2writeDMA7Mem((u16*)iopPhysMem(madr), size * 2);
			else if (dmaNum == 4)
				SPU2writeDMA4Mem((u16*)iopPhysMem(madr), size * 2);
			break;

		case 0x01000200: //spu2 to cpu transfer
			PSXDMA_LOG("*** DMA %d - spu2mem *** %x addr = %x size = %x", dmaNum, chcr, madr, bcr);
			if (dmaNum == 7)
				SPU2readDMA7Mem((u16*)iopPhysMem(madr), size * 2);
			else if (dmaNum == 4)
				SPU2readDMA4Mem((u16*)iopPhysMem(madr), size * 2);
			psxCpu->Clear(spuCore ? HW_DMA7_MADR : HW_DMA4_MADR, size);
			break;

		default:
			Console.Error("*** DMA %d - SPU unknown *** %x addr = %x size = %x", dmaNum, chcr, madr, bcr);
			break;
	}
}

void psxDma4(u32 madr, u32 bcr, u32 chcr) // SPU2's Core 0
{
	psxDmaGeneric(madr, bcr, chcr, 0);
}

int psxDma4Interrupt()
{
#ifdef SPU2IRQTEST
	Console.Warning("psxDma4Interrupt()");
#endif
	HW_DMA4_CHCR &= ~0x01000000;
	psxDmaInterrupt(4);
	iopIntcIrq(9);
	return 1;
}

void spu2DMA4Irq()
{
#ifdef SPU2IRQTEST
	Console.Warning("spu2DMA4Irq()");
#endif
	SPU2interruptDMA4();
	if (HW_DMA4_CHCR & 0x01000000)
	{
		HW_DMA4_CHCR &= ~0x01000000;
		psxDmaInterrupt(4);
	}
}

void psxDma7(u32 madr, u32 bcr, u32 chcr) // SPU2's Core 1
{
	psxDmaGeneric(madr, bcr, chcr, 1);
}

int psxDma7Interrupt()
{
#ifdef SPU2IRQTEST
	Console.Warning("psxDma7Interrupt()");
#endif
	HW_DMA7_CHCR &= ~0x01000000;
	psxDmaInterrupt2(0);
	return 1;
}

void spu2DMA7Irq()
{
#ifdef SPU2IRQTEST
	Console.Warning("spu2DMA7Irq()");
#endif
	SPU2interruptDMA7();
	if (HW_DMA7_CHCR & 0x01000000)
	{
		HW_DMA7_CHCR &= ~0x01000000;
		psxDmaInterrupt2(0);
	}
}

#ifndef DISABLE_PSX_GPU_DMAS
void psxDma2(u32 madr, u32 bcr, u32 chcr) // GPU
{
	//DevCon.Warning("SIF2 IOP CHCR = %x MADR = %x BCR = %x first 16bits %x", chcr, madr, bcr, iopMemRead16(madr));
	sif2.iop.busy = true;
	sif2.iop.end = false;
	//SIF2Dma();
	// todo: psxmode: dmaSIF2 appears to interface with PGPU but everything is already handled without it.
	// it slows down psxmode if it's run.
	//dmaSIF2();
}

void psxDma6(u32 madr, u32 bcr, u32 chcr)
{
	u32* mem = (u32*)iopPhysMem(madr);

	PSXDMA_LOG("*** DMA 6 - OT *** %lx addr = %lx size = %lx", chcr, madr, bcr);

	if (chcr == 0x11000002)
	{
		while (bcr--)
		{
			*mem-- = (madr - 4) & 0xffffff;
			madr -= 4;
		}
		mem++;
		*mem = 0xffffff;
	}
	else
	{
		// Unknown option
		PSXDMA_LOG("*** DMA 6 - OT unknown *** %lx addr = %lx size = %lx", chcr, madr, bcr);
	}
	HW_DMA6_CHCR &= ~0x01000000;
	psxDmaInterrupt(6);
}
#endif

void psxDma8(u32 madr, u32 bcr, u32 chcr)
{
	const int size = (bcr >> 16) * (bcr & 0xFFFF) * 8;

	switch (chcr & 0x01000201)
	{
		case 0x01000201: //cpu to dev9 transfer
			PSXDMA_LOG("*** DMA 8 - DEV9 mem2dev9 *** %lx addr = %lx size = %lx", chcr, madr, bcr);
			DEV9writeDMA8Mem((u32*)iopPhysMem(madr), size);
			break;

		case 0x01000200: //dev9 to cpu transfer
			PSXDMA_LOG("*** DMA 8 - DEV9 dev9mem *** %lx addr = %lx size = %lx", chcr, madr, bcr);
			DEV9readDMA8Mem((u32*)iopPhysMem(madr), size);
			break;

		default:
			PSXDMA_LOG("*** DMA 8 - DEV9 unknown *** %lx addr = %lx size = %lx", chcr, madr, bcr);
			break;
	}
	HW_DMA8_CHCR &= ~0x01000000;
	psxDmaInterrupt2(1);
}

void psxDma9(u32 madr, u32 bcr, u32 chcr)
{
	SIF_LOG("IOP: dmaSIF0 chcr = %lx, madr = %lx, bcr = %lx, tadr = %lx", chcr, madr, bcr, HW_DMA9_TADR);

	sif0.iop.busy = true;
	sif0.iop.end = false;

	SIF0Dma();
}

void psxDma10(u32 madr, u32 bcr, u32 chcr)
{
	SIF_LOG("IOP: dmaSIF1 chcr = %lx, madr = %lx, bcr = %lx", chcr, madr, bcr);

	sif1.iop.busy = true;
	sif1.iop.end = false;

	SIF1Dma();
}

void psxDma11(u32 madr, u32 bcr, u32 chcr)
{
	unsigned int i, j;
	int size = (bcr >> 16) * (bcr & 0xffff);
	PSXDMA_LOG("*** DMA 11 - SIO2 in *** %lx addr = %lx size = %lx", chcr, madr, bcr);
	// Set dmaBlockSize, so SIO2 knows to count based on the DMA block rather than SEND3 length.
	// When SEND3 is written, SIO2 will automatically reset this to zero.
	g_Sio2.dmaBlockSize = (bcr & 0xffff) * 4;

	if (chcr != 0x01000201)
	{
		return;
	}

	for (i = 0; i < (bcr >> 16); i++)
	{
		for (j = 0; j < ((bcr & 0xFFFF) * 4); j++)
		{
			const u8 data = iopMemRead8(madr);
			g_Sio2.Write(data);
			madr++;
		}
	}

	HW_DMA11_MADR = madr;
	PSX_INT(IopEvt_Dma11, (size >> 2));
}

void psxDMA11Interrupt()
{
	if (HW_DMA11_CHCR & 0x01000000)
	{
		HW_DMA11_CHCR &= ~0x01000000;
		psxDmaInterrupt2(4);
	}
}

void psxDma12(u32 madr, u32 bcr, u32 chcr)
{
	int size = ((bcr >> 16) * (bcr & 0xFFFF)) * 4;
	PSXDMA_LOG("*** DMA 12 - SIO2 out *** %lx addr = %lx size = %lx", chcr, madr, size);

	if (chcr != 0x41000200)
	{
		return;
	}

	bcr = size;

	while (bcr > 0)
	{
		const u8 data = g_Sio2.Read();
		iopMemWrite8(madr, data);
		bcr--;
		madr++;
	}

	HW_DMA12_MADR = madr;
	PSX_INT(IopEvt_Dma12, (size >> 2));
}

void psxDMA12Interrupt()
{
	if (HW_DMA12_CHCR & 0x01000000)
	{
		HW_DMA12_CHCR &= ~0x01000000;
		psxDmaInterrupt2(5);
	}
}
