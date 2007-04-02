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
#include <math.h>
#include "PsxCommon.h"

psxCounter psxCounters[8];
u32 psxNextCounter, psxNextsCounter;
static int cnts = 6;
u8 psxhblankgate = 0;

static void psxRcntUpd16(u32 index) {
	psxCounters[index].sCycle  = psxRegs.cycle;
	psxCounters[index].sCycleT = psxRegs.cycle;

	psxCounters[index].Cycle  = (0xffff - (psxRcntRcount16(index)&0xffff)) * psxCounters[index].rate;
	psxCounters[index].CycleT = (psxCounters[index].target - psxRcntRcount16(index)) * psxCounters[index].rate;
}

static void psxRcntUpd32(u32 index) {
	psxCounters[index].sCycle  = psxRegs.cycle;
	psxCounters[index].sCycleT = psxRegs.cycle;

	psxCounters[index].Cycle  = (0xffffffff - ((u32)psxRcntRcount32(index)&0xffffffff)) * psxCounters[index].rate;
	psxCounters[index].CycleT = (psxCounters[index].target - (u32)psxRcntRcount32(index)) * psxCounters[index].rate;
}

static void psxRcntReset16(u32 index) {
	psxCounters[index].count = 0;

	psxCounters[index].mode&= ~0x18301C00;
	psxRcntUpd16(index);
}

static void psxRcntReset32(u32 index) {
	psxCounters[index].count = 0;
	//psxCounters[index].otarget = 0x10000;

	psxCounters[index].mode&= ~0x18301C00;
	psxRcntUpd32(index);
}

static void psxRcntSet() {
	u32 c;
	int i;

	/*if (Config.SafeCnts) {
		psxNextCounter = 0;
		psxNextsCounter = 0;
		return;
	}*/

	psxNextCounter = 0xffffffff;
	psxNextsCounter = psxRegs.cycle;

	for (i=0; i<3; i++) {
		c = (u32)(0xffff - psxRcntCycles(i)) * psxCounters[i].rate;
		if (c < psxNextCounter) {
			psxNextCounter = c;
		}
		if(psxCounters[i].mode & 0x08000000) continue;
		c = (u32)(psxCounters[i].target - psxRcntCycles(i)) * psxCounters[i].rate;
		if (c < psxNextCounter) {
			psxNextCounter = c;
		}
	}
	for (i=3; i<7; i++) {
		c = (u32)(0xffffffff - psxRcntCycles(i)) * psxCounters[i].rate;
		if (c < psxNextCounter) {
			psxNextCounter = c;
		}
		if(psxCounters[i].mode & 0x08000000) continue;
		c = (u32)(psxCounters[i].target - psxRcntCycles(i)) * psxCounters[i].rate;
		if (c < psxNextCounter) {
			psxNextCounter = c;
		}
	}

//	SysPrintf("psxRcntSet: %x (cycle)\n", psxNextCounter, psxRegs.cycle);
}


void psxRcntInit() {
	int i;

	memset(psxCounters, 0, sizeof(psxCounters));

	for (i=0; i<3; i++) {
		psxCounters[i].rate = 1;
		psxCounters[i].mode|= 0x0400;
		psxCounters[i].target = 0xffff;
	}
	for (i=3; i<6; i++) {
		psxCounters[i].rate = 1;
		psxCounters[i].mode|= 0x0400;
		psxCounters[i].target = 0xffffffff;
	}

	psxCounters[0].interrupt = 0x10;
	psxCounters[1].interrupt = 0x20;
	psxCounters[2].interrupt = 0x40;

	psxCounters[3].interrupt = 0x04000;
	psxCounters[4].interrupt = 0x08000;
	psxCounters[5].interrupt = 0x10000;

	if (SPU2async != NULL) {
		cnts = 7;

		psxCounters[6].rate = 1;
		psxCounters[6].target = 768*64;
		psxCounters[6].mode = 0x8;
	} else cnts = 6;

	for (i=0; i<3; i++)
		psxRcntUpd16(i);
	for (i=3; i<6; i++)
		psxRcntUpd32(i);
	for (i=6; i<cnts; i++)
		psxRcntUpd16(i);
	psxRcntSet();
}

