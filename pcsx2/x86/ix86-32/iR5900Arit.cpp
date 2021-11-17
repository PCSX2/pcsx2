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
#include "R5900OpcodeTables.h"
#include "iR5900.h"

using namespace x86Emitter;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/

// TODO: overflow checks

#ifndef ARITHMETIC_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC_DEL(ADD,   _Rd_);
REC_FUNC_DEL(ADDU,  _Rd_);
REC_FUNC_DEL(DADD,  _Rd_);
REC_FUNC_DEL(DADDU, _Rd_);
REC_FUNC_DEL(SUB,   _Rd_);
REC_FUNC_DEL(SUBU,  _Rd_);
REC_FUNC_DEL(DSUB,  _Rd_);
REC_FUNC_DEL(DSUBU, _Rd_);
REC_FUNC_DEL(AND,   _Rd_);
REC_FUNC_DEL(OR,    _Rd_);
REC_FUNC_DEL(XOR,   _Rd_);
REC_FUNC_DEL(NOR,   _Rd_);
REC_FUNC_DEL(SLT,   _Rd_);
REC_FUNC_DEL(SLTU,  _Rd_);

#else

//// ADD
void recADD_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = g_cpuConstRegs[_Rs_].SL[0] + g_cpuConstRegs[_Rt_].SL[0];
}

void recADD_constv(int info, int creg, u32 vreg)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	s32 cval = g_cpuConstRegs[creg].SL[0];

	xMOV(eax, ptr32[&cpuRegs.GPR.r[vreg].SL[0]]);
	if (cval)
		xADD(eax, cval);
	eeSignExtendTo(_Rd_, _Rd_ == vreg && !cval);
}

// s is constant
void recADD_consts(int info)
{
	recADD_constv(info, _Rs_, _Rt_);
}

// t is constant
void recADD_constt(int info)
{
	recADD_constv(info, _Rt_, _Rs_);
}

// nothing is constant
void recADD_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xMOV(eax, ptr32[&cpuRegs.GPR.r[_Rs_].SL[0]]);
	if (_Rs_ == _Rt_)
		xADD(eax, eax);
	else
		xADD(eax, ptr32[&cpuRegs.GPR.r[_Rt_].SL[0]]);
	eeSignExtendTo(_Rd_);
}

EERECOMPILE_CODE0(ADD, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

//// ADDU
void recADDU(void)
{
	recADD();
}

//// DADD
void recDADD_const(void)
{
	g_cpuConstRegs[_Rd_].SD[0] = g_cpuConstRegs[_Rs_].SD[0] + g_cpuConstRegs[_Rt_].SD[0];
}

void recDADD_constv(int info, int creg, u32 vreg)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	GPR_reg64 cval = g_cpuConstRegs[creg];

#ifdef __M_X86_64
	if (_Rd_ == vreg)
	{
		if (!cval.SD[0])
			return; // no-op
		xImm64Op(xADD, ptr64[&cpuRegs.GPR.r[_Rd_].SD[0]], rax, cval.SD[0]);
	}
	else
	{
		if (cval.SD[0])
		{
			xMOV64(rax, cval.SD[0]);
			xADD(rax, ptr64[&cpuRegs.GPR.r[vreg].SD[0]]);
		}
		else
		{
			xMOV(rax, ptr64[&cpuRegs.GPR.r[vreg].SD[0]]);
		}
		xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].SD[0]], rax);
	}
#else
	if (_Rd_ == vreg)
	{
		if (!cval.SD[0])
			return; // no-op
		xADD(ptr32[&cpuRegs.GPR.r[_Rd_].SL[0]], cval.SL[0]);
		xADC(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]], cval.SL[1]);
	}
	else
	{
		xMOV(eax, ptr32[&cpuRegs.GPR.r[vreg].SL[0]]);
		xMOV(edx, ptr32[&cpuRegs.GPR.r[vreg].SL[1]]);
		if (cval.SD[0])
		{
			xADD(eax, cval.SL[0]);
			xADC(edx, cval.SL[1]);
		}
		xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[0]], eax);
		xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]], edx);
	}
#endif
}

void recDADD_consts(int info)
{
	recDADD_constv(info, _Rs_, _Rt_);
}

void recDADD_constt(int info)
{
	recDADD_constv(info, _Rt_, _Rs_);
}

void recDADD_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	u32 rs = _Rs_, rt = _Rt_;
	if (_Rd_ == _Rt_)
		rs = _Rt_, rt = _Rs_;

