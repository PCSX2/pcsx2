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
#include <time.h>

#include "iR3000A.h"
#include "IopMem.h"
#include "IopDma.h"
#include "IopGte.h"

using namespace x86Emitter;

extern int g_psxWriteOk;
extern u32 g_psxMaxRecMem;

// R3000A instruction implementation
#define REC_FUNC(f) \
	static void rpsx##f() \
	{ \
		xMOV(ptr32[&psxRegs.code], (u32)psxRegs.code); \
		_psxFlushCall(FLUSH_EVERYTHING); \
		xFastCall((void*)(uptr)psx##f); \
		PSX_DEL_CONST(_Rt_); \
		/*	branch = 2; */ \
	}

// Same as above but with a different naming convension (to avoid various rename)
#define REC_GTE_FUNC(f) \
	static void rgte##f() \
	{ \
		xMOV(ptr32[&psxRegs.code], (u32)psxRegs.code); \
		_psxFlushCall(FLUSH_EVERYTHING); \
		xFastCall((void*)(uptr)gte##f); \
		PSX_DEL_CONST(_Rt_); \
		/*	branch = 2; */ \
	}

extern void psxLWL();
extern void psxLWR();
extern void psxSWL();
extern void psxSWR();

////
void rpsxADDIU_const()
{
	g_psxConstRegs[_Rt_] = g_psxConstRegs[_Rs_] + _Imm_;
}

// adds a constant to sreg and puts into dreg
void rpsxADDconst(int dreg, int sreg, u32 off, int info)
{
	if (sreg)
	{
		if (sreg == dreg)
		{
			xADD(ptr32[&psxRegs.GPR.r[dreg]], off);
		}
		else
		{
			xMOV(eax, ptr32[&psxRegs.GPR.r[sreg]]);
			if (off)
				xADD(eax, off);
			xMOV(ptr32[&psxRegs.GPR.r[dreg]], eax);
		}
	}
	else
	{
		xMOV(ptr32[&psxRegs.GPR.r[dreg]], off);
	}
}

void rpsxADDIU_(int info)
{
	// Rt = Rs + Im
	if (!_Rt_)
		return;
	rpsxADDconst(_Rt_, _Rs_, _Imm_, info);
}

PSXRECOMPILE_CONSTCODE1(ADDIU);

void rpsxADDI() { rpsxADDIU(); }

//// SLTI
void rpsxSLTI_const()
{
	g_psxConstRegs[_Rt_] = *(int*)&g_psxConstRegs[_Rs_] < _Imm_;
}

void rpsxSLTconst(int info, int dreg, int sreg, int imm)
{
	xXOR(eax, eax);
	xCMP(ptr32[&psxRegs.GPR.r[sreg]], imm);
	xSETL(al);
	xMOV(ptr32[&psxRegs.GPR.r[dreg]], eax);
}

void rpsxSLTI_(int info) { rpsxSLTconst(info, _Rt_, _Rs_, _Imm_); }

PSXRECOMPILE_CONSTCODE1(SLTI);

//// SLTIU
void rpsxSLTIU_const()
{
	g_psxConstRegs[_Rt_] = g_psxConstRegs[_Rs_] < (u32)_Imm_;
}

void rpsxSLTUconst(int info, int dreg, int sreg, int imm)
{
	xXOR(eax, eax);
	xCMP(ptr32[&psxRegs.GPR.r[sreg]], imm);
	xSETB(al);
	xMOV(ptr32[&psxRegs.GPR.r[dreg]], eax);
}

void rpsxSLTIU_(int info) { rpsxSLTUconst(info, _Rt_, _Rs_, (s32)_Imm_); }

PSXRECOMPILE_CONSTCODE1(SLTIU);

//// ANDI
void rpsxANDI_const()
{
	g_psxConstRegs[_Rt_] = g_psxConstRegs[_Rs_] & _ImmU_;
}

void rpsxANDconst(int info, int dreg, int sreg, u32 imm)
{
	if (imm)
	{
		if (sreg == dreg)
		{
			xAND(ptr32[&psxRegs.GPR.r[dreg]], imm);
		}
		else
		{
			xMOV(eax, ptr32[&psxRegs.GPR.r[sreg]]);
			xAND(eax, imm);
			xMOV(ptr32[&psxRegs.GPR.r[dreg]], eax);
		}
	}
	else
	{
		xMOV(ptr32[&psxRegs.GPR.r[dreg]], 0);
	}
}

void rpsxANDI_(int info) { rpsxANDconst(info, _Rt_, _Rs_, _ImmU_); }

PSXRECOMPILE_CONSTCODE1(ANDI);

//// ORI
void rpsxORI_const()
{
	g_psxConstRegs[_Rt_] = g_psxConstRegs[_Rs_] | _ImmU_;
}

void rpsxORconst(int info, int dreg, int sreg, u32 imm)
{
	if (imm)
	{
		if (sreg == dreg)
		{
			xOR(ptr32[&psxRegs.GPR.r[dreg]], imm);
		}
		else
		{
			xMOV(eax, ptr32[&psxRegs.GPR.r[sreg]]);
			xOR(eax, imm);
			xMOV(ptr32[&psxRegs.GPR.r[dreg]], eax);
		}
	}
	else
	{
		if (dreg != sreg)
		{
			xMOV(ecx, ptr32[&psxRegs.GPR.r[sreg]]);
			xMOV(ptr32[&psxRegs.GPR.r[dreg]], ecx);
		}
	}
}

void rpsxORI_(int info) { rpsxORconst(info, _Rt_, _Rs_, _ImmU_); }

PSXRECOMPILE_CONSTCODE1(ORI);

void rpsxXORI_const()
{
	g_psxConstRegs[_Rt_] = g_psxConstRegs[_Rs_] ^ _ImmU_;
}

void rpsxXORconst(int info, int dreg, int sreg, u32 imm)
{
	if (imm == 0xffffffff)
	{
		if (dreg == sreg)
		{
			xNOT(ptr32[&psxRegs.GPR.r[dreg]]);
		}
		else
		{
			xMOV(ecx, ptr32[&psxRegs.GPR.r[sreg]]);
			xNOT(ecx);
			xMOV(ptr32[&psxRegs.GPR.r[dreg]], ecx);
		}
	}
	else if (imm)
	{

		if (sreg == dreg)
		{
			xXOR(ptr32[&psxRegs.GPR.r[dreg]], imm);
		}
		else
		{
			xMOV(eax, ptr32[&psxRegs.GPR.r[sreg]]);
			xXOR(eax, imm);
			xMOV(ptr32[&psxRegs.GPR.r[dreg]], eax);
		}
	}
	else
	{
		if (dreg != sreg)
		{
			xMOV(ecx, ptr32[&psxRegs.GPR.r[sreg]]);
			xMOV(ptr32[&psxRegs.GPR.r[dreg]], ecx);
		}
	}
}

void rpsxXORI_(int info) { rpsxXORconst(info, _Rt_, _Rs_, _ImmU_); }

PSXRECOMPILE_CONSTCODE1(XORI);

void rpsxLUI()
{
	if (!_Rt_)
		return;
	_psxOnWriteReg(_Rt_);
	_psxDeleteReg(_Rt_, 0);
	PSX_SET_CONST(_Rt_);
	g_psxConstRegs[_Rt_] = psxRegs.code << 16;
}

void rpsxADDU_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rs_] + g_psxConstRegs[_Rt_];
}

void rpsxADDU_consts(int info) { rpsxADDconst(_Rd_, _Rt_, g_psxConstRegs[_Rs_], info); }
void rpsxADDU_constt(int info)
{
	info |= PROCESS_EE_SET_S(EEREC_T);
	rpsxADDconst(_Rd_, _Rs_, g_psxConstRegs[_Rt_], info);
}

void rpsxADDU_(int info)
{
	if (_Rs_ && _Rt_)
	{
		xMOV(eax, ptr32[&psxRegs.GPR.r[_Rs_]]);
		xADD(eax, ptr32[&psxRegs.GPR.r[_Rt_]]);
	}
	else if (_Rs_)
	{
		xMOV(eax, ptr32[&psxRegs.GPR.r[_Rs_]]);
	}
	else if (_Rt_)
	{
		xMOV(eax, ptr32[&psxRegs.GPR.r[_Rt_]]);
	}
	else
	{
		xXOR(eax, eax);
	}
	xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
}

