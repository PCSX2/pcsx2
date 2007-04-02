/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2005  Pcsx2 Team
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "PsxCommon.h"

#ifdef __MSCW32__
#pragma warning(disable:4244)
#endif

// *** FOR WORKS ON PADS AND MEMORY CARDS *****

const unsigned char buf1[] = {0x25, 0x46, 0x01, 0x31, 0x00, 0xA2, 0x11, 0x01, 0xE1};//{0x64, 0x23, 0x2, 0x43, 0x0, 0xa2, 0x11, 0x1, 0xb4}; 
const unsigned char buf2[] = {0xC5, 0xBD, 0x66, 0x00, 0x59, 0x44, 0x01, 0x02, 0x00};//{0x6d, 0x21, 0x30, 0x0, 0x55, 0x19, 0x2, 0x2, 0x30};
const unsigned char buf4[] = {0x02, 0x9A, 0x9E, 0x06, 0x6D, 0x3C, 0xF0, 0x7E, 0xDF};//{0xa3, 0x5d, 0x2f, 0xa2, 0xd8, 0x7c, 0x5b, 0x35, 0xb9};
const unsigned char buff[] = {0xC2, 0x39, 0x6F, 0x27, 0xC8, 0xDF, 0x2A, 0x23, 0xAD};//{0xf3, 0x9b, 0x32, 0x87, 0x31, 0xda, 0x9d, 0x10, 0xbb};
const unsigned char buf11[] = {0xA8, 0x42, 0x5D, 0x87, 0x65, 0x32, 0x6F, 0xE8, 0xE0};//{0x39, 0xd2, 0xb9, 0x5c, 0xcf, 0x31, 0x2d, 0x23, 0xfe};
const unsigned char buf13[] = {0x46, 0x31, 0xFC, 0x97, 0xA8, 0x6D, 0xE2, 0x12, 0x29};//{0x20, 0x2e, 0xd7, 0x99, 0x92, 0x29, 0x4a, 0x12, 0xa3};

const unsigned char cardh[4] = { 0xFF, 0xFF, 0x5a, 0x5d };
struct mc_command_0x26_tag mc_command_0x26=
	{'+', 512, 16, 0x4000, 0x52, 'Z'};//sizeof()==11

// clk cycle byte
// 4us * 8bits = ((PSXCLK / 1000000) * 32) / BIAS; (linuzappz)
#define SIO_INT() PSX_INT(16, PSXCLK/250000); /*270;*/

void _ReadMcd(char *data, u32 adr, int size) {
	ReadMcd(sio.CtrlReg&0x2000?2:1, data, adr, size);
}

void _SaveMcd(char *data, u32 adr, int size) {
	SaveMcd(sio.CtrlReg&0x2000?2:1, data, adr, size);
}

unsigned char xor(unsigned char *buf, unsigned int length){
	register unsigned char i, x;

	for (x=0, i=0; i<length; i++)	x ^= buf[i];
	return x & 0xFF;
}

int sioInit() {
	memset(&sio, 0, sizeof(sio));

// Transfer(?) Ready and the Buffer is Empty
	sio.StatReg = TX_RDY | TX_EMPTY;
	sio.packetsize = 0;
	sio.terminator = 'Z';

	return 0;
}

unsigned char sioRead8() {
	unsigned char ret = 0xFF;

	if ((sio.StatReg & RX_RDY)/* && (sio.CtrlReg & RX_PERM)*/) {
//		sio.StatReg &= ~RX_OVERRUN;
		ret = sio.buf[sio.parp];
		if (sio.parp == sio.bufcount) {
			sio.StatReg &= ~RX_RDY;		// Receive is not Ready now?
			sio.StatReg |= TX_EMPTY;	// Buffer is Empty
			if (sio.mcdst == 5) {
				sio.mcdst = 0;
				if (sio.rdwr == 2) {
					_SaveMcd(&sio.buf[1], (sio.adrL | (sio.adrH << 8)) * 128, 128);
				}
			}
			if (sio.padst == 2) sio.padst = 0;
			if (sio.mcdst == 1) {
				sio.mcdst = 2;
				sio.StatReg&= ~TX_EMPTY;
				sio.StatReg|= RX_RDY;
			}
		}
	}

#ifdef PAD_LOG
	PAD_LOG("sio read8 ;ret = %x\n", ret);
#endif
	return ret;
}

