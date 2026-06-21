// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE Multiply/Divide Instruction Codegen — memory-based
// MULT/DIV write to HI:LO registers, optionally Rd.
// ARM64 has native SMULL/UMULL and SDIV/UDIV.
// All operands via cpuRegs memory.

#include "arm64/iR5900-arm64.h"

namespace a64 = vixl::aarch64;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

namespace Interp = R5900::Interpreter::OpcodeImpl;

#ifdef FORCE_INTERP_MULTDIV
REC_FUNC(MULT);
REC_FUNC(MULTU);
REC_FUNC(DIV);
REC_FUNC(DIVU);
#else

// Load Rs/Rt lower 32 bits from memory (or const)
static void loadRs32()
{
	if (GPR_IS_CONST1(_Rs_))
		armAsm->Mov(a64::w1, g_cpuConstRegs[_Rs_].UL[0]);
	else
		armLoadEERegPtr(a64::w1, &cpuRegs.GPR.r[_Rs_].UL[0]);
}

static void loadRt32()
{
	if (GPR_IS_CONST1(_Rt_))
		armAsm->Mov(a64::w2, g_cpuConstRegs[_Rt_].UL[0]);
	else
		armLoadEERegPtr(a64::w2, &cpuRegs.GPR.r[_Rt_].UL[0]);
}

// Write LO and HI from 64-bit result in x0
// lo = lower 32, hi = upper 32, both sign-extended to 64 bits
static void recWritebackHILO(bool upper)
{
	armAsm->Sxtw(RXSCRATCH, a64::w0);
	armAsm->Str(RXSCRATCH, armCpuRegMem(upper ? &cpuRegs.LO.UD[1] : &cpuRegs.LO.UD[0]));

	armAsm->Asr(a64::x0, a64::x0, 32);
	armAsm->Sxtw(RXSCRATCH, a64::w0);
	armAsm->Str(RXSCRATCH, armCpuRegMem(upper ? &cpuRegs.HI.UD[1] : &cpuRegs.HI.UD[0]));
}

// Write Rd from LO (memory-based — no register allocation)
static void recWritebackRd()
{
	if (!_Rd_) return;

	_deleteEEreg(_Rd_, 0);
	GPR_DEL_CONST(_Rd_);
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.LO.UD[0]);
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
}

//// MULT — signed 32-bit multiply, result in HI:LO, optionally Rd
void recMULT()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		s64 result = (s64)(s32)g_cpuConstRegs[_Rs_].UL[0] * (s64)(s32)g_cpuConstRegs[_Rt_].UL[0];

		armAsm->Mov(RXSCRATCH, (s64)(s32)(u32)result);
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[0]));

		armAsm->Mov(RXSCRATCH, (s64)(s32)(u32)(result >> 32));
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.HI.UD[0]));

		if (_Rd_)
		{
			_deleteEEreg(_Rd_, 0);
			g_cpuConstRegs[_Rd_].SD[0] = (s32)(u32)result;
			GPR_SET_CONST(_Rd_);
		}
		return;
	}

	_eeFlushAllDirty();
	loadRs32();
	loadRt32();

	armAsm->Smull(a64::x0, a64::w1, a64::w2);

	recWritebackHILO(false);
	recWritebackRd();
}

//// MULTU — unsigned 32-bit multiply
void recMULTU()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		u64 result = (u64)g_cpuConstRegs[_Rs_].UL[0] * (u64)g_cpuConstRegs[_Rt_].UL[0];

		armAsm->Mov(RXSCRATCH, (s64)(s32)(u32)result);
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[0]));

		armAsm->Mov(RXSCRATCH, (s64)(s32)(u32)(result >> 32));
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.HI.UD[0]));

		if (_Rd_)
		{
			_deleteEEreg(_Rd_, 0);
			g_cpuConstRegs[_Rd_].SD[0] = (s32)(u32)result;
			GPR_SET_CONST(_Rd_);
		}
		return;
	}

	_eeFlushAllDirty();
	loadRs32();
	loadRt32();

	armAsm->Umull(a64::x0, a64::w1, a64::w2);

	recWritebackHILO(false);
	recWritebackRd();
}

