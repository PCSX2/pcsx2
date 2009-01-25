/*  ZeroSPU2
 *  Copyright (C) 2006-2007 zerofrog
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

#include "zerospu2.h"

#include <assert.h>
#include <stdlib.h>

#include "SoundTouch/SoundTouch.h"
#include "SoundTouch/WavFile.h"

void CALLBACK SPU2readDMA4Mem(u16 *pMem, int size)
{
	u32 spuaddr = C0_SPUADDR();
	int i;

	SPU2_LOG("SPU2 readDMA4Mem size %x, addr: %x\n", size, pMem);

	for (i=0;i<size;i++)
	{
		*pMem++ = *(u16*)(spu2mem+spuaddr);
		if ((spu2Rs16(REG_C0_CTRL) & 0x40) && C0_IRQA() == spuaddr)
		{
			C0_SPUADDR_SET(spuaddr);
			IRQINFO |= 4;
			SPU2_LOG("SPU2readDMA4Mem:interrupt\n");
			irqCallbackSPU2();
		}

		spuaddr++;		   // inc spu addr
		if (spuaddr>0x0fffff) spuaddr=0; // wrap at 2Mb
	}

	spuaddr+=19; //Transfer Local To Host TSAH/L + Data Size + 20 (already +1'd)
	C0_SPUADDR_SET(spuaddr);

	// got from J.F. and Kanodin... is it needed?
	spu2Ru16(REG_C0_SPUSTAT) &=~0x80;									 // DMA complete
	SPUStartCycle[0] = SPUCycles;
	SPUTargetCycle[0] = size;
	interrupt |= (1<<1);
}

void CALLBACK SPU2readDMA7Mem(u16* pMem, int size)
{
	u32 spuaddr = C1_SPUADDR();
	int i;

	SPU2_LOG("SPU2 readDMA7Mem size %x, addr: %x\n", size, pMem);

	for (i=0;i<size;i++)
	{
		*pMem++ = *(u16*)(spu2mem+spuaddr);
		if ((spu2Rs16(REG_C1_CTRL)&0x40) && (C1_IRQA() == spuaddr))
		{
			C1_SPUADDR_SET(spuaddr);
			IRQINFO |= 8;
			SPU2_LOG("SPU2readDMA7Mem:interrupt\n");
			irqCallbackSPU2();
		}
		spuaddr++;							// inc spu addr
		if (spuaddr>0x0fffff) // wrap at 2Mb
			spuaddr=0;			 // wrap
	}

	spuaddr+=19; //Transfer Local To Host TSAH/L + Data Size + 20 (already +1'd)
	C1_SPUADDR_SET(spuaddr);

	// got from J.F. and Kanodin... is it needed?
	spu2Ru16(REG_C1_SPUSTAT)&=~0x80;									 // DMA complete
	SPUStartCycle[1] = SPUCycles;
	SPUTargetCycle[1] = size;
	interrupt |= (1<<2);
}

// WRITE

// AutoDMA's are used to transfer to the DIRECT INPUT area of the spu2 memory
// Left and Right channels are always interleaved together in the transfer so 
// the AutoDMA's deinterleaves them and transfers them. An interrupt is
// generated when half of the buffer (256 short-words for left and 256 
// short-words for right ) has been transferred. Another interrupt occurs at 
// the end of the transfer.
int ADMAS4Write()
{
	u32 spuaddr;
	ADMA *Adma = &Adma4;
	
	if (interrupt & 0x2)
	{
		printf("4 returning for interrupt\n");
		return 0;
	}
	if (Adma->AmountLeft <= 0)
	{
		printf("4 amount left is 0\n");
		return 1;
	}

	assert( Adma->AmountLeft >= 512 );
	spuaddr = C0_SPUADDR();
	
	// SPU2 Deinterleaves the Left and Right Channels
	memcpy((short*)(spu2mem + spuaddr + 0x2000),(short*)Adma->MemAddr,512);
	Adma->MemAddr += 256;
	memcpy((short*)(spu2mem + spuaddr + 0x2200),(short*)Adma->MemAddr,512);
	Adma->MemAddr += 256;
	
	if ((spu2Ru16(REG_C0_CTRL)&0x40) && ((spuaddr + 0x2400) <= C0_IRQA() &&  (spuaddr + 0x2400 + 256) >= C0_IRQA()))
	{
		IRQINFO |= 4;
		printf("ADMA 4 Mem access:interrupt\n");
		irqCallbackSPU2();
	}
	
	if ((spu2Ru16(REG_C0_CTRL)&0x40) && ((spuaddr + 0x2600) <= C0_IRQA() &&  (spuaddr + 0x2600 + 256) >= C0_IRQA()))
	{
		IRQINFO |= 4;
		printf("ADMA 4 Mem access:interrupt\n");
		irqCallbackSPU2();
	}

	spuaddr = (spuaddr + 256) & 511;
	C0_SPUADDR_SET(spuaddr);
	
	Adma->AmountLeft-=512;

	if (Adma->AmountLeft > 0) 
		return 0;
	else 
		return 1;
}

int ADMAS7Write()
{
	u32 spuaddr;
	ADMA *Adma = &Adma7;
	
	if (interrupt & 0x4)
	{
		printf("7 returning for interrupt\n");
		return 0;
	}
	if (Adma->AmountLeft <= 0)
	{
		printf("7 amount left is 0\n");
		return 1;
	}

	assert( Adma->AmountLeft >= 512 );
	spuaddr = C1_SPUADDR();
	
	// SPU2 Deinterleaves the Left and Right Channels
	memcpy((short*)(spu2mem + spuaddr + 0x2400),(short*)Adma->MemAddr,512);
	Adma->MemAddr += 256;
	
	memcpy((short*)(spu2mem + spuaddr + 0x2600),(short*)Adma->MemAddr,512);
	Adma->MemAddr += 256;
	
	if ((spu2Ru16(REG_C1_CTRL)&0x40) && ((spuaddr + 0x2400) <= C1_IRQA() &&  (spuaddr + 0x2400 + 256) >= C1_IRQA()))
	{
		IRQINFO |= 8;
		printf("ADMA 7 Mem access:interrupt\n");
		irqCallbackSPU2();
	}
	
	if ((spu2Ru16(REG_C1_CTRL)&0x40) && ((spuaddr + 0x2600) <= C1_IRQA() &&  (spuaddr + 0x2600 + 256) >= C1_IRQA()))
	{
		IRQINFO |= 8;
		printf("ADMA 7 Mem access:interrupt\n");
		irqCallbackSPU2();
	}
	
	spuaddr = (spuaddr + 256) & 511;
	C1_SPUADDR_SET(spuaddr);
	
	Adma->AmountLeft-=512;
   
	assert( Adma->AmountLeft >= 0 );

	if (Adma->AmountLeft > 0) 
		return 0;
	else 
		return 1;
}

void CALLBACK SPU2writeDMA4Mem(u16* pMem, int size)
{
	u32 spuaddr;
	ADMA *Adma = &Adma4;

	SPU2_LOG("SPU2 writeDMA4Mem size %x, addr: %x(spu2:%x), ctrl: %x, adma: %x\n", size, pMem, C0_SPUADDR(), spu2Ru16(REG_C0_CTRL), spu2Ru16(REG_C0_ADMAS));

	if ((spu2Ru16(REG_C0_ADMAS) & 0x1) && (spu2Ru16(REG_C0_CTRL) & 0x30) == 0 && size)
	{
		// if still active, don't destroy adma4
		if ( !Adma->Enabled )
			Adma->Index = 0;

		Adma->MemAddr = pMem;
		Adma->AmountLeft = size;
		SPUTargetCycle[0] = size;
		spu2Ru16(REG_C0_SPUSTAT)&=~0x80;
		if (!Adma->Enabled || Adma->Index > 384) 
		{
			C0_SPUADDR_SET(0);
			if (ADMAS4Write())
			{
				SPUStartCycle[0] = SPUCycles;
				interrupt |= (1<<1);
			}
		}
		Adma->Enabled = 1;
		return;
	}

	spuaddr = C0_SPUADDR();
	memcpy((unsigned char*)(spu2mem + spuaddr),(unsigned char*)pMem,size<<1);
	spuaddr += size;
	C0_SPUADDR_SET(spuaddr);
	
	if ((spu2Ru16(REG_C0_CTRL)&0x40) && (spuaddr < C0_IRQA() && C0_IRQA() <= spuaddr+0x20))
	{
		IRQINFO |= 4;
		SPU2_LOG("SPU2writeDMA4Mem:interrupt\n");
		irqCallbackSPU2();
	}
	
	if (spuaddr>0xFFFFE)
		spuaddr = 0x2800;
	C0_SPUADDR_SET(spuaddr);

	MemAddr[0] += size<<1;
	spu2Ru16(REG_C0_SPUSTAT)&=~0x80;
	SPUStartCycle[0] = SPUCycles;
	SPUTargetCycle[0] = size;
	interrupt |= (1<<1);
}

void CALLBACK SPU2writeDMA7Mem(u16* pMem, int size)
{
	u32 spuaddr;
	ADMA *Adma = &Adma7;

	SPU2_LOG("SPU2 writeDMA7Mem size %x, addr: %x(spu2:%x), ctrl: %x, adma: %x\n", size, pMem, C1_SPUADDR(), spu2Ru16(REG_C1_CTRL), spu2Ru16(REG_C1_ADMAS));

	if ((spu2Ru16(REG_C1_ADMAS) & 0x2) && (spu2Ru16(REG_C1_CTRL) & 0x30) == 0 && size)
	{
		if (!Adma->Enabled ) Adma->Index = 0;
	
		Adma->MemAddr = pMem;
		Adma->AmountLeft = size;
		SPUTargetCycle[1] = size;
		spu2Ru16(REG_C1_SPUSTAT)&=~0x80;
		if (!Adma->Enabled || Adma->Index > 384) 
		{
			C1_SPUADDR_SET(0);
			if (ADMAS7Write())
			{
				SPUStartCycle[1] = SPUCycles;
				interrupt |= (1<<2);
			}
		}
		Adma->Enabled = 1;

		return;
	}

#ifdef _DEBUG
	if (conf.Log && conf.options & OPTION_RECORDING)
		LogPacketSound(pMem, 0x8000);
#endif

	spuaddr = C1_SPUADDR();
	memcpy((unsigned char*)(spu2mem + spuaddr),(unsigned char*)pMem,size<<1);
	spuaddr += size;
	C1_SPUADDR_SET(spuaddr);
	
	if ((spu2Ru16(REG_C1_CTRL)&0x40) && (spuaddr < C1_IRQA() && C1_IRQA() <= spuaddr+0x20))
	{
		IRQINFO |= 8;
		SPU2_LOG("SPU2writeDMA7Mem:interrupt\n");
		irqCallbackSPU2();
	}
	
	if (spuaddr>0xFFFFE) spuaddr = 0x2800;
	C1_SPUADDR_SET(spuaddr);

	MemAddr[1] += size<<1;
	spu2Ru16(REG_C1_SPUSTAT)&=~0x80;
	SPUStartCycle[1] = SPUCycles;
	SPUTargetCycle[1] = size;
	interrupt |= (1<<2);
}

void CALLBACK SPU2interruptDMA4()
{
	SPU2_LOG("SPU2 interruptDMA4\n");
	spu2Rs16(REG_C0_CTRL)&=~0x30;
	spu2Ru16(REG_C0_SPUSTAT)|=0x80;
}

void CALLBACK SPU2interruptDMA7()
{
	SPU2_LOG("SPU2 interruptDMA7\n");
	spu2Rs16(REG_C1_CTRL)&=~0x30;
	spu2Ru16(REG_C1_SPUSTAT)|=0x80;
}