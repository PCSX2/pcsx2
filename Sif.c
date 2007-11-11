/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2004  Pcsx2 Team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <malloc.h>
#include "Common.h"
#include "PsxCommon.h"
#include "Sif.h"
#include "Sifcmd.h"

#define sif0dma ((DMACh*)&PS2MEM_HW[0xc000])
#define sif1dma ((DMACh*)&PS2MEM_HW[0xc400])
#define sif2dma ((DMACh*)&PS2MEM_HW[0xc800])

#define FIFO_SIF0_W 128
#define FIFO_SIF1_W 128

typedef struct {
	u32 fifoData[FIFO_SIF0_W];	
	int fifoReadPos;
	int fifoWritePos;
	int fifoSize;
	int chain;
	int end;
	int tagMode;
	int counter;
	struct sifData sifData;
} _sif0;

typedef struct {
	u32 fifoData[FIFO_SIF1_W];
	int fifoReadPos;
	int fifoWritePos;
	int fifoSize;
	int chain;
	int end;
	int tagMode;
	int counter;
} _sif1;

_sif0 sif0;
_sif1 sif1;

int sifInit() {
	memset(&sif0, 0, sizeof(sif0));
	memset(&sif1, 0, sizeof(sif1));

	return 0;
}

void SIF0write(u32 *from, int words)
{
	if(words > (FIFO_SIF0_W-sif0.fifoWritePos)) {
		int wP0 = (FIFO_SIF0_W-sif0.fifoWritePos);
		int wP1 = words - wP0;
		memcpy_fast(&sif0.fifoData[sif0.fifoWritePos], from, wP0 << 2);
		memcpy_fast(&sif0.fifoData[0], &from[wP0], wP1 << 2);
		sif0.fifoWritePos = wP1;
	}
	else
	{
		memcpy_fast(&sif0.fifoData[sif0.fifoWritePos], from, words << 2);
		sif0.fifoWritePos += words;
	}

	sif0.fifoSize += words;
#ifdef SIF_LOG
	SIF_LOG("  SIF0 + %d = %d (pos=%d)\n", words, sif0.fifoSize, sif0.fifoWritePos);
//    {
//        int i;
//        for(i = 0; i < words; i += 4) {
//        SIF_LOG(" EE SIF write data: %x %x %x %x\n", from[i], from[i+1], from[i+2], from[i+3]);
//        }
//    }
#endif

/*	if (sif0.fifoSize == FIFO_SIF0_W) {
		Cpu->ExecuteBlock();
	}*/
}

void SIF0read(u32 *to, int words)
{
	if(words > (FIFO_SIF0_W-sif0.fifoReadPos))
	{
		int wP0 = (FIFO_SIF0_W-sif0.fifoReadPos);
		int wP1 = words - wP0;
		memcpy_fast(to, &sif0.fifoData[sif0.fifoReadPos], wP0 << 2);
		memcpy_fast(&to[wP0], &sif0.fifoData[0], wP1 << 2);
		sif0.fifoReadPos = wP1;
	}
	else
	{
		memcpy_fast(to, &sif0.fifoData[sif0.fifoReadPos], words << 2);
		sif0.fifoReadPos += words;
	}

	sif0.fifoSize -= words;
#ifdef SIF_LOG
	SIF_LOG("  SIF0 - %d = %d (pos=%d)\n", words, sif0.fifoSize, sif0.fifoReadPos);
#endif
}

void SIF1write(u32 *from, int words)
{
	if(words > (FIFO_SIF1_W-sif1.fifoWritePos))
	{
		int wP0 = (FIFO_SIF1_W-sif1.fifoWritePos);
		int wP1 = words - wP0;
		memcpy_fast(&sif1.fifoData[sif1.fifoWritePos], from, wP0 << 2);
		memcpy_fast(&sif1.fifoData[0], &from[wP0], wP1 << 2);
		sif1.fifoWritePos = wP1;
	}
	else
	{
		memcpy_fast(&sif1.fifoData[sif1.fifoWritePos], from, words << 2);
		sif1.fifoWritePos += words;
	}

	sif1.fifoSize += words;
#ifdef SIF_LOG
	SIF_LOG("  SIF1 + %d = %d (pos=%d)\n", words, sif1.fifoSize, sif1.fifoWritePos);
#endif

/*	if (sif1.fifoSize == FIFO_SIF1_W) {
		psxCpu->ExecuteBlock();
	}*/
}

