// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 IOP Opcode Implementations
// ALU/shift/mult-div/move/load-store/LWL-LWR/branch/COP0 have native codegen;
// only GTE (COP2) ops fall back to the interpreter via REC_GTE_FUNC.

#include "arm64/iR3000A-arm64.h"
#include "arm64/AsmHelpers.h"
#include "IopMem.h"
#include "IopDma.h" // also declares iopTestIntc()
#include "IopGte.h"

#include "common/Assertions.h"
#include "common/Console.h"

namespace a64 = vixl::aarch64;

extern int g_psxWriteOk;
extern u32 g_psxMaxRecMem;

// IOP interpreter function declarations (defined in R3000AOpcodeTables.cpp)
extern void psxADDI();  extern void psxADDIU(); extern void psxSLTI();  extern void psxSLTIU();
extern void psxANDI();  extern void psxORI();   extern void psxXORI();  extern void psxLUI();
extern void psxADD();   extern void psxADDU();  extern void psxSUB();   extern void psxSUBU();
extern void psxAND();   extern void psxOR();    extern void psxXOR();   extern void psxNOR();
extern void psxSLT();   extern void psxSLTU();
extern void psxSLL();   extern void psxSRL();   extern void psxSRA();
extern void psxSLLV();  extern void psxSRLV();  extern void psxSRAV();
extern void psxMULT();  extern void psxMULTU(); extern void psxDIV();   extern void psxDIVU();
extern void psxMFHI();  extern void psxMTHI();  extern void psxMFLO();  extern void psxMTLO();
extern void psxLB();    extern void psxLH();    extern void psxLW();
extern void psxLBU();   extern void psxLHU();   extern void psxLWL();   extern void psxLWR();
extern void psxSB();    extern void psxSH();    extern void psxSW();
extern void psxSWL();   extern void psxSWR();
extern void psxMFC0();  extern void psxMTC0();  extern void psxCFC0();  extern void psxCTC0();
extern void psxRFE();

// GTE functions (defined in IopGte.cpp)
extern void gteMFC2();  extern void gteMTC2();  extern void gteCFC2();  extern void gteCTC2();
extern void gteLWC2();  extern void gteSWC2();
extern void gteRTPS();  extern void gteNCLIP(); extern void gteOP();
extern void gteDPCS();  extern void gteINTPL(); extern void gteMVMVA(); extern void gteNCDS();
extern void gteCDP();   extern void gteNCDT();  extern void gteNCCS();  extern void gteCC();
extern void gteNCS();   extern void gteNCT();   extern void gteSQR();   extern void gteDCPL();
extern void gteDPCT();  extern void gteAVSZ3(); extern void gteAVSZ4(); extern void gteRTPT();
extern void gteGPF();   extern void gteGPL();   extern void gteNCCT();

////////////////////////////////////////////////////////////////////
// Interpreter Fallback Macro

#define REC_FUNC(f) \
	static void rpsx##f() \
	{ \
		armAsm->Mov(RWSCRATCH, (u32)psxRegs.code); \
		armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.code)); \
		_psxFlushCall(FLUSH_EVERYTHING); \
		armEmitCall((void*)(uptr)psx##f); \
		PSX_DEL_CONST(_Rt_); \
	}

#define REC_GTE_FUNC(f) \
	static void rgte##f() \
	{ \
		armAsm->Mov(RWSCRATCH, (u32)psxRegs.code); \
		armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.code)); \
		_psxFlushCall(FLUSH_EVERYTHING); \
		armEmitCall((void*)(uptr)gte##f); \
	}

////////////////////////////////////////////////////////////////////
// ALU Immediate Instructions — rt = rs op imm16

////////////////////////////////////////////////////////////////////
// ALU Instructions — allocator-aware native codegen with const propagation
//
// These keep results live in allocated host regs across IOP instruction
// boundaries, flushing only at block end / before C calls.
// This mirrors the proven PSX_REG_OP path below.

// Allocator-aware 2-op IOP immediate macro: rt = rs OP imm.
// Const-folds when Rs is const; otherwise allocates Rs (MODE_READ) and Rt
// (MODE_WRITE) in host GPRs and emits codeGen, which sees host-reg indices
// rs (Rs) and rt (Rt). The result is left live in the allocator.
#define PSX_IMM_OP(name, constExpr, codeGen) \
	static void rpsx##name() \
	{ \
		if (!_Rt_) \
		{ \
			/* IRX module-import HLE trampoline marker: ADDIU $0,$0,idx encodes as */ \
			/* opcode 0x2400xxxx — the only I-type imm op whose top half is 0x2400. */ \
			if ((psxRegs.code >> 16) == 0x2400) \
				psxRecompileIrxImport(); \
			return; \
		} \
		if (PSX_IS_CONST1(_Rs_)) \
		{ \
			const u32 result = (constExpr); \
			_psxDeleteReg(_Rt_, 0); \
			PSX_SET_CONST(_Rt_); \
			g_psxConstRegs[_Rt_] = result; \
			return; \
		} \
		_addNeededPSXtoArm64GPR(_Rs_); \
		_addNeededPSXtoArm64GPR(_Rt_); \
		const int rs = _allocArm64GPR(ARM64TYPE_PSX, _Rs_, MODE_READ); \
		const int rt = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_WRITE); \
		codeGen; \
		_clearNeededArm64GPRregs(); \
		PSX_DEL_CONST(_Rt_); \
	}

// PS2 IOP doesn't trap signed overflow, so ADDI is an exact alias of ADDIU.
PSX_IMM_OP(ADDIU, g_psxConstRegs[_Rs_] + _Imm_,
	{ armAsm->Add(armWRegister(rt), armWRegister(rs), static_cast<s64>(static_cast<s32>(_Imm_))); })
static void rpsxADDI() { rpsxADDIU(); }
PSX_IMM_OP(ANDI, g_psxConstRegs[_Rs_] & _ImmU_,
	{ armAsm->And(armWRegister(rt), armWRegister(rs), static_cast<u64>(_ImmU_)); })
PSX_IMM_OP(ORI, g_psxConstRegs[_Rs_] | _ImmU_,
	{ armAsm->Orr(armWRegister(rt), armWRegister(rs), static_cast<u64>(_ImmU_)); })
PSX_IMM_OP(XORI, g_psxConstRegs[_Rs_] ^ _ImmU_,
	{ armAsm->Eor(armWRegister(rt), armWRegister(rs), static_cast<u64>(_ImmU_)); })
PSX_IMM_OP(SLTI, ((s32)g_psxConstRegs[_Rs_] < (s32)_Imm_) ? 1u : 0u,
	{ armAsm->Cmp(armWRegister(rs), static_cast<s64>(static_cast<s32>(_Imm_))); armAsm->Cset(armWRegister(rt), a64::lt); })
PSX_IMM_OP(SLTIU, (g_psxConstRegs[_Rs_] < (u32)(s32)_Imm_) ? 1u : 0u,
	{ armAsm->Cmp(armWRegister(rs), static_cast<s64>(static_cast<s32>(_Imm_))); armAsm->Cset(armWRegister(rt), a64::lo); })

static void rpsxLUI()
{
	if (!_Rt_) return;
	_psxDeleteReg(_Rt_, 0);
	PSX_SET_CONST(_Rt_);
	g_psxConstRegs[_Rt_] = psxRegs.code << 16;
}

// Allocator-aware 3-op IOP macro: rd = rs OP rt.
//
// Const-folds when both Rs and Rt are const (matches x86
// PSXRECOMPILE_CONSTCODE0); otherwise allocates Rd in a host GPR and uses
// `codeGen` to emit one of three branches:
//   rs_const  → emit `op(rd, rt, const_Rs)` (or per-op flipped form)
//   rt_const  → emit `op(rd, rs, const_Rt)`
//   else      → emit `op(rd, rs, rt)`
// The codeGen block sees five locals: bool rs_const, rt_const, int rs, rt, rd
// (rs/rt are -1 when their const fast-path is taken).
#define PSX_REG_OP(name, foldExpr, codeGen) \
	static void rpsx##name() \
	{ \
		if (!_Rd_) return; \
		if (PSX_IS_CONST2(_Rs_, _Rt_)) \
		{ \
			_psxDeleteReg(_Rd_, 0); \
			PSX_SET_CONST(_Rd_); \
			g_psxConstRegs[_Rd_] = (foldExpr); \
			return; \
		} \
		_addNeededPSXtoArm64GPR(_Rs_); \
		_addNeededPSXtoArm64GPR(_Rt_); \
		_addNeededPSXtoArm64GPR(_Rd_); \
		const bool rs_const = PSX_IS_CONST1(_Rs_); \
		const bool rt_const = PSX_IS_CONST1(_Rt_); \
		const int rs = rs_const ? -1 : _allocArm64GPR(ARM64TYPE_PSX, _Rs_, MODE_READ); \
		const int rt = rt_const ? -1 : _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_READ); \
		const int rd = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE); \
		(void)rs; (void)rt; \
		codeGen; \
		_clearNeededArm64GPRregs(); \
		PSX_DEL_CONST(_Rd_); \
	}

