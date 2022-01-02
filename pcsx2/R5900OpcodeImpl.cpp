/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#include "PrecompiledHeader.h"
#include "Common.h"

#include <float.h>

#include "R5900.h"
#include "R5900OpcodeTables.h"
#include "R5900Exceptions.h"
#include "GS.h"
#include "CDVD/CDVD.h"
#include "ps2/BiosTools.h"

GS_VideoMode gsVideoMode = GS_VideoMode::Uninitialized;
bool gsIsInterlaced = false;

static __fi bool _add64_Overflow( s64 x, s64 y, s64 &ret )
{
	const s64 result = x + y;

	// Let's all give gigaherz a big round of applause for finding this gem,
	// which apparently works, and generates compact/fast x86 code too (the
	// other method below is like 5-10 times slower).

	if( ((~(x^y))&(x^result)) < 0 ) {
		cpuException(0x30, cpuRegs.branch);		// fixme: is 0x30 right for overflow??
		return true;
	}

	// the not-as-fast style!
	//if( ((x >= 0) && (y >= 0) && (result <  0)) ||
	//	((x <  0) && (y <  0) && (result >= 0)) )
	//	cpuException(0x30, cpuRegs.branch);

	ret = result;
	return false;
}

static __fi bool _add32_Overflow( s32 x, s32 y, s64 &ret )
{
	GPR_reg64 result;  result.SD[0] = (s64)x + y;

	// This 32bit method can rely on the MIPS documented method of checking for
	// overflow, whichs imply compares bit 32 (rightmost bit of the upper word),
	// against bit 31 (leftmost of the lower word).

	// If bit32 != bit31 then we have an overflow.
	if( (result.UL[0]>>31) != (result.UL[1] & 1) ) {
		cpuException(0x30, cpuRegs.branch);
		return true;
	}
	
	ret = result.SD[0];

	return false;
}


const R5900::OPCODE& R5900::GetCurrentInstruction()
{
	const OPCODE* opcode = &R5900::OpcodeTables::tbl_Standard[_Opcode_];

	while( opcode->getsubclass != NULL )
		opcode = &opcode->getsubclass(cpuRegs.code);

	return *opcode;
}

const R5900::OPCODE& R5900::GetInstruction(u32 op)
{
	const OPCODE* opcode = &R5900::OpcodeTables::tbl_Standard[op >> 26];

	while( opcode->getsubclass != NULL )
		opcode = &opcode->getsubclass(op);

	return *opcode;
}

const char * const R5900::bios[256]=
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
	"SetCPUTimerHandler",	"SetCPUTimer",	"SetOsdConfigParam2",	"GetOsdConfigParam2",
//0x70
	"GsGetIMR_iGsGetIMR",				"GsGetIMR_iGsPutIMR",	"SetPgifHandler", 				"SetVSyncFlag",
	"RFU116",							"print", 				"sceSifDmaStat_isceSifDmaStat", "sceSifSetDma_isceSifSetDma",
	"sceSifSetDChain_isceSifSetDChain", "sceSifSetReg",			"sceSifGetReg",					"ExecOSD",
	"Deci2Call",						"PSMode",				"MachineType",					"GetMemorySize",
};

static u32 deci2addr = 0;
static u32 deci2handler = 0;
static char deci2buffer[256];

void Deci2Reset()
{
	deci2handler	= 0;
	deci2addr		= 0;
	memzero( deci2buffer );
}

void SaveStateBase::deci2Freeze()
{
	FreezeTag( "deci2" );

	Freeze( deci2addr );
	Freeze( deci2handler );
	Freeze( deci2buffer );
}

/*
 *	int Deci2Call(int, u_int *);
 *
 *  HLE implementation of the Deci2 interface.
 */

static int __Deci2Call(int call, u32 *addr)
{
	if (call > 0x10)
		return -1;

	switch (call)
	{
		case 1: // open
			if( addr != NULL )
			{
				deci2addr = addr[1];
				BIOS_LOG("deci2open: %x,%x,%x,%x",
						 addr[3], addr[2], addr[1], addr[0]);
				deci2handler = addr[2];
			}
			else
			{
				deci2handler = 0;
				DevCon.Warning( "Deci2Call.Open > NULL address ignored." );
			}
			return 1;

		case 2: // close
			deci2addr = 0;
			deci2handler = 0;
			return 1;

		case 3: // reqsend
		{
			char reqaddr[128];
			if( addr != NULL )
				sprintf( reqaddr, "%x %x %x %x", addr[3], addr[2], addr[1], addr[0] );

			if (!deci2addr) return 1;
			
			const u32* d2ptr = (u32*)PSM(deci2addr);

			BIOS_LOG("deci2reqsend: %s: deci2addr: %x,%x,%x,buf=%x %x,%x,len=%x,%x",
				(( addr == NULL ) ? "NULL" : reqaddr),
				d2ptr[7], d2ptr[6], d2ptr[5], d2ptr[4],
				d2ptr[3], d2ptr[2], d2ptr[1], d2ptr[0]);

//			cpuRegs.pc = deci2handler;
//			Console.WriteLn("deci2msg: %s",  (char*)PSM(d2ptr[4]+0xc));

			if (d2ptr[1]>0xc){
				// this looks horribly wrong, justification please?
				u8* pdeciaddr = (u8*)dmaGetAddr(d2ptr[4]+0xc, false);
				if( pdeciaddr == NULL )
					pdeciaddr = (u8*)PSM(d2ptr[4]+0xc);
				else
					pdeciaddr += (d2ptr[4]+0xc) % 16;

				const int copylen = std::min<uint>(255, d2ptr[1]-0xc);
				memcpy(deci2buffer, pdeciaddr, copylen );
				deci2buffer[copylen] = '\0';

				eeConLog( ShiftJIS_ConvertString(deci2buffer) );
			}
			((u32*)PSM(deci2addr))[3] = 0;
			return 1;
		}

		case 4: // poll
			if( addr != NULL )
				BIOS_LOG("deci2poll: %x,%x,%x,%x\n", addr[3], addr[2], addr[1], addr[0]);
			return 1;

		case 5: // exrecv
			return 1;

		case 6: // exsend
			return 1;

		case 0x10://kputs
			if( addr != NULL )
			{
				eeDeci2Log( ShiftJIS_ConvertString((char*)PSM(*addr)) );
			}
			return 1;
	}

	return 0;
}

