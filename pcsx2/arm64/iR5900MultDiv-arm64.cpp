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

// Fetch Rs/Rt lower 32 bits for the mul/div ops. Substitution-aware
// (EE-SRA 2 WS-C6): returns the pin / MODE_READ allocator reg directly (zero
// insns) or materializes into w1/w2. Every caller sits right after
// _eeFlushAllDirty (the post-flush coherence contract), and every consumer
// below is read-only on the sources — EXCEPT the DIVU remainder Msub, which
// therefore targets w3 rather than writing a source in place.
static a64::Register loadRs32()
{
	if (GPR_IS_CONST1(_Rs_))
	{
		armAsm->Mov(a64::w1, g_cpuConstRegs[_Rs_].UL[0]);
		return a64::w1;
	}
	return _eeGetGPRSourceReg(a64::w1, _Rs_);
}

static a64::Register loadRt32()
{
	if (GPR_IS_CONST1(_Rt_))
	{
		armAsm->Mov(a64::w2, g_cpuConstRegs[_Rt_].UL[0]);
		return a64::w2;
	}
	return _eeGetGPRSourceReg(a64::w2, _Rt_);
}

// Write LO and HI from 64-bit result in x0 (clobbers x0)
// lo = lower 32, hi = upper 32, both sign-extended to 64 bits
static void recWritebackHILO(bool upper)
{
	armAsm->Sxtw(RXSCRATCH, a64::w0);
	armAsm->Str(RXSCRATCH, armCpuRegMem(upper ? &cpuRegs.LO.UD[1] : &cpuRegs.LO.UD[0]));

	// Asr(x0, #32) already yields the sign-extended upper half — store it directly (GE-01).
	armAsm->Asr(a64::x0, a64::x0, 32);
	armAsm->Str(a64::x0, armCpuRegMem(upper ? &cpuRegs.HI.UD[1] : &cpuRegs.HI.UD[0]));
}

// Write Rd from LO (memory-based — no register allocation)
static void recWritebackRd()
{
	if (!_Rd_) return;

	_deleteEEreg(_Rd_, 0);
	GPR_DEL_CONST(_Rd_);
	const a64::Register dst = armEEDestForGPR(_Rd_, RXSCRATCH);
	armLoadEERegPtr(dst, &cpuRegs.LO.UD[0]);
	armStoreEERegPtr(dst, &cpuRegs.GPR.r[_Rd_].UD[0]);
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
	const a64::Register rs32 = loadRs32();
	const a64::Register rt32 = loadRt32();

	armAsm->Smull(a64::x0, rs32, rt32);

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
	const a64::Register rs32 = loadRs32();
	const a64::Register rt32 = loadRt32();

	armAsm->Umull(a64::x0, rs32, rt32);

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
	const a64::Register rs32 = loadRs32();
	const a64::Register rt32 = loadRt32();

	// Branch on rt == 0 → div-by-zero handler.
	a64::Label divByZero;
	a64::Label done;
	armAsm->Cbz(rt32, &divByZero);

	// Normal path: SDIV w0, w1, w2; MSUB w3 = w1 - w0 * w2 (remainder).
	armAsm->Sdiv(a64::w0, rs32, rt32);
	armAsm->Msub(a64::w3, a64::w0, rt32, rs32);
	armAsm->B(&done);

	// Div-by-zero: w0 = (rs >= 0 ? -1 : 1), w3 = rs.
	// Cneg w0, w0, lt: if rs < 0, w0 = -(-1) = 1; else w0 = -1.
	armAsm->Bind(&divByZero);
	armAsm->Mov(a64::w0, -1);
	armAsm->Cmp(rs32, 0);
	armAsm->Cneg(a64::w0, a64::w0, a64::lt);
	armAsm->Mov(a64::w3, rs32);                       // HI = rs

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
	const a64::Register rs32 = loadRs32();
	const a64::Register rt32 = loadRt32();

	a64::Label divByZero;
	a64::Label done;
	armAsm->Cbz(rt32, &divByZero);

	// Normal path: UDIV w0; MSUB remainder into w3 — NOT in place over the Rs
	// source, which may be a pin (WS-C6 pin-safety; the old in-place form
	// predates operand substitution).
	armAsm->Udiv(a64::w0, rs32, rt32);
	armAsm->Msub(a64::w3, a64::w0, rt32, rs32);
	armAsm->B(&done);

	// Div-by-zero: w0 = -1; HI = rs.
	armAsm->Bind(&divByZero);
	armAsm->Mov(a64::w0, -1);
	armAsm->Mov(a64::w3, rs32);

	armAsm->Bind(&done);

	// Store LO = sign_extend(quotient or -1)
	armAsm->Sxtw(RXSCRATCH, a64::w0);
	armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[0]));

	// Store HI = sign_extend(remainder or rs)
	armAsm->Sxtw(RXSCRATCH, a64::w3);
	armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.HI.UD[0]));
}

#endif // !FORCE_INTERP_MULTDIV