PSX_REG_OP(ADD,
	g_psxConstRegs[_Rs_] + g_psxConstRegs[_Rt_],
	{
		if (rs_const)
			armAsm->Add(armWRegister(rd), armWRegister(rt), static_cast<s64>(static_cast<s32>(g_psxConstRegs[_Rs_])));
		else if (rt_const)
			armAsm->Add(armWRegister(rd), armWRegister(rs), static_cast<s64>(static_cast<s32>(g_psxConstRegs[_Rt_])));
		else
			armAsm->Add(armWRegister(rd), armWRegister(rs), armWRegister(rt));
	})

PSX_REG_OP(ADDU,
	g_psxConstRegs[_Rs_] + g_psxConstRegs[_Rt_],
	{
		if (rs_const)
			armAsm->Add(armWRegister(rd), armWRegister(rt), static_cast<s64>(static_cast<s32>(g_psxConstRegs[_Rs_])));
		else if (rt_const)
			armAsm->Add(armWRegister(rd), armWRegister(rs), static_cast<s64>(static_cast<s32>(g_psxConstRegs[_Rt_])));
		else
			armAsm->Add(armWRegister(rd), armWRegister(rs), armWRegister(rt));
	})

PSX_REG_OP(SUB,
	g_psxConstRegs[_Rs_] - g_psxConstRegs[_Rt_],
	{
		if (_Rs_ == _Rt_) // Rs - Rs == 0
			armAsm->Mov(armWRegister(rd), 0);
		else if (rs_const)
		{
			const u32 cv = g_psxConstRegs[_Rs_];
			if (cv == 0)
				armAsm->Neg(armWRegister(rd), armWRegister(rt));
			else
			{
				// rd may alias rt (Rd == Rt); materialize the const minuend in a
				// non-allocatable scratch so the Mov can't clobber the subtrahend.
				armAsm->Mov(RWSCRATCH, cv);
				armAsm->Sub(armWRegister(rd), RWSCRATCH, armWRegister(rt));
			}
		}
		else if (rt_const)
			armAsm->Sub(armWRegister(rd), armWRegister(rs), static_cast<s64>(static_cast<s32>(g_psxConstRegs[_Rt_])));
		else
			armAsm->Sub(armWRegister(rd), armWRegister(rs), armWRegister(rt));
	})

PSX_REG_OP(SUBU,
	g_psxConstRegs[_Rs_] - g_psxConstRegs[_Rt_],
	{
		if (_Rs_ == _Rt_) // Rs - Rs == 0
			armAsm->Mov(armWRegister(rd), 0);
		else if (rs_const)
		{
			const u32 cv = g_psxConstRegs[_Rs_];
			if (cv == 0)
				armAsm->Neg(armWRegister(rd), armWRegister(rt));
			else
			{
				// rd may alias rt (Rd == Rt); materialize the const minuend in a
				// non-allocatable scratch so the Mov can't clobber the subtrahend.
				armAsm->Mov(RWSCRATCH, cv);
				armAsm->Sub(armWRegister(rd), RWSCRATCH, armWRegister(rt));
			}
		}
		else if (rt_const)
			armAsm->Sub(armWRegister(rd), armWRegister(rs), static_cast<s64>(static_cast<s32>(g_psxConstRegs[_Rt_])));
		else
			armAsm->Sub(armWRegister(rd), armWRegister(rs), armWRegister(rt));
	})

PSX_REG_OP(AND,
	g_psxConstRegs[_Rs_] & g_psxConstRegs[_Rt_],
	{
		if (rs_const)
			armAsm->And(armWRegister(rd), armWRegister(rt), static_cast<u64>(g_psxConstRegs[_Rs_]));
		else if (rt_const)
			armAsm->And(armWRegister(rd), armWRegister(rs), static_cast<u64>(g_psxConstRegs[_Rt_]));
		else
			armAsm->And(armWRegister(rd), armWRegister(rs), armWRegister(rt));
	})

PSX_REG_OP(OR,
	g_psxConstRegs[_Rs_] | g_psxConstRegs[_Rt_],
	{
		if (rs_const)
			armAsm->Orr(armWRegister(rd), armWRegister(rt), static_cast<u64>(g_psxConstRegs[_Rs_]));
		else if (rt_const)
			armAsm->Orr(armWRegister(rd), armWRegister(rs), static_cast<u64>(g_psxConstRegs[_Rt_]));
		else
			armAsm->Orr(armWRegister(rd), armWRegister(rs), armWRegister(rt));
	})

PSX_REG_OP(XOR,
	g_psxConstRegs[_Rs_] ^ g_psxConstRegs[_Rt_],
	{
		if (rs_const)
			armAsm->Eor(armWRegister(rd), armWRegister(rt), static_cast<u64>(g_psxConstRegs[_Rs_]));
		else if (rt_const)
			armAsm->Eor(armWRegister(rd), armWRegister(rs), static_cast<u64>(g_psxConstRegs[_Rt_]));
		else
			armAsm->Eor(armWRegister(rd), armWRegister(rs), armWRegister(rt));
	})

PSX_REG_OP(NOR,
	~(g_psxConstRegs[_Rs_] | g_psxConstRegs[_Rt_]),
	{
		if (rs_const)
			armAsm->Orr(armWRegister(rd), armWRegister(rt), static_cast<u64>(g_psxConstRegs[_Rs_]));
		else if (rt_const)
			armAsm->Orr(armWRegister(rd), armWRegister(rs), static_cast<u64>(g_psxConstRegs[_Rt_]));
		else
			armAsm->Orr(armWRegister(rd), armWRegister(rs), armWRegister(rt));
		armAsm->Mvn(armWRegister(rd), armWRegister(rd));
	})

PSX_REG_OP(SLT,
	((s32)g_psxConstRegs[_Rs_] < (s32)g_psxConstRegs[_Rt_]) ? 1u : 0u,
	{
		// Rs < Rt (signed). When Rs is const k, equivalent to Rt > k → cset gt.
		if (rs_const)
		{
			armAsm->Cmp(armWRegister(rt), static_cast<s64>(static_cast<s32>(g_psxConstRegs[_Rs_])));
			armAsm->Cset(armWRegister(rd), a64::gt);
		}
		else if (rt_const)
		{
			armAsm->Cmp(armWRegister(rs), static_cast<s64>(static_cast<s32>(g_psxConstRegs[_Rt_])));
			armAsm->Cset(armWRegister(rd), a64::lt);
		}
		else
		{
			armAsm->Cmp(armWRegister(rs), armWRegister(rt));
			armAsm->Cset(armWRegister(rd), a64::lt);
		}
	})

PSX_REG_OP(SLTU,
	(g_psxConstRegs[_Rs_] < g_psxConstRegs[_Rt_]) ? 1u : 0u,
	{
		// Rs <u Rt. When Rs is const k, equivalent to Rt >u k → cset hi.
		if (rs_const)
		{
			armAsm->Cmp(armWRegister(rt), static_cast<u64>(g_psxConstRegs[_Rs_]));
			armAsm->Cset(armWRegister(rd), a64::hi);
		}
		else if (rt_const)
		{
			armAsm->Cmp(armWRegister(rs), static_cast<u64>(g_psxConstRegs[_Rt_]));
			armAsm->Cset(armWRegister(rd), a64::lo);
		}
		else
		{
			armAsm->Cmp(armWRegister(rs), armWRegister(rt));
			armAsm->Cset(armWRegister(rd), a64::lo);
		}
	})

////////////////////////////////////////////////////////////////////
// Shift Instructions — rd = rt << sa (or variable shift by rs)

