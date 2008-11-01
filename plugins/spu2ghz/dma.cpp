//GiGaHeRz's SPU2 Driver
//Copyright (c) 2003-2008, David Quintana <gigaherz@gmail.com>
//
//This library is free software; you can redistribute it and/or
//modify it under the terms of the GNU Lesser General Public
//License as published by the Free Software Foundation; either
//version 2.1 of the License, or (at your option) any later version.
//
//This library is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//Lesser General Public License for more details.
//
//You should have received a copy of the GNU Lesser General Public
//License along with this library; if not, write to the Free Software
//Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
#include "spu2.h"

extern u8 callirq;

FILE *DMA4LogFile=0;
FILE *DMA7LogFile=0;
FILE *ADMA4LogFile=0;
FILE *ADMA7LogFile=0;
FILE *ADMAOutLogFile=0;

FILE *REGWRTLogFile[2]={0,0};

int packcount=0;

u16* MBASE[2] = {0,0};

u16* DMABaseAddr;

void DMALogOpen() {
	if(!DMALog()) return;
	DMA4LogFile=fopen(DMA4LogFileName,"wb");
	DMA7LogFile=fopen(DMA7LogFileName,"wb");
	ADMA4LogFile=fopen("logs/adma4.raw","wb");
	ADMA7LogFile=fopen("logs/adma7.raw","wb");
	ADMAOutLogFile=fopen("logs/admaOut.raw","wb");
	//REGWRTLogFile[0]=fopen("logs/RegWrite0.raw","wb");
	//REGWRTLogFile[1]=fopen("logs/RegWrite1.raw","wb");
}
void DMA4LogWrite(void *lpData, u32 ulSize) {
	if(!DMALog()) return;
	if (!DMA4LogFile) return;
	fwrite(lpData,ulSize,1,DMA4LogFile);
}

void DMA7LogWrite(void *lpData, u32 ulSize) {
	if(!DMALog()) return;
	if (!DMA7LogFile) return;
	fwrite(lpData,ulSize,1,DMA7LogFile);
}

void ADMA4LogWrite(void *lpData, u32 ulSize) {
	if(!DMALog()) return;
	if (!ADMA4LogFile) return;
	fwrite(lpData,ulSize,1,ADMA4LogFile);
}
void ADMA7LogWrite(void *lpData, u32 ulSize) {
	if(!DMALog()) return;
	if (!ADMA7LogFile) return;
	fwrite(lpData,ulSize,1,ADMA7LogFile);
}
void ADMAOutLogWrite(void *lpData, u32 ulSize) {
	if(!DMALog()) return;
	if (!ADMAOutLogFile) return;
	fwrite(lpData,ulSize,1,ADMAOutLogFile);
}

void RegWriteLog(u32 core,u16 value)
{
	if(!DMALog()) return;
	if (!REGWRTLogFile[core]) return;
	fwrite(&value,2,1,REGWRTLogFile[core]);
}

void DMALogClose() {
	if(!DMALog()) return;
	if (DMA4LogFile) fclose(DMA4LogFile);
	if (DMA7LogFile) fclose(DMA7LogFile);
	if (REGWRTLogFile[0]) fclose(REGWRTLogFile[0]);
	if (REGWRTLogFile[1]) fclose(REGWRTLogFile[1]);
	if (ADMA4LogFile) fclose(ADMA4LogFile);
	if (ADMA7LogFile) fclose(ADMA7LogFile);
	if (ADMAOutLogFile) fclose(ADMAOutLogFile);
}


__forceinline u16 DmaRead(u32 core)
{
	const u16 ret = (u16)spu2M_Read(Cores[core].TDA);
	Cores[core].TDA++;
	Cores[core].TDA&=0xfffff;
	return ret;
}

__forceinline void DmaWrite(u32 core, u16 value)
{
	spu2M_Write( Cores[core].TSA, value );
	Cores[core].TSA++;
	Cores[core].TSA&=0xfffff;
}

void AutoDMAReadBuffer(int core, int mode) //mode: 0= split stereo; 1 = do not split stereo
{
	int spos=((Cores[core].InputPos+0xff)&0x100); //starting position of the free buffer

	if(core==0)
		ADMA4LogWrite(Cores[core].DMAPtr+Cores[core].InputDataProgress,0x400);
	else
		ADMA7LogWrite(Cores[core].DMAPtr+Cores[core].InputDataProgress,0x400);

	if(mode)
	{
		//hacky :p

		memcpy((Cores[core].ADMATempBuffer+(spos<<1)),Cores[core].DMAPtr+Cores[core].InputDataProgress,0x400);
		Cores[core].MADR+=0x400;
		Cores[core].InputDataLeft-=0x200;
		Cores[core].InputDataProgress+=0x200;
	}
	else
	{
		memcpy((Cores[core].ADMATempBuffer+spos),Cores[core].DMAPtr+Cores[core].InputDataProgress,0x200);
		//memcpy((spu2mem+0x2000+(core<<10)+spos),Cores[core].DMAPtr+Cores[core].InputDataProgress,0x200);
		Cores[core].MADR+=0x200;
		Cores[core].InputDataLeft-=0x100;
		Cores[core].InputDataProgress+=0x100;

		memcpy((Cores[core].ADMATempBuffer+spos+0x200),Cores[core].DMAPtr+Cores[core].InputDataProgress,0x200);
		//memcpy((spu2mem+0x2200+(core<<10)+spos),Cores[core].DMAPtr+Cores[core].InputDataProgress,0x200);
		Cores[core].MADR+=0x200;
		Cores[core].InputDataLeft-=0x100;
		Cores[core].InputDataProgress+=0x100;
	}
	// See ReadInput at mixer.cpp for explanation on the commented out lines
	//
}

