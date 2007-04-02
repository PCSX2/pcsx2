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

#include "PsxCommon.h"
#include "iR5900.h"

#ifdef __MSCW32__
#pragma warning(disable:4244)
#endif

// NOTE: Any modifications to read/write fns should also go into their const counterparts

void psxHwReset() {
/*	if (Config.Sio) psxHu32(0x1070) |= 0x80;
	if (Config.SpuIrq) psxHu32(0x1070) |= 0x200;*/

	memset(psxH, 0, 0x10000);

//	mdecInit(); //intialize mdec decoder
	cdrReset();
	cdvdReset();
	psxRcntInit();
	sioInit();
//	sio2Reset();
}

u8 psxHwRead8(u32 add) {
	u8 hard;

	if (add >= 0x1f801600 && add < 0x1f801700) {
		return USBread8(add);
	}

	switch (add) {
		case 0x1f801040: hard = sioRead8();break; 
      //  case 0x1f801050: hard = serial_read8(); break;//for use of serial port ignore for now

		case 0x1f80146e: // DEV9_R_REV
			return DEV9read8(add);

#ifdef PCSX2_DEVBUILD
		case 0x1f801100:
		case 0x1f801104:
		case 0x1f801108:
		case 0x1f801110:
		case 0x1f801114:
		case 0x1f801118:
		case 0x1f801120:
		case 0x1f801124:
		case 0x1f801128:
		case 0x1f801480:
		case 0x1f801484:
		case 0x1f801488:
		case 0x1f801490:
		case 0x1f801494:
		case 0x1f801498:
		case 0x1f8014a0:
		case 0x1f8014a4:
		case 0x1f8014a8:
			SysPrintf("8bit counter read %x\n", add);
			hard = psxHu8(add);
			return hard;
#endif

		case 0x1f801800: hard = cdrRead0(); break;
		case 0x1f801801: hard = cdrRead1(); break;
		case 0x1f801802: hard = cdrRead2(); break;
		case 0x1f801803: hard = cdrRead3(); break;

		case 0x1f803100: // PS/EE/IOP conf related
			hard = 0x10; // Dram 2M
			break;

		case 0x1F808264:
			hard = sio2_fifoOut();//sio2 serial data feed/fifo_out
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 read8 DATAOUT %08X\n", hard);
#endif
			return hard;

		default:
			hard = psxHu8(add); 
#ifdef PSXHW_LOG
			PSXHW_LOG("*Unkwnown 8bit read at address %lx\n", add);
#endif
			return hard;
	}
	
#ifdef PSXHW_LOG
	PSXHW_LOG("*Known 8bit read at address %lx value %x\n", add, hard);
#endif
	return hard;
}

#define CONSTREAD8_CALL(name) { \
	iFlushCall(0); \
	CALLFunc((u32)name); \
	if( sign ) MOVSX32R8toR(EAX, EAX); \
	else MOVZX32R8toR(EAX, EAX); \
} \

static u32 s_16 = 0x10;

int psxHwConstRead8(u32 x86reg, u32 add, u32 sign) {
	
	if (add >= 0x1f801600 && add < 0x1f801700) {
		PUSH32I(add);
		CONSTREAD8_CALL(USBread8);
		// since calling from different dll, esp already changed
		return 1;
	}

	switch (add) {
		case 0x1f801040:
			CONSTREAD8_CALL(sioRead8);
			return 1;
      //  case 0x1f801050: hard = serial_read8(); break;//for use of serial port ignore for now

#ifdef PCSX2_DEVBUILD
		case 0x1f801100:
		case 0x1f801104:
		case 0x1f801108:
		case 0x1f801110:
		case 0x1f801114:
		case 0x1f801118:
		case 0x1f801120:
		case 0x1f801124:
		case 0x1f801128:
		case 0x1f801480:
		case 0x1f801484:
		case 0x1f801488:
		case 0x1f801490:
		case 0x1f801494:
		case 0x1f801498:
		case 0x1f8014a0:
		case 0x1f8014a4:
		case 0x1f8014a8:
			SysPrintf("8bit counter read %x\n", add);
			_eeReadConstMem8(x86reg, (u32)&psxH[(add) & 0xffff], sign);
			return 0;
#endif

		case 0x1f80146e: // DEV9_R_REV
			PUSH32I(add);
			CONSTREAD8_CALL(DEV9read8);
			return 1;

		case 0x1f801800: CONSTREAD8_CALL(cdrRead0); return 1;
		case 0x1f801801: CONSTREAD8_CALL(cdrRead1); return 1;
		case 0x1f801802: CONSTREAD8_CALL(cdrRead2); return 1;
		case 0x1f801803: CONSTREAD8_CALL(cdrRead3); return 1;

		case 0x1f803100: // PS/EE/IOP conf related
			if( IS_XMMREG(x86reg) ) SSEX_MOVD_M32_to_XMM(x86reg&0xf, (u32)&s_16);
			else if( IS_MMXREG(x86reg) ) MOVDMtoMMX(x86reg&0xf, (u32)&s_16);
			else MOV32ItoR(x86reg, 0x10);
			return 0;

		case 0x1F808264: //sio2 serial data feed/fifo_out
			CONSTREAD8_CALL(sio2_fifoOut);
			return 1;

		default:
			_eeReadConstMem8(x86reg, (u32)&psxH[(add) & 0xffff], sign);
			return 0;
	}
}

u16 psxHwRead16(u32 add) {
	u16 hard;

	if (add >= 0x1f801600 && add < 0x1f801700) {
		return USBread16(add);
	}

	switch (add) {
#ifdef PSXHW_LOG
		case 0x1f801070: PSXHW_LOG("IREG 16bit read %x\n", psxHu16(0x1070));
			return psxHu16(0x1070);
#endif
#ifdef PSXHW_LOG
		case 0x1f801074: PSXHW_LOG("IMASK 16bit read %x\n", psxHu16(0x1074));
			return psxHu16(0x1074);
#endif

		case 0x1f801040:
			hard = sioRead8();
			hard|= sioRead8() << 8;
#ifdef PAD_LOG
			PAD_LOG("sio read16 %lx; ret = %x\n", add&0xf, hard);
#endif
			return hard;
		case 0x1f801044:
			hard = sio.StatReg;
#ifdef PAD_LOG
			PAD_LOG("sio read16 %lx; ret = %x\n", add&0xf, hard);
#endif
			return hard;
		case 0x1f801048:
			hard = sio.ModeReg;
#ifdef PAD_LOG
			PAD_LOG("sio read16 %lx; ret = %x\n", add&0xf, hard);
#endif
			return hard;
		case 0x1f80104a:
			hard = sio.CtrlReg;
#ifdef PAD_LOG
			PAD_LOG("sio read16 %lx; ret = %x\n", add&0xf, hard);
#endif
			return hard;
		case 0x1f80104e:
			hard = sio.BaudReg;
#ifdef PAD_LOG
			PAD_LOG("sio read16 %lx; ret = %x\n", add&0xf, hard);
#endif
			return hard;

		//Serial port stuff not support now ;P
	 // case 0x1f801050: hard = serial_read16(); break;
	 //	case 0x1f801054: hard = serial_status_read(); break;
	 //	case 0x1f80105a: hard = serial_control_read(); break;
	 //	case 0x1f80105e: hard = serial_baud_read(); break;
	
		case 0x1f801100:
			hard = (u16)psxRcntRcount16(0);
#ifdef PSXHW_LOG
			PSXHW_LOG("T0 count read16: %x\n", hard);
#endif
			return hard;
		case 0x1f801104:
			hard = psxCounters[0].mode;
#ifdef PSXHW_LOG
			PSXHW_LOG("T0 mode read16: %x\n", hard);
#endif
			return hard;
		case 0x1f801108:
			hard = psxCounters[0].target;
#ifdef PSXHW_LOG
			PSXHW_LOG("T0 target read16: %x\n", hard);
#endif
			return hard;
		case 0x1f801110:
			hard = (u16)psxRcntRcount16(1);
#ifdef PSXHW_LOG
			PSXHW_LOG("T1 count read16: %x\n", hard);
#endif
			return hard;
		case 0x1f801114:
			hard = psxCounters[1].mode;
#ifdef PSXHW_LOG
			PSXHW_LOG("T1 mode read16: %x\n", hard);
#endif
			return hard;
		case 0x1f801118:
			hard = psxCounters[1].target;
#ifdef PSXHW_LOG
			PSXHW_LOG("T1 target read16: %x\n", hard);
#endif
			return hard;
		case 0x1f801120:
			hard = (u16)psxRcntRcount16(2);
#ifdef PSXHW_LOG
			PSXHW_LOG("T2 count read16: %x\n", hard);
#endif
			return hard;
		case 0x1f801124:
			hard = psxCounters[2].mode;
#ifdef PSXHW_LOG
			PSXHW_LOG("T2 mode read16: %x\n", hard);
#endif
			return hard;
		case 0x1f801128:
			hard = psxCounters[2].target;
#ifdef PSXHW_LOG
			PSXHW_LOG("T2 target read16: %x\n", hard);
#endif
			return hard;

		case 0x1f80146e: // DEV9_R_REV
			return DEV9read16(add);

		case 0x1f801480:
			hard = (u16)psxRcntRcount32(3);
#ifdef PSXHW_LOG
			PSXHW_LOG("T3 count read16: %lx\n", hard);
#endif
			return hard;
		case 0x1f801484:
			hard = psxCounters[3].mode;
#ifdef PSXHW_LOG
			PSXHW_LOG("T3 mode read16: %lx\n", hard);
#endif
			return hard;
		case 0x1f801488:
			hard = psxCounters[3].target;
#ifdef PSXHW_LOG
			PSXHW_LOG("T3 target read16: %lx\n", hard);
#endif
			return hard;
		case 0x1f801490:
			hard = (u16)psxRcntRcount32(4);
#ifdef PSXHW_LOG
			PSXHW_LOG("T4 count read16: %lx\n", hard);
#endif
			return hard;
		case 0x1f801494:
			hard = psxCounters[4].mode;
#ifdef PSXHW_LOG
			PSXHW_LOG("T4 mode read16: %lx\n", hard);
#endif
			return hard;
		case 0x1f801498:
			hard = psxCounters[4].target;
#ifdef PSXHW_LOG
			PSXHW_LOG("T4 target read16: %lx\n", hard);
#endif
			return hard;
		case 0x1f8014a0:
			hard = (u16)psxRcntRcount32(5);
#ifdef PSXHW_LOG
			PSXHW_LOG("T5 count read16: %lx\n", hard);
#endif
			return hard;
		case 0x1f8014a4:
			hard = psxCounters[5].mode;
#ifdef PSXHW_LOG
			PSXHW_LOG("T5 mode read16: %lx\n", hard);
#endif
			return hard;
		case 0x1f8014a8:
			hard = psxCounters[5].target;
#ifdef PSXHW_LOG
			PSXHW_LOG("T5 target read16: %lx\n", hard);
#endif
			return hard;

		case 0x1f801504:
			hard = psxHu16(0x1504);
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA7 BCR_size 16bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801506:
			hard = psxHu16(0x1506);
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA7 BCR_count 16bit read %lx\n", hard);
#endif
			return hard;
		//case 0x1f802030: hard =   //int_2000????
		//case 0x1f802040: hard =//dip switches...??

		default:
			if (add>=0x1f801c00 && add<0x1f801e00) {
            	hard = SPU2read(add);
			} else {
				hard = psxHu16(add); 
#ifdef PSXHW_LOG
				PSXHW_LOG("*Unkwnown 16bit read at address %lx\n", add);
#endif
			}
            return hard;
	}
	
#ifdef PSXHW_LOG
	PSXHW_LOG("*Known 16bit read at address %lx value %x\n", add, hard);
#endif
	return hard;
}

