// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

/* TODO
 -Fix the flags Proper as they aren't handle now..
 -Add BC Table opcodes
 -Add Interlock in QMFC2,QMTC2,CFC2,CTC2
 -Finish instruction set
 -Bug Fixes!!!
*/

#include "Common.h"

#include <cmath>

#include "R5900OpcodeTables.h"
#include "VUmicro.h"
#include "Vif_Dma.h"
#include "MTVU.h"

#define _Ft_ _Rt_
#define _Fs_ _Rd_
#define _Fd_ _Sa_

#define _Fsf_ ((cpuRegs.code >> 21) & 0x03)
#define _Ftf_ ((cpuRegs.code >> 23) & 0x03)

using namespace R5900;

void COP2_BC2() { Int_COP2BC2PrintTable[_Rt_]();}
void COP2_SPECIAL() { _vu0FinishMicro(); Int_COP2SPECIAL1PrintTable[_Funct_]();}

void COP2_SPECIAL2() {
	Int_COP2SPECIAL2PrintTable[(cpuRegs.code & 0x3) | ((cpuRegs.code >> 4) & 0x7c)]();
}

void COP2_Unknown()
{
	CPU_LOG("Unknown COP2 opcode called");
}

//****************************************************************************

__fi void _vu0run(bool breakOnMbit, bool addCycles, bool sync_only) {

	if (!(VU0.VI[REG_VPU_STAT].UL & 1)) return;

	//VU0 is ahead of the EE and M-Bit is already encountered, so no need to wait for it, just catch up the EE
	if ((VU0.flags & VUFLAG_MFLAGSET) && breakOnMbit && (s32)(cpuRegs.cycle - VU0.cycle) <= 0)
	{
		cpuRegs.cycle = VU0.cycle;
		return;
	}

	if(!EmuConfig.Cpu.Recompiler.EnableEE)
		intUpdateCPUCycles();

	u32 startcycle = cpuRegs.cycle;
	s32 runCycles  = 0x7fffffff;

	if (sync_only)
	{
		runCycles  = (s32)(cpuRegs.cycle - VU0.cycle);

		if (runCycles < 0)
			return;
	}

	do { // Run VU until it finishes or M-Bit
		CpuVU0->Execute(runCycles);
	} while ((VU0.VI[REG_VPU_STAT].UL & 1)						// E-bit Termination
	  &&	!sync_only && (!breakOnMbit || (!(VU0.flags & VUFLAG_MFLAGSET) && (s32)(cpuRegs.cycle - VU0.cycle) > 0)));	// M-bit Break

	// Add cycles if called from EE's COP2
	if (addCycles)
	{
		cpuRegs.cycle += (VU0.cycle - startcycle);
		CpuVU1->ExecuteBlock(0); // Catch up VU1 as it's likely fallen behind

		if(VU0.VI[REG_VPU_STAT].UL & 1)
			cpuSetNextEventDelta(4);
	}
}

void _vu0WaitMicro()   { _vu0run(1, 1, 0); } // Runs VU0 Micro Until E-bit or M-Bit End
void _vu0FinishMicro() { _vu0run(0, 1, 0); } // Runs VU0 Micro Until E-Bit End
void vu0Finish()	   { _vu0run(0, 0, 0); } // Runs VU0 Micro Until E-Bit End (doesn't stall EE)
void vu0Sync()		   { _vu0run(0, 0, 1); } // Runs VU0 until it catches up

namespace R5900 {
namespace Interpreter{
namespace OpcodeImpl
{
	void LQC2() {
		vu0Sync();
		u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + (s16)cpuRegs.code;
		if (_Ft_) {
			memRead128(addr, VU0.VF[_Ft_].UQ);
		} else {
			u128 val;
 			memRead128(addr, val);
		}
	}

	// Asadr.Changed
	//TODO: check this
	// HUH why ? doesn't make any sense ...
	void SQC2() {
		vu0Sync();
		u32 addr = _Imm_ + cpuRegs.GPR.r[_Rs_].UL[0];
		memWrite128(addr, VU0.VF[_Ft_].UQ);
	}
}}}


void QMFC2() {
	vu0Sync();

	if (cpuRegs.code & 1) {
		_vu0FinishMicro();
	}

	if (_Rt_ == 0) return;
	cpuRegs.GPR.r[_Rt_].UD[0] = VU0.VF[_Fs_].UD[0];
	cpuRegs.GPR.r[_Rt_].UD[1] = VU0.VF[_Fs_].UD[1];
}

void QMTC2() {
	vu0Sync();

	if (cpuRegs.code & 1) {
		_vu0WaitMicro();
	}

	if (_Fs_ == 0) return;
	VU0.VF[_Fs_].UD[0] = cpuRegs.GPR.r[_Rt_].UD[0];
	VU0.VF[_Fs_].UD[1] = cpuRegs.GPR.r[_Rt_].UD[1];
}

void CFC2() {
	vu0Sync();

	if (cpuRegs.code & 1) {
		_vu0FinishMicro();
	}

	if (_Rt_ == 0) return;

	if (_Fs_ == REG_R)
		cpuRegs.GPR.r[_Rt_].UL[0] = VU0.VI[REG_R].UL & 0x7FFFFF;
	else
	{
		cpuRegs.GPR.r[_Rt_].UL[0] = VU0.VI[_Fs_].UL;

		if (VU0.VI[_Fs_].UL & 0x80000000)
			cpuRegs.GPR.r[_Rt_].UL[1] = 0xffffffff;
		else
			cpuRegs.GPR.r[_Rt_].UL[1] = 0;
	}

}

void CTC2() {
	vu0Sync();

	if (cpuRegs.code & 1) {
		_vu0WaitMicro();
	}

	if (_Fs_ == 0) return;

	switch(_Fs_) {
		case REG_MAC_FLAG: // read-only
		case REG_TPC:      // read-only
		case REG_VPU_STAT: // read-only
			break;
		case REG_R:
			VU0.VI[REG_R].UL = ((cpuRegs.GPR.r[_Rt_].UL[0] & 0x7FFFFF) | 0x3F800000);
			break;
		case REG_FBRST:
			VU0.VI[REG_FBRST].UL = cpuRegs.GPR.r[_Rt_].UL[0] & 0x0C0C;
			if (cpuRegs.GPR.r[_Rt_].UL[0] & 0x1) { // VU0 Force Break
				Console.Error("fixme: VU0 Force Break");
			}
			if (cpuRegs.GPR.r[_Rt_].UL[0] & 0x2) { // VU0 Reset
				//Console.WriteLn("fixme: VU0 Reset");
				vu0ResetRegs();
			}
			if (cpuRegs.GPR.r[_Rt_].UL[0] & 0x100) { // VU1 Force Break
				Console.Error("fixme: VU1 Force Break");
			}
			if (cpuRegs.GPR.r[_Rt_].UL[0] & 0x200) { // VU1 Reset
//				Console.WriteLn("fixme: VU1 Reset");
				vu1ResetRegs();
			}
			break;
		case REG_CMSAR1: // REG_CMSAR1
			vu1Finish(true);
			vu1ExecMicro(cpuRegs.GPR.r[_Rt_].US[0]);	// Execute VU1 Micro SubRoutine
			break;
		default:
			VU0.VI[_Fs_].UL = cpuRegs.GPR.r[_Rt_].UL[0];
			break;
	}
}