void sioWrite8(unsigned char value) {
	u32 i = 0;
#ifdef PAD_LOG
	PAD_LOG("sio write8 %x\n", value);
#endif
	switch (sio.padst) {
		case 1: SIO_INT();
			if ((value&0x40) == 0x40) {
				sio.padst = 2; sio.parp = 1;
				switch (sio.CtrlReg&0x2002) {
					case 0x0002:
						sio.packetsize ++;	// Total packet size sent
						sio.buf[sio.parp] = PAD1poll(value);
						break;
					case 0x2002:
						sio.packetsize ++;	// Total packet size sent
						sio.buf[sio.parp] = PAD2poll(value);
						break;
				}
				if (!(sio.buf[sio.parp] & 0x0f)) {
					sio.bufcount = 2 + 32;
				} else {
					sio.bufcount = 2 + (sio.buf[sio.parp] & 0x0f) * 2;
				}
			}
			else sio.padst = 0;
			return;
		case 2:
			sio.parp++;
			switch (sio.CtrlReg&0x2002) {
				case 0x0002: sio.packetsize ++; sio.buf[sio.parp] = PAD1poll(value); break;
				case 0x2002: sio.packetsize ++; sio.buf[sio.parp] = PAD2poll(value); break;
			}
			if (sio.parp == sio.bufcount) { sio.padst = 0; return; }
			SIO_INT();
			return;
	}

	switch (sio.mcdst) {
		case 1:
			sio.packetsize++;
			SIO_INT();
			if (sio.rdwr) { sio.parp++; return; }
			sio.parp = 1;
			sio.buf[0]=sio.buf[1]=0xFF;
			switch (value) {
			case 0x11: 
			case 0x12: 
			case 0x81:
				sio.bufcount =  8; 
				memset(sio.buf, 0xFF, sio.bufcount+1);
				sio.buf[3] = sio.terminator;
				sio.buf[2] = '+';
				sio.mcdst = 99; 
				sio2.packet.recvVal3 = 0x8c;
				if(value == 0x81) {
					if(sio.mc_command==0x42)
						sio2.packet.recvVal1 = 0x1600; // Writing
					else if(sio.mc_command==0x43) sio2.packet.recvVal1 = 0x1700; // Reading
				}
				break;
			case 0x21: 
			case 0x22: 
			case 0x23: 
                sio.bufcount =  8; sio.mcdst = 99; sio.sector=0; sio.k=0;
				memset(sio.buf, 0xFF, sio.bufcount+1);
				sio2.packet.recvVal3 = 0x8c; 
				sio.buf[sio.bufcount]=sio.terminator;
				sio.buf[sio.bufcount-1]='+';
                break;
			case 0x24:											break;//! 
			case 0x25:											break;//!
			case 0x26: 
				sio.bufcount = 12; sio.mcdst = 99; sio2.packet.recvVal3 = 0x83;	
				memset(sio.buf, 0xFF, sio.bufcount+1);
				memcpy(&sio.buf[2], &mc_command_0x26, sizeof(mc_command_0x26));
				sio.buf[sio.bufcount]=sio.terminator;
				break;
			case 0x27: 
			case 0x28: 
			case 0xBF:
				sio.bufcount =  8; sio.mcdst = 99; sio2.packet.recvVal3 = 0x8b;
				memset(sio.buf, 0xFF, sio.bufcount+1);
				sio.buf[4]=sio.terminator;
				sio.buf[3]='+';
				break;
			case 0x42: 
			case 0x43: 
			case 0x82:
				if(value==0x82 && sio.lastsector==sio.sector) sio.mode = 2;
				if(value==0x42) sio.mode = 0;
				if(value==0x43)	sio.lastsector = sio.sector;
				
				sio.bufcount =133; sio.mcdst = 99;
				memset(sio.buf, 0xFF, sio.bufcount+1);
				sio.buf[sio.bufcount]=sio.terminator;
				sio.buf[sio.bufcount-1]='+';
				break;
			case 0xf0: 		//don't handle this here, see below		  //__card_auth_00_03_05_08_0a_0c_0e_10_12_14
			case 0xf1:												  //__card_auth_00_03_05_08_0a_0c_0e_10_12_14
			case 0xf2:				  
				sio.mcdst = 99;	
				break;//__card_auth_00_03_05_08_0a_0c_0e_10_12_14
			case 0xf3: //__card_auth_0x60_F3
			case 0xf7: 
				sio.bufcount = 8; sio.mcdst = 99;
				memset(sio.buf, 0xFF, sio.bufcount+1);
				sio.buf[4]=sio.terminator;
				sio.buf[3]='+';
				break;//__card_auth_key_change_F7(sendBuf[2])
			case 0x52:
				sio.rdwr = 1; memset(sio.buf, 0xFF, sio.bufcount+1);
				sio.buf[sio.bufcount]=sio.terminator; sio.buf[sio.bufcount-1]='+';
				break;
			case 0x57:
				sio.rdwr = 2; memset(sio.buf, 0xFF, sio.bufcount+1);
				sio.buf[sio.bufcount]=sio.terminator; sio.buf[sio.bufcount-1]='+';	
				break;
			default:
				sio.mcdst = 0;
				memset(sio.buf, 0xFF, sio.bufcount+1);
				sio.buf[sio.bufcount]=sio.terminator; sio.buf[sio.bufcount-1]='+';	
#ifdef MEMCARDS_LOG
				MEMCARDS_LOG("Unknown MC(%d) command 0x%02X\n", ((sio.CtrlReg&0x2000)>>13)+1, value);
#endif	
			}
			sio.mc_command=value;
			if (sio.mc_command!=0x21 && sio.mc_command!=0x22 && sio.mc_command!=0x23 &&
			   (sio.mc_command!=0xF0) && (sio.mc_command!=0xF1) && (sio.mc_command!=0xF2)) {
#ifdef MEMCARDS_LOG
				MEMCARDS_LOG("MC(%d) command 0x%02X\n", ((sio.CtrlReg&0x2000)>>13)+1, value);
#endif
			}
			return;
		case 99://as is...
			sio.packetsize++;
			sio.parp++;
			switch(sio.mc_command)
			{
			case 0x21:
			case 0x22:
			case 0x23:
                if (sio.parp==2)sio.sector|=(value & 0xFF)<< 0;
				if (sio.parp==3)sio.sector|=(value & 0xFF)<< 8;
				if (sio.parp==4)sio.sector|=(value & 0xFF)<<16;
				if (sio.parp==5)sio.sector|=(value & 0xFF)<<24;
				if (sio.parp==6)
				{
#ifdef MEMCARDS_LOG
                 MEMCARDS_LOG("MC(%d) command 0x%02X sio.sector 0x%04X\n",
								((sio.CtrlReg&0x2000)>>13)+1, sio.mc_command, sio.sector);
#endif
				}
				break;
			case 0x27:
				if(sio.parp==2)	{
					sio.terminator = value;
					sio.buf[4] = value;
				}
				break;
			case 0x28:
				if(sio.parp == 2) {
					sio.buf[2] = '+';
					if(value == 0) {
						sio.buf[4] = 0xFF;
						sio.buf[3] = sio.terminator;
					}else{
						sio.buf[4] = 0x5A;
						sio.buf[3] = sio.terminator;
					}
				}	
				break;
			case 0x42:
				if (sio.parp==2) {
					sio.bufcount=5+value;
					memset(sio.buf, 0xFF, sio.bufcount+1);
					sio.buf[sio.bufcount-1]='+';
					sio.buf[sio.bufcount]=sio.terminator;
				} else
				if ((sio.parp>2) && (sio.parp<sio.bufcount-2)) {
					sio.buf[sio.parp]=value;
				} else
				if (sio.parp==sio.bufcount-2) {
					if (xor(&sio.buf[3], sio.bufcount-5)==value) {
                        _SaveMcd(&sio.buf[3], (512+16)*sio.sector+sio.k, sio.bufcount-5);
						sio.buf[sio.bufcount-1]=value;
						sio.k+=sio.bufcount-5;
					}else {
#ifdef MEMCARDS_LOG
						MEMCARDS_LOG("MC(%d) write XOR value error 0x%02X != ^0x%02X\n",
							((sio.CtrlReg&0x2000)>>13)+1, value, xor(&sio.buf[3], sio.bufcount-5));
#endif
					}
				}
				break;
			case 0x43:
				if (sio.parp==2){
					sio.bufcount=value+5;
					sio.buf[3]='+';
					_ReadMcd(&sio.buf[4], (512+16)*sio.sector+sio.k, value);
					if(sio.mode==2) {
						int j;
						for(j=0; j < value; j++)
							sio.buf[4+j] = ~sio.buf[4+j];
					}
					sio.k+=value;
					sio.buf[sio.bufcount-1]=xor(&sio.buf[4], value);
					sio.buf[sio.bufcount]=sio.terminator;
				}
				break;
			case 0x82:
				if(sio.parp==2) {
					sio.buf[2]='+';
					sio.buf[3]=sio.terminator;
				}
				break;
			case 0xF0:
				if (sio.parp==2)
				{
					u32 flag=0;
#ifdef MEMCARDS_LOG
					MEMCARDS_LOG("MC(%d) command 0xF0:0x%02X\n", ((sio.CtrlReg&0x2000)>>13)+1, value);
#endif
					switch(value){
					case  1:
						flag = 0;
						sio.bufcount=13;
						memset(sio.buf, 0xFF, sio.bufcount+1);
						memcpy(&sio.buf[4],buf1,9);//xor value for OUT data
						sio.buf[3]='+';
						break;//OUT
					case  2:
						flag = 0;
						sio.bufcount=13;
						memset(sio.buf, 0xFF, sio.bufcount+1);
						memcpy(&sio.buf[4],buf2,9);//xor value for OUT data
						sio.buf[3]='+';
						break;//OUT
					case  4:
						flag = 0;
						sio.bufcount=13;
						memset(sio.buf, 0xFF, sio.bufcount+1);
						memcpy(&sio.buf[4],buf4,9);//xor value for OUT data
						sio.buf[3]='+';
						break;//OUT
					case  6:
					case  7:
					case 11:
						flag=0;
						sio.bufcount=13;
						memset(sio.buf, 0xFF, sio.bufcount+1);
						sio.buf[sio.bufcount-1]='+';
						break;//IN
					case 15:
						flag = 0;
						sio.bufcount=13;
						memset(sio.buf, 0xFF, sio.bufcount+1);
						memcpy(&sio.buf[4],buff,9);//xor value for OUT data
						sio.buf[3]='+';
						break;//OUT
					case 17:
						flag = 0;
						sio.bufcount=13;
						memset(sio.buf, 0xFF, sio.bufcount+1);
						memcpy(&sio.buf[4],buf11,9);//xor value for OUT data
						sio.buf[3]='+';
						break;//OUT
					case 19:
						flag = 0;
						sio.bufcount=13;
						memset(sio.buf, 0xFF, sio.bufcount+1);
						memcpy(&sio.buf[4],buf13,9);//xor value for OUT data
						sio.buf[3]='+';
						break;//OUT
					default:
						sio.bufcount=4;
						memset(sio.buf, 0xFF, sio.bufcount+1);
						sio.buf[sio.bufcount-1]='+';
					}
					
					if ((sio.bufcount==13) && flag){
						sio.buf[sio.bufcount-1] = 0;//xor value for OUT data
						sio.buf[3]='+';
					}
					sio.buf[sio.bufcount]=sio.terminator;
				}
				break;
			}
			if (sio.bufcount<=sio.parp)	sio.mcdst = 0;
			return;
	}

	switch (sio.mtapst) {
		case 0x1:
			sio.packetsize++;
			sio.parp = 1;
			SIO_INT();
			switch(value) {
			case 0x12:  sio.mtapst = 2; sio.bufcount = 5;	break;
			case 0x13:  sio.mtapst = 2; sio.bufcount = 5;	break;
			case 0x21:	sio.mtapst = 2; sio.bufcount = 6;	break;
			}
			sio.buf[sio.bufcount]='Z';
			sio.buf[sio.bufcount-1]='+';
			return;
		case 0x2:
			sio.packetsize++;
			sio.parp++;
            if (sio.bufcount<=sio.parp)	sio.mcdst = 0;
			SIO_INT();
			return;
	}

	switch (value) {
		case 0x01: // start pad
			sio.StatReg &= ~TX_EMPTY;	// Now the Buffer is not empty
			sio.StatReg |= RX_RDY;		// Transfer is Ready

			switch (sio.CtrlReg&0x2002) {
				case 0x0002: sio.buf[0] = PAD1startPoll(1); break;
				case 0x2002: sio.buf[0] = PAD2startPoll(2); break;
			}

			sio.bufcount = 2;
			sio.parp = 0;
			sio.padst = 1;
			sio.packetsize = 1; // Count this one too ! :P
			sio2.packet.recvVal1 = 0x1100; // Pad is present
			SIO_INT();
			return;

		case 0x21: // start mtap
			sio.StatReg &= ~TX_EMPTY;	// Now the Buffer is not empty
			sio.StatReg |= RX_RDY;		// Transfer is Ready
			sio.parp = 0;
			sio.packetsize = 1; // Count this one too ! :P
			sio.mtapst = 1;
			sio2.packet.recvVal1 = 0x1D100; // Mtap is not connected :)
			SIO_INT();
			return;

		case 0x61: // start remote control sensor
			sio.StatReg &= ~TX_EMPTY;	// Now the Buffer is not empty
			sio.StatReg |= RX_RDY;		// Transfer is Ready
			sio.parp = 0;
			sio.packetsize = 1; // Count this one too ! :P
			sio2.packet.recvVal1 = 0x1100; // Pad is present
			SIO_INT();
			return;

		case 0x81: // start memcard
			sio.StatReg &= ~TX_EMPTY;
			sio.StatReg |= RX_RDY;
			memcpy(sio.buf, cardh, 4);
			sio.parp = 0;
			sio.bufcount = 3;
			sio.mcdst = 1;
			sio.packetsize = 1;
			sio.rdwr = 0;
			sio2.packet.recvVal1 = 0x1100; // Memcard1 is present
			SIO_INT();
			return;
	}
}

