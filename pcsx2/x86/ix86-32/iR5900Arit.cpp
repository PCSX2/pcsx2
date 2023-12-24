// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "x86/iR5900.h"

using namespace x86Emitter;

namespace R5900::Dynarec::OpcodeImpl
{
/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/

// TODO: overflow checks

#ifndef ARITHMETIC_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC_DEL(ADD, _Rd_);
REC_FUNC_DEL(ADDU, _Rd_);
REC_FUNC_DEL(DADD, _Rd_);
REC_FUNC_DEL(DADDU, _Rd_);
REC_FUNC_DEL(SUB, _Rd_);
REC_FUNC_DEL(SUBU, _Rd_);
REC_FUNC_DEL(DSUB, _Rd_);
REC_FUNC_DEL(DSUBU, _Rd_);
REC_FUNC_DEL(AND, _Rd_);
REC_FUNC_DEL(OR, _Rd_);
REC_FUNC_DEL(XOR, _Rd_);
REC_FUNC_DEL(NOR, _Rd_);
REC_FUNC_DEL(SLT, _Rd_);
REC_FUNC_DEL(SLTU, _Rd_);

#else

static void recMoveStoD(int info)
{
	if (info & PROCESS_EE_S)
		xMOV(xRegister32(EEREC_D), xRegister32(EEREC_S));
	else
		xMOV(xRegister32(EEREC_D), ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]]);
}

static void recMoveStoD64(int info)
{
	if (info & PROCESS_EE_S)
		xMOV(xRegister64(EEREC_D), xRegister64(EEREC_S));
	else
		xMOV(xRegister64(EEREC_D), ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]]);
}

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

//// ADD
static void recADD_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] + g_cpuConstRegs[_Rt_].UL[0]));
}

// s is constant
static void recADD_consts(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 cval = g_cpuConstRegs[_Rs_].SL[0];
	recMoveTtoD(info);
	if (cval != 0)
		xADD(xRegister32(EEREC_D), cval);
	xMOVSX(xRegister64(EEREC_D), xRegister32(EEREC_D));
}

// t is constant
static void recADD_constt(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 cval = g_cpuConstRegs[_Rt_].SL[0];
	recMoveStoD(info);
	if (cval != 0)
		xADD(xRegister32(EEREC_D), cval);
	xMOVSX(xRegister64(EEREC_D), xRegister32(EEREC_D));
}

// nothing is constant
static void recADD_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			xADD(xRegister32(EEREC_D), xRegister32(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			xADD(xRegister32(EEREC_D), xRegister32(EEREC_S));
		}
		else
		{
			xMOV(xRegister32(EEREC_D), xRegister32(EEREC_S));
			xADD(xRegister32(EEREC_D), xRegister32(EEREC_T));
		}
	}
	else if (info & PROCESS_EE_S)
	{
		xMOV(xRegister32(EEREC_D), xRegister32(EEREC_S));
		xADD(xRegister32(EEREC_D), ptr32[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	}
	else if (info & PROCESS_EE_T)
	{
		xMOV(xRegister32(EEREC_D), xRegister32(EEREC_T));
		xADD(xRegister32(EEREC_D), ptr32[&cpuRegs.GPR.r[_Rs_].UD[0]]);
	}
	else
	{
		xMOV(xRegister32(EEREC_D), ptr32[&cpuRegs.GPR.r[_Rs_].UD[0]]);
		xADD(xRegister32(EEREC_D), ptr32[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	}

	xMOVSX(xRegister64(EEREC_D), xRegister32(EEREC_D));
}

EERECOMPILE_CODERC0(ADD, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

//// ADDU
void recADDU(void)
{
	recADD();
}

//// DADD
void recDADD_const(void)
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] + g_cpuConstRegs[_Rt_].UD[0];
}

// s is constant
static void recDADD_consts(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 cval = g_cpuConstRegs[_Rs_].SD[0];
	recMoveTtoD64(info);
	if (cval != 0)
		xImm64Op(xADD, xRegister64(EEREC_D), rax, cval);
}

// t is constant
static void recDADD_constt(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 cval = g_cpuConstRegs[_Rt_].SD[0];
	recMoveStoD64(info);
	if (cval != 0)
		xImm64Op(xADD, xRegister64(EEREC_D), rax, cval);
}

// nothing is constant
static void recDADD_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			xADD(xRegister64(EEREC_D), xRegister64(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			xADD(xRegister64(EEREC_D), xRegister64(EEREC_S));
		}
		else
		{
			xMOV(xRegister64(EEREC_D), xRegister64(EEREC_S));
			xADD(xRegister64(EEREC_D), xRegister64(EEREC_T));
		}
	}
	else if (info & PROCESS_EE_S)
	{
		xMOV(xRegister64(EEREC_D), xRegister64(EEREC_S));
		xADD(xRegister64(EEREC_D), ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	}
	else if (info & PROCESS_EE_T)
	{
		xMOV(xRegister64(EEREC_D), xRegister64(EEREC_T));
		xADD(xRegister64(EEREC_D), ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]]);
	}
	else
	{
		xMOV(xRegister64(EEREC_D), ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]]);
		xADD(xRegister64(EEREC_D), ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	}
}