static void rpsxSLL()
{
	if (!_Rd_) return;
	if (PSX_IS_CONST1(_Rt_)) {
		_psxDeleteReg(_Rd_, 0);
		PSX_SET_CONST(_Rd_);
		g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rt_] << _Sa_;
		return;
	}
	_addNeededPSXtoArm64GPR(_Rt_); _addNeededPSXtoArm64GPR(_Rd_);
	int rt = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_READ);
	int rd = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE);
	if (_Sa_)
		armAsm->Lsl(armWRegister(rd), armWRegister(rt), _Sa_);
	else
		armAsm->Mov(armWRegister(rd), armWRegister(rt));
	_clearNeededArm64GPRregs();
	PSX_DEL_CONST(_Rd_);
}

static void rpsxSRL()
{
	if (!_Rd_) return;
	if (PSX_IS_CONST1(_Rt_)) {
		_psxDeleteReg(_Rd_, 0);
		PSX_SET_CONST(_Rd_);
		g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rt_] >> _Sa_;
		return;
	}
	_addNeededPSXtoArm64GPR(_Rt_); _addNeededPSXtoArm64GPR(_Rd_);
	int rt = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_READ);
	int rd = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE);
	if (_Sa_)
		armAsm->Lsr(armWRegister(rd), armWRegister(rt), _Sa_);
	else
		armAsm->Mov(armWRegister(rd), armWRegister(rt));
	_clearNeededArm64GPRregs();
	PSX_DEL_CONST(_Rd_);
}

static void rpsxSRA()
{
	if (!_Rd_) return;
	if (PSX_IS_CONST1(_Rt_)) {
		_psxDeleteReg(_Rd_, 0);
		PSX_SET_CONST(_Rd_);
		g_psxConstRegs[_Rd_] = (s32)g_psxConstRegs[_Rt_] >> _Sa_;
		return;
	}
	_addNeededPSXtoArm64GPR(_Rt_); _addNeededPSXtoArm64GPR(_Rd_);
	int rt = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_READ);
	int rd = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE);
	if (_Sa_)
		armAsm->Asr(armWRegister(rd), armWRegister(rt), _Sa_);
	else
		armAsm->Mov(armWRegister(rd), armWRegister(rt));
	_clearNeededArm64GPRregs();
	PSX_DEL_CONST(_Rd_);
}

// Variable shifts: rd = rt <op> (rs & 0x1F). When Rs is const, fold to a
// fixed-immediate shift — same emit as rpsxSLL/SRL/SRA in the imm form.
static void rpsxSLLV()
{
	if (!_Rd_) return;
	if (PSX_IS_CONST2(_Rs_, _Rt_)) {
		_psxDeleteReg(_Rd_, 0);
		PSX_SET_CONST(_Rd_);
		g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rt_] << (g_psxConstRegs[_Rs_] & 0x1F);
		return;
	}
	_addNeededPSXtoArm64GPR(_Rs_); _addNeededPSXtoArm64GPR(_Rt_); _addNeededPSXtoArm64GPR(_Rd_);
	int rt = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_READ);
	if (PSX_IS_CONST1(_Rs_)) {
		int rd = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE);
		const u32 sa = g_psxConstRegs[_Rs_] & 0x1F;
		if (sa)
			armAsm->Lsl(armWRegister(rd), armWRegister(rt), sa);
		else
			armAsm->Mov(armWRegister(rd), armWRegister(rt));
	}
	else {
		// Alloc Rs BEFORE Rd: when Rd == Rs, allocating Rd MODE_WRITE first
		// would grab a fresh write-only slot (no load), and the subsequent
		// Rs MODE_READ alloc would reuse that empty slot without loading.
		int rs = _allocArm64GPR(ARM64TYPE_PSX, _Rs_, MODE_READ);
		int rd = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE);
		armAsm->Lsl(armWRegister(rd), armWRegister(rt), armWRegister(rs));
	}
	_clearNeededArm64GPRregs();
	PSX_DEL_CONST(_Rd_);
}

static void rpsxSRLV()
{
	if (!_Rd_) return;
	if (PSX_IS_CONST2(_Rs_, _Rt_)) {
		_psxDeleteReg(_Rd_, 0);
		PSX_SET_CONST(_Rd_);
		g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rt_] >> (g_psxConstRegs[_Rs_] & 0x1F);
		return;
	}
	_addNeededPSXtoArm64GPR(_Rs_); _addNeededPSXtoArm64GPR(_Rt_); _addNeededPSXtoArm64GPR(_Rd_);
	int rt = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_READ);
	if (PSX_IS_CONST1(_Rs_)) {
		int rd = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE);
		const u32 sa = g_psxConstRegs[_Rs_] & 0x1F;
		if (sa)
			armAsm->Lsr(armWRegister(rd), armWRegister(rt), sa);
		else
			armAsm->Mov(armWRegister(rd), armWRegister(rt));
	}
	else {
		int rs = _allocArm64GPR(ARM64TYPE_PSX, _Rs_, MODE_READ);
		int rd = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE);
		armAsm->Lsr(armWRegister(rd), armWRegister(rt), armWRegister(rs));
	}
	_clearNeededArm64GPRregs();
	PSX_DEL_CONST(_Rd_);
}

static void rpsxSRAV()
{
	if (!_Rd_) return;
	if (PSX_IS_CONST2(_Rs_, _Rt_)) {
		_psxDeleteReg(_Rd_, 0);
		PSX_SET_CONST(_Rd_);
		g_psxConstRegs[_Rd_] = (s32)g_psxConstRegs[_Rt_] >> (g_psxConstRegs[_Rs_] & 0x1F);
		return;
	}
	_addNeededPSXtoArm64GPR(_Rs_); _addNeededPSXtoArm64GPR(_Rt_); _addNeededPSXtoArm64GPR(_Rd_);
	int rt = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_READ);
	if (PSX_IS_CONST1(_Rs_)) {
		int rd = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE);
		const u32 sa = g_psxConstRegs[_Rs_] & 0x1F;
		if (sa)
			armAsm->Asr(armWRegister(rd), armWRegister(rt), sa);
		else
			armAsm->Mov(armWRegister(rd), armWRegister(rt));
	}
	else {
		int rs = _allocArm64GPR(ARM64TYPE_PSX, _Rs_, MODE_READ);
		int rd = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE);
		armAsm->Asr(armWRegister(rd), armWRegister(rt), armWRegister(rs));
	}
	_clearNeededArm64GPRregs();
	PSX_DEL_CONST(_Rd_);
}

////////////////////////////////////////////////////////////////////
// Multiply/Divide — write to HI:LO

// Sources for MULT/MULTU/DIV/DIVU are loaded directly from psxRegs.GPR
// (not via _psxMoveGPRtoR / _allocArm64GPR) on purpose. The hi-half of the
// product / quotient is written into w0/x0, which collides with the host
// register the allocator would otherwise pick for $Rs or $Rt. Going through
// the allocator leaves a stale "$Rt lives in w0" mapping that downstream
// instructions then trust — e.g. the GCC divide-by-zero check
// `bne $Rt, $zero, +2 / break 7` reads w0 (now the quotient, often 0) and
// fires BREAK even though $Rt was nonzero. Mirrors the x86 IOP rec, which
// also bypasses the allocator with `xMOV(eax, ptr32[&psxRegs.GPR.r[_Rs_]])`.

static void rpsxMULT()
{
	_psxFlushCall(FLUSH_EVERYTHING);
	armLoadPsxRegPtr(a64::w1, &psxRegs.GPR.r[_Rs_]);
	armLoadPsxRegPtr(a64::w2, &psxRegs.GPR.r[_Rt_]);
	armAsm->Smull(a64::x0, a64::w1, a64::w2);
	// IOP HI:LO are 32-bit registers
	armAsm->Str(a64::w0, armPsxRegMem(&psxRegs.GPR.n.lo));
	armAsm->Lsr(a64::x0, a64::x0, 32);
	armAsm->Str(a64::w0, armPsxRegMem(&psxRegs.GPR.n.hi));
	g_iopCyclePenalty = psxInstCycles_Mult;
}