#define CONSTREAD16_CALL(name) { \
	iFlushCall(0); \
	CALLFunc((u32)name); \
	if( sign ) MOVSX32R16toR(EAX, EAX); \
	else MOVZX32R16toR(EAX, EAX); \
} \

void psxConstReadCounterMode16(int x86reg, int index, int sign)
{
	if( IS_MMXREG(x86reg) ) {
		MOV16MtoR(ECX, (u32)&psxCounters[index].mode);
		MOVDMtoMMX(x86reg&0xf, (u32)&psxCounters[index].mode - 2);
	}
	else {
		if( sign ) MOVSX32M16toR(ECX, (u32)&psxCounters[index].mode);
		else MOVZX32M16toR(ECX, (u32)&psxCounters[index].mode);

		MOV32RtoR(x86reg, ECX);
	}
	
	//AND16ItoR(ECX, ~0x1800);
	//OR16ItoR(ECX, 0x400);
	MOV16RtoM(psxCounters[index].mode, ECX);
}

int psxHwConstRead16(u32 x86reg, u32 add, u32 sign) {
	if (add >= 0x1f801600 && add < 0x1f801700) {
		PUSH32I(add);
		CONSTREAD16_CALL(USBread16);
		return 1;
	}

	switch (add) {

		case 0x1f801040:
			iFlushCall(0);
			CALLFunc((u32)sioRead8);
			PUSH32R(EAX);
			CALLFunc((u32)sioRead8);
			POP32R(ECX);
			AND32ItoR(ECX, 0xff);
			SHL32ItoR(EAX, 8);
			OR32RtoR(EAX, ECX);
			if( sign ) MOVSX32R16toR(EAX, EAX);
			else MOVZX32R16toR(EAX, EAX);
			return 1;

		case 0x1f801044:
			_eeReadConstMem16(x86reg, (u32)&sio.StatReg, sign);
			return 0;

		case 0x1f801048:
			_eeReadConstMem16(x86reg, (u32)&sio.ModeReg, sign);
			return 0;

		case 0x1f80104a:
			_eeReadConstMem16(x86reg, (u32)&sio.CtrlReg, sign);
			return 0;

		case 0x1f80104e:
			_eeReadConstMem16(x86reg, (u32)&sio.BaudReg, sign);
			return 0;

		// counters[0]
		case 0x1f801100:
			PUSH32I(0);
			CONSTREAD16_CALL(psxRcntRcount16);
			ADD32ItoR(ESP, 4);
			return 1;
		case 0x1f801104:
			psxConstReadCounterMode16(x86reg, 0, sign);
			return 0;

		case 0x1f801108:
			_eeReadConstMem16(x86reg, (u32)&psxCounters[0].target, sign);
			return 0;

		// counters[1]
		case 0x1f801110:
			PUSH32I(1);
			CONSTREAD16_CALL(psxRcntRcount16);
			ADD32ItoR(ESP, 4);
			return 1;
		case 0x1f801114:
			psxConstReadCounterMode16(x86reg, 1, sign);
			return 0;

		case 0x1f801118:
			_eeReadConstMem16(x86reg, (u32)&psxCounters[1].target, sign);
			return 0;

		// counters[2]
		case 0x1f801120:
			PUSH32I(2);
			CONSTREAD16_CALL(psxRcntRcount16);
			ADD32ItoR(ESP, 4);
			return 1;
		case 0x1f801124:
			psxConstReadCounterMode16(x86reg, 2, sign);
			return 0;

		case 0x1f801128:
			_eeReadConstMem16(x86reg, (u32)&psxCounters[2].target, sign);
			return 0;

		case 0x1f80146e: // DEV9_R_REV
			PUSH32I(add);
			CONSTREAD16_CALL(DEV9read16);
			return 1;

		// counters[3]
		case 0x1f801480:
			PUSH32I(3);
			CONSTREAD16_CALL(psxRcntRcount32);
			ADD32ItoR(ESP, 4);
			return 1;

		case 0x1f801484:
			psxConstReadCounterMode16(x86reg, 3, sign);
			return 0;

		case 0x1f801488:
			_eeReadConstMem16(x86reg, (u32)&psxCounters[3].target, sign);
			return 0;

		// counters[4]
		case 0x1f801490:
			PUSH32I(4);
			CONSTREAD16_CALL(psxRcntRcount32);
			ADD32ItoR(ESP, 4);
			return 1;

		case 0x1f801494:
			psxConstReadCounterMode16(x86reg, 4, sign);
			return 0;
			
		case 0x1f801498:
			_eeReadConstMem16(x86reg, (u32)&psxCounters[4].target, sign);
			return 0;

		// counters[5]
		case 0x1f8014a0:
			PUSH32I(5);
			CONSTREAD16_CALL(psxRcntRcount32);
			ADD32ItoR(ESP, 4);
			return 1;

		case 0x1f8014a4:
			psxConstReadCounterMode16(x86reg, 5, sign);
			return 0;

		case 0x1f8014a8:
			_eeReadConstMem16(x86reg, (u32)&psxCounters[5].target, sign);
			return 0;

		default:			
			if (add>=0x1f801c00 && add<0x1f801e00) {
			
				PUSH32I(add);
				CONSTREAD16_CALL(SPU2read);
				return 1;
			} else {
				_eeReadConstMem16(x86reg, (u32)&psxH[(add) & 0xffff], sign);
				return 0;
			}
	}
}