namespace R5900 {
namespace Interpreter {
namespace OpcodeImpl {

void COP2()
{
	//std::string disOut;
	//disR5900Fasm(disOut, cpuRegs.code, cpuRegs.pc);

	//VU0_LOG("%s", disOut.c_str());
	Int_COP2PrintTable[_Rs_]();
}

void Unknown() {
	CPU_LOG("%8.8lx: Unknown opcode called", cpuRegs.pc);
}

void MMI_Unknown() { Console.Warning("Unknown MMI opcode called"); }
void COP0_Unknown() { Console.Warning("Unknown COP0 opcode called"); }
void COP1_Unknown() { Console.Warning("Unknown FPU/COP1 opcode called"); }



/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/

// Implementation Notes:
//  * It is important that instructions perform overflow checks prior to shortcutting on
//    the zero register (when it is used as a destination).  Overflow exceptions are still
//    handled even though the result is discarded.

// Rt = Rs + Im signed [exception on overflow]
void ADDI()
{
	s64 result;
	bool overflow = _add32_Overflow( cpuRegs.GPR.r[_Rs_].SD[0], _Imm_, result );
	if (overflow || !_Rt_) return;
	cpuRegs.GPR.r[_Rt_].SD[0] = result;
}

// Rt = Rs + Im signed !!! [overflow ignored]
// This instruction is effectively identical to ADDI.  It is not a true unsigned operation,
// but rather it is a signed operation that ignores overflows.
void ADDIU()
{
	if (!_Rt_) return;
	cpuRegs.GPR.r[_Rt_].UD[0] = u64(s64(s32(cpuRegs.GPR.r[_Rs_].UL[0] + u32(s32(_Imm_)))));
}

// Rt = Rs + Im [exception on overflow]
// This is the full 64 bit version of ADDI.  Overflow occurs at 64 bits instead
// of at 32 bits.
void DADDI()
{
	s64 result;
	bool overflow = _add64_Overflow( cpuRegs.GPR.r[_Rs_].SD[0], _Imm_, result );
	if (overflow || !_Rt_) return;
	cpuRegs.GPR.r[_Rt_].SD[0] = result;
}

// Rt = Rs + Im [overflow ignored]
// This instruction is effectively identical to DADDI.  It is not a true unsigned operation,
// but rather it is a signed operation that ignores overflows.
void DADDIU()
{
	if (!_Rt_) return;
	cpuRegs.GPR.r[_Rt_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0] + u64(s64(_Imm_));
}
void ANDI() 	{ if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0] & (u64)_ImmU_; } // Rt = Rs And Im (zero-extended)
void ORI() 	    { if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0] | (u64)_ImmU_; } // Rt = Rs Or  Im (zero-extended)
void XORI() 	{ if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0] ^ (u64)_ImmU_; } // Rt = Rs Xor Im (zero-extended)
void SLTI()     { if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = (cpuRegs.GPR.r[_Rs_].SD[0] < (s64)(_Imm_)) ? 1 : 0; } // Rt = Rs < Im (signed)
void SLTIU()    { if (!_Rt_) return; cpuRegs.GPR.r[_Rt_].UD[0] = (cpuRegs.GPR.r[_Rs_].UD[0] < (u64)(_Imm_)) ? 1 : 0; } // Rt = Rs < Im (unsigned)

/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/

// Rd = Rs + Rt		(Exception on Integer Overflow)
void ADD()
{
	s64 result;
	bool overflow = _add32_Overflow( cpuRegs.GPR.r[_Rs_].SD[0], cpuRegs.GPR.r[_Rt_].SD[0], result );
	if (overflow || !_Rd_) return;
	cpuRegs.GPR.r[_Rd_].SD[0] = result;
}

void DADD()
{
	s64 result;
	bool overflow = _add64_Overflow( cpuRegs.GPR.r[_Rs_].SD[0], cpuRegs.GPR.r[_Rt_].SD[0], result );
	if (overflow || !_Rd_) return;
	cpuRegs.GPR.r[_Rd_].SD[0] = result;
}

// Rd = Rs - Rt		(Exception on Integer Overflow)
void SUB()
{
	s64 result;
	bool overflow = _add32_Overflow( cpuRegs.GPR.r[_Rs_].SD[0], -cpuRegs.GPR.r[_Rt_].SD[0], result );
	if (overflow || !_Rd_) return;
	cpuRegs.GPR.r[_Rd_].SD[0] = result;
}