// Write Rd from LO1 (pipeline 1)
static void recWritebackRd1()
{
	if (!_Rd_) return;

	_deleteEEreg(_Rd_, 0);
	GPR_DEL_CONST(_Rd_);
	const a64::Register dst = armEEDestForGPR(_Rd_, RXSCRATCH);
	armLoadEERegPtr(dst, &cpuRegs.LO.UD[1]);
	armStoreEERegPtr(dst, &cpuRegs.GPR.r[_Rd_].UD[0]);
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
	const a64::Register rs32 = loadRs32();
	const a64::Register rt32 = loadRt32();

	armAsm->Smull(a64::x0, rs32, rt32);

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
	const a64::Register rs32 = loadRs32();
	const a64::Register rt32 = loadRt32();

	armAsm->Umull(a64::x0, rs32, rt32);

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
	const a64::Register rs32 = loadRs32();
	const a64::Register rt32 = loadRt32();

	a64::Label divByZero;
	a64::Label done;
	armAsm->Cbz(rt32, &divByZero);

	armAsm->Sdiv(a64::w0, rs32, rt32);
	armAsm->Msub(a64::w3, a64::w0, rt32, rs32);
	armAsm->B(&done);

	// Div-by-zero: w0 = (rs >= 0 ? -1 : 1), w3 = rs.  See recDIV for Cneg rationale.
	armAsm->Bind(&divByZero);
	armAsm->Mov(a64::w0, -1);
	armAsm->Cmp(rs32, 0);
	armAsm->Cneg(a64::w0, a64::w0, a64::lt);
	armAsm->Mov(a64::w3, rs32);                       // HI = rs

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
	const a64::Register rs32 = loadRs32();
	const a64::Register rt32 = loadRt32();

	a64::Label divByZero;
	a64::Label done;
	armAsm->Cbz(rt32, &divByZero);

	// Remainder into w3, not in place — see recDIVU (WS-C6 pin-safety).
	armAsm->Udiv(a64::w0, rs32, rt32);
	armAsm->Msub(a64::w3, a64::w0, rt32, rs32);
	armAsm->B(&done);

	armAsm->Bind(&divByZero);
	armAsm->Mov(a64::w0, -1);
	armAsm->Mov(a64::w3, rs32);

	armAsm->Bind(&done);

	armAsm->Sxtw(RXSCRATCH, a64::w0);
	armAsm->Str(RXSCRATCH, armCpuRegMem(&cpuRegs.LO.UD[1]));

	armAsm->Sxtw(RXSCRATCH, a64::w3);
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
	const a64::Register rs32 = loadRs32();
	const a64::Register rt32 = loadRt32();

	// x0 = Rs * Rt (signed 32x32→64)
	armAsm->Smull(a64::x0, rs32, rt32);

	// Load existing HI:LO into x1
	armLoadEERegPtr(a64::w3, &cpuRegs.LO.UL[0]);
	armLoadEERegPtr(RWSCRATCH, &cpuRegs.HI.UL[0]); // w8: reserved scratch (w4 is allocatable)
	armAsm->Orr(a64::x3, a64::x3, a64::Operand(RXSCRATCH, a64::LSL, 32));

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
	const a64::Register rs32 = loadRs32();
	const a64::Register rt32 = loadRt32();

	armAsm->Umull(a64::x0, rs32, rt32);

	armLoadEERegPtr(a64::w3, &cpuRegs.LO.UL[0]);
	armLoadEERegPtr(RWSCRATCH, &cpuRegs.HI.UL[0]); // w8: reserved scratch (w4 is allocatable)
	armAsm->Orr(a64::x3, a64::x3, a64::Operand(RXSCRATCH, a64::LSL, 32));

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
	const a64::Register rs32 = loadRs32();
	const a64::Register rt32 = loadRt32();

	armAsm->Smull(a64::x0, rs32, rt32);

	armLoadEERegPtr(a64::w3, &cpuRegs.LO.UL[2]);  // LO1 = LO.UL[2] (upper 64 bits)
	armLoadEERegPtr(RWSCRATCH, &cpuRegs.HI.UL[2]); // HI1 = HI.UL[2] (w8: reserved scratch — w4 is allocatable)
	armAsm->Orr(a64::x3, a64::x3, a64::Operand(RXSCRATCH, a64::LSL, 32));

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
	const a64::Register rs32 = loadRs32();
	const a64::Register rt32 = loadRt32();

	armAsm->Umull(a64::x0, rs32, rt32);

	armLoadEERegPtr(a64::w3, &cpuRegs.LO.UL[2]);
	armLoadEERegPtr(RWSCRATCH, &cpuRegs.HI.UL[2]); // w8: reserved scratch (w4 is allocatable)
	armAsm->Orr(a64::x3, a64::x3, a64::Operand(RXSCRATCH, a64::LSL, 32));

	armAsm->Add(a64::x0, a64::x0, a64::x3);

	recWritebackHILO(true);
	recWritebackRd1();
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
