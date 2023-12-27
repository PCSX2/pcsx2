// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "x86/iR5900.h"
#include "x86/iR5900LoadStore.h"

using namespace x86Emitter;

#define REC_STORES
#define REC_LOADS

static int RETURN_READ_IN_RAX()
{
	return rax.GetId();
}

namespace R5900::Dynarec::OpcodeImpl
{

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/
#ifndef LOADSTORE_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC_DEL(LB, _Rt_);
REC_FUNC_DEL(LBU, _Rt_);
REC_FUNC_DEL(LH, _Rt_);
REC_FUNC_DEL(LHU, _Rt_);
REC_FUNC_DEL(LW, _Rt_);
REC_FUNC_DEL(LWU, _Rt_);
REC_FUNC_DEL(LWL, _Rt_);
REC_FUNC_DEL(LWR, _Rt_);
REC_FUNC_DEL(LD, _Rt_);
REC_FUNC_DEL(LDR, _Rt_);
REC_FUNC_DEL(LDL, _Rt_);
REC_FUNC_DEL(LQ, _Rt_);
REC_FUNC(SB);
REC_FUNC(SH);
REC_FUNC(SW);
REC_FUNC(SWL);
REC_FUNC(SWR);
REC_FUNC(SD);
REC_FUNC(SDL);
REC_FUNC(SDR);
REC_FUNC(SQ);
REC_FUNC(LWC1);
REC_FUNC(SWC1);
REC_FUNC(LQC2);
REC_FUNC(SQC2);

#else

using namespace Interpreter::OpcodeImpl;

//////////////////////////////////////////////////////////////////////////////////////////
//
static void recLoadQuad(u32 bits, bool sign)
{
	pxAssume(bits == 128);

	// This mess is so we allocate *after* the vtlb flush, not before.
	vtlb_ReadRegAllocCallback alloc_cb = nullptr;
	if (_Rt_)
		alloc_cb = []() { return _allocGPRtoXMMreg(_Rt_, MODE_WRITE); };

	int xmmreg;
	if (GPR_IS_CONST1(_Rs_))
	{
		const u32 srcadr = (g_cpuConstRegs[_Rs_].UL[0] + _Imm_) & ~0x0f;
		xmmreg = vtlb_DynGenReadQuad_Const(bits, srcadr, _Rt_ ? alloc_cb : nullptr);
	}
	else
	{
		// Load ECX with the source memory address that we're reading from.
		_freeX86reg(arg1regd);
		_eeMoveGPRtoR(arg1reg, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		// force 16 byte alignment on 128 bit reads
		xAND(arg1regd, ~0x0F);

		xmmreg = vtlb_DynGenReadQuad(bits, arg1regd.GetId(), _Rt_ ? alloc_cb : nullptr);
	}

	// if there was a constant, it should have been invalidated.
	pxAssert(!_Rt_ || !GPR_IS_CONST1(_Rt_));
	if (!_Rt_)
		_freeXMMreg(xmmreg);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
static void recLoad(u32 bits, bool sign)
{
	pxAssume(bits <= 64);

	// This mess is so we allocate *after* the vtlb flush, not before.
	// TODO(Stenzek): If not live, save directly to state, and delete constant.
	vtlb_ReadRegAllocCallback alloc_cb = nullptr;
	if (_Rt_)
		alloc_cb = []() { return _allocX86reg(X86TYPE_GPR, _Rt_, MODE_WRITE); };

	int x86reg;
	if (GPR_IS_CONST1(_Rs_))
	{
		const u32 srcadr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		x86reg = vtlb_DynGenReadNonQuad_Const(bits, sign, false, srcadr, alloc_cb);
	}
	else
	{
		// Load arg1 with the source memory address that we're reading from.
		_freeX86reg(arg1regd);
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		x86reg = vtlb_DynGenReadNonQuad(bits, sign, false, arg1regd.GetId(), alloc_cb);
	}

	// if there was a constant, it should have been invalidated.
	pxAssert(!_Rt_ || !GPR_IS_CONST1(_Rt_));
	if (!_Rt_)
		_freeX86reg(x86reg);
}

//////////////////////////////////////////////////////////////////////////////////////////
//

static void recStore(u32 bits)
{
	// Performance note: Const prop for the store address is good, always.
	// Constprop for the value being stored is not really worthwhile (better to use register
	// allocation -- simpler code and just as fast)

	int regt;
	bool xmm;
	if (bits < 128)
	{
		regt = _allocX86reg(X86TYPE_GPR, _Rt_, MODE_READ);
		xmm = false;
	}
	else
	{
		regt = _allocGPRtoXMMreg(_Rt_, MODE_READ);
		xmm = true;
	}

	// Load ECX with the destination address, or issue a direct optimized write
	// if the address is a constant propagation.

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 dstadr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		if (bits == 128)
			dstadr &= ~0x0f;

		vtlb_DynGenWrite_Const(bits, xmm, dstadr, regt);
	}
	else
	{
		if (_Rs_ != 0)
		{
			// TODO(Stenzek): Preload Rs when it's live. Turn into LEA.
			_eeMoveGPRtoR(arg1regd, _Rs_);
			if (_Imm_ != 0)
				xADD(arg1regd, _Imm_);
		}
		else
		{
			xMOV(arg1regd, _Imm_);
		}

		if (bits == 128)
			xAND(arg1regd, ~0x0F);

		// TODO(Stenzek): Use Rs directly if imm=0. But beware of upper bits.
		vtlb_DynGenWrite(bits, xmm, arg1regd.GetId(), regt);
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
//
void recLB()
{
	recLoad(8, true);
	EE::Profiler.EmitOp(eeOpcode::LB);
}
void recLBU()
{
	recLoad(8, false);
	EE::Profiler.EmitOp(eeOpcode::LBU);
}
void recLH()
{
	recLoad(16, true);
	EE::Profiler.EmitOp(eeOpcode::LH);
}
void recLHU()
{
	recLoad(16, false);
	EE::Profiler.EmitOp(eeOpcode::LHU);
}
void recLW()
{
	recLoad(32, true);
	EE::Profiler.EmitOp(eeOpcode::LW);
}
void recLWU()
{
	recLoad(32, false);
	EE::Profiler.EmitOp(eeOpcode::LWU);
}
void recLD()
{
	recLoad(64, false);
	EE::Profiler.EmitOp(eeOpcode::LD);
}
void recLQ()
{
	recLoadQuad(128, false);
	EE::Profiler.EmitOp(eeOpcode::LQ);
}

void recSB()
{
	recStore(8);
	EE::Profiler.EmitOp(eeOpcode::SB);
}
void recSH()
{
	recStore(16);
	EE::Profiler.EmitOp(eeOpcode::SH);
}
void recSW()
{
	recStore(32);
	EE::Profiler.EmitOp(eeOpcode::SW);
}
void recSD()
{
	recStore(64);
	EE::Profiler.EmitOp(eeOpcode::SD);
}
void recSQ()
{
	recStore(128);
	EE::Profiler.EmitOp(eeOpcode::SQ);
}

////////////////////////////////////////////////////

void recLWL()
{
#ifdef REC_LOADS
	_freeX86reg(eax);
	_freeX86reg(ecx);
	_freeX86reg(edx);
	_freeX86reg(arg1regd);

	// avoid flushing and immediately reading back
	if (_Rt_)
		_addNeededX86reg(X86TYPE_GPR, _Rt_);
	if (_Rs_)
		_addNeededX86reg(X86TYPE_GPR, _Rs_);

	const xRegister32 temp(_allocX86reg(X86TYPE_TEMP, 0, MODE_CALLEESAVED));

	_eeMoveGPRtoR(arg1regd, _Rs_);
	if (_Imm_ != 0)
		xADD(arg1regd, _Imm_);

	// calleeSavedReg1 = bit offset in word
	xMOV(temp, arg1regd);
	xAND(temp, 3);
	xSHL(temp, 3);

	xAND(arg1regd, ~3);
	vtlb_DynGenReadNonQuad(32, false, false, arg1regd.GetId(), RETURN_READ_IN_RAX);

	if (!_Rt_)
	{
		_freeX86reg(temp);
		return;
	}

	// mask off bytes loaded
	xMOV(ecx, temp);
	_freeX86reg(temp);

	const int treg = _allocX86reg(X86TYPE_GPR, _Rt_, MODE_READ | MODE_WRITE);
	xMOV(edx, 0xffffff);
	xSHR(edx, cl);
	xAND(edx, xRegister32(treg));

	// OR in bytes loaded
	xNEG(ecx);
	xADD(ecx, 24);
	xSHL(eax, cl);
	xOR(eax, edx);
	xMOVSX(xRegister64(treg), eax);
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);

	recCall(LWL);
#endif

	EE::Profiler.EmitOp(eeOpcode::LWL);
}

////////////////////////////////////////////////////
void recLWR()
{
#ifdef REC_LOADS
	_freeX86reg(eax);
	_freeX86reg(ecx);
	_freeX86reg(edx);
	_freeX86reg(arg1regd);

	// avoid flushing and immediately reading back
	if (_Rt_)
		_addNeededX86reg(X86TYPE_GPR, _Rt_);
	if (_Rs_)
		_addNeededX86reg(X86TYPE_GPR, _Rs_);

	const xRegister32 temp(_allocX86reg(X86TYPE_TEMP, 0, MODE_CALLEESAVED));

	_eeMoveGPRtoR(arg1regd, _Rs_);
	if (_Imm_ != 0)
		xADD(arg1regd, _Imm_);

	// edi = bit offset in word
	xMOV(temp, arg1regd);

	xAND(arg1regd, ~3);
	vtlb_DynGenReadNonQuad(32, false, false, arg1regd.GetId(), RETURN_READ_IN_RAX);

	if (!_Rt_)
	{
		_freeX86reg(temp);
		return;
	}

	const int treg = _allocX86reg(X86TYPE_GPR, _Rt_, MODE_READ | MODE_WRITE);
	xAND(temp, 3);

	xForwardJE8 nomask;
	xSHL(temp, 3);
	// mask off bytes loaded
	xMOV(ecx, 24);
	xSUB(ecx, temp);
	xMOV(edx, 0xffffff00);
	xSHL(edx, cl);
	xAND(xRegister32(treg), edx);

	// OR in bytes loaded
	xMOV(ecx, temp);
	xSHR(eax, cl);
	xOR(xRegister32(treg), eax);

	xForwardJump8 end;
	nomask.SetTarget();
	// NOTE: This might look wrong, but it's correct - see interpreter.
	xMOVSX(xRegister64(treg), eax);
	end.SetTarget();
	_freeX86reg(temp);
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);

	recCall(LWR);
#endif

	EE::Profiler.EmitOp(eeOpcode::LWR);
}

////////////////////////////////////////////////////

void recSWL()
{
#ifdef REC_STORES
	// avoid flushing and immediately reading back
	_addNeededX86reg(X86TYPE_GPR, _Rs_);

	// preload Rt, since we can't do so inside the branch
	if (!GPR_IS_CONST1(_Rt_))
		_allocX86reg(X86TYPE_GPR, _Rt_, MODE_READ);
	else
		_addNeededX86reg(X86TYPE_GPR, _Rt_);

	const xRegister32 temp(_allocX86reg(X86TYPE_TEMP, 0, MODE_CALLEESAVED));
	_freeX86reg(eax);
	_freeX86reg(ecx);
	_freeX86reg(arg1regd);
	_freeX86reg(arg2regd);

	_eeMoveGPRtoR(arg1regd, _Rs_);
	if (_Imm_ != 0)
		xADD(arg1regd, _Imm_);

	// edi = bit offset in word
	xMOV(temp, arg1regd);
	xAND(arg1regd, ~3);
	xAND(temp, 3);
	xCMP(temp, 3);

	// If we're not using fastmem, we need to flush early. Because the first read
	// (which would flush) happens inside a branch.
	if (!CHECK_FASTMEM || vtlb_IsFaultingPC(pc))
		iFlushCall(FLUSH_FULLVTLB);

	xForwardJE8 skip;
	xSHL(temp, 3);

	vtlb_DynGenReadNonQuad(32, false, false, arg1regd.GetId(), RETURN_READ_IN_RAX);

	// mask read -> arg2
	xMOV(ecx, temp);
	xMOV(arg2regd, 0xffffff00);
	xSHL(arg2regd, cl);
	xAND(arg2regd, eax);

	if (_Rt_)
	{
		// mask write and OR -> edx
		xNEG(ecx);
		xADD(ecx, 24);
		_eeMoveGPRtoR(eax, _Rt_, false);
		xSHR(eax, cl);
		xOR(arg2regd, eax);
	}

	_eeMoveGPRtoR(arg1regd, _Rs_, false);
	if (_Imm_ != 0)
		xADD(arg1regd, _Imm_);
	xAND(arg1regd, ~3);

	xForwardJump8 end;
	skip.SetTarget();
	_eeMoveGPRtoR(arg2regd, _Rt_, false);
	end.SetTarget();

	_freeX86reg(temp);
	vtlb_DynGenWrite(32, false, arg1regd.GetId(), arg2regd.GetId());
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);
	recCall(SWL);
#endif

	EE::Profiler.EmitOp(eeOpcode::SWL);
}

////////////////////////////////////////////////////
void recSWR()
{
#ifdef REC_STORES
	// avoid flushing and immediately reading back
	_addNeededX86reg(X86TYPE_GPR, _Rs_);

	// preload Rt, since we can't do so inside the branch
	if (!GPR_IS_CONST1(_Rt_))
		_allocX86reg(X86TYPE_GPR, _Rt_, MODE_READ);
	else
		_addNeededX86reg(X86TYPE_GPR, _Rt_);

	const xRegister32 temp(_allocX86reg(X86TYPE_TEMP, 0, MODE_CALLEESAVED));
	_freeX86reg(ecx);
	_freeX86reg(arg1regd);
	_freeX86reg(arg2regd);

	_eeMoveGPRtoR(arg1regd, _Rs_);
	if (_Imm_ != 0)
		xADD(arg1regd, _Imm_);

	// edi = bit offset in word
	xMOV(temp, arg1regd);
	xAND(arg1regd, ~3);
	xAND(temp, 3);

	// If we're not using fastmem, we need to flush early. Because the first read
	// (which would flush) happens inside a branch.
	if (!CHECK_FASTMEM || vtlb_IsFaultingPC(pc))
		iFlushCall(FLUSH_FULLVTLB);

	xForwardJE8 skip;
	xSHL(temp, 3);

	vtlb_DynGenReadNonQuad(32, false, false, arg1regd.GetId(), RETURN_READ_IN_RAX);

	// mask read -> edx
	xMOV(ecx, 24);
	xSUB(ecx, temp);
	xMOV(arg2regd, 0xffffff);
	xSHR(arg2regd, cl);
	xAND(arg2regd, eax);

	if (_Rt_)
	{
		// mask write and OR -> edx
		xMOV(ecx, temp);
		_eeMoveGPRtoR(eax, _Rt_, false);
		xSHL(eax, cl);
		xOR(arg2regd, eax);
	}

	_eeMoveGPRtoR(arg1regd, _Rs_, false);
	if (_Imm_ != 0)
		xADD(arg1regd, _Imm_);
	xAND(arg1regd, ~3);

	xForwardJump8 end;
	skip.SetTarget();
	_eeMoveGPRtoR(arg2regd, _Rt_, false);
	end.SetTarget();

	_freeX86reg(temp);
	vtlb_DynGenWrite(32, false, arg1regd.GetId(), arg2regd.GetId());
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);
	recCall(SWR);
#endif