PSXRECOMPILE_CONSTCODE0(ADDU);

void rpsxADD() { rpsxADDU(); }


void rpsxSUBU_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rs_] - g_psxConstRegs[_Rt_];
}

void rpsxSUBU_consts(int info)
{
	xMOV(eax, g_psxConstRegs[_Rs_]);
	xSUB(eax, ptr32[&psxRegs.GPR.r[_Rt_]]);
	xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
}

void rpsxSUBU_constt(int info) { rpsxADDconst(_Rd_, _Rs_, -(int)g_psxConstRegs[_Rt_], info); }

void rpsxSUBU_(int info)
{
	// Rd = Rs - Rt
	if (!_Rd_)
		return;

	if (_Rd_ == _Rs_)
	{
		xMOV(eax, ptr32[&psxRegs.GPR.r[_Rt_]]);
		xSUB(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
	}
	else
	{
		xMOV(eax, ptr32[&psxRegs.GPR.r[_Rs_]]);
		xSUB(eax, ptr32[&psxRegs.GPR.r[_Rt_]]);
		xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
	}
}

PSXRECOMPILE_CONSTCODE0(SUBU);

void rpsxSUB() { rpsxSUBU(); }

void rpsxLogicalOp(int info, int op)
{
	if (_Rd_ == _Rs_ || _Rd_ == _Rt_)
	{
		int vreg = _Rd_ == _Rs_ ? _Rt_ : _Rs_;
		xMOV(ecx, ptr32[&psxRegs.GPR.r[vreg]]);

		switch (op) {
			case 0: xAND(ptr32[&psxRegs.GPR.r[_Rd_]], ecx); break;
			case 1: xOR (ptr32[&psxRegs.GPR.r[_Rd_]], ecx); break;
			case 2: xXOR(ptr32[&psxRegs.GPR.r[_Rd_]], ecx); break;
			case 3: xOR (ptr32[&psxRegs.GPR.r[_Rd_]], ecx); break;
			default: pxAssert(0);
		}

		if (op == 3)
			xNOT(ptr32[&psxRegs.GPR.r[_Rd_]]);
	}
	else
	{
		xMOV(ecx, ptr32[&psxRegs.GPR.r[_Rs_]]);

		switch (op) {
			case 0: xAND(ecx, ptr32[&psxRegs.GPR.r[_Rt_]]); break;
			case 1: xOR (ecx, ptr32[&psxRegs.GPR.r[_Rt_]]); break;
			case 2: xXOR(ecx, ptr32[&psxRegs.GPR.r[_Rt_]]); break;
			case 3: xOR (ecx, ptr32[&psxRegs.GPR.r[_Rt_]]); break;
			default: pxAssert(0);
		}

		if (op == 3)
			xNOT(ecx);
		xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], ecx);
	}
}

void rpsxAND_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rs_] & g_psxConstRegs[_Rt_];
}

void rpsxAND_consts(int info) { rpsxANDconst(info, _Rd_, _Rt_, g_psxConstRegs[_Rs_]); }
void rpsxAND_constt(int info) { rpsxANDconst(info, _Rd_, _Rs_, g_psxConstRegs[_Rt_]); }
void rpsxAND_(int info) { rpsxLogicalOp(info, 0); }

PSXRECOMPILE_CONSTCODE0(AND);

void rpsxOR_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rs_] | g_psxConstRegs[_Rt_];
}

void rpsxOR_consts(int info) { rpsxORconst(info, _Rd_, _Rt_, g_psxConstRegs[_Rs_]); }
void rpsxOR_constt(int info) { rpsxORconst(info, _Rd_, _Rs_, g_psxConstRegs[_Rt_]); }
void rpsxOR_(int info) { rpsxLogicalOp(info, 1); }

PSXRECOMPILE_CONSTCODE0(OR);

//// XOR
void rpsxXOR_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rs_] ^ g_psxConstRegs[_Rt_];
}

void rpsxXOR_consts(int info) { rpsxXORconst(info, _Rd_, _Rt_, g_psxConstRegs[_Rs_]); }
void rpsxXOR_constt(int info) { rpsxXORconst(info, _Rd_, _Rs_, g_psxConstRegs[_Rt_]); }
void rpsxXOR_(int info) { rpsxLogicalOp(info, 2); }

PSXRECOMPILE_CONSTCODE0(XOR);

//// NOR
void rpsxNOR_const()
{
	g_psxConstRegs[_Rd_] = ~(g_psxConstRegs[_Rs_] | g_psxConstRegs[_Rt_]);
}

void rpsxNORconst(int info, int dreg, int sreg, u32 imm)
{
	if (imm)
	{
		if (dreg == sreg)
		{
			xOR(ptr32[&psxRegs.GPR.r[dreg]], imm);
			xNOT(ptr32[&psxRegs.GPR.r[dreg]]);
		}
		else
		{
			xMOV(ecx, ptr32[&psxRegs.GPR.r[sreg]]);
			xOR(ecx, imm);
			xNOT(ecx);
			xMOV(ptr32[&psxRegs.GPR.r[dreg]], ecx);
		}
	}
	else
	{
		if (dreg == sreg)
		{
			xNOT(ptr32[&psxRegs.GPR.r[dreg]]);
		}
		else
		{
			xMOV(ecx, ptr32[&psxRegs.GPR.r[sreg]]);
			xNOT(ecx);
			xMOV(ptr32[&psxRegs.GPR.r[dreg]], ecx);
		}
	}
}

void rpsxNOR_consts(int info) { rpsxNORconst(info, _Rd_, _Rt_, g_psxConstRegs[_Rs_]); }
void rpsxNOR_constt(int info) { rpsxNORconst(info, _Rd_, _Rs_, g_psxConstRegs[_Rt_]); }
void rpsxNOR_(int info) { rpsxLogicalOp(info, 3); }

PSXRECOMPILE_CONSTCODE0(NOR);

//// SLT
void rpsxSLT_const()
{
	g_psxConstRegs[_Rd_] = *(int*)&g_psxConstRegs[_Rs_] < *(int*)&g_psxConstRegs[_Rt_];
}

void rpsxSLT_consts(int info)
{
	xXOR(eax, eax);
	xCMP(ptr32[&psxRegs.GPR.r[_Rt_]], g_psxConstRegs[_Rs_]);
	xSETG(al);
	xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
}

void rpsxSLT_constt(int info) { rpsxSLTconst(info, _Rd_, _Rs_, g_psxConstRegs[_Rt_]); }
void rpsxSLT_(int info)
{
	xMOV(eax, ptr32[&psxRegs.GPR.r[_Rs_]]);
	xCMP(eax, ptr32[&psxRegs.GPR.r[_Rt_]]);
	xSETL(al);
	xAND(eax, 0xff);
	xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
}

PSXRECOMPILE_CONSTCODE0(SLT);

//// SLTU
void rpsxSLTU_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rs_] < g_psxConstRegs[_Rt_];
}

void rpsxSLTU_consts(int info)
{
	xXOR(eax, eax);
	xCMP(ptr32[&psxRegs.GPR.r[_Rt_]], g_psxConstRegs[_Rs_]);
	xSETA(al);
	xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
}

void rpsxSLTU_constt(int info) { rpsxSLTUconst(info, _Rd_, _Rs_, g_psxConstRegs[_Rt_]); }
void rpsxSLTU_(int info)
{
	// Rd = Rs < Rt (unsigned)
	if (!_Rd_)
		return;

	xMOV(eax, ptr32[&psxRegs.GPR.r[_Rs_]]);
	xCMP(eax, ptr32[&psxRegs.GPR.r[_Rt_]]);
	xSBB(eax, eax);
	xNEG(eax);
	xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
}

PSXRECOMPILE_CONSTCODE0(SLTU);

