// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE Code Generation Templates
// Ports of x86/ix86-32/iR5900Templates.cpp for ARM64 register allocator.
// These templates handle register allocation, constant propagation dispatch,
// and register renaming for the standard instruction patterns.

#include "arm64/iR5900-arm64.h"
#include "Common.h"
#include "Memory.h"
#include "VU.h"
#include "VUmicro.h"

namespace a64 = vixl::aarch64;

////////////////////
// Code Templates //
////////////////////


////////////////////////////////////
// Memory-based scalar templates  //
// No register allocation — all   //
// operands via cpuRegs memory.   //
////////////////////////////////////

// rd = rs OP rt — allocator-resident scalar ALU.
//
// The x86 shape (pcsx2/x86/ix86-32/iR5900Templates.cpp eeRecompileCodeRC0 +
// pcsx2/x86/iCore.cpp _allocX86reg): live sources are held READ in a host GPR so
// a following op re-reads them for free, and the destination is held WRITE
// (deferred canonical store — the existing seam machinery writes it back:
// _eeFlushAllDirty at branch forks, iFlushCall at C calls, the block-tail flush).
// Dead sources fall through to a memory load inside the leaf helper (matching
// x86's liveness gate via EEINST_USEDTEST), so a slot is spent only where it can
// be reused. x0/x1 are carved out of the EE pool (EE_ALLOCATABLE_MASK) so the
// leaves' RWARG scratch + fallback loads can never alias a resident operand.
// Handler bodies are unchanged: the memLoad*/memDestD/memStoreD leaves resolve
// each guest reg through the central pin/allocator accessors and the allocations
// set up here.
void eeRecompileCodeRC0_MEM(R5900FNPTR constcode, R5900FNPTR_INFO constscode, R5900FNPTR_INFO consttcode, R5900FNPTR_INFO noconstcode, int xmminfo)
{
	if (!_Rd_ && (xmminfo & XMMINFO_WRITED))
		return;

	const bool s_is_const = GPR_IS_CONST1(_Rs_);
	const bool t_is_const = GPR_IS_CONST1(_Rt_);

	// Both-const: compile-time evaluation
	if (s_is_const && t_is_const)
	{
		if (_Rd_ && (xmminfo & XMMINFO_WRITED))
		{
			_deleteEEreg(_Rd_, 0);  // discard live homes (NEON writes back UD[1])
			GPR_SET_CONST(_Rd_);
		}
		constcode();
		return;
	}

	// A source gets a pool register only if it is live past this op, or if the
	// dest aliases it (the dest's WRITE alloc reuses the aliased source's home,
	// and every handler consumes its sources before overwriting the dest). A
	// PINNED source is excluded: its authoritative home is the pin mirror (or a
	// live NEON quad), never a pool slot — giving it one would violate the
	// pin/allocator mutual-exclusion invariant and load a stale lazy-dirty value.
	const bool s_needs_reg = (xmminfo & XMMINFO_READS) && !s_is_const && !armEEPinForGPR(_Rs_) &&
		(EEINST_USEDTEST(_Rs_) || ((xmminfo & XMMINFO_WRITED) && _Rd_ == _Rs_));
	const bool t_needs_reg = (xmminfo & XMMINFO_READT) && !t_is_const && !armEEPinForGPR(_Rt_) &&
		(EEINST_USEDTEST(_Rt_) || ((xmminfo & XMMINFO_WRITED) && _Rd_ == _Rt_));

	// Mark every operand needed first so a later alloc can't evict an earlier
	// one mid-op (a resident source must survive until its leaf load reads it).
	if ((xmminfo & XMMINFO_READS) && !s_is_const)
		_addNeededGPRtoArm64GPR(_Rs_);
	if ((xmminfo & XMMINFO_READT) && !t_is_const)
		_addNeededGPRtoArm64GPR(_Rt_);
	if (xmminfo & XMMINFO_WRITED)
		_addNeededGPRtoArm64GPR(_Rd_);

	// Sources first (an aliased-dest source must be resident before the dest
	// allocation can share its home).
	if (s_needs_reg && _checkArm64GPR(ARM64TYPE_GPR, _Rs_, MODE_READ) < 0)
		_allocArm64GPR(ARM64TYPE_GPR, _Rs_, MODE_READ);
	if (t_needs_reg && _checkArm64GPR(ARM64TYPE_GPR, _Rt_, MODE_READ) < 0)
		_allocArm64GPR(ARM64TYPE_GPR, _Rt_, MODE_READ);

	if (xmminfo & (XMMINFO_WRITED | XMMINFO_READD))
	{
		// Free any 128-bit NEON home first, writing back the untouched upper 64
		// (scalar MIPS ops only write UD[0]); a stale NEON copy would otherwise
		// shadow the new GPR home. A pinned dest keeps its pin home — the
		// memDestD/memStoreD leaves route through it (write-through/lazy-dirty).
		_deleteGPRtoNEONreg(_Rd_, DELETE_REG_FREE);
		if (!armEEPinForGPR(_Rd_))
		{
			const int moded = ((xmminfo & XMMINFO_WRITED) ? MODE_WRITE : 0) |
				((xmminfo & XMMINFO_READD) ? MODE_READ : 0);
			_allocArm64GPR(ARM64TYPE_GPR, _Rd_, moded);
		}
	}

	if (xmminfo & XMMINFO_WRITED)
		GPR_DEL_CONST(_Rd_);

	u32 info = 0;  // leaves resolve operands via the pin table / allocator

	if (s_is_const)
	{
		constscode(info);
		return;
	}

	if (t_is_const)
	{
		consttcode(info);
		return;
	}

	noconstcode(info);
}