void sioWriteCtrl16(unsigned short value) {
	sio.CtrlReg = value & ~RESET_ERR;
	if (value & RESET_ERR) sio.StatReg &= ~IRQ;
	if ((sio.CtrlReg & SIO_RESET) || (!sio.CtrlReg)) {
		sio.mtapst = 0; sio.padst = 0; sio.mcdst = 0; sio.parp = 0;
		sio.StatReg = TX_RDY | TX_EMPTY;
		psxRegs.interrupt&= ~(1<<16);
	}
}

int  sioInterrupt() {
#ifdef PAD_LOG
	PAD_LOG("Sio Interrupt\n");
#endif
	sio.StatReg|= IRQ;
	psxHu32(0x1070)|=0x80;
	return 1;
}

FILE *LoadMcd(int mcd) {
	char str[256];
	FILE *f;

	if (mcd == 1) {
		strcpy(str, Config.Mcd1);
	} else {
		strcpy(str, Config.Mcd2);
	}
	if (*str == 0) sprintf(str, "memcards/Mcd00%d.ps2", mcd);
	f = fopen(str, "r+b");
	if (f == NULL) {
		CreateMcd(str);
		f = fopen(str, "r+b");
	}
	if (f == NULL) {
		SysMessage (_("Failed loading MemCard %s"), str); return NULL;
	}

	return f;
}

