// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE Shift Instruction Codegen — memory-based
// All operands loaded from / stored to cpuRegs.GPR memory.
// No register allocation for scalar ops.

#include "arm64/iR5900-arm64.h"

namespace a64 = vixl::aarch64;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

namespace Interp = R5900::Interpreter::OpcodeImpl;

#ifdef FORCE_INTERP_SHIFT
REC_FUNC(SLL);
REC_FUNC(SRL);
REC_FUNC(SRA);
REC_FUNC(DSLL);
REC_FUNC(DSRL);
REC_FUNC(DSRA);
REC_FUNC(DSLL32);
REC_FUNC(DSRL32);
REC_FUNC(DSRA32);
REC_FUNC(SLLV);
REC_FUNC(SRLV);
REC_FUNC(SRAV);
REC_FUNC(DSLLV);
REC_FUNC(DSRLV);
REC_FUNC(DSRAV);
#else

// Operand helpers. Loads return the register holding the operand, coherent by
// construction: a pin mirror when the guest reg is pinned, an allocator-
// resident slot once residency is flipped on, else the scratch (memory load).
// Callers must treat the returned register as READ-ONLY and put results in
// scratch (or pass them straight to the store helper).
static a64::Register memLoadS32()
{
	return _eeGetGPRSourceReg(RWARG1, _Rs_);
}

static a64::Register memLoadS64()
{
	return _eeGetGPRSourceReg(RXARG1, _Rs_);
}

static a64::Register memLoadT32()
{
	return _eeGetGPRSourceReg(RWSCRATCH, _Rt_);
}

static a64::Register memLoadT64()
{
	return _eeGetGPRSourceReg(RXSCRATCH, _Rt_);
}

static void memStoreD(const a64::Register& src)
{
	_eeStoreGPRDestReg(_Rd_, src);
}

// Dest home for the shift result: the FINAL result-producing instruction
// targets it, then memStoreD deposits (a pin/memory store, or a deferred
// allocator-slot store under the flip). See _eeGetGPRDestReg's contract —
// intermediates stay in scratch. alloc_if_used=false keeps Phase-1 behavior.
static a64::Register memDestD()
{
	return _eeGetGPRDestReg(_Rd_, RXSCRATCH);
}

/*********************************************************
 * Shift with constant amount — rd = rt SHIFT sa         *
 * Uses eeRecompileCodeRC2_MEM                            *
 *********************************************************/

//// SLL — rd = sign_extend(rt << sa)
static void recSLL_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] << _Sa_);
}

static void recSLL_(int info)
{
	const a64::Register rt = memLoadT32();
	const a64::Register dst = memDestD();
	if (_Sa_ != 0)
	{
		// Sbfiz(sa, 32-sa) == sign_extend((s32)(rt << sa)) in ONE insn (GE-06):
		// source bits [31-sa:0] land at [31:sa], sign-extended from bit 31.
		// Reads only the low 32 source bits, so pin upper halves are inert.
		armAsm->Sbfiz(dst, rt.X(), _Sa_, 32 - _Sa_);
	}
	else
	{
		armAsm->Sxtw(dst, rt);
	}
	memStoreD(dst);
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC2_MEM, SLL, XMMINFO_WRITED | XMMINFO_READT);

//// SRL — rd = sign_extend(rt >> sa) (logical)
static void recSRL_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] >> _Sa_);
}

static void recSRL_(int info)
{
	const a64::Register rt = memLoadT32();
	const a64::Register dst = memDestD();
	if (_Sa_ != 0)
	{
		// sa>0 clears bit 31 of the 32-bit result, so zero-extension IS the
		// MIPS sign-extension: Ubfx(sa, 32-sa) in ONE insn (GE-06).
		armAsm->Ubfx(dst, rt.X(), _Sa_, 32 - _Sa_);
	}
	else
	{
		armAsm->Sxtw(dst, rt);
	}
	memStoreD(dst);
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC2_MEM, SRL, XMMINFO_WRITED | XMMINFO_READT);

//// SRA — rd = sign_extend(rt >> sa) (arithmetic)
static void recSRA_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].SL[0] >> _Sa_);
}

static void recSRA_(int info)
{
	const a64::Register rt = memLoadT32();
	const a64::Register dst = memDestD();
	if (_Sa_ != 0)
	{
		// Sbfx(sa, 32-sa) == sign_extend((s32)rt >> sa) in ONE insn (GE-06).
		armAsm->Sbfx(dst, rt.X(), _Sa_, 32 - _Sa_);
	}
	else
	{
		armAsm->Sxtw(dst, rt);
	}
	memStoreD(dst);
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC2_MEM, SRA, XMMINFO_WRITED | XMMINFO_READT);

