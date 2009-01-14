/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "PrecompiledHeader.h"

#include "Common.h"
#include "R5900.h"
#include "InterTables.h"
#include "VUmicro.h"
#include "ix86/ix86.h"

#include <float.h>

extern int vu0branch, vu1branch;

namespace R5900
{
	const char * const bios[256]=
	{
	//0x00
		"RFU000_FullReset", "ResetEE",				"SetGsCrt",				"RFU003",
		"Exit",				"RFU005",				"LoadExecPS2",			"ExecPS2",
		"RFU008",			"RFU009",				"AddSbusIntcHandler",	"RemoveSbusIntcHandler", 
		"Interrupt2Iop",	"SetVTLBRefillHandler", "SetVCommonHandler",	"SetVInterruptHandler", 
	//0x10
		"AddIntcHandler",	"RemoveIntcHandler",	"AddDmacHandler",		"RemoveDmacHandler",
		"_EnableIntc",		"_DisableIntc",			"_EnableDmac",			"_DisableDmac",
		"_SetAlarm",		"_ReleaseAlarm",		"_iEnableIntc",			"_iDisableIntc",
		"_iEnableDmac",		"_iDisableDmac",		"_iSetAlarm",			"_iReleaseAlarm", 
	//0x20
		"CreateThread",			"DeleteThread",		"StartThread",			"ExitThread", 
		"ExitDeleteThread",		"TerminateThread",	"iTerminateThread",		"DisableDispatchThread",
		"EnableDispatchThread",		"ChangeThreadPriority", "iChangeThreadPriority",	"RotateThreadReadyQueue",
		"iRotateThreadReadyQueue",	"ReleaseWaitThread",	"iReleaseWaitThread",		"GetThreadId", 
	//0x30
		"ReferThreadStatus","iReferThreadStatus",	"SleepThread",		"WakeupThread",
		"_iWakeupThread",   "CancelWakeupThread",	"iCancelWakeupThread",	"SuspendThread",
		"iSuspendThread",   "ResumeThread",		"iResumeThread",	"JoinThread",
		"RFU060",	    "RFU061",			"EndOfHeap",		 "RFU063", 
	//0x40
		"CreateSema",	    "DeleteSema",	"SignalSema",		"iSignalSema", 
		"WaitSema",	    "PollSema",		"iPollSema",		"ReferSemaStatus", 
		"iReferSemaStatus", "RFU073",		"SetOsdConfigParam", 	"GetOsdConfigParam",
		"GetGsHParam",	    "GetGsVParam",	"SetGsHParam",		"SetGsVParam",
	//0x50
		"RFU080_CreateEventFlag",	"RFU081_DeleteEventFlag", 
		"RFU082_SetEventFlag",		"RFU083_iSetEventFlag", 
		"RFU084_ClearEventFlag",	"RFU085_iClearEventFlag", 
		"RFU086_WaitEventFlag",		"RFU087_PollEventFlag", 
		"RFU088_iPollEventFlag",	"RFU089_ReferEventFlagStatus", 
		"RFU090_iReferEventFlagStatus", "RFU091_GetEntryAddress", 
		"EnableIntcHandler_iEnableIntcHandler", 
		"DisableIntcHandler_iDisableIntcHandler", 
		"EnableDmacHandler_iEnableDmacHandler", 
		"DisableDmacHandler_iDisableDmacHandler", 
	//0x60
		"KSeg0",				"EnableCache",	"DisableCache",			"GetCop0",
		"FlushCache",			"RFU101",		"CpuConfig",			"iGetCop0",
		"iFlushCache",			"RFU105",		"iCpuConfig", 			"sceSifStopDma",
		"SetCPUTimerHandler",	"SetCPUTimer",	"SetOsdConfigParam2",	"SetOsdConfigParam2",
	//0x70
		"GsGetIMR_iGsGetIMR",				"GsGetIMR_iGsPutIMR",	"SetPgifHandler", 				"SetVSyncFlag",
		"RFU116",							"print", 				"sceSifDmaStat_isceSifDmaStat", "sceSifSetDma_isceSifSetDma", 
		"sceSifSetDChain_isceSifSetDChain", "sceSifSetReg",			"sceSifGetReg",					"ExecOSD", 
		"Deci2Call",						"PSMode",				"MachineType",					"GetMemorySize", 
	};