void SIF1read(u32 *to, int words)
{
	if(words > (FIFO_SIF1_W-sif1.fifoReadPos))
	{
		int wP0 = (FIFO_SIF1_W-sif1.fifoReadPos);
		int wP1 = words - wP0;
		memcpy_fast(to, &sif1.fifoData[sif1.fifoReadPos], wP0 << 2);
		memcpy_fast(&to[wP0], &sif1.fifoData[0], wP1 << 2);
		sif1.fifoReadPos = wP1;
	}
	else
	{
		memcpy_fast(to, &sif1.fifoData[sif1.fifoReadPos], words << 2);
		sif1.fifoReadPos += words;
	}

	sif1.fifoSize -= words;
#ifdef SIF_LOG
	SIF_LOG("  SIF1 - %d = %d (pos=%d)\n", words, sif1.fifoSize, sif1.fifoReadPos);
#endif
}

void SIF0Dma()
{
	u32 *ptag;
	int notDone;
	//int cycles = 0, psxCycles = 0;

#ifdef SIF_LOG
	SIF_LOG("SIF0 DMA start...\n");
#endif

	do
	{
		notDone = 0;
		/*if ((psHu32(DMAC_CTRL) & 0xC0)) { 
			SysPrintf("DMA Stall Control %x\n",(psHu32(DMAC_CTRL) & 0xC0));
		}*/
		if(HW_DMA9_CHCR & 0x01000000) // If EE SIF0 is enabled
		{
			int size = sif0.counter; //HW_DMA9_BCR >> 16;

			if(size == 0) // If there's no more to transfer
			{
				// Note.. add normal mode here
				if (sif0.sifData.data & 0xC0000000) // If NORMAL mode or end of CHAIN, or interrupt then stop DMA
				{
#ifdef SIF_LOG
					SIF_LOG(" IOP SIF Stopped\n");
#endif

					// Stop & signal interrupts on IOP
					HW_DMA9_CHCR &= ~0x01000000; //reset TR flag
					psxDmaInterrupt2(2);
					//hwIntcIrq(INTC_SBUS);
					sif0.sifData.data = 0;
				}
				else if(FIFO_SIF1_W-sif0.fifoSize >= 2) // Chain mode
				{
					// Process DMA tag at HW_DMA9_TADR
					sif0.sifData = *(struct sifData *)PSXM(HW_DMA9_TADR);

					sif0.sifData.words = (sif0.sifData.words + 3) & 0xfffffffc; // Round up to nearest 4.

					SIF0write((u32*)PSXM(HW_DMA9_TADR+8), 4);

					//psxCycles += 2;

                    HW_DMA9_MADR = sif0.sifData.data & 0xFFFFFF;
					HW_DMA9_TADR += 16; ///HW_DMA9_MADR + 16 + sif0.sifData.words << 2;
					//HW_DMA9_BCR = (sif0.sifData.words << 16) | 1;
					sif0.counter = sif0.sifData.words & 0xFFFFFF;
					notDone = 1;

#ifdef SIF_LOG
					SIF_LOG(" SIF0 Tag: madr=%lx, tadr=%lx, counter=%lx (%08X_%08X)\n", HW_DMA9_MADR, HW_DMA9_TADR, sif0.counter, sif0.sifData.words, sif0.sifData.data);
#endif
					if(sif0.sifData.data & 0x40000000)
					{
#ifdef SIF_LOG
						SIF_LOG("   END\n");
#endif
					}
					else
					{
#ifdef SIF_LOG
						SIF_LOG("   CNT %08X, %08X\n", sif0.sifData.data, sif0.sifData.words);
#endif
					}
				}
			}
			else // There's some data ready to transfer into the fifo..
			{
				int wTransfer = sif0.counter; // HW_DMA9_BCR >> 16;

				if (wTransfer > (FIFO_SIF0_W-sif0.fifoSize))
					wTransfer = (FIFO_SIF0_W-sif0.fifoSize);

#ifdef SIF_LOG
				SIF_LOG("+++++++++++ %lX of %lX\n", wTransfer, sif0.counter /*(HW_DMA9_BCR >> 16)*/ );
#endif

				SIF0write((u32*)PSXM(HW_DMA9_MADR), wTransfer);
				HW_DMA9_MADR += wTransfer << 2;
				//HW_DMA9_BCR = (HW_DMA9_BCR & 0xFFFF) | (((HW_DMA9_BCR >> 16) - wTransfer)<<16);
				//psxCycles += (wTransfer / 4) * BIAS;
				sif0.counter -= wTransfer;

				notDone = 1;		
			}
		}

	if(sif0dma->chcr & 0x100) // If EE SIF enabled and there's something to transfer
		{
			int size = sif0dma->qwc;
			if ((psHu32(DMAC_CTRL) & 0x30) == 0x10) { // STS == fromSIF0
				SysPrintf("SIF0 stall control\n");
			}
			if(size > 0) // If we're reading something continue to do so
			{
				if(sif0.fifoSize > 0)
				{
					int readSize = size;

					if(readSize > (sif0.fifoSize/4))
						readSize = (sif0.fifoSize/4);

					//SIF_LOG(" EE SIF doing transfer %04Xqw to %08X\n", readSize, sif0dma->madr);
#ifdef SIF_LOG
					SIF_LOG("----------- %lX of %lX\n", readSize << 2, size << 2 );
#endif

					_dmaGetAddr(sif0dma, ptag, sif0dma->madr, 5);

					SIF0read((u32*)ptag, readSize*4);
//                    {
//                        int i;
//                        for(i = 0; i < readSize; ++i) {
//                            SIF_LOG("EE SIF0 read madr: %x %x %x %x\n", ((u32*)ptag)[4*i+0], ((u32*)ptag)[4*i+1], ((u32*)ptag)[4*i+2], ((u32*)ptag)[4*i+3]);
//                        }
//                    }

					Cpu->Clear(sif0dma->madr, readSize*4);

					//cycles += readSize * BIAS;
					sif0dma->qwc -= readSize;
					sif0dma->madr += readSize << 4;

					notDone = 1;
				}
			}
			else
			{
				if(sif0.chain && sif0dma->chcr & 0x80000000) // Stop on tag IRQ
				{
					// Tag interrupt
#ifdef SIF_LOG
					SIF_LOG(" EE SIF interrupt\n");
#endif
					sif0dma->chcr &= ~0x100;
					hwDmacIrq(5);
					notDone = 0;
				}
				else if(sif0.end) // Stop on tag END
				{
					// End tag.
#ifdef SIF_LOG
					SIF_LOG(" EE SIF end\n");
#endif
					sif0dma->chcr &= ~0x100;
					hwDmacIrq(5);
					notDone = 0;
				}
				else if(sif0.fifoSize >= 4) // Read a tag
				{
					static PCSX2_ALIGNED16(u32 tag[4]);
					SIF0read((u32*)&tag[0], 4); // Tag
                    SIF_LOG(" EE SIF read tag: %x %x %x %x\n", tag[0], tag[1], tag[2], tag[3]);

					sif0dma->qwc = (u16)tag[0];
					sif0dma->madr = tag[1];
					sif0dma->chcr = (sif0dma->chcr & 0xffff) | (tag[0] & 0xffff0000);

#ifdef SIF_LOG
					SIF_LOG(" EE SIF dest chain tag madr:%08X qwc:%04X id:%X irq:%d(%08X_%08X)\n", sif0dma->madr, sif0dma->qwc, (tag[0]>>28)&3, (tag[0]>>31)&1, tag[1], tag[0]);
#endif
					if ((psHu32(DMAC_CTRL) & 0x30) != 0 && ((tag[0]>>28)&3) == 0)
                        psHu32(DMAC_STADR) = sif0dma->madr + (sif0dma->qwc * 16);
					notDone = 1;
					sif0.chain = 1;
					if(tag[0] & 0x40000000)
						sif0.end = 1;
					
				}
			}
		}
	}while(notDone);

	FreezeMMXRegs(0);
	FreezeXMMRegs(0)
}