//// MULT
void rpsxMULT_const()
{
	u64 res = (s64)((s64)*(int*)&g_psxConstRegs[_Rs_] * (s64)*(int*)&g_psxConstRegs[_Rt_]);

	xMOV(ptr32[&psxRegs.GPR.n.hi], (u32)((res >> 32) & 0xffffffff));
	xMOV(ptr32[&psxRegs.GPR.n.lo], (u32)(res & 0xffffffff));
}

void rpsxMULTsuperconst(int info, int sreg, int imm, int sign)
{
	// Lo/Hi = Rs * Rt (signed)
	xMOV(eax, imm);
	if (sign)
		xMUL(ptr32[&psxRegs.GPR.r[sreg]]);
	else
		xUMUL(ptr32[&psxRegs.GPR.r[sreg]]);
	xMOV(ptr32[&psxRegs.GPR.n.lo], eax);
	xMOV(ptr32[&psxRegs.GPR.n.hi], edx);
}

void rpsxMULTsuper(int info, int sign)
{
	// Lo/Hi = Rs * Rt (signed)
	xMOV(eax, ptr32[&psxRegs.GPR.r[_Rs_]]);
	if (sign)
		xMUL(ptr32[&psxRegs.GPR.r[_Rt_]]);
	else
		xUMUL(ptr32[&psxRegs.GPR.r[_Rt_]]);
	xMOV(ptr32[&psxRegs.GPR.n.lo], eax);
	xMOV(ptr32[&psxRegs.GPR.n.hi], edx);
}

void rpsxMULT_consts(int info) { rpsxMULTsuperconst(info, _Rt_, g_psxConstRegs[_Rs_], 1); }
void rpsxMULT_constt(int info) { rpsxMULTsuperconst(info, _Rs_, g_psxConstRegs[_Rt_], 1); }
void rpsxMULT_(int info) { rpsxMULTsuper(info, 1); }

PSXRECOMPILE_CONSTCODE3_PENALTY(MULT, 1, psxInstCycles_Mult);

//// MULTU
void rpsxMULTU_const()
{
	u64 res = (u64)((u64)g_psxConstRegs[_Rs_] * (u64)g_psxConstRegs[_Rt_]);

	xMOV(ptr32[&psxRegs.GPR.n.hi], (u32)((res >> 32) & 0xffffffff));
	xMOV(ptr32[&psxRegs.GPR.n.lo], (u32)(res & 0xffffffff));
}

void rpsxMULTU_consts(int info) { rpsxMULTsuperconst(info, _Rt_, g_psxConstRegs[_Rs_], 0); }
void rpsxMULTU_constt(int info) { rpsxMULTsuperconst(info, _Rs_, g_psxConstRegs[_Rt_], 0); }
void rpsxMULTU_(int info) { rpsxMULTsuper(info, 0); }

PSXRECOMPILE_CONSTCODE3_PENALTY(MULTU, 1, psxInstCycles_Mult);

//// DIV
void rpsxDIV_const()
{
	u32 lo, hi;

	/*
	 * Normally, when 0x80000000(-2147483648), the signed minimum value, is divided by 0xFFFFFFFF(-1), the
	 * 	operation will result in overflow. However, in this instruction an overflow exception does not occur and the
	 * 	result will be as follows:
	 * 	Quotient: 0x80000000 (-2147483648), and remainder: 0x00000000 (0)
	 */
	// Of course x86 cpu does overflow !
	if (g_psxConstRegs[_Rs_] == 0x80000000u && g_psxConstRegs[_Rt_] == 0xFFFFFFFFu)
	{
		xMOV(ptr32[&psxRegs.GPR.n.hi], 0);
		xMOV(ptr32[&psxRegs.GPR.n.lo], 0x80000000);
		return;
	}

	if (g_psxConstRegs[_Rt_] != 0)
	{
		lo = *(int*)&g_psxConstRegs[_Rs_] / *(int*)&g_psxConstRegs[_Rt_];
		hi = *(int*)&g_psxConstRegs[_Rs_] % *(int*)&g_psxConstRegs[_Rt_];
		xMOV(ptr32[&psxRegs.GPR.n.hi], hi);
		xMOV(ptr32[&psxRegs.GPR.n.lo], lo);
	}
	else
	{
		xMOV(ptr32[&psxRegs.GPR.n.hi], g_psxConstRegs[_Rs_]);
		if (g_psxConstRegs[_Rs_] & 0x80000000u)
		{
			xMOV(ptr32[&psxRegs.GPR.n.lo], 0x1);
		}
		else
		{
			xMOV(ptr32[&psxRegs.GPR.n.lo], 0xFFFFFFFFu);
		}
	}
}

void rpsxDIVsuper(int info, int sign, int process = 0)
{
	// Lo/Hi = Rs / Rt (signed)
	if (process & PROCESS_CONSTT)
		xMOV(ecx, g_psxConstRegs[_Rt_]);
	else
		xMOV(ecx, ptr32[&psxRegs.GPR.r[_Rt_]]);

	if (process & PROCESS_CONSTS)
		xMOV(eax, g_psxConstRegs[_Rs_]);
	else
		xMOV(eax, ptr32[&psxRegs.GPR.r[_Rs_]]);

	u8* end1;
	if (sign) //test for overflow (x86 will just throw an exception)
	{
		xCMP(eax, 0x80000000);
		u8* cont1 = JNE8(0);
		xCMP(ecx, 0xffffffff);
		u8* cont2 = JNE8(0);
		//overflow case:
		xXOR(edx, edx); //EAX remains 0x80000000
		end1 = JMP8(0);

		x86SetJ8(cont1);
		x86SetJ8(cont2);
	}

	xCMP(ecx, 0);
	u8* cont3 = JNE8(0);

	//divide by zero
	xMOV(edx, eax);
	if (sign) //set EAX to (EAX < 0)?1:-1
	{
		xSAR(eax, 31); //(EAX < 0)?-1:0
		xSHL(eax, 1); //(EAX < 0)?-2:0
		xNOT(eax); //(EAX < 0)?1:-1
	}
	else
		xMOV(eax, 0xffffffff);
	u8* end2 = JMP8(0);

	// Normal division
	x86SetJ8(cont3);
	if (sign)
	{
		xCDQ();
		xDIV(ecx);
	}
	else
	{
		xXOR(edx, edx);
		xUDIV(ecx);
	}

	if (sign)
		x86SetJ8(end1);
	x86SetJ8(end2);

	xMOV(ptr32[&psxRegs.GPR.n.lo], eax);
	xMOV(ptr32[&psxRegs.GPR.n.hi], edx);
}

void rpsxDIV_consts(int info) { rpsxDIVsuper(info, 1, PROCESS_CONSTS); }
void rpsxDIV_constt(int info) { rpsxDIVsuper(info, 1, PROCESS_CONSTT); }
void rpsxDIV_(int info) { rpsxDIVsuper(info, 1); }

PSXRECOMPILE_CONSTCODE3_PENALTY(DIV, 1, psxInstCycles_Div);

//// DIVU
void rpsxDIVU_const()
{
	u32 lo, hi;

	if (g_psxConstRegs[_Rt_] != 0)
	{
		lo = g_psxConstRegs[_Rs_] / g_psxConstRegs[_Rt_];
		hi = g_psxConstRegs[_Rs_] % g_psxConstRegs[_Rt_];
		xMOV(ptr32[&psxRegs.GPR.n.hi], hi);
		xMOV(ptr32[&psxRegs.GPR.n.lo], lo);
	}
	else
	{
		xMOV(ptr32[&psxRegs.GPR.n.hi], g_psxConstRegs[_Rs_]);
		xMOV(ptr32[&psxRegs.GPR.n.lo], 0xFFFFFFFFu);
	}
}

void rpsxDIVU_consts(int info) { rpsxDIVsuper(info, 0, PROCESS_CONSTS); }
void rpsxDIVU_constt(int info) { rpsxDIVsuper(info, 0, PROCESS_CONSTT); }
void rpsxDIVU_(int info) { rpsxDIVsuper(info, 0); }

PSXRECOMPILE_CONSTCODE3_PENALTY(DIVU, 1, psxInstCycles_Div);