static void rpsxMULTU()
{
	_psxFlushCall(FLUSH_EVERYTHING);
	armLoadPsxRegPtr(a64::w1, &psxRegs.GPR.r[_Rs_]);
	armLoadPsxRegPtr(a64::w2, &psxRegs.GPR.r[_Rt_]);
	armAsm->Umull(a64::x0, a64::w1, a64::w2);
	armAsm->Str(a64::w0, armPsxRegMem(&psxRegs.GPR.n.lo));
	armAsm->Lsr(a64::x0, a64::x0, 32);
	armAsm->Str(a64::w0, armPsxRegMem(&psxRegs.GPR.n.hi));
	g_iopCyclePenalty = psxInstCycles_Mult;
}

static void rpsxDIV()
{
	_psxFlushCall(FLUSH_EVERYTHING);
	armLoadPsxRegPtr(a64::w1, &psxRegs.GPR.r[_Rs_]);
	armLoadPsxRegPtr(a64::w2, &psxRegs.GPR.r[_Rt_]);
	a64::Label zero_case, done;
	armAsm->Cbz(a64::w2, &zero_case);
	// Normal path: SDIV is defined on aarch64 for the (INT_MIN / -1) overflow
	// case (returns INT_MIN, remainder 0) which matches psxDIV()'s explicit
	// overflow branch in R3000AOpcodeTables.cpp:69, so only the divide-by-zero
	// case needs fixing here.
	armAsm->Sdiv(a64::w0, a64::w1, a64::w2);
	armAsm->Msub(a64::w3, a64::w0, a64::w2, a64::w1);
	armAsm->B(&done);
	armAsm->Bind(&zero_case);
	// LO = sign(Rs) ? 1 : 0xFFFFFFFF;  HI = Rs.  Matches psxDIV(_rRt_==0).
	armAsm->Mov(a64::w0, -1);
	armAsm->Cmp(a64::w1, 0);
	armAsm->Cneg(a64::w0, a64::w0, a64::mi);
	armAsm->Mov(a64::w3, a64::w1);
	armAsm->Bind(&done);
	armAsm->Str(a64::w0, armPsxRegMem(&psxRegs.GPR.n.lo));
	armAsm->Str(a64::w3, armPsxRegMem(&psxRegs.GPR.n.hi));
	g_iopCyclePenalty = psxInstCycles_Div;
}

static void rpsxDIVU()
{
	_psxFlushCall(FLUSH_EVERYTHING);
	armLoadPsxRegPtr(a64::w1, &psxRegs.GPR.r[_Rs_]);
	armLoadPsxRegPtr(a64::w2, &psxRegs.GPR.r[_Rt_]);
	a64::Label zero_case, done;
	armAsm->Cbz(a64::w2, &zero_case);
	armAsm->Udiv(a64::w0, a64::w1, a64::w2);
	armAsm->Msub(a64::w3, a64::w0, a64::w2, a64::w1);
	armAsm->B(&done);
	armAsm->Bind(&zero_case);
	// LO = 0xFFFFFFFF; HI = Rs. Matches psxDIVU(_rRt_==0).
	armAsm->Mov(a64::w0, -1);
	armAsm->Mov(a64::w3, a64::w1);
	armAsm->Bind(&done);
	armAsm->Str(a64::w0, armPsxRegMem(&psxRegs.GPR.n.lo));
	armAsm->Str(a64::w3, armPsxRegMem(&psxRegs.GPR.n.hi));
	g_iopCyclePenalty = psxInstCycles_Div;
}

////////////////////////////////////////////////////////////////////
// Move from/to HI/LO

static void rpsxMFHI()
{
	if (!_Rd_) return;
	_psxDeleteReg(_Rd_, 0);
	PSX_DEL_CONST(_Rd_);
	int rd = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE);
	armLoadPsxRegPtr(armWRegister(rd), &psxRegs.GPR.n.hi);
	_clearNeededArm64GPRregs();
}

static void rpsxMTHI()
{
	// const Rs: store immediate to hi directly.
	if (PSX_IS_CONST1(_Rs_))
	{
		armAsm->Mov(RWSCRATCH, g_psxConstRegs[_Rs_]);
		armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.GPR.n.hi));
		return;
	}
	// Otherwise read Rs via the allocator — no FLUSH_EVERYTHING needed here.
	_addNeededPSXtoArm64GPR(_Rs_);
	int rs = _allocArm64GPR(ARM64TYPE_PSX, _Rs_, MODE_READ);
	armAsm->Str(armWRegister(rs), armPsxRegMem(&psxRegs.GPR.n.hi));
	_clearNeededArm64GPRregs();
}

static void rpsxMFLO()
{
	if (!_Rd_) return;
	_psxDeleteReg(_Rd_, 0);
	PSX_DEL_CONST(_Rd_);
	int rd = _allocArm64GPR(ARM64TYPE_PSX, _Rd_, MODE_WRITE);
	armLoadPsxRegPtr(armWRegister(rd), &psxRegs.GPR.n.lo);
	_clearNeededArm64GPRregs();
}

static void rpsxMTLO()
{
	if (PSX_IS_CONST1(_Rs_))
	{
		armAsm->Mov(RWSCRATCH, g_psxConstRegs[_Rs_]);
		armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.GPR.n.lo));
		return;
	}
	_addNeededPSXtoArm64GPR(_Rs_);
	int rs = _allocArm64GPR(ARM64TYPE_PSX, _Rs_, MODE_READ);
	armAsm->Str(armWRegister(rs), armPsxRegMem(&psxRegs.GPR.n.lo));
	_clearNeededArm64GPRregs();
}

////////////////////////////////////////////////////////////////////
// Load/Store
//
// Loads:  compute address in w0, flush, call iopMemReadN, sign/zero extend, store result
// Stores: compute address in w0, value in w1, flush, call iopMemWriteN

static void rpsxLoadGeneric(int size, bool sign)
{
	// Read Rs const value FIRST — before deleting Rt (critical when Rs==Rt,
	// since _psxDeleteReg clears const state and frees the host register).
	const bool rs_const = PSX_IS_CONST1(_Rs_);
	const u32 rs_val = rs_const ? g_psxConstRegs[_Rs_] : 0;

	// Delete destination register (flush=1 to write back, in case Rs==Rt
	// and Rs is in a host register — need the value in memory).
	if (_Rt_)
		_psxDeleteReg(_Rt_, 1);

	_psxFlushCall(FLUSH_EVERYTHING);

	// Compute address: base + imm16 (after flush, safe to use w0)
	if (rs_const)
	{
		armAsm->Mov(RWARG1, rs_val + _Imm_);
	}
	else
	{
		armLoadPsxRegPtr(RWARG1, &psxRegs.GPR.r[_Rs_]);
		if (_Imm_ != 0)
			armAsm->Add(RWARG1, RWARG1, static_cast<s64>(static_cast<s32>(_Imm_)));
	}

	// Call iopMemRead — address in w0, result returned in w0
	switch (size)
	{
		case 8:  armEmitCall((void*)iopMemRead8);  break;
		case 16: armEmitCall((void*)iopMemRead16); break;
		case 32: armEmitCall((void*)iopMemRead32); break;
	}

	if (!_Rt_)
		return; // dummy read

	// Sign/zero extend result (w0)
	switch (size)
	{
		case 8:
			if (sign)
				armAsm->Sxtb(RWARG1, RWARG1);
			else
				armAsm->Uxtb(RWARG1, RWARG1);
			break;
		case 16:
			if (sign)
				armAsm->Sxth(RWARG1, RWARG1);
			else
				armAsm->Uxth(RWARG1, RWARG1);
			break;
		case 32:
			break; // no extension needed
	}

	// Store result to destination register
	armStorePsxRegPtr(RWARG1, &psxRegs.GPR.r[_Rt_]);
}

static void rpsxStoreGeneric(int size)
{
	// Read const values before flush
	const bool rs_const = PSX_IS_CONST1(_Rs_);
	const u32 rs_val = rs_const ? g_psxConstRegs[_Rs_] : 0;
	const bool rt_const = PSX_IS_CONST1(_Rt_);
	const u32 rt_val = rt_const ? g_psxConstRegs[_Rt_] : 0;

	// Flush all registers BEFORE computing operands
	_psxFlushCall(FLUSH_EVERYTHING);

	// Compute address: base + imm16
	if (rs_const)
	{
		armAsm->Mov(RWARG1, rs_val + _Imm_);
	}
	else
	{
		armLoadPsxRegPtr(RWARG1, &psxRegs.GPR.r[_Rs_]);
		if (_Imm_ != 0)
			armAsm->Add(RWARG1, RWARG1, static_cast<s64>(static_cast<s32>(_Imm_)));
	}

	// Load store value into w1
	if (rt_const)
		armAsm->Mov(RWARG2, rt_val);
	else
		armLoadPsxRegPtr(RWARG2, &psxRegs.GPR.r[_Rt_]);

	// Call iopMemWrite — address in w0, value in w1
	switch (size)
	{
		case 8:  armEmitCall((void*)iopMemWrite8);  break;
		case 16: armEmitCall((void*)iopMemWrite16); break;
		case 32: armEmitCall((void*)iopMemWrite32); break;
	}
}