void SeekMcd(FILE *f, u32 adr) {
	u32 size;

	fseek(f, 0, SEEK_END); size = ftell(f);
	if (size == MCD_SIZE + 64)
		fseek(f, adr + 64, SEEK_SET);
	else if (size == MCD_SIZE + 3904)
		fseek(f, adr + 3904, SEEK_SET);
	else
		fseek(f, adr, SEEK_SET);
}

void ReadMcd(int mcd, char *data, u32 adr, int size) {
	FILE *f = LoadMcd(mcd);
	if (f == NULL) {
		memset(data, 0, size);
		return;
	}
	SeekMcd(f, adr);
	fread(data, 1, size, f);
	fclose(f);
}

void SaveMcd(int mcd, char *data, u32 adr, int size) {
	FILE *f = LoadMcd(mcd);
	if (f == NULL) {
		return;
	}
	SeekMcd(f, adr);
	fwrite(data, 1, size, f);
	fclose(f);
}

void CreateMcd(char *mcd) {
	FILE *fp;	
	int i=0, j=0;
	int enc[16] = {0x77,0x7f,0x7f,0x77,0x7f,0x7f,0x77,0x7f,0x7f,0x77,0x7f,0x7f,0,0,0,0};

	fp = fopen(mcd, "wb");
	if (fp == NULL) return;
	for(i=0; i < 16384; i++) 
	{
		for(j=0; j < 128; j++) fputc(0x00,fp);
		for(j=0; j < 128; j++) fputc(0x00,fp);
		for(j=0; j < 128; j++) fputc(0x00,fp);
		for(j=0; j < 128; j++) fputc(0x00,fp);
		for(j=0; j < 16; j++) fputc(enc[j],fp);
	}
	fclose(fp);
}

int sioFreeze(gzFile f, int Mode) {
	gzfreeze(&sio, sizeof(sio));

	return 0;
}


