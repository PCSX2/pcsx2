// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <ctime>

#include "iR3000A.h"
#include "IopMem.h"
#include "IopDma.h"
#include "IopGte.h"

#include "common/Console.h"

using namespace x86Emitter;

extern int g_psxWriteOk;
extern u32 g_psxMaxRecMem;

// R3000A instruction implementation
#define REC_FUNC(f) \
	static void rpsx##f() \
	{ \
		xMOV(ptr32[&psxRegs.code], (u32)psxRegs.code); \
		_psxFlushCall(FLUSH_EVERYTHING); \
		xFastCall((void*)(uptr)psx##f); \
		PSX_DEL_CONST(_Rt_); \
		/*	branch = 2; */ \
	}

// Same as above but with a different naming convension (to avoid various rename)
#define REC_GTE_FUNC(f) \
	static void rgte##f() \
	{ \
		xMOV(ptr32[&psxRegs.code], (u32)psxRegs.code); \
		_psxFlushCall(FLUSH_EVERYTHING); \
		xFastCall((void*)(uptr)gte##f); \
		PSX_DEL_CONST(_Rt_); \
		/*	branch = 2; */ \
	}

extern void psxLWL();
extern void psxLWR();
extern void psxSWL();
extern void psxSWR();

// TODO(Stenzek): Operate directly on mem when destination register is not live.
// Do we want aligned targets? Seems wasteful...
#ifdef PCSX2_DEBUG
#define x86SetJ32A x86SetJ32
#endif

static int rpsxAllocRegIfUsed(int reg, int mode)
{
	if (EEINST_USEDTEST(reg))
		return _allocX86reg(X86TYPE_PSX, reg, mode);
	else
		return _checkX86reg(X86TYPE_PSX, reg, mode);
}

static void rpsxMoveStoT(int info)
{
	if (EEREC_T == EEREC_S)
		return;

	if (info & PROCESS_EE_S)
		xMOV(xRegister32(EEREC_T), xRegister32(EEREC_S));
	else
		xMOV(xRegister32(EEREC_T), ptr32[&psxRegs.GPR.r[_Rs_]]);
}

static void rpsxMoveStoD(int info)
{
	if (EEREC_D == EEREC_S)
		return;

	if (info & PROCESS_EE_S)
		xMOV(xRegister32(EEREC_D), xRegister32(EEREC_S));
	else
		xMOV(xRegister32(EEREC_D), ptr32[&psxRegs.GPR.r[_Rs_]]);
}

static void rpsxMoveTtoD(int info)
{
	if (EEREC_D == EEREC_T)
		return;

	if (info & PROCESS_EE_T)
		xMOV(xRegister32(EEREC_D), xRegister32(EEREC_T));
	else
		xMOV(xRegister32(EEREC_D), ptr32[&psxRegs.GPR.r[_Rt_]]);
}

static void rpsxMoveSToECX(int info)
{
	if (info & PROCESS_EE_S)
		xMOV(ecx, xRegister32(EEREC_S));
	else
		xMOV(ecx, ptr32[&psxRegs.GPR.r[_Rs_]]);
}

static void rpsxCopyReg(int dest, int src)
{
	// try a simple rename first...
	const int roldsrc = _checkX86reg(X86TYPE_PSX, src, MODE_READ);
	if (roldsrc >= 0 && psxTryRenameReg(dest, src, roldsrc, 0, 0) >= 0)
		return;

	const int rdest = rpsxAllocRegIfUsed(dest, MODE_WRITE);
	if (PSX_IS_CONST1(src))
	{
		if (dest < 32)
		{
			g_psxConstRegs[dest] = g_psxConstRegs[src];
			PSX_SET_CONST(dest);
		}
		else
		{
			if (rdest >= 0)
				xMOV(xRegister32(rdest), g_psxConstRegs[src]);
			else
				xMOV(ptr32[&psxRegs.GPR.r[dest]], g_psxConstRegs[src]);
		}

		return;
	}

	if (dest < 32)
		PSX_DEL_CONST(dest);

	const int rsrc = rpsxAllocRegIfUsed(src, MODE_READ);
	if (rsrc >= 0 && rdest >= 0)
	{
		xMOV(xRegister32(rdest), xRegister32(rsrc));
	}
	else if (rdest >= 0)
	{
		xMOV(xRegister32(rdest), ptr32[&psxRegs.GPR.r[src]]);
	}
	else if (rsrc >= 0)
	{
		xMOV(ptr32[&psxRegs.GPR.r[dest]], xRegister32(rsrc));
	}
	else
	{
		xMOV(eax, ptr32[&psxRegs.GPR.r[src]]);
		xMOV(ptr32[&psxRegs.GPR.r[dest]], eax);
	}
}

////
static void rpsxADDIU_const()
{
	g_psxConstRegs[_Rt_] = g_psxConstRegs[_Rs_] + _Imm_;
}

static void rpsxADDIU_(int info)
{
	// Rt = Rs + Im
	rpsxMoveStoT(info);
	if (_Imm_ != 0)
		xADD(xRegister32(EEREC_T), _Imm_);
}

PSXRECOMPILE_CONSTCODE1(ADDIU, XMMINFO_WRITET | XMMINFO_READS);

void rpsxADDI() { rpsxADDIU(); }

//// SLTI
static void rpsxSLTI_const()
{
	g_psxConstRegs[_Rt_] = *(int*)&g_psxConstRegs[_Rs_] < _Imm_;
}

static void rpsxSLTI_(int info)
{
	const xRegister32 dreg((_Rt_ == _Rs_) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_T);
	xXOR(dreg, dreg);

	if (info & PROCESS_EE_S)
		xCMP(xRegister32(EEREC_S), _Imm_);
	else
		xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], _Imm_);

	xSETL(xRegister8(dreg));

	if (dreg.GetId() != EEREC_T)
	{
		std::swap(x86regs[dreg.GetId()], x86regs[EEREC_T]);
		_freeX86reg(EEREC_T);
	}
}

PSXRECOMPILE_CONSTCODE1(SLTI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_NORENAME);

//// SLTIU
static void rpsxSLTIU_const()
{
	g_psxConstRegs[_Rt_] = g_psxConstRegs[_Rs_] < (u32)_Imm_;
}

static void rpsxSLTIU_(int info)
{
	const xRegister32 dreg((_Rt_ == _Rs_) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_T);
	xXOR(dreg, dreg);

	if (info & PROCESS_EE_S)
		xCMP(xRegister32(EEREC_S), _Imm_);
	else
		xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], _Imm_);

	xSETB(xRegister8(dreg));

	if (dreg.GetId() != EEREC_T)
	{
		std::swap(x86regs[dreg.GetId()], x86regs[EEREC_T]);
		_freeX86reg(EEREC_T);
	}
}

PSXRECOMPILE_CONSTCODE1(SLTIU, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_NORENAME);

static void rpsxLogicalOpI(u64 info, int op)
{
	if (_ImmU_ != 0)
	{
		rpsxMoveStoT(info);
		switch (op)
		{
			case 0:
				xAND(xRegister32(EEREC_T), _ImmU_);
				break;
			case 1:
				xOR(xRegister32(EEREC_T), _ImmU_);
				break;
			case 2:
				xXOR(xRegister32(EEREC_T), _ImmU_);
				break;

				jNO_DEFAULT
		}
	}
	else
	{
		if (op == 0)
		{
			xXOR(xRegister32(EEREC_T), xRegister32(EEREC_T));
		}
		else if (EEREC_T != EEREC_S)
		{
			rpsxMoveStoT(info);
		}
	}
}

//// ANDI
static void rpsxANDI_const()
{
	g_psxConstRegs[_Rt_] = g_psxConstRegs[_Rs_] & _ImmU_;
}

static void rpsxANDI_(int info)
{
	rpsxLogicalOpI(info, 0);
}

PSXRECOMPILE_CONSTCODE1(ANDI, XMMINFO_WRITET | XMMINFO_READS);

//// ORI
static void rpsxORI_const()
{
	g_psxConstRegs[_Rt_] = g_psxConstRegs[_Rs_] | _ImmU_;
}

static void rpsxORI_(int info)
{
	rpsxLogicalOpI(info, 1);
}

PSXRECOMPILE_CONSTCODE1(ORI, XMMINFO_WRITET | XMMINFO_READS);

static void rpsxXORI_const()
{
	g_psxConstRegs[_Rt_] = g_psxConstRegs[_Rs_] ^ _ImmU_;
}