static void rpsxLB()  { rpsxLoadGeneric(8, true); }
static void rpsxLBU() { rpsxLoadGeneric(8, false); }
static void rpsxLH()  { rpsxLoadGeneric(16, true); }
static void rpsxLHU() { rpsxLoadGeneric(16, false); }
static void rpsxLW()  { rpsxLoadGeneric(32, false); }
static void rpsxSB()  { rpsxStoreGeneric(8); }
static void rpsxSH()  { rpsxStoreGeneric(16); }
static void rpsxSW()  { rpsxStoreGeneric(32); }

// =====================================================================================================
//  Unaligned word load/store: LWL / LWR / SWL / SWR
//  ----------------------------------------------------------------------------------------------------
//  These are partial-word merges keyed on the low two bits of the byte
//  address — *not* generic unaligned loads. Compiler-emitted LWL+LWR or
//  SWL+SWR pairs construct an unaligned 32-bit access; in isolation each
//  op merges memory bytes with the existing register/memory contents per
//  the formulae in pcsx2/R3000AOpcodeTables.cpp:psxLWL/LWR/SWL/SWR.
//
//  byte_addr = rs + imm; (addr & 3) is saved to the stack across the
//  iopMemRead32/Write32 C call; then the mask + shift + or merge is done
//  inline, replacing the REC_FUNC interp fallback.
// =====================================================================================================

// Compute byte address (rs + imm) into RWARG1, leaving (byte_addr & 3) in
// RWSCRATCH for the caller's later use *before* the C call clobbers w0.
// On return, RWARG1 holds the aligned address (byte_addr & ~3) ready for
// iopMemRead32/iopMemWrite32.
static void rpsxComputeUnalignedAddr()
{
	const bool rs_const = PSX_IS_CONST1(_Rs_);
	const u32 rs_val = rs_const ? g_psxConstRegs[_Rs_] : 0;

	if (rs_const)
	{
		const u32 byte_addr = rs_val + _Imm_;
		armAsm->Mov(RWARG1, byte_addr & ~3u);     // aligned address for memRead/Write32
		armAsm->Mov(RWSCRATCH, byte_addr & 3u);   // shift_input
	}
	else
	{
		armLoadPsxRegPtr(RWARG1, &psxRegs.GPR.r[_Rs_]); // w0 = rs
		if (_Imm_ != 0)
			armAsm->Add(RWARG1, RWARG1, static_cast<s64>(static_cast<s32>(_Imm_))); // w0 = rs + imm (byte_addr)
		armAsm->And(RWSCRATCH, RWARG1, 3);        // RWSCRATCH = byte_addr & 3
		armAsm->Bic(RWARG1, RWARG1, 3);           // w0 = byte_addr & ~3
	}
}

static void rpsxLWL()
{
	if (_Rt_)
		_psxDeleteReg(_Rt_, 1);

	_psxFlushCall(FLUSH_EVERYTHING);

	rpsxComputeUnalignedAddr();

	// Save shift_input across the C call (callee-saved would also work,
	// but the IOP rec doesn't reserve any of x19-x28 for the emitter).
	armAsm->Sub(a64::sp, a64::sp, 16);
	armAsm->Str(RWSCRATCH, a64::MemOperand(a64::sp));

	armEmitCall((void*)iopMemRead32);              // w0 = mem (aligned word)

	armAsm->Ldr(a64::w1, a64::MemOperand(a64::sp));
	armAsm->Add(a64::sp, a64::sp, 16);

	if (!_Rt_)
		return; // dummy read — preserve memory side effects only

	// shift = (byte_addr & 3) * 8.
	armAsm->Lsl(a64::w1, a64::w1, 3);              // w1 = shift

	// mask = 0x00ffffff >> shift, mem_shift = 24 - shift.
	armAsm->Mov(a64::w2, 0x00ffffffu);
	armAsm->Lsr(a64::w2, a64::w2, a64::w1);        // w2 = mask
	armAsm->Mov(RWSCRATCH, 24);
	armAsm->Sub(RWSCRATCH, RWSCRATCH, a64::w1);    // RWSCRATCH = 24 - shift
	armAsm->Lsl(RWARG1, RWARG1, RWSCRATCH);        // w0 = mem << (24 - shift)

	// Merge: rt = (rt & mask) | (mem << (24 - shift)).
	armLoadPsxRegPtr(a64::w3, &psxRegs.GPR.r[_Rt_]);
	armAsm->And(a64::w3, a64::w3, a64::w2);
	armAsm->Orr(RWARG1, RWARG1, a64::w3);
	armStorePsxRegPtr(RWARG1, &psxRegs.GPR.r[_Rt_]);
}

static void rpsxLWR()
{
	if (_Rt_)
		_psxDeleteReg(_Rt_, 1);

	_psxFlushCall(FLUSH_EVERYTHING);

	rpsxComputeUnalignedAddr();

	armAsm->Sub(a64::sp, a64::sp, 16);
	armAsm->Str(RWSCRATCH, a64::MemOperand(a64::sp));

	armEmitCall((void*)iopMemRead32);              // w0 = mem

	armAsm->Ldr(a64::w1, a64::MemOperand(a64::sp));
	armAsm->Add(a64::sp, a64::sp, 16);

	if (!_Rt_)
		return;

	// shift = (byte_addr & 3) * 8.
	armAsm->Lsl(a64::w1, a64::w1, 3);              // w1 = shift

	// mask = 0xffffff00 << (24 - shift); mem_shift = shift.
	armAsm->Mov(a64::w2, 0xffffff00u);
	armAsm->Mov(RWSCRATCH, 24);
	armAsm->Sub(RWSCRATCH, RWSCRATCH, a64::w1);    // RWSCRATCH = 24 - shift
	armAsm->Lsl(a64::w2, a64::w2, RWSCRATCH);      // w2 = mask
	armAsm->Lsr(RWARG1, RWARG1, a64::w1);          // w0 = mem >> shift

	armLoadPsxRegPtr(a64::w3, &psxRegs.GPR.r[_Rt_]);
	armAsm->And(a64::w3, a64::w3, a64::w2);
	armAsm->Orr(RWARG1, RWARG1, a64::w3);
	armStorePsxRegPtr(RWARG1, &psxRegs.GPR.r[_Rt_]);
}

static void rpsxSWL()
{
	const bool rt_const = PSX_IS_CONST1(_Rt_);
	const u32 rt_val = rt_const ? g_psxConstRegs[_Rt_] : 0;

	_psxFlushCall(FLUSH_EVERYTHING);

	rpsxComputeUnalignedAddr();

	// Save aligned addr (RWARG1) and shift_input (RWSCRATCH) across the
	// memRead call. Both needed for the subsequent memWrite + merge.
	armAsm->Sub(a64::sp, a64::sp, 16);
	armAsm->Str(RWARG1, a64::MemOperand(a64::sp, 0));
	armAsm->Str(RWSCRATCH, a64::MemOperand(a64::sp, 4));

	armEmitCall((void*)iopMemRead32);              // w0 = mem

	// Reload shift_input and aligned addr; mem stays in w0.
	armAsm->Ldr(a64::w1, a64::MemOperand(a64::sp, 4));  // w1 = shift_input

	// shift = (byte_addr & 3) * 8.
	armAsm->Lsl(a64::w1, a64::w1, 3);              // w1 = shift

	// rt_shifted = rt >> (24 - shift)
	if (rt_const)
		armAsm->Mov(a64::w3, rt_val);
	else
		armLoadPsxRegPtr(a64::w3, &psxRegs.GPR.r[_Rt_]);
	armAsm->Mov(RWSCRATCH, 24);
	armAsm->Sub(RWSCRATCH, RWSCRATCH, a64::w1);    // RWSCRATCH = 24 - shift
	armAsm->Lsr(a64::w3, a64::w3, RWSCRATCH);      // w3 = rt >> (24 - shift)

	// mem_masked = mem & (0xffffff00 << shift)
	armAsm->Mov(a64::w2, 0xffffff00u);
	armAsm->Lsl(a64::w2, a64::w2, a64::w1);        // w2 = mask
	armAsm->And(a64::w0, a64::w0, a64::w2);        // w0 = mem & mask
	armAsm->Orr(a64::w0, a64::w0, a64::w3);        // merged value

	// Now write back. iopMemWrite32(addr, value): w0 = addr, w1 = value.
	armAsm->Mov(RWARG2, a64::w0);                  // w1 = value
	armAsm->Ldr(RWARG1, a64::MemOperand(a64::sp, 0)); // w0 = aligned addr
	armAsm->Add(a64::sp, a64::sp, 16);

	armEmitCall((void*)iopMemWrite32);
}