	const OPCODE& GetCurrentInstruction()
	{
		const OPCODE* opcode = &R5900::OpcodeTables::tbl_Standard[_Opcode_];

		while( opcode->getsubclass != NULL )
			opcode = &opcode->getsubclass();

		return *opcode;
	}

namespace Interpreter
{
static int branch2 = 0;

// These macros are used to assemble the repassembler functions

#ifdef PCSX2_DEVBUILD
static void debugI()
{
	//CPU_LOG("%s\n", disR5900Current.getCString());
	if (cpuRegs.GPR.n.r0.UD[0] || cpuRegs.GPR.n.r0.UD[1]) Console::Error("R0 is not zero!!!!");
}
#else
static void debugI() {}
#endif

static u32 cpuBlockCycles = 0;		// 3 bit fixed point version of cycle count

static std::string disOut;

static __forceinline void execI()
{
#ifdef _DEBUG
    if (memRead32(cpuRegs.pc, &cpuRegs.code) == -1) return;
	debugI();
#else
    cpuRegs.code = *(u32 *)PSM(cpuRegs.pc);
#endif

	const OPCODE& opcode = GetCurrentInstruction();

	cpuBlockCycles += opcode.cycles;
	cpuRegs.pc += 4;

	opcode.interpret();
}

static bool EventRaised = false;

static __forceinline void _doBranch_shared(u32 tar)
{
	// fixme: first off, cpuRegs.pc is assigned after execI(), which breaks exceptions
	// that might be thrown by execI().  I need to research how exceptions work again,
	// and make sure I record the correct PC

	branch2 = cpuRegs.branch = 1;
	const u32 oldBranchPC = cpuRegs.pc;
	execI();

	// branch being 0 means an exception was thrown, since only the exception
	// handler should ever clear it.

	if( cpuRegs.branch != 0 )
	{
		cpuRegs.pc = tar;
		cpuRegs.branch = 0;
	}
}

static void __fastcall doBranch( u32 target )
{
	_doBranch_shared( target );
	cpuRegs.cycle += cpuBlockCycles >> 3;
	cpuBlockCycles &= (1<<3)-1;
	EventRaised |= intEventTest();
}

void __fastcall intDoBranch(u32 target)
{
	//SysPrintf("Interpreter Branch \n");
	_doBranch_shared( target );
}

void intSetBranch() {
	branch2 = 1;
}

void COP1_Unknown() {
	FPU_LOG("Unknown FPU opcode called\n");
}

void COP0_Unknown(){
	CPU_LOG("COP0 Unknown opcode called\n");
}

namespace OpcodeImpl
{
void COP2()
{
	std::string disOut;
	disR5900Fasm(disOut, cpuRegs.code, cpuRegs.pc);

	VU0_LOG("%s\n", disOut.c_str());
	Int_COP2PrintTable[_Rs_]();
}

void Unknown() {
	CPU_LOG("%8.8lx: Unknown opcode called\n", cpuRegs.pc);
}

void MMI_Unknown() { Console::Notice("Unknown MMI opcode called"); }
void COP0_Unknown() { Console::Notice("Unknown COP0 opcode called"); }
void COP1_Unknown() { Console::Notice("Unknown FPU/COP1 opcode called"); }


/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/
void ADDI() 	{ if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = cpuRegs.GPR.r[_Rs_].SL[0] + _Imm_; }// Rt = Rs + Im signed!!!!
void ADDIU()    { if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = cpuRegs.GPR.r[_Rs_].SL[0] + _Imm_; }// Rt = Rs + Im signed !!!
void DADDI()    { if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = cpuRegs.GPR.r[_Rs_].SD[0] + _Imm_; }// Rt = Rs + Im 
void DADDIU()   { if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = cpuRegs.GPR.r[_Rs_].SD[0] + _Imm_; }// Rt = Rs + Im 
void ANDI() 	{ if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0] & (u64)_ImmU_; } // Rt = Rs And Im (zero-extended)
void ORI() 	    { if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0] | (u64)_ImmU_; } // Rt = Rs Or  Im (zero-extended)
void XORI() 	{ if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0] ^ (u64)_ImmU_; } // Rt = Rs Xor Im (zero-extended)
void SLTI()     { if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = cpuRegs.GPR.r[_Rs_].SD[0] < (s64)(_Imm_); } // Rt = Rs < Im (signed)
void SLTIU()    { if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0] < (u64)(_Imm_); } // Rt = Rs < Im (unsigned)

/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/
void ADD()	    { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].SL[0]  + cpuRegs.GPR.r[_Rt_].SL[0];}	// Rd = Rs + Rt		(Exception on Integer Overflow)
void ADDU() 	{ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].SL[0]  + cpuRegs.GPR.r[_Rt_].SL[0];}	// Rd = Rs + Rt
void DADD()     { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].SD[0]  + cpuRegs.GPR.r[_Rt_].SD[0]; }
void DADDU()    { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].SD[0]  + cpuRegs.GPR.r[_Rt_].SD[0]; }
void SUB() 	    { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].SL[0]  - cpuRegs.GPR.r[_Rt_].SL[0];}	// Rd = Rs - Rt		(Exception on Integer Overflow)
void SUBU() 	{ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].SL[0]  - cpuRegs.GPR.r[_Rt_].SL[0]; }	// Rd = Rs - Rt
void DSUB() 	{ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].SD[0]  - cpuRegs.GPR.r[_Rt_].SD[0];}	
void DSUBU() 	{ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].SD[0]  - cpuRegs.GPR.r[_Rt_].SD[0]; }
void AND() 	    { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0]  & cpuRegs.GPR.r[_Rt_].UD[0]; }	// Rd = Rs And Rt
void OR() 	    { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0]  | cpuRegs.GPR.r[_Rt_].UD[0]; }	// Rd = Rs Or  Rt
void XOR() 	    { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0]  ^ cpuRegs.GPR.r[_Rt_].UD[0]; }	// Rd = Rs Xor Rt
void NOR() 	    { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] =~(cpuRegs.GPR.r[_Rs_].UD[0] | cpuRegs.GPR.r[_Rt_].UD[0]); }// Rd = Rs Nor Rt
void SLT()		{ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].SD[0] < cpuRegs.GPR.r[_Rt_].SD[0]; }	// Rd = Rs < Rt (signed)
void SLTU()		{ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0] < cpuRegs.GPR.r[_Rt_].UD[0]; }	// Rd = Rs < Rt (unsigned)

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/