EERECOMPILE_CODERC0(DADD, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// DADDU
void recDADDU(void)
{
	recDADD();
}

//// SUB

static void recSUB_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] - g_cpuConstRegs[_Rt_].UL[0]));
}

static void recSUB_consts(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 sval = g_cpuConstRegs[_Rs_].SL[0];
	xMOV(eax, sval);

	if (info & PROCESS_EE_T)
		xSUB(eax, xRegister32(EEREC_T));
	else
		xSUB(eax, ptr32[&cpuRegs.GPR.r[_Rt_].SL[0]]);

	xMOVSX(xRegister64(EEREC_D), eax);
}

static void recSUB_constt(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 tval = g_cpuConstRegs[_Rt_].SL[0];
	recMoveStoD(info);
	if (tval != 0)
		xSUB(xRegister32(EEREC_D), tval);

	xMOVSX(xRegister64(EEREC_D), xRegister32(EEREC_D));
}

static void recSUB_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	if (_Rs_ == _Rt_)
	{
		xXOR(xRegister32(EEREC_D), xRegister32(EEREC_D));
		return;
	}

	// a bit messier here because it's not commutative..
	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			xSUB(xRegister32(EEREC_D), xRegister32(EEREC_T));
			xMOVSX(xRegister64(EEREC_D), xRegister32(EEREC_D));
		}
		else if (EEREC_D == EEREC_T)
		{
			// D might equal T
			xMOV(eax, xRegister32(EEREC_S));
			xSUB(eax, xRegister32(EEREC_T));
			xMOVSX(xRegister64(EEREC_D), eax);
		}
		else
		{
			xMOV(xRegister32(EEREC_D), xRegister32(EEREC_S));
			xSUB(xRegister32(EEREC_D), xRegister32(EEREC_T));
			xMOVSX(xRegister64(EEREC_D), xRegister32(EEREC_D));
		}
	}
	else if (info & PROCESS_EE_S)
	{
		xMOV(xRegister32(EEREC_D), xRegister32(EEREC_S));
		xSUB(xRegister32(EEREC_D), ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
		xMOVSX(xRegister64(EEREC_D), xRegister32(EEREC_D));
	}
	else if (info & PROCESS_EE_T)
	{
		// D might equal T
		xMOV(eax, ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]]);
		xSUB(eax, xRegister32(EEREC_T));
		xMOVSX(xRegister64(EEREC_D), eax);
	}
	else
	{
		xMOV(xRegister32(EEREC_D), ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]]);
		xSUB(xRegister32(EEREC_D), ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
		xMOVSX(xRegister64(EEREC_D), xRegister32(EEREC_D));
	}
}