//// DSLL — rd = rt << sa (64-bit)
static void recDSLL_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rt_].UD[0] << _Sa_;
}

static void recDSLL_(int info)
{
	const a64::Register rt = memLoadT64();
	if (_Sa_ != 0)
	{
		const a64::Register dst = memDestD();
		armAsm->Lsl(dst, rt, _Sa_);
		memStoreD(dst);
	}
	else
	{
		memStoreD(rt);
	}
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC2_MEM, DSLL, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

//// DSRL
static void recDSRL_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rt_].UD[0] >> _Sa_;
}

static void recDSRL_(int info)
{
	const a64::Register rt = memLoadT64();
	if (_Sa_ != 0)
	{
		const a64::Register dst = memDestD();
		armAsm->Lsr(dst, rt, _Sa_);
		memStoreD(dst);
	}
	else
	{
		memStoreD(rt);
	}
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC2_MEM, DSRL, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

//// DSRA
static void recDSRA_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = g_cpuConstRegs[_Rt_].SD[0] >> _Sa_;
}

static void recDSRA_(int info)
{
	const a64::Register rt = memLoadT64();
	if (_Sa_ != 0)
	{
		const a64::Register dst = memDestD();
		armAsm->Asr(dst, rt, _Sa_);
		memStoreD(dst);
	}
	else
	{
		memStoreD(rt);
	}
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC2_MEM, DSRA, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

//// DSLL32 — rd = rt << (sa + 32)
static void recDSLL32_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rt_].UD[0] << (_Sa_ + 32);
}

static void recDSLL32_(int info)
{
	const a64::Register rt = memLoadT64();
	const a64::Register dst = memDestD();
	armAsm->Lsl(dst, rt, _Sa_ + 32);
	memStoreD(dst);
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC2_MEM, DSLL32, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

//// DSRL32
static void recDSRL32_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rt_].UD[0] >> (_Sa_ + 32);
}

static void recDSRL32_(int info)
{
	const a64::Register rt = memLoadT64();
	const a64::Register dst = memDestD();
	armAsm->Lsr(dst, rt, _Sa_ + 32);
	memStoreD(dst);
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC2_MEM, DSRL32, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

//// DSRA32
static void recDSRA32_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = g_cpuConstRegs[_Rt_].SD[0] >> (_Sa_ + 32);
}

static void recDSRA32_(int info)
{
	const a64::Register rt = memLoadT64();
	const a64::Register dst = memDestD();
	armAsm->Asr(dst, rt, _Sa_ + 32);
	memStoreD(dst);
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC2_MEM, DSRA32, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

/*********************************************************
 * Variable shifts — rd = rt SHIFT rs                    *
 * Uses eeRecompileCodeRC0_MEM                            *
 *********************************************************/

//// SLLV — rd = sign_extend((rt << rs[4:0]))
static void recSLLV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] << (g_cpuConstRegs[_Rs_].UL[0] & 0x1f));
}

static void recSLLV_consts(int info)
{
	const a64::Register rt = memLoadT32();
	const a64::Register dst = memDestD();
	u32 sa = g_cpuConstRegs[_Rs_].UL[0] & 0x1f;
	if (sa != 0)
	{
		armAsm->Lsl(RWSCRATCH, rt, sa);
		armAsm->Sxtw(dst, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(dst, rt);
	}
	memStoreD(dst);
}

static void recSLLV_constt(int info)
{
	const a64::Register rs = memLoadS32();
	const a64::Register dst = memDestD();
	armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].UL[0]);
	armAsm->Lsl(RWSCRATCH, RWSCRATCH, rs);
	armAsm->Sxtw(dst, RWSCRATCH);
	memStoreD(dst);
}

static void recSLLV_(int info)
{
	const a64::Register rs = memLoadS32();
	const a64::Register rt = memLoadT32();
	const a64::Register dst = memDestD();
	armAsm->Lsl(RWSCRATCH, rt, rs);
	armAsm->Sxtw(dst, RWSCRATCH);
	memStoreD(dst);
}

EERECOMPILE_CODERC0_MEM(SLLV, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

//// SRLV
static void recSRLV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x1f));
}

static void recSRLV_consts(int info)
{
	const a64::Register rt = memLoadT32();
	const a64::Register dst = memDestD();
	u32 sa = g_cpuConstRegs[_Rs_].UL[0] & 0x1f;
	if (sa != 0)
	{
		armAsm->Lsr(RWSCRATCH, rt, sa);
		armAsm->Sxtw(dst, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(dst, rt);
	}
	memStoreD(dst);
}

static void recSRLV_constt(int info)
{
	const a64::Register rs = memLoadS32();
	const a64::Register dst = memDestD();
	armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].UL[0]);
	armAsm->Lsr(RWSCRATCH, RWSCRATCH, rs);
	armAsm->Sxtw(dst, RWSCRATCH);
	memStoreD(dst);
}