void J()   {
#ifdef _DEBUG
	u32 temp = _JumpTarget_;
	u32 pc = cpuRegs.pc;
#endif
	doBranch(_JumpTarget_);
#ifdef _DEBUG
	JumpCheckSym(temp, pc);
#endif
}

void JAL() {
#ifdef _DEBUG
	u32 temp = _JumpTarget_;
	u32 pc = cpuRegs.pc;
#endif
	_SetLink(31); doBranch(_JumpTarget_);
#ifdef _DEBUG
	JumpCheckSym(temp, pc);
#endif
}

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
void JR()   { 
#ifdef _DEBUG
	u32 temp = cpuRegs.GPR.r[_Rs_].UL[0];
	u32 pc = cpuRegs.pc;
	int rs = _Rs_;
#endif
	doBranch(cpuRegs.GPR.r[_Rs_].UL[0]); 
#ifdef _DEBUG
	JumpCheckSym(temp, pc);
	if (rs == 31) JumpCheckSymRet(pc);
#endif
}

void JALR() { 
	u32 temp = cpuRegs.GPR.r[_Rs_].UL[0];
#ifdef _DEBUG
	u32 pc = cpuRegs.pc;
#endif

	if (_Rd_) { _SetLink(_Rd_); }
	doBranch(temp);
#ifdef _DEBUG
	JumpCheckSym(temp, pc);
#endif
}


/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/
void DIV() {
    if (cpuRegs.GPR.r[_Rt_].SL[0] != 0) {
        cpuRegs.LO.SD[0] = cpuRegs.GPR.r[_Rs_].SL[0] / cpuRegs.GPR.r[_Rt_].SL[0];
        cpuRegs.HI.SD[0] = cpuRegs.GPR.r[_Rs_].SL[0] % cpuRegs.GPR.r[_Rt_].SL[0];
    }
}

void DIVU() {
	if (cpuRegs.GPR.r[_Rt_].UL[0] != 0) {
		cpuRegs.LO.SD[0] = cpuRegs.GPR.r[_Rs_].UL[0] / cpuRegs.GPR.r[_Rt_].UL[0];
		cpuRegs.HI.SD[0] = cpuRegs.GPR.r[_Rs_].UL[0] % cpuRegs.GPR.r[_Rt_].UL[0];
	}
}

void MULT() { //different in ps2...
	s64 res = (s64)cpuRegs.GPR.r[_Rs_].SL[0] * (s64)cpuRegs.GPR.r[_Rt_].SL[0];

	cpuRegs.LO.UD[0] = (s32)(res & 0xffffffff);
	cpuRegs.HI.UD[0] = (s32)(res >> 32);

	if (!_Rd_) return;
	cpuRegs.GPR.r[_Rd_].UD[0]= cpuRegs.LO.UD[0]; //that is the difference
}

void MULTU() { //different in ps2..
	u64 res = (u64)cpuRegs.GPR.r[_Rs_].UL[0] * (u64)cpuRegs.GPR.r[_Rt_].UL[0];

	cpuRegs.LO.UD[0] = (s32)(res & 0xffffffff);
	cpuRegs.HI.UD[0] = (s32)(res >> 32);

	if (!_Rd_) return;
	cpuRegs.GPR.r[_Rd_].UD[0]= cpuRegs.LO.UD[0]; //that is the difference
}

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
void LUI()  { 
	if (!_Rt_) return; 
	cpuRegs.GPR.r[_Rt_].UD[0] = (s32)(cpuRegs.code << 16);
}

/*********************************************************
* Move from HI/LO to GPR                                 *
* Format:  OP rd                                         *
*********************************************************/
void MFHI() { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.HI.UD[0]; } // Rd = Hi
void MFLO() { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.LO.UD[0]; } // Rd = Lo

/*********************************************************
* Move to GPR to HI/LO & Register jump                   *
* Format:  OP rs                                         *
*********************************************************/
void MTHI() { cpuRegs.HI.UD[0] = cpuRegs.GPR.r[_Rs_].UD[0]; } // Hi = Rs
void MTLO() { cpuRegs.LO.UD[0] = cpuRegs.GPR.r[_Rs_].UD[0]; } // Lo = Rs


