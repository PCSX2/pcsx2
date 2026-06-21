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

// Memory load/store helpers — always use cpuRegs memory
static void memLoadS32() { armLoadEERegPtr(RWARG1, &cpuRegs.GPR.r[_Rs_].UL[0]); }
static void memLoadT32() { armLoadEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rt_].UL[0]); }
static void memLoadS64() { armLoadEERegPtr(RXARG1, &cpuRegs.GPR.r[_Rs_].UD[0]); }
static void memLoadT64() { armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rt_].UD[0]); }
static void memStoreD() { armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]); }

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
	memLoadT32();
	if (cval != 0)
		armAsm->Add(RWSCRATCH, RWSCRATCH, cval);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreD();
}

static void recADD_constt(int info)
{
	const s32 cval = g_cpuConstRegs[_Rt_].SL[0];
	memLoadS32();
	if (cval != 0)
	{
		armAsm->Add(RWSCRATCH, RWARG1, cval);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(RXSCRATCH, RWARG1);
	}
	memStoreD();
}

static void recADD_(int info)
{
	memLoadS32();
	memLoadT32();
	armAsm->Add(RWSCRATCH, RWARG1, RWSCRATCH);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreD();
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
	memLoadT64();
	if (cval != 0)
	{
		armAsm->Mov(RXARG1, cval);
		armAsm->Add(RXSCRATCH, RXSCRATCH, RXARG1);
	}
	memStoreD();
}

static void recDADD_constt(int info)
{
	const s64 cval = g_cpuConstRegs[_Rt_].SD[0];
	memLoadS64();
	if (cval != 0)
		armAsm->Add(RXSCRATCH, RXARG1, cval);
	else
		armAsm->Mov(RXSCRATCH, RXARG1);
	memStoreD();
}

static void recDADD_(int info)
{
	memLoadS64();
	memLoadT64();
	armAsm->Add(RXSCRATCH, RXARG1, RXSCRATCH);
	memStoreD();
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
	memLoadT32();
	armAsm->Mov(RWARG1, cval);
	armAsm->Sub(RWSCRATCH, RWARG1, RWSCRATCH);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreD();
}

static void recSUB_constt(int info)
{
	const s32 cval = g_cpuConstRegs[_Rt_].SL[0];
	memLoadS32();
	if (cval != 0)
	{
		armAsm->Sub(RWSCRATCH, RWARG1, cval);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(RXSCRATCH, RWARG1);
	}
	memStoreD();
}

static void recSUB_(int info)
{
	// rs - rs == 0 always; emit a single store of zero.
	if (_Rs_ == _Rt_)
	{
		armStoreEERegPtr(a64::xzr, &cpuRegs.GPR.r[_Rd_].UD[0]);
		return;
	}
	memLoadS32();
	memLoadT32();
	armAsm->Sub(RWSCRATCH, RWARG1, RWSCRATCH);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreD();
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
	memLoadT64();
	armAsm->Mov(RXARG1, cval);
	armAsm->Sub(RXSCRATCH, RXARG1, RXSCRATCH);
	memStoreD();
}

static void recDSUB_constt(int info)
{
	const s64 cval = g_cpuConstRegs[_Rt_].SD[0];
	memLoadS64();
	if (cval != 0)
		armAsm->Sub(RXSCRATCH, RXARG1, cval);
	else
		armAsm->Mov(RXSCRATCH, RXARG1);
	memStoreD();
}

static void recDSUB_(int info)
{
	// rs - rs == 0 always; emit a single store of zero.
	if (_Rs_ == _Rt_)
	{
		armStoreEERegPtr(a64::xzr, &cpuRegs.GPR.r[_Rd_].UD[0]);
		return;
	}
	memLoadS64();
	memLoadT64();
	armAsm->Sub(RXSCRATCH, RXARG1, RXSCRATCH);
	memStoreD();
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
	memLoadT64();
	armAsm->And(RXSCRATCH, RXSCRATCH, g_cpuConstRegs[_Rs_].UD[0]);
	memStoreD();
}

static void recAND_constt(int info)
{
	memLoadS64();
	armAsm->And(RXSCRATCH, RXARG1, g_cpuConstRegs[_Rt_].UD[0]);
	memStoreD();
}

static void recAND_(int info)
{
	memLoadS64();
	memLoadT64();
	armAsm->And(RXSCRATCH, RXARG1, RXSCRATCH);
	memStoreD();
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
	memLoadT64();
	if (cval != 0)
		armAsm->Orr(RXSCRATCH, RXSCRATCH, cval);
	memStoreD();
}

static void recOR_constt(int info)
{
	const u64 cval = g_cpuConstRegs[_Rt_].UD[0];
	memLoadS64();
	if (cval != 0)
		armAsm->Orr(RXSCRATCH, RXARG1, cval);
	else
		armAsm->Mov(RXSCRATCH, RXARG1);
	memStoreD();
}