	EE::Profiler.EmitOp(eeOpcode::SWR);
}

////////////////////////////////////////////////////

/// Masks rt with (0xffffffffffffffff maskshift maskamt), merges with (value shift amt), leaves result in value
static void ldlrhelper_const(int maskamt, const xImpl_Group2& maskshift, int amt, const xImpl_Group2& shift, const xRegister64& value, const xRegister64& rt)
{
	pxAssert(rt.GetId() != ecx.GetId() && value.GetId() != ecx.GetId());

	// Would xor rcx, rcx; not rcx be better here?
	xMOV(rcx, -1);

	maskshift(rcx, maskamt);
	xAND(rt, rcx);

	shift(value, amt);
	xOR(rt, value);
}

/// Masks rt with (0xffffffffffffffff maskshift maskamt), merges with (value shift amt), leaves result in value
static void ldlrhelper(const xRegister32& maskamt, const xImpl_Group2& maskshift, const xRegister32& amt, const xImpl_Group2& shift, const xRegister64& value, const xRegister64& rt)
{
	pxAssert(rt.GetId() != ecx.GetId() && amt.GetId() != ecx.GetId() && value.GetId() != ecx.GetId());

	// Would xor rcx, rcx; not rcx be better here?
	const xRegister64 maskamt64(maskamt);
	xMOV(ecx, maskamt);
	xMOV(maskamt64, -1);
	maskshift(maskamt64, cl);
	xAND(rt, maskamt64);

	xMOV(ecx, amt);
	shift(value, cl);
	xOR(rt, value);
}

