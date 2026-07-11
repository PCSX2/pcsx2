// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE Jump Instruction Codegen

#include "arm64/iR5900-arm64.h"
#include "Config.h"
#include "vtlb.h"
#include "common/Console.h"

namespace a64 = vixl::aarch64;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

namespace Interp = R5900::Interpreter::OpcodeImpl;

#ifdef FORCE_INTERP_JUMP
REC_SYS(J);
REC_SYS(JAL);
REC_SYS(JR);
REC_SYS(JALR);
#else

/*********************************************************
 * Jump to target                                        *
 * Format: OP target                                     *
 *********************************************************/

//// J
void recJ()
{
	u32 newpc = (_InstrucTarget_ << 2) + (pc & 0xf0000000);
	recompileNextInstruction(true, false);
	if (EmuConfig.Gamefixes.GoemonTlbHack)
		SetBranchImm(vtlb_V2P(newpc));
	else
		SetBranchImm(newpc);
}

//// JAL — jump and link (r31 = return address)
void recJAL()
{
	u32 newpc = (_InstrucTarget_ << 2) + (pc & 0xf0000000);
	const u32 retpc = pc + 4; // == the $ra link value; capture before the delay slot advances pc
	_deleteEEreg(31, 0);
	if (EE_CONST_PROP)
	{
		GPR_SET_CONST(31);
		g_cpuConstRegs[31].UL[0] = retpc;
		g_cpuConstRegs[31].UL[1] = 0;
	}
	else
	{
		armAsm->Mov(RXSCRATCH, (u64)retpc);
		armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[31].UD[0]);
	}

	recompileNextInstruction(true, false);
	if (EmuConfig.Gamefixes.GoemonTlbHack)
		SetBranchImm(vtlb_V2P(newpc));
#if EE_CALLRET_STACK
	else
		SetBranchImmCall(newpc, retpc);
#else
	else
		SetBranchImm(newpc);
#endif
}

/*********************************************************
 * Register jump                                         *
 * Format: OP rs, rd                                     *
 *********************************************************/

//// JR — jump to address in rs
void recJR()
{
	const u32 rs = _Rs_;

	// Save jump target to memory BEFORE delay slot, so it can't be lost
	// if the delay slot evicts registers. A pinned/allocator-resident rs is
	// stored directly (WS-C5) — _deleteEEreg(rs, 1) flushed first, so the
	// pin is coherent (post-flush contract of _eeGetGPRSourceReg).
	_deleteEEreg(rs, 1); // flush rs to memory
	armStoreEERegPtr(_eeGetGPRSourceReg(RWSCRATCH, rs), &cpuRegs.pcWriteback);

	recompileNextInstruction(true, false);

	// JR $ra is the ABI return idiom — pop the call-ret ring and RET so the
	// hardware RAS (pushed by the paired call-site BL) predicts the target.
	// Emit-gated off under GoemonTlbHack: SetBranchReg compares V2P-translated
	// targets there, which can never match the virtual frame RAs.
	if (rs == 31 && !EmuConfig.Gamefixes.GoemonTlbHack)
		SetBranchReg(EEBranchRegMode::Return);
	else
		SetBranchReg();
}

//// JALR — jump to rs, link in rd
void recJALR()
{
	const u32 rs = _Rs_;
	const u32 rd = _Rd_;
	const u32 newpc = pc + 4;

	// Save jump target to memory BEFORE delay slot.
	// Must read rs before writing rd in case rd == rs — the Str below
	// captures the target into pcWriteback before the rd write can refresh
	// a shared pin. (WS-C5; post-flush pin coherence via _deleteEEreg.)
	_deleteEEreg(rs, 1); // flush rs to memory
	armStoreEERegPtr(_eeGetGPRSourceReg(RWSCRATCH, rs), &cpuRegs.pcWriteback);

	// Write link address to rd
	if (rd)
	{
		_deleteEEreg(rd, 0);
		if (EE_CONST_PROP)
		{
			GPR_SET_CONST(rd);
			g_cpuConstRegs[rd].UL[0] = newpc;
			g_cpuConstRegs[rd].UL[1] = 0;
		}
		else
		{
			armAsm->Mov(RXSCRATCH, (u64)newpc);
			armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[rd].UD[0]);
		}
	}

	recompileNextInstruction(true, false);

	// JALR linking into $ra is the ABI indirect-call idiom — push a call-ret
	// frame and transfer via BL so the callee's return RETs to our landing.
	// Other link registers don't pair with the JR-$ra pop, so they take the
	// plain jump (their returns just compare-miss if the callee uses jr $ra).
	if (rd == 31 && !EmuConfig.Gamefixes.GoemonTlbHack)
		SetBranchReg(EEBranchRegMode::Call, newpc);
	else
		SetBranchReg();
}

#endif // !FORCE_INTERP_JUMP

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