//// DIV — signed 32-bit divide. LO = quotient, HI = remainder.
// PS2 div-by-zero: LO = (rs >= 0 ? -1 : 1), HI = rs (sign-extended into 64-bit
// HI/LO). Matches the interpreter and the PS2 hardware spec.
void recDIV()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		s32 rs = g_cpuConstRegs[_Rs_].SL[0];
		s32 rt = g_cpuConstRegs[_Rt_].SL[0];

		s32 lo, hi;
		if (rt == 0)
		{
			lo = (rs >= 0) ? -1 : 1;
			hi = rs;
		}
		else if (rs == (s32)0x80000000 && rt == -1)
		{
			lo = (s32)0x80000000;
			hi = 0;
		}
		else
		{
			lo = rs / rt;
			hi = rs % rt;
		}

		armAsm->Mov(RXSCRATCH, (s64)lo);
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[0]));

		armAsm->Mov(RXSCRATCH, (s64)hi);
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.HI.UD[0]));
		return;
	}

	_eeFlushAllDirty();
	loadRs32();
	loadRt32();

	// Branch on rt == 0 → div-by-zero handler.
	a64::Label divByZero;
	a64::Label done;
	armAsm->Cbz(a64::w2, &divByZero);

	// Normal path: SDIV w0, w1, w2; MSUB w3 = w1 - w0 * w2 (remainder).
	armAsm->Sdiv(a64::w0, a64::w1, a64::w2);
	armAsm->Msub(a64::w3, a64::w0, a64::w2, a64::w1);
	armAsm->B(&done);

	// Div-by-zero: w0 = (rs >= 0 ? -1 : 1), w3 = rs.
	// Cneg w0, w0, lt: if rs < 0, w0 = -(-1) = 1; else w0 = -1.
	armAsm->Bind(&divByZero);
	armAsm->Mov(a64::w0, -1);
	armAsm->Cmp(a64::w1, 0);
	armAsm->Cneg(a64::w0, a64::w0, a64::lt);
	armAsm->Mov(a64::w3, a64::w1);                    // HI = rs

	armAsm->Bind(&done);

	// Store LO = sign_extend(quotient or -1/1)
	armAsm->Sxtw(RXSCRATCH, a64::w0);
	armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[0]));

	// Store HI = sign_extend(remainder or rs)
	armAsm->Sxtw(RXSCRATCH, a64::w3);
	armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.HI.UD[0]));
}

//// DIVU — unsigned 32-bit divide. PS2 div-by-zero: LO = -1 (0xffffffff
//// sign-extended), HI = rs.
void recDIVU()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		u32 rs = g_cpuConstRegs[_Rs_].UL[0];
		u32 rt = g_cpuConstRegs[_Rt_].UL[0];

		s32 lo, hi;
		if (rt == 0)
		{
			lo = -1;
			hi = (s32)rs;
		}
		else
		{
			lo = (s32)(rs / rt);
			hi = (s32)(rs % rt);
		}

		armAsm->Mov(RXSCRATCH, (s64)lo);
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[0]));

		armAsm->Mov(RXSCRATCH, (s64)hi);
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.HI.UD[0]));
		return;
	}

	_eeFlushAllDirty();
	loadRs32();
	loadRt32();

	a64::Label divByZero;
	a64::Label done;
	armAsm->Cbz(a64::w2, &divByZero);

	// Normal path: UDIV w0, w1, w2; MSUB w1 = w1 - w0 * w2 (remainder in-place
	// over Rs scratch; Msub permits rd==ra). On div-by-zero w1 still holds Rs,
	// so the slow path doesn't need a separate Mov w3, w1.
	armAsm->Udiv(a64::w0, a64::w1, a64::w2);
	armAsm->Msub(a64::w1, a64::w0, a64::w2, a64::w1);
	armAsm->B(&done);

	// Div-by-zero: w0 = -1; w1 already holds Rs (HI).
	armAsm->Bind(&divByZero);
	armAsm->Mov(a64::w0, -1);

	armAsm->Bind(&done);

	// Store LO = sign_extend(quotient or -1)
	armAsm->Sxtw(RXSCRATCH, a64::w0);
	armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[0]));

	// Store HI = sign_extend(remainder or rs)
	armAsm->Sxtw(RXSCRATCH, a64::w1);
	armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.HI.UD[0]));
}