void StartADMAWrite(int core,u16 *pMem, u32 sz)
{
	int size=(sz)&(~511);
	if(MsgAutoDMA()) ConLog(" * SPU2: DMA%c AutoDMA Transfer of %d bytes to %x (%02x %x %04x).\n",(core==0)?'4':'7',size<<1,Cores[core].TSA,Cores[core].DMABits,Cores[core].AutoDMACtrl,(~Cores[core].Regs.ATTR)&0x7fff);

	Cores[core].InputDataProgress=0;
	if((Cores[core].AutoDMACtrl&(core+1))==0)
	{
		Cores[core].TSA=0x2000+(core<<10);
		Cores[core].DMAICounter=size;
	}
	else if(size>=512)
	{
		Cores[core].InputDataLeft=size;
		if(Cores[core].AdmaInProgress==0)
		{
#ifdef PCM24_S1_INTERLEAVE
			if((core==1)&&((PlayMode&8)==8))
			{
				AutoDMAReadBuffer(core,1);
			}
			else
			{
				AutoDMAReadBuffer(core,0);
			}
#else
			if(((PlayMode&4)==4)&&(core==0))
				Cores[0].InputPos=0;

			AutoDMAReadBuffer(core,0);
#endif

			if(size==512)
				Cores[core].DMAICounter=size;
		}

		Cores[core].AdmaInProgress=1;
	}
	else
	{
		Cores[core].InputDataLeft=0;
		Cores[core].DMAICounter=1;
	}
	Cores[core].TADR=Cores[core].MADR+(size<<1);
}

void DoDMAWrite(int core,u16 *pMem,u32 size)
{
	u32 i;

	{
		// Perform an alignment check.
		// Not really important.  Everythign should work regardless,
		// but it could be indicative of an emulation foopah elsewhere.

		uptr pa = ((uptr)pMem)&7;
		uptr pm = Cores[core].TSA&0x7;

		if(pa || pm)
		{
			fprintf(stderr, "* SPU2 : Missaligned addr in DMA write!\n");
		}
	}


	if(core==0)
		DMA4LogWrite(pMem,size<<1);
	else
		DMA7LogWrite(pMem,size<<1);

	if(MsgDMA()) ConLog(" * SPU2: DMA%c Transfer of %d bytes to %x (%02x %x %04x).\n",(core==0)?'4':'7',size<<1,Cores[core].TSA,Cores[core].DMABits,Cores[core].AutoDMACtrl,(~Cores[core].Regs.ATTR)&0x7fff);

	// split the DMA copy into two chunks if needed.

	// Instead of checking the adpcm cache for every word, we check for every block.
	// That way we can use the optimized fast write instruction to commit the memory.

	Cores[core].TDA = Cores[core].TSA & 0xfffff;

	u32 buff1end = Cores[core].TDA + size;
	s32 buff2end = buff1end - 0xfffff;
	if( buff2end > 0 )
		buff1end = 0xfffff;

	{
		u32 nexta = Cores[core].TDA >> 3;		// next address in 8 word blocks
		const u32 leftsidebit = nexta & 31;
		u32 rightsidebit;		// assigned later
		nexta >>= 5;

		// Left side remainder:
		// this produces a bitmask of the left side remainder of the cache flags:
		pcm_cache_flags[nexta] &= (1ul << leftsidebit)-1;

		// middle run!
		// Traverse from start to finish in 8*32 word blocks,
		// and clear all the the pcm cache flags for each block.

		for(; Cores[core].TDA<buff1end; ++Cores[core].TDA, ++pMem)
			*GetMemPtr( Cores[core].TDA ) = *pMem;

		buff1end >>= (3+5);		// 8 words per block, 32 blocks per int.
		memset( &pcm_cache_flags[nexta], 0, sizeof( u32 ) * (buff1end-nexta) );

		if( buff2end > 0 )
		{
			// second branch needs cleared:
			// It starts at the beginning of memory and moves forward to buff2end

			const u32 endpt2 = buff2end >> (3+5);		// 8 words per block, 32 blocks per int.
			memset( pcm_cache_flags, 0, sizeof( u32 ) * endpt2 );

			for(Cores[core].TDA=0; Cores[core].TDA<(u32)buff2end; ++Cores[core].TDA, ++pMem)
				*GetMemPtr( Cores[core].TDA ) = *pMem;

			rightsidebit = ( buff2end >> 3 );
			nexta = endpt2;
		}
		else
		{
			rightsidebit = (Cores[core].TDA >> 3);
			nexta = buff1end;
		}

		// clear the right-side remainder:
		pcm_cache_flags[nexta] &= ~((1ul << (32-(rightsidebit&31)))-1);
	}

	i=Cores[core].TSA;
	Cores[core].TDA=Cores[core].TSA+size;
	if((Cores[core].TDA>0xFFFFF)||((Cores[core].TDA>=Cores[core].IRQA)&&(i<=Cores[core].IRQA))) {
		if(Cores[core].IRQEnable)
		{
			Spdif.Info=4<<core;
			SetIrqCall();
		}
	}
	Cores[core].TSA=Cores[core].TDA&0xFFFF0;
	Cores[core].DMAICounter=size;
	Cores[core].TADR=Cores[core].MADR+(size<<1);
}