void psxVSyncStart() {
	cdvdVsync();
	psxHu32(0x1070)|= 1;
	psxCheckStartGate(1);
	psxCheckStartGate(3);
}

void psxVSyncEnd() {
	psxHu32(0x1070)|= 0x800;
	psxCheckEndGate(1);
	psxCheckEndGate(3);
}
void psxCheckEndGate(int counter) { //Check Gate events when Vsync Ends
	int i = counter;

	if(counter < 3){  //Gates for 16bit counters
		if((psxCounters[i].mode & 0x1) == 0) return; //Ignore Gate
		if(counter == 0) psxhblankgate &= ~1;
		switch((psxCounters[i].mode & 0x6) >> 1) {
			case 0x0: //GATE_ON_count
				psxCounters[i].count += (u16)psxRcntRcount16(i); //Only counts when signal is on
				break;
			case 0x1: //GATE_ON_ClearStart
				//Do nothing
				break;
			case 0x2: //GATE_ON_Clear_OFF_Start
				psxRcntUpd16(i); //Gate Starts counting from here
				break;
			case 0x3: //GATE_ON_Start
				//Do Nothing, already started and counting
				break;
			default:
				SysPrintf("PCSX2 Warning: 16bit IOP Counter Gate Not Set!\n");
				break;
		}
	}

	if(counter >= 3){  //Gates for 32bit counters
		if((psxCounters[i].mode & 0x1) == 0) return; //Ignore Gate
		switch((psxCounters[i].mode & 0x6) >> 1) {
			case 0x0: //GATE_ON_count
				psxCounters[i].count += (u32)psxRcntRcount32(i);  //Only counts when signal is on
				break;
			case 0x1: //GATE_ON_ClearStart
				//Do nothing
				break;
			case 0x2: //GATE_ON_Clear_OFF_Start
				psxRcntUpd32(i); //Gate Starts counting from here
				break;
			case 0x3: //GATE_ON_Start
				//Do Nothing, already started and counting
				break;
			default:
				SysPrintf("PCSX2 Warning: 32bit IOP Counter Gate Not Set!\n");
				break;
		}
	}
}
void psxCheckStartGate(int counter) {  //Check Gate events when Vsync Starts
	int i = counter;

	if(counter < 3){  //Gates for 16bit counters
		if((psxCounters[i].mode & 0x1) == 0) return; //Ignore Gate
		if(counter == 0) psxhblankgate |= 1;
		switch((psxCounters[i].mode & 0x6) >> 1) {
			case 0x0: //GATE_ON_count
				psxRcntUpd16(i);
				break;
			case 0x1: //GATE_ON_ClearStart
				psxRcntReset16(i);
				break;
			case 0x2: //GATE_ON_Clear_OFF_Start
				psxRcntReset16(i);
				break;
			case 0x3: //GATE_ON_Start
				if(psxCounters[i].mode & 0x10000000)psxCounters[i].count += (u16)psxRcntRcount16(i); //Save the count if not first cycle
				psxRcntUpd16(i);
				psxCounters[i].mode |= 0x10000000;
				break;
			default:
				SysPrintf("PCSX2 Warning: 16bit IOP Counter Gate Not Set!\n");
				break;
		}
	}

	if(counter >= 3){  //Gates for 32bit counters
		if((psxCounters[i].mode & 0x1) == 0) return; //Ignore Gate
		switch((psxCounters[i].mode & 0x6) >> 1) {
			case 0x0: //GATE_ON_count
				psxRcntUpd32(i);
				break;
			case 0x1: //GATE_ON_ClearStart
				psxRcntReset32(i);
				break;
			case 0x2: //GATE_ON_Clear_OFF_Start
				psxRcntReset32(i);
				break;
			case 0x3: //GATE_ON_Start
				if(psxCounters[i].mode & 0x10000000)psxCounters[i].count += (u32)psxRcntRcount32(i); //Save the count if not first cycle
				psxRcntUpd32(i);
				psxCounters[i].mode |= 0x10000000;
				break;
			default:
				SysPrintf("PCSX2 Warning: 32bit IOP Counter Gate Not Set!\n");
				break;
		}
	}
}