#endif // !FORCE_INTERP_MULTDIV

// Write Rd from LO1 (pipeline 1)
static void recWritebackRd1()
{
	if (!_Rd_) return;

	_deleteEEreg(_Rd_, 0);
	GPR_DEL_CONST(_Rd_);
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.LO.UD[1]);
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
}

//// MULT1 — signed 32-bit multiply, pipeline 1 (HI1:LO1)
void recMULT1()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		s64 result = (s64)(s32)g_cpuConstRegs[_Rs_].UL[0] * (s64)(s32)g_cpuConstRegs[_Rt_].UL[0];

		armAsm->Mov(RXSCRATCH, (s64)(s32)(u32)result);
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[1]));

		armAsm->Mov(RXSCRATCH, (s64)(s32)(u32)(result >> 32));
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.HI.UD[1]));

		if (_Rd_)
		{
			_deleteEEreg(_Rd_, 0);
			g_cpuConstRegs[_Rd_].SD[0] = (s32)(u32)result;
			GPR_SET_CONST(_Rd_);
		}
		return;
	}

	_eeFlushAllDirty();
	loadRs32();
	loadRt32();

	armAsm->Smull(a64::x0, a64::w1, a64::w2);

	recWritebackHILO(true);
	recWritebackRd1();
}

//// MULTU1 — unsigned 32-bit multiply, pipeline 1
void recMULTU1()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		u64 result = (u64)g_cpuConstRegs[_Rs_].UL[0] * (u64)g_cpuConstRegs[_Rt_].UL[0];

		armAsm->Mov(RXSCRATCH, (s64)(s32)(u32)result);
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[1]));

		armAsm->Mov(RXSCRATCH, (s64)(s32)(u32)(result >> 32));
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.HI.UD[1]));

		if (_Rd_)
		{
			_deleteEEreg(_Rd_, 0);
			g_cpuConstRegs[_Rd_].SD[0] = (s32)(u32)result;
			GPR_SET_CONST(_Rd_);
		}
		return;
	}

	_eeFlushAllDirty();
	loadRs32();
	loadRt32();

	armAsm->Umull(a64::x0, a64::w1, a64::w2);

	recWritebackHILO(true);
	recWritebackRd1();
}

//// DIV1 — signed 32-bit divide, pipeline 1. Same div-by-zero spec as DIV.
void recDIV1()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		s32 rs = g_cpuConstRegs[_Rs_].SL[0];
		s32 rt = g_cpuConstRegs[_Rt_].SL[0];

		s32 lo, hi;
		if (rt == 0)
		{
			lo = (rs >= 0) ? -1 : 1;
			hi = rs;
		}
		else if (rs == (s32)0x80000000 && rt == -1)
		{
			lo = (s32)0x80000000;
			hi = 0;
		}
		else
		{
			lo = rs / rt;
			hi = rs % rt;
		}

		armAsm->Mov(RXSCRATCH, (s64)lo);
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[1]));

		armAsm->Mov(RXSCRATCH, (s64)hi);
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.HI.UD[1]));
		return;
	}

	_eeFlushAllDirty();
	loadRs32();
	loadRt32();

	a64::Label divByZero;
	a64::Label done;
	armAsm->Cbz(a64::w2, &divByZero);

	armAsm->Sdiv(a64::w0, a64::w1, a64::w2);
	armAsm->Msub(a64::w3, a64::w0, a64::w2, a64::w1);
	armAsm->B(&done);

	// Div-by-zero: w0 = (rs >= 0 ? -1 : 1), w3 = rs.  See recDIV for Cneg rationale.
	armAsm->Bind(&divByZero);
	armAsm->Mov(a64::w0, -1);
	armAsm->Cmp(a64::w1, 0);
	armAsm->Cneg(a64::w0, a64::w0, a64::lt);
	armAsm->Mov(a64::w3, a64::w1);                    // HI = rs

	armAsm->Bind(&done);

	armAsm->Sxtw(RXSCRATCH, a64::w0);
	armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[1]));

	armAsm->Sxtw(RXSCRATCH, a64::w3);
	armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.HI.UD[1]));
}