void SIF1Dma()
{
	int id;
	u32 *ptag;
	int notDone;
	//int cycles = 0, psxCycles = 0;

	do
	{
		notDone = 0;

		if(sif1dma->chcr & 0x100) // If EE SIF1 is enabled
		{
			if ((psHu32(DMAC_CTRL) & 0xC0) == 0xC0) { // STS == fromSIF1
				SysPrintf("SIF1 stall control\n");
			}

			if(sif1dma->qwc == 0) // If there's no more to transfer
			{
				if ((sif1dma->chcr & 0xc) == 0 || sif1.end) // If NORMAL mode or end of CHAIN then stop DMA
				{
					// Stop & signal interrupts on EE
					sif1dma->chcr &= ~0x100;
					hwDmacIrq(6);
					sif1.chain = 0;
					sif1.end = 0;
				}
				else // Chain mode
				{
					// Process DMA tag at sif1dma->tadr
					notDone = 1;
					_dmaGetAddr(sif1dma, ptag, sif1dma->tadr, 6);
					sif1dma->chcr = ( sif1dma->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 ); // Copy the tag
					sif1dma->qwc = (u16)ptag[0];

					if (sif1dma->chcr & 0x40) {
						SysPrintf("SIF1 TTE\n");
						SIF1write(ptag+2, 2);
					}

					sif1.chain = 1;
					id        = (ptag[0] >> 28) & 0x7;

					switch(id)
					{
						case 0: // refe
#ifdef SIF_LOG
							SIF_LOG("   REFE %08X\n", ptag[1]);
#endif
							sif1.end = 1;
							sif1dma->madr = ptag[1];
							sif1dma->tadr += 16;
							break;

						case 1: // cnt
#ifdef SIF_LOG
							SIF_LOG("   CNT\n");
#endif
							sif1dma->madr = sif1dma->tadr + 16;
							sif1dma->tadr = sif1dma->madr + (sif1dma->qwc << 4);
							break;

						case 2: // next
#ifdef SIF_LOG
							SIF_LOG("   NEXT %08X\n", ptag[1]);
#endif
							sif1dma->madr = sif1dma->tadr + 16;
							sif1dma->tadr = ptag[1];
							break;

						case 3: // ref
						case 4: // refs
#ifdef SIF_LOG
							SIF_LOG("   REF %08X\n", ptag[1]);
#endif
							sif1dma->madr = ptag[1];
							sif1dma->tadr += 16;
							break;

						case 7: // end
#ifdef SIF_LOG
							SIF_LOG("   END\n");
#endif
							sif1.end = 1;
							sif1dma->madr = sif1dma->tadr + 16;
							sif1dma->tadr = sif1dma->madr + (sif1dma->qwc << 4);
							break;
							
						default:
							SysPrintf("Bad addr1 source chain\n");
					}
				}
			}
			else // There's some data ready to transfer into the fifo..
			{
				int qwTransfer = sif1dma->qwc;
				u32 *data;

				notDone = 1;
				
				_dmaGetAddr(sif1dma, data, sif1dma->madr, 6);

				if(qwTransfer > (FIFO_SIF1_W-sif1.fifoSize)/4) // Copy part of sif1dma into FIFO
					qwTransfer = (FIFO_SIF1_W-sif1.fifoSize)/4;

				SIF1write(data, qwTransfer << 2);
				
				sif1dma->madr += qwTransfer << 4;
				//cycles += qwTransfer * BIAS;
				sif1dma->qwc -= qwTransfer;
			}
		}

		if(HW_DMA10_CHCR & 0x01000000 ) // If IOP SIF enabled and there's something to transfer
		{
			int size = sif1.counter; 
			
			if(size > 0) // If we're reading something continue to do so
			{
				if(sif1.fifoSize > 0)
				{
					int readSize = size;

					if(readSize > sif1.fifoSize)
						readSize = sif1.fifoSize;

#ifdef SIF_LOG
					SIF_LOG(" IOP SIF doing transfer %04X to %08X\n", readSize, HW_DMA10_MADR);
#endif

					SIF1read((u32*)PSXM(HW_DMA10_MADR), readSize);
					psxCpu->Clear(HW_DMA10_MADR, readSize);
					//psxCycles += readSize / 4;
					sif1.counter = size-readSize;
					HW_DMA10_MADR += readSize << 2;
					notDone = 1;
				}
			}

			if(size <= 0 || sif1.fifoSize == 0)
			{
				if(sif1.tagMode & 0x80) // Stop on tag IRQ
				{
					// Tag interrupt
#ifdef SIF_LOG
					SIF_LOG(" IOP SIF interrupt\n");
#endif
					HW_DMA10_CHCR &= ~0x01000000; //reset TR flag
					psxDmaInterrupt2(3);
					//hwIntcIrq(INTC_SBUS);
					sif1.tagMode = 0;
					notDone = 0;
				}
				else if(sif1.tagMode & 0x40) // Stop on tag END
				{
					// End tag.
#ifdef SIF_LOG
					SIF_LOG(" IOP SIF end\n");
#endif
					HW_DMA10_CHCR &= ~0x01000000; //reset TR flag
					psxDmaInterrupt2(3);
					//hwIntcIrq(INTC_SBUS);
					sif1.tagMode = 0;
					notDone = 0;
				}
				else if(sif1.fifoSize >= 4) // Read a tag
				{
					struct sifData d;

					SIF1read((u32*)&d, 4);

#ifdef SIF_LOG
					SIF_LOG(" IOP SIF dest chain tag madr:%08X wc:%04X id:%X irq:%d\n", d.data & 0xffffff, d.words, (d.data>>28)&7, (d.data>>31)&1);
#endif
					HW_DMA10_MADR = d.data & 0xffffff;
					sif1.counter = d.words;
					sif1.tagMode = (d.data >> 24) & 0xFF;
					notDone = 1;
				}
			}
		}
	}while(notDone);

	FreezeMMXRegs(0);
	FreezeXMMRegs(0);
}