void _testRcnt16target(int i) {
	psxCounters[i].mode|= 0x0800; // Target flag
if (!(psxCounters[i].mode & 0x08000000) && psxCounters[i].mode & 0x10) { // Target interrupt
	
#ifdef PSXCNT_LOG
		PSXCNT_LOG("[%d] target 0x%x >= 0x%x (CycleT); count=0x%x, target=0x%x\n", i, (psxRegs.cycle - psxCounters[i].sCycleT), psxCounters[i].CycleT, psxRcntRcount16(i), psxCounters[i].target);
#endif
		psxHu32(0x1070)|= psxCounters[i].interrupt;
		if(psxCounters[i].mode & 0x80)psxCounters[i].mode&= ~0x0400; // Interrupt flag		
		
	}

	if (psxCounters[i].mode & 0x08) { // Reset on target
		psxCounters[i].count = 0;
		psxRcntUpd16(i);//psxRcntReset16(i);
		//psxCounters[i].mode|= 0x0C00; // Interrupt flag
	}
	if(!(psxCounters[i].mode & 0x40)) psxCounters[i].mode|= 0x08000000;
		else {
			psxRcntWtarget16(i, psxRcntRcount16(i) + psxCounters[i].target); //Repeat Interrupt
		}
}

void _testRcnt16overflow(int i) {
	psxCounters[i].mode|= 0x1000; // Overflow flag
	if (psxCounters[i].mode & 0x0020) { // Overflow interrupt
#ifdef PSXCNT_LOG
		PSXCNT_LOG("[%d] overflow 0x%x >= 0x%x (Cycle); Rcount=0x%x, count=0x%x\n", i, (psxRegs.cycle - psxCounters[i].sCycle), psxCounters[i].Cycle, psxRcntRcount16(i), psxCounters[i].count);
#endif		
		psxHu32(0x1070)|= psxCounters[i].interrupt;
		if(psxCounters[i].mode & 0x80)psxCounters[i].mode&= ~0x0400; // Interrupt flag
	}

	psxCounters[i].count = 0;
	psxRcntUpd16(i);//psxRcntReset16(i);
	//psxCounters[i].mode|= 0x1400; // Overflow flag
}

void _testRcnt32target(int i) {
	psxCounters[i].mode|= 0x0800; // Target flag
	if (!(psxCounters[i].mode & 0x08000000) && psxCounters[i].mode & 0x10) { // Target interrupt
/*#ifdef PSXCNT_LOG
		PSXCNT_LOG("[%d] target 0x%x >= 0x%x (CycleT); count=0x%x, target=0x%x\n", i, (psxRegs.cycle - psxCounters[i].sCycleT), psxCounters[i].CycleT, psxRcntRcount32(i), psxCounters[i].target);
#endif*/
		psxHu32(0x1070)|= psxCounters[i].interrupt;
		if(psxCounters[i].mode & 0x80)psxCounters[i].mode&= ~0x0400; // Interrupt flag	
		
	}	

	if (psxCounters[i].mode & 0x08) { // Reset on target
		psxCounters[i].count = 0;
		psxRcntUpd32(i);
		//psxCounters[i].mode|= 0x0C00; // Interrupt flag
	} 	 
	if(!(psxCounters[i].mode & 0x40))psxCounters[i].mode|= 0x08000000;
		else {
			psxRcntWtarget32(i, psxRcntRcount32(i) + psxCounters[i].target); //Repeat Interrupt
		}
}