// TLB loadstore functions
REC_FUNC(LWL);
REC_FUNC(LWR);
REC_FUNC(SWL);
REC_FUNC(SWR);

using namespace x86Emitter;

static void rpsxLB()
{
	_psxDeleteReg(_Rs_, 1);
	_psxOnWriteReg(_Rt_);
	_psxDeleteReg(_Rt_, 0);

	xMOV(ecx, ptr32[&psxRegs.GPR.r[_Rs_]]);
	if (_Imm_)
		xADD(ecx, _Imm_);
	xFastCall((void*)iopMemRead8, ecx); // returns value in EAX
	if (_Rt_)
	{
		xMOVSX(eax, al);
		xMOV(ptr32[&psxRegs.GPR.r[_Rt_]], eax);
	}
	PSX_DEL_CONST(_Rt_);
}

static void rpsxLBU()
{
	_psxDeleteReg(_Rs_, 1);
	_psxOnWriteReg(_Rt_);
	_psxDeleteReg(_Rt_, 0);

	xMOV(ecx, ptr32[&psxRegs.GPR.r[_Rs_]]);
	if (_Imm_)
		xADD(ecx, _Imm_);
	xFastCall((void*)iopMemRead8, ecx); // returns value in EAX
	if (_Rt_)
	{
		xMOVZX(eax, al);
		xMOV(ptr32[&psxRegs.GPR.r[_Rt_]], eax);
	}
	PSX_DEL_CONST(_Rt_);
}

static void rpsxLH()
{
	_psxDeleteReg(_Rs_, 1);
	_psxOnWriteReg(_Rt_);
	_psxDeleteReg(_Rt_, 0);

	xMOV(ecx, ptr32[&psxRegs.GPR.r[_Rs_]]);
	if (_Imm_)
		xADD(ecx, _Imm_);
	xFastCall((void*)iopMemRead16, ecx); // returns value in EAX
	if (_Rt_)
	{
		xMOVSX(eax, ax);
		xMOV(ptr32[&psxRegs.GPR.r[_Rt_]], eax);
	}
	PSX_DEL_CONST(_Rt_);
}

static void rpsxLHU()
{
	_psxDeleteReg(_Rs_, 1);
	_psxOnWriteReg(_Rt_);
	_psxDeleteReg(_Rt_, 0);

	xMOV(ecx, ptr32[&psxRegs.GPR.r[_Rs_]]);
	if (_Imm_)
		xADD(ecx, _Imm_);
	xFastCall((void*)iopMemRead16, ecx); // returns value in EAX
	if (_Rt_)
	{
		xMOVZX(eax, ax);
		xMOV(ptr32[&psxRegs.GPR.r[_Rt_]], eax);
	}
	PSX_DEL_CONST(_Rt_);
}

static void rpsxLW()
{
	_psxDeleteReg(_Rs_, 1);
	_psxOnWriteReg(_Rt_);
	_psxDeleteReg(_Rt_, 0);

	_psxFlushCall(FLUSH_EVERYTHING);
	xMOV(ecx, ptr32[&psxRegs.GPR.r[_Rs_]]);
	if (_Imm_)
		xADD(ecx, _Imm_);

	xTEST(ecx, 0x10000000);
	j8Ptr[0] = JZ8(0);

	xFastCall((void*)iopMemRead32, ecx); // returns value in EAX
	if (_Rt_)
	{
		xMOV(ptr32[&psxRegs.GPR.r[_Rt_]], eax);
	}
	j8Ptr[1] = JMP8(0);
	x86SetJ8(j8Ptr[0]);

	// read from psM directly
	xAND(ecx, 0x1fffff);

	xMOV(ecx, ptr32[xComplexAddress(rax, iopMem->Main, rcx)]);
	if (_Rt_)
	{
		xMOV(ptr32[&psxRegs.GPR.r[_Rt_]], ecx);
	}

	x86SetJ8(j8Ptr[1]);
	PSX_DEL_CONST(_Rt_);
}

static void rpsxSB()
{
	_psxDeleteReg(_Rs_, 1);
	_psxDeleteReg(_Rt_, 1);

	xMOV(arg1regd, ptr32[&psxRegs.GPR.r[_Rs_]]);
	if (_Imm_)
		xADD(arg1regd, _Imm_);
	xMOV(arg2regd, ptr32[&psxRegs.GPR.r[_Rt_]]);
	xFastCall((void*)iopMemWrite8, arg1regd, arg2regd);
}

static void rpsxSH()
{
	_psxDeleteReg(_Rs_, 1);
	_psxDeleteReg(_Rt_, 1);

	xMOV(arg1regd, ptr32[&psxRegs.GPR.r[_Rs_]]);
	if (_Imm_)
		xADD(arg1regd, _Imm_);
	xMOV(arg2regd, ptr32[&psxRegs.GPR.r[_Rt_]]);
	xFastCall((void*)iopMemWrite16, arg1regd, arg2regd);
}

static void rpsxSW()
{
	_psxDeleteReg(_Rs_, 1);
	_psxDeleteReg(_Rt_, 1);

	xMOV(arg1regd, ptr32[&psxRegs.GPR.r[_Rs_]]);
	if (_Imm_)
		xADD(arg1regd, _Imm_);
	xMOV(arg2regd, ptr32[&psxRegs.GPR.r[_Rt_]]);
	xFastCall((void*)iopMemWrite32, arg1regd, arg2regd);
}

//// SLL
void rpsxSLL_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rt_] << _Sa_;
}

// shifttype: 0 - sll, 1 - srl, 2 - sra
void rpsxShiftConst(int info, int rdreg, int rtreg, int imm, int shifttype)
{
	imm &= 0x1f;
	if (imm)
	{
		if (rdreg == rtreg)
		{
			switch (shifttype)
			{
				case 0: xSHL(ptr32[&psxRegs.GPR.r[rdreg]], imm); break;
				case 1: xSHR(ptr32[&psxRegs.GPR.r[rdreg]], imm); break;
				case 2: xSAR(ptr32[&psxRegs.GPR.r[rdreg]], imm); break;
			}
		}
		else
		{
			xMOV(eax, ptr32[&psxRegs.GPR.r[rtreg]]);
			switch (shifttype)
			{
				case 0: xSHL(eax, imm); break;
				case 1: xSHR(eax, imm); break;
				case 2: xSAR(eax, imm); break;
			}
			xMOV(ptr32[&psxRegs.GPR.r[rdreg]], eax);
		}
	}
	else
	{
		if (rdreg != rtreg)
		{
			xMOV(eax, ptr32[&psxRegs.GPR.r[rtreg]]);
			xMOV(ptr32[&psxRegs.GPR.r[rdreg]], eax);
		}
	}
}

void rpsxSLL_(int info) { rpsxShiftConst(info, _Rd_, _Rt_, _Sa_, 0); }
PSXRECOMPILE_CONSTCODE2(SLL);

//// SRL
void rpsxSRL_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rt_] >> _Sa_;
}

void rpsxSRL_(int info) { rpsxShiftConst(info, _Rd_, _Rt_, _Sa_, 1); }
PSXRECOMPILE_CONSTCODE2(SRL);

//// SRA
void rpsxSRA_const()
{
	g_psxConstRegs[_Rd_] = *(int*)&g_psxConstRegs[_Rt_] >> _Sa_;
}

void rpsxSRA_(int info) { rpsxShiftConst(info, _Rd_, _Rt_, _Sa_, 2); }
PSXRECOMPILE_CONSTCODE2(SRA);

//// SLLV
void rpsxSLLV_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rt_] << (g_psxConstRegs[_Rs_] & 0x1f);
}

void rpsxShiftVconsts(int info, int shifttype)
{
	rpsxShiftConst(info, _Rd_, _Rt_, g_psxConstRegs[_Rs_], shifttype);
}

void rpsxShiftVconstt(int info, int shifttype)
{
	xMOV(eax, g_psxConstRegs[_Rt_]);
	xMOV(ecx, ptr32[&psxRegs.GPR.r[_Rs_]]);
	switch (shifttype)
	{
		case 0: xSHL(eax, cl); break;
		case 1: xSHR(eax, cl); break;
		case 2: xSAR(eax, cl); break;
	}
	xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
}