void recLDL()
{
	if (!_Rt_)
		return;

#ifdef REC_LOADS
	// avoid flushing and immediately reading back
	if (_Rt_)
		_addNeededX86reg(X86TYPE_GPR, _Rt_);
	if (_Rs_)
		_addNeededX86reg(X86TYPE_GPR, _Rs_);

	const xRegister32 temp1(_allocX86reg(X86TYPE_TEMP, 0, MODE_CALLEESAVED));
	_freeX86reg(eax);
	_freeX86reg(ecx);
	_freeX86reg(edx);
	_freeX86reg(arg1regd);

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 srcadr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;

		// If _Rs_ is equal to _Rt_ we need to put the shift in to eax since it won't take the CONST path.
		if (_Rs_ == _Rt_)
			xMOV(temp1, srcadr);

		srcadr &= ~0x07;

		vtlb_DynGenReadNonQuad_Const(64, false, false, srcadr, RETURN_READ_IN_RAX);
	}
	else
	{
		// Load ECX with the source memory address that we're reading from.
		_freeX86reg(arg1regd);
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		xMOV(temp1, arg1regd);
		xAND(arg1regd, ~0x07);

		vtlb_DynGenReadNonQuad(64, false, false, arg1regd.GetId(), RETURN_READ_IN_RAX);
	}

	const xRegister64 treg(_allocX86reg(X86TYPE_GPR, _Rt_, MODE_READ | MODE_WRITE));

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 shift = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		shift = ((shift & 0x7) + 1) * 8;
		if (shift != 64)
		{
			ldlrhelper_const(shift, xSHR, 64 - shift, xSHL, rax, treg);
		}
		else
		{
			xMOV(treg, rax);
		}
	}
	else
	{
		xAND(temp1, 0x7);
		xCMP(temp1, 7);
		xCMOVE(treg, rax); // swap register with memory when not shifting
		xForwardJE8 skip;
		// Calculate the shift from top bit to lowest.
		xADD(temp1, 1);
		xMOV(edx, 64);
		xSHL(temp1, 3);
		xSUB(edx, temp1);

		ldlrhelper(temp1, xSHR, edx, xSHL, rax, treg);
		skip.SetTarget();
	}

	_freeX86reg(temp1);
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);
	recCall(LDL);
