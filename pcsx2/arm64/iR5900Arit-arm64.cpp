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

// Operand helpers. Loads return the register holding the operand, coherent by
// construction: a pin mirror when the guest reg is pinned, an allocator-
// resident slot once residency is flipped on, else the scratch (memory load).
// Callers must treat the returned register as READ-ONLY and put results in
// scratch (or pass them straight to the store helper). Mirrors the Phase-1
// conversion of the sibling shift/imm-ALU helper sets — the residency probe
// lives once in the central _eeGetGPRSourceReg/_eeGetGPRDestReg/_eeStoreGPRDestReg
// accessors (cf. PCSX2 x86: pcsx2/x86/iCore.cpp _allocX86reg + the
// pcsx2/x86/ix86-32/iR5900Templates.cpp RC0 template).
static a64::Register memLoadS32()
{
	return _eeGetGPRSourceReg(RWARG1, _Rs_);
}
static a64::Register memLoadT32()
{
	return _eeGetGPRSourceReg(RWSCRATCH, _Rt_);
}
static a64::Register memLoadS64()
{
	return _eeGetGPRSourceReg(RXARG1, _Rs_);
}
static a64::Register memLoadT64()
{
	return _eeGetGPRSourceReg(RXSCRATCH, _Rt_);
}
static void memStoreD(const a64::Register& src)
{
	_eeStoreGPRDestReg(_Rd_, src);
}
// Dest home for the result: the FINAL result-producing instruction targets it,
// then memStoreD deposits (a pin/memory store, or a deferred allocator-slot
// store under the flip). See _eeGetGPRDestReg's contract — intermediates stay
// in scratch. alloc_if_used=false keeps Phase-1 behavior; the RC0 template
// owns the dest allocation.
static a64::Register memDestD()
{
	return _eeGetGPRDestReg(_Rd_, RXSCRATCH);
}

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
	const a64::Register dst = memDestD();
	if (cval != 0)
	{
		armAsm->Add(RWSCRATCH, rt, cval);
		armAsm->Sxtw(dst, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(dst, rt);
	}
	memStoreD(dst);
}