#ifdef __M_X86_64
	if (_Rd_ == _Rs_ && _Rs_ == _Rt_)
	{
		xSHL(ptr64[&cpuRegs.GPR.r[_Rd_].SD[0]], 1);
		return;
	}

	xMOV(rax, ptr64[&cpuRegs.GPR.r[rt].SD[0]]);

	if (_Rd_ == rs)
	{
		xADD(ptr64[&cpuRegs.GPR.r[_Rd_].SD[0]], rax);
		return;
	}
	else if (rs == rt)
	{
		xADD(rax, rax);
	}
	else
	{
		xADD(rax, ptr32[&cpuRegs.GPR.r[rs].SD[0]]);
	}

	xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].SD[0]], rax);
#else
	xMOV(eax, ptr32[&cpuRegs.GPR.r[rt].SL[0]]);

	if (_Rd_ == _Rs_ && _Rs_ == _Rt_)
	{
		xSHLD(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]], eax, 1);
		xSHL(ptr32[&cpuRegs.GPR.r[_Rd_].SL[0]], 1);
		return;
	}

	xMOV(edx, ptr32[&cpuRegs.GPR.r[rt].SL[1]]);

	if (_Rd_ == rs)
	{
		xADD(ptr32[&cpuRegs.GPR.r[_Rd_].SL[0]], eax);
		xADC(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]], edx);
		return;
	}
	else if (rs == rt)
	{
		xADD(eax, eax);
		xADC(edx, edx);
	}
	else
	{
		xADD(eax, ptr32[&cpuRegs.GPR.r[rs].SL[0]]);
		xADC(edx, ptr32[&cpuRegs.GPR.r[rs].SL[1]]);
	}

	xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[0]], eax);
	xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]], edx);
#endif
}

EERECOMPILE_CODE0(DADD, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

//// DADDU
void recDADDU(void)
{
	recDADD();
}

//// SUB

void recSUB_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = g_cpuConstRegs[_Rs_].SL[0] - g_cpuConstRegs[_Rt_].SL[0];
}

void recSUB_consts(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	s32 sval = g_cpuConstRegs[_Rs_].SL[0];

	xMOV(eax, sval);
	xSUB(eax, ptr32[&cpuRegs.GPR.r[_Rt_].SL[0]]);
	eeSignExtendTo(_Rd_);
}

void recSUB_constt(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	s32 tval = g_cpuConstRegs[_Rt_].SL[0];

	xMOV(eax, ptr32[&cpuRegs.GPR.r[_Rs_].SL[0]]);
	if (tval)
		xSUB(eax, tval);
	eeSignExtendTo(_Rd_, _Rd_ == _Rs_ && !tval);
}

void recSUB_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	if (_Rs_ == _Rt_)
	{
#ifdef __M_X86_64
		xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].SD[0]], 0);
#else
		xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[0]], 0);
		xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]], 0);
#endif
		return;
	}

	xMOV(eax, ptr32[&cpuRegs.GPR.r[_Rs_].SL[0]]);
	xSUB(eax, ptr32[&cpuRegs.GPR.r[_Rt_].SL[0]]);
	eeSignExtendTo(_Rd_);
}

EERECOMPILE_CODE0(SUB, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// SUBU
void recSUBU(void)
{
	recSUB();
}

//// DSUB
void recDSUB_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = g_cpuConstRegs[_Rs_].SD[0] - g_cpuConstRegs[_Rt_].SD[0];
}

void recDSUB_consts(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	GPR_reg64 sval = g_cpuConstRegs[_Rs_];

#ifdef __M_X86_64
	if (!sval.SD[0] && _Rd_ == _Rt_)
	{
		xNEG(ptr64[&cpuRegs.GPR.r[_Rd_].SD[0]]);
		return;
	}
	else
	{
		xMOV64(rax, sval.SD[0]);
	}

	xSUB(rax, ptr32[&cpuRegs.GPR.r[_Rt_].SD[0]]);
	xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].SL[0]], rax);
#else
	if (!sval.SD[0] && _Rd_ == _Rt_)
	{
		/* To understand this 64-bit negate, consider that a negate in 2's complement
		 * is a NOT then an ADD 1.  The upper word should only have the NOT stage unless
		 * the ADD overflows.  The ADD only overflows if the lower word is 0.
		 * Incrementing before a NEG is the same as a NOT and the carry flag is set for
		 * a non-zero lower word.
		 */
		xNEG(ptr32[&cpuRegs.GPR.r[_Rd_].SL[0]]);
		xADC(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]], 0);
		xNEG(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]]);
		return;
	}
	else
	{
		xMOV(eax, sval.SL[0]);
		xMOV(edx, sval.SL[1]);
	}

	xSUB(eax, ptr32[&cpuRegs.GPR.r[_Rt_].SL[0]]);
	xSBB(edx, ptr32[&cpuRegs.GPR.r[_Rt_].SL[1]]);
	xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[0]], eax);
	xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]], edx);