// Rd = Rs - Rt		(Exception on Integer Overflow)
void DSUB()
{
	s64 result;
	bool overflow = _add64_Overflow( cpuRegs.GPR.r[_Rs_].SD[0], -cpuRegs.GPR.r[_Rt_].SD[0], result );
	if (overflow || !_Rd_) return;
	cpuRegs.GPR.r[_Rd_].SD[0] = result;
}

void ADDU() 	{ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = u64(s64(s32(cpuRegs.GPR.r[_Rs_].UL[0]  + cpuRegs.GPR.r[_Rt_].UL[0]))); }	// Rd = Rs + Rt
void DADDU()    { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0]  + cpuRegs.GPR.r[_Rt_].UD[0]; }
void SUBU() 	{ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = u64(s64(s32(cpuRegs.GPR.r[_Rs_].UL[0]  - cpuRegs.GPR.r[_Rt_].UL[0]))); }	// Rd = Rs - Rt
void DSUBU() 	{ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0]  - cpuRegs.GPR.r[_Rt_].UD[0]; }
void AND() 	    { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0]  & cpuRegs.GPR.r[_Rt_].UD[0]; }	// Rd = Rs And Rt
void OR() 	    { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0]  | cpuRegs.GPR.r[_Rt_].UD[0]; }	// Rd = Rs Or  Rt
void XOR() 	    { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rs_].UD[0]  ^ cpuRegs.GPR.r[_Rt_].UD[0]; }	// Rd = Rs Xor Rt
void NOR() 	    { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] =~(cpuRegs.GPR.r[_Rs_].UD[0] | cpuRegs.GPR.r[_Rt_].UD[0]); }// Rd = Rs Nor Rt
void SLT()		{ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = (cpuRegs.GPR.r[_Rs_].SD[0] < cpuRegs.GPR.r[_Rt_].SD[0]) ? 1 : 0; }	// Rd = Rs < Rt (signed)
void SLTU()		{ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = (cpuRegs.GPR.r[_Rs_].UD[0] < cpuRegs.GPR.r[_Rt_].UD[0]) ? 1 : 0; }	// Rd = Rs < Rt (unsigned)

/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/

// Signed division "overflows" on (0x80000000 / -1), here (LO = 0x80000000, HI = 0) is returned by MIPS
// in division by zero on MIPS, it appears that:
// LO gets 1 if rs is negative (and the division is signed) and -1 otherwise.
// HI gets the value of rs.

// Result is stored in HI/LO [no arithmetic exceptions]
void DIV()
{
	if (cpuRegs.GPR.r[_Rs_].UL[0] == 0x80000000 && cpuRegs.GPR.r[_Rt_].UL[0] == 0xffffffff)
	{
		cpuRegs.LO.SD[0] = (s32)0x80000000;
		cpuRegs.HI.SD[0] = (s32)0x0;
	}
    else if (cpuRegs.GPR.r[_Rt_].SL[0] != 0)
    {
        cpuRegs.LO.SD[0] = cpuRegs.GPR.r[_Rs_].SL[0] / cpuRegs.GPR.r[_Rt_].SL[0];
        cpuRegs.HI.SD[0] = cpuRegs.GPR.r[_Rs_].SL[0] % cpuRegs.GPR.r[_Rt_].SL[0];
    }
	else
	{
		cpuRegs.LO.SD[0] = (cpuRegs.GPR.r[_Rs_].SL[0] < 0) ? 1 : -1;
		cpuRegs.HI.SD[0] = cpuRegs.GPR.r[_Rs_].SL[0];
	}
}

// Result is stored in HI/LO [no arithmetic exceptions]
void DIVU()
{
	if (cpuRegs.GPR.r[_Rt_].UL[0] != 0)
	{
		// note: DIVU has no sign extension when assigning back to 64 bits
		// note 2: reference material strongly disagrees. (air)
		cpuRegs.LO.SD[0] = (s32)(cpuRegs.GPR.r[_Rs_].UL[0] / cpuRegs.GPR.r[_Rt_].UL[0]);
		cpuRegs.HI.SD[0] = (s32)(cpuRegs.GPR.r[_Rs_].UL[0] % cpuRegs.GPR.r[_Rt_].UL[0]);
	}
	else
	{
		cpuRegs.LO.SD[0] = -1;
		cpuRegs.HI.SD[0] = cpuRegs.GPR.r[_Rs_].SL[0];
	}
}

// Result is written to both HI/LO and to the _Rd_ (Lo only)
void MULT()
{
	s64 res = (s64)cpuRegs.GPR.r[_Rs_].SL[0] * cpuRegs.GPR.r[_Rt_].SL[0];

	// Sign-extend into 64 bits:
	cpuRegs.LO.SD[0] = (s32)(res & 0xffffffff);
	cpuRegs.HI.SD[0] = (s32)(res >> 32);

	if( _Rd_ ) cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.LO.UD[0];
}

