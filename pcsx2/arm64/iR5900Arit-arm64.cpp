// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE ALU Instruction Codegen — memory-based
// All operands loaded from / stored to cpuRegs.GPR memory.
// No register allocation for scalar ops.

#include "arm64/iR5900-arm64.h"

namespace a64 = vixl::aarch64;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

namespace Interp = R5900::Interpreter::OpcodeImpl;

#ifdef FORCE_INTERP_ALU
REC_FUNC(ADD);
void recADDU() { recADD(); }
REC_FUNC(DADD);
void recDADDU() { recDADD(); }
REC_FUNC(SUB);
void recSUBU() { recSUB(); }
REC_FUNC(DSUB);
void recDSUBU() { recDSUB(); }
REC_FUNC(AND);
REC_FUNC(OR);
REC_FUNC(XOR);
REC_FUNC(NOR);
REC_FUNC(SLT);
REC_FUNC(SLTU);
#else

// Memory load/store helpers. Loads return the register holding the operand:
// the write-through pin mirror when the guest reg is pinned ($sp/$ra — no
// load emitted), the scratch otherwise. Callers must treat the returned
// register as READ-ONLY and put results in scratch (or pass them straight
// to the store helper).
static a64::Register memLoadS32()
{
	if (const a64::Register* pin = armEEPinForGPR(_Rs_))
		return pin->W();
	armLoadEERegPtr(RWARG1, &cpuRegs.GPR.r[_Rs_].UL[0]);
	return RWARG1;
}
static a64::Register memLoadT32()
{
	if (const a64::Register* pin = armEEPinForGPR(_Rt_))
		return pin->W();
	armLoadEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rt_].UL[0]);
	return RWSCRATCH;
}
static a64::Register memLoadS64()
{
	if (const a64::Register* pin = armEEPinForGPR(_Rs_))
		return *pin;
	armLoadEERegPtr(RXARG1, &cpuRegs.GPR.r[_Rs_].UD[0]);
	return RXARG1;
}
static a64::Register memLoadT64()
{
	if (const a64::Register* pin = armEEPinForGPR(_Rt_))
		return *pin;
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rt_].UD[0]);
	return RXSCRATCH;
}
static void memStoreD(const a64::Register& src) { armStoreEERegPtr(src, &cpuRegs.GPR.r[_Rd_].UD[0]); }

/*********************************************************
 * Register arithmetic — rd = rs OP rt                   *
 * 32-bit ops sign-extend result to 64 bits              *
 *********************************************************/

//// ADD / ADDU

static void recADD_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] + g_cpuConstRegs[_Rt_].UL[0]));
}

static void recADD_consts(int info)
{
	const s32 cval = g_cpuConstRegs[_Rs_].SL[0];
	const a64::Register rt = memLoadT32();
	if (cval != 0)
	{
		armAsm->Add(RWSCRATCH, rt, cval);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(RXSCRATCH, rt);
	}
	memStoreD(RXSCRATCH);
}

static void recADD_constt(int info)
{
	const s32 cval = g_cpuConstRegs[_Rt_].SL[0];
	const a64::Register rs = memLoadS32();
	if (cval != 0)
	{
		armAsm->Add(RWSCRATCH, rs, cval);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(RXSCRATCH, rs);
	}
	memStoreD(RXSCRATCH);
}

static void recADD_(int info)
{
	const a64::Register rs = memLoadS32();
	const a64::Register rt = memLoadT32();
	armAsm->Add(RWSCRATCH, rs, rt);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreD(RXSCRATCH);
}

EERECOMPILE_CODERC0_MEM(ADD, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

void recADDU() { recADD(); }

//// DADD / DADDU

static void recDADD_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] + g_cpuConstRegs[_Rt_].UD[0];
}

static void recDADD_consts(int info)
{
	const s64 cval = g_cpuConstRegs[_Rs_].SD[0];
	const a64::Register rt = memLoadT64();
	if (cval != 0)
	{
		armAsm->Mov(RXARG1, cval);
		armAsm->Add(RXSCRATCH, rt, RXARG1);
		memStoreD(RXSCRATCH);
	}
	else
	{
		memStoreD(rt);
	}
}

static void recDADD_constt(int info)
{
	const s64 cval = g_cpuConstRegs[_Rt_].SD[0];
	const a64::Register rs = memLoadS64();
	if (cval != 0)
	{
		armAsm->Add(RXSCRATCH, rs, cval);
		memStoreD(RXSCRATCH);
	}
	else
	{
		memStoreD(rs);
	}
}

static void recDADD_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	armAsm->Add(RXSCRATCH, rs, rt);
	memStoreD(RXSCRATCH);
}