#endif
}

void recDSUB_constt(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	GPR_reg64 tval = g_cpuConstRegs[_Rt_];

#ifdef __M_X86_64
	if (_Rd_ == _Rs_)
	{
		xImm64Op(xSUB, ptr64[&cpuRegs.GPR.r[_Rd_].SD[0]], rax, tval.SD[0]);
	}
	else
	{
		xMOV(rax, ptr64[&cpuRegs.GPR.r[_Rs_].SD[0]]);
		if (tval.SD[0])
		{
			xImm64Op(xSUB, rax, rdx, tval.SD[0]);
		}
		xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].SL[0]], rax);
	}
#else
	if (_Rd_ == _Rs_)
	{
		xSUB(ptr32[&cpuRegs.GPR.r[_Rd_].SL[0]], tval.SL[0]);
		xSBB(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]], tval.SL[1]);
	}
	else
	{
		xMOV(eax, ptr32[&cpuRegs.GPR.r[_Rs_].SL[0]]);
		xMOV(edx, ptr32[&cpuRegs.GPR.r[_Rs_].SL[1]]);
		if (tval.SD[0])
		{
			xSUB(eax, tval.SL[0]);
			xSBB(edx, tval.SL[1]);
		}
		xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[0]], eax);
		xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]], edx);
	}
#endif
}

void recDSUB_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

#ifdef __M_X86_64
	if (_Rs_ == _Rt_)
	{
		xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].SD[0]], 0);
	}
	else if (_Rd_ == _Rs_)
	{
		xMOV(rax, ptr64[&cpuRegs.GPR.r[_Rt_].SD[0]]);
		xSUB(ptr64[&cpuRegs.GPR.r[_Rd_].SD[0]], rax);
	}
	else
	{
		xMOV(rax, ptr64[&cpuRegs.GPR.r[_Rs_].SD[0]]);
		xSUB(rax, ptr64[&cpuRegs.GPR.r[_Rt_].SD[0]]);
		xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].SL[0]], rax);
	}
#else
	if (_Rs_ == _Rt_)
	{
		xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[0]], 0);
		xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]], 0);
	}
	else if (_Rd_ == _Rs_)
	{
		xMOV(eax, ptr32[&cpuRegs.GPR.r[_Rt_].SL[0]]);
		xMOV(edx, ptr32[&cpuRegs.GPR.r[_Rt_].SL[1]]);
		xSUB(ptr32[&cpuRegs.GPR.r[_Rd_].SL[0]], eax);
		xSBB(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]], edx);
	}
	else
	{
		xMOV(eax, ptr32[&cpuRegs.GPR.r[_Rs_].SL[0]]);
		xMOV(edx, ptr32[&cpuRegs.GPR.r[_Rs_].SL[1]]);
		xSUB(eax, ptr32[&cpuRegs.GPR.r[_Rt_].SL[0]]);
		xSBB(edx, ptr32[&cpuRegs.GPR.r[_Rt_].SL[1]]);
		xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[0]], eax);
		xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].SL[1]], edx);
	}
#endif
}

EERECOMPILE_CODE0(DSUB, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

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

static void recLogicalOp_constv(LogicalOp op, int info, int creg, u32 vreg)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xImpl_G1Logic bad{};
	const xImpl_G1Logic& xOP = op == LogicalOp::AND ? xAND
	                         : op == LogicalOp::OR  ? xOR
	                         : op == LogicalOp::XOR ? xXOR
	                         : op == LogicalOp::NOR ? xOR : bad;
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
#ifdef __M_X86_64
	if (hasFixed && cval.SD[0] == fixedInput)
	{
		xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], fixedOutput);
	}
	else if (_Rd_ == vreg)
	{
		if (cval.SD[0] != identityInput)
			xImm64Op(xOP, ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], rax, cval.UD[0]);
		if (op == LogicalOp::NOR)
			xNOT(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]]);
	}
	else
	{
		if (cval.SD[0] != identityInput)
		{
			xMOV64(rax, cval.SD[0]);
			xOP(rax, ptr32[&cpuRegs.GPR.r[vreg].UD[0]]);
		}
		else
		{
			xMOV(rax, ptr32[&cpuRegs.GPR.r[vreg].UD[0]]);
		}
		if (op == LogicalOp::NOR)
			xNOT(rax);
		xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
	}
