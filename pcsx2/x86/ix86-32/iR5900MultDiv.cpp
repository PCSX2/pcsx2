// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "x86/iR5900.h"

using namespace x86Emitter;

namespace Interp = R5900::Interpreter::OpcodeImpl;

namespace R5900::Dynarec::OpcodeImpl
{

/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/
#ifndef MULTDIV_RECOMPILE

REC_FUNC_DEL(MULT, _Rd_);
REC_FUNC_DEL(MULTU, _Rd_);
REC_FUNC_DEL(MULT1, _Rd_);
REC_FUNC_DEL(MULTU1, _Rd_);

REC_FUNC(DIV);
REC_FUNC(DIVU);
REC_FUNC(DIV1);
REC_FUNC(DIVU1);

REC_FUNC_DEL(MADD, _Rd_);
REC_FUNC_DEL(MADDU, _Rd_);
REC_FUNC_DEL(MADD1, _Rd_);
REC_FUNC_DEL(MADDU1, _Rd_);

#else

static void recWritebackHILO(int info, bool writed, bool upper)
{
	// writeback low 32 bits, sign extended to 64 bits
	bool eax_sign_extended = false;

	// case 1: LO is already in an XMM - use the xmm
	// case 2: LO is used as an XMM later in the block - use or allocate the XMM
	// case 3: LO is used as a GPR later in the block - use XMM if upper, otherwise use GPR, so it can be renamed
	// case 4: LO is already in a GPR - write to the GPR, or write to memory if upper
	// case 4: LO is not used - writeback to memory

	if (EEINST_LIVETEST(XMMGPR_LO))
	{
		const bool loused = EEINST_USEDTEST(XMMGPR_LO);
		const bool lousedxmm = loused && (upper || EEINST_XMMUSEDTEST(XMMGPR_LO));
		const int xmmlo = lousedxmm ? _allocGPRtoXMMreg(XMMGPR_LO, MODE_READ | MODE_WRITE) : _checkXMMreg(XMMTYPE_GPRREG, XMMGPR_LO, MODE_WRITE);
		if (xmmlo >= 0)
		{
			// we use CDQE over MOVSX because it's shorter.
			xCDQE();
			xPINSR.Q(xRegisterSSE(xmmlo), rax, static_cast<u8>(upper));
		}
		else
		{
			const int gprlo = upper ? -1 : (loused ? _allocX86reg(X86TYPE_GPR, XMMGPR_LO, MODE_WRITE) : _checkX86reg(X86TYPE_GPR, XMMGPR_LO, MODE_WRITE));
			if (gprlo >= 0)
			{
				xMOVSX(xRegister64(gprlo), eax);
			}
			else
			{
				xCDQE();
				eax_sign_extended = true;
				xMOV(ptr64[&cpuRegs.LO.UD[upper]], rax);
			}
		}
	}

	if (EEINST_LIVETEST(XMMGPR_HI))
	{
		const bool hiused = EEINST_USEDTEST(XMMGPR_HI);
		const bool hiusedxmm = hiused && (upper || EEINST_XMMUSEDTEST(XMMGPR_HI));
		const int xmmhi = hiusedxmm ? _allocGPRtoXMMreg(XMMGPR_HI, MODE_READ | MODE_WRITE) : _checkXMMreg(XMMTYPE_GPRREG, XMMGPR_HI, MODE_WRITE);
		if (xmmhi >= 0)
		{
			xMOVSX(rdx, edx);
			xPINSR.Q(xRegisterSSE(xmmhi), rdx, static_cast<u8>(upper));
		}
		else
		{
			const int gprhi = upper ? -1 : (hiused ? _allocX86reg(X86TYPE_GPR, XMMGPR_HI, MODE_WRITE) : _checkX86reg(X86TYPE_GPR, XMMGPR_HI, MODE_WRITE));
			if (gprhi >= 0)
			{
				xMOVSX(xRegister64(gprhi), edx);
			}
			else
			{
				xMOVSX(rdx, edx);
				xMOV(ptr64[&cpuRegs.HI.UD[upper]], rdx);
			}
		}
	}

	// writeback lo to Rd if present
	if (writed && _Rd_ && EEINST_LIVETEST(_Rd_))
	{
		// TODO: This can be made optimal by keeping it in an xmm.
		// But currently the templates aren't hooked up for that - we'd need a "allow xmm" flag.
		if (info & PROCESS_EE_D)
		{
			if (eax_sign_extended)
				xMOV(xRegister64(EEREC_D), rax);
			else
				xMOVSX(xRegister64(EEREC_D), eax);
		}
		else
		{
			if (!eax_sign_extended)
				xCDQE();
			xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
		}
	}
}


static void recWritebackConstHILO(u64 res, bool writed, int upper)
{
	// It's not often that MULT/DIV are entirely constant. So while the MOV64s here are not optimal
	// by any means, it's not something that's going to be hit often enough to worry about a cache.
	// Except for apparently when it's getting set to all-zeros, but that'll be fine with immediates.
	const s64 loval = static_cast<s64>(static_cast<s32>(static_cast<u32>(res)));
	const s64 hival = static_cast<s64>(static_cast<s32>(static_cast<u32>(res >> 32)));

	if (EEINST_LIVETEST(XMMGPR_LO))
	{
		const bool lolive = EEINST_USEDTEST(XMMGPR_LO);
		const bool lolivexmm = lolive && (upper || EEINST_XMMUSEDTEST(XMMGPR_LO));
		const int xmmlo = lolivexmm ? _allocGPRtoXMMreg(XMMGPR_LO, MODE_READ | MODE_WRITE) : _checkXMMreg(XMMTYPE_GPRREG, XMMGPR_LO, MODE_WRITE);
		if (xmmlo >= 0)
		{
			xMOV64(rax, loval);
			xPINSR.Q(xRegisterSSE(xmmlo), rax, static_cast<u8>(upper));
		}
		else
		{
			const int gprlo = upper ? -1 : (lolive ? _allocX86reg(X86TYPE_GPR, XMMGPR_LO, MODE_WRITE) : _checkX86reg(X86TYPE_GPR, XMMGPR_LO, MODE_WRITE));
			if (gprlo >= 0)
				xImm64Op(xMOV, xRegister64(gprlo), rax, loval);
			else
				xImm64Op(xMOV, ptr64[&cpuRegs.LO.UD[upper]], rax, loval);
		}
	}

	if (EEINST_LIVETEST(XMMGPR_HI))
	{
		const bool hilive = EEINST_USEDTEST(XMMGPR_HI);
		const bool hilivexmm = hilive && (upper || EEINST_XMMUSEDTEST(XMMGPR_HI));
		const int xmmhi = hilivexmm ? _allocGPRtoXMMreg(XMMGPR_HI, MODE_READ | MODE_WRITE) : _checkXMMreg(XMMTYPE_GPRREG, XMMGPR_HI, MODE_WRITE);
		if (xmmhi >= 0)
		{
			xMOV64(rax, hival);
			xPINSR.Q(xRegisterSSE(xmmhi), rax, static_cast<u8>(upper));
		}
		else
		{
			const int gprhi = upper ? -1 : (hilive ? _allocX86reg(X86TYPE_GPR, XMMGPR_HI, MODE_WRITE) : _checkX86reg(X86TYPE_GPR, XMMGPR_HI, MODE_WRITE));
			if (gprhi >= 0)
				xImm64Op(xMOV, xRegister64(gprhi), rax, hival);
			else
				xImm64Op(xMOV, ptr64[&cpuRegs.HI.UD[upper]], rax, hival);
		}
	}

	// writeback lo to Rd if present
	if (writed && _Rd_ && EEINST_LIVETEST(_Rd_))
	{
		_eeOnWriteReg(_Rd_, 0);

		const int regd = _checkX86reg(X86TYPE_GPR, _Rd_, MODE_WRITE);
		if (regd >= 0)
			xImm64Op(xMOV, xRegister64(regd), rax, loval);
		else
			xImm64Op(xMOV, ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], rax, loval);
	}
}