static void rpsxXORI_(int info)
{
	rpsxLogicalOpI(info, 2);
}

PSXRECOMPILE_CONSTCODE1(XORI, XMMINFO_WRITET | XMMINFO_READS);

void rpsxLUI()
{
	if (!_Rt_)
		return;
	_psxOnWriteReg(_Rt_);
	_psxDeleteReg(_Rt_, 0);
	PSX_SET_CONST(_Rt_);
	g_psxConstRegs[_Rt_] = psxRegs.code << 16;
}

static void rpsxADDU_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rs_] + g_psxConstRegs[_Rt_];
}

static void rpsxADDU_consts(int info)
{
	const s32 cval = static_cast<s32>(g_psxConstRegs[_Rs_]);
	rpsxMoveTtoD(info);
	if (cval != 0)
		xADD(xRegister32(EEREC_D), cval);
}

static void rpsxADDU_constt(int info)
{
	const s32 cval = static_cast<s32>(g_psxConstRegs[_Rt_]);
	rpsxMoveStoD(info);
	if (cval != 0)
		xADD(xRegister32(EEREC_D), cval);
}

void rpsxADDU_(int info)
{
	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			xADD(xRegister32(EEREC_D), xRegister32(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			xADD(xRegister32(EEREC_D), xRegister32(EEREC_S));
		}
		else
		{
			xMOV(xRegister32(EEREC_D), xRegister32(EEREC_S));
			xADD(xRegister32(EEREC_D), xRegister32(EEREC_T));
		}
	}
	else if (info & PROCESS_EE_S)
	{
		xMOV(xRegister32(EEREC_D), xRegister32(EEREC_S));
		xADD(xRegister32(EEREC_D), ptr32[&psxRegs.GPR.r[_Rt_]]);
	}
	else if (info & PROCESS_EE_T)
	{
		xMOV(xRegister32(EEREC_D), xRegister32(EEREC_T));
		xADD(xRegister32(EEREC_D), ptr32[&psxRegs.GPR.r[_Rs_]]);
	}
	else
	{
		xMOV(xRegister32(EEREC_D), ptr32[&psxRegs.GPR.r[_Rs_]]);
		xADD(xRegister32(EEREC_D), ptr32[&psxRegs.GPR.r[_Rt_]]);
	}
}

PSXRECOMPILE_CONSTCODE0(ADDU, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

void rpsxADD() { rpsxADDU(); }

static void rpsxSUBU_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rs_] - g_psxConstRegs[_Rt_];
}

static void rpsxSUBU_consts(int info)
{
	// more complex because Rt can be Rd, and we're reversing the op
	const s32 sval = g_psxConstRegs[_Rs_];
	const xRegister32 dreg((_Rt_ == _Rd_) ? eax.GetId() : EEREC_D);
	xMOV(dreg, sval);

	if (info & PROCESS_EE_T)
		xSUB(dreg, xRegister32(EEREC_T));
	else
		xSUB(dreg, ptr32[&psxRegs.GPR.r[_Rt_]]);

	xMOV(xRegister32(EEREC_D), dreg);
}

static void rpsxSUBU_constt(int info)
{
	const s32 tval = g_psxConstRegs[_Rt_];
	rpsxMoveStoD(info);
	if (tval != 0)
		xSUB(xRegister32(EEREC_D), tval);
}

static void rpsxSUBU_(int info)
{
	// Rd = Rs - Rt
	if (_Rs_ == _Rt_)
	{
		xXOR(xRegister32(EEREC_D), xRegister32(EEREC_D));
		return;
	}

	// a bit messier here because it's not commutative..
	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			xSUB(xRegister32(EEREC_D), xRegister32(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			// D might equal T
			const xRegister32 dreg((_Rt_ == _Rd_) ? eax.GetId() : EEREC_D);
			xMOV(dreg, xRegister32(EEREC_S));
			xSUB(dreg, xRegister32(EEREC_T));
			xMOV(xRegister32(EEREC_D), dreg);
		}
		else
		{
			xMOV(xRegister32(EEREC_D), xRegister32(EEREC_S));
			xSUB(xRegister32(EEREC_D), xRegister32(EEREC_T));
		}
	}
	else if (info & PROCESS_EE_S)
	{
		xMOV(xRegister32(EEREC_D), xRegister32(EEREC_S));
		xSUB(xRegister32(EEREC_D), ptr32[&psxRegs.GPR.r[_Rt_]]);
	}
	else if (info & PROCESS_EE_T)
	{
		// D might equal T
		const xRegister32 dreg((_Rt_ == _Rd_) ? eax.GetId() : EEREC_D);
		xMOV(dreg, ptr32[&psxRegs.GPR.r[_Rs_]]);
		xSUB(dreg, xRegister32(EEREC_T));
		xMOV(xRegister32(EEREC_D), dreg);
	}
	else
	{
		xMOV(xRegister32(EEREC_D), ptr32[&psxRegs.GPR.r[_Rs_]]);
		xSUB(xRegister32(EEREC_D), ptr32[&psxRegs.GPR.r[_Rt_]]);
	}
}

PSXRECOMPILE_CONSTCODE0(SUBU, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

void rpsxSUB() { rpsxSUBU(); }

namespace
{
	enum class LogicalOp
	{
		AND,
		OR,
		XOR,
		NOR
	};
} // namespace

static void rpsxLogicalOp_constv(LogicalOp op, int info, int creg, u32 vreg, int regv)
{
	xImpl_G1Logic bad{};
	const xImpl_G1Logic& xOP = op == LogicalOp::AND ? xAND : op == LogicalOp::OR ? xOR :
														 op == LogicalOp::XOR    ? xXOR :
														 op == LogicalOp::NOR    ? xOR :
                                                                                   bad;
	s32 fixedInput, fixedOutput, identityInput;
	bool hasFixed = true;
	switch (op)
	{
		case LogicalOp::AND:
			fixedInput = 0;
			fixedOutput = 0;
			identityInput = -1;
			break;
		case LogicalOp::OR:
			fixedInput = -1;
			fixedOutput = -1;
			identityInput = 0;
			break;
		case LogicalOp::XOR:
			hasFixed = false;
			identityInput = 0;
			break;
		case LogicalOp::NOR:
			fixedInput = -1;
			fixedOutput = 0;
			identityInput = 0;
			break;
		default:
			pxAssert(0);
	}

	const s32 cval = static_cast<s32>(g_psxConstRegs[creg]);

	if (hasFixed && cval == fixedInput)
	{
		xMOV(xRegister32(EEREC_D), fixedOutput);
	}
	else
	{
		if (regv >= 0)
			xMOV(xRegister32(EEREC_D), xRegister32(regv));
		else
			xMOV(xRegister32(EEREC_D), ptr32[&psxRegs.GPR.r[vreg]]);
		if (cval != identityInput)
			xOP(xRegister32(EEREC_D), cval);
		if (op == LogicalOp::NOR)
			xNOT(xRegister32(EEREC_D));
	}
}

static void rpsxLogicalOp(LogicalOp op, int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xImpl_G1Logic bad{};
	const xImpl_G1Logic& xOP = op == LogicalOp::AND ? xAND : op == LogicalOp::OR ? xOR :
														 op == LogicalOp::XOR    ? xXOR :
														 op == LogicalOp::NOR    ? xOR :
                                                                                   bad;
	pxAssert(&xOP != &bad);

	// swap because it's commutative and Rd might be Rt
	u32 rs = _Rs_, rt = _Rt_;
	int regs = (info & PROCESS_EE_S) ? EEREC_S : -1, regt = (info & PROCESS_EE_T) ? EEREC_T : -1;
	if (_Rd_ == _Rt_)
	{
		std::swap(rs, rt);
		std::swap(regs, regt);
	}

	if (op == LogicalOp::XOR && rs == rt)
	{
		xXOR(xRegister32(EEREC_D), xRegister32(EEREC_D));
	}
	else
	{
		if (regs >= 0)
			xMOV(xRegister32(EEREC_D), xRegister32(regs));
		else
			xMOV(xRegister32(EEREC_D), ptr32[&psxRegs.GPR.r[rs]]);

		if (regt >= 0)
			xOP(xRegister32(EEREC_D), xRegister32(regt));
		else
			xOP(xRegister32(EEREC_D), ptr32[&psxRegs.GPR.r[rt]]);

		if (op == LogicalOp::NOR)
			xNOT(xRegister32(EEREC_D));
	}
}

static void rpsxAND_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rs_] & g_psxConstRegs[_Rt_];
}