static void rpsxSWR()
{
	const bool rt_const = PSX_IS_CONST1(_Rt_);
	const u32 rt_val = rt_const ? g_psxConstRegs[_Rt_] : 0;

	_psxFlushCall(FLUSH_EVERYTHING);

	rpsxComputeUnalignedAddr();

	armAsm->Sub(a64::sp, a64::sp, 16);
	armAsm->Str(RWARG1, a64::MemOperand(a64::sp, 0));
	armAsm->Str(RWSCRATCH, a64::MemOperand(a64::sp, 4));

	armEmitCall((void*)iopMemRead32);              // w0 = mem

	armAsm->Ldr(a64::w1, a64::MemOperand(a64::sp, 4));
	armAsm->Lsl(a64::w1, a64::w1, 3);              // w1 = shift

	// rt_shifted = rt << shift
	if (rt_const)
		armAsm->Mov(a64::w3, rt_val);
	else
		armLoadPsxRegPtr(a64::w3, &psxRegs.GPR.r[_Rt_]);
	armAsm->Lsl(a64::w3, a64::w3, a64::w1);        // w3 = rt << shift

	// mem_masked = mem & (0x00ffffff >> (24 - shift))
	armAsm->Mov(a64::w2, 0x00ffffffu);
	armAsm->Mov(RWSCRATCH, 24);
	armAsm->Sub(RWSCRATCH, RWSCRATCH, a64::w1);    // RWSCRATCH = 24 - shift
	armAsm->Lsr(a64::w2, a64::w2, RWSCRATCH);      // w2 = mask
	armAsm->And(a64::w0, a64::w0, a64::w2);        // w0 = mem & mask
	armAsm->Orr(a64::w0, a64::w0, a64::w3);        // merged value

	armAsm->Mov(RWARG2, a64::w0);
	armAsm->Ldr(RWARG1, a64::MemOperand(a64::sp, 0));
	armAsm->Add(a64::sp, a64::sp, 16);

	armEmitCall((void*)iopMemWrite32);
}

////////////////////////////////////////////////////////////////////
// Branch/Jump Instructions

static void rpsxJ()
{
	u32 newpc = _InstrucTarget_ * 4 + (psxpc & 0xf0000000);
	psxRecompileNextInstruction(true, false);
	psxSetBranchImm(newpc);
}

static void rpsxJAL()
{
	u32 newpc = _InstrucTarget_ * 4 + (psxpc & 0xf0000000);
	_psxDeleteReg(31, 0);
	PSX_SET_CONST(31);
	g_psxConstRegs[31] = psxpc + 4;

	psxRecompileNextInstruction(true, false);
	psxSetBranchImm(newpc);
}

static void rpsxJR()
{
	// Save branch target to pcWriteback before delay slot — the delay slot's
	// recCall will clobber w0 via _psxFlushCall.
	_psxMoveGPRtoR(RWSCRATCH, _Rs_);
	armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.pcWriteback));
	_psxFlushCall(FLUSH_EVERYTHING);

	const bool swap = psxTrySwapDelaySlot(_Rs_, 0, 0);
	if (!swap)
		psxRecompileNextInstruction(true, false);
	psxSetBranchReg();
}

static void rpsxJALR()
{
	// Save branch target to pcWriteback before delay slot
	_psxMoveGPRtoR(RWSCRATCH, _Rs_);
	armAsm->Str(RWSCRATCH, armPsxRegMem(&psxRegs.pcWriteback));

	// Capture link before swap advances psxpc past the delay slot.
	const u32 newpc = psxpc + 4;

	// Rd == Rs disables swap — the delay slot reading Rs would observe the
	// post-link value instead of the pre-link one.
	const bool swap = (_Rd_ == _Rs_) ? false : psxTrySwapDelaySlot(_Rs_, 0, _Rd_);

	// Save return address
	if (_Rd_)
	{
		_psxDeleteReg(_Rd_, 0);
		PSX_SET_CONST(_Rd_);
		g_psxConstRegs[_Rd_] = newpc;
	}

	_psxFlushCall(FLUSH_EVERYTHING);
	if (!swap)
		psxRecompileNextInstruction(true, false);
	psxSetBranchReg();
}

// Helper for conditional branches: compare Rs and Rt, branch if condition met
static void rpsxBranchCompare(a64::Condition cond)
{
	u32 branchTo = ((s32)(s16)_Imm_ * 4) + psxpc;

	// Compare Rs and Rt
	if (PSX_IS_CONST2(_Rs_, _Rt_))
	{
		// Both constant — evaluate at compile time
		bool taken = false;
		if (cond == a64::eq) taken = (g_psxConstRegs[_Rs_] == g_psxConstRegs[_Rt_]);
		else if (cond == a64::ne) taken = (g_psxConstRegs[_Rs_] != g_psxConstRegs[_Rt_]);
		_psxFlushAllDirty();
		psxRecompileNextInstruction(true, false);
		psxSetBranchImm(taken ? branchTo : psxpc);
		return;
	}

	// Hoist delay slot ahead of compare when it doesn't reference Rs/Rt.
	// Flush AFTER swap so any cache dirties left by the swapped delay-slot
	// instruction commit to memory before the compare/branch — the runtime
	// taken path otherwise won't emit those flushes.
	const bool swap = psxTrySwapDelaySlot(_Rs_, _Rt_, 0);
	_psxFlushAllDirty();

	// Runtime comparison
	if (PSX_IS_CONST1(_Rs_))
	{
		int rt = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_READ);
		armAsm->Cmp(armWRegister(rt), g_psxConstRegs[_Rs_]);
		// Condition is symmetric for eq/ne so operand reversal is safe
	}
	else if (PSX_IS_CONST1(_Rt_))
	{
		int rs = _allocArm64GPR(ARM64TYPE_PSX, _Rs_, MODE_READ);
		armAsm->Cmp(armWRegister(rs), g_psxConstRegs[_Rt_]);
	}
	else
	{
		int rs = _allocArm64GPR(ARM64TYPE_PSX, _Rs_, MODE_READ);
		int rt = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_READ);
		armAsm->Cmp(armWRegister(rs), armWRegister(rt));
	}

	_clearNeededArm64GPRregs();

	a64::Label taken;
	armAsm->B(&taken, cond);

	// Not taken path
	if (!swap)
	{
		psxSaveBranchState();
		psxRecompileNextInstruction(true, false);
	}
	psxSetBranchImm(psxpc);

	// Taken path — recompile delay slot from the correct PC
	armAsm->Bind(&taken);
	if (!swap)
	{
		psxpc -= 4;
		psxLoadBranchState();
		psxRecompileNextInstruction(true, false);
	}
	psxSetBranchImm(branchTo);
}

static void rpsxBEQ() { rpsxBranchCompare(a64::eq); }
static void rpsxBNE() { rpsxBranchCompare(a64::ne); }