EERECOMPILE_CODERC0_MEM(DADD, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

void recDADDU() { recDADD(); }

//// SUB / SUBU

static void recSUB_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] - g_cpuConstRegs[_Rt_].UL[0]));
}

static void recSUB_consts(int info)
{
	const s32 cval = g_cpuConstRegs[_Rs_].SL[0];
	const a64::Register rt = memLoadT32();
	armAsm->Mov(RWARG1, cval);
	armAsm->Sub(RWSCRATCH, RWARG1, rt);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreD(RXSCRATCH);
}

static void recSUB_constt(int info)
{
	const s32 cval = g_cpuConstRegs[_Rt_].SL[0];
	const a64::Register rs = memLoadS32();
	if (cval != 0)
	{
		armAsm->Sub(RWSCRATCH, rs, cval);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(RXSCRATCH, rs);
	}
	memStoreD(RXSCRATCH);
}

static void recSUB_(int info)
{
	// rs - rs == 0 always; emit a single store of zero.
	if (_Rs_ == _Rt_)
	{
		memStoreD(a64::xzr);
		return;
	}
	const a64::Register rs = memLoadS32();
	const a64::Register rt = memLoadT32();
	armAsm->Sub(RWSCRATCH, rs, rt);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreD(RXSCRATCH);
}

EERECOMPILE_CODERC0_MEM(SUB, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

void recSUBU() { recSUB(); }

//// DSUB / DSUBU

static void recDSUB_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] - g_cpuConstRegs[_Rt_].UD[0];
}

static void recDSUB_consts(int info)
{
	const s64 cval = g_cpuConstRegs[_Rs_].SD[0];
	const a64::Register rt = memLoadT64();
	armAsm->Mov(RXARG1, cval);
	armAsm->Sub(RXSCRATCH, RXARG1, rt);
	memStoreD(RXSCRATCH);
}

static void recDSUB_constt(int info)
{
	const s64 cval = g_cpuConstRegs[_Rt_].SD[0];
	const a64::Register rs = memLoadS64();
	if (cval != 0)
	{
		armAsm->Sub(RXSCRATCH, rs, cval);
		memStoreD(RXSCRATCH);
	}
	else
	{
		memStoreD(rs);
	}
}

static void recDSUB_(int info)
{
	// rs - rs == 0 always; emit a single store of zero.
	if (_Rs_ == _Rt_)
	{
		memStoreD(a64::xzr);
		return;
	}
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	armAsm->Sub(RXSCRATCH, rs, rt);
	memStoreD(RXSCRATCH);
}

EERECOMPILE_CODERC0_MEM(DSUB, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

void recDSUBU() { recDSUB(); }

//// AND — 64-bit

static void recAND_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] & g_cpuConstRegs[_Rt_].UD[0];
}

static void recAND_consts(int info)
{
	const a64::Register rt = memLoadT64();
	armAsm->And(RXSCRATCH, rt, g_cpuConstRegs[_Rs_].UD[0]);
	memStoreD(RXSCRATCH);
}

static void recAND_constt(int info)
{
	const a64::Register rs = memLoadS64();
	armAsm->And(RXSCRATCH, rs, g_cpuConstRegs[_Rt_].UD[0]);
	memStoreD(RXSCRATCH);
}

static void recAND_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	armAsm->And(RXSCRATCH, rs, rt);
	memStoreD(RXSCRATCH);
}

EERECOMPILE_CODERC0_MEM(AND, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// OR — 64-bit

static void recOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] | g_cpuConstRegs[_Rt_].UD[0];
}

static void recOR_consts(int info)
{
	const u64 cval = g_cpuConstRegs[_Rs_].UD[0];
	const a64::Register rt = memLoadT64();
	if (cval != 0)
	{
		armAsm->Orr(RXSCRATCH, rt, cval);
		memStoreD(RXSCRATCH);
	}
	else
	{
		memStoreD(rt);
	}
}

static void recOR_constt(int info)
{
	const u64 cval = g_cpuConstRegs[_Rt_].UD[0];
	const a64::Register rs = memLoadS64();
	if (cval != 0)
	{
		armAsm->Orr(RXSCRATCH, rs, cval);
		memStoreD(RXSCRATCH);
	}
	else
	{
		memStoreD(rs);
	}
}

static void recOR_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	armAsm->Orr(RXSCRATCH, rs, rt);
	memStoreD(RXSCRATCH);
}

EERECOMPILE_CODERC0_MEM(OR, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// XOR — 64-bit

static void recXOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] ^ g_cpuConstRegs[_Rt_].UD[0];
}