//// DIVU1 — unsigned 32-bit divide, pipeline 1. Same div-by-zero spec as DIVU.
void recDIVU1()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		u32 rs = g_cpuConstRegs[_Rs_].UL[0];
		u32 rt = g_cpuConstRegs[_Rt_].UL[0];

		s32 lo, hi;
		if (rt == 0)
		{
			lo = -1;
			hi = (s32)rs;
		}
		else
		{
			lo = (s32)(rs / rt);
			hi = (s32)(rs % rt);
		}

		armAsm->Mov(RXSCRATCH, (s64)lo);
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[1]));

		armAsm->Mov(RXSCRATCH, (s64)hi);
		armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.HI.UD[1]));
		return;
	}

	_eeFlushAllDirty();
	loadRs32();
	loadRt32();

	a64::Label divByZero;
	a64::Label done;
	armAsm->Cbz(a64::w2, &divByZero);

	// See recDIVU for the Msub-into-w1 rationale.
	armAsm->Udiv(a64::w0, a64::w1, a64::w2);
	armAsm->Msub(a64::w1, a64::w0, a64::w2, a64::w1);
	armAsm->B(&done);

	armAsm->Bind(&divByZero);
	armAsm->Mov(a64::w0, -1);

	armAsm->Bind(&done);

	armAsm->Sxtw(RXSCRATCH, a64::w0);
	armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[1]));

	armAsm->Sxtw(RXSCRATCH, a64::w1);
	armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.HI.UD[1]));
}

//// MADD — signed multiply-add: HI:LO += Rs * Rt, Rd = LO
void recMADD()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		s64 result = (s64)(s32)g_cpuConstRegs[_Rs_].UL[0] * (s64)(s32)g_cpuConstRegs[_Rt_].UL[0];

		// Add to existing HI:LO — load, add, store
		_eeFlushAllDirty();
		armLoadEERegPtr(a64::w1, &cpuRegs.LO.UL[0]);
		armLoadEERegPtr(a64::w2, &cpuRegs.HI.UL[0]);
		armAsm->Orr(a64::x1, a64::x1, a64::Operand(a64::x2, a64::LSL, 32));
		armAsm->Mov(RXSCRATCH, result);
		armAsm->Add(a64::x0, a64::x1, RXSCRATCH);

		recWritebackHILO(false);
		recWritebackRd();
		return;
	}

	_eeFlushAllDirty();
	loadRs32();
	loadRt32();

	// x0 = Rs * Rt (signed 32x32→64)
	armAsm->Smull(a64::x0, a64::w1, a64::w2);

	// Load existing HI:LO into x1
	armLoadEERegPtr(a64::w3, &cpuRegs.LO.UL[0]);
	armLoadEERegPtr(a64::w4, &cpuRegs.HI.UL[0]);
	armAsm->Orr(a64::x3, a64::x3, a64::Operand(a64::x4, a64::LSL, 32));

	// Add
	armAsm->Add(a64::x0, a64::x0, a64::x3);

	recWritebackHILO(false);
	recWritebackRd();
}