#endif

	EE::Profiler.EmitOp(eeOpcode::LDL);
}

////////////////////////////////////////////////////
void recLDR()
{
	if (!_Rt_)
		return;

#ifdef REC_LOADS
	// avoid flushing and immediately reading back
	if (_Rt_)
		_addNeededX86reg(X86TYPE_GPR, _Rt_);
	if (_Rs_)
		_addNeededX86reg(X86TYPE_GPR, _Rs_);

	const xRegister32 temp1(_allocX86reg(X86TYPE_TEMP, 0, MODE_CALLEESAVED));
	_freeX86reg(eax);
	_freeX86reg(ecx);
	_freeX86reg(edx);
	_freeX86reg(arg1regd);

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 srcadr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;

		// If _Rs_ is equal to _Rt_ we need to put the shift in to eax since it won't take the CONST path.
		if (_Rs_ == _Rt_)
			xMOV(temp1, srcadr);

		srcadr &= ~0x07;

		vtlb_DynGenReadNonQuad_Const(64, false, false, srcadr, RETURN_READ_IN_RAX);
	}
	else
	{
		// Load ECX with the source memory address that we're reading from.
		_freeX86reg(arg1regd);
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		xMOV(temp1, arg1regd);
		xAND(arg1regd, ~0x07);

		vtlb_DynGenReadNonQuad(64, false, false, arg1regd.GetId(), RETURN_READ_IN_RAX);
	}

	const xRegister64 treg(_allocX86reg(X86TYPE_GPR, _Rt_, MODE_READ | MODE_WRITE));

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 shift = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		shift = (shift & 0x7) * 8;
		if (shift != 0)
		{
			ldlrhelper_const(64 - shift, xSHL, shift, xSHR, rax, treg);
		}
		else
		{
			xMOV(treg, rax);
		}
	}
	else
	{
		xAND(temp1, 0x7);
		xCMOVE(treg, rax); // swap register with memory when not shifting
		xForwardJE8 skip;
		// Calculate the shift from top bit to lowest.
		xMOV(edx, 64);
		xSHL(temp1, 3);
		xSUB(edx, temp1);

		ldlrhelper(edx, xSHL, temp1, xSHR, rax, treg);
		skip.SetTarget();
	}

	_freeX86reg(temp1);
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);
	recCall(LDR);