//// MULT
static void recMULT_const()
{
	s64 res = (s64)g_cpuConstRegs[_Rs_].SL[0] * (s64)g_cpuConstRegs[_Rt_].SL[0];

	recWritebackConstHILO(res, 1, 0);
}

static void recMULTsuper(int info, bool sign, bool upper, int process)
{
	// TODO(Stenzek): Use MULX where available.
	if (process & PROCESS_CONSTS)
	{
		xMOV(eax, g_cpuConstRegs[_Rs_].UL[0]);
		if (info & PROCESS_EE_T)
			sign ? xMUL(xRegister32(EEREC_T)) : xUMUL(xRegister32(EEREC_T));
		else
			sign ? xMUL(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]) : xUMUL(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	}
	else if (process & PROCESS_CONSTT)
	{
		xMOV(eax, g_cpuConstRegs[_Rt_].UL[0]);
		if (info & PROCESS_EE_S)
			sign ? xMUL(xRegister32(EEREC_S)) : xUMUL(xRegister32(EEREC_S));
		else
			sign ? xMUL(ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]]) : xUMUL(ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]]);
	}
	else
	{
		// S is more likely to be in a register than T (so put T in eax).
		if (info & PROCESS_EE_T)
			xMOV(eax, xRegister32(EEREC_T));
		else
			xMOV(eax, ptr[&cpuRegs.GPR.r[_Rt_].UL[0]]);

		if (info & PROCESS_EE_S)
			sign ? xMUL(xRegister32(EEREC_S)) : xUMUL(xRegister32(EEREC_S));
		else
			sign ? xMUL(ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]]) : xUMUL(ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]]);
	}

	recWritebackHILO(info, 1, upper);
}

