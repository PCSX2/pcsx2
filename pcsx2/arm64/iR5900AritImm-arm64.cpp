// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
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

// Operand helpers. Loads return the register holding the operand, coherent by
// construction: a pin mirror when the guest reg is pinned, an allocator-
// resident slot once residency is flipped on, else the scratch (memory load).
// Callers must treat the returned register as READ-ONLY and put results in
// scratch (or pass them straight to the store helper).
static a64::Register memLoadS32()
{
	return _eeGetGPRSourceReg(RWSCRATCH, _Rs_);
}
static a64::Register memLoadS64()
{
	return _eeGetGPRSourceReg(RXSCRATCH, _Rs_);
}
static void memStoreT(const a64::Register& src) { _eeStoreGPRDestReg(_Rt_, src); }
// Dest home for the result: the FINAL result-producing instruction targets it,
// then memStoreT deposits (a pin/memory store, or a deferred allocator-slot
// store under the flip). See _eeGetGPRDestReg's contract — intermediates stay
// in scratch. alloc_if_used=false keeps Phase-1 behavior.
static a64::Register memDestT() { return _eeGetGPRDestReg(_Rt_, RXSCRATCH); }

//// ADDI / ADDIU — rt = sign_extend(rs + imm)
static void recADDI_const()
{
	g_cpuConstRegs[_Rt_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] + u32(s32(_Imm_))));
}

static void recADDI_(int info)
{
	const a64::Register rs = memLoadS32();
	const a64::Register dst = memDestT();
	if (_Imm_ != 0)
	{
		armAsm->Add(RWSCRATCH, rs, _Imm_);
		armAsm->Sxtw(dst, RWSCRATCH);
	}
	else
	{
		armAsm->Sxtw(dst, rs);
	}
	memStoreT(dst);
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
	const a64::Register rs = memLoadS64();
	if (_Imm_ != 0)
	{
		// vixl's Add(int64_t) picks the right ADD/SUB-imm encoding and
		// materializes via x16 when the immediate is unencodable.
		const a64::Register dst = memDestT();
		armAsm->Add(dst, rs, static_cast<int64_t>(static_cast<s32>(_Imm_)));
		memStoreT(dst);
	}
	else
	{
		memStoreT(rs);
	}
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
	if (_ImmU_ == 0)
	{
		memStoreT(a64::xzr);
		return;
	}
	const a64::Register rs = memLoadS64();
	const a64::Register dst = memDestT();
	armAsm->And(dst, rs, static_cast<uint64_t>(static_cast<u16>(_ImmU_)));
	memStoreT(dst);
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC1_MEM, ANDI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

//// ORI — rt = rs | zero_extend(imm16)
static void recORI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] | (u64)(u16)_ImmU_;
}

static void recORI_(int info)
{
	const a64::Register rs = memLoadS64();
	if (_ImmU_ != 0)
	{
		const a64::Register dst = memDestT();
		armAsm->Orr(dst, rs, static_cast<uint64_t>(static_cast<u16>(_ImmU_)));
		memStoreT(dst);
	}
	else
	{
		memStoreT(rs);
	}
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC1_MEM, ORI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

//// XORI — rt = rs ^ zero_extend(imm16)
static void recXORI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] ^ (u64)(u16)_ImmU_;
}

static void recXORI_(int info)
{
	const a64::Register rs = memLoadS64();
	if (_ImmU_ != 0)
	{
		const a64::Register dst = memDestT();
		armAsm->Eor(dst, rs, static_cast<uint64_t>(static_cast<u16>(_ImmU_)));
		memStoreT(dst);
	}
	else
	{
		memStoreT(rs);
	}
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC1_MEM, XORI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

//// SLTI — rt = (rs < sign_extend(imm)) ? 1 : 0 (signed)
static void recSLTI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = (g_cpuConstRegs[_Rs_].SD[0] < (s64)(s32)_Imm_) ? 1 : 0;
}

static void recSLTI_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register dst = memDestT();
	armAsm->Cmp(rs, static_cast<int64_t>(static_cast<s32>(_Imm_)));
	armAsm->Cset(dst, a64::lt);
	memStoreT(dst);
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC1_MEM, SLTI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

//// SLTIU — rt = (rs < sign_extend(imm)) ? 1 : 0 (unsigned)
static void recSLTIU_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = (g_cpuConstRegs[_Rs_].UD[0] < (u64)(s64)(s32)_Imm_) ? 1 : 0;
}

static void recSLTIU_(int info)
{
	const a64::Register rs = memLoadS64();
	const a64::Register dst = memDestT();
	// Sign-extended imm — Cmp condition flags are signedness-agnostic; only
	// the Cset (lo = unsigned-less-than) differs from SLTI.
	armAsm->Cmp(rs, static_cast<int64_t>(static_cast<s32>(_Imm_)));
	armAsm->Cset(dst, a64::lo);
	memStoreT(dst);
}

EERECOMPILE_CODEX_MEM(eeRecompileCodeRC1_MEM, SLTIU, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

#endif // !FORCE_INTERP_ARITIMM

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