// BLEZ / BGTZ / BLTZ / BGEZ — compare Rs against zero
static void rpsxBranchZero(a64::Condition cond)
{
	u32 branchTo = ((s32)(s16)_Imm_ * 4) + psxpc;

	if (PSX_IS_CONST1(_Rs_))
	{
		bool taken = false;
		s32 val = (s32)g_psxConstRegs[_Rs_];
		if (cond == a64::le) taken = (val <= 0);
		else if (cond == a64::gt) taken = (val > 0);
		else if (cond == a64::lt) taken = (val < 0);
		else if (cond == a64::ge) taken = (val >= 0);
		// No _psxFlushAllDirty() here: the branch is resolved statically (single
		// successor, no compare needing clean regs and no Save/LoadBranchState
		// snapshot), so the explicit flush is redundant — the delay-slot recompile
		// manages its own dirties and block-end flushes the rest.
		psxRecompileNextInstruction(true, false);
		psxSetBranchImm(taken ? branchTo : psxpc);
		return;
	}

	// Hoist delay slot ahead of compare when it doesn't reference Rs.
	// Flush AFTER swap so any cache dirties left by the swapped delay-slot
	// instruction commit to memory before the compare/branch.
	const bool swap = psxTrySwapDelaySlot(_Rs_, 0, 0);
	_psxFlushAllDirty();

	int rs = _allocArm64GPR(ARM64TYPE_PSX, _Rs_, MODE_READ);
	armAsm->Cmp(armWRegister(rs), 0);
	_clearNeededArm64GPRregs();

	a64::Label taken;
	armAsm->B(&taken, cond);

	if (!swap)
	{
		psxSaveBranchState();
		psxRecompileNextInstruction(true, false);
	}
	psxSetBranchImm(psxpc);

	armAsm->Bind(&taken);
	if (!swap)
	{
		psxpc -= 4;
		psxLoadBranchState();
		psxRecompileNextInstruction(true, false);
	}
	psxSetBranchImm(branchTo);
}

static void rpsxBLEZ() { rpsxBranchZero(a64::le); }
static void rpsxBGTZ() { rpsxBranchZero(a64::gt); }
static void rpsxBLTZ() { rpsxBranchZero(a64::lt); }
static void rpsxBGEZ() { rpsxBranchZero(a64::ge); }

static void rpsxBLTZAL()
{
	_psxDeleteReg(31, 0);
	PSX_SET_CONST(31);
	g_psxConstRegs[31] = psxpc + 4;
	rpsxBranchZero(a64::lt);
}

static void rpsxBGEZAL()
{
	_psxDeleteReg(31, 0);
	PSX_SET_CONST(31);
	g_psxConstRegs[31] = psxpc + 4;
	rpsxBranchZero(a64::ge);
}

////////////////////////////////////////////////////////////////////
// COP0

// MFC0/CFC0: Rt = CP0[Rd]
static void rpsxMFC0()
{
	if (!_Rt_)
		return;

	// Mirrors x86 rpsxMFC0: allocate Rt as a write target and load CP0[Rd] into
	// it. CP0 is never register-allocated, so its memory copy is always current;
	// and nothing here calls a C function, so no flush is needed.
	const int rt = _allocArm64GPR(ARM64TYPE_PSX, _Rt_, MODE_WRITE);
	armLoadPsxRegPtr(armWRegister(rt), &psxRegs.CP0.r[_Rd_]);
}

static void rpsxCFC0() { rpsxMFC0(); }

// MTC0/CTC0: CP0[Rd] = Rt
static void rpsxMTC0()
{
	// Mirrors x86 rpsxMTC0: read Rt allocator-aware (const / dirty host reg /
	// memory, via _psxMoveGPRtoR) and store to CP0[Rd]. No flush — no C call
	// follows, and CP0 is not register-allocated so the memory store stands.
	_psxMoveGPRtoR(RWSCRATCH, _Rt_);
	armStorePsxRegPtr(RWSCRATCH, &psxRegs.CP0.r[_Rd_]);
}

static void rpsxCTC0() { rpsxMTC0(); }

// RFE: Status = (Status & 0xFFFFFFF0) | ((Status & 0x3C) >> 2)
// Then test IOP INTC to raise any pending interrupts.
static void rpsxRFE()
{
	_psxFlushCall(FLUSH_EVERYTHING);

	armLoadPsxRegPtr(RWSCRATCH, &psxRegs.CP0.n.Status);
	armAsm->Ubfx(RWARG1, RWSCRATCH, 2, 4);        // (Status >> 2) & 0xF == (Status & 0x3C) >> 2
	armAsm->Bfi(RWSCRATCH, RWARG1, 0, 4);          // replace low 4 bits of Status
	armStorePsxRegPtr(RWSCRATCH, &psxRegs.CP0.n.Status);

	armEmitCall((void*)iopTestIntc);
}

////////////////////////////////////////////////////////////////////
// GTE (COP2)

REC_GTE_FUNC(MFC2);
REC_GTE_FUNC(MTC2);
REC_GTE_FUNC(CFC2);
REC_GTE_FUNC(CTC2);
REC_GTE_FUNC(LWC2);
REC_GTE_FUNC(SWC2);

REC_GTE_FUNC(RTPS);
REC_GTE_FUNC(NCLIP);
REC_GTE_FUNC(OP);
REC_GTE_FUNC(DPCS);
REC_GTE_FUNC(INTPL);
REC_GTE_FUNC(MVMVA);
REC_GTE_FUNC(NCDS);
REC_GTE_FUNC(CDP);
REC_GTE_FUNC(NCDT);
REC_GTE_FUNC(NCCS);
REC_GTE_FUNC(CC);
REC_GTE_FUNC(NCS);
REC_GTE_FUNC(NCT);
REC_GTE_FUNC(SQR);
REC_GTE_FUNC(DCPL);
REC_GTE_FUNC(DPCT);
REC_GTE_FUNC(AVSZ3);
REC_GTE_FUNC(AVSZ4);
REC_GTE_FUNC(RTPT);
REC_GTE_FUNC(GPF);
REC_GTE_FUNC(GPL);
REC_GTE_FUNC(NCCT);

////////////////////////////////////////////////////////////////////
// rpsxSYSCALL and rpsxBREAK are defined in iR3000A-arm64.cpp

////////////////////////////////////////////////////////////////////
// Dispatch Tables

extern void (*rpsxBSC[64])();
extern void (*rpsxSPC[64])();
extern void (*rpsxREG[32])();
extern void (*rpsxCP0[32])();
extern void (*rpsxCP2[64])();
extern void (*rpsxCP2BSC[32])();

// Defined in iR3000A-arm64.cpp
extern void rpsxSYSCALL();
extern void rpsxBREAK();

static void rpsxSPECIAL() { rpsxSPC[_Funct_](); }
static void rpsxREGIMM() { rpsxREG[_Rt_](); }
static void rpsxCOP0() { rpsxCP0[_Rs_](); }
static void rpsxCOP2() { rpsxCP2[_Funct_](); }
static void rpsxBASIC() { rpsxCP2BSC[_Rs_](); }

static void rpsxNULL()
{
	Console.WriteLn("psxUNK: %8.8x", psxRegs.code);
}

// clang-format off
void (*rpsxBSC[64])() = {
	rpsxSPECIAL, rpsxREGIMM, rpsxJ   , rpsxJAL  , rpsxBEQ , rpsxBNE , rpsxBLEZ, rpsxBGTZ,
	rpsxADDI   , rpsxADDIU , rpsxSLTI, rpsxSLTIU, rpsxANDI, rpsxORI , rpsxXORI, rpsxLUI ,
	rpsxCOP0   , rpsxNULL  , rpsxCOP2, rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL   , rpsxNULL  , rpsxNULL, rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxLB     , rpsxLH    , rpsxLWL , rpsxLW   , rpsxLBU , rpsxLHU , rpsxLWR , rpsxNULL,
	rpsxSB     , rpsxSH    , rpsxSWL , rpsxSW   , rpsxNULL, rpsxNULL, rpsxSWR , rpsxNULL,
	rpsxNULL   , rpsxNULL  , rgteLWC2, rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL   , rpsxNULL  , rgteSWC2, rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
};

void (*rpsxSPC[64])() = {
	rpsxSLL , rpsxNULL, rpsxSRL , rpsxSRA , rpsxSLLV   , rpsxNULL , rpsxSRLV, rpsxSRAV,
	rpsxJR  , rpsxJALR, rpsxNULL, rpsxNULL, rpsxSYSCALL, rpsxBREAK, rpsxNULL, rpsxNULL,
	rpsxMFHI, rpsxMTHI, rpsxMFLO, rpsxMTLO, rpsxNULL   , rpsxNULL , rpsxNULL, rpsxNULL,
	rpsxMULT, rpsxMULTU, rpsxDIV, rpsxDIVU, rpsxNULL   , rpsxNULL , rpsxNULL, rpsxNULL,
	rpsxADD , rpsxADDU, rpsxSUB , rpsxSUBU, rpsxAND    , rpsxOR   , rpsxXOR , rpsxNOR ,
	rpsxNULL, rpsxNULL, rpsxSLT , rpsxSLTU, rpsxNULL   , rpsxNULL , rpsxNULL, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL   , rpsxNULL , rpsxNULL, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL   , rpsxNULL , rpsxNULL, rpsxNULL,
};