int  sif0Interrupt() {
	/*if (psxHu32(0x1070) & 8) {
		PSX_INT(9, 0x800);
		return 0;
	}*/

	psxDmaInterrupt2(2);
	//hwIntcIrq(INTC_SBUS);
	return 1;
}

int  sif1Interrupt() {
	/*if (psxHu32(0x1070) & 8) {
		PSX_INT(10, 0x800);
		return 0;
	}*/

	HW_DMA10_CHCR &= ~0x01000000; //reset TR flag
	psxDmaInterrupt2(3);
	//hwIntcIrq(INTC_SBUS);
	return 1;
}

int  EEsif0Interrupt() {
	/*if (psHu32(DMAC_STAT) & (1<<5)) {
		INT(5, 0x800);
		return 0;
	}*/
	sif0dma->chcr &= ~0x100;
	hwDmacIrq(5);

	return 1;
}

int  EEsif1Interrupt() {
	/*if (psHu32(DMAC_STAT) & (1<<6)) {
		INT(6, 0x800);
		return 0;
	}*/
	hwDmacIrq(6);

	return 1;
}

void dmaSIF0() {

#ifdef SIF_LOG
	SIF_LOG("EE: dmaSIF0 chcr = %lx, madr = %lx, qwc  = %lx, tadr = %lx\n",
			sif0dma->chcr, sif0dma->madr, sif0dma->qwc, sif0dma->tadr);
#endif

	if (sif0.fifoReadPos != sif0.fifoWritePos) {
		SysPrintf("warning, sif0.fifoReadPos != sif0.fifoWritePos\n");
	}
//    if(sif0dma->qwc > 0 & (sif0dma->chcr & 0x4) == 0x4) {
//        sif0dma->chcr &= ~4; //Halflife sets a QWC amount in chain mode, no tadr set.
//        SysPrintf("yo\n");
//    }

	psHu32(0x1000F240) |= 0x2000;
	if(sif0dma->chcr & 0x100 && HW_DMA9_CHCR & 0x01000000) {
		hwIntcIrq(INTC_SBUS);
		SIF0Dma();
		psHu32(0x1000F240) &= ~0x20;
		psHu32(0x1000F240) &= ~0x2000;
	}
}

