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

static a64::Register memLoadS64()
{
	if (const a64::Register* pin = armEEPinForGPR(_Rs_))
		return *pin;
	armLoadEERegPtr(RXARG1, &cpuRegs.GPR.r[_Rs_].UD[0]);
	return RXARG1;
}

static a64::Register memLoadT32()
{
	if (const a64::Register* pin = armEEPinForGPR(_Rt_))
		return pin->W();
	armLoadEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rt_].UL[0]);
	return RWSCRATCH;
}

static a64::Register memLoadT64()
{
	if (const a64::Register* pin = armEEPinForGPR(_Rt_))
		return *pin;
	armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rt_].UD[0]);
	return RXSCRATCH;
}

static void memStoreD(const a64::Register& src)
{
	armStoreEERegPtr(src, &cpuRegs.GPR.r[_Rd_].UD[0]);
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
	if (_Sa_ != 0)
	{
		armAsm->Lsl(RWSCRATCH, rt, _Sa_);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(RXSCRATCH, rt);
	}
	memStoreD(RXSCRATCH);
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
	if (_Sa_ != 0)
	{
		armAsm->Lsr(RWSCRATCH, rt, _Sa_);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(RXSCRATCH, rt);
	}
	memStoreD(RXSCRATCH);
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
	if (_Sa_ != 0)
	{
		armAsm->Asr(RWSCRATCH, rt, _Sa_);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(RXSCRATCH, rt);
	}
	memStoreD(RXSCRATCH);
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
		armAsm->Lsl(RXSCRATCH, rt, _Sa_);
		memStoreD(RXSCRATCH);
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
		armAsm->Lsr(RXSCRATCH, rt, _Sa_);
		memStoreD(RXSCRATCH);
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
		armAsm->Asr(RXSCRATCH, rt, _Sa_);
		memStoreD(RXSCRATCH);
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
	armAsm->Lsl(RXSCRATCH, rt, _Sa_ + 32);
	memStoreD(RXSCRATCH);
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
	armAsm->Lsr(RXSCRATCH, rt, _Sa_ + 32);
	memStoreD(RXSCRATCH);
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
	armAsm->Asr(RXSCRATCH, rt, _Sa_ + 32);
	memStoreD(RXSCRATCH);
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
	u32 sa = g_cpuConstRegs[_Rs_].UL[0] & 0x1f;
	if (sa != 0)
	{
		armAsm->Lsl(RWSCRATCH, rt, sa);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(RXSCRATCH, rt);
	}
	memStoreD(RXSCRATCH);
}

static void recSLLV_constt(int info)
{
	const a64::Register rs = memLoadS32();
	armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].UL[0]);
	armAsm->Lsl(RWSCRATCH, RWSCRATCH, rs);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreD(RXSCRATCH);
}

static void recSLLV_(int info)
{
	const a64::Register rs = memLoadS32();
	const a64::Register rt = memLoadT32();
	armAsm->Lsl(RWSCRATCH, rt, rs);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreD(RXSCRATCH);
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
	u32 sa = g_cpuConstRegs[_Rs_].UL[0] & 0x1f;
	if (sa != 0)
	{
		armAsm->Lsr(RWSCRATCH, rt, sa);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(RXSCRATCH, rt);
	}
	memStoreD(RXSCRATCH);
}

static void recSRLV_constt(int info)
{
	const a64::Register rs = memLoadS32();
	armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].UL[0]);
	armAsm->Lsr(RWSCRATCH, RWSCRATCH, rs);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreD(RXSCRATCH);
}

static void recSRLV_(int info)
{
	const a64::Register rs = memLoadS32();
	const a64::Register rt = memLoadT32();
	armAsm->Lsr(RWSCRATCH, rt, rs);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreD(RXSCRATCH);
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
	u32 sa = g_cpuConstRegs[_Rs_].UL[0] & 0x1f;
	if (sa != 0)
	{
		armAsm->Asr(RWSCRATCH, rt, sa);
		armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(RXSCRATCH, rt);
	}
	memStoreD(RXSCRATCH);
}

static void recSRAV_constt(int info)
{
	const a64::Register rs = memLoadS32();
	armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].SL[0]);
	armAsm->Asr(RWSCRATCH, RWSCRATCH, rs);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreD(RXSCRATCH);
}

static void recSRAV_(int info)
{
	const a64::Register rs = memLoadS32();
	const a64::Register rt = memLoadT32();
	armAsm->Asr(RWSCRATCH, rt, rs);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreD(RXSCRATCH);
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
		armAsm->Lsl(RXSCRATCH, rt, sa);
		memStoreD(RXSCRATCH);
	}
	else
	{
		memStoreD(rt);
	}
}

static void recDSLLV_constt(int info)
{
	const a64::Register rs = memLoadS64();
	armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rt_].UD[0]);
	armAsm->Lsl(RXSCRATCH, RXSCRATCH, rs);
	memStoreD(RXSCRATCH);
}

static void recDSLLV_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	armAsm->Lsl(RXSCRATCH, rt, rs);
	memStoreD(RXSCRATCH);
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
		armAsm->Lsr(RXSCRATCH, rt, sa);
		memStoreD(RXSCRATCH);
	}
	else
	{
		memStoreD(rt);
	}
}

static void recDSRLV_constt(int info)
{
	const a64::Register rs = memLoadS64();
	armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rt_].UD[0]);
	armAsm->Lsr(RXSCRATCH, RXSCRATCH, rs);
	memStoreD(RXSCRATCH);
}

static void recDSRLV_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	armAsm->Lsr(RXSCRATCH, rt, rs);
	memStoreD(RXSCRATCH);
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
		armAsm->Asr(RXSCRATCH, rt, sa);
		memStoreD(RXSCRATCH);
	}
	else
	{
		memStoreD(rt);
	}
}

static void recDSRAV_constt(int info)
{
	const a64::Register rs = memLoadS64();
	armAsm->Mov(RXSCRATCH, g_cpuConstRegs[_Rt_].SD[0]);
	armAsm->Asr(RXSCRATCH, RXSCRATCH, rs);
	memStoreD(RXSCRATCH);
}

static void recDSRAV_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register rt = memLoadT64();
	armAsm->Asr(RXSCRATCH, rt, rs);
	memStoreD(RXSCRATCH);
}

EERECOMPILE_CODERC0_MEM(DSRAV, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

#endif // !FORCE_INTERP_SHIFT

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