#endif

	EE::Profiler.EmitOp(eeOpcode::LDR);
}

////////////////////////////////////////////////////

/// Masks value with (0xffffffffffffffff maskshift maskamt), merges with (rt shift amt), saves to dummyValue
static void sdlrhelper_const(int maskamt, const xImpl_Group2& maskshift, int amt, const xImpl_Group2& shift, const xRegister64& value, const xRegister64& rt)
{
	pxAssert(rt.GetId() != ecx.GetId() && value.GetId() != ecx.GetId());
	xMOV(rcx, -1);
	maskshift(rcx, maskamt);
	xAND(rcx, value);

	shift(rt, amt);
	xOR(rt, rcx);
}

/// Masks value with (0xffffffffffffffff maskshift maskamt), merges with (rt shift amt), saves to dummyValue
static void sdlrhelper(const xRegister32& maskamt, const xImpl_Group2& maskshift, const xRegister32& amt, const xImpl_Group2& shift, const xRegister64& value, const xRegister64& rt)
{
	pxAssert(rt.GetId() != ecx.GetId() && amt.GetId() != ecx.GetId() && value.GetId() != ecx.GetId());

	// Generate mask 128-(shiftx8)
	const xRegister64 maskamt64(maskamt);
	xMOV(ecx, maskamt);
	xMOV(maskamt64, -1);
	maskshift(maskamt64, cl);
	xAND(maskamt64, value);

	// Shift over reg value
	xMOV(ecx, amt);
	shift(rt, cl);
	xOR(rt, maskamt64);
}