// rt = rs OP imm16 (memory-based)
void eeRecompileCodeRC1_MEM(R5900FNPTR constcode, R5900FNPTR_INFO noconstcode, int xmminfo)
{
	pxAssert((xmminfo & (XMMINFO_READS | XMMINFO_WRITET)) == (XMMINFO_READS | XMMINFO_WRITET));

	if (!_Rt_)
		return;

	// Const: compile-time evaluation
	if (GPR_IS_CONST1(_Rs_))
	{
		_deleteEEreg(_Rt_, 0);
		GPR_SET_CONST(_Rt_);
		constcode();
		return;
	}

	// Flush source to memory
	_deleteEEreg(_Rs_, 1);

	// Discard dest (about to overwrite)
	_deleteEEreg(_Rt_, 0);
	GPR_DEL_CONST(_Rt_);

	u32 info = 0;
	noconstcode(info);
}

// rd = rt OP sa (memory-based)
void eeRecompileCodeRC2_MEM(R5900FNPTR constcode, R5900FNPTR_INFO noconstcode, int xmminfo)
{
	pxAssert((xmminfo & (XMMINFO_READT | XMMINFO_WRITED)) == (XMMINFO_READT | XMMINFO_WRITED));

	if (!_Rd_)
		return;

	// Const: compile-time evaluation
	if (GPR_IS_CONST1(_Rt_))
	{
		_deleteEEreg(_Rd_, 0);
		GPR_SET_CONST(_Rd_);
		constcode();
		return;
	}

	// Flush source to memory
	_deleteEEreg(_Rt_, 1);

	// Discard dest (about to overwrite)
	_deleteEEreg(_Rd_, 0);
	GPR_DEL_CONST(_Rd_);

	u32 info = 0;
	noconstcode(info);
}

// 128-bit NEON allocation for MMI/XMM operations
int eeRecompileCodeXMM(int xmminfo)
{
	int info = PROCESS_EE_XMM;

	// EEREC_LO, EEREC_HI and EEREC_ACC all decode from the same 5-bit info-word
	// field (see iCore-arm64.h): five distinct 5-bit register fields plus the
	// presence flags do not fit in the 32-bit info word. This is safe only because
	// no op needs two of {LO, HI, ACC} live at once through the allocator — integer
	// MULT/DIV/MADD and PMFHL load LO/HI directly from memory, and ACC is FPU-only.
	// Requesting both LO and HI here would OR two register indices into one field
	// and silently miscompile, so guard it; an op that genuinely needs both must
	// bypass the allocator (see recPMFHL).
	pxAssertRel(!((xmminfo & (XMMINFO_READLO | XMMINFO_WRITELO)) &&
					 (xmminfo & (XMMINFO_READHI | XMMINFO_WRITEHI))),
		"eeRecompileCodeXMM: LO and HI share an info-word field; an op needing both "
		"must bypass the allocator (see recPMFHL).");

	if (xmminfo & (XMMINFO_READLO | XMMINFO_WRITELO))
		_addNeededGPRtoNEONreg(NEONGPR_LO);
	if (xmminfo & (XMMINFO_READHI | XMMINFO_WRITEHI))
		_addNeededGPRtoNEONreg(NEONGPR_HI);
	if (xmminfo & XMMINFO_READS)
		_addNeededGPRtoNEONreg(_Rs_);
	if (xmminfo & XMMINFO_READT)
		_addNeededGPRtoNEONreg(_Rt_);
	if (xmminfo & XMMINFO_WRITED)
		_addNeededGPRtoNEONreg(_Rd_);

	if (xmminfo & XMMINFO_READS)
	{
		const int reg = _allocGPRtoNEONreg(_Rs_, MODE_READ);
		info |= PROCESS_EE_SET_S(reg);
	}
	if (xmminfo & XMMINFO_READT)
	{
		const int reg = _allocGPRtoNEONreg(_Rt_, MODE_READ);
		info |= PROCESS_EE_SET_T(reg);
	}

	if (xmminfo & XMMINFO_WRITED)
	{
		int readd = MODE_WRITE | ((xmminfo & XMMINFO_READD) ? MODE_READ : 0);

		int regd = _checkNEONreg(NEONTYPE_GPRREG, _Rd_, readd);
		if (regd < 0)
		{
			// TODO: register renaming for NEON
			regd = _allocGPRtoNEONreg(_Rd_, readd);
		}
		info |= PROCESS_EE_SET_D(regd);
	}

	// INVARIANT: no EE opcode currently passes XMMINFO_*LO/HI, so these two
	// branches never execute and LO/HI are never NEON-resident — which is why
	// the MMI mul/mac handlers (recPMADDUW/PMFHL/PMTHI/PMTLO in iMMI-arm64.cpp)
	// can Str/Ldr LO/HI straight to memory without an allocator flush. If a
	// future op DOES request XMMINFO_*LO/HI, every direct-memory LO/HI handler
	// in iMMI-arm64.cpp must regain a _deleteGPRtoNEONreg(NEONGPR_LO/HI) flush.
	if (xmminfo & (XMMINFO_READLO | XMMINFO_WRITELO))
	{
		info |= PROCESS_EE_SET_LO(_allocGPRtoNEONreg(NEONGPR_LO,
			((xmminfo & XMMINFO_READLO) ? MODE_READ : 0) | ((xmminfo & XMMINFO_WRITELO) ? MODE_WRITE : 0)));
	}
	if (xmminfo & (XMMINFO_READHI | XMMINFO_WRITEHI))
	{
		info |= PROCESS_EE_SET_HI(_allocGPRtoNEONreg(NEONGPR_HI,
			((xmminfo & XMMINFO_READHI) ? MODE_READ : 0) | ((xmminfo & XMMINFO_WRITEHI) ? MODE_WRITE : 0)));
	}

	if (xmminfo & XMMINFO_WRITED)
		GPR_DEL_CONST(_Rd_);

	_validateRegs();
	return info;
}