void rpsxSLLV_consts(int info) { rpsxShiftVconsts(info, 0); }
void rpsxSLLV_constt(int info) { rpsxShiftVconstt(info, 0); }
void rpsxSLLV_(int info)
{
	xMOV(eax, ptr32[&psxRegs.GPR.r[_Rt_]]);
	xMOV(ecx, ptr32[&psxRegs.GPR.r[_Rs_]]);
	xSHL(eax, cl);
	xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
}

PSXRECOMPILE_CONSTCODE0(SLLV);

//// SRLV
void rpsxSRLV_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rt_] >> (g_psxConstRegs[_Rs_] & 0x1f);
}

void rpsxSRLV_consts(int info) { rpsxShiftVconsts(info, 1); }
void rpsxSRLV_constt(int info) { rpsxShiftVconstt(info, 1); }
void rpsxSRLV_(int info)
{
	xMOV(eax, ptr32[&psxRegs.GPR.r[_Rt_]]);
	xMOV(ecx, ptr32[&psxRegs.GPR.r[_Rs_]]);
	xSHR(eax, cl);
	xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
}

PSXRECOMPILE_CONSTCODE0(SRLV);

//// SRAV
void rpsxSRAV_const()
{
	g_psxConstRegs[_Rd_] = *(int*)&g_psxConstRegs[_Rt_] >> (g_psxConstRegs[_Rs_] & 0x1f);
}

void rpsxSRAV_consts(int info) { rpsxShiftVconsts(info, 2); }
void rpsxSRAV_constt(int info) { rpsxShiftVconstt(info, 2); }
void rpsxSRAV_(int info)
{
	xMOV(eax, ptr32[&psxRegs.GPR.r[_Rt_]]);
	xMOV(ecx, ptr32[&psxRegs.GPR.r[_Rs_]]);
	xSAR(eax, cl);
	xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
}

PSXRECOMPILE_CONSTCODE0(SRAV);

extern void rpsxSYSCALL();
extern void rpsxBREAK();

void rpsxMFHI()
{
	if (!_Rd_)
		return;

	_psxOnWriteReg(_Rd_);
	_psxDeleteReg(_Rd_, 0);
	xMOV(eax, ptr32[&psxRegs.GPR.n.hi]);
	xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
}

void rpsxMTHI()
{
	if (PSX_IS_CONST1(_Rs_))
	{
		xMOV(ptr32[&psxRegs.GPR.n.hi], g_psxConstRegs[_Rs_]);
	}
	else
	{
		_psxDeleteReg(_Rs_, 1);
		xMOV(eax, ptr32[&psxRegs.GPR.r[_Rs_]]);
		xMOV(ptr32[&psxRegs.GPR.n.hi], eax);
	}
}

void rpsxMFLO()
{
	if (!_Rd_)
		return;

	_psxOnWriteReg(_Rd_);
	_psxDeleteReg(_Rd_, 0);
	xMOV(eax, ptr32[&psxRegs.GPR.n.lo]);
	xMOV(ptr32[&psxRegs.GPR.r[_Rd_]], eax);
}

void rpsxMTLO()
{
	if (PSX_IS_CONST1(_Rs_))
	{
		xMOV(ptr32[&psxRegs.GPR.n.lo], g_psxConstRegs[_Rs_]);
	}
	else
	{
		_psxDeleteReg(_Rs_, 1);
		xMOV(eax, ptr32[&psxRegs.GPR.r[_Rs_]]);
		xMOV(ptr32[&psxRegs.GPR.n.lo], eax);
	}
}

void rpsxJ()
{
	// j target
	u32 newpc = _InstrucTarget_ * 4 + (psxpc & 0xf0000000);
	psxRecompileNextInstruction(1);
	psxSetBranchImm(newpc);
}

void rpsxJAL()
{
	u32 newpc = (_InstrucTarget_ << 2) + (psxpc & 0xf0000000);
	_psxDeleteReg(31, 0);
	PSX_SET_CONST(31);
	g_psxConstRegs[31] = psxpc + 4;

	psxRecompileNextInstruction(1);
	psxSetBranchImm(newpc);
}

void rpsxJR()
{
	psxSetBranchReg(_Rs_);
}

void rpsxJALR()
{
	// jalr Rs
	_allocX86reg(calleeSavedReg2d, X86TYPE_PCWRITEBACK, 0, MODE_WRITE);
	_psxMoveGPRtoR(calleeSavedReg2d, _Rs_);

	if (_Rd_)
	{
		_psxDeleteReg(_Rd_, 0);
		PSX_SET_CONST(_Rd_);
		g_psxConstRegs[_Rd_] = psxpc + 4;
	}

	psxRecompileNextInstruction(1);

	if (x86regs[calleeSavedReg2d.GetId()].inuse)
	{
		pxAssert(x86regs[calleeSavedReg2d.GetId()].type == X86TYPE_PCWRITEBACK);
		xMOV(ptr32[&psxRegs.pc], calleeSavedReg2d);
		x86regs[calleeSavedReg2d.GetId()].inuse = 0;
#ifdef PCSX2_DEBUG
		xOR(calleeSavedReg2d, calleeSavedReg2d);
#endif
	}
	else
	{
		xMOV(eax, ptr32[&g_recWriteback]);
		xMOV(ptr32[&psxRegs.pc], eax);
#ifdef PCSX2_DEBUG
		xOR(eax, eax);
#endif
	}
#ifdef PCSX2_DEBUG
	xForwardJNZ8 skipAssert;
	xWrite8(0xcc);
	skipAssert.SetTarget();
#endif

	psxSetBranchReg(0xffffffff);
}

//// BEQ
static u32* s_pbranchjmp;

void rpsxSetBranchEQ(int info, int process)
{
	if (process & PROCESS_CONSTS)
	{
		xCMP(ptr32[&psxRegs.GPR.r[_Rt_]], g_psxConstRegs[_Rs_]);
		s_pbranchjmp = JNE32(0);
	}
	else if (process & PROCESS_CONSTT)
	{
		xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], g_psxConstRegs[_Rt_]);
		s_pbranchjmp = JNE32(0);
	}
	else
	{
		xMOV(eax, ptr32[&psxRegs.GPR.r[_Rs_]]);
		xCMP(eax, ptr32[&psxRegs.GPR.r[_Rt_]]);
		s_pbranchjmp = JNE32(0);
	}
}

void rpsxBEQ_const()
{
	u32 branchTo;

	if (g_psxConstRegs[_Rs_] == g_psxConstRegs[_Rt_])
		branchTo = ((s32)_Imm_ * 4) + psxpc;
	else
		branchTo = psxpc + 4;

	psxRecompileNextInstruction(1);
	psxSetBranchImm(branchTo);
}

void rpsxBEQ_process(int info, int process)
{
	u32 branchTo = ((s32)_Imm_ * 4) + psxpc;

	if (_Rs_ == _Rt_)
	{
		psxRecompileNextInstruction(1);
		psxSetBranchImm(branchTo);
	}
	else
	{
		_psxFlushAllUnused();
		psxSaveBranchState();

		rpsxSetBranchEQ(info, process);

		psxRecompileNextInstruction(1);
		psxSetBranchImm(branchTo);

		x86SetJ32A(s_pbranchjmp);

		// recopy the next inst
		psxpc -= 4;
		psxLoadBranchState();
		psxRecompileNextInstruction(1);

		psxSetBranchImm(psxpc);
	}
}

void rpsxBEQ_(int info) { rpsxBEQ_process(info, 0); }
void rpsxBEQ_consts(int info) { rpsxBEQ_process(info, PROCESS_CONSTS); }
void rpsxBEQ_constt(int info) { rpsxBEQ_process(info, PROCESS_CONSTT); }
PSXRECOMPILE_CONSTCODE3(BEQ, 0);

//// BNE
void rpsxBNE_const()
{
	u32 branchTo;

	if (g_psxConstRegs[_Rs_] != g_psxConstRegs[_Rt_])
		branchTo = ((s32)_Imm_ * 4) + psxpc;
	else
		branchTo = psxpc + 4;

	psxRecompileNextInstruction(1);
	psxSetBranchImm(branchTo);
}