EERECOMPILE_CODERC0(SUB, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// SUBU
void recSUBU(void)
{
	recSUB();
}

//// DSUB
static void recDSUB_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] - g_cpuConstRegs[_Rt_].UD[0];
}

static void recDSUB_consts(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	// gross, because if d == t, we can't destroy t
	const s64 sval = g_cpuConstRegs[_Rs_].SD[0];
	const xRegister64 regd((info & PROCESS_EE_T && EEREC_D == EEREC_T) ? rax.GetId() : EEREC_D);
	xMOV64(regd, sval);

	if (info & PROCESS_EE_T)
		xSUB(regd, xRegister64(EEREC_T));
	else
		xSUB(regd, ptr64[&cpuRegs.GPR.r[_Rt_].SD[0]]);

	// emitter will eliminate redundant moves.
	xMOV(xRegister64(EEREC_D), regd);
}

static void recDSUB_constt(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 tval = g_cpuConstRegs[_Rt_].SD[0];
	recMoveStoD64(info);
	if (tval != 0)
		xImm64Op(xSUB, xRegister64(EEREC_D), rax, tval);
}

static void recDSUB_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	if (_Rs_ == _Rt_)
	{
		xXOR(xRegister32(EEREC_D), xRegister32(EEREC_D));
		return;
	}

	// a bit messier here because it's not commutative..
	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		// D might equal T
		const xRegister64 regd(EEREC_D == EEREC_T ? rax.GetId() : EEREC_D);
		xMOV(regd, xRegister64(EEREC_S));
		xSUB(regd, xRegister64(EEREC_T));
		xMOV(xRegister64(EEREC_D), regd);
	}
	else if (info & PROCESS_EE_S)
	{
		xMOV(xRegister64(EEREC_D), xRegister64(EEREC_S));
		xSUB(xRegister64(EEREC_D), ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	}
	else if (info & PROCESS_EE_T)
	{
		// D might equal T
		const xRegister64 regd(EEREC_D == EEREC_T ? rax.GetId() : EEREC_D);
		xMOV(regd, ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]]);
		xSUB(regd, xRegister64(EEREC_T));
		xMOV(xRegister64(EEREC_D), regd);
	}
	else
	{
		xMOV(xRegister64(EEREC_D), ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]]);
		xSUB(xRegister64(EEREC_D), ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	}
}

EERECOMPILE_CODERC0(DSUB, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// DSUBU
void recDSUBU(void)
{
	recDSUB();
}

namespace
{
enum class LogicalOp
{
	AND,
	OR,
	XOR,
	NOR
};
} // namespace

static void recLogicalOp_constv(LogicalOp op, int info, int creg, u32 vreg, int regv)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xImpl_G1Logic bad{};
	const xImpl_G1Logic& xOP = op == LogicalOp::AND ? xAND : op == LogicalOp::OR ? xOR :
														 op == LogicalOp::XOR    ? xXOR :
														 op == LogicalOp::NOR    ? xOR :
                                                                                   bad;
	s64 fixedInput, fixedOutput, identityInput;
	bool hasFixed = true;
	switch (op)
	{
		case LogicalOp::AND:
			fixedInput = 0;
			fixedOutput = 0;
			identityInput = -1;
			break;
		case LogicalOp::OR:
			fixedInput = -1;
			fixedOutput = -1;
			identityInput = 0;
			break;
		case LogicalOp::XOR:
			hasFixed = false;
			identityInput = 0;
			break;
		case LogicalOp::NOR:
			fixedInput = -1;
			fixedOutput = 0;
			identityInput = 0;
			break;
		default:
			pxAssert(0);
	}

	GPR_reg64 cval = g_cpuConstRegs[creg];

	if (hasFixed && cval.SD[0] == fixedInput)
	{
		xMOV64(xRegister64(EEREC_D), fixedOutput);
	}
	else
	{
		if (regv >= 0)
			xMOV(xRegister64(EEREC_D), xRegister64(regv));
		else
			xMOV(xRegister64(EEREC_D), ptr64[&cpuRegs.GPR.r[vreg].UD[0]]);
		if (cval.SD[0] != identityInput)
			xImm64Op(xOP, xRegister64(EEREC_D), rax, cval.UD[0]);
		if (op == LogicalOp::NOR)
			xNOT(xRegister64(EEREC_D));
	}
}