static void recOR_(int info)
{
	memLoadS64();
	memLoadT64();
	armAsm->Orr(RXSCRATCH, RXARG1, RXSCRATCH);
	memStoreD();
}

EERECOMPILE_CODERC0_MEM(OR, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// XOR — 64-bit

static void recXOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] ^ g_cpuConstRegs[_Rt_].UD[0];
}

static void recXOR_consts(int info)
{
	memLoadT64();
	armAsm->Mov(RXARG1, g_cpuConstRegs[_Rs_].UD[0]);
	armAsm->Eor(RXSCRATCH, RXSCRATCH, RXARG1);
	memStoreD();
}

static void recXOR_constt(int info)
{
	memLoadS64();
	armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rt_].UD[0]);
	armAsm->Eor(RXSCRATCH, RXARG1, RXSCRATCH);
	memStoreD();
}

static void recXOR_(int info)
{
	// rs ^ rs == 0 always; skip the two operand loads (mirrors recSUB_).
	if (_Rs_ == _Rt_)
	{
		armAsm->Mov(RXSCRATCH, 0);
		memStoreD();
		return;
	}
	memLoadS64();
	memLoadT64();
	armAsm->Eor(RXSCRATCH, RXARG1, RXSCRATCH);
	memStoreD();
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
	memLoadT64();
	if (cval != 0)
		armAsm->Orr(RXSCRATCH, RXSCRATCH, cval);
	armAsm->Mvn(RXSCRATCH, RXSCRATCH);
	memStoreD();
}

static void recNOR_constt(int info)
{
	const u64 cval = g_cpuConstRegs[_Rt_].UD[0];
	memLoadS64();
	if (cval != 0)
		armAsm->Orr(RXSCRATCH, RXARG1, cval);
	else
		armAsm->Mov(RXSCRATCH, RXARG1);
	armAsm->Mvn(RXSCRATCH, RXSCRATCH);
	memStoreD();
}

static void recNOR_(int info)
{
	memLoadS64();
	memLoadT64();
	armAsm->Orr(RXSCRATCH, RXARG1, RXSCRATCH);
	armAsm->Mvn(RXSCRATCH, RXSCRATCH);
	memStoreD();
}

EERECOMPILE_CODERC0_MEM(NOR, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// SLT — rd = (rs < rt) ? 1 : 0 (signed 64-bit compare)

static void recSLT_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (g_cpuConstRegs[_Rs_].SD[0] < g_cpuConstRegs[_Rt_].SD[0]) ? 1 : 0;
}

static void recSLT_consts(int info)
{
	memLoadT64();
	armAsm->Mov(RXARG1, g_cpuConstRegs[_Rs_].SD[0]);
	armAsm->Cmp(RXARG1, RXSCRATCH);
	armAsm->Cset(RXSCRATCH, a64::lt);
	memStoreD();
}

static void recSLT_constt(int info)
{
	memLoadS64();
	armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rt_].SD[0]);
	armAsm->Cmp(RXARG1, RXSCRATCH);
	armAsm->Cset(RXSCRATCH, a64::lt);
	memStoreD();
}

static void recSLT_(int info)
{
	memLoadS64();
	memLoadT64();
	armAsm->Cmp(RXARG1, RXSCRATCH);
	armAsm->Cset(RXSCRATCH, a64::lt);
	memStoreD();
}

EERECOMPILE_CODERC0_MEM(SLT, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// SLTU — rd = (rs < rt) ? 1 : 0 (unsigned 64-bit compare)

static void recSLTU_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (g_cpuConstRegs[_Rs_].UD[0] < g_cpuConstRegs[_Rt_].UD[0]) ? 1 : 0;
}

static void recSLTU_consts(int info)
{
	memLoadT64();
	armAsm->Mov(RXARG1, g_cpuConstRegs[_Rs_].UD[0]);
	armAsm->Cmp(RXARG1, RXSCRATCH);
	armAsm->Cset(RXSCRATCH, a64::lo);
	memStoreD();
}

static void recSLTU_constt(int info)
{
	memLoadS64();
	armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rt_].UD[0]);
	armAsm->Cmp(RXARG1, RXSCRATCH);
	armAsm->Cset(RXSCRATCH, a64::lo);
	memStoreD();
}

static void recSLTU_(int info)
{
	memLoadS64();
	memLoadT64();
	armAsm->Cmp(RXARG1, RXSCRATCH);
	armAsm->Cset(RXSCRATCH, a64::lo);
	memStoreD();
}

EERECOMPILE_CODERC0_MEM(SLTU, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

#endif // !FORCE_INTERP_ALU

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