void dmaSIF1() {

#ifdef SIF_LOG
	SIF_LOG("EE: dmaSIF1 chcr = %lx, madr = %lx, qwc  = %lx, tadr = %lx\n",
			sif1dma->chcr, sif1dma->madr, sif1dma->qwc, sif1dma->tadr);
#endif

	if (sif1.fifoReadPos != sif1.fifoWritePos) {
		SysPrintf("warning, sif1.fifoReadPos != sif1.fifoWritePos\n");
	}

//    if(sif1dma->qwc > 0 & (sif1dma->chcr & 0x4) == 0x4) {
//        sif1dma->chcr &= ~4; //Halflife sets a QWC amount in chain mode, no tadr set.
//        SysPrintf("yo2\n");
//    }

	psHu32(0x1000F240) |= 0x4000;
	if(sif1dma->chcr & 0x100 && HW_DMA10_CHCR & 0x01000000) {
		SIF1Dma();
		psHu32(0x1000F240) &= ~0x40;
		psHu32(0x1000F240) &= ~0x100;
		psHu32(0x1000F240) &= ~0x4000;
	}
}

void dmaSIF2() {

#ifdef SIF_LOG
	SIF_LOG("dmaSIF2 chcr = %lx, madr = %lx, qwc  = %lx\n",
			sif2dma->chcr, sif2dma->madr, sif2dma->qwc);
#endif

    sif2dma->chcr&= ~0x100;
	hwDmacIrq(7);
	SysPrintf("*PCSX2*: dmaSIF2\n");
}


int  sifFreeze(gzFile f, int Mode) {
	gzfreeze(&sif0, sizeof(sif0));
	gzfreeze(&sif1, sizeof(sif1));
	return 0;
}
