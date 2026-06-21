// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE ALU Immediate Instruction Codegen — memory-based
// rt = rs OP imm16. All operands via cpuRegs memory.

#include "arm64/iR5900-arm64.h"

namespace a64 = vixl::aarch64;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

namespace Interp = R5900::Interpreter::OpcodeImpl;

#ifdef FORCE_INTERP_ARITIMM
REC_FUNC(ADDI);
void recADDIU() { recADDI(); }
REC_FUNC(DADDI);
void recDADDIU() { recDADDI(); }
REC_FUNC(ANDI);
REC_FUNC(ORI);
REC_FUNC(XORI);
REC_FUNC(SLTI);
REC_FUNC(SLTIU);
#else

// Memory load/store helpers
static void memLoadS32() { armLoadEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rs_].UL[0]); }
static void memLoadS64() { armLoadEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rs_].UD[0]); }
static void memStoreT() { armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rt_].UD[0]); }

//// ADDI / ADDIU — rt = sign_extend(rs + imm)
static void recADDI_const()
{
	g_cpuConstRegs[_Rt_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] + u32(s32(_Imm_))));
}

static void recADDI_(int info)
{
	memLoadS32();
	if (_Imm_ != 0)
		armAsm->Add(RWSCRATCH, RWSCRATCH, _Imm_);
	armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
	memStoreT();
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC1_MEM, ADDI, XMMINFO_WRITET | XMMINFO_READS);

void recADDIU() { recADDI(); }

//// DADDI / DADDIU — rt = rs + sign_extend(imm)
static void recDADDI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] + u64(s64(_Imm_));
}

static void recDADDI_(int info)
{
	memLoadS64();
	if (_Imm_ != 0)
	{
		// vixl's Add(int64_t) picks the right ADD/SUB-imm encoding and
		// materializes via x16 when the immediate is unencodable.
		armAsm->Add(RXSCRATCH, RXSCRATCH, static_cast<int64_t>(static_cast<s32>(_Imm_)));
	}
	memStoreT();
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC1_MEM, DADDI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

void recDADDIU() { recDADDI(); }

//// ANDI — rt = rs & zero_extend(imm16)
static void recANDI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] & (u64)(u16)_ImmU_;
}

static void recANDI_(int info)
{
	memLoadS64();
	if (_ImmU_ == 0)
		armAsm->Mov(RXSCRATCH, 0);
	else
		armAsm->And(RXSCRATCH, RXSCRATCH, static_cast<uint64_t>(static_cast<u16>(_ImmU_)));
	memStoreT();
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC1_MEM, ANDI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

//// ORI — rt = rs | zero_extend(imm16)
static void recORI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] | (u64)(u16)_ImmU_;
}

static void recORI_(int info)
{
	memLoadS64();
	if (_ImmU_ != 0)
		armAsm->Orr(RXSCRATCH, RXSCRATCH, static_cast<uint64_t>(static_cast<u16>(_ImmU_)));
	memStoreT();
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC1_MEM, ORI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

//// XORI — rt = rs ^ zero_extend(imm16)
static void recXORI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] ^ (u64)(u16)_ImmU_;
}

static void recXORI_(int info)
{
	memLoadS64();
	if (_ImmU_ != 0)
		armAsm->Eor(RXSCRATCH, RXSCRATCH, static_cast<uint64_t>(static_cast<u16>(_ImmU_)));
	memStoreT();
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC1_MEM, XORI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

//// SLTI — rt = (rs < sign_extend(imm)) ? 1 : 0 (signed)
static void recSLTI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = (g_cpuConstRegs[_Rs_].SD[0] < (s64)(s32)_Imm_) ? 1 : 0;
}

static void recSLTI_(int info)
{
	memLoadS64();
	armAsm->Cmp(RXSCRATCH, static_cast<int64_t>(static_cast<s32>(_Imm_)));
	armAsm->Cset(RXSCRATCH, a64::lt);
	memStoreT();
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC1_MEM, SLTI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

//// SLTIU — rt = (rs < sign_extend(imm)) ? 1 : 0 (unsigned)
static void recSLTIU_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = (g_cpuConstRegs[_Rs_].UD[0] < (u64)(s64)(s32)_Imm_) ? 1 : 0;
}

static void recSLTIU_(int info)
{
	memLoadS64();
	// Sign-extended imm — Cmp condition flags are signedness-agnostic; only
	// the Cset (lo = unsigned-less-than) differs from SLTI.
	armAsm->Cmp(RXSCRATCH, static_cast<int64_t>(static_cast<s32>(_Imm_)));
	armAsm->Cset(RXSCRATCH, a64::lo);
	memStoreT();
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC1_MEM, SLTIU, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

#endif // !FORCE_INTERP_ARITIMM

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