static void recXOR_consts(int info)
{
	const a64::Register rt = memLoadT64();
	armAsm->Mov(RXARG1, g_cpuConstRegs[_Rs_].UD[0]);
	armAsm->Eor(RXSCRATCH, rt, RXARG1);
	memStoreD(RXSCRATCH);
}

static void recXOR_constt(int info)
{
	const a64::Register rs = memLoadS64();
	armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rt_].UD[0]);
	armAsm->Eor(RXSCRATCH, rs, RXSCRATCH);
	memStoreD(RXSCRATCH);
}

static void recXOR_(int info)
{
	// rs ^ rs == 0 always; skip the two operand loads (mirrors recSUB_).
	if (_Rs_ == _Rt_)
	{
		memStoreD(a64::xzr);
		return;
	}
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	armAsm->Eor(RXSCRATCH, rs, rt);
	memStoreD(RXSCRATCH);
}

EERECOMPILE_CODERC0_MEM(XOR, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// NOR — rd = ~(rs | rt), 64-bit

static void recNOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = ~(g_cpuConstRegs[_Rs_].UD[0] | g_cpuConstRegs[_Rt_].UD[0]);
}

static void recNOR_consts(int info)
{
	const u64 cval = g_cpuConstRegs[_Rs_].UD[0];
	const a64::Register rt = memLoadT64();
	if (cval != 0)
	{
		armAsm->Orr(RXSCRATCH, rt, cval);
		armAsm->Mvn(RXSCRATCH, RXSCRATCH);
	}
	else
	{
		armAsm->Mvn(RXSCRATCH, rt);
	}
	memStoreD(RXSCRATCH);
}

static void recNOR_constt(int info)
{
	const u64 cval = g_cpuConstRegs[_Rt_].UD[0];
	const a64::Register rs = memLoadS64();
	if (cval != 0)
	{
		armAsm->Orr(RXSCRATCH, rs, cval);
		armAsm->Mvn(RXSCRATCH, RXSCRATCH);
	}
	else
	{
		armAsm->Mvn(RXSCRATCH, rs);
	}
	memStoreD(RXSCRATCH);
}

static void recNOR_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	armAsm->Orr(RXSCRATCH, rs, rt);
	armAsm->Mvn(RXSCRATCH, RXSCRATCH);
	memStoreD(RXSCRATCH);
}

EERECOMPILE_CODERC0_MEM(NOR, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// SLT — rd = (rs < rt) ? 1 : 0 (signed 64-bit compare)

static void recSLT_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (g_cpuConstRegs[_Rs_].SD[0] < g_cpuConstRegs[_Rt_].SD[0]) ? 1 : 0;
}

static void recSLT_consts(int info)
{
	const a64::Register rt = memLoadT64();
	armAsm->Mov(RXARG1, g_cpuConstRegs[_Rs_].SD[0]);
	armAsm->Cmp(RXARG1, rt);
	armAsm->Cset(RXSCRATCH, a64::lt);
	memStoreD(RXSCRATCH);
}

static void recSLT_constt(int info)
{
	const a64::Register rs = memLoadS64();
	armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rt_].SD[0]);
	armAsm->Cmp(rs, RXSCRATCH);
	armAsm->Cset(RXSCRATCH, a64::lt);
	memStoreD(RXSCRATCH);
}

static void recSLT_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	armAsm->Cmp(rs, rt);
	armAsm->Cset(RXSCRATCH, a64::lt);
	memStoreD(RXSCRATCH);
}

EERECOMPILE_CODERC0_MEM(SLT, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// SLTU — rd = (rs < rt) ? 1 : 0 (unsigned 64-bit compare)

static void recSLTU_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (g_cpuConstRegs[_Rs_].UD[0] < g_cpuConstRegs[_Rt_].UD[0]) ? 1 : 0;
}

static void recSLTU_consts(int info)
{
	const a64::Register rt = memLoadT64();
	armAsm->Mov(RXARG1, g_cpuConstRegs[_Rs_].UD[0]);
	armAsm->Cmp(RXARG1, rt);
	armAsm->Cset(RXSCRATCH, a64::lo);
	memStoreD(RXSCRATCH);
}

static void recSLTU_constt(int info)
{
	const a64::Register rs = memLoadS64();
	armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rt_].UD[0]);
	armAsm->Cmp(rs, RXSCRATCH);
	armAsm->Cset(RXSCRATCH, a64::lo);
	memStoreD(RXSCRATCH);
}

static void recSLTU_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	armAsm->Cmp(rs, rt);
	armAsm->Cset(RXSCRATCH, a64::lo);
	memStoreD(RXSCRATCH);
}

EERECOMPILE_CODERC0_MEM(SLTU, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

#endif // !FORCE_INTERP_ALU

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