static void rpsxAND_consts(int info)
{
	rpsxLogicalOp_constv(LogicalOp::AND, info, _Rs_, _Rt_, (info & PROCESS_EE_T) ? EEREC_T : -1);
}

static void rpsxAND_constt(int info)
{
	rpsxLogicalOp_constv(LogicalOp::AND, info, _Rt_, _Rs_, (info & PROCESS_EE_S) ? EEREC_S : -1);
}

static void rpsxAND_(int info)
{
	rpsxLogicalOp(LogicalOp::AND, info);
}

PSXRECOMPILE_CONSTCODE0(AND, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

static void rpsxOR_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rs_] | g_psxConstRegs[_Rt_];
}

static void rpsxOR_consts(int info)
{
	rpsxLogicalOp_constv(LogicalOp::OR, info, _Rs_, _Rt_, (info & PROCESS_EE_T) ? EEREC_T : -1);
}

static void rpsxOR_constt(int info)
{
	rpsxLogicalOp_constv(LogicalOp::OR, info, _Rt_, _Rs_, (info & PROCESS_EE_S) ? EEREC_S : -1);
}

static void rpsxOR_(int info)
{
	rpsxLogicalOp(LogicalOp::OR, info);
}

PSXRECOMPILE_CONSTCODE0(OR, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

//// XOR
static void rpsxXOR_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rs_] ^ g_psxConstRegs[_Rt_];
}

static void rpsxXOR_consts(int info)
{
	rpsxLogicalOp_constv(LogicalOp::XOR, info, _Rs_, _Rt_, (info & PROCESS_EE_T) ? EEREC_T : -1);
}

static void rpsxXOR_constt(int info)
{
	rpsxLogicalOp_constv(LogicalOp::XOR, info, _Rt_, _Rs_, (info & PROCESS_EE_S) ? EEREC_S : -1);
}

static void rpsxXOR_(int info)
{
	rpsxLogicalOp(LogicalOp::XOR, info);
}

PSXRECOMPILE_CONSTCODE0(XOR, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

//// NOR
static void rpsxNOR_const()
{
	g_psxConstRegs[_Rd_] = ~(g_psxConstRegs[_Rs_] | g_psxConstRegs[_Rt_]);
}

static void rpsxNOR_consts(int info)
{
	rpsxLogicalOp_constv(LogicalOp::NOR, info, _Rs_, _Rt_, (info & PROCESS_EE_T) ? EEREC_T : -1);
}

static void rpsxNOR_constt(int info)
{
	rpsxLogicalOp_constv(LogicalOp::NOR, info, _Rt_, _Rs_, (info & PROCESS_EE_S) ? EEREC_S : -1);
}

static void rpsxNOR_(int info)
{
	rpsxLogicalOp(LogicalOp::NOR, info);
}

PSXRECOMPILE_CONSTCODE0(NOR, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

//// SLT
static void rpsxSLT_const()
{
	g_psxConstRegs[_Rd_] = *(int*)&g_psxConstRegs[_Rs_] < *(int*)&g_psxConstRegs[_Rt_];
}

static void rpsxSLTs_const(int info, int sign, int st)
{
	const s32 cval = g_psxConstRegs[st ? _Rt_ : _Rs_];

	const xImpl_Set& SET = st ? (sign ? xSETL : xSETB) : (sign ? xSETG : xSETA);

	const xRegister32 dreg((_Rd_ == (st ? _Rs_ : _Rt_)) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_D);
	const int regs = st ? ((info & PROCESS_EE_S) ? EEREC_S : -1) : ((info & PROCESS_EE_T) ? EEREC_T : -1);
	xXOR(dreg, dreg);

	if (regs >= 0)
		xCMP(xRegister32(regs), cval);
	else
		xCMP(ptr32[&psxRegs.GPR.r[st ? _Rs_ : _Rt_]], cval);
	SET(xRegister8(dreg));

	if (dreg.GetId() != EEREC_D)
	{
		std::swap(x86regs[dreg.GetId()], x86regs[EEREC_D]);
		_freeX86reg(EEREC_D);
	}
}

static void rpsxSLTs_(int info, int sign)
{
	const xImpl_Set& SET = sign ? xSETL : xSETB;

	// need to keep Rs/Rt around.
	const xRegister32 dreg((_Rd_ == _Rt_ || _Rd_ == _Rs_) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_D);

	// force Rs into a register, may as well cache it since we're loading anyway.
	const int regs = (info & PROCESS_EE_S) ? EEREC_S : _allocX86reg(X86TYPE_PSX, _Rs_, MODE_READ);

	xXOR(dreg, dreg);
	if (info & PROCESS_EE_T)
		xCMP(xRegister32(regs), xRegister32(EEREC_T));
	else
		xCMP(xRegister32(regs), ptr32[&psxRegs.GPR.r[_Rt_]]);

	SET(xRegister8(dreg));

	if (dreg.GetId() != EEREC_D)
	{
		std::swap(x86regs[dreg.GetId()], x86regs[EEREC_D]);
		_freeX86reg(EEREC_D);
	}
}

static void rpsxSLT_consts(int info)
{
	rpsxSLTs_const(info, 1, 0);
}

static void rpsxSLT_constt(int info)
{
	rpsxSLTs_const(info, 1, 1);
}

static void rpsxSLT_(int info)
{
	rpsxSLTs_(info, 1);
}

PSXRECOMPILE_CONSTCODE0(SLT, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_NORENAME);

//// SLTU
static void rpsxSLTU_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rs_] < g_psxConstRegs[_Rt_];
}

static void rpsxSLTU_consts(int info)
{
	rpsxSLTs_const(info, 0, 0);
}

static void rpsxSLTU_constt(int info)
{
	rpsxSLTs_const(info, 0, 1);
}

static void rpsxSLTU_(int info)
{
	rpsxSLTs_(info, 0);
}

PSXRECOMPILE_CONSTCODE0(SLTU, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_NORENAME);

//// MULT
static void rpsxMULT_const()
{
	_deletePSXtoX86reg(PSX_HI, DELETE_REG_FREE_NO_WRITEBACK);
	_deletePSXtoX86reg(PSX_LO, DELETE_REG_FREE_NO_WRITEBACK);

	u64 res = (s64)((s64) * (int*)&g_psxConstRegs[_Rs_] * (s64) * (int*)&g_psxConstRegs[_Rt_]);

	xMOV(ptr32[&psxRegs.GPR.n.hi], (u32)((res >> 32) & 0xffffffff));
	xMOV(ptr32[&psxRegs.GPR.n.lo], (u32)(res & 0xffffffff));
}

static void rpsxWritebackHILO(int info)
{
	if (EEINST_LIVETEST(PSX_LO))
	{
		if (info & PROCESS_EE_LO)
			xMOV(xRegister32(EEREC_LO), eax);
		else
			xMOV(ptr32[&psxRegs.GPR.n.lo], eax);
	}

	if (EEINST_LIVETEST(PSX_HI))
	{
		if (info & PROCESS_EE_HI)
			xMOV(xRegister32(EEREC_HI), edx);
		else
			xMOV(ptr32[&psxRegs.GPR.n.hi], edx);
	}
}

static void rpsxMULTsuperconst(int info, int sreg, int imm, int sign)
{
	// Lo/Hi = Rs * Rt (signed)
	xMOV(eax, imm);

	const int regs = rpsxAllocRegIfUsed(sreg, MODE_READ);
	if (sign)
	{
		if (regs >= 0)
			xMUL(xRegister32(regs));
		else
			xMUL(ptr32[&psxRegs.GPR.r[sreg]]);
	}
	else
	{
		if (regs >= 0)
			xUMUL(xRegister32(regs));
		else
			xUMUL(ptr32[&psxRegs.GPR.r[sreg]]);
	}

	rpsxWritebackHILO(info);
}

static void rpsxMULTsuper(int info, int sign)
{
	// Lo/Hi = Rs * Rt (signed)
	_psxMoveGPRtoR(eax, _Rs_);

	const int regt = rpsxAllocRegIfUsed(_Rt_, MODE_READ);
	if (sign)
	{
		if (regt >= 0)
			xMUL(xRegister32(regt));
		else
			xMUL(ptr32[&psxRegs.GPR.r[_Rt_]]);
	}
	else
	{
		if (regt >= 0)
			xUMUL(xRegister32(regt));
		else
			xUMUL(ptr32[&psxRegs.GPR.r[_Rt_]]);
	}

	rpsxWritebackHILO(info);
}

