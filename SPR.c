/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2003  Pcsx2 Team
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
#include "Common.h"
#include "SPR.h"

#include "iR5900.h"

#define spr0 ((DMACh*)&PS2MEM_HW[0xD000])
#define spr1 ((DMACh*)&PS2MEM_HW[0xD400])

void sprInit() {
}

//__inline static void SPR0transfer(u32 *data, int size) {
///*	while (size > 0) {
//#ifdef SPR_LOG
//		SPR_LOG("SPR1transfer: %x\n", *data);
//#endif
//		data++; size--;
//	}*/
//	size <<= 2;
//	if ((psHu32(DMAC_CTRL) & 0xC) == 0xC || // GIF MFIFO
//		(psHu32(DMAC_CTRL) & 0xC) == 0x8) { // VIF1 MFIFO
//		hwMFIFOWrite(spr0->madr, (u8*)&PS2MEM_SCRATCH[spr0->sadr & 0x3fff], size);
//	} else {
//		u32 * p = (u32*)&PS2MEM_SCRATCH[spr0->sadr & 0x3fff];
//		//WriteCodeSSE2(p,data,size >> 4);
//		memcpy_amd((u8*)data, &PS2MEM_SCRATCH[spr0->sadr & 0x3fff], size);
//	}
//	spr0->sadr+= size;
//}

static void TestClearVUs(u32 madr, u32 size)
{
	if( madr >= 0x11000000 ) {
		if( madr < 0x11004000 ) {
#ifdef _DEBUG
			SysPrintf("scratch pad clearing vu0\n");
#endif
			Cpu->ClearVU0(madr&0xfff, size);
		}
		else if( madr >= 0x11008000 && madr < 0x1100c000 ) {
#ifdef _DEBUG
			SysPrintf("scratch pad clearing vu1\n");
#endif
			Cpu->ClearVU1(madr&0x3fff, size);
		}
	}
}

int  _SPR0chain() {
	u32 qwc = spr0->qwc;
	u32 *pMem;

	if (qwc == 0) return 0;

	pMem = (u32*)dmaGetAddr(spr0->madr);
	if (pMem == NULL) return -1;

	//SPR0transfer(pMem, qwc << 2);
	qwc <<= 4;
	if ((psHu32(DMAC_CTRL) & 0xC) == 0xC || // GIF MFIFO
		(psHu32(DMAC_CTRL) & 0xC) == 0x8) { // VIF1 MFIFO
		hwMFIFOWrite(spr0->madr, (u8*)&PS2MEM_SCRATCH[spr0->sadr & 0x3fff], qwc);
		spr0->madr += (spr0->qwc * 16);
		spr0->madr = psHu32(DMAC_RBOR) + (spr0->madr & psHu32(DMAC_RBSR));
	} else {
		memcpy_amd((u8*)pMem, &PS2MEM_SCRATCH[spr0->sadr & 0x3fff], qwc);

		Cpu->Clear(spr0->madr, qwc>>2);
		// clear VU mem also!
		TestClearVUs(spr0->madr, qwc>>2);
		
		spr0->madr += qwc;
	}
	spr0->sadr += qwc;

	spr0->qwc = 0;
    return (qwc>>4) * BIAS; // bus is 1/2 the ee speed
}

#define SPR0chain() \
	if (spr0->qwc) { \
		cycles += _SPR0chain(); \
/*		cycles+= spr0->qwc / BIAS;*/ /* guessing */ \
	}

void _SPR0interleave() {
	int qwc = spr0->qwc;
	int sqwc = psHu32(DMAC_SQWC) & 0xff;
	int tqwc = (psHu32(DMAC_SQWC) >> 16) & 0xff;
	int cycles = 0;
	u32 *pMem;
	//SysPrintf("dmaSPR0 interleave\n");
#ifdef SPR_LOG
		SPR_LOG("SPR0 interleave size=%d, tqwc=%d, sqwc=%d, addr=%lx sadr=%lx\n",
				spr0->qwc, tqwc, sqwc, spr0->madr, spr0->sadr);
#endif
	while (qwc > 0) {
		spr0->qwc = min(tqwc, qwc); qwc-= spr0->qwc;
		pMem = (u32*)dmaGetAddr(spr0->madr);
		if ((psHu32(DMAC_CTRL) & 0xC) == 0xC || // GIF MFIFO
			(psHu32(DMAC_CTRL) & 0xC) == 0x8) { // VIF1 MFIFO
			hwMFIFOWrite(spr0->madr, (u8*)&PS2MEM_SCRATCH[spr0->sadr & 0x3fff], spr0->qwc<<4);
		} else {
			Cpu->Clear(spr0->madr, spr0->qwc<<2);
			// clear VU mem also!
			TestClearVUs(spr0->madr, spr0->qwc<<2);

			memcpy_amd((u8*)pMem, &PS2MEM_SCRATCH[spr0->sadr & 0x3fff], spr0->qwc<<4);
		}
		cycles += tqwc * BIAS;
		spr0->sadr+= spr0->qwc * 16;
		spr0->madr+= (sqwc+spr0->qwc)*16; //qwc-= sqwc;
	}

	spr0->qwc = 0;	
	INT(8, cycles);
}