void rpsxBNE_process(int info, int process)
{
	u32 branchTo = ((s32)_Imm_ * 4) + psxpc;

	if (_Rs_ == _Rt_)
	{
		psxRecompileNextInstruction(1);
		psxSetBranchImm(psxpc);
		return;
	}

	_psxFlushAllUnused();
	rpsxSetBranchEQ(info, process);

	psxSaveBranchState();
	psxRecompileNextInstruction(1);
	psxSetBranchImm(psxpc);

	x86SetJ32A(s_pbranchjmp);

	// recopy the next inst
	psxpc -= 4;
	psxLoadBranchState();
	psxRecompileNextInstruction(1);

	psxSetBranchImm(branchTo);
}

void rpsxBNE_(int info) { rpsxBNE_process(info, 0); }
void rpsxBNE_consts(int info) { rpsxBNE_process(info, PROCESS_CONSTS); }
void rpsxBNE_constt(int info) { rpsxBNE_process(info, PROCESS_CONSTT); }
PSXRECOMPILE_CONSTCODE3(BNE, 0);

//// BLTZ
void rpsxBLTZ()
{
	// Branch if Rs < 0
	u32 branchTo = (s32)_Imm_ * 4 + psxpc;

	_psxFlushAllUnused();

	if (PSX_IS_CONST1(_Rs_))
	{
		if ((int)g_psxConstRegs[_Rs_] >= 0)
			branchTo = psxpc + 4;

		psxRecompileNextInstruction(1);
		psxSetBranchImm(branchTo);
		return;
	}

	xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], 0);
	u32* pjmp = JL32(0);

	psxSaveBranchState();
	psxRecompileNextInstruction(1);

	psxSetBranchImm(psxpc);

	x86SetJ32A(pjmp);

	// recopy the next inst
	psxpc -= 4;
	psxLoadBranchState();
	psxRecompileNextInstruction(1);

	psxSetBranchImm(branchTo);
}

//// BGEZ
void rpsxBGEZ()
{
	u32 branchTo = ((s32)_Imm_ * 4) + psxpc;

	_psxFlushAllUnused();

	if (PSX_IS_CONST1(_Rs_))
	{
		if ((int)g_psxConstRegs[_Rs_] < 0)
			branchTo = psxpc + 4;

		psxRecompileNextInstruction(1);
		psxSetBranchImm(branchTo);
		return;
	}

	xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], 0);
	u32* pjmp = JGE32(0);

	psxSaveBranchState();
	psxRecompileNextInstruction(1);

	psxSetBranchImm(psxpc);

	x86SetJ32A(pjmp);

	// recopy the next inst
	psxpc -= 4;
	psxLoadBranchState();
	psxRecompileNextInstruction(1);

	psxSetBranchImm(branchTo);
}

//// BLTZAL
void rpsxBLTZAL()
{
	// Branch if Rs < 0
	u32 branchTo = (s32)_Imm_ * 4 + psxpc;

	_psxFlushConstReg(31);
	_psxDeleteReg(31, 0);
	_psxFlushAllUnused();

	PSX_SET_CONST(31);
	g_psxConstRegs[31] = psxpc + 4;

	if (PSX_IS_CONST1(_Rs_))
	{
		if ((int)g_psxConstRegs[_Rs_] >= 0)
			branchTo = psxpc + 4;

		psxRecompileNextInstruction(1);
		psxSetBranchImm(branchTo);
		return;
	}

	xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], 0);
	u32* pjmp = JL32(0);

	psxSaveBranchState();

	psxRecompileNextInstruction(1);

	psxSetBranchImm(psxpc);

	x86SetJ32A(pjmp);

	// recopy the next inst
	psxpc -= 4;
	psxLoadBranchState();
	psxRecompileNextInstruction(1);

	psxSetBranchImm(branchTo);
}

//// BGEZAL
void rpsxBGEZAL()
{
	u32 branchTo = ((s32)_Imm_ * 4) + psxpc;

	_psxFlushConstReg(31);
	_psxDeleteReg(31, 0);
	_psxFlushAllUnused();

	PSX_SET_CONST(31);
	g_psxConstRegs[31] = psxpc + 4;

	if (PSX_IS_CONST1(_Rs_))
	{
		if ((int)g_psxConstRegs[_Rs_] < 0)
			branchTo = psxpc + 4;

		psxRecompileNextInstruction(1);
		psxSetBranchImm(branchTo);
		return;
	}

	xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], 0);
	u32* pjmp = JGE32(0);

	psxSaveBranchState();
	psxRecompileNextInstruction(1);

	psxSetBranchImm(psxpc);

	x86SetJ32A(pjmp);

	// recopy the next inst
	psxpc -= 4;
	psxLoadBranchState();
	psxRecompileNextInstruction(1);

	psxSetBranchImm(branchTo);
}

//// BLEZ
void rpsxBLEZ()
{
	// Branch if Rs <= 0
	u32 branchTo = (s32)_Imm_ * 4 + psxpc;

	_psxFlushAllUnused();

	if (PSX_IS_CONST1(_Rs_))
	{
		if ((int)g_psxConstRegs[_Rs_] > 0)
			branchTo = psxpc + 4;

		psxRecompileNextInstruction(1);
		psxSetBranchImm(branchTo);
		return;
	}

	_psxDeleteReg(_Rs_, 1);
	_clearNeededX86regs();

	xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], 0);
	u32* pjmp = JLE32(0);

	psxSaveBranchState();
	psxRecompileNextInstruction(1);
	psxSetBranchImm(psxpc);

	x86SetJ32A(pjmp);

	psxpc -= 4;
	psxLoadBranchState();
	psxRecompileNextInstruction(1);
	psxSetBranchImm(branchTo);
}

//// BGTZ
void rpsxBGTZ()
{
	// Branch if Rs > 0
	u32 branchTo = (s32)_Imm_ * 4 + psxpc;

	_psxFlushAllUnused();

	if (PSX_IS_CONST1(_Rs_))
	{
		if ((int)g_psxConstRegs[_Rs_] <= 0)
			branchTo = psxpc + 4;

		psxRecompileNextInstruction(1);
		psxSetBranchImm(branchTo);
		return;
	}

	_psxDeleteReg(_Rs_, 1);
	_clearNeededX86regs();

	xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], 0);
	u32* pjmp = JG32(0);

	psxSaveBranchState();
	psxRecompileNextInstruction(1);
	psxSetBranchImm(psxpc);

	x86SetJ32A(pjmp);

	psxpc -= 4;
	psxLoadBranchState();
	psxRecompileNextInstruction(1);
	psxSetBranchImm(branchTo);
}

void rpsxMFC0()
{
	// Rt = Cop0->Rd
	if (!_Rt_)
		return;

	_psxOnWriteReg(_Rt_);
	xMOV(eax, ptr32[&psxRegs.CP0.r[_Rd_]]);
	xMOV(ptr32[&psxRegs.GPR.r[_Rt_]], eax);
}

void rpsxCFC0()
{
	// Rt = Cop0->Rd
	if (!_Rt_)
		return;

	_psxOnWriteReg(_Rt_);
	xMOV(eax, ptr32[&psxRegs.CP0.r[_Rd_]]);
	xMOV(ptr32[&psxRegs.GPR.r[_Rt_]], eax);
}

void rpsxMTC0()
{
	// Cop0->Rd = Rt
	if (PSX_IS_CONST1(_Rt_))
	{
		xMOV(ptr32[&psxRegs.CP0.r[_Rd_]], g_psxConstRegs[_Rt_]);
	}
	else
	{
		_psxDeleteReg(_Rt_, 1);
		xMOV(eax, ptr32[&psxRegs.GPR.r[_Rt_]]);
		xMOV(ptr32[&psxRegs.CP0.r[_Rd_]], eax);
	}
}

void rpsxCTC0()
{
	// Cop0->Rd = Rt
	rpsxMTC0();
}

