// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE Move Instruction Codegen — memory-based
// All operands via cpuRegs memory.

#include "arm64/iR5900-arm64.h"

namespace a64 = vixl::aarch64;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

namespace Interp = R5900::Interpreter::OpcodeImpl;

#ifdef FORCE_INTERP_MOVE
REC_FUNC(LUI);
REC_FUNC(MFLO);
REC_FUNC(MFHI);
REC_FUNC(MTLO);
REC_FUNC(MTHI);
REC_FUNC(MFLO1);
REC_FUNC(MFHI1);
REC_FUNC(MTLO1);
REC_FUNC(MTHI1);
REC_FUNC(MOVZ);
REC_FUNC(MOVN);
#else

//// LUI — rt = imm16 << 16 (sign-extended to 64 bits)
void recLUI()
{
	if (!_Rt_) return;

	_deleteEEreg(_Rt_, 0);

	if (EE_CONST_PROP)
	{
		g_cpuConstRegs[_Rt_].SD[0] = s64(s32((u32)_ImmU_ << 16));
		GPR_SET_CONST(_Rt_);
	}
	else
	{
		GPR_DEL_CONST(_Rt_);
		armAsm->Mov(RXSCRATCH, s64(s32((u32)_ImmU_ << 16)));
		armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rt_].UD[0]);
	}
}

//// MFLO / MFHI — rd = LO/HI (memory to memory)
void recMFLO()
{
	if (!_Rd_) return;

	_deleteEEreg(_Rd_, 0);
	GPR_DEL_CONST(_Rd_);
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.LO.UD[0]);
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
}

void recMFHI()
{
	if (!_Rd_) return;

	_deleteEEreg(_Rd_, 0);
	GPR_DEL_CONST(_Rd_);
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.HI.UD[0]);
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
}

//// MTLO / MTHI — LO/HI = rs (memory to memory)
void recMTLO()
{
	if (GPR_IS_CONST1(_Rs_))
	{
		armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rs_].SD[0]);
		armStoreEERegPtr(RXSCRATCH, &cpuRegs.LO.UD[0]);
	}
	else
	{
		_deleteEEreg(_Rs_, 1);
		armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rs_].UD[0]);
		armStoreEERegPtr(RXSCRATCH, &cpuRegs.LO.UD[0]);
	}
}

void recMTHI()
{
	if (GPR_IS_CONST1(_Rs_))
	{
		armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rs_].SD[0]);
		armStoreEERegPtr(RXSCRATCH, &cpuRegs.HI.UD[0]);
	}
	else
	{
		_deleteEEreg(_Rs_, 1);
		armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rs_].UD[0]);
		armStoreEERegPtr(RXSCRATCH, &cpuRegs.HI.UD[0]);
	}
}

//// MFLO1/MFHI1 — rd = LO1/HI1 (upper 64 bits of LO/HI)
void recMFLO1()
{
	if (!_Rd_) return;

	_deleteEEreg(_Rd_, 0);
	GPR_DEL_CONST(_Rd_);
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.LO.UD[1]);
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
}

void recMFHI1()
{
	if (!_Rd_) return;

	_deleteEEreg(_Rd_, 0);
	GPR_DEL_CONST(_Rd_);
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.HI.UD[1]);
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
}

//// MTLO1/MTHI1 — LO1/HI1 = rs
void recMTLO1()
{
	if (GPR_IS_CONST1(_Rs_))
	{
		armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rs_].SD[0]);
		armStoreEERegPtr(RXSCRATCH, &cpuRegs.LO.UD[1]);
	}
	else
	{
		_deleteEEreg(_Rs_, 1);
		armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rs_].UD[0]);
		armStoreEERegPtr(RXSCRATCH, &cpuRegs.LO.UD[1]);
	}
}

void recMTHI1()
{
	if (GPR_IS_CONST1(_Rs_))
	{
		armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rs_].SD[0]);
		armStoreEERegPtr(RXSCRATCH, &cpuRegs.HI.UD[1]);
	}
	else
	{
		_deleteEEreg(_Rs_, 1);
		armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rs_].UD[0]);
		armStoreEERegPtr(RXSCRATCH, &cpuRegs.HI.UD[1]);
	}
}

//// MOVZ — if (rt == 0) then rd = rs
// Memory-based: all loads/stores via cpuRegs

static void recMOVZtemp_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0];
}

static void recMOVZtemp_consts(int info)
{
	// S is const — load T from memory, compare, conditionally store
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rt_].UD[0]);
	armAsm->Cmp(RXSCRATCH, 0);

	armAsm->Mov(RXARG1, g_cpuConstRegs[_Rs_].SD[0]);
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
	armAsm->Csel(RXSCRATCH, RXARG1, RXSCRATCH, a64::eq);
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
}

static void recMOVZtemp_constt(int info)
{
	// T is constant and zero (checked in wrapper) — unconditional move
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rs_].UD[0]);
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
}

static void recMOVZtemp_(int info)
{
	// Load T for comparison
	armLoadEERegPtr(RXARG1, &cpuRegs.GPR.r[_Rt_].UD[0]);
	armAsm->Cmp(RXARG1, 0);

	// Load S
	armLoadEERegPtr(RXARG1, &cpuRegs.GPR.r[_Rs_].UD[0]);

	// Load current D, conditional select, store back
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
	armAsm->Csel(RXSCRATCH, RXARG1, RXSCRATCH, a64::eq);
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
}

static EERECOMPILE_CODERC0_MEM(MOVZtemp, XMMINFO_READS | XMMINFO_READT | XMMINFO_READD | XMMINFO_WRITED | XMMINFO_NORENAME);

void recMOVZ()
{
	if (_Rs_ == _Rd_)
		return;

	if (GPR_IS_CONST1(_Rt_) && g_cpuConstRegs[_Rt_].UD[0] != 0)
		return;

	recMOVZtemp();
}

//// MOVN — if (rt != 0) then rd = rs

static void recMOVNtemp_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0];
}

static void recMOVNtemp_consts(int info)
{
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rt_].UD[0]);
	armAsm->Cmp(RXSCRATCH, 0);

	armAsm->Mov(RXARG1, g_cpuConstRegs[_Rs_].SD[0]);
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
	armAsm->Csel(RXSCRATCH, RXARG1, RXSCRATCH, a64::ne);
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
}

static void recMOVNtemp_constt(int info)
{
	// T is constant and non-zero (checked in wrapper) — unconditional move
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rs_].UD[0]);
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
}

static void recMOVNtemp_(int info)
{
	armLoadEERegPtr(RXARG1, &cpuRegs.GPR.r[_Rt_].UD[0]);
	armAsm->Cmp(RXARG1, 0);

	armLoadEERegPtr(RXARG1, &cpuRegs.GPR.r[_Rs_].UD[0]);

	armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
	armAsm->Csel(RXSCRATCH, RXARG1, RXSCRATCH, a64::ne);
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
}

static EERECOMPILE_CODERC0_MEM(MOVNtemp, XMMINFO_READS | XMMINFO_READT | XMMINFO_READD | XMMINFO_WRITED | XMMINFO_NORENAME);

void recMOVN()
{
	if (_Rs_ == _Rd_)
		return;

	if (GPR_IS_CONST1(_Rt_) && g_cpuConstRegs[_Rt_].UD[0] == 0)
		return;

	recMOVNtemp();
}

#endif // !FORCE_INTERP_MOVE

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
