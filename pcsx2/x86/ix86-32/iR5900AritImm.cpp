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
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/

#ifndef ARITHMETICIMM_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC_DEL(ADDI,   _Rt_);
REC_FUNC_DEL(ADDIU,  _Rt_);
REC_FUNC_DEL(DADDI,  _Rt_);
REC_FUNC_DEL(DADDIU, _Rt_);
REC_FUNC_DEL(ANDI,   _Rt_);
REC_FUNC_DEL(ORI,    _Rt_);
REC_FUNC_DEL(XORI,   _Rt_);

REC_FUNC_DEL(SLTI,   _Rt_);
REC_FUNC_DEL(SLTIU,  _Rt_);

#else

//// ADDI
void recADDI_const(void)
{
	g_cpuConstRegs[_Rt_].SD[0] = (s64)(g_cpuConstRegs[_Rs_].SL[0] + (s32)_Imm_);
}

void recADDI_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	if (_Rt_ == _Rs_)
	{
		// must perform the ADD unconditionally, to maintain flags status:
		xADD(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]], _Imm_);
		_signExtendSFtoM((uptr)&cpuRegs.GPR.r[_Rt_].UL[1]);
	}
	else
	{
		xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);

		if (_Imm_ != 0)
			xADD(eax, _Imm_);

		eeSignExtendTo(_Rt_);
	}
}

EERECOMPILE_CODEX(eeRecompileCode1, ADDI);

////////////////////////////////////////////////////
void recADDIU()
{
	recADDI();
}

////////////////////////////////////////////////////
void recDADDI_const()
{
	g_cpuConstRegs[_Rt_].SD[0] = g_cpuConstRegs[_Rs_].SD[0] + (s64)_Imm_;
}

void recDADDI_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

#ifdef __M_X86_64
	if (_Rt_ == _Rs_)
	{
		xADD(ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]], _Imm_);
	}
	else
	{
		xMOV(rax, ptr[&cpuRegs.GPR.r[_Rs_].UD[0]]);

		if (_Imm_ != 0)
		{
			xADD(rax, _Imm_);
		}

		xMOV(ptr[&cpuRegs.GPR.r[_Rt_].UD[0]], rax);
	}
#else
	if (_Rt_ == _Rs_)
	{
		xADD(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]], _Imm_);
		xADC(ptr32[&cpuRegs.GPR.r[_Rt_].UL[1]], _Imm_ < 0 ? 0xffffffff : 0);
	}
	else
	{
		xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);

		xMOV(edx, ptr[&cpuRegs.GPR.r[_Rs_].UL[1]]);

		if (_Imm_ != 0)
		{
			xADD(eax, _Imm_);
			xADC(edx, _Imm_ < 0 ? 0xffffffff : 0);
		}

		xMOV(ptr[&cpuRegs.GPR.r[_Rt_].UL[0]], eax);

		xMOV(ptr[&cpuRegs.GPR.r[_Rt_].UL[1]], edx);
	}
#endif
}

EERECOMPILE_CODEX(eeRecompileCode1, DADDI);

//// DADDIU
void recDADDIU()
{
	recDADDI();
}

//// SLTIU
void recSLTIU_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] < (u64)(_Imm_);
}

extern void recSLTmemconstt(int regd, int regs, u32 mem, int sign);
extern u32 s_sltone;

void recSLTIU_(int info)
{
#ifdef __M_X86_64
	xXOR(eax, eax);
	xCMP(ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]], _Imm_);
	xSETB(al);
	xMOV(ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]], rax);
#else
	xMOV(eax, 1);

	xCMP(ptr32[&cpuRegs.GPR.r[_Rs_].UL[1]], _Imm_ >= 0 ? 0 : 0xffffffff);
	j8Ptr[0] = JB8(0);
	j8Ptr[2] = JA8(0);

	xCMP(ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]], (s32)_Imm_);
	j8Ptr[1] = JB8(0);

	x86SetJ8(j8Ptr[2]);
	xXOR(eax, eax);

	x86SetJ8(j8Ptr[0]);
	x86SetJ8(j8Ptr[1]);

	xMOV(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]], eax);
	xMOV(ptr32[&cpuRegs.GPR.r[_Rt_].UL[1]], 0);
#endif
}

EERECOMPILE_CODEX(eeRecompileCode1, SLTIU);

//// SLTI
void recSLTI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].SD[0] < (s64)(_Imm_);
}

void recSLTI_(int info)
{
	// test silent hill if modding
#ifdef __M_X86_64
	xXOR(eax, eax);
	xCMP(ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]], _Imm_);
	xSETL(al);
	xMOV(ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]], rax);