u32 psxHwRead32(u32 add) {
	u32 hard;

	if (add >= 0x1f801600 && add < 0x1f801700) {
		return USBread32(add);
	}
	if (add >= 0x1f808400 && add <= 0x1f808550) {//the size is a complete guess..
		return FWread32(add);
	}

	switch (add) {
		case 0x1f801040:
			hard = sioRead8();
			hard|= sioRead8() << 8;
			hard|= sioRead8() << 16;
			hard|= sioRead8() << 24;
#ifdef PAD_LOG
			PAD_LOG("sio read32 ;ret = %lx\n", hard);
#endif
			return hard;
			
	//	case 0x1f801050: hard = serial_read32(); break;//serial port
#ifdef PSXHW_LOG
		case 0x1f801060:
			PSXHW_LOG("RAM size read %lx\n", psxHu32(0x1060));
			return psxHu32(0x1060);
#endif
#ifdef PSXHW_LOG
		case 0x1f801070: PSXHW_LOG("IREG 32bit read %x\n", psxHu32(0x1070));
			return psxHu32(0x1070);
#endif
#ifdef PSXHW_LOG
		case 0x1f801074: PSXHW_LOG("IMASK 32bit read %x\n", psxHu32(0x1074));
			return psxHu32(0x1074);
#endif
		case 0x1f801078:
#ifdef PSXHW_LOG
			PSXHW_LOG("ICTRL 32bit read %x\n", psxHu32(0x1078));
#endif
			hard = psxHu32(0x1078);
			psxHu32(0x1078) = 0;
			return hard;

/*		case 0x1f801810:
//			hard = GPU_readData();
#ifdef PSXHW_LOG
			PSXHW_LOG("GPU DATA 32bit read %lx\n", hard);
#endif
			return hard;*/
/*		case 0x1f801814:
			hard = GPU_readStatus();
#ifdef PSXHW_LOG
			PSXHW_LOG("GPU STATUS 32bit read %lx\n", hard);
#endif
			return hard;
*/
/*		case 0x1f801820: hard = mdecRead0(); break;
		case 0x1f801824: hard = mdecRead1(); break;
*/
#ifdef PSXHW_LOG
		case 0x1f8010a0:
			PSXHW_LOG("DMA2 MADR 32bit read %lx\n", psxHu32(0x10a0));
			return HW_DMA2_MADR;
		case 0x1f8010a4:
			PSXHW_LOG("DMA2 BCR 32bit read %lx\n", psxHu32(0x10a4));
			return HW_DMA2_BCR;
		case 0x1f8010a8:
			PSXHW_LOG("DMA2 CHCR 32bit read %lx\n", psxHu32(0x10a8));
			return HW_DMA2_CHCR;
#endif

#ifdef PSXHW_LOG
		case 0x1f8010b0:
			PSXHW_LOG("DMA3 MADR 32bit read %lx\n", psxHu32(0x10b0));
			return HW_DMA3_MADR;
		case 0x1f8010b4:
			PSXHW_LOG("DMA3 BCR 32bit read %lx\n", psxHu32(0x10b4));
			return HW_DMA3_BCR;
		case 0x1f8010b8:
			PSXHW_LOG("DMA3 CHCR 32bit read %lx\n", psxHu32(0x10b8));
			return HW_DMA3_CHCR;
#endif

#ifdef PSXHW_LOG
		case 0x1f801520:
			PSXHW_LOG("DMA9 MADR 32bit read %lx\n", HW_DMA9_MADR);
			return HW_DMA9_MADR;
		case 0x1f801524:
			PSXHW_LOG("DMA9 BCR 32bit read %lx\n", HW_DMA9_BCR);
			return HW_DMA9_BCR;
		case 0x1f801528:
			PSXHW_LOG("DMA9 CHCR 32bit read %lx\n", HW_DMA9_CHCR);
			return HW_DMA9_CHCR;
		case 0x1f80152C:
			PSXHW_LOG("DMA9 TADR 32bit read %lx\n", HW_DMA9_TADR);
			return HW_DMA9_TADR;
#endif

#ifdef PSXHW_LOG
		case 0x1f801530:
			PSXHW_LOG("DMA10 MADR 32bit read %lx\n", HW_DMA10_MADR);
			return HW_DMA10_MADR;
		case 0x1f801534:
			PSXHW_LOG("DMA10 BCR 32bit read %lx\n", HW_DMA10_BCR);
			return HW_DMA10_BCR;
		case 0x1f801538:
			PSXHW_LOG("DMA10 CHCR 32bit read %lx\n", HW_DMA10_CHCR);
			return HW_DMA10_CHCR;
#endif

//		case 0x1f8010f0:  PSXHW_LOG("DMA PCR 32bit read " << psxHu32(0x10f0));
//			return HW_DMA_PCR; // dma rest channel
#ifdef PSXHW_LOG
		case 0x1f8010f4:
			PSXHW_LOG("DMA ICR 32bit read %lx\n", HW_DMA_ICR);
			return HW_DMA_ICR;
#endif
//SSBus registers
		case 0x1f801000:
			hard = psxHu32(0x1000);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <spd_addr> 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801004:
			hard = psxHu32(0x1004);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <pio_addr> 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801008:
			hard = psxHu32(0x1008);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <spd_delay> 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f80100C:
			hard = psxHu32(0x100C);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS dev1_delay 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801010:
			hard = psxHu32(0x1010);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS rom_delay 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801014:
			hard = psxHu32(0x1014);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS spu_delay 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801018:
			hard = psxHu32(0x1018);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS dev5_delay 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f80101C:
			hard = psxHu32(0x101C);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <pio_delay> 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801020:
			hard = psxHu32(0x1020);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS com_delay 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801400:
			hard = psxHu32(0x1400);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS dev1_addr 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801404:
			hard = psxHu32(0x1404);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS spu_addr 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801408:
			hard = psxHu32(0x1408);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS dev5_addr 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f80140C:
			hard = psxHu32(0x140C);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS spu1_addr 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801410:
			hard = psxHu32(0x1410);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <dev9_addr3> 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801414:
			hard = psxHu32(0x1414);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS spu1_delay 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801418:
			hard = psxHu32(0x1418);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <dev9_delay2> 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f80141C:
			hard = psxHu32(0x141C);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <dev9_delay3> 32bit read %lx\n", hard);
#endif
			return hard;
		case 0x1f801420:
			hard = psxHu32(0x1420);
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <dev9_delay1> 32bit read %lx\n", hard);
#endif
			return hard;

		case 0x1f8010f0:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA PCR 32bit read %lx\n", HW_DMA_PCR);
#endif
			return HW_DMA_PCR;

		case 0x1f8010c8:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA4 CHCR 32bit read %lx\n", HW_DMA4_CHCR);
#endif
		 return HW_DMA4_CHCR;       // DMA4 chcr (SPU DMA)
			
		// time for rootcounters :)
		case 0x1f801100:
			hard = (u16)psxRcntRcount16(0);
#ifdef PSXHW_LOG
			PSXHW_LOG("T0 count read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f801104:
			hard = (u16)psxCounters[0].mode;
#ifdef PSXHW_LOG
			PSXHW_LOG("T0 mode read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f801108:
			hard = psxCounters[0].target;
#ifdef PSXHW_LOG
			PSXHW_LOG("T0 target read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f801110:
			hard = (u16)psxRcntRcount16(1);
#ifdef PSXHW_LOG
			PSXHW_LOG("T1 count read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f801114:
			hard = (u16)psxCounters[1].mode;
#ifdef PSXHW_LOG
			PSXHW_LOG("T1 mode read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f801118:
			hard = psxCounters[1].target;
#ifdef PSXHW_LOG
			PSXHW_LOG("T1 target read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f801120:
			hard = (u16)psxRcntRcount16(2);
#ifdef PSXHW_LOG
			PSXHW_LOG("T2 count read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f801124:
			hard = (u16)psxCounters[2].mode;
#ifdef PSXHW_LOG
			PSXHW_LOG("T2 mode read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f801128:
			hard = psxCounters[2].target;
#ifdef PSXHW_LOG
			PSXHW_LOG("T2 target read32: %lx\n", hard);
#endif
			return hard;

		case 0x1f801480:
			hard = (u32)psxRcntRcount32(3);
#ifdef PSXHW_LOG
			PSXHW_LOG("T3 count read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f801484:
			hard = (u16)psxCounters[3].mode;
#ifdef PSXHW_LOG
			PSXHW_LOG("T3 mode read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f801488:
			hard = psxCounters[3].target;
#ifdef PSXHW_LOG
			PSXHW_LOG("T3 target read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f801490:
			hard = (u32)psxRcntRcount32(4);
#ifdef PSXHW_LOG
			PSXHW_LOG("T4 count read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f801494:
			hard = (u16)psxCounters[4].mode;
#ifdef PSXHW_LOG
			PSXHW_LOG("T4 mode read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f801498:
			hard = psxCounters[4].target;
#ifdef PSXHW_LOG
			PSXHW_LOG("T4 target read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f8014a0:
			hard = (u32)psxRcntRcount32(5);
#ifdef PSXHW_LOG
			PSXHW_LOG("T5 count read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f8014a4:
			hard = (u16)psxCounters[5].mode;
#ifdef PSXHW_LOG
			PSXHW_LOG("T5 mode read32: %lx\n", hard);
#endif
			return hard;
		case 0x1f8014a8:
			hard = psxCounters[5].target;
#ifdef PSXHW_LOG
			PSXHW_LOG("T5 target read32: %lx\n", hard);
#endif
			return hard;

		case 0x1f801450:
			hard = psxHu32(add);
#ifdef PSXHW_LOG
			PSXHW_LOG("%08X ICFG 32bit read %x\n", psxRegs.pc, hard);
#endif
			return hard;


		case 0x1F8010C0:
			HW_DMA4_MADR = SPU2ReadMemAddr(0);
			return HW_DMA4_MADR;

		case 0x1f801500:
			HW_DMA7_MADR = SPU2ReadMemAddr(1);
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA7 MADR 32bit read %lx\n", HW_DMA7_MADR);
#endif
			return HW_DMA7_MADR;  // DMA7 madr
		case 0x1f801504:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA7 BCR 32bit read %lx\n", HW_DMA7_BCR);
#endif
			return HW_DMA7_BCR; // DMA7 bcr

		case 0x1f801508:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA7 CHCR 32bit read %lx\n", HW_DMA7_CHCR);
#endif
			return HW_DMA7_CHCR;         // DMA7 chcr (SPU2)		

		case 0x1f801570:
			hard = psxHu32(0x1570);
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA PCR2 32bit read %lx\n", hard);
#endif
			return hard;
#ifdef PSXHW_LOG
		case 0x1f801574:
			PSXHW_LOG("DMA ICR2 32bit read %lx\n", HW_DMA_ICR2);
			return HW_DMA_ICR2;
#endif

		case 0x1F808200:
		case 0x1F808204:
		case 0x1F808208:
		case 0x1F80820C:
		case 0x1F808210:
		case 0x1F808214:
		case 0x1F808218:
		case 0x1F80821C:
		case 0x1F808220:
		case 0x1F808224:
		case 0x1F808228:
		case 0x1F80822C:
		case 0x1F808230:
		case 0x1F808234:
		case 0x1F808238:
		case 0x1F80823C:
			hard=sio2_getSend3((add-0x1F808200)/4);
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 read param[%d] (%lx)\n", (add-0x1F808200)/4, hard);
#endif
			return hard;

		case 0x1F808240:
		case 0x1F808248:
		case 0x1F808250:
		case 0x1F80825C:
			hard=sio2_getSend1((add-0x1F808240)/8);
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 read send1[%d] (%lx)\n", (add-0x1F808240)/8, hard);
#endif
			return hard;
		
		case 0x1F808244:
		case 0x1F80824C:
		case 0x1F808254:
		case 0x1F808258:
			hard=sio2_getSend2((add-0x1F808244)/8);
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 read send2[%d] (%lx)\n", (add-0x1F808244)/8, hard);
#endif
			return hard;

		case 0x1F808268:
			hard=sio2_getCtrl();
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 read CTRL (%lx)\n", hard);
#endif
			return hard;

		case 0x1F80826C:
			hard=sio2_getRecv1();
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 read Recv1 (%lx)\n", hard);
#endif
			return hard;

		case 0x1F808270:
			hard=sio2_getRecv2();
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 read Recv2 (%lx)\n", hard);
#endif
			return hard;

		case 0x1F808274:
			hard=sio2_getRecv3();
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 read Recv3 (%lx)\n", hard);
#endif
			return hard;

		case 0x1F808278:
			hard=sio2_get8278();
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 read [8278] (%lx)\n", hard);
#endif
			return hard;

		case 0x1F80827C:
			hard=sio2_get827C();
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 read [827C] (%lx)\n", hard);
#endif
			return hard;

		case 0x1F808280:
			hard=sio2_getIntr();
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 read INTR (%lx)\n", hard);
#endif
			return hard;

		default:
			hard = psxHu32(add); 
#ifdef PSXHW_LOG
			PSXHW_LOG("*Unknown 32bit read at address %lx: %lx\n", add, hard);
#endif
			return hard;
	}
#ifdef PSXHW_LOG
	PSXHW_LOG("*Known 32bit read at address %lx: %lx\n", add, hard);
#endif
	return hard;
}

void psxConstReadCounterMode32(int x86reg, int index)
{
	if( IS_MMXREG(x86reg) ) {
		MOV16MtoR(ECX, (u32)&psxCounters[index].mode);
		MOVDMtoMMX(x86reg&0xf, (u32)&psxCounters[index].mode);
	}
	else {
		MOVZX32M16toR(ECX, (u32)&psxCounters[index].mode);
		MOV32RtoR(x86reg, ECX);
	}

	//AND16ItoR(ECX, ~0x1800);
	//OR16ItoR(ECX, 0x400);
	MOV16RtoM(psxCounters[index].mode, ECX);
}

static u32 s_tempsio;
int psxHwConstRead32(u32 x86reg, u32 add) {
	if (add >= 0x1f801600 && add < 0x1f801700) {
		iFlushCall(0);
		PUSH32I(add);
		CALLFunc((u32)USBread32);
		return 1;
	}
	if (add >= 0x1f808400 && add <= 0x1f808550) {//the size is a complete guess..
		iFlushCall(0);
		PUSH32I(add);
		CALLFunc((u32)FWread32);
		return 1;
	}

	switch (add) {
		case 0x1f801040:
			iFlushCall(0);
			CALLFunc((u32)sioRead8);
			AND32ItoR(EAX, 0xff);
			MOV32RtoM((u32)&s_tempsio, EAX);
			CALLFunc((u32)sioRead8);
			AND32ItoR(EAX, 0xff);
			SHL32ItoR(EAX, 8);
			OR32RtoM((u32)&s_tempsio, EAX);

			// 3rd
			CALLFunc((u32)sioRead8);
			AND32ItoR(EAX, 0xff);
			SHL32ItoR(EAX, 16);
			OR32RtoM((u32)&s_tempsio, EAX);

			// 4th
			CALLFunc((u32)sioRead8);
			SHL32ItoR(EAX, 24);
			OR32MtoR(EAX, (u32)&s_tempsio);
			return 1;
			
		//case 0x1f801050: hard = serial_read32(); break;//serial port
		case 0x1f801078:
#ifdef PSXHW_LOG
			PSXHW_LOG("ICTRL 32bit read %x\n", psxHu32(0x1078));
#endif
			_eeReadConstMem32(x86reg, (u32)&psxH[add&0xffff]);
			MOV32ItoM((u32)&psxH[add&0xffff], 0);
			return 0;
		
			// counters[0]
		case 0x1f801100:
			iFlushCall(0);
			PUSH32I(0);
			CALLFunc((u32)psxRcntRcount16);
			ADD32ItoR(ESP, 4);
			return 1;
		case 0x1f801104:
			psxConstReadCounterMode32(x86reg, 0);
			return 0;

		case 0x1f801108:
			_eeReadConstMem32(x86reg, (u32)&psxCounters[0].target);
			return 0;

		// counters[1]
		case 0x1f801110:
			iFlushCall(0);
			PUSH32I(1);
			CALLFunc((u32)psxRcntRcount16);
			ADD32ItoR(ESP, 4);
			return 1;
		case 0x1f801114:
			psxConstReadCounterMode32(x86reg, 1);
			return 0;

		case 0x1f801118:
			_eeReadConstMem32(x86reg, (u32)&psxCounters[1].target);
			return 0;

		// counters[2]
		case 0x1f801120:
			iFlushCall(0);
			PUSH32I(2);
			CALLFunc((u32)psxRcntRcount16);
			ADD32ItoR(ESP, 4);
			return 1;
		case 0x1f801124:
			psxConstReadCounterMode32(x86reg, 2);
			return 0;

		case 0x1f801128:
			_eeReadConstMem32(x86reg, (u32)&psxCounters[2].target);
			return 0;

		// counters[3]
		case 0x1f801480:
			iFlushCall(0);
			PUSH32I(3);
			CALLFunc((u32)psxRcntRcount32);
			ADD32ItoR(ESP, 4);
			return 1;

		case 0x1f801484:
			psxConstReadCounterMode32(x86reg, 3);
			return 0;

		case 0x1f801488:
			_eeReadConstMem32(x86reg, (u32)&psxCounters[3].target);
			return 0;

		// counters[4]
		case 0x1f801490:
			iFlushCall(0);
			PUSH32I(4);
			CALLFunc((u32)psxRcntRcount32);
			ADD32ItoR(ESP, 4);
			return 1;

		case 0x1f801494:
			psxConstReadCounterMode32(x86reg, 4);
			return 0;
			
		case 0x1f801498:
			_eeReadConstMem32(x86reg, (u32)&psxCounters[4].target);
			return 0;

		// counters[5]
		case 0x1f8014a0:
			iFlushCall(0);
			PUSH32I(5);
			CALLFunc((u32)psxRcntRcount32);
			ADD32ItoR(ESP, 4);
			return 1;

		case 0x1f8014a4:
			psxConstReadCounterMode32(x86reg, 5);
			return 0;

		case 0x1f8014a8:
			_eeReadConstMem32(x86reg, (u32)&psxCounters[5].target);
			return 0;

		case 0x1F808200:
		case 0x1F808204:
		case 0x1F808208:
		case 0x1F80820C:
		case 0x1F808210:
		case 0x1F808214:
		case 0x1F808218:
		case 0x1F80821C:
		case 0x1F808220:
		case 0x1F808224:
		case 0x1F808228:
		case 0x1F80822C:
		case 0x1F808230:
		case 0x1F808234:
		case 0x1F808238:
		case 0x1F80823C:
			iFlushCall(0);
			PUSH32I((add-0x1F808200)/4);
			CALLFunc((u32)sio2_getSend3);
			ADD32ItoR(ESP, 4);
			return 1;

		case 0x1F808240:
		case 0x1F808248:
		case 0x1F808250:
		case 0x1F80825C:
			iFlushCall(0);
			PUSH32I((add-0x1F808240)/8);
			CALLFunc((u32)sio2_getSend1);
			ADD32ItoR(ESP, 4);
			return 1;
		
		case 0x1F808244:
		case 0x1F80824C:
		case 0x1F808254:
		case 0x1F808258:
			iFlushCall(0);
			PUSH32I((add-0x1F808244)/8);
			CALLFunc((u32)sio2_getSend2);
			ADD32ItoR(ESP, 4);
			return 1;

		case 0x1F808268:
			iFlushCall(0);
			CALLFunc((u32)sio2_getCtrl);
			return 1;
			
		case 0x1F80826C:
			iFlushCall(0);
			CALLFunc((u32)sio2_getRecv1);
			return 1;

		case 0x1F808270:
			iFlushCall(0);
			CALLFunc((u32)sio2_getRecv2);
			return 1;

		case 0x1F808274:
			iFlushCall(0);
			CALLFunc((u32)sio2_getRecv3);
			return 1;

		case 0x1F808278:
			iFlushCall(0);
			CALLFunc((u32)sio2_get8278);
			return 1;

		case 0x1F80827C:
			iFlushCall(0);
			CALLFunc((u32)sio2_get827C);
			return 1;

		case 0x1F808280:
			iFlushCall(0);
			CALLFunc((u32)sio2_getIntr);
			return 1;

		case 0x1F801C00:
			iFlushCall(0);
			CALLFunc((u32)SPU2ReadMemAddr(0));
			return 1;

		case 0x1F801500:
			iFlushCall(0);
			CALLFunc((u32)SPU2ReadMemAddr(1));
			return 1;

		default:
			_eeReadConstMem32(x86reg, (u32)&psxH[(add) & 0xffff]);
			return 0;
	}
}

static int pbufi;
static s8 pbuf[1024];

#define DmaExec(n) { \
	if (HW_DMA##n##_CHCR & 0x01000000 && \
		HW_DMA_PCR & (8 << (n * 4))) { \
		psxDma##n(HW_DMA##n##_MADR, HW_DMA##n##_BCR, HW_DMA##n##_CHCR); \
	} \
}

void psxHwWrite8(u32 add, u8 value) {
	if (add >= 0x1f801600 && add < 0x1f801700) {
		USBwrite8(add, value); return;
	}
#ifdef PCSX2_DEVBUILD
	if((add & 0xf) == 0xa) SysPrintf("8bit write (possible chcr set) %x value %x\n", add, value);
#endif
	switch (add) {
		case 0x1f801040: sioWrite8(value); break;
	//	case 0x1f801050: serial_write8(value); break;//serial port

		case 0x1f801100:
		case 0x1f801104:
		case 0x1f801108:
		case 0x1f801110:
		case 0x1f801114:
		case 0x1f801118:
		case 0x1f801120:
		case 0x1f801124:
		case 0x1f801128:
		case 0x1f801480:
		case 0x1f801484:
		case 0x1f801488:
		case 0x1f801490:
		case 0x1f801494:
		case 0x1f801498:
		case 0x1f8014a0:
		case 0x1f8014a4:
		case 0x1f8014a8:
			SysPrintf("8bit counter write %x\n", add);
			psxHu8(add) = value;
			return;
		case 0x1f801450:
#ifdef PSXHW_LOG
			if (value) { PSXHW_LOG("%08X ICFG 8bit write %lx\n", psxRegs.pc, value); }
#endif
			psxHu8(0x1450) = value;
			return;

		case 0x1f801800: cdrWrite0(value); break;
		case 0x1f801801: cdrWrite1(value); break;
		case 0x1f801802: cdrWrite2(value); break;
		case 0x1f801803: cdrWrite3(value); break;

		case 0x1f80380c:
			if (value == '\r') break;
			if (value == '\n' || pbufi >= 1023) {
				pbuf[pbufi++] = 0; pbufi = 0;
				SysPrintf("%s\n", pbuf); break;
			}
			pbuf[pbufi++] = value;
			break;

		case 0x1F808260:
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 write8 DATAIN <- %08X\n", value);
#endif
			sio2_fifoIn(value);return;//serial data feed/fifo

		default:
			psxHu8(add) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("*Unknown 8bit write at address %lx value %x\n", add, value);
#endif
			return;
	}
	psxHu8(add) = value;
#ifdef PSXHW_LOG
	PSXHW_LOG("*Known 8bit write at address %lx value %x\n", add, value);
#endif
}

#define CONSTWRITE_CALL(name) { \
	_recPushReg(mmreg); \
	iFlushCall(0); \
	CALLFunc((u32)name); \
	ADD32ItoR(ESP, 4); \
} \

void Write8PrintBuffer(u8 value)
{
	if (value == '\r') return;
	if (value == '\n' || pbufi >= 1023) {
		pbuf[pbufi++] = 0; pbufi = 0;
		SysPrintf("%s\n", pbuf); return;
	}
	pbuf[pbufi++] = value;
}

void psxHwConstWrite8(u32 add, int mmreg)
{
	if (add >= 0x1f801600 && add < 0x1f801700) {
		_recPushReg(mmreg);
		iFlushCall(0);
		PUSH32I(add);
		CALLFunc((u32)USBwrite8);
		return;
	}

	switch (add) {
		case 0x1f801040:
			CONSTWRITE_CALL(sioWrite8); break;
		//case 0x1f801050: serial_write8(value); break;//serial port
		case 0x1f801100:
		case 0x1f801104:
		case 0x1f801108:
		case 0x1f801110:
		case 0x1f801114:
		case 0x1f801118:
		case 0x1f801120:
		case 0x1f801124:
		case 0x1f801128:
		case 0x1f801480:
		case 0x1f801484:
		case 0x1f801488:
		case 0x1f801490:
		case 0x1f801494:
		case 0x1f801498:
		case 0x1f8014a0:
		case 0x1f8014a4:
		case 0x1f8014a8:
			SysPrintf("8bit counter write %x\n", add);
			_eeWriteConstMem8((u32)&psxH[(add) & 0xffff], mmreg);
			return;
		case 0x1f801800: CONSTWRITE_CALL(cdrWrite0); break;
		case 0x1f801801: CONSTWRITE_CALL(cdrWrite1); break;
		case 0x1f801802: CONSTWRITE_CALL(cdrWrite2); break;
		case 0x1f801803: CONSTWRITE_CALL(cdrWrite3); break;
		case 0x1f80380c: CONSTWRITE_CALL(Write8PrintBuffer); break;
		case 0x1F808260: CONSTWRITE_CALL(sio2_fifoIn); break;

		default:
			_eeWriteConstMem8((u32)&psxH[(add) & 0xffff], mmreg);
			return;
	}
}

void psxHwWrite16(u32 add, u16 value) {
	if (add >= 0x1f801600 && add < 0x1f801700) {
		USBwrite16(add, value); return;
	}
#ifdef PCSX2_DEVBUILD
	if((add & 0xf) == 0x9) SysPrintf("16bit write (possible chcr set) %x value %x\n", add, value);
#endif
	switch (add) {
		case 0x1f801040:
			sioWrite8((u8)value);
			sioWrite8((u8)(value>>8));
#ifdef PAD_LOG
			PAD_LOG ("sio write16 %lx, %x\n", add&0xf, value);
#endif
			return;
		case 0x1f801044:
#ifdef PAD_LOG
			PAD_LOG ("sio write16 %lx, %x\n", add&0xf, value);
#endif
			return;
		case 0x1f801048:
			sio.ModeReg = value;
#ifdef PAD_LOG
			PAD_LOG ("sio write16 %lx, %x\n", add&0xf, value);
#endif
			return;
		case 0x1f80104a: // control register
			sioWriteCtrl16(value);
#ifdef PAD_LOG
			PAD_LOG ("sio write16 %lx, %x\n", add&0xf, value);
#endif
			return;
		case 0x1f80104e: // baudrate register
			sio.BaudReg = value;
#ifdef PAD_LOG
			PAD_LOG ("sio write16 %lx, %x\n", add&0xf, value);
#endif
			return;

		//serial port ;P
	//  case 0x1f801050: serial_write16(value); break;
	//	case 0x1f80105a: serial_control_write(value);break;
	//	case 0x1f80105e: serial_baud_write(value); break;
	//	case 0x1f801054: serial_status_write(value); break;

		case 0x1f801070: 
#ifdef PSXHW_LOG
			PSXHW_LOG("IREG 16bit write %x\n", value);
#endif
//			if (Config.Sio) psxHu16(0x1070) |= 0x80;
//			if (Config.SpuIrq) psxHu16(0x1070) |= 0x200;
			psxHu16(0x1070) &= value;
			return;
#ifdef PSXHW_LOG
		case 0x1f801074: PSXHW_LOG("IMASK 16bit write %x\n", value);
			psxHu16(0x1074) = value;
			return;
#endif

		case 0x1f8010c4:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA4 BCR_size 16bit write %lx\n", value);
#endif
			psxHu16(0x10c4) = value; return; // DMA4 bcr_size

		case 0x1f8010c6:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA4 BCR_count 16bit write %lx\n", value);
#endif
			psxHu16(0x10c6) = value; return; // DMA4 bcr_count

		case 0x1f801100:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 0 COUNT 16bit write %x\n", value);
#endif
			psxRcntWcount16(0, value); return;
		case 0x1f801104:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 0 MODE 16bit write %x\n", value);
#endif
			psxRcnt0Wmode(value); return;
		case 0x1f801108:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 0 TARGET 16bit write %x\n", value);
#endif
			psxRcntWtarget16(0, value); return;

		case 0x1f801110:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 1 COUNT 16bit write %x\n", value);
#endif
			psxRcntWcount16(1, value); return;
		case 0x1f801114:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 1 MODE 16bit write %x\n", value);
#endif
			psxRcnt1Wmode(value); return;
		case 0x1f801118:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 1 TARGET 16bit write %x\n", value);
#endif
			psxRcntWtarget16(1, value); return;

		case 0x1f801120:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 2 COUNT 16bit write %x\n", value);
#endif
			psxRcntWcount16(2, value); return;
		case 0x1f801124:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 2 MODE 16bit write %x\n", value);
#endif
			psxRcnt2Wmode(value); return;
		case 0x1f801128:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 2 TARGET 16bit write %x\n", value);
#endif
			psxRcntWtarget16(2, value); return;

		case 0x1f801450:
#ifdef PSXHW_LOG
			if (value) { PSXHW_LOG("%08X ICFG 16bit write %lx\n", psxRegs.pc, value); }
#endif
			psxHu16(0x1450) = value/* & (~0x8)*/;
			return;

		case 0x1f801480:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 3 COUNT 16bit write %lx\n", value);
#endif
			psxRcntWcount32(3, value); return;
		case 0x1f801484:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 3 MODE 16bit write %lx\n", value);
#endif
			psxRcnt3Wmode(value); return;
		case 0x1f801488:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 3 TARGET 16bit write %lx\n", value);
#endif
			psxRcntWtarget32(3, value); return;

		case 0x1f801490:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 4 COUNT 16bit write %lx\n", value);
#endif
			psxRcntWcount32(4, value); return;
		case 0x1f801494:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 4 MODE 16bit write %lx\n", value);
#endif
			psxRcnt4Wmode(value); return;
		case 0x1f801498:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 4 TARGET 16bit write %lx\n", value);
#endif
			psxRcntWtarget32(4, value); return;

		case 0x1f8014a0:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 5 COUNT 16bit write %lx\n", value);
#endif
			psxRcntWcount32(5, value); return;
		case 0x1f8014a4:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 5 MODE 16bit write %lx\n", value);
#endif
			psxRcnt5Wmode(value); return;
		case 0x1f8014a8:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 5 TARGET 16bit write %lx\n", value);
#endif
			psxRcntWtarget32(5, value); return;

		case 0x1f801504:
			psxHu16(0x1504) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA7 BCR_size 16bit write %lx\n", value);
#endif
			return;
		case 0x1f801506:
			psxHu16(0x1506) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA7 BCR_count 16bit write %lx\n", value);
#endif
			return;
		default:
			if (add>=0x1f801c00 && add<0x1f801e00) {
				SPU2write(add, value);
				return;
			}

			psxHu16(add) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("*Unknown 16bit write at address %lx value %x\n", add, value);
#endif
			return;
	}
	psxHu16(add) = value;
#ifdef PSXHW_LOG
	PSXHW_LOG("*Known 16bit write at address %lx value %x\n", add, value);
#endif
}

void psxHwConstWrite16(u32 add, int mmreg) {
	if (add >= 0x1f801600 && add < 0x1f801700) {
		_recPushReg(mmreg);
		iFlushCall(0);
		PUSH32I(add);
		CALLFunc((u32)USBwrite16);
		return;
	}

	switch (add) {
		case 0x1f801040:
			_recPushReg(mmreg);
			iFlushCall(0);
			CALLFunc((u32)sioWrite8);
			ADD32ItoR(ESP, 1);
			CALLFunc((u32)sioWrite8);
			ADD32ItoR(ESP, 3);
			return;
		case 0x1f801044:
			return;
		case 0x1f801048:
			_eeWriteConstMem16((u32)&sio.ModeReg, mmreg);
			return;
		case 0x1f80104a: // control register
			CONSTWRITE_CALL(sioWriteCtrl16);
			return;
		case 0x1f80104e: // baudrate register
			_eeWriteConstMem16((u32)&sio.BaudReg, mmreg);
			return;

		case 0x1f801070: 
			_eeWriteConstMem16OP((u32)&psxH[(add) & 0xffff], mmreg, 0);
			return;

		// counters[0]
		case 0x1f801100:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(0);
			CALLFunc((u32)psxRcntWcount16);
			ADD32ItoR(ESP, 8);
			return;
		case 0x1f801104:
			CONSTWRITE_CALL(psxRcnt0Wmode);
			return;
		case 0x1f801108:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(0);
			CALLFunc((u32)psxRcntWtarget16);
			ADD32ItoR(ESP, 8);
			return;

		// counters[1]
		case 0x1f801110:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(1);
			CALLFunc((u32)psxRcntWcount16);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1f801114:
			CONSTWRITE_CALL(psxRcnt1Wmode);
			return;

		case 0x1f801118:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(1);
			CALLFunc((u32)psxRcntWtarget16);
			ADD32ItoR(ESP, 8);
			return;

		// counters[2]
		case 0x1f801120:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(2);
			CALLFunc((u32)psxRcntWcount16);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1f801124:
			CONSTWRITE_CALL(psxRcnt2Wmode);
			return;

		case 0x1f801128:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(2);
			CALLFunc((u32)psxRcntWtarget16);
			ADD32ItoR(ESP, 8);
			return;

		// counters[3]
		case 0x1f801480:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(3);
			CALLFunc((u32)psxRcntWcount32);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1f801484:
			CONSTWRITE_CALL(psxRcnt3Wmode);
			return;

		case 0x1f801488:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(3);
			CALLFunc((u32)psxRcntWtarget32);
			ADD32ItoR(ESP, 8);
			return;

		// counters[4]
		case 0x1f801490:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(4);
			CALLFunc((u32)psxRcntWcount32);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1f801494:
			CONSTWRITE_CALL(psxRcnt4Wmode);
			return;

		case 0x1f801498:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(4);
			CALLFunc((u32)psxRcntWtarget32);
			ADD32ItoR(ESP, 8);
			return;

		// counters[5]
		case 0x1f8014a0:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(5);
			CALLFunc((u32)psxRcntWcount32);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1f8014a4:
			CONSTWRITE_CALL(psxRcnt5Wmode);
			return;

		case 0x1f8014a8:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(5);
			CALLFunc((u32)psxRcntWtarget32);
			ADD32ItoR(ESP, 8);
			return;

		default:
			if (add>=0x1f801c00 && add<0x1f801e00) {
				_recPushReg(mmreg);
				iFlushCall(0);
				PUSH32I(add);
            	CALLFunc((u32)SPU2write);
				// leave esp alone
				return;
			}

			_eeWriteConstMem16((u32)&psxH[(add) & 0xffff], mmreg);
			return;
	}
}

#define DmaExec2(n) { \
	if (HW_DMA##n##_CHCR & 0x01000000 && \
		HW_DMA_PCR2 & (8 << ((n-7) * 4))) { \
		psxDma##n(HW_DMA##n##_MADR, HW_DMA##n##_BCR, HW_DMA##n##_CHCR); \
	} \
}

void psxHwWrite32(u32 add, u32 value) {
	if (add >= 0x1f801600 && add < 0x1f801700) {
		USBwrite32(add, value); return;
	}
	if (add >= 0x1f808400 && add <= 0x1f808550) {
		FWwrite32(add, value); return;
	}
	switch (add) {
	    case 0x1f801040:
			sioWrite8((u8)value);
			sioWrite8((u8)((value&0xff) >>  8));
			sioWrite8((u8)((value&0xff) >> 16));
			sioWrite8((u8)((value&0xff) >> 24));
#ifdef PAD_LOG
			PAD_LOG("sio write32 %lx\n", value);
#endif
			return;
	//	case 0x1f801050: serial_write32(value); break;//serial port
#ifdef PSXHW_LOG
		case 0x1f801060:
			PSXHW_LOG("RAM size write %lx\n", value);
			psxHu32(add) = value;
			return; // Ram size
#endif

		case 0x1f801070: 
#ifdef PSXHW_LOG
			PSXHW_LOG("IREG 32bit write %lx\n", value);
#endif
//			if (Config.Sio) psxHu32(0x1070) |= 0x80;
//			if (Config.SpuIrq) psxHu32(0x1070) |= 0x200;
			psxHu32(0x1070) &= value;
			return;
#ifdef PSXHW_LOG
		case 0x1f801074:
			PSXHW_LOG("IMASK 32bit write %lx\n", value);
			psxHu32(0x1074) = value;
			return;

		case 0x1f801078: 
			PSXHW_LOG("ICTRL 32bit write %lx\n", value);
//			SysPrintf("ICTRL 32bit write %lx\n", value);
			psxHu32(0x1078) = value;
			return;
#endif

		//SSBus registers
		case 0x1f801000:
			psxHu32(0x1000) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <spd_addr> 32bit write %lx\n", value);
#endif
			return;
		case 0x1f801004:
			psxHu32(0x1004) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <pio_addr> 32bit write %lx\n", value);
#endif
			return;
		case 0x1f801008:
			psxHu32(0x1008) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <spd_delay> 32bit write %lx\n", value);
#endif
			return;
		case 0x1f80100C:
			psxHu32(0x100C) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS dev1_delay 32bit write %lx\n", value);
#endif
			return;
		case 0x1f801010:
			psxHu32(0x1010) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS rom_delay 32bit write %lx\n", value);
#endif
			return;
		case 0x1f801014:
			psxHu32(0x1014) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS spu_delay 32bit write %lx\n", value);
#endif
			return;
		case 0x1f801018:
			psxHu32(0x1018) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS dev5_delay 32bit write %lx\n", value);
#endif
			return;
		case 0x1f80101C:
			psxHu32(0x101C) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <pio_delay> 32bit write %lx\n", value);
#endif
			return;
		case 0x1f801020:
			psxHu32(0x1020) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS com_delay 32bit write %lx\n", value);
#endif
			return;
		case 0x1f801400:
			psxHu32(0x1400) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS dev1_addr 32bit write %lx\n", value);
#endif
			return;
		case 0x1f801404:
			psxHu32(0x1404) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS spu_addr 32bit write %lx\n", value);
#endif
			return;
		case 0x1f801408:
			psxHu32(0x1408) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS dev5_addr 32bit write %lx\n", value);
#endif
			return;
		case 0x1f80140C:
			psxHu32(0x140C) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS spu1_addr 32bit write %lx\n", value);
#endif
			return;
		case 0x1f801410:
			psxHu32(0x1410) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <dev9_addr3> 32bit write %lx\n", value);
#endif
			return;
		case 0x1f801414:
			psxHu32(0x1414) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS spu1_delay 32bit write %lx\n", value);
#endif
			return;
		case 0x1f801418:
			psxHu32(0x1418) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <dev9_delay2> 32bit write %lx\n", value);
#endif
			return;
		case 0x1f80141C:
			psxHu32(0x141C) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <dev9_delay3> 32bit write %lx\n", value);
#endif
			return;
		case 0x1f801420:
			psxHu32(0x1420) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("SSBUS <dev9_delay1> 32bit write %lx\n", value);
#endif
			return;
#ifdef PSXHW_LOG
		case 0x1f801080:
			PSXHW_LOG("DMA0 MADR 32bit write %lx\n", value);
			HW_DMA0_MADR = value; return; // DMA0 madr
		case 0x1f801084:
			PSXHW_LOG("DMA0 BCR 32bit write %lx\n", value);
			HW_DMA0_BCR  = value; return; // DMA0 bcr
#endif
		case 0x1f801088:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA0 CHCR 32bit write %lx\n", value);
#endif
			HW_DMA0_CHCR = value;        // DMA0 chcr (MDEC in DMA)
//			DmaExec(0);
			return;

#ifdef PSXHW_LOG
		case 0x1f801090:
			PSXHW_LOG("DMA1 MADR 32bit write %lx\n", value);
			HW_DMA1_MADR = value; return; // DMA1 madr
		case 0x1f801094:
			PSXHW_LOG("DMA1 BCR 32bit write %lx\n", value);
			HW_DMA1_BCR  = value; return; // DMA1 bcr
#endif
		case 0x1f801098:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA1 CHCR 32bit write %lx\n", value);
#endif
			HW_DMA1_CHCR = value;        // DMA1 chcr (MDEC out DMA)
//			DmaExec(1);
			return;
		
#ifdef PSXHW_LOG
		case 0x1f8010a0:
			PSXHW_LOG("DMA2 MADR 32bit write %lx\n", value);
			HW_DMA2_MADR = value; return; // DMA2 madr
		case 0x1f8010a4:
			PSXHW_LOG("DMA2 BCR 32bit write %lx\n", value);
			HW_DMA2_BCR  = value; return; // DMA2 bcr
#endif
		case 0x1f8010a8:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA2 CHCR 32bit write %lx\n", value);
#endif
			HW_DMA2_CHCR = value;        // DMA2 chcr (GPU DMA)
			DmaExec(2);
			return;

#ifdef PSXHW_LOG
		case 0x1f8010b0:
			PSXHW_LOG("DMA3 MADR 32bit write %lx\n", value);
			HW_DMA3_MADR = value; return; // DMA3 madr
		case 0x1f8010b4:
			PSXHW_LOG("DMA3 BCR 32bit write %lx\n", value);
			HW_DMA3_BCR  = value; return; // DMA3 bcr
#endif
		case 0x1f8010b8:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA3 CHCR 32bit write %lx\n", value);
#endif
			HW_DMA3_CHCR = value;        // DMA3 chcr (CDROM DMA)
			DmaExec(3);
			
			return;

		case 0x1f8010c0:
			PSXHW_LOG("DMA4 MADR 32bit write %lx\n", value);
			SPU2WriteMemAddr(0,value);
			HW_DMA4_MADR = value; return; // DMA4 madr
		case 0x1f8010c4:
			PSXHW_LOG("DMA4 BCR 32bit write %lx\n", value);
			HW_DMA4_BCR  = value; return; // DMA4 bcr
		case 0x1f8010c8:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA4 CHCR 32bit write %lx\n", value);
#endif
			HW_DMA4_CHCR = value;        // DMA4 chcr (SPU DMA)
			DmaExec(4);
			return;

#if 0
		case 0x1f8010d0: break; //DMA5write_madr();
		case 0x1f8010d4: break; //DMA5write_bcr();
		case 0x1f8010d8: break; //DMA5write_chcr(); // Not yet needed??
#endif

#ifdef PSXHW_LOG
		case 0x1f8010e0:
			PSXHW_LOG("DMA6 MADR 32bit write %lx\n", value);
			HW_DMA6_MADR = value; return; // DMA6 madr
		case 0x1f8010e4:
			PSXHW_LOG("DMA6 BCR 32bit write %lx\n", value);
			HW_DMA6_BCR  = value; return; // DMA6 bcr
#endif
		case 0x1f8010e8:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA6 CHCR 32bit write %lx\n", value);
#endif
			HW_DMA6_CHCR = value;         // DMA6 chcr (OT clear)
			DmaExec(6);
			return;

		case 0x1f801500:
			PSXHW_LOG("DMA7 MADR 32bit write %lx\n", value);
			SPU2WriteMemAddr(1,value);
			HW_DMA7_MADR = value; return; // DMA7 madr
		case 0x1f801504:
			PSXHW_LOG("DMA7 BCR 32bit write %lx\n", value);
			HW_DMA7_BCR  = value; return; // DMA7 bcr
		case 0x1f801508:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA7 CHCR 32bit write %lx\n", value);
#endif
			HW_DMA7_CHCR = value;         // DMA7 chcr (SPU2)
			DmaExec2(7);
			return;

#ifdef PSXHW_LOG
		case 0x1f801510:
			PSXHW_LOG("DMA8 MADR 32bit write %lx\n", value);
			HW_DMA8_MADR = value; return; // DMA8 madr
		case 0x1f801514:
			PSXHW_LOG("DMA8 BCR 32bit write %lx\n", value);
			HW_DMA8_BCR  = value; return; // DMA8 bcr
#endif
		case 0x1f801518:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA8 CHCR 32bit write %lx\n", value);
#endif
			HW_DMA8_CHCR = value;         // DMA8 chcr (DEV9)
			DmaExec2(8);
			return;

#ifdef PSXHW_LOG
		case 0x1f801520:
			PSXHW_LOG("DMA9 MADR 32bit write %lx\n", value);
			HW_DMA9_MADR = value; return; // DMA9 madr
		case 0x1f801524:
			PSXHW_LOG("DMA9 BCR 32bit write %lx\n", value);
			HW_DMA9_BCR  = value; return; // DMA9 bcr
#endif
		case 0x1f801528:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA9 CHCR 32bit write %lx\n", value);
#endif
			HW_DMA9_CHCR = value;         // DMA9 chcr (SIF0)
			DmaExec2(9);
			return;
#ifdef PSXHW_LOG
		case 0x1f80152c:
			PSXHW_LOG("DMA9 TADR 32bit write %lx\n", value);
			HW_DMA9_TADR = value; return; // DMA9 tadr
#endif

#ifdef PSXHW_LOG
		case 0x1f801530:
			PSXHW_LOG("DMA10 MADR 32bit write %lx\n", value);
			HW_DMA10_MADR = value; return; // DMA10 madr
		case 0x1f801534:
			PSXHW_LOG("DMA10 BCR 32bit write %lx\n", value);
			HW_DMA10_BCR  = value; return; // DMA10 bcr
#endif
		case 0x1f801538:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA10 CHCR 32bit write %lx\n", value);
#endif
			HW_DMA10_CHCR = value;         // DMA10 chcr (SIF1)
			DmaExec2(10);
			return;

#ifdef PSXHW_LOG
		case 0x1f801540:
			PSXHW_LOG("DMA11 SIO2in MADR 32bit write %lx\n", value);
			HW_DMA11_MADR = value; return;
		
		case 0x1f801544:
			PSXHW_LOG("DMA11 SIO2in BCR 32bit write %lx\n", value);
			HW_DMA11_BCR  = value; return;
#endif
		case 0x1f801548:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA11 SIO2in CHCR 32bit write %lx\n", value);
#endif
			HW_DMA11_CHCR = value;         // DMA11 chcr (SIO2 in)
			DmaExec2(11);
			return;

#ifdef PSXHW_LOG
		case 0x1f801550:
			PSXHW_LOG("DMA12 SIO2out MADR 32bit write %lx\n", value);
			HW_DMA12_MADR = value; return;
		
		case 0x1f801554:
			PSXHW_LOG("DMA12 SIO2out BCR 32bit write %lx\n", value);
			HW_DMA12_BCR  = value; return;
#endif
		case 0x1f801558:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA12 SIO2out CHCR 32bit write %lx\n", value);
#endif
			HW_DMA12_CHCR = value;         // DMA12 chcr (SIO2 out)
			DmaExec2(12);
			return;

		case 0x1f801570:
			psxHu32(0x1570) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA PCR2 32bit write %lx\n", value);
#endif
			return;
#ifdef PSXHW_LOG
		case 0x1f8010f0:
			PSXHW_LOG("DMA PCR 32bit write %lx\n", value);
			HW_DMA_PCR = value;
			return;
#endif

		case 0x1f8010f4:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA ICR 32bit write %lx\n", value);
#endif
		{
			u32 tmp = (~value) & HW_DMA_ICR;
			HW_DMA_ICR = ((tmp ^ value) & 0xffffff) ^ tmp;
			return;
		}

		case 0x1f801574:
#ifdef PSXHW_LOG
			PSXHW_LOG("DMA ICR2 32bit write %lx\n", value);
#endif
		{
			u32 tmp = (~value) & HW_DMA_ICR2;
			HW_DMA_ICR2 = ((tmp ^ value) & 0xffffff) ^ tmp;
			return;
		}

/*		case 0x1f801810:
#ifdef PSXHW_LOG
			PSXHW_LOG("GPU DATA 32bit write %lx\n", value);
#endif
			GPU_writeData(value); return;
		case 0x1f801814:
#ifdef PSXHW_LOG
			PSXHW_LOG("GPU STATUS 32bit write %lx\n", value);
#endif
			GPU_writeStatus(value); return;
*/
/*		case 0x1f801820:
			mdecWrite0(value); break;
		case 0x1f801824:
			mdecWrite1(value); break;
*/
		case 0x1f801100:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 0 COUNT 32bit write %lx\n", value);
#endif
			psxRcntWcount16(0, value ); return;
		case 0x1f801104:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 0 MODE 32bit write %lx\n", value);
#endif
			psxRcnt0Wmode(value); return;
		case 0x1f801108:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 0 TARGET 32bit write %lx\n", value);
#endif
			psxRcntWtarget16(0, value ); return;

		case 0x1f801110:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 1 COUNT 32bit write %lx\n", value);
#endif
			psxRcntWcount16(1, value ); return;
		case 0x1f801114:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 1 MODE 32bit write %lx\n", value);
#endif
			psxRcnt1Wmode(value); return;
		case 0x1f801118:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 1 TARGET 32bit write %lx\n", value);
#endif
			psxRcntWtarget16(1, value ); return;

		case 0x1f801120:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 2 COUNT 32bit write %lx\n", value);
#endif
			psxRcntWcount16(2, value ); return;
		case 0x1f801124:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 2 MODE 32bit write %lx\n", value);
#endif
			psxRcnt2Wmode(value); return;
		case 0x1f801128:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 2 TARGET 32bit write %lx\n", value);
#endif
			psxRcntWtarget16(2, value); return;

		case 0x1f801480:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 3 COUNT 32bit write %lx\n", value);
#endif
			psxRcntWcount32(3, value); return;
		case 0x1f801484:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 3 MODE 32bit write %lx\n", value);
#endif
			psxRcnt3Wmode(value); return;
		case 0x1f801488:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 3 TARGET 32bit write %lx\n", value);
#endif
			psxRcntWtarget32(3, value); return;

		case 0x1f801490:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 4 COUNT 32bit write %lx\n", value);
#endif
			psxRcntWcount32(4, value); return;
		case 0x1f801494:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 4 MODE 32bit write %lx\n", value);
#endif
			psxRcnt4Wmode(value); return;
		case 0x1f801498:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 4 TARGET 32bit write %lx\n", value);
#endif
			psxRcntWtarget32(4, value); return;

		case 0x1f8014a0:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 5 COUNT 32bit write %lx\n", value);
#endif
			psxRcntWcount32(5, value); return;
		case 0x1f8014a4:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 5 MODE 32bit write %lx\n", value);
#endif
			psxRcnt5Wmode(value); return;
		case 0x1f8014a8:
#ifdef PSXHW_LOG
			PSXHW_LOG("COUNTER 5 TARGET 32bit write %lx\n", value);
#endif
			psxRcntWtarget32(5, value); return;

		case 0x1f8014c0:
#ifdef PSXHW_LOG
			PSXHW_LOG("RTC_HOLDMODE 32bit write %lx\n", value);
#endif
			SysPrintf("RTC_HOLDMODE 32bit write %lx\n", value);
			break;

		case 0x1f801450:
#ifdef PSXHW_LOG
			if (value) { PSXHW_LOG("%08X ICFG 32bit write %lx\n", psxRegs.pc, value); }
#endif
/*			if (value &&
				psxSu32(0x20) == 0x20000 &&
				(psxSu32(0x30) == 0x20000 ||
				 psxSu32(0x30) == 0x40000)) { // don't ask me why :P
				psxSu32(0x20) = 0x10000;
				psxSu32(0x30) = 0x10000;
			}*/
			psxHu32(0x1450) = /*(*/value/* & (~0x8)) | (psxHu32(0x1450) & 0x8)*/;
			return;

		case 0x1F808200:
		case 0x1F808204:
		case 0x1F808208:
		case 0x1F80820C:
		case 0x1F808210:
		case 0x1F808214:
		case 0x1F808218:
		case 0x1F80821C:
		case 0x1F808220:
		case 0x1F808224:
		case 0x1F808228:
		case 0x1F80822C:
		case 0x1F808230:
		case 0x1F808234:
		case 0x1F808238:
		case 0x1F80823C:
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 write param[%d] <- %lx\n", (add-0x1F808200)/4, value);
#endif
			sio2_setSend3((add-0x1F808200)/4, value);	return;

		case 0x1F808240:
		case 0x1F808248:
		case 0x1F808250:
		case 0x1F808258:
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 write send1[%d] <- %lx\n", (add-0x1F808240)/8, value);
#endif
			sio2_setSend1((add-0x1F808240)/8, value);	return;

		case 0x1F808244:
		case 0x1F80824C:
		case 0x1F808254:
		case 0x1F80825C:
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 write send2[%d] <- %lx\n", (add-0x1F808244)/8, value);
#endif
			sio2_setSend2((add-0x1F808244)/8, value);	return;

		case 0x1F808268:
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 write CTRL <- %lx\n", value);
#endif
			sio2_setCtrl(value);	return;

		case 0x1F808278:
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 write [8278] <- %lx\n", value);
#endif
			sio2_set8278(value);	return;

		case 0x1F80827C:
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 write [827C] <- %lx\n", value);
#endif
			sio2_set827C(value);	return;

		case 0x1F808280:
#ifdef PSXHW_LOG
			PSXHW_LOG("SIO2 write INTR <- %lx\n", value);
#endif
			sio2_setIntr(value);	return;

		default:
			psxHu32(add) = value;
#ifdef PSXHW_LOG
			PSXHW_LOG("*Unknown 32bit write at address %lx value %lx\n", add, value);
#endif
			return;
	}
	psxHu32(add) = value;
#ifdef PSXHW_LOG
	PSXHW_LOG("*Known 32bit write at address %lx value %lx\n", add, value);
#endif
}

#define recDmaExec(n) { \
	iFlushCall(0); \
	if( n > 6 ) \
		TEST32ItoM((u32)&HW_DMA_PCR2, 8 << ((n-7) * 4)); \
	else \
		TEST32ItoM((u32)&HW_DMA_PCR, 8 << (n * 4)); \
	j8Ptr[5] = JZ8(0); \
	MOV32MtoR(EAX, (u32)&HW_DMA##n##_CHCR); \
	TEST32ItoR(EAX, 0x01000000); \
	j8Ptr[6] = JZ8(0); \
	\
	PUSH32R(EAX); \
	PUSH32M((u32)&HW_DMA##n##_BCR); \
	PUSH32M((u32)&HW_DMA##n##_MADR); \
	CALLFunc((u32)psxDma##n); \
	ADD32ItoR(ESP, 12); \
	\
	x86SetJ8( j8Ptr[5] ); \
	x86SetJ8( j8Ptr[6] ); \
} \

#define CONSTWRITE_CALL32(name) { \
	iFlushCall(0); \
	_recPushReg(mmreg); \
	CALLFunc((u32)name); \
	ADD32ItoR(ESP, 4); \
} \

void psxHwConstWrite32(u32 add, int mmreg)
{
	if (add >= 0x1f801600 && add < 0x1f801700) {
		_recPushReg(mmreg);
		iFlushCall(0);
		PUSH32I(add);
		CALLFunc((u32)USBwrite32);
		return;
	}
	if (add >= 0x1f808400 && add <= 0x1f808550) {
		_recPushReg(mmreg);
		iFlushCall(0);
		PUSH32I(add);
		CALLFunc((u32)FWwrite32);
		return;
	}

	switch (add) {
	    case 0x1f801040:
			_recPushReg(mmreg);
			iFlushCall(0);
			CALLFunc((u32)sioWrite8);
			ADD32ItoR(ESP, 1);
			CALLFunc((u32)sioWrite8);
			ADD32ItoR(ESP, 1);
			CALLFunc((u32)sioWrite8);
			ADD32ItoR(ESP, 1);
			CALLFunc((u32)sioWrite8);
			ADD32ItoR(ESP, 1);
			return;

		case 0x1f801070:
			_eeWriteConstMem32OP((u32)&psxH[(add) & 0xffff], mmreg, 0); // and
			return;

//		case 0x1f801088:
//			HW_DMA0_CHCR = value;        // DMA0 chcr (MDEC in DMA)
////			DmaExec(0);
//			return;

//		case 0x1f801098:
//			HW_DMA1_CHCR = value;        // DMA1 chcr (MDEC out DMA)
////			DmaExec(1);
//			return;
		
		case 0x1f8010a8:
			_eeWriteConstMem32((u32)&psxH[(add) & 0xffff], mmreg);
			recDmaExec(2);
			return;

		case 0x1f8010b8:
			_eeWriteConstMem32((u32)&psxH[(add) & 0xffff], mmreg);
			recDmaExec(3);
			return;

		case 0x1f8010c8:
			_eeWriteConstMem32((u32)&psxH[(add) & 0xffff], mmreg);
			recDmaExec(4);
			return;

		case 0x1f8010e8:
			_eeWriteConstMem32((u32)&psxH[(add) & 0xffff], mmreg);
			recDmaExec(6);
			return;

		case 0x1f801508:
			_eeWriteConstMem32((u32)&psxH[(add) & 0xffff], mmreg);
			recDmaExec(7);
			return;

		case 0x1f801518:
			_eeWriteConstMem32((u32)&psxH[(add) & 0xffff], mmreg);
			recDmaExec(8);
			return;

		case 0x1f801528:
			_eeWriteConstMem32((u32)&psxH[(add) & 0xffff], mmreg);
			recDmaExec(9);
			return;

		case 0x1f801538:
			_eeWriteConstMem32((u32)&psxH[(add) & 0xffff], mmreg);
			recDmaExec(10);
			return;

		case 0x1f801548:
			_eeWriteConstMem32((u32)&psxH[(add) & 0xffff], mmreg);
			recDmaExec(11);
			return;

		case 0x1f801558:
			_eeWriteConstMem32((u32)&psxH[(add) & 0xffff], mmreg);
			recDmaExec(12);
			return;

		case 0x1f8010f4:
		case 0x1f801574:
		{
			// u32 tmp = (~value) & HW_DMA_ICR;
			_eeMoveMMREGtoR(EAX, mmreg);
			MOV32RtoR(ECX, EAX);
			NOT32R(ECX);
			AND32MtoR(ECX, (u32)&psxH[(add) & 0xffff]);

			// HW_DMA_ICR = ((tmp ^ value) & 0xffffff) ^ tmp;
			XOR32RtoR(EAX, ECX);
			AND32ItoR(EAX, 0xffffff);
			XOR32RtoR(EAX, ECX);
			MOV32RtoM((u32)&psxH[(add) & 0xffff], EAX);
			return;
		}

		// counters[0]
		case 0x1f801100:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(0);
			CALLFunc((u32)psxRcntWcount16);
			ADD32ItoR(ESP, 8);
			return;
		case 0x1f801104:
			CONSTWRITE_CALL32(psxRcnt0Wmode);
			return;
		case 0x1f801108:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(0);
			CALLFunc((u32)psxRcntWtarget16);
			ADD32ItoR(ESP, 8);
			return;

		// counters[1]
		case 0x1f801110:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(1);
			CALLFunc((u32)psxRcntWcount16);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1f801114:
			CONSTWRITE_CALL32(psxRcnt1Wmode);
			return;

		case 0x1f801118:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(1);
			CALLFunc((u32)psxRcntWtarget16);
			ADD32ItoR(ESP, 8);
			return;

		// counters[2]
		case 0x1f801120:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(2);
			CALLFunc((u32)psxRcntWcount16);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1f801124:
			CONSTWRITE_CALL32(psxRcnt2Wmode);
			return;

		case 0x1f801128:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(2);
			CALLFunc((u32)psxRcntWtarget16);
			ADD32ItoR(ESP, 8);
			return;

		// counters[3]
		case 0x1f801480:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(3);
			CALLFunc((u32)psxRcntWcount32);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1f801484:
			CONSTWRITE_CALL32(psxRcnt3Wmode);
			return;

		case 0x1f801488:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(3);
			CALLFunc((u32)psxRcntWtarget32);
			ADD32ItoR(ESP, 8);
			return;

		// counters[4]
		case 0x1f801490:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(4);
			CALLFunc((u32)psxRcntWcount32);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1f801494:
			CONSTWRITE_CALL32(psxRcnt4Wmode);
			return;

		case 0x1f801498:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(4);
			CALLFunc((u32)psxRcntWtarget32);
			ADD32ItoR(ESP, 8);
			return;

		// counters[5]
		case 0x1f8014a0:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(5);
			CALLFunc((u32)psxRcntWcount32);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1f8014a4:
			CONSTWRITE_CALL32(psxRcnt5Wmode);
			return;

		case 0x1f8014a8:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(5);
			CALLFunc((u32)psxRcntWtarget32);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1f8014c0:
			SysPrintf("RTC_HOLDMODE 32bit write\n");
			break;

		case 0x1F808200:
		case 0x1F808204:
		case 0x1F808208:
		case 0x1F80820C:
		case 0x1F808210:
		case 0x1F808214:
		case 0x1F808218:
		case 0x1F80821C:
		case 0x1F808220:
		case 0x1F808224:
		case 0x1F808228:
		case 0x1F80822C:
		case 0x1F808230:
		case 0x1F808234:
		case 0x1F808238:
		case 0x1F80823C:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I((add-0x1F808200)/4);
			CALLFunc((u32)sio2_setSend3);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1F808240:
		case 0x1F808248:
		case 0x1F808250:
		case 0x1F808258:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I((add-0x1F808240)/8);
			CALLFunc((u32)sio2_setSend1);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1F808244:
		case 0x1F80824C:
		case 0x1F808254:
		case 0x1F80825C:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I((add-0x1F808244)/8);
			CALLFunc((u32)sio2_setSend2);
			ADD32ItoR(ESP, 8);
			return;

		case 0x1F808268: CONSTWRITE_CALL32(sio2_setCtrl); return;
		case 0x1F808278: CONSTWRITE_CALL32(sio2_set8278);	return;
		case 0x1F80827C: CONSTWRITE_CALL32(sio2_set827C);	return;
		case 0x1F808280: CONSTWRITE_CALL32(sio2_setIntr);	return;
		
		case 0x1F8010C0:
			_eeWriteConstMem32((u32)&psxH[(add) & 0xffff], mmreg);
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(0);
			CALLFunc((u32)SPU2WriteMemAddr);
			return;

		case 0x1F801500:
			_eeWriteConstMem32((u32)&psxH[(add) & 0xffff], mmreg);
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(1);
			CALLFunc((u32)SPU2WriteMemAddr);
			return;
		default:
			_eeWriteConstMem32((u32)&psxH[(add) & 0xffff], mmreg);
			return;
	}
}

u8 psxHw4Read8(u32 add) {
	u8 hard;

	switch (add) {
		case 0x1f402004: return cdvdRead04();
		case 0x1f402005: return cdvdRead05();
		case 0x1f402006: return cdvdRead06();
		case 0x1f402007: return cdvdRead07();
		case 0x1f402008: return cdvdRead08();
		case 0x1f40200A: return cdvdRead0A();
		case 0x1f40200B: return cdvdRead0B();
		case 0x1f40200C: return cdvdRead0C();
		case 0x1f40200D: return cdvdRead0D();
		case 0x1f40200E: return cdvdRead0E();
		case 0x1f40200F: return cdvdRead0F();
		case 0x1f402013: return cdvdRead13();
		case 0x1f402015: return cdvdRead15();
		case 0x1f402016: return cdvdRead16();
		case 0x1f402017: return cdvdRead17();
		case 0x1f402018: return cdvdRead18();
		case 0x1f402020: return cdvdRead20();
		case 0x1f402021: return cdvdRead21();
		case 0x1f402022: return cdvdRead22();
		case 0x1f402023: return cdvdRead23();
		case 0x1f402024: return cdvdRead24();
		case 0x1f402028: return cdvdRead28();
		case 0x1f402029: return cdvdRead29();
		case 0x1f40202A: return cdvdRead2A();
		case 0x1f40202B: return cdvdRead2B();
		case 0x1f40202C: return cdvdRead2C();
		case 0x1f402030: return cdvdRead30();
		case 0x1f402031: return cdvdRead31();
		case 0x1f402032: return cdvdRead32();
		case 0x1f402033: return cdvdRead33();
		case 0x1f402034: return cdvdRead34();
		case 0x1f402038: return cdvdRead38();
		case 0x1f402039: return cdvdRead39();
		case 0x1f40203A: return cdvdRead3A();
		default:
#ifdef PSXHW_LOG
			PSXHW_LOG("*Unkwnown 8bit read at address %lx\n", add);
#endif
			SysPrintf("*Unkwnown 8bit read at address %lx\n", add);
			return 0;
	}
	
#ifdef PSXHW_LOG
	PSXHW_LOG("*Known 8bit read at address %lx value %x\n", add, hard);
#endif

	return hard;
}

int psxHw4ConstRead8(u32 x86reg, u32 add, u32 sign) {
	switch (add) {
		case 0x1f402004: CONSTREAD8_CALL((u32)cdvdRead04); return 1;
		case 0x1f402005: CONSTREAD8_CALL((u32)cdvdRead05); return 1;
		case 0x1f402006: CONSTREAD8_CALL((u32)cdvdRead06); return 1;
		case 0x1f402007: CONSTREAD8_CALL((u32)cdvdRead07); return 1;
		case 0x1f402008: CONSTREAD8_CALL((u32)cdvdRead08); return 1;
		case 0x1f40200A: CONSTREAD8_CALL((u32)cdvdRead0A); return 1;
		case 0x1f40200B: CONSTREAD8_CALL((u32)cdvdRead0B); return 1;
		case 0x1f40200C: CONSTREAD8_CALL((u32)cdvdRead0C); return 1;
		case 0x1f40200D: CONSTREAD8_CALL((u32)cdvdRead0D); return 1;
		case 0x1f40200E: CONSTREAD8_CALL((u32)cdvdRead0E); return 1;
		case 0x1f40200F: CONSTREAD8_CALL((u32)cdvdRead0F); return 1;
		case 0x1f402013: CONSTREAD8_CALL((u32)cdvdRead13); return 1;
		case 0x1f402015: CONSTREAD8_CALL((u32)cdvdRead15); return 1;
		case 0x1f402016: CONSTREAD8_CALL((u32)cdvdRead16); return 1;
		case 0x1f402017: CONSTREAD8_CALL((u32)cdvdRead17); return 1;
		case 0x1f402018: CONSTREAD8_CALL((u32)cdvdRead18); return 1;
		case 0x1f402020: CONSTREAD8_CALL((u32)cdvdRead20); return 1;
		case 0x1f402021: CONSTREAD8_CALL((u32)cdvdRead21); return 1;
		case 0x1f402022: CONSTREAD8_CALL((u32)cdvdRead22); return 1;
		case 0x1f402023: CONSTREAD8_CALL((u32)cdvdRead23); return 1;
		case 0x1f402024: CONSTREAD8_CALL((u32)cdvdRead24); return 1;
		case 0x1f402028: CONSTREAD8_CALL((u32)cdvdRead28); return 1;
		case 0x1f402029: CONSTREAD8_CALL((u32)cdvdRead29); return 1;
		case 0x1f40202A: CONSTREAD8_CALL((u32)cdvdRead2A); return 1;
		case 0x1f40202B: CONSTREAD8_CALL((u32)cdvdRead2B); return 1;
		case 0x1f40202C: CONSTREAD8_CALL((u32)cdvdRead2C); return 1;
		case 0x1f402030: CONSTREAD8_CALL((u32)cdvdRead30); return 1;
		case 0x1f402031: CONSTREAD8_CALL((u32)cdvdRead31); return 1;
		case 0x1f402032: CONSTREAD8_CALL((u32)cdvdRead32); return 1;
		case 0x1f402033: CONSTREAD8_CALL((u32)cdvdRead33); return 1;
		case 0x1f402034: CONSTREAD8_CALL((u32)cdvdRead34); return 1;
		case 0x1f402038: CONSTREAD8_CALL((u32)cdvdRead38); return 1;
		case 0x1f402039: CONSTREAD8_CALL((u32)cdvdRead39); return 1;
		case 0x1f40203A: CONSTREAD8_CALL((u32)cdvdRead3A); return 1;
		default:
			SysPrintf("*Unkwnown 8bit read at address %lx\n", add);
			XOR32RtoR(x86reg, x86reg);
			return 0;
	}
}

void psxHw4Write8(u32 add, u8 value) {
	switch (add) {
		case 0x1f402004: cdvdWrite04(value); return;
		case 0x1f402005: cdvdWrite05(value); return;
		case 0x1f402006: cdvdWrite06(value); return;
		case 0x1f402007: cdvdWrite07(value); return;
		case 0x1f402008: cdvdWrite08(value); return;
		case 0x1f40200A: cdvdWrite0A(value); return;
		case 0x1f40200F: cdvdWrite0F(value); return;
		case 0x1f402014: cdvdWrite14(value); return;
		case 0x1f402016:
			cdvdWrite16(value);
			FreezeMMXRegs(0);
			return;
		case 0x1f402017: cdvdWrite17(value); return;
		case 0x1f402018: cdvdWrite18(value); return;
		case 0x1f40203A: cdvdWrite3A(value); return;
		default:
#ifdef PSXHW_LOG
			PSXHW_LOG("*Unknown 8bit write at address %lx value %x\n", add, value);
#endif
			SysPrintf("*Unknown 8bit write at address %lx value %x\n", add, value);
			return;
	}
#ifdef PSXHW_LOG
	PSXHW_LOG("*Known 8bit write at address %lx value %x\n", add, value);
#endif
}

void psxHw4ConstWrite8(u32 add, int mmreg) {
	switch (add) {
		case 0x1f402004: CONSTWRITE_CALL(cdvdWrite04); return;
		case 0x1f402005: CONSTWRITE_CALL(cdvdWrite05); return;
		case 0x1f402006: CONSTWRITE_CALL(cdvdWrite06); return;
		case 0x1f402007: CONSTWRITE_CALL(cdvdWrite07); return;
		case 0x1f402008: CONSTWRITE_CALL(cdvdWrite08); return;
		case 0x1f40200A: CONSTWRITE_CALL(cdvdWrite0A); return;
		case 0x1f40200F: CONSTWRITE_CALL(cdvdWrite0F); return;
		case 0x1f402014: CONSTWRITE_CALL(cdvdWrite14); return;
		case 0x1f402016: 
			_freeMMXregs();
			CONSTWRITE_CALL(cdvdWrite16);
			return;
		case 0x1f402017: CONSTWRITE_CALL(cdvdWrite17); return;
		case 0x1f402018: CONSTWRITE_CALL(cdvdWrite18); return;
		case 0x1f40203A: CONSTWRITE_CALL(cdvdWrite3A); return;
		default:
			SysPrintf("*Unknown 8bit write at address %lx\n", add);
			return;
	}
}

void psxDmaInterrupt(int n) {
	if (HW_DMA_ICR & (1 << (16 + n))) {
		HW_DMA_ICR|= (1 << (24 + n));
		psxRegs.CP0.n.Cause |= 1 << (9 + n);
		psxHu32(0x1070) |= 8;
		
	}
}

void psxDmaInterrupt2(int n) {
	if (HW_DMA_ICR2 & (1 << (16 + n))) {
/*		if (HW_DMA_ICR2 & (1 << (24 + n))) {
			SysPrintf("*PCSX2*: HW_DMA_ICR2 n=%d already set\n", n);
		}
		if (psxHu32(0x1070) & 8) {
			SysPrintf("*PCSX2*: psxHu32(0x1070) 8 already set (n=%d)\n", n);
		}*/
		HW_DMA_ICR2|= (1 << (24 + n));
		psxRegs.CP0.n.Cause |= 1 << (16 + n);
		psxHu32(0x1070) |= 8;
	}
}