void _testRcnt32overflow(int i) {
	psxCounters[i].mode|= 0x1000; // Overflow flag
		if (psxCounters[i].mode & 0x0020) { // Overflow interrupt
#ifdef PSXCNT_LOG
			PSXCNT_LOG("[%d] overflow 0x%x >= 0x%x (Cycle); Rcount=0x%x, count=0x%x\n", i, (psxRegs.cycle - psxCounters[i].sCycle), psxCounters[i].Cycle, psxRcntRcount32(i), psxCounters[i].count);
#endif	
			psxHu32(0x1070)|= psxCounters[i].interrupt;
			if(psxCounters[i].mode & 0x80)psxCounters[i].mode&= ~0x0400; // Interrupt flag
		}
		psxCounters[i].count = 0;
		psxRcntUpd32(i);//psxRcntReset32(i);
		//psxCounters[i].mode|= 0x1400; // Overflow flag
}

void _testRcnt16(int i) {
	//if(!(psxCounters[i].mode & 0x08000000)) {
		if (psxRcntCycles(i) >= psxCounters[i].target)
			_testRcnt16target(i);
	//}

	if (psxRcntCycles(i) >= 0xffff) {
			_testRcnt16overflow(i);
			psxCounters[i].count = 0;
			psxRcntUpd16(i);
	}

	/* mode & 0x08000000 means target interrupt already happened, 
	   if so don't retest the target */
	 //return;

	
}

void _testRcnt32(int i) {

	//if (!(psxCounters[i].mode & 0x08000000)) {

		if (psxRcntCycles(i) >= psxCounters[i].target)
			_testRcnt32target(i);
	
	//}

	if (psxRcntCycles(i) >= 0xffffffff) {
			_testRcnt32overflow(i);
			psxCounters[i].count = 0;
			psxRcntUpd32(i);
	}
		

	/* mode & 0x08000000 means target interrupt already happened, 
	   if so don't retest the target */
	
}

void psxRcntUpdate() {
		_testRcnt16(0);
		_testRcnt16(1);
		_testRcnt16(2);
		_testRcnt32(3);
		_testRcnt32(4);
		_testRcnt32(5);

	if (cnts >= 7) {
		if (psxRcntCycles(6) >= psxCounters[6].target) {
			SPU2async((u32)(psxRegs.cycle - psxCounters[6].sCycleT));
			psxRcntReset16(6);
		}
	}

	psxRcntSet();
}

void psxRcntWcount16(int index, u32 value) {
#ifdef PSXCNT_LOG
	PSXCNT_LOG("writeCcount[%d] = %x\n", index, value);
#endif
#ifdef PCSX2_DEVBUILD
	//SysPrintf("Write to 16bit count reg counter %x\n", index);
#endif
	//psxCounters[index].mode &= ~0x08001C00;
	psxCounters[index].count = 0;
	psxRcntUpd16(index);
	psxRcntSet();
}

void psxRcntWcount32(int index, u32 value) {
#ifdef PSXCNT_LOG
	PSXCNT_LOG("writeCcount[%d] = %x\n", index, value);
#endif
#ifdef PCSX2_DEVBUILD
	//SysPrintf("Write to 32bit count reg counter %x\n", index);
#endif
	//psxCounters[index].mode &= ~0x08001C00;
	psxCounters[index].count = 0;
	psxRcntUpd32(index);
	psxRcntSet();
}

void psxRcnt0Wmode(u32 value)  {
#ifdef PSXCNT_LOG
	PSXCNT_LOG("IOP writeCmode[0] = %lx\n", value);
#endif
	if (value & 0x1c00) {
		//SysPrintf("Counter 0 Value write %x\n", value & 0x1c00);
	}
	/*if ((value & 0x3ff) == (psxCounters[0].mode & 0x3ff)) {
		return;
	}*/
	psxCounters[0].mode = value;
	psxCounters[0].mode|= 0x0400;
	//psxCounters[0].count = psxRcntRcount16(0);
	psxCounters[0].rate = 1;
	
    
	if(value & 0x100) psxCounters[0].rate = PSXPIXEL;
	// Need to set a rate and target
	/*if(psxCounters[0].mode & 0x1){
	//	psxCounters[0].mode &= ~0x18101C00;
		psxRcntReset16(0);
	} else {*/
	psxCounters[0].count = 0;
	psxRcntUpd16(0);
	psxRcntSet();
	//}
}