// Result is written to both HI/LO and to the _Rd_ (Lo only)
void MULTU()
{
	u64 res = (u64)cpuRegs.GPR.r[_Rs_].UL[0] * cpuRegs.GPR.r[_Rt_].UL[0];

	// Note: sign-extend into 64 bits even though it's an unsigned mult.
	cpuRegs.LO.SD[0] = (s32)(res & 0xffffffff);
	cpuRegs.HI.SD[0] = (s32)(res >> 32);

	if( _Rd_ ) cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.LO.UD[0];
}

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
void LUI() {
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
void SRA()   { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = (s32)(cpuRegs.GPR.r[_Rt_].SL[0] >> _Sa_); } // Rd = Rt >> sa (arithmetic)
void SRL()   { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = (s32)(cpuRegs.GPR.r[_Rt_].UL[0] >> _Sa_); } // Rd = Rt >> sa (logical) [sign extend!!]
void SLL()   { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = (s32)(cpuRegs.GPR.r[_Rt_].UL[0] << _Sa_); } // Rd = Rt << sa
void DSLL()  { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = (u64)(cpuRegs.GPR.r[_Rt_].UD[0] << _Sa_); }
void DSLL32(){ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = (u64)(cpuRegs.GPR.r[_Rt_].UD[0] << (_Sa_+32));}
void DSRA()  { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = cpuRegs.GPR.r[_Rt_].SD[0] >> _Sa_; }
void DSRA32(){ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].SD[0] = cpuRegs.GPR.r[_Rt_].SD[0] >> (_Sa_+32);}
void DSRL()  { if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rt_].UD[0] >> _Sa_; }
void DSRL32(){ if (!_Rd_) return; cpuRegs.GPR.r[_Rd_].UD[0] = cpuRegs.GPR.r[_Rt_].UD[0] >> (_Sa_+32);}

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
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

// Implementation Notes Regarding Memory Operations:
//  * It it 'correct' to do all loads into temp variables, even if the destination GPR
//    is the zero reg (which nullifies the result).  The memory needs to be accessed
//    regardless so that hardware registers behave as expected (some clear on read) and
//    so that TLB Misses are handled as expected as well.
//
//  * Low/High varieties of instructions, such as LWL/LWH, do *not* raise Address Error
//    exceptions, since the lower bits of the address are used to determine the portions
//    of the address/register operations.


void LB()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	s8 temp = memRead8(addr);

	if (!_Rt_) return;
	cpuRegs.GPR.r[_Rt_].SD[0] = temp;
}

void LBU()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u8 temp = memRead8(addr);

	if (!_Rt_) return;
	cpuRegs.GPR.r[_Rt_].UD[0] = temp;
}

void LH()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;

	if( addr & 1 )
		throw R5900Exception::AddressError( addr, false );

	s16 temp = memRead16(addr);

	if (!_Rt_) return;
	cpuRegs.GPR.r[_Rt_].SD[0] = temp;
}

void LHU()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;

	if( addr & 1 )
		throw R5900Exception::AddressError( addr, false );

	u16 temp = memRead16(addr);

	if (!_Rt_) return;
	cpuRegs.GPR.r[_Rt_].UD[0] = temp;
}

void LW()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;

	if( addr & 3 )
		throw R5900Exception::AddressError( addr, false );

	u32 temp = memRead32(addr);

	if (!_Rt_) return;
	cpuRegs.GPR.r[_Rt_].SD[0] = (s32)temp;
}

void LWU()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;

	if( addr & 3 )
		throw R5900Exception::AddressError( addr, false );

	u32 temp = memRead32(addr);

	if (!_Rt_) return;
	cpuRegs.GPR.r[_Rt_].UD[0] = temp;
}

static const u32 LWL_MASK[4] = { 0xffffff, 0x0000ffff, 0x000000ff, 0x00000000 };
static const u32 LWR_MASK[4] = { 0x000000, 0xff000000, 0xffff0000, 0xffffff00 };
static const u8 LWL_SHIFT[4] = { 24, 16, 8, 0 };
static const u8 LWR_SHIFT[4] = { 0, 8, 16, 24 };

void LWL()
{
	s32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 3;

	u32 mem = memRead32(addr & ~3);

	if (!_Rt_) return;

	// ensure the compiler does correct sign extension into 64 bits by using s32
	cpuRegs.GPR.r[_Rt_].SD[0] =	(s32)((cpuRegs.GPR.r[_Rt_].UL[0] & LWL_MASK[shift]) |
								(mem << LWL_SHIFT[shift]));

	/*
	Mem = 1234.  Reg = abcd
	(result is always sign extended into the upper 32 bits of the Rt)

	0   4bcd   (mem << 24) | (reg & 0x00ffffff)
	1   34cd   (mem << 16) | (reg & 0x0000ffff)
	2   234d   (mem <<  8) | (reg & 0x000000ff)
	3   1234   (mem      ) | (reg & 0x00000000)
	*/
}

void LWR()
{
	s32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 3;

	u32 mem = memRead32(addr & ~3);

	if (!_Rt_) return;

	// Use unsigned math here, and conditionally sign extend below, when needed.
	mem = (cpuRegs.GPR.r[_Rt_].UL[0] & LWR_MASK[shift]) | (mem >> LWR_SHIFT[shift]);

	if( shift == 0 )
	{
		// This special case requires sign extension into the full 64 bit dest.
		cpuRegs.GPR.r[_Rt_].SD[0] =	(s32)mem;
	}
	else
	{
		// This case sets the lower 32 bits of the target register.  Upper
		// 32 bits are always preserved.
		cpuRegs.GPR.r[_Rt_].UL[0] =	mem;
	}

	/*
	Mem = 1234.  Reg = abcd

	0   1234   (mem      ) | (reg & 0x00000000)	[sign extend into upper 32 bits!]
	1   a123   (mem >>  8) | (reg & 0xff000000)
	2   ab12   (mem >> 16) | (reg & 0xffff0000)
	3   abc1   (mem >> 24) | (reg & 0xffffff00)
	*/
}

// dummy variable used as a destination address for writes to the zero register, so
// that the zero register always stays zero.
alignas(16) static GPR_reg m_dummy_gpr_zero;