static void recLogicalOp(LogicalOp op, int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xImpl_G1Logic bad{};
	const xImpl_G1Logic& xOP = op == LogicalOp::AND ? xAND : op == LogicalOp::OR ? xOR :
														 op == LogicalOp::XOR    ? xXOR :
														 op == LogicalOp::NOR    ? xOR :
                                                                                   bad;
	pxAssert(&xOP != &bad);

	// swap because it's commutative and Rd might be Rt
	u32 rs = _Rs_, rt = _Rt_;
	int regs = (info & PROCESS_EE_S) ? EEREC_S : -1, regt = (info & PROCESS_EE_T) ? EEREC_T : -1;
	if (_Rd_ == _Rt_)
	{
		std::swap(rs, rt);
		std::swap(regs, regt);
	}

	if (op == LogicalOp::XOR && rs == rt)
	{
		xXOR(xRegister32(EEREC_D), xRegister32(EEREC_D));
	}
	else
	{
		if (regs >= 0)
			xMOV(xRegister64(EEREC_D), xRegister64(regs));
		else
			xMOV(xRegister64(EEREC_D), ptr64[&cpuRegs.GPR.r[rs].UD[0]]);

		if (regt >= 0)
			xOP(xRegister64(EEREC_D), xRegister64(regt));
		else
			xOP(xRegister64(EEREC_D), ptr64[&cpuRegs.GPR.r[rt].UD[0]]);

		if (op == LogicalOp::NOR)
			xNOT(xRegister64(EEREC_D));
	}
}

//// AND
static void recAND_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] & g_cpuConstRegs[_Rt_].UD[0];
}

static void recAND_consts(int info)
{
	recLogicalOp_constv(LogicalOp::AND, info, _Rs_, _Rt_, (info & PROCESS_EE_T) ? EEREC_T : -1);
}

static void recAND_constt(int info)
{
	recLogicalOp_constv(LogicalOp::AND, info, _Rt_, _Rs_, (info & PROCESS_EE_S) ? EEREC_S : -1);
}

static void recAND_(int info)
{
	recLogicalOp(LogicalOp::AND, info);
}

EERECOMPILE_CODERC0(AND, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// OR
static void recOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] | g_cpuConstRegs[_Rt_].UD[0];
}

static void recOR_consts(int info)
{
	recLogicalOp_constv(LogicalOp::OR, info, _Rs_, _Rt_, (info & PROCESS_EE_T) ? EEREC_T : -1);
}

static void recOR_constt(int info)
{
	recLogicalOp_constv(LogicalOp::OR, info, _Rt_, _Rs_, (info & PROCESS_EE_S) ? EEREC_S : -1);
}

static void recOR_(int info)
{
	recLogicalOp(LogicalOp::OR, info);
}

EERECOMPILE_CODERC0(OR, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// XOR
static void recXOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] ^ g_cpuConstRegs[_Rt_].UD[0];
}

static void recXOR_consts(int info)
{
	recLogicalOp_constv(LogicalOp::XOR, info, _Rs_, _Rt_, (info & PROCESS_EE_T) ? EEREC_T : -1);
}

static void recXOR_constt(int info)
{
	recLogicalOp_constv(LogicalOp::XOR, info, _Rt_, _Rs_, (info & PROCESS_EE_S) ? EEREC_S : -1);
}

static void recXOR_(int info)
{
	recLogicalOp(LogicalOp::XOR, info);
}