/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
void SLL()   { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = (s32)(cpuRegs.GPR.r[_Rt_].UL[0] << _Sa_); } // Rd = Rt << sa
void DSLL()  { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = (u64)(cpuRegs.GPR.r[_Rt_].UD[0] << _Sa_); }
void DSLL32(){ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = (u64)(cpuRegs.GPR.r[_Rt_].UD[0] << (_Sa_+32));}
void SRA()   { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = (s32)(cpuRegs.GPR.r[_Rt_].SL[0] >> _Sa_); } // Rd = Rt >> sa (arithmetic)
void DSRA()  { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = (u64)(cpuRegs.GPR.r[_Rt_].SD[0] >> _Sa_); }
void DSRA32(){ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = (u64)(cpuRegs.GPR.r[_Rt_].SD[0] >> (_Sa_+32));}
void SRL()   { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = (s32)(cpuRegs.GPR.r[_Rt_].UL[0] >> _Sa_); } // Rd = Rt >> sa (logical)
void DSRL()  { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = (u64)(cpuRegs.GPR.r[_Rt_].UD[0] >> _Sa_); }
void DSRL32(){ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = (u64)(cpuRegs.GPR.r[_Rt_].UD[0] >> (_Sa_+32));}

/*********************************************************
* Shift arithmetic with variant register shift           *
* Format:  OP rd, rt, rs                                 *
*********************************************************/
void SLLV() { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = (s32)(cpuRegs.GPR.r[_Rt_].UL[0] << (cpuRegs.GPR.r[_Rs_].UL[0] &0x1f));} // Rd = Rt << rs
void SRAV() { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = (s32)(cpuRegs.GPR.r[_Rt_].SL[0] >> (cpuRegs.GPR.r[_Rs_].UL[0] &0x1f));} // Rd = Rt >> rs (arithmetic)
void SRLV() { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = (s32)(cpuRegs.GPR.r[_Rt_].UL[0] >> (cpuRegs.GPR.r[_Rs_].UL[0] &0x1f));} // Rd = Rt >> rs (logical)
void DSLLV(){ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = (u64)(cpuRegs.GPR.r[_Rt_].UD[0] << (cpuRegs.GPR.r[_Rs_].UL[0] &0x3f));}  
void DSRAV(){ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = (s64)(cpuRegs.GPR.r[_Rt_].SD[0] >> (cpuRegs.GPR.r[_Rs_].UL[0] &0x3f));}
void DSRLV(){ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = (u64)(cpuRegs.GPR.r[_Rt_].UD[0] >> (cpuRegs.GPR.r[_Rs_].UL[0] &0x3f));}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
#define RepBranchi32(op) \
	if (cpuRegs.GPR.r[_Rs_].SD[0] op cpuRegs.GPR.r[_Rt_].SD[0]) doBranch(_BranchTarget_); \
	else intEventTest();


void BEQ() {	RepBranchi32(==) }  // Branch if Rs == Rt
void BNE() {	RepBranchi32(!=) }  // Branch if Rs != Rt

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/
#define RepZBranchi32(op) \
	if(cpuRegs.GPR.r[_Rs_].SD[0] op 0) { \
		doBranch(_BranchTarget_); \
	}

#define RepZBranchLinki32(op) \
	_SetLink(31); \
	if(cpuRegs.GPR.r[_Rs_].SD[0] op 0) { \
		doBranch(_BranchTarget_); \
	}

void BGEZ()   { RepZBranchi32(>=) }      // Branch if Rs >= 0
void BGEZAL() { RepZBranchLinki32(>=) }  // Branch if Rs >= 0 and link
void BGTZ()   { RepZBranchi32(>) }       // Branch if Rs >  0
void BLEZ()   { RepZBranchi32(<=) }      // Branch if Rs <= 0
void BLTZ()   { RepZBranchi32(<) }       // Branch if Rs <  0
void BLTZAL() { RepZBranchLinki32(<) }   // Branch if Rs <  0 and link


/*********************************************************
* Register branch logic  Likely                          *
* Format:  OP rs, offset                                 *
*********************************************************/
#define RepZBranchi32Likely(op) \
	if(cpuRegs.GPR.r[_Rs_].SD[0] op 0) { \
		doBranch(_BranchTarget_); \
	} else { cpuRegs.pc +=4; intEventTest(); }

#define RepZBranchLinki32Likely(op) \
	_SetLink(31); \
	if(cpuRegs.GPR.r[_Rs_].SD[0] op 0) { \
		doBranch(_BranchTarget_); \
	} else { cpuRegs.pc +=4; intEventTest(); }

#define RepBranchi32Likely(op) \
	if(cpuRegs.GPR.r[_Rs_].SD[0] op cpuRegs.GPR.r[_Rt_].SD[0]) { \
		doBranch(_BranchTarget_); \
	} else { cpuRegs.pc +=4; intEventTest(); }


void BEQL()    {  RepBranchi32Likely(==)      }  // Branch if Rs == Rt
void BNEL()    {  RepBranchi32Likely(!=)      }  // Branch if Rs != Rt
void BLEZL()   {  RepZBranchi32Likely(<=)     }  // Branch if Rs <= 0
void BGTZL()   {  RepZBranchi32Likely(>)      }  // Branch if Rs >  0
void BLTZL()   {  RepZBranchi32Likely(<)      }  // Branch if Rs <  0
void BGEZL()   {  RepZBranchi32Likely(>=)     }  // Branch if Rs >= 0
void BLTZALL() {  RepZBranchLinki32Likely(<)  }  // Branch if Rs <  0 and link
void BGEZALL() {  RepZBranchLinki32Likely(>=) }  // Branch if Rs >= 0 and link

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