void psxRcnt1Wmode(u32 value)  {
#ifdef PSXCNT_LOG
	PSXCNT_LOG("IOP writeCmode[1] = %lx\n", value);
#endif
	if (value & 0x1c00) {
		//SysPrintf("Counter 1 Value write %x\n", value & 0x1c00);
	}
	/*if ((value & 0x3ff) == (psxCounters[1].mode & 0x3ff)) {
		return;
	}*/
	psxCounters[1].mode = value;
	psxCounters[1].mode|= 0x0400;
	//psxCounters[1].count = psxRcntRcount16(1);
	psxCounters[1].rate = 1;
	
	if(value & 0x100)psxCounters[1].rate = PSXHBLANK;

	/*if(psxCounters[1].mode & 0x1){
		//psxCounters[1].mode &= ~0x18101C00;
		psxRcntReset16(1);
	} else {*/
	psxCounters[1].count = 0;
	psxRcntUpd16(1);
	psxRcntSet();
	//}
}

void psxRcnt2Wmode(u32 value)  {
#ifdef PSXCNT_LOG
	PSXCNT_LOG("IOP writeCmode[2] = %lx\n", value);
#endif
	if (value & 0x1c00) {
		//SysPrintf("Counter 2 Value write %x\n", value & 0x1c00);
	}
	/*if ((value & 0x3ff) == (psxCounters[2].mode & 0x3ff)) {
		return;
	}*/

	psxCounters[2].mode = value;
	psxCounters[2].mode|= 0x0400;
	//psxCounters[2].count = psxRcntRcount16(2);
	switch(value & 0x200){
		case 0x200:
			psxCounters[2].rate = 8;
			break;
		case 0x000:
			psxCounters[2].rate = 1;
			break;
	}

if(psxCounters[2].mode & 0x1){
	SysPrintf("Gate set on IOP C2, disabling\n");
	psxCounters[2].mode|= 0x1000000;
}
	// Need to set a rate and target
	psxCounters[2].count = 0;
	psxRcntUpd16(2);
	psxRcntSet();
}

void psxRcnt3Wmode(u32 value)  {
#ifdef PSXCNT_LOG
	PSXCNT_LOG("IOP writeCmode[3] = %lx\n", value);
#endif
	if (value & 0x1c00) {
		//SysPrintf("Counter 3 Value write %x\n", value & 0x1c00);
	}
	/*if ((value & 0x3ff) == (psxCounters[3].mode & 0x3ff)) {
		return;
	}*/
	psxCounters[3].mode = value;
	//psxCounters[3].count = psxRcntRcount32(3);
	psxCounters[3].rate = 1;
	psxCounters[3].mode|= 0x0400;
	if(value & 0x100)psxCounters[3].rate = PSXHBLANK;
  
	/*if(psxCounters[3].mode & 0x1){
		//psxCounters[3].mode &= ~0x18101C00;
		psxRcntReset32(3);
	} else {*/
	psxCounters[3].count = 0;
	psxRcntUpd32(3);
	psxRcntSet();
	//}
}

void psxRcnt4Wmode(u32 value)  {
#ifdef PSXCNT_LOG
	PSXCNT_LOG("IOP writeCmode[4] = %lx\n", value);
#endif
	if (value & 0x1c00) {
		//SysPrintf("Counter 4 Value write %x\n", value & 0x1c00);
	}
	/*if ((value & 0x3ff) == (psxCounters[4].mode & 0x3ff)) {
		return;
	}*/
	psxCounters[4].mode = value;
	psxCounters[4].mode|= 0x0400;
	//psxCounters[4].count = psxRcntRcount32(4);
	switch(value & 0x6000){
		case 0x0000:
            psxCounters[4].rate = 1;
			break;
		case 0x2000:
			psxCounters[4].rate = 8;
			break;
		case 0x4000:
			psxCounters[4].rate = 16;
			break;
		case 0x6000:
			psxCounters[4].rate = 256;
			break;
	}
	// Need to set a rate and target
if(psxCounters[4].mode & 0x1){
	SysPrintf("Gate set on IOP C4, disabling\n");
	psxCounters[4].mode|= 0x1000000;
}
	psxCounters[4].count = 0;
	psxRcntUpd32(4);
	psxRcntSet();
}