void _dmaSPR0() {
	u32 *ptag;
	int id;
	int cycles = 0;
	int done = 0;

	if ((psHu32(DMAC_CTRL) & 0x30) == 0x20) { // STS == fromSPR
		SysPrintf("SPR0 stall %d\n", (psHu32(DMAC_CTRL)>>6)&3);
	}

	if ((spr0->chcr & 0xc) == 0x8) { // Interleave Mode
		_SPR0interleave();
		return;
	}

	// Transfer Dn_QWC from SPR to Dn_MADR
	
	

	if ((spr0->chcr & 0xc) == 0 || spr0->qwc > 0) { // Normal Mode
		SPR0chain();
			INT(8, cycles);
		
		return;
	}

	// Destination Chain Mode

	while (done == 0) {  // Loop while Dn_CHCR.STR is 1
		ptag = (u32*)&PS2MEM_SCRATCH[spr0->sadr & 0x3fff];
		spr0->sadr+= 16;

		// Transfer dma tag if tte is set
//		if (spr0->chcr & 0x40) SPR0transfer(ptag, 4);

		spr0->chcr = ( spr0->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );	//Transfer upper part of tag to CHCR bits 31-15

		id        = (ptag[0] >> 28) & 0x7;		//ID for DmaChain copied from bit 28 of the tag
		spr0->qwc  = (u16)ptag[0];				//QWC set to lower 16bits of the tag
		spr0->madr = ptag[1];					//MADR = ADDR field

#ifdef SPR_LOG
		SPR_LOG("dmaChain %8.8x_%8.8x size=%d, id=%d, addr=%lx\n",
				ptag[1], ptag[0], spr0->qwc, id, spr0->madr);
#endif
		if ((psHu32(DMAC_CTRL) & 0x30) == 0x20) { // STS == fromSPR
			SysPrintf("SPR stall control\n");
		}
		
		switch (id) {
			case 0: // CNTS - Transfer QWC following the tag (Stall Control)
			if ((psHu32(DMAC_CTRL) & 0x30) == 0x20	) psHu32(DMAC_STADR) = spr0->madr + (spr0->qwc * 16);					//Copy MADR to DMAC_STADR stall addr register
				break;

			case 1: // CNT - Transfer QWC following the tag.
				break;

			case 7: // End - Transfer QWC following the tag
				done = 1;								//End Transfer
				break;
		}
		SPR0chain();
		if (spr0->chcr & 0x80 && ptag[0] >> 31) {			 //Check TIE bit of CHCR and IRQ bit of tag
			//SysPrintf("SPR0 TIE\n");
			done = 1;
			spr0->qwc = 0;
			break;
		}
		

/*		if (spr0->chcr & 0x80 && ptag[0] >> 31) {
#ifdef SPR_LOG
			SPR_LOG("dmaIrq Set\n");
#endif
			spr0->chcr&= ~0x100;
			hwDmacIrq(8);
			return;
		}*/
	}
	
		INT(8, cycles);
	
}

int SPRFROMinterrupt()
{
	spr0->chcr&= ~0x100;
	hwDmacIrq(8);
	return 1;
}

extern void mfifoGIFtransfer(int);
#define gif ((DMACh*)&PS2MEM_HW[0xA000])
void dmaSPR0() { // fromSPR
	int qwc = spr0->qwc;
#ifdef SPR_LOG
	SPR_LOG("dmaSPR0 chcr = %lx, madr = %lx, qwc  = %lx, sadr = %lx\n",
			spr0->chcr, spr0->madr, spr0->qwc, spr0->sadr);
#endif

	_dmaSPR0();
	if ((psHu32(DMAC_CTRL) & 0xC) == 0xC) { // GIF MFIFO
		spr0->madr = psHu32(DMAC_RBOR) + (spr0->madr & psHu32(DMAC_RBSR));
		//SysPrintf("mfifoGIFtransfer %x madr %x, tadr %x\n", gif->chcr, gif->madr, gif->tadr);
		if(gif->chcr & 0x100)mfifoGIFtransfer(qwc);
	} else
	if ((psHu32(DMAC_CTRL) & 0xC) == 0x8) { // VIF1 MFIFO
		spr0->madr = psHu32(DMAC_RBOR) + (spr0->madr & psHu32(DMAC_RBSR));
		//SysPrintf("mfifoVIF1transfer %x madr %x, tadr %x\n", vif1ch->chcr, vif1ch->madr, vif1ch->tadr);
		if(vif1ch->chcr & 0x100)mfifoVIF1transfer(qwc);
	}
	
	FreezeMMXRegs(0);
	FreezeXMMRegs(0);
}

__inline static void SPR1transfer(u32 *data, int size) {
/*	{
		int i;
		for (i=0; i<size; i++) {
#ifdef SPR_LOG 
		   SPR_LOG( "SPR1transfer[0x%x]: 0x%x\n", (spr1->sadr+i*4) & 0x3fff, data[i] );
#endif
		}
	}*/
	//Cpu->Clear(spr1->sadr, size); // why?
	memcpy_amd(&PS2MEM_SCRATCH[spr1->sadr & 0x3fff], (u8*)data, size << 2);

	spr1->sadr+= size << 2;
}