#else
	for (int i = 0; i < 2; i++)
	{
		if (hasFixed && cval.SL[i] == (s32)fixedInput)
		{
			xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].UL[i]], (s32)fixedOutput);
		}
		else if (_Rd_ == vreg)
		{
			if (cval.SL[i] != identityInput)
				xOP(ptr32[&cpuRegs.GPR.r[_Rd_].UL[i]], cval.UL[i]);
			if (op == LogicalOp::NOR)
				xNOT(ptr32[&cpuRegs.GPR.r[_Rd_].UL[i]]);
		}
		else
		{
			xMOV(eax, ptr32[&cpuRegs.GPR.r[vreg].UL[i]]);
			if (cval.SL[i] != identityInput)
				xOP(eax, cval.UL[i]);
			if (op == LogicalOp::NOR)
				xNOT(eax);
			xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].UL[i]], eax);
		}
	}
#endif
}

static void recLogicalOp(LogicalOp op, int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xImpl_G1Logic bad{};
	const xImpl_G1Logic& xOP = op == LogicalOp::AND ? xAND
	                         : op == LogicalOp::OR  ? xOR
	                         : op == LogicalOp::XOR ? xXOR
	                         : op == LogicalOp::NOR ? xOR : bad;
	pxAssert(&xOP != &bad);

	u32 rs = _Rs_, rt = _Rt_;
	if (_Rd_ == _Rt_)
		rs = _Rt_, rt = _Rs_;

#ifdef __M_X86_64
	if (op == LogicalOp::XOR && rs == rt)
	{
		xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], 0);
	}
	else if (_Rd_ == rs)
	{
		if (rs != rt)
		{
			xMOV(rax, ptr64[&cpuRegs.GPR.r[rt].UD[0]]);
			xOP(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
		}
		if (op == LogicalOp::NOR)
			xNOT(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]]);
	}
	else
	{
		xMOV(rax, ptr64[&cpuRegs.GPR.r[rs].UD[0]]);
		if (rs != rt)
			xOP(rax, ptr64[&cpuRegs.GPR.r[rt].UD[0]]);
		if (op == LogicalOp::NOR)
			xNOT(rax);
		xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
	}
#else
	for (int i = 0; i < 2; i++)
	{
		if (op == LogicalOp::XOR && rs == rt)
		{
			xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].UL[i]], 0);
		}
		else if (_Rd_ == rs)
		{
			if (rs != rt)
			{
				xMOV(eax, ptr32[&cpuRegs.GPR.r[rt].UL[i]]);
				xOP(ptr32[&cpuRegs.GPR.r[_Rd_].UL[i]], eax);
			}
			if (op == LogicalOp::NOR)
				xNOT(ptr32[&cpuRegs.GPR.r[_Rd_].UL[i]]);
		}
		else
		{
			xMOV(eax, ptr32[&cpuRegs.GPR.r[rs].UL[i]]);
			if (rs != rt)
				xOP(eax, ptr32[&cpuRegs.GPR.r[rt].UL[i]]);
			if (op == LogicalOp::NOR)
				xNOT(eax);
			xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].UL[i]], eax);
		}
	}
#endif
}

//// AND
void recAND_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] & g_cpuConstRegs[_Rt_].UD[0];
}

void recAND_consts(int info)
{
	recLogicalOp_constv(LogicalOp::AND, info, _Rs_, _Rt_);
}

void recAND_constt(int info)
{
	recLogicalOp_constv(LogicalOp::AND, info, _Rt_, _Rs_);
}

void recAND_(int info)
{
	recLogicalOp(LogicalOp::AND, info);
}

EERECOMPILE_CODE0(AND, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// OR
void recOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] | g_cpuConstRegs[_Rt_].UD[0];
}

void recOR_consts(int info)
{
	recLogicalOp_constv(LogicalOp::OR, info, _Rs_, _Rt_);
}

void recOR_constt(int info)
{
	recLogicalOp_constv(LogicalOp::OR, info, _Rt_, _Rs_);
}

void recOR_(int info)
{
	recLogicalOp(LogicalOp::OR, info);
}

EERECOMPILE_CODE0(OR, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// XOR
void recXOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] ^ g_cpuConstRegs[_Rt_].UD[0];
}