static void rpsxMULT_consts(int info)
{
	rpsxMULTsuperconst(info, _Rt_, g_psxConstRegs[_Rs_], 1);
}

static void rpsxMULT_constt(int info)
{
	rpsxMULTsuperconst(info, _Rs_, g_psxConstRegs[_Rt_], 1);
}

static void rpsxMULT_(int info)
{
	rpsxMULTsuper(info, 1);
}

PSXRECOMPILE_CONSTCODE3_PENALTY(MULT, 1, psxInstCycles_Mult);

//// MULTU
static void rpsxMULTU_const()
{
	_deletePSXtoX86reg(PSX_HI, DELETE_REG_FREE_NO_WRITEBACK);
	_deletePSXtoX86reg(PSX_LO, DELETE_REG_FREE_NO_WRITEBACK);

	u64 res = (u64)((u64)g_psxConstRegs[_Rs_] * (u64)g_psxConstRegs[_Rt_]);

	xMOV(ptr32[&psxRegs.GPR.n.hi], (u32)((res >> 32) & 0xffffffff));
	xMOV(ptr32[&psxRegs.GPR.n.lo], (u32)(res & 0xffffffff));
}

static void rpsxMULTU_consts(int info)
{
	rpsxMULTsuperconst(info, _Rt_, g_psxConstRegs[_Rs_], 0);
}

static void rpsxMULTU_constt(int info)
{
	rpsxMULTsuperconst(info, _Rs_, g_psxConstRegs[_Rt_], 0);
}

static void rpsxMULTU_(int info)
{
	rpsxMULTsuper(info, 0);
}

PSXRECOMPILE_CONSTCODE3_PENALTY(MULTU, 1, psxInstCycles_Mult);

//// DIV
static void rpsxDIV_const()
{
	_deletePSXtoX86reg(PSX_HI, DELETE_REG_FREE_NO_WRITEBACK);
	_deletePSXtoX86reg(PSX_LO, DELETE_REG_FREE_NO_WRITEBACK);

	u32 lo, hi;

	/*
	 * Normally, when 0x80000000(-2147483648), the signed minimum value, is divided by 0xFFFFFFFF(-1), the
	 * 	operation will result in overflow. However, in this instruction an overflow exception does not occur and the
	 * 	result will be as follows:
	 * 	Quotient: 0x80000000 (-2147483648), and remainder: 0x00000000 (0)
	 */
	// Of course x86 cpu does overflow !
	if (g_psxConstRegs[_Rs_] == 0x80000000u && g_psxConstRegs[_Rt_] == 0xFFFFFFFFu)
	{
		xMOV(ptr32[&psxRegs.GPR.n.hi], 0);
		xMOV(ptr32[&psxRegs.GPR.n.lo], 0x80000000);
		return;
	}

	if (g_psxConstRegs[_Rt_] != 0)
	{
		lo = *(int*)&g_psxConstRegs[_Rs_] / *(int*)&g_psxConstRegs[_Rt_];
		hi = *(int*)&g_psxConstRegs[_Rs_] % *(int*)&g_psxConstRegs[_Rt_];
		xMOV(ptr32[&psxRegs.GPR.n.hi], hi);
		xMOV(ptr32[&psxRegs.GPR.n.lo], lo);
	}
	else
	{
		xMOV(ptr32[&psxRegs.GPR.n.hi], g_psxConstRegs[_Rs_]);
		if (g_psxConstRegs[_Rs_] & 0x80000000u)
		{
			xMOV(ptr32[&psxRegs.GPR.n.lo], 0x1);
		}
		else
		{
			xMOV(ptr32[&psxRegs.GPR.n.lo], 0xFFFFFFFFu);
		}
	}
}

static void rpsxDIVsuper(int info, int sign, int process = 0)
{
	// Lo/Hi = Rs / Rt (signed)
	if (process & PROCESS_CONSTT)
		xMOV(ecx, g_psxConstRegs[_Rt_]);
	else if (info & PROCESS_EE_T)
		xMOV(ecx, xRegister32(EEREC_T));
	else
		xMOV(ecx, ptr32[&psxRegs.GPR.r[_Rt_]]);

	if (process & PROCESS_CONSTS)
		xMOV(eax, g_psxConstRegs[_Rs_]);
	else if (info & PROCESS_EE_S)
		xMOV(eax, xRegister32(EEREC_S));
	else
		xMOV(eax, ptr32[&psxRegs.GPR.r[_Rs_]]);

	u8* end1;
	if (sign) //test for overflow (x86 will just throw an exception)
	{
		xCMP(eax, 0x80000000);
		u8* cont1 = JNE8(0);
		xCMP(ecx, 0xffffffff);
		u8* cont2 = JNE8(0);
		//overflow case:
		xXOR(edx, edx); //EAX remains 0x80000000
		end1 = JMP8(0);

		x86SetJ8(cont1);
		x86SetJ8(cont2);
	}

	xCMP(ecx, 0);
	u8* cont3 = JNE8(0);

	//divide by zero
	xMOV(edx, eax);
	if (sign) //set EAX to (EAX < 0)?1:-1
	{
		xSAR(eax, 31); //(EAX < 0)?-1:0
		xSHL(eax, 1); //(EAX < 0)?-2:0
		xNOT(eax); //(EAX < 0)?1:-1
	}
	else
		xMOV(eax, 0xffffffff);
	u8* end2 = JMP8(0);

	// Normal division
	x86SetJ8(cont3);
	if (sign)
	{
		xCDQ();
		xDIV(ecx);
	}
	else
	{
		xXOR(edx, edx);
		xUDIV(ecx);
	}

	if (sign)
		x86SetJ8(end1);
	x86SetJ8(end2);

	rpsxWritebackHILO(info);
}

static void rpsxDIV_consts(int info)
{
	rpsxDIVsuper(info, 1, PROCESS_CONSTS);
}

static void rpsxDIV_constt(int info)
{
	rpsxDIVsuper(info, 1, PROCESS_CONSTT);
}

static void rpsxDIV_(int info)
{
	rpsxDIVsuper(info, 1);
}

PSXRECOMPILE_CONSTCODE3_PENALTY(DIV, 1, psxInstCycles_Div);

//// DIVU
void rpsxDIVU_const()
{
	u32 lo, hi;

	_deletePSXtoX86reg(PSX_HI, DELETE_REG_FREE_NO_WRITEBACK);
	_deletePSXtoX86reg(PSX_LO, DELETE_REG_FREE_NO_WRITEBACK);

	if (g_psxConstRegs[_Rt_] != 0)
	{
		lo = g_psxConstRegs[_Rs_] / g_psxConstRegs[_Rt_];
		hi = g_psxConstRegs[_Rs_] % g_psxConstRegs[_Rt_];
		xMOV(ptr32[&psxRegs.GPR.n.hi], hi);
		xMOV(ptr32[&psxRegs.GPR.n.lo], lo);
	}
	else
	{
		xMOV(ptr32[&psxRegs.GPR.n.hi], g_psxConstRegs[_Rs_]);
		xMOV(ptr32[&psxRegs.GPR.n.lo], 0xFFFFFFFFu);
	}
}

void rpsxDIVU_consts(int info)
{
	rpsxDIVsuper(info, 0, PROCESS_CONSTS);
}

void rpsxDIVU_constt(int info)
{
	rpsxDIVsuper(info, 0, PROCESS_CONSTT);
}

void rpsxDIVU_(int info)
{
	rpsxDIVsuper(info, 0);
}

PSXRECOMPILE_CONSTCODE3_PENALTY(DIVU, 1, psxInstCycles_Div);

// TLB loadstore functions

static u8* rpsxGetConstantAddressOperand(bool store)
{
#if 0
	if (!PSX_IS_CONST1(_Rs_))
		return nullptr;

	const u32 addr = g_psxConstRegs[_Rs_];
	return store ? iopVirtMemW<u8>(addr) : const_cast<u8*>(iopVirtMemR<u8>(addr));
#else
	return nullptr;
#endif
}

static void rpsxCalcAddressOperand()
{
	// if it's a const register, just flush it, since we'll need to do that
	// when we call the load/store function anyway
	int rs;
	if (PSX_IS_CONST1(_Rs_))
		rs = _allocX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
	else
		rs = _checkX86reg(X86TYPE_PSX, _Rs_, MODE_READ);

	_freeX86reg(arg1regd);

	if (rs >= 0)
		xMOV(arg1regd, xRegister32(rs));
	else
		xMOV(arg1regd, ptr32[&psxRegs.GPR.r[_Rs_]]);

	if (_Imm_)
		xADD(arg1regd, _Imm_);
}