static void recADD_constt(int info)
{
	const s32 cval = g_cpuConstRegs[_Rt_].SL[0];
	const a64::Register rs = memLoadS32();
	const a64::Register dst = memDestD();
	if (cval != 0)
	{
		armAsm->Add(RWSCRATCH, rs, cval);
		armAsm->Sxtw(dst, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(dst, rs);
	}
	memStoreD(dst);
}

static void recADD_(int info)
{
	const a64::Register rs = memLoadS32();
	const a64::Register rt = memLoadT32();
	const a64::Register dst = memDestD();
	armAsm->Add(RWSCRATCH, rs, rt);
	armAsm->Sxtw(dst, RWSCRATCH);
	memStoreD(dst);
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
		const a64::Register dst = memDestD();
		armAsm->Add(dst, rt, cval); // commutative — vixl folds encodable imms (GE-05)
		memStoreD(dst);
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
		const a64::Register dst = memDestD();
		armAsm->Add(dst, rs, cval);
		memStoreD(dst);
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
	const a64::Register dst = memDestD();
	armAsm->Add(dst, rs, rt);
	memStoreD(dst);
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
	const a64::Register dst = memDestD();
	armAsm->Mov(RWARG1, cval);
	armAsm->Sub(RWSCRATCH, RWARG1, rt);
	armAsm->Sxtw(dst, RWSCRATCH);
	memStoreD(dst);
}

static void recSUB_constt(int info)
{
	const s32 cval = g_cpuConstRegs[_Rt_].SL[0];
	const a64::Register rs = memLoadS32();
	const a64::Register dst = memDestD();
	if (cval != 0)
	{
		armAsm->Sub(RWSCRATCH, rs, cval);
		armAsm->Sxtw(dst, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(dst, rs);
	}
	memStoreD(dst);
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
	const a64::Register dst = memDestD();
	armAsm->Sub(RWSCRATCH, rs, rt);
	armAsm->Sxtw(dst, RWSCRATCH);
	memStoreD(dst);
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
	const a64::Register dst = memDestD();
	// cval - rt == -(rt - cval): the negated form folds an add/sub-encodable
	// imm (2 insns, 0 for the const); non-encodable keeps Mov+Sub — the
	// Sub-with-temp fallback would be 3 insns vs this 2 (GE-05).
	if (cval == 0)
	{
		armAsm->Neg(dst, rt);
	}
	else if (a64::Assembler::IsImmAddSub(cval) || (cval != INT64_MIN && a64::Assembler::IsImmAddSub(-cval)))
	{
		armAsm->Sub(RXSCRATCH, rt, cval);
		armAsm->Neg(dst, RXSCRATCH);
	}
	else
	{
		armAsm->Mov(RXARG1, cval);
		armAsm->Sub(dst, RXARG1, rt);
	}
	memStoreD(dst);
}

static void recDSUB_constt(int info)
{
	const s64 cval = g_cpuConstRegs[_Rt_].SD[0];
	const a64::Register rs = memLoadS64();
	if (cval != 0)
	{
		const a64::Register dst = memDestD();
		armAsm->Sub(dst, rs, cval);
		memStoreD(dst);
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
	const a64::Register dst = memDestD();
	armAsm->Sub(dst, rs, rt);
	memStoreD(dst);
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
	const a64::Register dst = memDestD();
	armAsm->And(dst, rt, g_cpuConstRegs[_Rs_].UD[0]);
	memStoreD(dst);
}

static void recAND_constt(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register dst = memDestD();
	armAsm->And(dst, rs, g_cpuConstRegs[_Rt_].UD[0]);
	memStoreD(dst);
}

static void recAND_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	const a64::Register dst = memDestD();
	armAsm->And(dst, rs, rt);
	memStoreD(dst);
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
		const a64::Register dst = memDestD();
		armAsm->Orr(dst, rt, cval);
		memStoreD(dst);
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
		const a64::Register dst = memDestD();
		armAsm->Orr(dst, rs, cval);
		memStoreD(dst);
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
	const a64::Register dst = memDestD();
	armAsm->Orr(dst, rs, rt);
	memStoreD(dst);
}

EERECOMPILE_CODERC0_MEM(OR, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// XOR — 64-bit

static void recXOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] ^ g_cpuConstRegs[_Rt_].UD[0];
}

static void recXOR_consts(int info)
{
	const u64 cval = g_cpuConstRegs[_Rs_].UD[0];
	const a64::Register rt = memLoadT64();
	if (cval != 0)
	{
		const a64::Register dst = memDestD();
		armAsm->Eor(dst, rt, cval); // vixl folds bitmask-encodable imms (GE-05)
		memStoreD(dst);
	}
	else
	{
		memStoreD(rt);
	}
}

static void recXOR_constt(int info)
{
	const u64 cval = g_cpuConstRegs[_Rt_].UD[0];
	const a64::Register rs = memLoadS64();
	if (cval != 0)
	{
		const a64::Register dst = memDestD();
		armAsm->Eor(dst, rs, cval);
		memStoreD(dst);
	}
	else
	{
		memStoreD(rs);
	}
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
	const a64::Register dst = memDestD();
	armAsm->Eor(dst, rs, rt);
	memStoreD(dst);
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
	const a64::Register dst = memDestD();
	if (cval != 0)
	{
		armAsm->Orr(RXSCRATCH, rt, cval);
		armAsm->Mvn(dst, RXSCRATCH);
	}
	else
	{
		armAsm->Mvn(dst, rt);
	}
	memStoreD(dst);
}

static void recNOR_constt(int info)
{
	const u64 cval = g_cpuConstRegs[_Rt_].UD[0];
	const a64::Register rs = memLoadS64();
	const a64::Register dst = memDestD();
	if (cval != 0)
	{
		armAsm->Orr(RXSCRATCH, rs, cval);
		armAsm->Mvn(dst, RXSCRATCH);
	}
	else
	{
		armAsm->Mvn(dst, rs);
	}
	memStoreD(dst);
}

static void recNOR_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	const a64::Register dst = memDestD();
	armAsm->Orr(RXSCRATCH, rs, rt);
	armAsm->Mvn(dst, RXSCRATCH);
	memStoreD(dst);
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
	const a64::Register dst = memDestD();
	// cval < rt  ==  rt > cval: reversed compare with flipped condition lets the
	// vixl macro fold an encodable immediate (GE-05).
	armAsm->Cmp(rt, g_cpuConstRegs[_Rs_].SD[0]);
	armAsm->Cset(dst, a64::gt);
	memStoreD(dst);
}

static void recSLT_constt(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register dst = memDestD();
	armAsm->Cmp(rs, g_cpuConstRegs[_Rt_].SD[0]);
	armAsm->Cset(dst, a64::lt);
	memStoreD(dst);
}

static void recSLT_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	const a64::Register dst = memDestD();
	armAsm->Cmp(rs, rt);
	armAsm->Cset(dst, a64::lt);
	memStoreD(dst);
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
	const a64::Register dst = memDestD();
	// cval <u rt  ==  rt >u cval (GE-05, see recSLT_consts).
	armAsm->Cmp(rt, g_cpuConstRegs[_Rs_].UD[0]);
	armAsm->Cset(dst, a64::hi);
	memStoreD(dst);
}

static void recSLTU_constt(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register dst = memDestD();
	armAsm->Cmp(rs, g_cpuConstRegs[_Rt_].UD[0]);
	armAsm->Cset(dst, a64::lo);
	memStoreD(dst);
}

static void recSLTU_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	const a64::Register dst = memDestD();
	armAsm->Cmp(rs, rt);
	armAsm->Cset(dst, a64::lo);
	memStoreD(dst);
}

EERECOMPILE_CODERC0_MEM(SLTU, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

#endif // !FORCE_INTERP_ALU

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