static void recMULT_(int info)
{
	recMULTsuper(info, true, false, 0);
}

static void recMULT_consts(int info)
{
	recMULTsuper(info, true, false, PROCESS_CONSTS);
}

static void recMULT_constt(int info)
{
	recMULTsuper(info, true, false, PROCESS_CONSTT);
}

// lo/hi allocation are taken care of in recWritebackHILO().
EERECOMPILE_CODERC0(MULT, XMMINFO_READS | XMMINFO_READT | (_Rd_ ? XMMINFO_WRITED : 0));

//// MULTU
static void recMULTU_const()
{
	const u64 res = (u64)g_cpuConstRegs[_Rs_].UL[0] * (u64)g_cpuConstRegs[_Rt_].UL[0];

	recWritebackConstHILO(res, 1, 0);
}

static void recMULTU_(int info)
{
	recMULTsuper(info, false, false, 0);
}

static void recMULTU_consts(int info)
{
	recMULTsuper(info, false, false, PROCESS_CONSTS);
}

static void recMULTU_constt(int info)
{
	recMULTsuper(info, false, false, PROCESS_CONSTT);
}

// don't specify XMMINFO_WRITELO or XMMINFO_WRITEHI, that is taken care of
EERECOMPILE_CODERC0(MULTU, XMMINFO_READS | XMMINFO_READT | (_Rd_ ? XMMINFO_WRITED : 0));

////////////////////////////////////////////////////
static void recMULT1_const()
{
	s64 res = (s64)g_cpuConstRegs[_Rs_].SL[0] * (s64)g_cpuConstRegs[_Rt_].SL[0];

	recWritebackConstHILO((u64)res, 1, 1);
}

static void recMULT1_(int info)
{
	recMULTsuper(info, true, true, 0);
}

static void recMULT1_consts(int info)
{
	recMULTsuper(info, true, true, PROCESS_CONSTS);
}

static void recMULT1_constt(int info)
{
	recMULTsuper(info, true, true, PROCESS_CONSTT);
}

EERECOMPILE_CODERC0(MULT1, XMMINFO_READS | XMMINFO_READT | (_Rd_ ? XMMINFO_WRITED : 0));

////////////////////////////////////////////////////
static void recMULTU1_const()
{
	u64 res = (u64)g_cpuConstRegs[_Rs_].UL[0] * (u64)g_cpuConstRegs[_Rt_].UL[0];

	recWritebackConstHILO(res, 1, 1);
}