void rpsxRFE()
{
	xMOV(eax, ptr32[&psxRegs.CP0.n.Status]);
	xMOV(ecx, eax);
	xAND(eax, 0xfffffff0);
	xAND(ecx, 0x3c);
	xSHR(ecx, 2);
	xOR(eax, ecx);
	xMOV(ptr32[&psxRegs.CP0.n.Status], eax);

	// Test the IOP's INTC status, so that any pending ints get raised.

	_psxFlushCall(0);
	xFastCall((void*)(uptr)&iopTestIntc);
}

//// COP2
REC_GTE_FUNC(RTPS);
REC_GTE_FUNC(NCLIP);
REC_GTE_FUNC(OP);
REC_GTE_FUNC(DPCS);
REC_GTE_FUNC(INTPL);
REC_GTE_FUNC(MVMVA);
REC_GTE_FUNC(NCDS);
REC_GTE_FUNC(CDP);
REC_GTE_FUNC(NCDT);
REC_GTE_FUNC(NCCS);
REC_GTE_FUNC(CC);
REC_GTE_FUNC(NCS);
REC_GTE_FUNC(NCT);
REC_GTE_FUNC(SQR);
REC_GTE_FUNC(DCPL);
REC_GTE_FUNC(DPCT);
REC_GTE_FUNC(AVSZ3);
REC_GTE_FUNC(AVSZ4);
REC_GTE_FUNC(RTPT);
REC_GTE_FUNC(GPF);
REC_GTE_FUNC(GPL);
REC_GTE_FUNC(NCCT);

REC_GTE_FUNC(MFC2);
REC_GTE_FUNC(CFC2);
REC_GTE_FUNC(MTC2);
REC_GTE_FUNC(CTC2);

REC_GTE_FUNC(LWC2);
REC_GTE_FUNC(SWC2);


// R3000A tables
extern void (*rpsxBSC[64])();
extern void (*rpsxSPC[64])();
extern void (*rpsxREG[32])();
extern void (*rpsxCP0[32])();
extern void (*rpsxCP2[64])();
extern void (*rpsxCP2BSC[32])();

static void rpsxSPECIAL() { rpsxSPC[_Funct_](); }
static void rpsxREGIMM() { rpsxREG[_Rt_](); }
static void rpsxCOP0() { rpsxCP0[_Rs_](); }
static void rpsxCOP2() { rpsxCP2[_Funct_](); }
static void rpsxBASIC() { rpsxCP2BSC[_Rs_](); }

static void rpsxNULL()
{
	Console.WriteLn("psxUNK: %8.8x", psxRegs.code);
}

void (*rpsxBSC[64])() = {
	rpsxSPECIAL, rpsxREGIMM, rpsxJ   , rpsxJAL  , rpsxBEQ , rpsxBNE , rpsxBLEZ, rpsxBGTZ,
	rpsxADDI   , rpsxADDIU , rpsxSLTI, rpsxSLTIU, rpsxANDI, rpsxORI , rpsxXORI, rpsxLUI ,
	rpsxCOP0   , rpsxNULL  , rpsxCOP2, rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL   , rpsxNULL  , rpsxNULL, rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxLB     , rpsxLH    , rpsxLWL , rpsxLW   , rpsxLBU , rpsxLHU , rpsxLWR , rpsxNULL,
	rpsxSB     , rpsxSH    , rpsxSWL , rpsxSW   , rpsxNULL, rpsxNULL, rpsxSWR , rpsxNULL,
	rpsxNULL   , rpsxNULL  , rgteLWC2, rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL   , rpsxNULL  , rgteSWC2, rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
};

void (*rpsxSPC[64])() = {
	rpsxSLL , rpsxNULL, rpsxSRL , rpsxSRA , rpsxSLLV   , rpsxNULL , rpsxSRLV, rpsxSRAV,
	rpsxJR  , rpsxJALR, rpsxNULL, rpsxNULL, rpsxSYSCALL, rpsxBREAK, rpsxNULL, rpsxNULL,
	rpsxMFHI, rpsxMTHI, rpsxMFLO, rpsxMTLO, rpsxNULL   , rpsxNULL , rpsxNULL, rpsxNULL,
	rpsxMULT, rpsxMULTU, rpsxDIV, rpsxDIVU, rpsxNULL   , rpsxNULL , rpsxNULL, rpsxNULL,
	rpsxADD , rpsxADDU, rpsxSUB , rpsxSUBU, rpsxAND    , rpsxOR   , rpsxXOR , rpsxNOR ,
	rpsxNULL, rpsxNULL, rpsxSLT , rpsxSLTU, rpsxNULL   , rpsxNULL , rpsxNULL, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL   , rpsxNULL , rpsxNULL, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL   , rpsxNULL , rpsxNULL, rpsxNULL,
};

void (*rpsxREG[32])() = {
	rpsxBLTZ  , rpsxBGEZ  , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL  , rpsxNULL  , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxBLTZAL, rpsxBGEZAL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL  , rpsxNULL  , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
};

void (*rpsxCP0[32])() = {
	rpsxMFC0, rpsxNULL, rpsxCFC0, rpsxNULL, rpsxMTC0, rpsxNULL, rpsxCTC0, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxRFE , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
};

void (*rpsxCP2[64])() = {
	rpsxBASIC, rgteRTPS , rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL , rgteNCLIP, rpsxNULL, // 00
	rpsxNULL , rpsxNULL , rpsxNULL , rpsxNULL, rgteOP  , rpsxNULL , rpsxNULL , rpsxNULL, // 08
	rgteDPCS , rgteINTPL, rgteMVMVA, rgteNCDS, rgteCDP , rpsxNULL , rgteNCDT , rpsxNULL, // 10
	rpsxNULL , rpsxNULL , rpsxNULL , rgteNCCS, rgteCC  , rpsxNULL , rgteNCS  , rpsxNULL, // 18
	rgteNCT  , rpsxNULL , rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL , rpsxNULL , rpsxNULL, // 20
	rgteSQR  , rgteDCPL , rgteDPCT , rpsxNULL, rpsxNULL, rgteAVSZ3, rgteAVSZ4, rpsxNULL, // 28
	rgteRTPT , rpsxNULL , rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL , rpsxNULL , rpsxNULL, // 30
	rpsxNULL , rpsxNULL , rpsxNULL , rpsxNULL, rpsxNULL, rgteGPF  , rgteGPL  , rgteNCCT, // 38
};

void (*rpsxCP2BSC[32])() = {
	rgteMFC2, rpsxNULL, rgteCFC2, rpsxNULL, rgteMTC2, rpsxNULL, rgteCTC2, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
};

////////////////////////////////////////////////
// Back-Prob Function Tables - Gathering Info //
////////////////////////////////////////////////
#define rpsxpropSetRead(reg) \
	{ \
		if (!(pinst->regs[reg] & EEINST_USED)) \
			pinst->regs[reg] |= EEINST_LASTUSE; \
		prev->regs[reg] |= EEINST_LIVE0 | EEINST_USED; \
		pinst->regs[reg] |= EEINST_USED; \
		_recFillRegister(*pinst, XMMTYPE_GPRREG, reg, 0); \
	}

#define rpsxpropSetWrite(reg) \
	{ \
		prev->regs[reg] &= ~EEINST_LIVE0; \
		if (!(pinst->regs[reg] & EEINST_USED)) \
			pinst->regs[reg] |= EEINST_LASTUSE; \
		pinst->regs[reg] |= EEINST_USED; \
		prev->regs[reg] |= EEINST_USED; \
		_recFillRegister(*pinst, XMMTYPE_GPRREG, reg, 1); \
	}

void rpsxpropBSC(EEINST* prev, EEINST* pinst);
void rpsxpropSPECIAL(EEINST* prev, EEINST* pinst);
void rpsxpropREGIMM(EEINST* prev, EEINST* pinst);
void rpsxpropCP0(EEINST* prev, EEINST* pinst);
void rpsxpropCP2(EEINST* prev, EEINST* pinst);

