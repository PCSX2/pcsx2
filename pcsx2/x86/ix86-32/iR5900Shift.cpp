// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "x86/iR5900.h"

using namespace x86Emitter;

namespace R5900::Dynarec::OpcodeImpl
{

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
#ifndef SHIFT_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC_DEL(SLL, _Rd_);
REC_FUNC_DEL(SRL, _Rd_);
REC_FUNC_DEL(SRA, _Rd_);
REC_FUNC_DEL(DSLL, _Rd_);
REC_FUNC_DEL(DSRL, _Rd_);
REC_FUNC_DEL(DSRA, _Rd_);
REC_FUNC_DEL(DSLL32, _Rd_);
REC_FUNC_DEL(DSRL32, _Rd_);
REC_FUNC_DEL(DSRA32, _Rd_);

REC_FUNC_DEL(SLLV, _Rd_);
REC_FUNC_DEL(SRLV, _Rd_);
REC_FUNC_DEL(SRAV, _Rd_);
REC_FUNC_DEL(DSLLV, _Rd_);
REC_FUNC_DEL(DSRLV, _Rd_);
REC_FUNC_DEL(DSRAV, _Rd_);

#else

static void recMoveTtoD(int info)
{
	if (info & PROCESS_EE_T)
		xMOV(xRegister32(EEREC_D), xRegister32(EEREC_T));
	else
		xMOV(xRegister32(EEREC_D), ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
}

static void recMoveTtoD64(int info)
{
	if (info & PROCESS_EE_T)
		xMOV(xRegister64(EEREC_D), xRegister64(EEREC_T));
	else
		xMOV(xRegister64(EEREC_D), ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]]);
}

static void recMoveSToRCX(int info)
{
	// load full 64-bits for store->load forwarding, since we always store >=64.
	if (info & PROCESS_EE_S)
		xMOV(rcx, xRegister64(EEREC_S));
	else
		xMOV(rcx, ptr64[&cpuRegs.GPR.r[_Rs_].UL[0]]);
}

//// SLL
static void recSLL_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] << _Sa_);
}

static void recSLLs_(int info, int sa)
{
	// TODO: Use BMI
	pxAssert(!(info & PROCESS_EE_XMM));

	recMoveTtoD(info);
	if (sa != 0)
		xSHL(xRegister32(EEREC_D), sa);
	xMOVSX(xRegister64(EEREC_D), xRegister32(EEREC_D));
}

static void recSLL_(int info)
{
	recSLLs_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, SLL, XMMINFO_WRITED | XMMINFO_READT);

//// SRL
static void recSRL_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] >> _Sa_);
}

static void recSRLs_(int info, int sa)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	recMoveTtoD(info);
	if (sa != 0)
		xSHR(xRegister32(EEREC_D), sa);
	xMOVSX(xRegister64(EEREC_D), xRegister32(EEREC_D));
}

static void recSRL_(int info)
{
	recSRLs_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, SRL, XMMINFO_WRITED | XMMINFO_READT);

//// SRA
static void recSRA_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].SL[0] >> _Sa_);
}

static void recSRAs_(int info, int sa)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	recMoveTtoD(info);
	if (sa != 0)
		xSAR(xRegister32(EEREC_D), sa);
	xMOVSX(xRegister64(EEREC_D), xRegister32(EEREC_D));
}

static void recSRA_(int info)
{
	recSRAs_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, SRA, XMMINFO_WRITED | XMMINFO_READT);

////////////////////////////////////////////////////
static void recDSLL_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] << _Sa_);
}

static void recDSLLs_(int info, int sa)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	recMoveTtoD64(info);
	if (sa != 0)
		xSHL(xRegister64(EEREC_D), sa);
}

static void recDSLL_(int info)
{
	recDSLLs_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, DSLL, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

////////////////////////////////////////////////////
static void recDSRL_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] >> _Sa_);
}

static void recDSRLs_(int info, int sa)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	recMoveTtoD64(info);
	if (sa != 0)
		xSHR(xRegister64(EEREC_D), sa);
}

static void recDSRL_(int info)
{
	recDSRLs_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, DSRL, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

//// DSRA
static void recDSRA_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (u64)(g_cpuConstRegs[_Rt_].SD[0] >> _Sa_);
}

static void recDSRAs_(int info, int sa)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	recMoveTtoD64(info);
	if (sa != 0)
		xSAR(xRegister64(EEREC_D), sa);
}

static void recDSRA_(int info)
{
	recDSRAs_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, DSRA, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

///// DSLL32
static void recDSLL32_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] << (_Sa_ + 32));
}

static void recDSLL32_(int info)
{
	recDSLLs_(info, _Sa_ + 32);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, DSLL32, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

//// DSRL32
static void recDSRL32_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] >> (_Sa_ + 32));
}