void LB() {
	u32 addr;

	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u8 temp;
	const u32 rt=_Rt_;
	if ((0==memRead8(addr, &temp)) && (rt!=0))
	{
		cpuRegs.GPR.r[rt].UD[0]=(s8)temp;
	}
}

void LBU() { 
	u32 addr;

	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u8 temp;
	const u32 rt=_Rt_;
	if ((0==memRead8(addr, &temp)) && (rt!=0))
	{
		cpuRegs.GPR.r[rt].UD[0]=temp;
	}
}

void LH() { 
	u32 addr;

	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u16 temp;
	const u32 rt=_Rt_;
	if ((0==memRead16(addr, &temp)) && (rt!=0))
	{
		cpuRegs.GPR.r[rt].UD[0]=(s16)temp;
	}
}

void LHU() { 
	u32 addr;

	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u16 temp;
	const u32 rt=_Rt_;
	if ((0==memRead16(addr, &temp)) && (rt!=0))
	{
		cpuRegs.GPR.r[rt].UD[0]=temp;
	}
}


void LW() {
	u32 addr;

	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;

	u32 temp;
	const u32 rt=_Rt_;
	if ((0==memRead32(addr, &temp)) && (rt!=0))
	{
		cpuRegs.GPR.r[rt].UD[0]=(s32)temp;
	}
}

void LWU() { 
	u32 addr;
	
	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;

	u32 temp;
	const u32 rt=_Rt_;
	if ((0==memRead32(addr, &temp)) && (rt!=0))
	{
		cpuRegs.GPR.r[rt].UD[0]=temp;
	}
}

u32 LWL_MASK[4] = { 0xffffff, 0xffff, 0xff, 0 };
u32 LWL_SHIFT[4] = { 24, 16, 8, 0 };

void LWL() {
	s32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 3;
	u32 mem;

	if (!_Rt_) return;
	if (memRead32(addr & ~3, &mem) == -1) return;
	cpuRegs.GPR.r[_Rt_].UD[0] =	(cpuRegs.GPR.r[_Rt_].UL[0] & LWL_MASK[shift]) | 
								(mem << LWL_SHIFT[shift]);

	/*
	Mem = 1234.  Reg = abcd

	0   4bcd   (mem << 24) | (reg & 0x00ffffff)
	1   34cd   (mem << 16) | (reg & 0x0000ffff)
	2   234d   (mem <<  8) | (reg & 0x000000ff)
	3   1234   (mem      ) | (reg & 0x00000000)
	*/
}

u32 LWR_MASK[4] = { 0, 0xff000000, 0xffff0000, 0xffffff00 };
u32 LWR_SHIFT[4] = { 0, 8, 16, 24 };

void LWR() {
	s32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 3;
	u32 mem;

	if (!_Rt_) return;
	if (memRead32(addr & ~3, &mem) == -1) return;
	cpuRegs.GPR.r[_Rt_].UD[0] =	(cpuRegs.GPR.r[_Rt_].UL[0] & LWR_MASK[shift]) | 
								(mem >> LWR_SHIFT[shift]);

	/*
	Mem = 1234.  Reg = abcd

	0   1234   (mem      ) | (reg & 0x00000000)
	1   a123   (mem >>  8) | (reg & 0xff000000)
	2   ab12   (mem >> 16) | (reg & 0xffff0000)
	3   abc1   (mem >> 24) | (reg & 0xffffff00)
	*/
}

void LD() {
    s32 addr;

	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	if (_Rt_) {
		memRead64(addr, &cpuRegs.GPR.r[_Rt_].UD[0]);
	} else {
		u64 dummy;
		memRead64(addr, &dummy);
	}
}

u64 LDL_MASK[8] = { 0x00ffffffffffffffLL, 0x0000ffffffffffffLL, 0x000000ffffffffffLL, 0x00000000ffffffffLL, 
					0x0000000000ffffffLL, 0x000000000000ffffLL, 0x00000000000000ffLL, 0x0000000000000000LL };
u32 LDL_SHIFT[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };

void LDL() {
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 7;
	u64 mem;

	if (!_Rt_) return;
	if (memRead64(addr & ~7, &mem) == -1) return;
	cpuRegs.GPR.r[_Rt_].UD[0] =	(cpuRegs.GPR.r[_Rt_].UD[0] & LDL_MASK[shift]) | 
								(mem << LDL_SHIFT[shift]);
}

u64 LDR_MASK[8] = { 0x0000000000000000LL, 0xff00000000000000LL, 0xffff000000000000LL, 0xffffff0000000000LL,
					0xffffffff00000000LL, 0xffffffffff000000LL, 0xffffffffffff0000LL, 0xffffffffffffff00LL };
u32 LDR_SHIFT[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };

void LDR() {  
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 7;
	u64 mem;

	if (!_Rt_) return;
	if (memRead64(addr & ~7, &mem) == -1) return;
	cpuRegs.GPR.r[_Rt_].UD[0] =	(cpuRegs.GPR.r[_Rt_].UD[0] & LDR_MASK[shift]) | 
								(mem >> LDR_SHIFT[shift]);
}

void LQ() {
	u32 addr;

	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	addr&=~0xf;

	if (_Rt_) {
		memRead128(addr, &cpuRegs.GPR.r[_Rt_].UD[0]);
	} else {
		u64 val[2];
		memRead128(addr, val);
	}
}

void SB() { 
	u32 addr;

	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
    memWrite8(addr, cpuRegs.GPR.r[_Rt_].UC[0]); 
}

void SH() { 
	u32 addr;

	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	memWrite16(addr, cpuRegs.GPR.r[_Rt_].US[0]); 
}

void SW(){  
	u32 addr;

	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
    memWrite32(addr, cpuRegs.GPR.r[_Rt_].UL[0]); 
}

u32 SWL_MASK[4] = { 0xffffff00, 0xffff0000, 0xff000000, 0x00000000 };
u32 SWL_SHIFT[4] = { 24, 16, 8, 0 };

void SWL() {
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 3;
	u32 mem;

	if (memRead32(addr & ~3, &mem) == -1) return;

	memWrite32(addr & ~3,  (cpuRegs.GPR.r[_Rt_].UL[0] >> SWL_SHIFT[shift]) |
		      (  mem & SWL_MASK[shift]) );
	/*
	Mem = 1234.  Reg = abcd

	0   123a   (reg >> 24) | (mem & 0xffffff00)
	1   12ab   (reg >> 16) | (mem & 0xffff0000)
	2   1abc   (reg >>  8) | (mem & 0xff000000)
	3   abcd   (reg      ) | (mem & 0x00000000)
	*/
}

u32 SWR_MASK[4] = { 0x00000000, 0x000000ff, 0x0000ffff, 0x00ffffff };
u32 SWR_SHIFT[4] = { 0, 8, 16, 24 };

void SWR() {
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 3;
	u32 mem;

	if (memRead32(addr & ~3, &mem) == -1) return;

	memWrite32(addr & ~3,  (cpuRegs.GPR.r[_Rt_].UL[0] << SWR_SHIFT[shift]) |
		      ( mem & SWR_MASK[shift]) );

	/*
	Mem = 1234.  Reg = abcd

	0   abcd   (reg      ) | (mem & 0x00000000)
	1   bcd4   (reg <<  8) | (mem & 0x000000ff)
	2   cd34   (reg << 16) | (mem & 0x0000ffff)
	3   d234   (reg << 24) | (mem & 0x00ffffff)
	*/
}

void SD() {
	u32 addr;

	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
    memWrite64(addr,&cpuRegs.GPR.r[_Rt_].UD[0]); 
}

u64 SDL_MASK[8] = { 0xffffffffffffff00LL, 0xffffffffffff0000LL, 0xffffffffff000000LL, 0xffffffff00000000LL, 
					0xffffff0000000000LL, 0xffff000000000000LL, 0xff00000000000000LL, 0x0000000000000000LL };
u32 SDL_SHIFT[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };

void SDL() {
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 7;
	u64 mem;

	if (memRead64(addr & ~7, &mem) == -1) return;
	mem =(cpuRegs.GPR.r[_Rt_].UD[0] >> SDL_SHIFT[shift]) |
		      ( mem & SDL_MASK[shift]);
	memWrite64(addr & ~7, &mem);
}

u64 SDR_MASK[8] = { 0x0000000000000000LL, 0x00000000000000ffLL, 0x000000000000ffffLL, 0x0000000000ffffffLL,
					0x00000000ffffffffLL, 0x000000ffffffffffLL, 0x0000ffffffffffffLL, 0x00ffffffffffffffLL };
u32 SDR_SHIFT[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };

void SDR() {
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 7;
	u64 mem;

	if (memRead64(addr & ~7, &mem) == -1) return;
	mem=(cpuRegs.GPR.r[_Rt_].UD[0] << SDR_SHIFT[shift]) |
		      ( mem & SDR_MASK[shift]);
	memWrite64(addr & ~7, &mem );
}

void SQ() {
	u32 addr;

	addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	addr&=~0xf;
	memWrite128(addr, &cpuRegs.GPR.r[_Rt_].UD[0]);
}

/*********************************************************
* Conditional Move                                       *
* Format:  OP rd, rs, rt                                 *
*********************************************************/

void MOVZ() {
	if (!_Rd_) return;
	if (cpuRegs.GPR.r[_Rt_].UD[0] == 0) {
		cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0];
	}
}
void MOVN() {
	if (!_Rd_) return;
	if (cpuRegs.GPR.r[_Rt_].UD[0] != 0) {
		cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0];
	}
}

/*********************************************************
* Special purpose instructions                           *
* Format:  OP                                            *
*********************************************************/

#include "Sifcmd.h"
/*
int __Deci2Call(int call, u32 *addr);
*/
u32 *deci2addr = NULL;
u32 deci2handler;
char deci2buffer[256];