static void rpsxCalcStoreOperand()
{
	int rt;
	if (PSX_IS_CONST1(_Rt_))
		rt = _allocX86reg(X86TYPE_PSX, _Rt_, MODE_READ);
	else
		rt = _checkX86reg(X86TYPE_PSX, _Rt_, MODE_READ);

	_freeX86reg(arg2regd);

	if (rt >= 0)
		xMOV(arg2regd, xRegister32(rt));
	else
		xMOV(arg2regd, ptr32[&psxRegs.GPR.r[_Rt_]]);
}

static void rpsxLoad(int size, bool sign)
{
	rpsxCalcAddressOperand();

	if (_Rt_ != 0)
	{
		PSX_DEL_CONST(_Rt_);
		_deletePSXtoX86reg(_Rt_, DELETE_REG_FREE_NO_WRITEBACK);
	}

	_psxFlushCall(FLUSH_FULLVTLB);
	xTEST(arg1regd, 0x10000000);
	xForwardJZ8 is_ram_read;

	switch (size)
	{
		case 8:
			xFastCall((void*)iopMemRead8);
			break;
		case 16:
			xFastCall((void*)iopMemRead16);
			break;
		case 32:
			xFastCall((void*)iopMemRead32);
			break;

			jNO_DEFAULT
	}

	if (_Rt_ == 0)
	{
		// dummy read
		is_ram_read.SetTarget();
		return;
	}

	xForwardJump8 done;
	is_ram_read.SetTarget();

	// read from psM directly
	xAND(arg1regd, 0x1fffff);

	auto addr = xComplexAddress(rax, iopMem->Main, arg1reg);
	switch (size)
	{
		case 8:
			xMOVZX(eax, ptr8[addr]);
			break;
		case 16:
			xMOVZX(eax, ptr16[addr]);
			break;
		case 32:
			xMOV(eax, ptr32[addr]);
			break;

			jNO_DEFAULT
	}

	done.SetTarget();

	const int rt = rpsxAllocRegIfUsed(_Rt_, MODE_WRITE);
	const xRegister32 dreg((rt < 0) ? eax.GetId() : rt);

	// sign/zero extend as needed
	switch (size)
	{
		case 8:
			sign ? xMOVSX(dreg, al) : xMOVZX(dreg, al);
			break;
		case 16:
			sign ? xMOVSX(dreg, ax) : xMOVZX(dreg, ax);
			break;
		case 32:
			xMOV(dreg, eax);
			break;
			jNO_DEFAULT
	}

	// if not caching, write back
	if (rt < 0)
		xMOV(ptr32[&psxRegs.GPR.r[_Rt_]], eax);
}


REC_FUNC(LWL);
REC_FUNC(LWR);
REC_FUNC(SWL);
REC_FUNC(SWR);

static void rpsxLB()
{
	rpsxLoad(8, true);
}

static void rpsxLBU()
{
	rpsxLoad(8, false);
}

static void rpsxLH()
{
	rpsxLoad(16, true);
}

static void rpsxLHU()
{
	rpsxLoad(16, false);
}

static void rpsxLW()
{
	rpsxLoad(32, false);
}

static void rpsxSB()
{
	rpsxCalcAddressOperand();
	rpsxCalcStoreOperand();
	_psxFlushCall(FLUSH_FULLVTLB);
	xFastCall((void*)iopMemWrite8);
}

static void rpsxSH()
{
	rpsxCalcAddressOperand();
	rpsxCalcStoreOperand();
	_psxFlushCall(FLUSH_FULLVTLB);
	xFastCall((void*)iopMemWrite16);
}

static void rpsxSW()
{
	u8* ptr = rpsxGetConstantAddressOperand(true);
	if (ptr)
	{
		const int rt = _allocX86reg(X86TYPE_PSX, _Rt_, MODE_READ);
		xMOV(ptr32[ptr], xRegister32(rt));
		return;
	}

	rpsxCalcAddressOperand();
	rpsxCalcStoreOperand();
	_psxFlushCall(FLUSH_FULLVTLB);
	xFastCall((void*)iopMemWrite32);
}

//// SLL
static void rpsxSLL_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rt_] << _Sa_;
}

static void rpsxSLLs_(int info, int sa)
{
	rpsxMoveTtoD(info);
	if (sa != 0)
		xSHL(xRegister32(EEREC_D), sa);
}

static void rpsxSLL_(int info)
{
	rpsxSLLs_(info, _Sa_);
}

PSXRECOMPILE_CONSTCODE2(SLL, XMMINFO_WRITED | XMMINFO_READS);

//// SRL
static void rpsxSRL_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rt_] >> _Sa_;
}

static void rpsxSRLs_(int info, int sa)
{
	rpsxMoveTtoD(info);
	if (sa != 0)
		xSHR(xRegister32(EEREC_D), sa);
}

static void rpsxSRL_(int info)
{
	rpsxSRLs_(info, _Sa_);
}

PSXRECOMPILE_CONSTCODE2(SRL, XMMINFO_WRITED | XMMINFO_READS);

//// SRA
static void rpsxSRA_const()
{
	g_psxConstRegs[_Rd_] = *(int*)&g_psxConstRegs[_Rt_] >> _Sa_;
}

static void rpsxSRAs_(int info, int sa)
{
	rpsxMoveTtoD(info);
	if (sa != 0)
		xSAR(xRegister32(EEREC_D), sa);
}

static void rpsxSRA_(int info)
{
	rpsxSRAs_(info, _Sa_);
}

PSXRECOMPILE_CONSTCODE2(SRA, XMMINFO_WRITED | XMMINFO_READS);

//// SLLV
static void rpsxShiftV_constt(int info, const xImpl_Group2& shift)
{
	pxAssert(_Rs_ != 0);
	rpsxMoveSToECX(info);
	xMOV(xRegister32(EEREC_D), g_psxConstRegs[_Rt_]);
	shift(xRegister32(EEREC_D), cl);
}

static void rpsxShiftV(int info, const xImpl_Group2& shift)
{
	pxAssert(_Rs_ != 0);

	rpsxMoveSToECX(info);
	rpsxMoveTtoD(info);
	shift(xRegister32(EEREC_D), cl);
}

static void rpsxSLLV_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rt_] << (g_psxConstRegs[_Rs_] & 0x1f);
}

static void rpsxSLLV_consts(int info)
{
	rpsxSLLs_(info, g_psxConstRegs[_Rs_] & 0x1f);
}

static void rpsxSLLV_constt(int info)
{
	rpsxShiftV_constt(info, xSHL);
}

static void rpsxSLLV_(int info)
{
	rpsxShiftV(info, xSHL);
}

PSXRECOMPILE_CONSTCODE0(SLLV, XMMINFO_WRITED | XMMINFO_READS);

//// SRLV
static void rpsxSRLV_const()
{
	g_psxConstRegs[_Rd_] = g_psxConstRegs[_Rt_] >> (g_psxConstRegs[_Rs_] & 0x1f);
}

static void rpsxSRLV_consts(int info)
{
	rpsxSRLs_(info, g_psxConstRegs[_Rs_] & 0x1f);
}

static void rpsxSRLV_constt(int info)
{
	rpsxShiftV_constt(info, xSHR);
}

static void rpsxSRLV_(int info)
{
	rpsxShiftV(info, xSHR);
}

PSXRECOMPILE_CONSTCODE0(SRLV, XMMINFO_WRITED | XMMINFO_READS);

//// SRAV
static void rpsxSRAV_const()
{
	g_psxConstRegs[_Rd_] = *(int*)&g_psxConstRegs[_Rt_] >> (g_psxConstRegs[_Rs_] & 0x1f);
}

static void rpsxSRAV_consts(int info)
{
	rpsxSRAs_(info, g_psxConstRegs[_Rs_] & 0x1f);
}

static void rpsxSRAV_constt(int info)
{
	rpsxShiftV_constt(info, xSAR);
}

static void rpsxSRAV_(int info)
{
	rpsxShiftV(info, xSAR);
}

PSXRECOMPILE_CONSTCODE0(SRAV, XMMINFO_WRITED | XMMINFO_READS);

extern void rpsxSYSCALL();
extern void rpsxBREAK();

static void rpsxMFHI()
{
	if (!_Rd_)
		return;

	rpsxCopyReg(_Rd_, PSX_HI);
}