//SPECIAL, REGIMM, J   , JAL  , BEQ , BNE , BLEZ, BGTZ,
//ADDI   , ADDIU , SLTI, SLTIU, ANDI, ORI , XORI, LUI ,
//COP0   , NULL  , COP2, NULL , NULL, NULL, NULL, NULL,
//NULL   , NULL  , NULL, NULL , NULL, NULL, NULL, NULL,
//LB     , LH    , LWL , LW   , LBU , LHU , LWR , NULL,
//SB     , SH    , SWL , SW   , NULL, NULL, SWR , NULL,
//NULL   , NULL  , NULL, NULL , NULL, NULL, NULL, NULL,
//NULL   , NULL  , NULL, NULL , NULL, NULL, NULL, NULL
void rpsxpropBSC(EEINST* prev, EEINST* pinst)
{
	switch (psxRegs.code >> 26)
	{
		case 0:
			rpsxpropSPECIAL(prev, pinst);
			break;
		case 1:
			rpsxpropREGIMM(prev, pinst);
			break;
		case 2: // j
			break;
		case 3: // jal
			rpsxpropSetWrite(31);
			break;
		case 4: // beq
		case 5: // bne
			rpsxpropSetRead(_Rs_);
			rpsxpropSetRead(_Rt_);
			break;

		case 6: // blez
		case 7: // bgtz
			rpsxpropSetRead(_Rs_);
			break;

		case 15: // lui
			rpsxpropSetWrite(_Rt_);
			break;

		case 16:
			rpsxpropCP0(prev, pinst);
			break;
		case 18:
			rpsxpropCP2(prev, pinst);
			break;

		// stores
		case 40: case 41: case 42: case 43: case 46:
			rpsxpropSetRead(_Rt_);
			rpsxpropSetRead(_Rs_);
			break;

		case 50: // LWC2
		case 58: // SWC2
			// Operation on COP2 registers/memory. GPRs are left untouched
			break;

		default:
			rpsxpropSetWrite(_Rt_);
			rpsxpropSetRead(_Rs_);
			break;
	}
}

//SLL , NULL, SRL , SRA , SLLV   , NULL , SRLV, SRAV,
//JR  , JALR, NULL, NULL, SYSCALL, BREAK, NULL, NULL,
//MFHI, MTHI, MFLO, MTLO, NULL   , NULL , NULL, NULL,
//MULT, MULTU, DIV, DIVU, NULL   , NULL , NULL, NULL,
//ADD , ADDU, SUB , SUBU, AND    , OR   , XOR , NOR ,
//NULL, NULL, SLT , SLTU, NULL   , NULL , NULL, NULL,
//NULL, NULL, NULL, NULL, NULL   , NULL , NULL, NULL,
//NULL, NULL, NULL, NULL, NULL   , NULL , NULL, NULL,
void rpsxpropSPECIAL(EEINST* prev, EEINST* pinst)
{
	switch (_Funct_)
	{
		case 0: // SLL
		case 2: // SRL
		case 3: // SRA
			rpsxpropSetWrite(_Rd_);
			rpsxpropSetRead(_Rt_);
			break;

		case 8: // JR
			rpsxpropSetRead(_Rs_);
			break;
		case 9: // JALR
			rpsxpropSetWrite(_Rd_);
			rpsxpropSetRead(_Rs_);
			break;

		case 12: // syscall
		case 13: // break
			_recClearInst(prev);
			prev->info = 0;
			break;
		case 15: // sync
			break;

		case 16: // mfhi
			rpsxpropSetWrite(_Rd_);
			rpsxpropSetRead(PSX_HI);
			break;
		case 17: // mthi
			rpsxpropSetWrite(PSX_HI);
			rpsxpropSetRead(_Rs_);
			break;
		case 18: // mflo
			rpsxpropSetWrite(_Rd_);
			rpsxpropSetRead(PSX_LO);
			break;
		case 19: // mtlo
			rpsxpropSetWrite(PSX_LO);
			rpsxpropSetRead(_Rs_);
			break;

		case 24: // mult
		case 25: // multu
		case 26: // div
		case 27: // divu
			rpsxpropSetWrite(PSX_LO);
			rpsxpropSetWrite(PSX_HI);
			rpsxpropSetRead(_Rs_);
			rpsxpropSetRead(_Rt_);
			break;

		case 32: // add
		case 33: // addu
		case 34: // sub
		case 35: // subu
			rpsxpropSetWrite(_Rd_);
			if (_Rs_)
				rpsxpropSetRead(_Rs_);
			if (_Rt_)
				rpsxpropSetRead(_Rt_);
			break;

		default:
			rpsxpropSetWrite(_Rd_);
			rpsxpropSetRead(_Rs_);
			rpsxpropSetRead(_Rt_);
			break;
	}
}

//BLTZ  , BGEZ  , NULL, NULL, NULL, NULL, NULL, NULL,
//NULL  , NULL  , NULL, NULL, NULL, NULL, NULL, NULL,
//BLTZAL, BGEZAL, NULL, NULL, NULL, NULL, NULL, NULL,
//NULL  , NULL  , NULL, NULL, NULL, NULL, NULL, NULL
void rpsxpropREGIMM(EEINST* prev, EEINST* pinst)
{
	switch (_Rt_)
	{
		case 0: // bltz
		case 1: // bgez
			rpsxpropSetRead(_Rs_);
			break;

		case 16: // bltzal
		case 17: // bgezal
			// do not write 31
			rpsxpropSetRead(_Rs_);
			break;

		jNO_DEFAULT
	}
}

//MFC0, NULL, CFC0, NULL, MTC0, NULL, CTC0, NULL,
//NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
//RFE , NULL, NULL, NULL, NULL, NULL, NULL, NULL,
//NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
void rpsxpropCP0(EEINST* prev, EEINST* pinst)
{
	switch (_Rs_)
	{
		case 0: // mfc0
		case 2: // cfc0
			rpsxpropSetWrite(_Rt_);
			break;

		case 4: // mtc0
		case 6: // ctc0
			rpsxpropSetRead(_Rt_);
			break;
		case 16: // rfe
			break;

		jNO_DEFAULT
	}
}


// Basic table:
// gteMFC2, psxNULL, gteCFC2, psxNULL, gteMTC2, psxNULL, gteCTC2, psxNULL,
// psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
// psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
// psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
void rpsxpropCP2_basic(EEINST* prev, EEINST* pinst)
{
	switch (_Rs_)
	{
		case 0: // mfc2
		case 2: // cfc2
			rpsxpropSetWrite(_Rt_);
			break;

		case 4: // mtc2
		case 6: // ctc2
			rpsxpropSetRead(_Rt_);
			break;

		default:
			pxFailDev("iop invalid opcode in const propagation (rpsxpropCP2/BASIC)");
			break;
	}
}


// Main table:
// psxBASIC, gteRTPS , psxNULL , psxNULL, psxNULL, psxNULL , gteNCLIP, psxNULL, // 00
// psxNULL , psxNULL , psxNULL , psxNULL, gteOP  , psxNULL , psxNULL , psxNULL, // 08
// gteDPCS , gteINTPL, gteMVMVA, gteNCDS, gteCDP , psxNULL , gteNCDT , psxNULL, // 10
// psxNULL , psxNULL , psxNULL , gteNCCS, gteCC  , psxNULL , gteNCS  , psxNULL, // 18
// gteNCT  , psxNULL , psxNULL , psxNULL, psxNULL, psxNULL , psxNULL , psxNULL, // 20
// gteSQR  , gteDCPL , gteDPCT , psxNULL, psxNULL, gteAVSZ3, gteAVSZ4, psxNULL, // 28
// gteRTPT , psxNULL , psxNULL , psxNULL, psxNULL, psxNULL , psxNULL , psxNULL, // 30
// psxNULL , psxNULL , psxNULL , psxNULL, psxNULL, gteGPF  , gteGPL  , gteNCCT, // 38
void rpsxpropCP2(EEINST* prev, EEINST* pinst)
{
	switch (_Funct_)
	{
		case 0: // Basic opcode
			rpsxpropCP2_basic(prev, pinst);
			break;

		default:
			// COP2 operation are likely done with internal COP2 registers
			// No impact on GPR
			break;
	}
}