/*
 *	int Deci2Call(int, u_int *);
 */

int __Deci2Call(int call, u32 *addr) {
	if (call > 0x10) {
		return -1;
	}

	switch (call) {
		case 1: // open
			deci2addr = (u32*)PSM(addr[1]);
			BIOS_LOG("deci2open: %x,%x,%x,%x\n",
					 addr[3], addr[2], addr[1], addr[0]);
			deci2handler = addr[2];
			return 1;

		case 2: // close
			return 1;

		case 3: // reqsend
			BIOS_LOG("deci2reqsend: %x,%x,%x,%x: deci2addr: %x,%x,%x,buf=%x %x,%x,len=%x,%x\n",
					 addr[3], addr[2], addr[1], addr[0],
					 deci2addr[7], deci2addr[6], deci2addr[5], deci2addr[4],
					 deci2addr[3], deci2addr[2], deci2addr[1], deci2addr[0]);

//			cpuRegs.pc = deci2handler;
//			SysPrintf("deci2msg: %s", (char*)PSM(deci2addr[4]+0xc));
			if (deci2addr == NULL) return 1;
			if (deci2addr[1]>0xc){
				u8* pdeciaddr = (u8*)dmaGetAddr(deci2addr[4]+0xc);
				if( pdeciaddr == NULL ) pdeciaddr = (u8*)PSM(deci2addr[4]+0xc);
				else pdeciaddr += (deci2addr[4]+0xc)%16;
				memcpy(deci2buffer, pdeciaddr, deci2addr[1]-0xc);
				deci2buffer[deci2addr[1]-0xc>=255?255:deci2addr[1]-0xc]='\0';
				Console::Write( Color_Cyan, deci2buffer );
			}
			deci2addr[3] = 0;
			return 1;

		case 4: // poll
			BIOS_LOG("deci2poll: %x,%x,%x,%x\n",
					 addr[3], addr[2], addr[1], addr[0]);
			return 1;

		case 5: // exrecv
			return 1;

		case 6: // exsend
			return 1;

		case 0x10://kputs
			Console::Write( Color_Cyan, "%s", params PSM(*addr));
			return 1;
	}

	return 0;
}


void SYSCALL() {
#ifdef BIOS_LOG
	u8 call;

		if (cpuRegs.GPR.n.v1.SL[0] < 0)
			 call = (u8)(-cpuRegs.GPR.n.v1.SL[0]);
		else call = cpuRegs.GPR.n.v1.UC[0];
		BIOS_LOG("Bios call: %s (%x)\n", bios[call], call);
		if (call == 0x7c && cpuRegs.GPR.n.a0.UL[0] == 0x10) {
			Console::Write( Color_Cyan, "%s", params PSM(PSMu32(cpuRegs.GPR.n.a1.UL[0])));
		} else
		//if (call == 0x7c) SysPrintf("Deci2Call: %x\n", cpuRegs.GPR.n.a0.UL[0]);
		if (call == 0x7c) __Deci2Call(cpuRegs.GPR.n.a0.UL[0], (u32*)PSM(cpuRegs.GPR.n.a1.UL[0]));
		if (call == 0x77) {
	struct t_sif_dma_transfer *dmat;
//	struct t_sif_cmd_header	*hdr;
//	struct t_sif_rpc_bind *bind;
//	struct t_rpc_server_data *server;
	int n_transfer;
	u32 addr;
//	int sid;

	n_transfer = cpuRegs.GPR.n.a1.UL[0] - 1;
	if (n_transfer >= 0) {
	addr = cpuRegs.GPR.n.a0.UL[0] + n_transfer * sizeof(struct t_sif_dma_transfer);
	dmat = (struct t_sif_dma_transfer*)PSM(addr);

	BIOS_LOG("bios_%s: n_transfer=%d, size=%x, attr=%x, dest=%x, src=%x\n",
			 bios[cpuRegs.GPR.n.v1.UC[0]], n_transfer,
			 dmat->size, dmat->attr,
			 dmat->dest, dmat->src);
	}
//Log=1;
		}
#endif
//	if (cpuRegs.GPR.n.v1.UD[0] == 0x77) Log=1;
	cpuRegs.pc -= 4;
	cpuException(0x20, cpuRegs.branch);
}

void BREAK(void) {
	cpuRegs.pc -= 4;
	cpuException(0x24, cpuRegs.branch);
}

void MFSA( void ) {
	if (!_Rd_) return;
	cpuRegs.GPR.r[_Rd_].SD[0] = (s64)cpuRegs.sa;
}

void MTSA( void ) {
	cpuRegs.sa = (s32)cpuRegs.GPR.r[_Rs_].SD[0];
}

void SYNC( void )
{
}

void PREF( void ) 
{
}



/*********************************************************
* Register trap                                          *
* Format:  OP rs, rt                                     *
*********************************************************/

void TGE() {
    if (cpuRegs.GPR.r[_Rs_].SD[0]>= cpuRegs.GPR.r[_Rt_].SD[0]) {
		cpuException(EXC_CODE_Tr, cpuRegs.branch);
	}
	//SysPrintf( "TrapInstruction: TGE\n" );
}