static void recMULTU1_(int info)
{
	recMULTsuper(info, false, true, 0);
}

static void recMULTU1_consts(int info)
{
	recMULTsuper(info, false, true, PROCESS_CONSTS);
}

static void recMULTU1_constt(int info)
{
	recMULTsuper(info, false, true, PROCESS_CONSTT);
}

EERECOMPILE_CODERC0(MULTU1, XMMINFO_READS | XMMINFO_READT | (_Rd_ ? XMMINFO_WRITED : 0));

//// DIV

static void recDIVconst(int upper)
{
	s32 quot, rem;
	if (g_cpuConstRegs[_Rs_].UL[0] == 0x80000000 && g_cpuConstRegs[_Rt_].SL[0] == -1)
	{
		quot = (s32)0x80000000;
		rem = 0;
	}
	else if (g_cpuConstRegs[_Rt_].SL[0] != 0)
	{
		quot = g_cpuConstRegs[_Rs_].SL[0] / g_cpuConstRegs[_Rt_].SL[0];
		rem = g_cpuConstRegs[_Rs_].SL[0] % g_cpuConstRegs[_Rt_].SL[0];
	}
	else
	{
		quot = (g_cpuConstRegs[_Rs_].SL[0] < 0) ? 1 : -1;
		rem = g_cpuConstRegs[_Rs_].SL[0];
	}
	recWritebackConstHILO((u64)quot | ((u64)rem << 32), 0, upper);
}

static void recDIV_const()
{
	recDIVconst(0);
}