void recXOR_consts(int info)
{
	recLogicalOp_constv(LogicalOp::XOR, info, _Rs_, _Rt_);
}

void recXOR_constt(int info)
{
	recLogicalOp_constv(LogicalOp::XOR, info, _Rt_, _Rs_);
}

void recXOR_(int info)
{
	recLogicalOp(LogicalOp::XOR, info);
}

EERECOMPILE_CODE0(XOR, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// NOR
void recNOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = ~(g_cpuConstRegs[_Rs_].UD[0] | g_cpuConstRegs[_Rt_].UD[0]);
}

void recNOR_consts(int info)
{
	recLogicalOp_constv(LogicalOp::NOR, info, _Rs_, _Rt_);
}

void recNOR_constt(int info)
{
	recLogicalOp_constv(LogicalOp::NOR, info, _Rt_, _Rs_);
}

void recNOR_(int info)
{
	recLogicalOp(LogicalOp::NOR, info);
}

EERECOMPILE_CODE0(NOR, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// SLT - test with silent hill, lemans
void recSLT_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].SD[0] < g_cpuConstRegs[_Rt_].SD[0];
}

void recSLTs_const(int info, int sign, int st)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	GPR_reg64 cval = g_cpuConstRegs[st ? _Rt_ : _Rs_];

#ifdef __M_X86_64
	const xImpl_Set& SET = st ? (sign ? xSETL : xSETB) : (sign ? xSETG : xSETA);

	xXOR(eax, eax);
	xImm64Op(xCMP, ptr64[&cpuRegs.GPR.r[st ? _Rs_ : _Rt_].UD[0]], rdx, cval.UD[0]);
	SET(al);
	xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
#else
	xMOV(eax, 1);

	xCMP(ptr32[&cpuRegs.GPR.r[st ? _Rs_ : _Rt_].UL[1]], cval.UL[1]);
	xForwardJump8 pass1(st ? (sign ? Jcc_Less : Jcc_Below) : (sign ? Jcc_Greater : Jcc_Above));
	xForwardJump8 fail(st ? (sign ? Jcc_Greater : Jcc_Above) : (sign ? Jcc_Less : Jcc_Below));
	{
		xCMP(ptr32[&cpuRegs.GPR.r[st ? _Rs_ : _Rt_].UL[0]], cval.UL[0]);
		xForwardJump8 pass2(st ? Jcc_Below : Jcc_Above);

		fail.SetTarget();
		xMOV(eax, 0);
		pass2.SetTarget();
	}
	pass1.SetTarget();

	xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].UL[0]], eax);
	xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].UL[1]], 0);
#endif
}

void recSLTs_(int info, int sign)
{
	pxAssert(!(info & PROCESS_EE_XMM));
#ifdef __M_X86_64
	const xImpl_Set& SET = sign ? xSETL : xSETB;

	xXOR(eax, eax);
	xMOV(rdx, ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]]);
	xCMP(rdx, ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	SET(al);
	xMOV(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
#else
	xMOV(eax, 1);

	xMOV(edx, ptr32[&cpuRegs.GPR.r[_Rs_].UL[1]]);
	xCMP(edx, ptr32[&cpuRegs.GPR.r[_Rt_].UL[1]]);
	xForwardJump8 pass1(sign ? Jcc_Less : Jcc_Below);
	xForwardJump8 fail(sign ? Jcc_Greater : Jcc_Above);
	{
		xMOV(edx, ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]]);
		xCMP(edx, ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
		xForwardJump8 pass2(Jcc_Below);

		fail.SetTarget();
		xMOV(eax, 0);
		pass2.SetTarget();
	}
	pass1.SetTarget();

	xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].UL[0]], eax);
	xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].UL[1]], 0);
#endif
}

void recSLT_consts(int info)
{
	recSLTs_const(info, 1, 0);
}

void recSLT_constt(int info)
{
	recSLTs_const(info, 1, 1);
}

void recSLT_(int info)
{
	recSLTs_(info, 1);
}

EERECOMPILE_CODE0(SLT, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

// SLTU - test with silent hill, lemans
void recSLTU_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] < g_cpuConstRegs[_Rt_].UD[0];
}

void recSLTU_consts(int info)
{
	recSLTs_const(info, 0, 0);
}

void recSLTU_constt(int info)
{
	recSLTs_const(info, 0, 1);
}

void recSLTU_(int info)
{
	recSLTs_(info, 0);
}

EERECOMPILE_CODE0(SLTU, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

#endif

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