EERECOMPILE_CODERC0(XOR, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// NOR
static void recNOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = ~(g_cpuConstRegs[_Rs_].UD[0] | g_cpuConstRegs[_Rt_].UD[0]);
}

static void recNOR_consts(int info)
{
	recLogicalOp_constv(LogicalOp::NOR, info, _Rs_, _Rt_, (info & PROCESS_EE_T) ? EEREC_T : -1);
}

static void recNOR_constt(int info)
{
	recLogicalOp_constv(LogicalOp::NOR, info, _Rt_, _Rs_, (info & PROCESS_EE_S) ? EEREC_S : -1);
}

static void recNOR_(int info)
{
	recLogicalOp(LogicalOp::NOR, info);
}

EERECOMPILE_CODERC0(NOR, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// SLT - test with silent hill, lemans
static void recSLT_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].SD[0] < g_cpuConstRegs[_Rt_].SD[0];
}

static void recSLTs_const(int info, int sign, int st)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 cval = g_cpuConstRegs[st ? _Rt_ : _Rs_].SD[0];

	const xImpl_Set& SET = st ? (sign ? xSETL : xSETB) : (sign ? xSETG : xSETA);

	// If Rd == Rs or Rt, we can't xor it before it's used.
	// So, allocate a temporary register first, and then reallocate it to Rd.
	const xRegister32 dreg((_Rd_ == (st ? _Rs_ : _Rt_)) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_D);
	const int regs = st ? ((info & PROCESS_EE_S) ? EEREC_S : -1) : ((info & PROCESS_EE_T) ? EEREC_T : -1);
	xXOR(dreg, dreg);

	if (regs >= 0)
		xImm64Op(xCMP, xRegister64(regs), rcx, cval);
	else
		xImm64Op(xCMP, ptr64[&cpuRegs.GPR.r[st ? _Rs_ : _Rt_].UD[0]], rcx, cval);
	SET(xRegister8(dreg));

	if (dreg.GetId() != EEREC_D)
	{
		std::swap(x86regs[dreg.GetId()], x86regs[EEREC_D]);
		_freeX86reg(EEREC_D);
	}
}

static void recSLTs_(int info, int sign)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const xImpl_Set& SET = sign ? xSETL : xSETB;

	// need to keep Rs/Rt around.
	const xRegister32 dreg((_Rd_ == _Rt_ || _Rd_ == _Rs_) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_D);

	// force Rs into a register, may as well cache it since we're loading anyway.
	const int regs = (info & PROCESS_EE_S) ? EEREC_S : _allocX86reg(X86TYPE_GPR, _Rs_, MODE_READ);

	xXOR(dreg, dreg);
	if (info & PROCESS_EE_T)
		xCMP(xRegister64(regs), xRegister64(EEREC_T));
	else
		xCMP(xRegister64(regs), ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]]);

	SET(xRegister8(dreg));

	if (dreg.GetId() != EEREC_D)
	{
		std::swap(x86regs[dreg.GetId()], x86regs[EEREC_D]);
		_freeX86reg(EEREC_D);
	}
}

static void recSLT_consts(int info)
{
	recSLTs_const(info, 1, 0);
}

static void recSLT_constt(int info)
{
	recSLTs_const(info, 1, 1);
}

static void recSLT_(int info)
{
	recSLTs_(info, 1);
}

EERECOMPILE_CODERC0(SLT, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_NORENAME);

// SLTU - test with silent hill, lemans
static void recSLTU_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] < g_cpuConstRegs[_Rt_].UD[0];
}

static void recSLTU_consts(int info)
{
	recSLTs_const(info, 0, 0);
}

static void recSLTU_constt(int info)
{
	recSLTs_const(info, 0, 1);
}

static void recSLTU_(int info)
{
	recSLTs_(info, 0);
}

EERECOMPILE_CODERC0(SLTU, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_NORENAME);

#endif

} // namespace R5900::Dynarec::OpcodeImpl