static void recSRLV_(int info)
{
	const a64::Register rs = memLoadS32();
	const a64::Register rt = memLoadT32();
	const a64::Register dst = memDestD();
	armAsm->Lsr(RWSCRATCH, rt, rs);
	armAsm->Sxtw(dst, RWSCRATCH);
	memStoreD(dst);
}

EERECOMPILE_CODERC0_MEM(SRLV, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

//// SRAV
static void recSRAV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].SL[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x1f));
}

static void recSRAV_consts(int info)
{
	const a64::Register rt = memLoadT32();
	const a64::Register dst = memDestD();
	u32 sa = g_cpuConstRegs[_Rs_].UL[0] & 0x1f;
	if (sa != 0)
	{
		armAsm->Asr(RWSCRATCH, rt, sa);
		armAsm->Sxtw(dst, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(dst, rt);
	}
	memStoreD(dst);
}

static void recSRAV_constt(int info)
{
	const a64::Register rs = memLoadS32();
	const a64::Register dst = memDestD();
	armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].SL[0]);
	armAsm->Asr(RWSCRATCH, RWSCRATCH, rs);
	armAsm->Sxtw(dst, RWSCRATCH);
	memStoreD(dst);
}

static void recSRAV_(int info)
{
	const a64::Register rs = memLoadS32();
	const a64::Register rt = memLoadT32();
	const a64::Register dst = memDestD();
	armAsm->Asr(RWSCRATCH, rt, rs);
	armAsm->Sxtw(dst, RWSCRATCH);
	memStoreD(dst);
}

EERECOMPILE_CODERC0_MEM(SRAV, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

//// DSLLV — rd = rt << rs[5:0] (64-bit)
static void recDSLLV_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rt_].UD[0] << (g_cpuConstRegs[_Rs_].UL[0] & 0x3f);
}

static void recDSLLV_consts(int info)
{
	const a64::Register rt = memLoadT64();
	u32 sa = g_cpuConstRegs[_Rs_].UL[0] & 0x3f;
	if (sa != 0)
	{
		const a64::Register dst = memDestD();
		armAsm->Lsl(dst, rt, sa);
		memStoreD(dst);
	}
	else
	{
		memStoreD(rt);
	}
}

static void recDSLLV_constt(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register dst = memDestD();
	armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rt_].UD[0]);
	armAsm->Lsl(dst, RXSCRATCH, rs);
	memStoreD(dst);
}

static void recDSLLV_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	const a64::Register dst = memDestD();
	armAsm->Lsl(dst, rt, rs);
	memStoreD(dst);
}

EERECOMPILE_CODERC0_MEM(DSLLV, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// DSRLV
static void recDSRLV_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rt_].UD[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x3f);
}

static void recDSRLV_consts(int info)
{
	const a64::Register rt = memLoadT64();
	u32 sa = g_cpuConstRegs[_Rs_].UL[0] & 0x3f;
	if (sa != 0)
	{
		const a64::Register dst = memDestD();
		armAsm->Lsr(dst, rt, sa);
		memStoreD(dst);
	}
	else
	{
		memStoreD(rt);
	}
}

static void recDSRLV_constt(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register dst = memDestD();
	armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rt_].UD[0]);
	armAsm->Lsr(dst, RXSCRATCH, rs);
	memStoreD(dst);
}

static void recDSRLV_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	const a64::Register dst = memDestD();
	armAsm->Lsr(dst, rt, rs);
	memStoreD(dst);
}

EERECOMPILE_CODERC0_MEM(DSRLV, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// DSRAV
static void recDSRAV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = g_cpuConstRegs[_Rt_].SD[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x3f);
}

static void recDSRAV_consts(int info)
{
	const a64::Register rt = memLoadT64();
	u32 sa = g_cpuConstRegs[_Rs_].UL[0] & 0x3f;
	if (sa != 0)
	{
		const a64::Register dst = memDestD();
		armAsm->Asr(dst, rt, sa);
		memStoreD(dst);
	}
	else
	{
		memStoreD(rt);
	}
}

static void recDSRAV_constt(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register dst = memDestD();
	armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rt_].SD[0]);
	armAsm->Asr(dst, RXSCRATCH, rs);
	memStoreD(dst);
}

static void recDSRAV_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	const a64::Register dst = memDestD();
	armAsm->Asr(dst, rt, rs);
	memStoreD(dst);
}

EERECOMPILE_CODERC0_MEM(DSRAV, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

#endif // !FORCE_INTERP_SHIFT

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