static void recDIVsuper(int info, bool sign, bool upper, int process)
{
	const xRegister32 divisor((info & PROCESS_EE_T) ? EEREC_T : ecx.GetId());
	if (!(info & PROCESS_EE_T))
	{
		if (process & PROCESS_CONSTT)
			xMOV(divisor, g_cpuConstRegs[_Rt_].UL[0]);
		else
			xMOV(divisor, ptr[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	}

	// can't use edx, it's part of the dividend
	pxAssert(divisor.GetId() != edx.GetId());

	if (process & PROCESS_CONSTS)
		xMOV(eax, g_cpuConstRegs[_Rs_].UL[0]);
	else
		_eeMoveGPRtoR(rax, _Rs_);

	u8* end1;
	if (sign) //test for overflow (x86 will just throw an exception)
	{
		xCMP(eax, 0x80000000);
		u8* cont1 = JNE8(0);
		xCMP(divisor, 0xffffffff);
		u8* cont2 = JNE8(0);
		//overflow case:
		xXOR(edx, edx); //EAX remains 0x80000000
		end1 = JMP8(0);

		x86SetJ8(cont1);
		x86SetJ8(cont2);
	}

	xCMP(divisor, 0);
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

	x86SetJ8(cont3);
	if (sign)
	{
		xCDQ();
		xDIV(divisor);
	}
	else
	{
		xXOR(edx, edx);
		xUDIV(divisor);
	}

	if (sign)
		x86SetJ8(end1);
	x86SetJ8(end2);

	// need to execute regardless of bad divide
	recWritebackHILO(info, false, upper);
}

static void recDIV_(int info)
{
	recDIVsuper(info, 1, 0, 0);
}

static void recDIV_consts(int info)
{
	recDIVsuper(info, 1, 0, PROCESS_CONSTS);
}

static void recDIV_constt(int info)
{
	recDIVsuper(info, 1, 0, PROCESS_CONSTT);
}

// We handle S reading in the routine itself, since it needs to go into eax.
EERECOMPILE_CODERC0(DIV, /*XMMINFO_READS |*/ XMMINFO_READT);

//// DIVU
static void recDIVUconst(int upper)
{
	u32 quot, rem;
	if (g_cpuConstRegs[_Rt_].UL[0] != 0)
	{
		quot = g_cpuConstRegs[_Rs_].UL[0] / g_cpuConstRegs[_Rt_].UL[0];
		rem = g_cpuConstRegs[_Rs_].UL[0] % g_cpuConstRegs[_Rt_].UL[0];
	}
	else
	{
		quot = 0xffffffff;
		rem = g_cpuConstRegs[_Rs_].UL[0];
	}

	recWritebackConstHILO((u64)quot | ((u64)rem << 32), 0, upper);
}

static void recDIVU_const()
{
	recDIVUconst(0);
}

static void recDIVU_(int info)
{
	recDIVsuper(info, false, false, 0);
}

static void recDIVU_consts(int info)
{
	recDIVsuper(info, false, false, PROCESS_CONSTS);
}

static void recDIVU_constt(int info)
{
	recDIVsuper(info, false, false, PROCESS_CONSTT);
}

EERECOMPILE_CODERC0(DIVU, /*XMMINFO_READS |*/ XMMINFO_READT);

static void recDIV1_const()
{
	recDIVconst(1);
}

static void recDIV1_(int info)
{
	recDIVsuper(info, true, true, 0);
}

static void recDIV1_consts(int info)
{
	recDIVsuper(info, true, true, PROCESS_CONSTS);
}

static void recDIV1_constt(int info)
{
	recDIVsuper(info, true, true, PROCESS_CONSTT);
}

EERECOMPILE_CODERC0(DIV1, /*XMMINFO_READS |*/ XMMINFO_READT);

static void recDIVU1_const()
{
	recDIVUconst(1);
}

static void recDIVU1_(int info)
{
	recDIVsuper(info, false, true, 0);
}

static void recDIVU1_consts(int info)
{
	recDIVsuper(info, false, true, PROCESS_CONSTS);
}

static void recDIVU1_constt(int info)
{
	recDIVsuper(info, false, true, PROCESS_CONSTT);
}

EERECOMPILE_CODERC0(DIVU1, /*XMMINFO_READS |*/ XMMINFO_READT);

// TODO(Stenzek): All of these :(

static void writeBackMAddToHiLoRd(int hiloID)
{
	// eax -> LO, edx -> HI
	xCDQE();
	if (_Rd_)
	{
		_eeOnWriteReg(_Rd_, 1);
		_deleteEEreg(_Rd_, 0);
		xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
	}
	xMOV(ptr[&cpuRegs.LO.UD[hiloID]], rax);

	xMOVSX(rax, edx);
	xMOV(ptr[&cpuRegs.HI.UD[hiloID]], rax);
}

static void addConstantAndWriteBackToHiLoRd(int hiloID, u64 constant)
{
	const xRegister32& ehi = edx;

	_deleteEEreg(XMMGPR_LO, 1);
	_deleteEEreg(XMMGPR_HI, 1);

	xMOV(eax, ptr[&cpuRegs.LO.UL[hiloID * 2]]);
	xMOV(ehi, ptr[&cpuRegs.HI.UL[hiloID * 2]]);
	xADD(eax, (u32)(constant & 0xffffffff));
	xADC(ehi, (u32)(constant >> 32));
	writeBackMAddToHiLoRd(hiloID);
}

static void addEaxEdxAndWriteBackToHiLoRd(int hiloID)
{
	xADD(eax, ptr[&cpuRegs.LO.UL[hiloID * 2]]);
	xADC(edx, ptr[&cpuRegs.HI.UL[hiloID * 2]]);

	writeBackMAddToHiLoRd(hiloID);
}

void recMADD()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		u64 result = ((s64)g_cpuConstRegs[_Rs_].SL[0] * (s64)g_cpuConstRegs[_Rt_].SL[0]);
		addConstantAndWriteBackToHiLoRd(0, result);
		return;
	}

	_deleteEEreg(XMMGPR_LO, 1);
	_deleteEEreg(XMMGPR_HI, 1);
	_deleteGPRtoX86reg(_Rs_, DELETE_REG_FLUSH);
	_deleteGPRtoX86reg(_Rt_, DELETE_REG_FLUSH);
	_deleteGPRtoXMMreg(_Rs_, DELETE_REG_FLUSH);
	_deleteGPRtoXMMreg(_Rt_, DELETE_REG_FLUSH);

	if (GPR_IS_CONST1(_Rs_))
	{
		xMOV(eax, g_cpuConstRegs[_Rs_].UL[0]);
		xMUL(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	}
	else if (GPR_IS_CONST1(_Rt_))
	{
		xMOV(eax, g_cpuConstRegs[_Rt_].UL[0]);
		xMUL(ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]]);
	}
	else
	{
		xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
		xMUL(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	}

	addEaxEdxAndWriteBackToHiLoRd(0);
}