// Returns the x86 address of the requested GPR, which is safe for writing. (includes
// special handling for returning a dummy var for GPR0(zero), so that it's value is
// always preserved)
static GPR_reg* gpr_GetWritePtr( uint gpr )
{
	return (( gpr == 0 ) ? &m_dummy_gpr_zero : &cpuRegs.GPR.r[gpr]);
}

void LD()
{
    s32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;

	if( addr & 7 )
		throw R5900Exception::AddressError( addr, false );

	memRead64(addr, (u64*)gpr_GetWritePtr(_Rt_));
}

static const u64 LDL_MASK[8] =
{	0x00ffffffffffffffULL, 0x0000ffffffffffffULL, 0x000000ffffffffffULL, 0x00000000ffffffffULL,
	0x0000000000ffffffULL, 0x000000000000ffffULL, 0x00000000000000ffULL, 0x0000000000000000ULL
};
static const u64 LDR_MASK[8] =
{	0x0000000000000000ULL, 0xff00000000000000ULL, 0xffff000000000000ULL, 0xffffff0000000000ULL,
	0xffffffff00000000ULL, 0xffffffffff000000ULL, 0xffffffffffff0000ULL, 0xffffffffffffff00ULL
};

static const u8 LDR_SHIFT[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };
static const u8 LDL_SHIFT[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };


void LDL()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 7;

	u64 mem;
	memRead64(addr & ~7, &mem);

	if( !_Rt_ ) return;
	cpuRegs.GPR.r[_Rt_].UD[0] =	(cpuRegs.GPR.r[_Rt_].UD[0] & LDL_MASK[shift]) |
								(mem << LDL_SHIFT[shift]);
}

void LDR()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 7;

	u64 mem;
	memRead64(addr & ~7, &mem);

	if (!_Rt_) return;
	cpuRegs.GPR.r[_Rt_].UD[0] =	(cpuRegs.GPR.r[_Rt_].UD[0] & LDR_MASK[shift]) |
								(mem >> LDR_SHIFT[shift]);
}

void LQ()
{
	// MIPS Note: LQ and SQ are special and "silently" align memory addresses, thus
	// an address error due to unaligned access isn't possible like it is on other loads/stores.

	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	memRead128(addr & ~0xf, (u128*)gpr_GetWritePtr(_Rt_));
}

void SB()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	memWrite8(addr, cpuRegs.GPR.r[_Rt_].UC[0]);
}

void SH()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;

	if( addr & 1 )
		throw R5900Exception::AddressError( addr, true );

	memWrite16(addr, cpuRegs.GPR.r[_Rt_].US[0]);
}

void SW()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;

	if( addr & 3 )
		throw R5900Exception::AddressError( addr, true );

    memWrite32(addr, cpuRegs.GPR.r[_Rt_].UL[0]);
}

static const u32 SWL_MASK[4] = { 0xffffff00, 0xffff0000, 0xff000000, 0x00000000 };
static const u32 SWR_MASK[4] = { 0x00000000, 0x000000ff, 0x0000ffff, 0x00ffffff };

static const u8 SWR_SHIFT[4] = { 0, 8, 16, 24 };
static const u8 SWL_SHIFT[4] = { 24, 16, 8, 0 };

void SWL()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 3;
	u32 mem = memRead32( addr & ~3 );

	memWrite32( addr & ~3,
		(cpuRegs.GPR.r[_Rt_].UL[0] >> SWL_SHIFT[shift]) |
		(mem & SWL_MASK[shift])
	);

	/*
	Mem = 1234.  Reg = abcd

	0   123a   (reg >> 24) | (mem & 0xffffff00)
	1   12ab   (reg >> 16) | (mem & 0xffff0000)
	2   1abc   (reg >>  8) | (mem & 0xff000000)
	3   abcd   (reg      ) | (mem & 0x00000000)
	*/
}

void SWR() {
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 3;
	u32 mem = memRead32(addr & ~3);

	memWrite32( addr & ~3,
		(cpuRegs.GPR.r[_Rt_].UL[0] << SWR_SHIFT[shift]) |
		(mem & SWR_MASK[shift])
	);

	/*
	Mem = 1234.  Reg = abcd

	0   abcd   (reg      ) | (mem & 0x00000000)
	1   bcd4   (reg <<  8) | (mem & 0x000000ff)
	2   cd34   (reg << 16) | (mem & 0x0000ffff)
	3   d234   (reg << 24) | (mem & 0x00ffffff)
	*/
}

void SD()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;

	if( addr & 7 )
		throw R5900Exception::AddressError( addr, true );

    memWrite64(addr,&cpuRegs.GPR.r[_Rt_].UD[0]);
}

static const u64 SDL_MASK[8] =
{	0xffffffffffffff00ULL, 0xffffffffffff0000ULL, 0xffffffffff000000ULL, 0xffffffff00000000ULL,
	0xffffff0000000000ULL, 0xffff000000000000ULL, 0xff00000000000000ULL, 0x0000000000000000ULL
};
static const u64 SDR_MASK[8] =
{	0x0000000000000000ULL, 0x00000000000000ffULL, 0x000000000000ffffULL, 0x0000000000ffffffULL,
	0x00000000ffffffffULL, 0x000000ffffffffffULL, 0x0000ffffffffffffULL, 0x00ffffffffffffffULL
};

static const u8 SDL_SHIFT[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };
static const u8 SDR_SHIFT[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };

void SDL()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 7;
	u64 mem;

	memRead64(addr & ~7, &mem);
	mem = (cpuRegs.GPR.r[_Rt_].UD[0] >> SDL_SHIFT[shift]) |
		  (mem & SDL_MASK[shift]);
	memWrite64(addr & ~7, &mem);
}


void SDR()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	u32 shift = addr & 7;
	u64 mem;

	memRead64(addr & ~7, &mem);
	mem = (cpuRegs.GPR.r[_Rt_].UD[0] << SDR_SHIFT[shift]) |
		  (mem & SDR_MASK[shift]);
	memWrite64(addr & ~7, &mem );
}

void SQ()
{
	// MIPS Note: LQ and SQ are special and "silently" align memory addresses, thus
	// an address error due to unaligned access isn't possible like it is on other loads/stores.

	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	memWrite128(addr & ~0xf, cpuRegs.GPR.r[_Rt_].UQ);
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


// This function is the only one that uses Sifcmd.h in Pcsx2.
#include "Sifcmd.h"

void SYSCALL()
{
	u8 call;

	if (cpuRegs.GPR.n.v1.SL[0] < 0)
		call = (u8)(-cpuRegs.GPR.n.v1.SL[0]);
	else
		call = cpuRegs.GPR.n.v1.UC[0];

	BIOS_LOG("Bios call: %s (%x)", R5900::bios[call], call);


	switch (static_cast<Syscall>(call))
	{
		case Syscall::SetGsCrt:
		{
					//Function "SetGsCrt(Interlace, Mode, Field)"
					//Useful for fetching information of interlace/video/field display parameters of the Graphics Synthesizer

					gsIsInterlaced = cpuRegs.GPR.n.a0.UL[0] & 1;
					bool gsIsFrameMode = cpuRegs.GPR.n.a2.UL[0] & 1;
					const char* inter = (gsIsInterlaced) ? "Interlaced" : "Progressive";
					const char* field = (gsIsFrameMode) ? "FRAME" : "FIELD";
					std::string mode;
					// Warning info might be incorrect!
					switch (cpuRegs.GPR.n.a1.UC[0])
					{
						case 0x0:
						case 0x2:
							mode = "NTSC 640x448 @ 59.940 (59.82)"; gsSetVideoMode(GS_VideoMode::NTSC); break;

						case 0x1:
						case 0x3:
							mode = "PAL  640x512 @ 50.000 (49.76)"; gsSetVideoMode(GS_VideoMode::PAL); break;

						case 0x1A: mode = "VESA 640x480 @ 59.940"; gsSetVideoMode(GS_VideoMode::VESA); break;
						case 0x1B: mode = "VESA 640x480 @ 72.809"; gsSetVideoMode(GS_VideoMode::VESA); break;
						case 0x1C: mode = "VESA 640x480 @ 75.000"; gsSetVideoMode(GS_VideoMode::VESA); break;
						case 0x1D: mode = "VESA 640x480 @ 85.008"; gsSetVideoMode(GS_VideoMode::VESA); break;

						case 0x2A: mode = "VESA 800x600 @ 56.250"; gsSetVideoMode(GS_VideoMode::VESA); break;
						case 0x2B: mode = "VESA 800x600 @ 60.317"; gsSetVideoMode(GS_VideoMode::VESA); break;
						case 0x2C: mode = "VESA 800x600 @ 72.188"; gsSetVideoMode(GS_VideoMode::VESA); break;
						case 0x2D: mode = "VESA 800x600 @ 75.000"; gsSetVideoMode(GS_VideoMode::VESA); break;
						case 0x2E: mode = "VESA 800x600 @ 85.061"; gsSetVideoMode(GS_VideoMode::VESA); break;

						case 0x3B: mode = "VESA 1024x768 @ 60.004"; gsSetVideoMode(GS_VideoMode::VESA); break;
						case 0x3C: mode = "VESA 1024x768 @ 70.069"; gsSetVideoMode(GS_VideoMode::VESA); break;
						case 0x3D: mode = "VESA 1024x768 @ 75.029"; gsSetVideoMode(GS_VideoMode::VESA); break;
						case 0x3E: mode = "VESA 1024x768 @ 84.997"; gsSetVideoMode(GS_VideoMode::VESA); break;

						case 0x4A: mode = "VESA 1280x1024 @ 63.981"; gsSetVideoMode(GS_VideoMode::VESA); break;
						case 0x4B: mode = "VESA 1280x1024 @ 79.976"; gsSetVideoMode(GS_VideoMode::VESA); break;

						case 0x50: mode = "SDTV   720x480 @ 59.94"; gsSetVideoMode(GS_VideoMode::SDTV_480P); break;
						case 0x51: mode = "HDTV 1920x1080 @ 60.00"; gsSetVideoMode(GS_VideoMode::HDTV_1080I); break;
						case 0x52: mode = "HDTV  1280x720 @ ??.???"; gsSetVideoMode(GS_VideoMode::HDTV_720P); break;
						case 0x53: mode = "SDTV   768x576 @ ??.???"; gsSetVideoMode(GS_VideoMode::SDTV_576P); break;
						case 0x54: mode = "HDTV 1920x1080 @ ??.???"; gsSetVideoMode(GS_VideoMode::HDTV_1080P); break;

						case 0x72: mode = "DVD NTSC 640x448 @ ??.???"; gsSetVideoMode(GS_VideoMode::DVD_NTSC); break;
						case 0x73: mode = "DVD PAL 720x480 @ ??.???"; gsSetVideoMode(GS_VideoMode::DVD_PAL); break;

						default:
							DevCon.Error("Mode %x is not supported. Report me upstream", cpuRegs.GPR.n.a1.UC[0]);
							gsSetVideoMode(GS_VideoMode::Unknown);
					}
					DevCon.Warning("Set GS CRTC configuration. %s %s (%s)",mode.c_str(), inter, field);
				}
				break;
		case Syscall::SetOsdConfigParam:
			AllowParams1 = true;
			break;
		case Syscall::GetOsdConfigParam:
			if(!NoOSD && g_SkipBiosHack && !AllowParams1)
			{
				u32 memaddr = cpuRegs.GPR.n.a0.UL[0];
				u8 params[16];
			
				cdvdReadLanguageParams(params);

				u32 osdconf = 0;
				u32 timezone = params[4] | ((u32)(params[3] & 0x7) << 8);

				osdconf |= params[1] & 0x1F;						// SPDIF, Screen mode, RGB/Comp, Jap/Eng Switch (Early bios)
				osdconf |= (u32)params[0] << 5;						// PS1 Mode Settings
				osdconf |= (u32)((params[2] & 0xE0) >> 5) << 13;	// OSD Ver (Not sure but best guess)
				osdconf |= (u32)(params[2] & 0x1F) << 16;			// Language
				osdconf |= timezone << 21;							// Timezone

				memWrite32(memaddr, osdconf);
				return;
			}
			break;
		case Syscall::SetOsdConfigParam2:
			AllowParams2 = true;
			break;
		case Syscall::GetOsdConfigParam2:
			if (!NoOSD && g_SkipBiosHack && !AllowParams2)
			{
				u32 memaddr = cpuRegs.GPR.n.a0.UL[0];
				u8 params[16];

				cdvdReadLanguageParams(params);

				u32 osdconf2 = (u32)((params[3] & 0x78) << 1);  // Daylight Savings, 24hr clock, Date format

				memWrite32(memaddr, osdconf2);
				return;
			}
			break;
		case Syscall::SetVTLBRefillHandler:
			DevCon.Warning("A tlb refill handler is set. New handler %x", (u32*)PSM(cpuRegs.GPR.n.a1.UL[0]));
			break;
		case Syscall::StartThread:
		case Syscall::ChangeThreadPriority:
		{
			if (CurrentBiosInformation.threadListAddr == 0)
			{
				u32 offset = 0x0;
				// Suprisingly not that slow :)
				while (offset < 0x5000) // I find that the instructions are in between 0x4000 -> 0x5000
				{
					u32 addr = 0x80000000 + offset;
					const u32 inst1 = memRead32(addr);
					const u32 inst2 = memRead32(addr += 4);
					const u32 inst3 = memRead32(addr += 4);

					if (ThreadListInstructions[0] == inst1 && // sw v0,0x0(v0)
						ThreadListInstructions[1] == inst2 && // no-op
						ThreadListInstructions[2] == inst3) // no-op
					{
						// We've found the instruction pattern!
						// We (well, I) know that the thread address is always 0x8001 + the immediate of the 6th instruction from here
						const u32 op = memRead32(0x80000000 + offset + (sizeof(u32) * 6));
						CurrentBiosInformation.threadListAddr = 0x80010000 + static_cast<u16>(op) - 8; // Subtract 8 because the address here is offset by 8.
						DevCon.WriteLn("BIOS: Successfully found the instruction pattern. Assuming the thread list is here: %0x", CurrentBiosInformation.threadListAddr);
						break;
					}
					offset += 4;
				}
				if (!CurrentBiosInformation.threadListAddr)
				{
					// We couldn't find the address
					CurrentBiosInformation.threadListAddr = -1;
					// If you're here because a user has reported this message, this means that the instruction pattern is not present on their bios, or it is aligned weirdly.
					Console.Warning("BIOS Warning: Unable to get a thread list offset. The debugger thread and stack frame views will not be functional.");
				}
			}
		}
		break;
		case Syscall::sceSifSetDma:
			// The only thing this code is used for is the one log message, so don't execute it if we aren't logging bios messages.
			if (SysTraceActive(EE.Bios))
			{
				t_sif_dma_transfer *dmat;
				//struct t_sif_cmd_header	*hdr;
				//struct t_sif_rpc_bind *bind;
				//struct t_rpc_server_data *server;
				int n_transfer;
				u32 addr;
				//int sid;

				n_transfer = cpuRegs.GPR.n.a1.UL[0] - 1;
				if (n_transfer >= 0)
				{
					addr = cpuRegs.GPR.n.a0.UL[0] + n_transfer * sizeof(t_sif_dma_transfer);
					dmat = (t_sif_dma_transfer*)PSM(addr);

					BIOS_LOG("bios_%s: n_transfer=%d, size=%x, attr=%x, dest=%x, src=%x",
							R5900::bios[cpuRegs.GPR.n.v1.UC[0]], n_transfer,
							dmat->size, dmat->attr,
							dmat->dest, dmat->src);
				}
			}
			break;

		case Syscall::Deci2Call:
		{
			if (cpuRegs.GPR.n.a0.UL[0] == 0x10)
			{
				eeConLog(ShiftJIS_ConvertString((char*)PSM(memRead32(cpuRegs.GPR.n.a1.UL[0]))));
			}
			else
				__Deci2Call(cpuRegs.GPR.n.a0.UL[0], (u32*)PSM(cpuRegs.GPR.n.a1.UL[0]));

			break;
		}
		case Syscall::sysPrintOut:
		{
			if (cpuRegs.GPR.n.a0.UL[0] != 0)
			{
				// TODO: Only supports 7 format arguments. Need to read from the stack for more.
				// Is there a function which collects PS2 arguments?
				char* fmt = (char*)PSM(cpuRegs.GPR.n.a0.UL[0]);

				u64 regs[7] = {
					cpuRegs.GPR.n.a1.UL[0],
					cpuRegs.GPR.n.a2.UL[0],
					cpuRegs.GPR.n.a3.UL[0],
					cpuRegs.GPR.n.t0.UL[0],
					cpuRegs.GPR.n.t1.UL[0],
					cpuRegs.GPR.n.t2.UL[0],
					cpuRegs.GPR.n.t3.UL[0],
				};

				// Pretty much what this does is find instances of string arguments and remaps them.
				// Instead of the addresse(s) being relative to the PS2 address space, make them relative to program memory.
				// (This fixes issue #2865) 
				int curRegArg = 0;
				for (int i = 0; 1; i++)
				{
					if (fmt[i] == '\0')
						break;

					if (fmt[i] == '%')
					{
						// The extra check here is to be compatible with "%%s"
						if (i == 0 || fmt[i - 1] != '%') {
							if (fmt[i + 1] == 's') {
								regs[curRegArg] = (u64)PSM(regs[curRegArg]); // PS2 Address -> PCSX2 Address
							}
							curRegArg++;
						}
					}
				}

				sysConLog(fmt,
					regs[0],
					regs[1],
					regs[2],
					regs[3],
					regs[4],
					regs[5],
					regs[6]
				);
			}
			break;
		}
		

		default:
			break;
	}

	cpuRegs.pc -= 4;
	cpuException(0x20, cpuRegs.branch);
}

void BREAK() {
	cpuRegs.pc -= 4;
	cpuException(0x24, cpuRegs.branch);
}

void MFSA() {
	if (!_Rd_) return;
	cpuRegs.GPR.r[_Rd_].UD[0] = (u64)cpuRegs.sa;
}

void MTSA() {
	cpuRegs.sa = (u32)cpuRegs.GPR.r[_Rs_].UD[0];
}

// SNY supports three basic modes, two which synchronize memory accesses (related
// to the cache) and one which synchronizes the instruction pipeline (effectively
// a stall in either case).  Our emulation model does not track EE-side pipeline
// status or stalls, nor does it implement the CACHE.  Thus SYNC need do nothing.
void SYNC()
{
}

// Used to prefetch data into the EE's cache, or schedule a dirty write-back.
// CACHE is not emulated at this time (nor is there any need to emulate it), so
// this function does nothing in the context of our emulator.
void PREF()
{
}

static void trap(u16 code=0)
{
	// unimplemented?
	// throw R5900Exception::Trap(code);

	cpuRegs.pc -= 4;
	Console.Warning("Trap exception at 0x%08x", cpuRegs.pc);
	cpuException(0x34, cpuRegs.branch);
}

/*********************************************************
* Register trap                                          *
* Format:  OP rs, rt                                     *
*********************************************************/
void TGE()  { if (cpuRegs.GPR.r[_Rs_].SD[0] >= cpuRegs.GPR.r[_Rt_].SD[0]) trap(_TrapCode_); }
void TGEU() { if (cpuRegs.GPR.r[_Rs_].UD[0] >= cpuRegs.GPR.r[_Rt_].UD[0]) trap(_TrapCode_); }
void TLT()  { if (cpuRegs.GPR.r[_Rs_].SD[0] <  cpuRegs.GPR.r[_Rt_].SD[0]) trap(_TrapCode_); }
void TLTU() { if (cpuRegs.GPR.r[_Rs_].UD[0] <  cpuRegs.GPR.r[_Rt_].UD[0]) trap(_TrapCode_); }
void TEQ()  { if (cpuRegs.GPR.r[_Rs_].SD[0] == cpuRegs.GPR.r[_Rt_].SD[0]) trap(_TrapCode_); }
void TNE()  { if (cpuRegs.GPR.r[_Rs_].SD[0] != cpuRegs.GPR.r[_Rt_].SD[0]) trap(_TrapCode_); }

/*********************************************************
* Trap with immediate operand                            *
* Format:  OP rs, rt                                     *
*********************************************************/
void TGEI()  { if (cpuRegs.GPR.r[_Rs_].SD[0] >= _Imm_) trap(); }
void TLTI()  { if (cpuRegs.GPR.r[_Rs_].SD[0] <  _Imm_) trap(); }
void TEQI()  { if (cpuRegs.GPR.r[_Rs_].SD[0] == _Imm_) trap(); }
void TNEI()  { if (cpuRegs.GPR.r[_Rs_].SD[0] != _Imm_) trap(); }
void TGEIU() { if (cpuRegs.GPR.r[_Rs_].UD[0] >= (u64)_Imm_) trap(); }
void TLTIU() { if (cpuRegs.GPR.r[_Rs_].UD[0] <  (u64)_Imm_) trap(); }

/*********************************************************
* Sa intructions                                         *
* Format:  OP rs, rt                                     *
*********************************************************/

void MTSAB() {
	cpuRegs.sa = ((cpuRegs.GPR.r[_Rs_].UL[0] & 0xF) ^ (_Imm_ & 0xF));
}

void MTSAH() {
	cpuRegs.sa = ((cpuRegs.GPR.r[_Rs_].UL[0] & 0x7) ^ (_Imm_ & 0x7)) << 1;
}

} }	} // end namespace R5900::Interpreter::OpcodeImpl