void psxRcnt5Wmode(u32 value)  {
#ifdef PSXCNT_LOG
	PSXCNT_LOG("IOP writeCmode[5] = %lx\n", value);
#endif
	if (value & 0x1c00) {
		//SysPrintf("Counter 5 Value write %x\n", value & 0x1c00);
	}
	/*if ((value & 0x3ff) == (psxCounters[5].mode & 0x3ff)) {
		return;
	}*/
#ifdef PCSX2_DEVBUILD
	//SysPrintf("IOP writeCmode[5] = %lx\n", value);
#endif
	psxCounters[5].mode = value;
	psxCounters[5].mode|= 0x0400;
	//psxCounters[5].count = psxRcntRcount32(5);
	switch(value & 0x6000){
		case 0x0000:
            psxCounters[5].rate = 1;
			break;
		case 0x2000:
			psxCounters[5].rate = 8;
			break;
		case 0x4000:
			psxCounters[5].rate = 16;
			break;
		case 0x6000:
			psxCounters[5].rate = 256;
			break;
	}
	// Need to set a rate and target
if(psxCounters[5].mode & 0x1){
	SysPrintf("Gate set on IOP C5, disabling\n");
	psxCounters[5].mode|= 0x1000000;
}
	psxCounters[5].count = 0;
	psxRcntUpd32(5);
	psxRcntSet();
}

void psxRcntWtarget16(int index, u32 value) {
/*#ifdef PSXCNT_LOG
	PSXCNT_LOG("writeCtarget[%ld] = %lx\n", index, value);
#endif*/
	psxCounters[index].target = value;
	//psxCounters[index].mode &= ~0x08000800;
	//psxCounters[index].sCycleT = psxRegs.cycle;
	psxCounters[index].CycleT = ((psxCounters[index].target - psxRcntRcount16(index)) * psxCounters[index].rate);
//	psxRcntUpd16(index);
	psxRcntSet();
}

void psxRcntWtarget32(int index, u32 value) {
	psxCounters[index].target = value;
	//psxCounters[index].mode &= ~0x08000800;
	//psxCounters[index].sCycleT = psxRegs.cycle;
	psxCounters[index].CycleT = ((psxCounters[index].target - (u32)psxRcntRcount32(index)) * psxCounters[index].rate);
/*#ifdef PSXCNT_LOG
	PSXCNT_LOG("writeCtarget32[%ld] = %lx (count=%lx) ; sCycleT: %x CycleT: %x\n",
			   index, value, psxRcntRcount32(index), psxCounters[index].sCycleT, psxCounters[index].CycleT);
#endif*/
//	psxRcntUpd32(index);
	psxRcntSet();
}

u16 psxRcntRcount16(int index) {
	if(psxCounters[index].mode & 0x1000000) return 0;
	return (u16)(psxCounters[index].count + ((psxRegs.cycle - psxCounters[index].sCycleT) / psxCounters[index].rate));
}

u32 psxRcntRcount32(int index) {
	if(psxCounters[index].mode & 0x1000000) return 0;
	return (u32)(psxCounters[index].count + ((psxRegs.cycle - psxCounters[index].sCycleT) / psxCounters[index].rate));
}

u64 psxRcntCycles(int index) {
	if(psxCounters[index].mode & 0x1000000) return 0;
	return (u64)(psxCounters[index].count + ((psxRegs.cycle - psxCounters[index].sCycleT) / psxCounters[index].rate));
}

int psxRcntFreeze(gzFile f, int Mode) {
	gzfreezel(psxCounters);

	return 0;
}