// FPU allocation template
#define _Ft_ _Rt_
#define _Fs_ _Rd_
#define _Fd_ _Sa_

void eeFPURecompileCode(R5900FNPTR_INFO xmmcode, R5900FNPTR fpucode, int xmminfo)
{
	int mmregs = -1, mmregt = -1, mmregd = -1, mmregacc = -1;
	int info = PROCESS_EE_XMM;

	if (xmminfo & XMMINFO_READS)
		_addNeededFPtoNEONreg(_Fs_);
	if (xmminfo & XMMINFO_READT)
		_addNeededFPtoNEONreg(_Ft_);
	if (xmminfo & (XMMINFO_WRITED | XMMINFO_READD))
		_addNeededFPtoNEONreg(_Fd_);
	if (xmminfo & (XMMINFO_WRITEACC | XMMINFO_READACC))
		_addNeededFPACCtoNEONreg();

	if (xmminfo & XMMINFO_READT)
		mmregt = _allocFPtoNEONreg(_Ft_, MODE_READ);

	if (xmminfo & XMMINFO_READS)
	{
		mmregs = _allocFPtoNEONreg(_Fs_, MODE_READ);
		if ((xmminfo & XMMINFO_READT) && _Fs_ == _Ft_)
			mmregt = mmregs;
	}

	if (xmminfo & XMMINFO_READD)
	{
		pxAssert(xmminfo & XMMINFO_WRITED);
		mmregd = _allocFPtoNEONreg(_Fd_, MODE_READ);
	}

	if (xmminfo & XMMINFO_READACC)
		mmregacc = _allocFPACCtoNEONreg(MODE_READ);

	if (xmminfo & XMMINFO_WRITEACC)
	{
		int readacc = MODE_WRITE | ((xmminfo & XMMINFO_READACC) ? MODE_READ : 0);
		mmregacc = _checkNEONreg(NEONTYPE_FPACC, 0, readacc);
		if (mmregacc < 0)
			mmregacc = _allocFPACCtoNEONreg(readacc);
		arm64neon[mmregacc].mode |= MODE_WRITE;
	}
	else if (xmminfo & XMMINFO_WRITED)
	{
		int readd = MODE_WRITE | ((xmminfo & XMMINFO_READD) ? MODE_READ : 0);
		if (xmminfo & XMMINFO_READD)
			mmregd = _allocFPtoNEONreg(_Fd_, readd);
		else
			mmregd = _checkNEONreg(NEONTYPE_FPREG, _Fd_, readd);

		if (mmregd < 0)
			mmregd = _allocFPtoNEONreg(_Fd_, readd);
	}

	pxAssert(mmregs >= 0 || mmregt >= 0 || mmregd >= 0 || mmregacc >= 0);

	if (xmminfo & XMMINFO_WRITED)
	{
		pxAssert(mmregd >= 0);
		info |= PROCESS_EE_SET_D(mmregd);
	}
	if (xmminfo & (XMMINFO_WRITEACC | XMMINFO_READACC))
	{
		if (mmregacc >= 0)
			info |= PROCESS_EE_SET_ACC(mmregacc) | PROCESS_EE_ACC;
		else
			pxAssert(!(xmminfo & XMMINFO_WRITEACC));
	}

	if (xmminfo & XMMINFO_READS)
	{
		if (mmregs >= 0)
			info |= PROCESS_EE_SET_S(mmregs);
	}
	if (xmminfo & XMMINFO_READT)
	{
		if (mmregt >= 0)
			info |= PROCESS_EE_SET_T(mmregt);
	}

	xmmcode(info);
}

#undef _Ft_
#undef _Fs_
#undef _Fd_