static void recDSRL32_(int info)
{
	recDSRLs_(info, _Sa_ + 32);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, DSRL32, XMMINFO_WRITED | XMMINFO_READT);

//// DSRA32
static void recDSRA32_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (u64)(g_cpuConstRegs[_Rt_].SD[0] >> (_Sa_ + 32));
}

static void recDSRA32_(int info)
{
	recDSRAs_(info, _Sa_ + 32);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, DSRA32, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

/*********************************************************
* Shift arithmetic with variant register shift           *
* Format:  OP rd, rt, rs                                 *
*********************************************************/

static void recShiftV_constt(int info, const xImpl_Group2& shift)
{
	pxAssert(_Rs_ != 0);
	recMoveSToRCX(info);
	xMOV(xRegister32(EEREC_D), g_cpuConstRegs[_Rt_].UL[0]);
	shift(xRegister32(EEREC_D), cl);
	xMOVSX(xRegister64(EEREC_D), xRegister32(EEREC_D));
}

static void recShiftV(int info, const xImpl_Group2& shift)
{
	pxAssert(_Rs_ != 0);

	recMoveSToRCX(info);
	recMoveTtoD(info);
	shift(xRegister32(EEREC_D), cl);
	xMOVSX(xRegister64(EEREC_D), xRegister32(EEREC_D));
}

static void recDShiftV_constt(int info, const xImpl_Group2& shift)
{
	pxAssert(_Rs_ != 0);
	recMoveSToRCX(info);
	xMOV64(xRegister64(EEREC_D), g_cpuConstRegs[_Rt_].SD[0]);
	shift(xRegister64(EEREC_D), cl);
}

static void recDShiftV(int info, const xImpl_Group2& shift)
{
	pxAssert(_Rs_ != 0);
	recMoveSToRCX(info);
	recMoveTtoD64(info);
	shift(xRegister64(EEREC_D), cl);
}

//// SLLV
static void recSLLV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] << (g_cpuConstRegs[_Rs_].UL[0] & 0x1f));
}

static void recSLLV_consts(int info)
{
	recSLLs_(info, g_cpuConstRegs[_Rs_].UL[0] & 0x1f);
}

static void recSLLV_constt(int info)
{
	recShiftV_constt(info, xSHL);
}

static void recSLLV_(int info)
{
	recShiftV(info, xSHL);
}

EERECOMPILE_CODERC0(SLLV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// SRLV
static void recSRLV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x1f));
}

static void recSRLV_consts(int info)
{
	recSRLs_(info, g_cpuConstRegs[_Rs_].UL[0] & 0x1f);
}

static void recSRLV_constt(int info)
{
	recShiftV_constt(info, xSHR);
}

static void recSRLV_(int info)
{
	recShiftV(info, xSHR);
}

EERECOMPILE_CODERC0(SRLV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// SRAV
static void recSRAV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].SL[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x1f));
}

static void recSRAV_consts(int info)
{
	recSRAs_(info, g_cpuConstRegs[_Rs_].UL[0] & 0x1f);
}

static void recSRAV_constt(int info)
{
	recShiftV_constt(info, xSAR);
}

static void recSRAV_(int info)
{
	recShiftV(info, xSAR);
}

EERECOMPILE_CODERC0(SRAV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// DSLLV
static void recDSLLV_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] << (g_cpuConstRegs[_Rs_].UL[0] & 0x3f));
}

static void recDSLLV_consts(int info)
{
	int sa = g_cpuConstRegs[_Rs_].UL[0] & 0x3f;
	recDSLLs_(info, sa);
}

static void recDSLLV_constt(int info)
{
	recDShiftV_constt(info, xSHL);
}

static void recDSLLV_(int info)
{
	recDShiftV(info, xSHL);
}

EERECOMPILE_CODERC0(DSLLV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// DSRLV
static void recDSRLV_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x3f));
}

static void recDSRLV_consts(int info)
{
	int sa = g_cpuConstRegs[_Rs_].UL[0] & 0x3f;
	recDSRLs_(info, sa);
}

static void recDSRLV_constt(int info)
{
	recDShiftV_constt(info, xSHR);
}

static void recDSRLV_(int info)
{
	recDShiftV(info, xSHR);
}

EERECOMPILE_CODERC0(DSRLV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// DSRAV
static void recDSRAV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s64)(g_cpuConstRegs[_Rt_].SD[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x3f));
}

static void recDSRAV_consts(int info)
{
	int sa = g_cpuConstRegs[_Rs_].UL[0] & 0x3f;
	recDSRAs_(info, sa);
}

static void recDSRAV_constt(int info)
{
	recDShiftV_constt(info, xSAR);
}

static void recDSRAV_(int info)
{
	recDShiftV(info, xSAR);
}

EERECOMPILE_CODERC0(DSRAV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

#endif

} // namespace R5900::Dynarec::OpcodeImpl