static void rpsxMTHI()
{
	rpsxCopyReg(PSX_HI, _Rs_);
}

static void rpsxMFLO()
{
	if (!_Rd_)
		return;

	rpsxCopyReg(_Rd_, PSX_LO);
}

static void rpsxMTLO()
{
	rpsxCopyReg(PSX_LO, _Rs_);
}

static void rpsxJ()
{
	// j target
	u32 newpc = _InstrucTarget_ * 4 + (psxpc & 0xf0000000);
	psxRecompileNextInstruction(true, false);
	psxSetBranchImm(newpc);
}

static void rpsxJAL()
{
	u32 newpc = (_InstrucTarget_ << 2) + (psxpc & 0xf0000000);
	_psxDeleteReg(31, DELETE_REG_FREE_NO_WRITEBACK);
	PSX_SET_CONST(31);
	g_psxConstRegs[31] = psxpc + 4;

	psxRecompileNextInstruction(true, false);
	psxSetBranchImm(newpc);
}

static void rpsxJR()
{
	psxSetBranchReg(_Rs_);
}

static void rpsxJALR()
{
	const u32 newpc = psxpc + 4;
	const bool swap = (_Rd_ == _Rs_) ? false : psxTrySwapDelaySlot(_Rs_, 0, _Rd_);

	// jalr Rs
	int wbreg = -1;
	if (!swap)
	{
		wbreg = _allocX86reg(X86TYPE_PCWRITEBACK, 0, MODE_WRITE | MODE_CALLEESAVED);
		_psxMoveGPRtoR(xRegister32(wbreg), _Rs_);
	}

	if (_Rd_)
	{
		_psxDeleteReg(_Rd_, DELETE_REG_FREE_NO_WRITEBACK);
		PSX_SET_CONST(_Rd_);
		g_psxConstRegs[_Rd_] = newpc;
	}

	if (!swap)
	{
		psxRecompileNextInstruction(true, false);

		if (x86regs[wbreg].inuse && x86regs[wbreg].type == X86TYPE_PCWRITEBACK)
		{
			xMOV(ptr32[&psxRegs.pc], xRegister32(wbreg));
			x86regs[wbreg].inuse = 0;
		}
		else
		{
			xMOV(eax, ptr32[&psxRegs.pcWriteback]);
			xMOV(ptr32[&psxRegs.pc], eax);
		}
	}
	else
	{
		if (PSX_IS_DIRTY_CONST(_Rs_) || _hasX86reg(X86TYPE_PSX, _Rs_, 0))
		{
			const int x86reg = _allocX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
			xMOV(ptr32[&psxRegs.pc], xRegister32(x86reg));
		}
		else
		{
			_psxMoveGPRtoM((uptr)&psxRegs.pc, _Rs_);
		}
	}

	psxSetBranchReg(0xffffffff);
}

//// BEQ
static u32* s_pbranchjmp;

static void rpsxSetBranchEQ(int process)
{
	if (process & PROCESS_CONSTS)
	{
		const int regt = _checkX86reg(X86TYPE_PSX, _Rt_, MODE_READ);
		if (regt >= 0)
			xCMP(xRegister32(regt), g_psxConstRegs[_Rs_]);
		else
			xCMP(ptr32[&psxRegs.GPR.r[_Rt_]], g_psxConstRegs[_Rs_]);
	}
	else if (process & PROCESS_CONSTT)
	{
		const int regs = _checkX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
		if (regs >= 0)
			xCMP(xRegister32(regs), g_psxConstRegs[_Rt_]);
		else
			xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], g_psxConstRegs[_Rt_]);
	}
	else
	{
		// force S into register, since we need to load it, may as well cache.
		const int regs = _allocX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
		const int regt = _checkX86reg(X86TYPE_PSX, _Rt_, MODE_READ);

		if (regt >= 0)
			xCMP(xRegister32(regs), xRegister32(regt));
		else
			xCMP(xRegister32(regs), ptr32[&psxRegs.GPR.r[_Rt_]]);
	}

	s_pbranchjmp = JNE32(0);
}

static void rpsxBEQ_const()
{
	u32 branchTo;

	if (g_psxConstRegs[_Rs_] == g_psxConstRegs[_Rt_])
		branchTo = ((s32)_Imm_ * 4) + psxpc;
	else
		branchTo = psxpc + 4;

	psxRecompileNextInstruction(true, false);
	psxSetBranchImm(branchTo);
}

static void rpsxBEQ_process(int process)
{
	u32 branchTo = ((s32)_Imm_ * 4) + psxpc;

	if (_Rs_ == _Rt_)
	{
		psxRecompileNextInstruction(true, false);
		psxSetBranchImm(branchTo);
	}
	else
	{
		const bool swap = psxTrySwapDelaySlot(_Rs_, _Rt_, 0);
		_psxFlushAllDirty();
		rpsxSetBranchEQ(process);

		if (!swap)
		{
			psxSaveBranchState();
			psxRecompileNextInstruction(true, false);
		}

		psxSetBranchImm(branchTo);

		x86SetJ32A(s_pbranchjmp);

		if (!swap)
		{
			// recopy the next inst
			psxpc -= 4;
			psxLoadBranchState();
			psxRecompileNextInstruction(true, false);
		}

		psxSetBranchImm(psxpc);
	}
}

static void rpsxBEQ()
{
	// prefer using the host register over an immediate, it'll be smaller code.
	if (PSX_IS_CONST2(_Rs_, _Rt_))
		rpsxBEQ_const();
	else if (PSX_IS_CONST1(_Rs_) && _checkX86reg(X86TYPE_PSX, _Rs_, MODE_READ) < 0)
		rpsxBEQ_process(PROCESS_CONSTS);
	else if (PSX_IS_CONST1(_Rt_) && _checkX86reg(X86TYPE_PSX, _Rt_, MODE_READ) < 0)
		rpsxBEQ_process(PROCESS_CONSTT);
	else
		rpsxBEQ_process(0);
}

//// BNE
static void rpsxBNE_const()
{
	u32 branchTo;

	if (g_psxConstRegs[_Rs_] != g_psxConstRegs[_Rt_])
		branchTo = ((s32)_Imm_ * 4) + psxpc;
	else
		branchTo = psxpc + 4;

	psxRecompileNextInstruction(true, false);
	psxSetBranchImm(branchTo);
}