void recMADDU()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		u64 result = ((u64)g_cpuConstRegs[_Rs_].UL[0] * (u64)g_cpuConstRegs[_Rt_].UL[0]);
		addConstantAndWriteBackToHiLoRd(0, result);
		return;
	}

	_deleteEEreg(XMMGPR_LO, 1);
	_deleteEEreg(XMMGPR_HI, 1);
	_deleteGPRtoX86reg(_Rs_, DELETE_REG_FLUSH);
	_deleteGPRtoX86reg(_Rt_, DELETE_REG_FLUSH);
	_deleteGPRtoXMMreg(_Rs_, DELETE_REG_FLUSH);
	_deleteGPRtoXMMreg(_Rt_, DELETE_REG_FLUSH);

	if (GPR_IS_CONST1(_Rs_))
	{
		xMOV(eax, g_cpuConstRegs[_Rs_].UL[0]);
		xUMUL(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	}
	else if (GPR_IS_CONST1(_Rt_))
	{
		xMOV(eax, g_cpuConstRegs[_Rt_].UL[0]);
		xUMUL(ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]]);
	}
	else
	{
		xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
		xUMUL(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	}

	addEaxEdxAndWriteBackToHiLoRd(0);
}

void recMADD1()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		u64 result = ((s64)g_cpuConstRegs[_Rs_].SL[0] * (s64)g_cpuConstRegs[_Rt_].SL[0]);
		addConstantAndWriteBackToHiLoRd(1, result);
		return;
	}

	_deleteEEreg(XMMGPR_LO, 1);
	_deleteEEreg(XMMGPR_HI, 1);
	_deleteGPRtoX86reg(_Rs_, DELETE_REG_FLUSH);
	_deleteGPRtoX86reg(_Rt_, DELETE_REG_FLUSH);
	_deleteGPRtoXMMreg(_Rs_, DELETE_REG_FLUSH);
	_deleteGPRtoXMMreg(_Rt_, DELETE_REG_FLUSH);

	if (GPR_IS_CONST1(_Rs_))
	{
		xMOV(eax, g_cpuConstRegs[_Rs_].UL[0]);
		xMUL(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	}
	else if (GPR_IS_CONST1(_Rt_))
	{
		xMOV(eax, g_cpuConstRegs[_Rt_].UL[0]);
		xMUL(ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]]);
	}
	else
	{
		xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
		xMUL(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	}

	addEaxEdxAndWriteBackToHiLoRd(1);
}

void recMADDU1()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		u64 result = ((u64)g_cpuConstRegs[_Rs_].UL[0] * (u64)g_cpuConstRegs[_Rt_].UL[0]);
		addConstantAndWriteBackToHiLoRd(1, result);
		return;
	}

	_deleteEEreg(XMMGPR_LO, 1);
	_deleteEEreg(XMMGPR_HI, 1);
	_deleteGPRtoX86reg(_Rs_, DELETE_REG_FLUSH);
	_deleteGPRtoX86reg(_Rt_, DELETE_REG_FLUSH);
	_deleteGPRtoXMMreg(_Rs_, DELETE_REG_FLUSH);
	_deleteGPRtoXMMreg(_Rt_, DELETE_REG_FLUSH);

	if (GPR_IS_CONST1(_Rs_))
	{
		xMOV(eax, g_cpuConstRegs[_Rs_].UL[0]);
		xUMUL(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	}
	else if (GPR_IS_CONST1(_Rt_))
	{
		xMOV(eax, g_cpuConstRegs[_Rt_].UL[0]);
		xUMUL(ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]]);
	}
	else
	{
		xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
		xUMUL(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	}

	addEaxEdxAndWriteBackToHiLoRd(1);
}


#endif

} // namespace R5900::Dynarec::OpcodeImpl