void recSDL()
{
#ifdef REC_STORES
	// avoid flushing and immediately reading back
	if (_Rt_)
		_addNeededX86reg(X86TYPE_GPR, _Rt_);

	_freeX86reg(ecx);
	_freeX86reg(arg2regd);

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 adr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		u32 aligned = adr & ~0x07;
		u32 shift = ((adr & 0x7) + 1) * 8;
		if (shift == 64)
		{
			_eeMoveGPRtoR(arg2reg, _Rt_);
		}
		else
		{
			vtlb_DynGenReadNonQuad_Const(64, false, false, aligned, RETURN_READ_IN_RAX);
			_eeMoveGPRtoR(arg2reg, _Rt_);
			sdlrhelper_const(shift, xSHL, 64 - shift, xSHR, rax, arg2reg);
		}
		vtlb_DynGenWrite_Const(64, false, aligned, arg2regd.GetId());
	}
	else
	{
		if (_Rs_)
			_addNeededX86reg(X86TYPE_GPR, _Rs_);

		// Load ECX with the source memory address that we're reading from.
		_freeX86reg(arg1regd);
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		_freeX86reg(ecx);
		_freeX86reg(edx);
		_freeX86reg(arg2regd);
		const xRegister32 temp1(_allocX86reg(X86TYPE_TEMP, 0, MODE_CALLEESAVED));
		const xRegister64 temp2(_allocX86reg(X86TYPE_TEMP, 0, MODE_CALLEESAVED));
		_eeMoveGPRtoR(arg2reg, _Rt_);

		xMOV(temp1, arg1regd);
		xMOV(temp2, arg2reg);
		xAND(arg1regd, ~0x07);
		xAND(temp1, 0x7);
		xCMP(temp1, 7);

		// If we're not using fastmem, we need to flush early. Because the first read
		// (which would flush) happens inside a branch.
		if (!CHECK_FASTMEM || vtlb_IsFaultingPC(pc))
			iFlushCall(FLUSH_FULLVTLB);

		xForwardJE8 skip;
		xADD(temp1, 1);
		vtlb_DynGenReadNonQuad(64, false, false, arg1regd.GetId(), RETURN_READ_IN_RAX);

		//Calculate the shift from top bit to lowest
		xMOV(edx, 64);
		xSHL(temp1, 3);
		xSUB(edx, temp1);

		sdlrhelper(temp1, xSHL, edx, xSHR, rax, temp2);

		_eeMoveGPRtoR(arg1regd, _Rs_, false);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);
		xAND(arg1regd, ~0x7);
		skip.SetTarget();

		vtlb_DynGenWrite(64, false, arg1regd.GetId(), temp2.GetId());
		_freeX86reg(temp2.GetId());
		_freeX86reg(temp1.GetId());
	}
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);
	recCall(SDL);