static void rpsxBNE_process(int process)
{
	const u32 branchTo = ((s32)_Imm_ * 4) + psxpc;

	if (_Rs_ == _Rt_)
	{
		psxRecompileNextInstruction(true, false);
		psxSetBranchImm(psxpc);
		return;
	}

	const bool swap = psxTrySwapDelaySlot(_Rs_, _Rt_, 0);
	_psxFlushAllDirty();
	rpsxSetBranchEQ(process);

	if (!swap)
	{
		psxSaveBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(psxpc);

	x86SetJ32A(s_pbranchjmp);

	if (!swap)
	{
		// recopy the next inst
		psxpc -= 4;
		psxLoadBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(branchTo);
}

static void rpsxBNE()
{
	if (PSX_IS_CONST2(_Rs_, _Rt_))
		rpsxBNE_const();
	else if (PSX_IS_CONST1(_Rs_) && _checkX86reg(X86TYPE_PSX, _Rs_, MODE_READ) < 0)
		rpsxBNE_process(PROCESS_CONSTS);
	else if (PSX_IS_CONST1(_Rt_) && _checkX86reg(X86TYPE_PSX, _Rt_, MODE_READ) < 0)
		rpsxBNE_process(PROCESS_CONSTT);
	else
		rpsxBNE_process(0);
}

//// BLTZ
static void rpsxBLTZ()
{
	// Branch if Rs < 0
	u32 branchTo = (s32)_Imm_ * 4 + psxpc;

	if (PSX_IS_CONST1(_Rs_))
	{
		if ((int)g_psxConstRegs[_Rs_] >= 0)
			branchTo = psxpc + 4;

		psxRecompileNextInstruction(true, false);
		psxSetBranchImm(branchTo);
		return;
	}

	const bool swap = psxTrySwapDelaySlot(_Rs_, 0, 0);
	_psxFlushAllDirty();

	const int regs = _checkX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
	if (regs >= 0)
		xCMP(xRegister32(regs), 0);
	else
		xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], 0);

	u32* pjmp = JL32(0);

	if (!swap)
	{
		psxSaveBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(psxpc);

	x86SetJ32A(pjmp);

	if (!swap)
	{
		// recopy the next inst
		psxpc -= 4;
		psxLoadBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(branchTo);
}

//// BGEZ
static void rpsxBGEZ()
{
	u32 branchTo = ((s32)_Imm_ * 4) + psxpc;

	if (PSX_IS_CONST1(_Rs_))
	{
		if ((int)g_psxConstRegs[_Rs_] < 0)
			branchTo = psxpc + 4;

		psxRecompileNextInstruction(true, false);
		psxSetBranchImm(branchTo);
		return;
	}

	const bool swap = psxTrySwapDelaySlot(_Rs_, 0, 0);
	_psxFlushAllDirty();

	const int regs = _checkX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
	if (regs >= 0)
		xCMP(xRegister32(regs), 0);
	else
		xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], 0);

	u32* pjmp = JGE32(0);

	if (!swap)
	{
		psxSaveBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(psxpc);

	x86SetJ32A(pjmp);

	if (!swap)
	{
		// recopy the next inst
		psxpc -= 4;
		psxLoadBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(branchTo);
}

//// BLTZAL
static void rpsxBLTZAL()
{
	// Branch if Rs < 0
	u32 branchTo = (s32)_Imm_ * 4 + psxpc;

	_psxDeleteReg(31, DELETE_REG_FREE_NO_WRITEBACK);

	PSX_SET_CONST(31);
	g_psxConstRegs[31] = psxpc + 4;

	if (PSX_IS_CONST1(_Rs_))
	{
		if ((int)g_psxConstRegs[_Rs_] >= 0)
			branchTo = psxpc + 4;

		psxRecompileNextInstruction(true, false);
		psxSetBranchImm(branchTo);
		return;
	}

	const bool swap = psxTrySwapDelaySlot(_Rs_, 0, 0);
	_psxFlushAllDirty();

	const int regs = _checkX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
	if (regs >= 0)
		xCMP(xRegister32(regs), 0);
	else
		xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], 0);

	u32* pjmp = JL32(0);

	if (!swap)
	{
		psxSaveBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(psxpc);

	x86SetJ32A(pjmp);

	if (!swap)
	{
		// recopy the next inst
		psxpc -= 4;
		psxLoadBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(branchTo);
}

//// BGEZAL
static void rpsxBGEZAL()
{
	u32 branchTo = ((s32)_Imm_ * 4) + psxpc;

	_psxDeleteReg(31, DELETE_REG_FREE_NO_WRITEBACK);

	PSX_SET_CONST(31);
	g_psxConstRegs[31] = psxpc + 4;

	if (PSX_IS_CONST1(_Rs_))
	{
		if ((int)g_psxConstRegs[_Rs_] < 0)
			branchTo = psxpc + 4;

		psxRecompileNextInstruction(true, false);
		psxSetBranchImm(branchTo);
		return;
	}

	const bool swap = psxTrySwapDelaySlot(_Rs_, 0, 0);
	_psxFlushAllDirty();

	const int regs = _checkX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
	if (regs >= 0)
		xCMP(xRegister32(regs), 0);
	else
		xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], 0);

	u32* pjmp = JGE32(0);

	if (!swap)
	{
		psxSaveBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(psxpc);

	x86SetJ32A(pjmp);

	if (!swap)
	{
		// recopy the next inst
		psxpc -= 4;
		psxLoadBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(branchTo);
}

//// BLEZ
static void rpsxBLEZ()
{
	// Branch if Rs <= 0
	u32 branchTo = (s32)_Imm_ * 4 + psxpc;

	if (PSX_IS_CONST1(_Rs_))
	{
		if ((int)g_psxConstRegs[_Rs_] > 0)
			branchTo = psxpc + 4;

		psxRecompileNextInstruction(true, false);
		psxSetBranchImm(branchTo);
		return;
	}

	const bool swap = psxTrySwapDelaySlot(_Rs_, 0, 0);
	_psxFlushAllDirty();

	const int regs = _checkX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
	if (regs >= 0)
		xCMP(xRegister32(regs), 0);
	else
		xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], 0);

	u32* pjmp = JLE32(0);

	if (!swap)
	{
		psxSaveBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(psxpc);

	x86SetJ32A(pjmp);

	if (!swap)
	{
		psxpc -= 4;
		psxLoadBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(branchTo);
}

//// BGTZ
static void rpsxBGTZ()
{
	// Branch if Rs > 0
	u32 branchTo = (s32)_Imm_ * 4 + psxpc;

	_psxFlushAllDirty();

	if (PSX_IS_CONST1(_Rs_))
	{
		if ((int)g_psxConstRegs[_Rs_] <= 0)
			branchTo = psxpc + 4;

		psxRecompileNextInstruction(true, false);
		psxSetBranchImm(branchTo);
		return;
	}

	const bool swap = psxTrySwapDelaySlot(_Rs_, 0, 0);
	_psxFlushAllDirty();

	const int regs = _checkX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
	if (regs >= 0)
		xCMP(xRegister32(regs), 0);
	else
		xCMP(ptr32[&psxRegs.GPR.r[_Rs_]], 0);

	u32* pjmp = JG32(0);

	if (!swap)
	{
		psxSaveBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(psxpc);

	x86SetJ32A(pjmp);

	if (!swap)
	{
		psxpc -= 4;
		psxLoadBranchState();
		psxRecompileNextInstruction(true, false);
	}

	psxSetBranchImm(branchTo);
}

static void rpsxMFC0()
{
	// Rt = Cop0->Rd
	if (!_Rt_)
		return;

	const int rt = _allocX86reg(X86TYPE_PSX, _Rt_, MODE_WRITE);
	xMOV(xRegister32(rt), ptr32[&psxRegs.CP0.r[_Rd_]]);
}

static void rpsxCFC0()
{
	// Rt = Cop0->Rd
	if (!_Rt_)
		return;

	const int rt = _allocX86reg(X86TYPE_PSX, _Rt_, MODE_WRITE);
	xMOV(xRegister32(rt), ptr32[&psxRegs.CP0.r[_Rd_]]);
}

static void rpsxMTC0()
{
	// Cop0->Rd = Rt
	if (PSX_IS_CONST1(_Rt_))
	{
		xMOV(ptr32[&psxRegs.CP0.r[_Rd_]], g_psxConstRegs[_Rt_]);
	}
	else
	{
		const int rt = _allocX86reg(X86TYPE_PSX, _Rt_, MODE_READ);
		xMOV(ptr32[&psxRegs.CP0.r[_Rd_]], xRegister32(rt));
	}
}

static void rpsxCTC0()
{
	// Cop0->Rd = Rt
	rpsxMTC0();
}

static void rpsxRFE()
{
	xMOV(eax, ptr32[&psxRegs.CP0.n.Status]);
	xMOV(ecx, eax);
	xAND(eax, 0xfffffff0);
	xAND(ecx, 0x3c);
	xSHR(ecx, 2);
	xOR(eax, ecx);
	xMOV(ptr32[&psxRegs.CP0.n.Status], eax);

	// Test the IOP's INTC status, so that any pending ints get raised.

	_psxFlushCall(0);
	xFastCall((void*)(uptr)&iopTestIntc);
}

//// COP2
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

REC_GTE_FUNC(MFC2);
REC_GTE_FUNC(CFC2);
REC_GTE_FUNC(MTC2);
REC_GTE_FUNC(CTC2);

REC_GTE_FUNC(LWC2);
REC_GTE_FUNC(SWC2);


// R3000A tables
extern void (*rpsxBSC[64])();
extern void (*rpsxSPC[64])();
extern void (*rpsxREG[32])();
extern void (*rpsxCP0[32])();
extern void (*rpsxCP2[64])();
extern void (*rpsxCP2BSC[32])();

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

////////////////////////////////////////////////
// Back-Prob Function Tables - Gathering Info //
////////////////////////////////////////////////
#define rpsxpropSetRead(reg) \
	{ \
		if (!(pinst->regs[reg] & EEINST_USED)) \
			pinst->regs[reg] |= EEINST_LASTUSE; \
		prev->regs[reg] |= EEINST_LIVE | EEINST_USED; \
		pinst->regs[reg] |= EEINST_USED; \
		_recFillRegister(*pinst, XMMTYPE_GPRREG, reg, 0); \
	}

#define rpsxpropSetWrite(reg) \
	{ \
		prev->regs[reg] &= ~(EEINST_LIVE | EEINST_USED); \
		if (!(pinst->regs[reg] & EEINST_USED)) \
			pinst->regs[reg] |= EEINST_LASTUSE; \
		pinst->regs[reg] |= EEINST_USED; \
		_recFillRegister(*pinst, XMMTYPE_GPRREG, reg, 1); \
	}

void rpsxpropBSC(EEINST* prev, EEINST* pinst);
void rpsxpropSPECIAL(EEINST* prev, EEINST* pinst);
void rpsxpropREGIMM(EEINST* prev, EEINST* pinst);
void rpsxpropCP0(EEINST* prev, EEINST* pinst);
void rpsxpropCP2(EEINST* prev, EEINST* pinst);

//SPECIAL, REGIMM, J   , JAL  , BEQ , BNE , BLEZ, BGTZ,
//ADDI   , ADDIU , SLTI, SLTIU, ANDI, ORI , XORI, LUI ,
//COP0   , NULL  , COP2, NULL , NULL, NULL, NULL, NULL,
//NULL   , NULL  , NULL, NULL , NULL, NULL, NULL, NULL,
//LB     , LH    , LWL , LW   , LBU , LHU , LWR , NULL,
//SB     , SH    , SWL , SW   , NULL, NULL, SWR , NULL,
//NULL   , NULL  , NULL, NULL , NULL, NULL, NULL, NULL,
//NULL   , NULL  , NULL, NULL , NULL, NULL, NULL, NULL
void rpsxpropBSC(EEINST* prev, EEINST* pinst)
{
	switch (psxRegs.code >> 26)
	{
		case 0:
			rpsxpropSPECIAL(prev, pinst);
			break;
		case 1:
			rpsxpropREGIMM(prev, pinst);
			break;
		case 2: // j
			break;
		case 3: // jal
			rpsxpropSetWrite(31);
			break;
		case 4: // beq
		case 5: // bne
			rpsxpropSetRead(_Rs_);
			rpsxpropSetRead(_Rt_);
			break;

		case 6: // blez
		case 7: // bgtz
			rpsxpropSetRead(_Rs_);
			break;

		case 15: // lui
			rpsxpropSetWrite(_Rt_);
			break;

		case 16:
			rpsxpropCP0(prev, pinst);
			break;
		case 18:
			rpsxpropCP2(prev, pinst);
			break;

		// stores
		case 40:
		case 41:
		case 42:
		case 43:
		case 46:
			rpsxpropSetRead(_Rt_);
			rpsxpropSetRead(_Rs_);
			break;

		case 50: // LWC2
		case 58: // SWC2
			// Operation on COP2 registers/memory. GPRs are left untouched
			break;

		default:
			rpsxpropSetWrite(_Rt_);
			rpsxpropSetRead(_Rs_);
			break;
	}
}

//SLL , NULL, SRL , SRA , SLLV   , NULL , SRLV, SRAV,
//JR  , JALR, NULL, NULL, SYSCALL, BREAK, NULL, NULL,
//MFHI, MTHI, MFLO, MTLO, NULL   , NULL , NULL, NULL,
//MULT, MULTU, DIV, DIVU, NULL   , NULL , NULL, NULL,
//ADD , ADDU, SUB , SUBU, AND    , OR   , XOR , NOR ,
//NULL, NULL, SLT , SLTU, NULL   , NULL , NULL, NULL,
//NULL, NULL, NULL, NULL, NULL   , NULL , NULL, NULL,
//NULL, NULL, NULL, NULL, NULL   , NULL , NULL, NULL,
void rpsxpropSPECIAL(EEINST* prev, EEINST* pinst)
{
	switch (_Funct_)
	{
		case 0: // SLL
		case 2: // SRL
		case 3: // SRA
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

		case 12: // syscall
		case 13: // break
			_recClearInst(prev);
			prev->info = 0;
			break;
		case 15: // sync
			break;

		case 16: // mfhi
			rpsxpropSetWrite(_Rd_);
			rpsxpropSetRead(PSX_HI);
			break;
		case 17: // mthi
			rpsxpropSetWrite(PSX_HI);
			rpsxpropSetRead(_Rs_);
			break;
		case 18: // mflo
			rpsxpropSetWrite(_Rd_);
			rpsxpropSetRead(PSX_LO);
			break;
		case 19: // mtlo
			rpsxpropSetWrite(PSX_LO);
			rpsxpropSetRead(_Rs_);
			break;

		case 24: // mult
		case 25: // multu
		case 26: // div
		case 27: // divu
			rpsxpropSetWrite(PSX_LO);
			rpsxpropSetWrite(PSX_HI);
			rpsxpropSetRead(_Rs_);
			rpsxpropSetRead(_Rt_);
			break;

		case 32: // add
		case 33: // addu
		case 34: // sub
		case 35: // subu
			rpsxpropSetWrite(_Rd_);
			if (_Rs_)
				rpsxpropSetRead(_Rs_);
			if (_Rt_)
				rpsxpropSetRead(_Rt_);
			break;

		default:
			rpsxpropSetWrite(_Rd_);
			rpsxpropSetRead(_Rs_);
			rpsxpropSetRead(_Rt_);
			break;
	}
}

//BLTZ  , BGEZ  , NULL, NULL, NULL, NULL, NULL, NULL,
//NULL  , NULL  , NULL, NULL, NULL, NULL, NULL, NULL,
//BLTZAL, BGEZAL, NULL, NULL, NULL, NULL, NULL, NULL,
//NULL  , NULL  , NULL, NULL, NULL, NULL, NULL, NULL
void rpsxpropREGIMM(EEINST* prev, EEINST* pinst)
{
	switch (_Rt_)
	{
		case 0: // bltz
		case 1: // bgez
			rpsxpropSetRead(_Rs_);
			break;

		case 16: // bltzal
		case 17: // bgezal
			// do not write 31
			rpsxpropSetRead(_Rs_);
			break;

			jNO_DEFAULT
	}
}

//MFC0, NULL, CFC0, NULL, MTC0, NULL, CTC0, NULL,
//NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
//RFE , NULL, NULL, NULL, NULL, NULL, NULL, NULL,
//NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
void rpsxpropCP0(EEINST* prev, EEINST* pinst)
{
	switch (_Rs_)
	{
		case 0: // mfc0
		case 2: // cfc0
			rpsxpropSetWrite(_Rt_);
			break;

		case 4: // mtc0
		case 6: // ctc0
			rpsxpropSetRead(_Rt_);
			break;
		case 16: // rfe
			break;

			jNO_DEFAULT
	}
}


// Basic table:
// gteMFC2, psxNULL, gteCFC2, psxNULL, gteMTC2, psxNULL, gteCTC2, psxNULL,
// psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
// psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
// psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL, psxNULL,
void rpsxpropCP2_basic(EEINST* prev, EEINST* pinst)
{
	switch (_Rs_)
	{
		case 0: // mfc2
		case 2: // cfc2
			rpsxpropSetWrite(_Rt_);
			break;

		case 4: // mtc2
		case 6: // ctc2
			rpsxpropSetRead(_Rt_);
			break;

		default:
			pxFail("iop invalid opcode in const propagation (rpsxpropCP2/BASIC)");
			break;
	}
}


// Main table:
// psxBASIC, gteRTPS , psxNULL , psxNULL, psxNULL, psxNULL , gteNCLIP, psxNULL, // 00
// psxNULL , psxNULL , psxNULL , psxNULL, gteOP  , psxNULL , psxNULL , psxNULL, // 08
// gteDPCS , gteINTPL, gteMVMVA, gteNCDS, gteCDP , psxNULL , gteNCDT , psxNULL, // 10
// psxNULL , psxNULL , psxNULL , gteNCCS, gteCC  , psxNULL , gteNCS  , psxNULL, // 18
// gteNCT  , psxNULL , psxNULL , psxNULL, psxNULL, psxNULL , psxNULL , psxNULL, // 20
// gteSQR  , gteDCPL , gteDPCT , psxNULL, psxNULL, gteAVSZ3, gteAVSZ4, psxNULL, // 28
// gteRTPT , psxNULL , psxNULL , psxNULL, psxNULL, psxNULL , psxNULL , psxNULL, // 30
// psxNULL , psxNULL , psxNULL , psxNULL, psxNULL, gteGPF  , gteGPL  , gteNCCT, // 38
void rpsxpropCP2(EEINST* prev, EEINST* pinst)
{
	switch (_Funct_)
	{
		case 0: // Basic opcode
			rpsxpropCP2_basic(prev, pinst);
			break;

		default:
			// COP2 operation are likely done with internal COP2 registers
			// No impact on GPR
			break;
	}
}