void (*rpsxREG[32])() = {
	rpsxBLTZ  , rpsxBGEZ  , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL  , rpsxNULL  , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxBLTZAL, rpsxBGEZAL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL  , rpsxNULL  , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
};

void (*rpsxCP0[32])() = {
	rpsxMFC0, rpsxNULL, rpsxCFC0, rpsxNULL, rpsxMTC0, rpsxNULL, rpsxCTC0, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxRFE , rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
};

void (*rpsxCP2[64])() = {
	rpsxBASIC, rgteRTPS , rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL , rgteNCLIP, rpsxNULL, // 00
	rpsxNULL , rpsxNULL , rpsxNULL , rpsxNULL, rgteOP  , rpsxNULL , rpsxNULL , rpsxNULL, // 08
	rgteDPCS , rgteINTPL, rgteMVMVA, rgteNCDS, rgteCDP , rpsxNULL , rgteNCDT , rpsxNULL, // 10
	rpsxNULL , rpsxNULL , rpsxNULL , rgteNCCS, rgteCC  , rpsxNULL , rgteNCS  , rpsxNULL, // 18
	rgteNCT  , rpsxNULL , rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL , rpsxNULL , rpsxNULL, // 20
	rgteSQR  , rgteDCPL , rgteDPCT , rpsxNULL, rpsxNULL, rgteAVSZ3, rgteAVSZ4, rpsxNULL, // 28
	rgteRTPT , rpsxNULL , rpsxNULL , rpsxNULL, rpsxNULL, rpsxNULL , rpsxNULL , rpsxNULL, // 30
	rpsxNULL , rpsxNULL , rpsxNULL , rpsxNULL, rpsxNULL, rgteGPF  , rgteGPL  , rgteNCCT, // 38
};

void (*rpsxCP2BSC[32])() = {
	rgteMFC2, rpsxNULL, rgteCFC2, rpsxNULL, rgteMTC2, rpsxNULL, rgteCTC2, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
	rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL, rpsxNULL,
};
// clang-format on

////////////////////////////////////////////////////////////////////
// Back-Propagation Analysis Tables (architecture-independent)
////////////////////////////////////////////////////////////////////

#define rpsxpropSetRead(reg) \
	{ \
		if (!(pinst->regs[reg] & EEINST_USED)) \
			pinst->regs[reg] |= EEINST_LASTUSE; \
		prev->regs[reg] |= EEINST_LIVE | EEINST_USED; \
		pinst->regs[reg] |= EEINST_USED; \
		_recFillRegister(*pinst, NEONTYPE_GPRREG, reg, 0); \
	}

#define rpsxpropSetWrite(reg) \
	{ \
		prev->regs[reg] &= ~(EEINST_LIVE | EEINST_USED); \
		if (!(pinst->regs[reg] & EEINST_USED)) \
			pinst->regs[reg] |= EEINST_LASTUSE; \
		pinst->regs[reg] |= EEINST_USED; \
		_recFillRegister(*pinst, NEONTYPE_GPRREG, reg, 1); \
	}

void rpsxpropBSC(EEINST* prev, EEINST* pinst);
void rpsxpropSPECIAL(EEINST* prev, EEINST* pinst);
void rpsxpropREGIMM(EEINST* prev, EEINST* pinst);
void rpsxpropCP0(EEINST* prev, EEINST* pinst);
void rpsxpropCP2(EEINST* prev, EEINST* pinst);

void rpsxpropBSC(EEINST* prev, EEINST* pinst)
{
	switch (psxRegs.code >> 26)
	{
		case 0: rpsxpropSPECIAL(prev, pinst); break;
		case 1: rpsxpropREGIMM(prev, pinst); break;
		case 2: break; // J
		case 3: rpsxpropSetWrite(31); break; // JAL
		case 4: case 5: // BEQ, BNE
			rpsxpropSetRead(_Rs_);
			rpsxpropSetRead(_Rt_);
			break;
		case 6: case 7: // BLEZ, BGTZ
			rpsxpropSetRead(_Rs_);
			break;
		case 15: // LUI
			rpsxpropSetWrite(_Rt_);
			break;
		case 16: rpsxpropCP0(prev, pinst); break;
		case 18: rpsxpropCP2(prev, pinst); break;
		case 40: case 41: case 42: case 43: case 46: // stores
			rpsxpropSetRead(_Rt_);
			rpsxpropSetRead(_Rs_);
			break;
		case 50: case 58: break; // LWC2, SWC2
		default:
			rpsxpropSetWrite(_Rt_);
			rpsxpropSetRead(_Rs_);
			break;
	}
}

void rpsxpropSPECIAL(EEINST* prev, EEINST* pinst)
{
	switch (_Funct_)
	{
		case 0: case 2: case 3: // SLL, SRL, SRA
			rpsxpropSetWrite(_Rd_);
			rpsxpropSetRead(_Rt_);
			break;
		case 8: // JR
			rpsxpropSetRead(_Rs_);
			break;
		case 9: // JALR
			rpsxpropSetWrite(_Rd_);
			rpsxpropSetRead(_Rs_);
			break;
		case 12: case 13: // SYSCALL, BREAK
			_recClearInst(prev);
			prev->info = 0;
			break;
		case 15: break; // SYNC
		case 16: // MFHI
			rpsxpropSetWrite(_Rd_);
			rpsxpropSetRead(PSX_HI);
			break;
		case 17: // MTHI
			rpsxpropSetWrite(PSX_HI);
			rpsxpropSetRead(_Rs_);
			break;
		case 18: // MFLO
			rpsxpropSetWrite(_Rd_);
			rpsxpropSetRead(PSX_LO);
			break;
		case 19: // MTLO
			rpsxpropSetWrite(PSX_LO);
			rpsxpropSetRead(_Rs_);
			break;
		case 24: case 25: case 26: case 27: // MULT, MULTU, DIV, DIVU
			rpsxpropSetWrite(PSX_LO);
			rpsxpropSetWrite(PSX_HI);
			rpsxpropSetRead(_Rs_);
			rpsxpropSetRead(_Rt_);
			break;
		case 32: case 33: case 34: case 35: // ADD, ADDU, SUB, SUBU
			rpsxpropSetWrite(_Rd_);
			if (_Rs_) rpsxpropSetRead(_Rs_);
			if (_Rt_) rpsxpropSetRead(_Rt_);
			break;
		default:
			rpsxpropSetWrite(_Rd_);
			rpsxpropSetRead(_Rs_);
			rpsxpropSetRead(_Rt_);
			break;
	}
}

void rpsxpropREGIMM(EEINST* prev, EEINST* pinst)
{
	switch (_Rt_)
	{
		case 0: case 1: // BLTZ, BGEZ
			rpsxpropSetRead(_Rs_);
			break;
		case 16: case 17: // BLTZAL, BGEZAL
			rpsxpropSetRead(_Rs_);
			break;
		default:
			break;
	}
}

void rpsxpropCP0(EEINST* prev, EEINST* pinst)
{
	switch (_Rs_)
	{
		case 0: case 2: // MFC0, CFC0
			rpsxpropSetWrite(_Rt_);
			break;
		case 4: case 6: // MTC0, CTC0
			rpsxpropSetRead(_Rt_);
			break;
		case 16: break; // RFE
		default: break;
	}
}

static void rpsxpropCP2_basic(EEINST* prev, EEINST* pinst)
{
	switch (_Rs_)
	{
		case 0: case 2: // MFC2, CFC2
			rpsxpropSetWrite(_Rt_);
			break;
		case 4: case 6: // MTC2, CTC2
			rpsxpropSetRead(_Rt_);
			break;
		default: break;
	}
}

void rpsxpropCP2(EEINST* prev, EEINST* pinst)
{
	switch (_Funct_)
	{
		case 0: rpsxpropCP2_basic(prev, pinst); break;
		default: break;
	}
}