#endif
	EE::Profiler.EmitOp(eeOpcode::SDL);
}

////////////////////////////////////////////////////
void recSDR()
{
#ifdef REC_STORES
	// avoid flushing and immediately reading back
	if (_Rt_)
		_addNeededX86reg(X86TYPE_GPR, _Rt_);

	_freeX86reg(ecx);
	_freeX86reg(arg2regd);

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 adr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		u32 aligned = adr & ~0x07;
		u32 shift = (adr & 0x7) * 8;
		if (shift == 0)
		{
			_eeMoveGPRtoR(arg2reg, _Rt_);
		}
		else
		{
			vtlb_DynGenReadNonQuad_Const(64, false, false, aligned, RETURN_READ_IN_RAX);
			_eeMoveGPRtoR(arg2reg, _Rt_);
			sdlrhelper_const(64 - shift, xSHR, shift, xSHL, rax, arg2reg);
		}

		vtlb_DynGenWrite_Const(64, false, aligned, arg2reg.GetId());
	}
	else
	{
		if (_Rs_)
			_addNeededX86reg(X86TYPE_GPR, _Rs_);

		// Load ECX with the source memory address that we're reading from.
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		_freeX86reg(ecx);
		_freeX86reg(edx);
		_freeX86reg(arg2regd);
		const xRegister32 temp1(_allocX86reg(X86TYPE_TEMP, 0, MODE_CALLEESAVED));
		const xRegister64 temp2(_allocX86reg(X86TYPE_TEMP, 0, MODE_CALLEESAVED));
		_eeMoveGPRtoR(arg2reg, _Rt_);

		xMOV(temp1, arg1regd);
		xMOV(temp2, arg2reg);
		xAND(arg1regd, ~0x07);
		xAND(temp1, 0x7);

		// If we're not using fastmem, we need to flush early. Because the first read
		// (which would flush) happens inside a branch.
		if (!CHECK_FASTMEM || vtlb_IsFaultingPC(pc))
			iFlushCall(FLUSH_FULLVTLB);

		xForwardJE8 skip;
		vtlb_DynGenReadNonQuad(64, false, false, arg1regd.GetId(), RETURN_READ_IN_RAX);

		xMOV(edx, 64);
		xSHL(temp1, 3);
		xSUB(edx, temp1);

		sdlrhelper(edx, xSHR, temp1, xSHL, rax, temp2);

		_eeMoveGPRtoR(arg1regd, _Rs_, false);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);
		xAND(arg1regd, ~0x7);
		xMOV(arg2reg, temp2);
		skip.SetTarget();

		vtlb_DynGenWrite(64, false, arg1regd.GetId(), temp2.GetId());
		_freeX86reg(temp2.GetId());
		_freeX86reg(temp1.GetId());
	}
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);
	recCall(SDR);