int  _SPR1chain() {
	u32 qwc = spr1->qwc;
	u32 *pMem;

	if (qwc == 0) return 0;

	pMem = (u32*)dmaGetAddr(spr1->madr);
	if (pMem == NULL) return -1;

	SPR1transfer(pMem, qwc << 2);
	spr1->madr+= spr1->qwc << 4;
	spr1->qwc = 0;
	return (qwc) * BIAS;
}

#define SPR1chain() \
	if (spr1->qwc) { \
		cycles += _SPR1chain(); \
/*		cycles+= spr1->qwc / BIAS;*/ /* guessing */ \
	}

void _SPR1interleave() {
	int qwc = spr1->qwc;
	int sqwc = psHu32(DMAC_SQWC) & 0xff;
	int tqwc = (psHu32(DMAC_SQWC) >> 16) & 0xff;
	int cycles = 0;
	u32 *pMem;

#ifdef SPR_LOG
		SPR_LOG("SPR1 interleave size=%d, tqwc=%d, sqwc=%d, addr=%lx sadr=%lx\n",
				spr1->qwc, tqwc, sqwc, spr1->madr, spr1->sadr);
#endif
	while (qwc > 0) {
		spr1->qwc = min(tqwc, qwc); qwc-= spr1->qwc;
		pMem = (u32*)dmaGetAddr(spr1->madr);
		memcpy_amd(&PS2MEM_SCRATCH[spr1->sadr & 0x3fff], (u8*)pMem, spr1->qwc <<4);
		spr1->sadr += spr1->qwc * 16;
		cycles += spr1->qwc * BIAS;
		spr1->madr+= (sqwc + spr1->qwc) * 16; //qwc-= sqwc;
	}

		spr1->qwc = 0;
		INT(9, cycles);
	
}

void dmaSPR1() { // toSPR
	u32 *ptag;
	int id, done=0;
	int cycles = 0;

#ifdef SPR_LOG
	SPR_LOG("dmaSPR1 chcr = 0x%x, madr = 0x%x, qwc  = 0x%x\n"
			"        tadr = 0x%x, sadr = 0x%x\n",
			spr1->chcr, spr1->madr, spr1->qwc,
			spr1->tadr, spr1->sadr);
#endif

	if ((spr1->chcr & 0xc) == 0x8) { // Interleave Mode
		_SPR1interleave();
		FreezeMMXRegs(0);
		return;
	}

	
	if ((spr1->chcr & 0xc) == 0 || spr1->qwc > 0) { // Normal Mode
		// Transfer Dn_QWC from Dn_MADR to SPR1
		SPR1chain();
		INT(9, cycles);
		FreezeMMXRegs(0);
		return;
	}

	// Chain Mode

	while (done == 0) {  // Loop while Dn_CHCR.STR is 1
		ptag = (u32*)dmaGetAddr(spr1->tadr);		//Set memory pointer to TADR
		if (ptag == NULL) {							//Is ptag empty?
			SysPrintf("SPR1 Tag BUSERR\n");
			spr1->chcr = ( spr1->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );	//Transfer upper part of tag to CHCR bits 31-15
			psHu32(DMAC_STAT)|= 1<<15;				//If yes, set BEIS (BUSERR) in DMAC_STAT register
			done = 1;
			break;
		}
		spr1->chcr = ( spr1->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );	//Transfer upper part of tag to CHCR bits 31-15

		id        = (ptag[0] >> 28) & 0x7;			//ID for DmaChain copied from bit 28 of the tag
		spr1->qwc  = (u16)ptag[0];					//QWC set to lower 16bits of the tag
		spr1->madr = ptag[1];						//MADR = ADDR field

		// Transfer dma tag if tte is set
		if (spr1->chcr & 0x40) {
#ifdef SPR_LOG
			SPR_LOG("SPR TTE: %x_%x\n", ptag[3], ptag[2]);
#endif
			SPR1transfer(ptag, 4);				//Transfer Tag

		}

		

#ifdef SPR_LOG
		SPR_LOG("dmaChain %8.8x_%8.8x size=%d, id=%d, addr=%lx\n",
				ptag[1], ptag[0], spr1->qwc, id, spr1->madr);
#endif
		
		done = hwDmacSrcChain(spr1, id);
		SPR1chain();										//Transfers the data set by the switch

		if (spr1->chcr & 0x80 && ptag[0] >> 31) {			//Check TIE bit of CHCR and IRQ bit of tag
#ifdef SPR_LOG
			SPR_LOG("dmaIrq Set\n");
#endif
			//SysPrintf("SPR1 TIE\n");
			spr1->qwc = 0;
			break;
		}
	}

	
	INT(9, cycles);
	FreezeMMXRegs(0);
}

int SPRTOinterrupt()
{
	spr1->chcr &= ~0x100;
	hwDmacIrq(9);
	return 1;
}