void SPU2readDMA(int core, u16* pMem, u32 size) 
{
	if(hasPtr) TimeUpdate(*cPtr,1);

	u32 i;
	Cores[core].TSA&=~7;
	Cores[core].TDA=Cores[core].TSA;
	for (i=0;i<size;i++)
		pMem[i]=DmaRead(core);
	i=Cores[core].TSA;
	Cores[core].TDA=Cores[core].TSA+size+0x1f;
	Cores[core].TSA=Cores[core].TDA&0xFFFFF;
	if((Cores[core].TDA>0xFFFFF)||((Cores[core].TSA<=Cores[core].IRQA)&&(i>=Cores[core].IRQA))) {
		if(Cores[core].IRQEnable)
		{
			Spdif.Info=4<<core;
			SetIrqCall();
		}
	}
	Cores[core].DMAICounter=size;
	Cores[core].Regs.STATX &= ~0x80;
	//Cores[core].Regs.ATTR |= 0x30;
	Cores[core].TADR=Cores[core].MADR+(size<<1);

}

void SPU2writeDMA(int core, u16* pMem, u32 size) 
{
	if(hasPtr) TimeUpdate(*cPtr,1);

	Cores[core].DMAPtr=pMem;

	if(size<2) {
		//if(dma7callback) dma7callback();
		Cores[core].Regs.STATX &= ~0x80;
		//Cores[core].Regs.ATTR |= 0x30;
		Cores[core].DMAICounter=1;

		return;
	}

	#ifndef PUBLIC
	DebugCores[core].lastsize=size;
	#endif
	Cores[core].TSA&=~7;

	bool adma_enable = ((Cores[core].AutoDMACtrl&(core+1))==(core+1));

	if(adma_enable)
	{
		Cores[core].TSA&=0x1fff;
		StartADMAWrite(core,pMem,size);
	}
	else
	{
		DoDMAWrite(core,pMem,size);
	}
	Cores[core].Regs.STATX &= ~0x80;
	//Cores[core].Regs.ATTR |= 0x30;
}

u32 CALLBACK SPU2ReadMemAddr(int core)
{
	return Cores[core].MADR;
}
void CALLBACK SPU2WriteMemAddr(int core,u32 value)
{
	Cores[core].MADR=value;
}

void CALLBACK SPU2setDMABaseAddr(uptr baseaddr)
{
   DMABaseAddr = (u16*)baseaddr;
}

void CALLBACK SPU2readDMA4Mem(u16 *pMem, u32 size) { //size now in 16bit units
	FileLog("[%10d] SPU2 readDMA4Mem size %x\n",Cycles, size<<1);
	SPU2readDMA(0,pMem,size);
}

void CALLBACK SPU2writeDMA4Mem(u16* pMem, u32 size) { //size now in 16bit units
	FileLog("[%10d] SPU2 writeDMA4Mem size %x at address %x\n",Cycles, size<<1, Cores[0].TSA);
#ifdef S2R_ENABLE
	if(!replay_mode)
		s2r_writedma4(Cycles,pMem,size);
#endif
	SPU2writeDMA(0,pMem,size);
}

void CALLBACK SPU2interruptDMA4() {
	FileLog("[%10d] SPU2 interruptDMA4\n",Cycles);
	Cores[0].Regs.STATX |= 0x80;
	//Cores[0].Regs.ATTR &= ~0x30;
}

void CALLBACK SPU2readDMA7Mem(u16* pMem, u32 size) {
	FileLog("[%10d] SPU2 readDMA7Mem size %x\n",Cycles, size<<1);

	SPU2readDMA(1,pMem,size);
}

void CALLBACK SPU2writeDMA7Mem(u16* pMem, u32 size) {
	FileLog("[%10d] SPU2 writeDMA7Mem size %x at address %x\n",Cycles, size<<1, Cores[1].TSA);
#ifdef S2R_ENABLE
	if(!replay_mode)
		s2r_writedma7(Cycles,pMem,size);
#endif
	SPU2writeDMA(1,pMem,size);
}

void CALLBACK SPU2interruptDMA7() {
	FileLog("[%10d] SPU2 interruptDMA7\n",Cycles);
	Cores[1].Regs.STATX |= 0x80;
	//Cores[1].Regs.ATTR &= ~0x30;
}