#endif
	EE::Profiler.EmitOp(eeOpcode::SDR);
}

//////////////////////////////////////////////////////////////////////////////////////////
/*********************************************************
* Load and store for COP1                                *
* Format:  OP rt, offset(base)                           *
*********************************************************/

////////////////////////////////////////////////////

void recLWC1()
{
#ifndef FPU_RECOMPILE
	recCall(::R5900::Interpreter::OpcodeImpl::LWC1);
#else

	const vtlb_ReadRegAllocCallback alloc_cb = []() { return _allocFPtoXMMreg(_Rt_, MODE_WRITE); };
	if (GPR_IS_CONST1(_Rs_))
	{
		const u32 addr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		vtlb_DynGenReadNonQuad_Const(32, false, true, addr, alloc_cb);
	}
	else
	{
		_freeX86reg(arg1regd);
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		vtlb_DynGenReadNonQuad(32, false, true, arg1regd.GetId(), alloc_cb);
	}

	EE::Profiler.EmitOp(eeOpcode::LWC1);
#endif
}

//////////////////////////////////////////////////////

void recSWC1()
{
#ifndef FPU_RECOMPILE
	recCall(::R5900::Interpreter::OpcodeImpl::SWC1);
#else
	const int regt = _allocFPtoXMMreg(_Rt_, MODE_READ);
	if (GPR_IS_CONST1(_Rs_))
	{
		const u32 addr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		vtlb_DynGenWrite_Const(32, true, addr, regt);
	}
	else
	{
		_freeX86reg(arg1regd);
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		vtlb_DynGenWrite(32, true, arg1regd.GetId(), regt);
	}

	EE::Profiler.EmitOp(eeOpcode::SWC1);
#endif
}

#endif

} // namespace R5900::Dynarec::OpcodeImpl