void TGEU() {
	if (cpuRegs.GPR.r[_Rs_].UD[0]>= cpuRegs.GPR.r[_Rt_].UD[0]) {
		cpuException(EXC_CODE_Tr, cpuRegs.branch);
	}
	//SysPrintf( "TrapInstruction: TGEU\n" );
}

void TLT() {
	if (cpuRegs.GPR.r[_Rs_].SD[0] < cpuRegs.GPR.r[_Rt_].SD[0]) {
		cpuException(EXC_CODE_Tr, cpuRegs.branch);
	}
	//SysPrintf( "TrapInstruction: TLT\n" );
}

void TLTU() {
	if (cpuRegs.GPR.r[_Rs_].UD[0] < cpuRegs.GPR.r[_Rt_].UD[0]) {
		cpuException(EXC_CODE_Tr, cpuRegs.branch);
	}
	//SysPrintf( "TrapInstruction: TLTU\n" );
}

void TEQ() {
	if (cpuRegs.GPR.r[_Rs_].SD[0] == cpuRegs.GPR.r[_Rt_].SD[0]) {
		cpuException(EXC_CODE_Tr, cpuRegs.branch);
	}
	//SysPrintf( "TrapInstruction: TEQ\n" );
}

void TNE() {
	if (cpuRegs.GPR.r[_Rs_].SD[0] != cpuRegs.GPR.r[_Rt_].SD[0]) {
		cpuException(EXC_CODE_Tr, cpuRegs.branch);
	}
	//SysPrintf( "TrapInstruction: TNE\n" );
}

/*********************************************************
* Trap with immediate operand                            *
* Format:  OP rs, rt                                     *
*********************************************************/

void TGEI() {

	if (cpuRegs.GPR.r[_Rs_].SD[0] >= _Imm_) {
		cpuException(EXC_CODE_Tr, cpuRegs.branch);
	}
	//SysPrintf( "TrapInstruction: Immediate\n" );
}

void TGEIU() {
	if (cpuRegs.GPR.r[_Rs_].UD[0] >= _ImmU_) {
		cpuException(EXC_CODE_Tr, cpuRegs.branch);
	}
	//SysPrintf( "TrapInstruction: Immediate\n" );
}

void TLTI() {
	if(cpuRegs.GPR.r[_Rs_].SD[0] < _Imm_) {
		cpuException(EXC_CODE_Tr, cpuRegs.branch);
	}
	//SysPrintf( "TrapInstruction: Immediate\n" );
}

void TLTIU() {
	if (cpuRegs.GPR.r[_Rs_].UD[0] < _ImmU_) {
		cpuException(EXC_CODE_Tr, cpuRegs.branch);
	}
	//SysPrintf( "TrapInstruction: Immediate\n" );
}

void TEQI() {
	if (cpuRegs.GPR.r[_Rs_].SD[0] == _Imm_) {
		cpuException(EXC_CODE_Tr, cpuRegs.branch);
	}
	//SysPrintf( "TrapInstruction: Immediate\n" );
}

void TNEI() {
	if (cpuRegs.GPR.r[_Rs_].SD[0] != _Imm_) {
		cpuException(EXC_CODE_Tr, cpuRegs.branch);
	}
	//SysPrintf( "TrapInstruction: Immediate\n" );
}

/*********************************************************
* Sa intructions                                         *
* Format:  OP rs, rt                                     *
*********************************************************/

void MTSAB() {
 	cpuRegs.sa = ((cpuRegs.GPR.r[_Rs_].UL[0] & 0xF) ^ (_Imm_ & 0xF)) << 3;
}

void MTSAH() {
    cpuRegs.sa = ((cpuRegs.GPR.r[_Rs_].UL[0] & 0x7) ^ (_Imm_ & 0x7)) << 4;
}

}	// end namespace R5900::Interpreter::OpcodeImpl

////////////////////////////////////////////////////////

void intAlloc()
{
	 // fixme : detect cpu for use the optimize asm code
}

void intReset()
{
	cpuRegs.branch = 0;
	branch2 = 0;
}

bool intEventTest()
{
	// Perform counters, ints, and IOP updates:
	return _cpuBranchTest_Shared();
}

void intExecute()
{
	g_EEFreezeRegs = false;

	// Mem protection should be handled by the caller here so that it can be
	// done in a more optimized fashion.

	EventRaised = false;

	while( !EventRaised )
	{
		execI();
	}
}

static void intExecuteBlock()
{
	g_EEFreezeRegs = false;

	PCSX2_MEM_PROTECT_BEGIN()
	branch2 = 0;
	while (!branch2) execI();
	PCSX2_MEM_PROTECT_END()
}

void intStep()
{
	g_EEFreezeRegs = false;
	execI();
}

void intClear(u32 Addr, u32 Size)
{
}

void intShutdown() {
}

}

using namespace Interpreter;

R5900cpu intCpu = {
	intAlloc,
	intReset,
	intStep,
	intExecute,
	intExecuteBlock,
	intClear,
	intShutdown
};

}