//// MADDU — unsigned multiply-add: HI:LO += Rs * Rt, Rd = LO
void recMADDU()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		u64 result = (u64)g_cpuConstRegs[_Rs_].UL[0] * (u64)g_cpuConstRegs[_Rt_].UL[0];

		_eeFlushAllDirty();
		armLoadEERegPtr(a64::w1, &cpuRegs.LO.UL[0]);
		armLoadEERegPtr(a64::w2, &cpuRegs.HI.UL[0]);
		armAsm->Orr(a64::x1, a64::x1, a64::Operand(a64::x2, a64::LSL, 32));
		armAsm->Mov(RXSCRATCH, result);
		armAsm->Add(a64::x0, a64::x1, RXSCRATCH);

		recWritebackHILO(false);
		recWritebackRd();
		return;
	}

	_eeFlushAllDirty();
	loadRs32();
	loadRt32();

	armAsm->Umull(a64::x0, a64::w1, a64::w2);

	armLoadEERegPtr(a64::w3, &cpuRegs.LO.UL[0]);
	armLoadEERegPtr(a64::w4, &cpuRegs.HI.UL[0]);
	armAsm->Orr(a64::x3, a64::x3, a64::Operand(a64::x4, a64::LSL, 32));

	armAsm->Add(a64::x0, a64::x0, a64::x3);

	recWritebackHILO(false);
	recWritebackRd();
}

//// MADD1 — signed multiply-add, pipeline 1: HI1:LO1 += Rs * Rt, Rd = LO1
void recMADD1()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		s64 result = (s64)(s32)g_cpuConstRegs[_Rs_].UL[0] * (s64)(s32)g_cpuConstRegs[_Rt_].UL[0];

		_eeFlushAllDirty();
		armLoadEERegPtr(a64::w1, &cpuRegs.LO.UL[2]);
		armLoadEERegPtr(a64::w2, &cpuRegs.HI.UL[2]);
		armAsm->Orr(a64::x1, a64::x1, a64::Operand(a64::x2, a64::LSL, 32));
		armAsm->Mov(RXSCRATCH, result);
		armAsm->Add(a64::x0, a64::x1, RXSCRATCH);

		recWritebackHILO(true);
		recWritebackRd1();
		return;
	}

	_eeFlushAllDirty();
	loadRs32();
	loadRt32();

	armAsm->Smull(a64::x0, a64::w1, a64::w2);

	armLoadEERegPtr(a64::w3, &cpuRegs.LO.UL[2]);  // LO1 = LO.UL[2] (upper 64 bits)
	armLoadEERegPtr(a64::w4, &cpuRegs.HI.UL[2]);  // HI1 = HI.UL[2]
	armAsm->Orr(a64::x3, a64::x3, a64::Operand(a64::x4, a64::LSL, 32));

	armAsm->Add(a64::x0, a64::x0, a64::x3);

	recWritebackHILO(true);
	recWritebackRd1();
}

//// MADDU1 — unsigned multiply-add, pipeline 1
void recMADDU1()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		u64 result = (u64)g_cpuConstRegs[_Rs_].UL[0] * (u64)g_cpuConstRegs[_Rt_].UL[0];

		_eeFlushAllDirty();
		armLoadEERegPtr(a64::w1, &cpuRegs.LO.UL[2]);
		armLoadEERegPtr(a64::w2, &cpuRegs.HI.UL[2]);
		armAsm->Orr(a64::x1, a64::x1, a64::Operand(a64::x2, a64::LSL, 32));
		armAsm->Mov(RXSCRATCH, result);
		armAsm->Add(a64::x0, a64::x1, RXSCRATCH);

		recWritebackHILO(true);
		recWritebackRd1();
		return;
	}

	_eeFlushAllDirty();
	loadRs32();
	loadRt32();

	armAsm->Umull(a64::x0, a64::w1, a64::w2);

	armLoadEERegPtr(a64::w3, &cpuRegs.LO.UL[2]);
	armLoadEERegPtr(a64::w4, &cpuRegs.HI.UL[2]);
	armAsm->Orr(a64::x3, a64::x3, a64::Operand(a64::x4, a64::LSL, 32));

	armAsm->Add(a64::x0, a64::x0, a64::x3);

	recWritebackHILO(true);
	recWritebackRd1();
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