#else
	xMOV(eax, 1);

	xCMP(ptr32[&cpuRegs.GPR.r[_Rs_].UL[1]], _Imm_ >= 0 ? 0 : 0xffffffff);
	j8Ptr[0] = JL8(0);
	j8Ptr[2] = JG8(0);

	xCMP(ptr32[&cpuRegs.GPR.r[_Rs_].UL[0]], (s32)_Imm_);
	j8Ptr[1] = JB8(0);

	x86SetJ8(j8Ptr[2]);
	xXOR(eax, eax);

	x86SetJ8(j8Ptr[0]);
	x86SetJ8(j8Ptr[1]);

	xMOV(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]], eax);
	xMOV(ptr32[&cpuRegs.GPR.r[_Rt_].UL[1]], 0);
#endif
}

EERECOMPILE_CODEX(eeRecompileCode1, SLTI);

//// ANDI
void recANDI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] & (u64)_ImmU_; // Zero-extended Immediate
}

enum class LogicalOp
{
	AND,
	OR,
	XOR
};

static void recLogicalOpI(int info, LogicalOp op)
{
	xImpl_G1Logic bad{};
	const xImpl_G1Logic& xOP = op == LogicalOp::AND ? xAND
	                         : op == LogicalOp::OR  ? xOR
	                         : op == LogicalOp::XOR ? xXOR : bad;
	pxAssert(&xOP != &bad);

#ifdef __M_X86_64
	if (_ImmU_ != 0)
	{
		if (_Rt_ == _Rs_)
		{
			if (op == LogicalOp::AND)
				xOP(ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]], _ImmU_);
			else
				xOP(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]], _ImmU_);
		}
		else
		{
			xMOV(rax, ptr[&cpuRegs.GPR.r[_Rs_].UD[0]]);
			xOP(rax, _ImmU_);
			xMOV(ptr[&cpuRegs.GPR.r[_Rt_].UD[0]], rax);
		}
	}
	else
	{
		if (op == LogicalOp::AND)
		{
			xMOV(ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]], 0);
		}
		else
		{
			if (_Rt_ != _Rs_)
			{
				xMOV(rax, ptr[&cpuRegs.GPR.r[_Rs_].UD[0]]);
				xMOV(ptr[&cpuRegs.GPR.r[_Rt_].UD[0]], rax);
			}
		}
	}
#else
	if (_ImmU_ != 0)
	{
		if (_Rt_ == _Rs_)
		{
			xOP(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]], _ImmU_);
		}
		else
		{
			xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
			if (op != LogicalOp::AND)
				xMOV(edx, ptr[&cpuRegs.GPR.r[_Rs_].UL[1]]);

			xOP(eax, _ImmU_);

			if (op != LogicalOp::AND)
				xMOV(ptr[&cpuRegs.GPR.r[_Rt_].UL[1]], edx);
			xMOV(ptr[&cpuRegs.GPR.r[_Rt_].UL[0]], eax);
		}

		if (op == LogicalOp::AND)
		{
			xMOV(ptr32[&cpuRegs.GPR.r[_Rt_].UL[1]], 0);
		}
	}
	else
	{
		if (op == LogicalOp::AND)
		{
			xMOV(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]], 0);
			xMOV(ptr32[&cpuRegs.GPR.r[_Rt_].UL[1]], 0);
		}
		else
		{
			if (_Rt_ != _Rs_)
			{
				xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
				xMOV(edx, ptr[&cpuRegs.GPR.r[_Rs_].UL[1]]);
				xMOV(ptr[&cpuRegs.GPR.r[_Rt_].UL[0]], eax);
				xMOV(ptr[&cpuRegs.GPR.r[_Rt_].UL[1]], edx);
			}
		}
	}
#endif
}

void recANDI_(int info)
{
	recLogicalOpI(info, LogicalOp::AND);
}

EERECOMPILE_CODEX(eeRecompileCode1, ANDI);

////////////////////////////////////////////////////
void recORI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] | (u64)_ImmU_; // Zero-extended Immediate
}

void recORI_(int info)
{
	recLogicalOpI(info, LogicalOp::OR);
}

EERECOMPILE_CODEX(eeRecompileCode1, ORI);

////////////////////////////////////////////////////
void recXORI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] ^ (u64)_ImmU_; // Zero-extended Immediate
}

void recXORI_(int info)
{
	recLogicalOpI(info, LogicalOp::XOR);
}

EERECOMPILE_CODEX(eeRecompileCode1, XORI);

#endif

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
