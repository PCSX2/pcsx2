// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) macro-mode analysis — Phase 7.9 (M0).
//
// Arch-neutral analysis ported faithfully from the x86 recompiler. This file is
// only compiled on ARM64 (pcsx2arm64Sources). It holds no VIXL / NEON emission;
// it is pure decode/analysis over EE opcodes, so the logic is copied verbatim
// from pcsx2/x86/ix86-32/iR5900.cpp (cop2flags) and pcsx2/x86/iR5900Analysis.cpp
// (the COP2 passes, ported in M1). Keeping it in its own TU mirrors the x86
// split and keeps aR5900.cpp focused on codegen.

#include "arm64/aR5900Analysis.h"

#include "Config.h"
#include "Memory.h"
#include "R5900.h"
#include "VU.h"
#include "DebugTools/Debug.h"

#include "common/Console.h"

#include <cstdlib>
#include <string>

using namespace R5900;

// Set per-op in recRecompile (aR5900.cpp); the macro-mode emit reads the COP2_*
// bits on g_pCurInstInfo->info to drive lazy VU0 sync. Defined here so both the
// analysis passes and the rec share one definition.
EEINST* g_pCurInstInfo = nullptr;

// opcode 'code' modifies:
// 1: status
// 2: MAC
// 4: clip
//
// Verbatim port of cop2flags() from pcsx2/x86/ix86-32/iR5900.cpp:1384 — pure bit
// decode, no x86 specifics. (022 == 0x12 == COP2 primary opcode.)
int cop2flags(u32 code)
{
	if (code >> 26 != 022)
		return 0; // not COP2
	if ((code >> 25 & 1) == 0)
		return 0; // a branch or transfer instruction

	switch (code >> 2 & 15)
	{
		case 15:
			switch (code >> 6 & 0x1f)
			{
				case 4: // ITOF*
				case 5: // FTOI*
				case 12: // MOVE MR32
				case 13: // LQI SQI LQD SQD
				case 15: // MTIR MFIR ILWR ISWR
				case 16: // RNEXT RGET RINIT RXOR
					return 0;
				case 7: // MULAq, ABS, MULAi, CLIP
					if ((code & 3) == 1) // ABS
						return 0;
					if ((code & 3) == 3) // CLIP
						return 4;
					return 3;
				case 11: // SUBA, MSUBA, OPMULA, NOP
					if ((code & 3) == 3) // NOP
						return 0;
					return 3;
				case 14: // DIV, SQRT, RSQRT, WAITQ
					if ((code & 3) == 3) // WAITQ
						return 0;
					return 1; // but different timing, ugh
				default:
					break;
			}
			break;
		case 4: // MAXbc
		case 5: // MINbc
		case 12: // IADD, ISUB, IADDI
		case 13: // IAND, IOR
		case 14: // VCALLMS, VCALLMSR
			return 0;
		case 7:
			if ((code & 1) == 1) // MAXi, MINIi
				return 0;
			return 3;
		case 10:
			if ((code & 3) == 3) // MAX
				return 0;
			return 3;
		case 11:
			if ((code & 3) == 3) // MINI
				return 0;
			return 3;
		default:
			break;
	}
	return 3;
}

// --------------------------------------------------------------------------------------
//  COP2 analysis passes — faithful port of pcsx2/x86/iR5900Analysis.cpp
// --------------------------------------------------------------------------------------
// Ported verbatim; the only ARM64 difference is the inst-cache indexing convention
// (no-offset map — see aR5900Analysis.h and MACRO_MODE_PLAN.md Phase M1). The backward-
// liveness back-prop tables (recBackpropBSC etc.) are NOT ported — ARM64 has no EE
// register allocator/liveness pass yet, and these two COP2 passes don't depend on them.

AnalysisPass::AnalysisPass() = default;

AnalysisPass::~AnalysisPass() = default;

void AnalysisPass::Run(u32 start, u32 end, EEINST* inst_cache)
{
}

template <class F>
void __fi AnalysisPass::ForEachInstruction(u32 start, u32 end, EEINST* inst_cache, const F& func)
{
	EEINST* eeinst = inst_cache;
	for (u32 apc = start; apc < end; apc += 4, eeinst++)
	{
		cpuRegs.code = memRead32(apc);
		if (!func(apc, eeinst))
			break;
	}
}

template <class F>
void __fi R5900::AnalysisPass::DumpAnnotatedBlock(u32 start, u32 end, EEINST* inst_cache, const F& func)
{
	std::string d;
	EEINST* eeinst = inst_cache;
	for (u32 apc = start; apc < end; apc += 4, eeinst++)
	{
		const u32 code = memRead32(apc);
		d.clear();
		disR5900Fasm(d, code, apc, false);
		func(apc, eeinst, d);
		Console.WriteLn("  %08X %08X %s", apc, code, d.c_str());
	}
}

COP2FlagHackPass::COP2FlagHackPass()
	: AnalysisPass()
{
}

COP2FlagHackPass::~COP2FlagHackPass() = default;

void COP2FlagHackPass::Run(u32 start, u32 end, EEINST* inst_cache)
{
	m_status_denormalized = false;
	m_last_status_write = nullptr;
	m_last_mac_write = nullptr;
	m_last_clip_write = nullptr;
	m_cfc2_pc = start;

	ForEachInstruction(start, end, inst_cache, [this, end](u32 apc, EEINST* inst) {
		// catch SB/SH/SW to potential DMA->VIF0->VU0 exec.
		// this is very unlikely in a cop2 chain.
		if (_Opcode_ == 050 || _Opcode_ == 051 || _Opcode_ == 053)
		{
			CommitAllFlags();
			return true;
		}
		else if (_Opcode_ != 022)
		{
			// not COP2
			return true;
		}

		// Detect ctc2 Status, zero, ..., cfc2 v0, Status pattern where we need accurate sticky bits.
		// Test case: Tekken Tag Tournament.
		if (_Rs_ == 6 && _Rd_ == REG_STATUS_FLAG)
		{
			// Read ahead, looking for cfc2.
			m_cfc2_pc = apc;
			ForEachInstruction(apc, end, inst, [this](u32 capc, EEINST*) {
				if (_Opcode_ == 022 && _Rs_ == 2 && _Rd_ == REG_STATUS_FLAG)
				{
					m_cfc2_pc = capc;
					return false;
				}
				return true;
			});
#ifdef PCSX2_DEVBUILD
			if (m_cfc2_pc != apc)
				DevCon.WriteLn("CTC2 at %08X paired with CFC2 %08X", apc, m_cfc2_pc);
#endif
		}

		// CFC2/CTC2
		if (_Rs_ == 6 || _Rs_ == 2)
		{
			switch (_Rd_)
			{
				case REG_STATUS_FLAG:
					CommitStatusFlag();
					break;
				case REG_MAC_FLAG:
					CommitMACFlag();
					break;
				case REG_CLIP_FLAG:
					CommitClipFlag();
					break;
				case REG_FBRST:
				{
					// only apply to CTC2, is FBRST readable?
					if (_Rs_ == 2)
						CommitAllFlags();
				}
				break;
			}
		}

		if (((cpuRegs.code >> 25 & 1) == 1) && ((cpuRegs.code >> 2 & 15) == 14))
		{
			// VCALLMS, everything needs to be up to date
			CommitAllFlags();
		}

		// 1 - status, 2 - mac, 3 - clip
		const int flags = cop2flags(cpuRegs.code);
		if (flags == 0)
			return true;

		// STATUS
		if (flags & 1)
		{
			if (!m_status_denormalized)
			{
				inst->info |= EEINST_COP2_DENORMALIZE_STATUS_FLAG;
				m_status_denormalized = true;
			}

			// If we're still behind the next CFC2 after the sticky bits got cleared, we need to update flags.
			// Also do this if we're a vsqrt/vrsqrt/vdiv, these update status unconditionally.
			const u32 sub_opcode = (cpuRegs.code & 3) | ((cpuRegs.code >> 4) & 0x7c);
			if (apc < m_cfc2_pc || (_Rs_ >= 020 && _Funct_ >= 074 && sub_opcode >= 070 && sub_opcode <= 072))
				inst->info |= EEINST_COP2_STATUS_FLAG;

			m_last_status_write = inst;
		}

		// MAC
		if (flags & 2)
		{
			m_last_mac_write = inst;
		}

		// CLIP
		if (flags & 4)
		{
			// we don't track the clip flag yet..
			// but it's unlikely that we'll have more than 4 clip flags in a row, because that would be pointless?
			inst->info |= EEINST_COP2_CLIP_FLAG;
			m_last_clip_write = inst;
		}

		return true;
	});

	CommitAllFlags();

#if 0
	if (m_cfc2_pc != start)
		DumpAnnotatedBlock(start, end, inst_cache);
#endif
}

void COP2FlagHackPass::DumpAnnotatedBlock(u32 start, u32 end, EEINST* inst_cache)
{
	AnalysisPass::DumpAnnotatedBlock(start, end, inst_cache, [](u32, EEINST* eeinst, std::string& d) {
		if (eeinst->info & EEINST_COP2_DENORMALIZE_STATUS_FLAG)
			d.append(" COP2_DENORMALIZE_STATUS_FLAG");
		if (eeinst->info & EEINST_COP2_NORMALIZE_STATUS_FLAG)
			d.append(" COP2_NORMALIZE_STATUS_FLAG");
		if (eeinst->info & EEINST_COP2_STATUS_FLAG)
			d.append(" COP2_STATUS_FLAG");
		if (eeinst->info & EEINST_COP2_MAC_FLAG)
			d.append(" COP2_MAC_FLAG");
		if (eeinst->info & EEINST_COP2_CLIP_FLAG)
			d.append(" COP2_CLIP_FLAG");
	});
}

void COP2FlagHackPass::CommitStatusFlag()
{
	if (m_last_status_write)
	{
		m_last_status_write->info |= EEINST_COP2_STATUS_FLAG | EEINST_COP2_NORMALIZE_STATUS_FLAG;
		m_status_denormalized = false;
	}
}

void COP2FlagHackPass::CommitMACFlag()
{
	if (m_last_mac_write)
		m_last_mac_write->info |= EEINST_COP2_MAC_FLAG;
}

void COP2FlagHackPass::CommitClipFlag()
{
	if (m_last_clip_write)
		m_last_clip_write->info |= EEINST_COP2_CLIP_FLAG;
}

void COP2FlagHackPass::CommitAllFlags()
{
	CommitStatusFlag();
	CommitMACFlag();
	CommitClipFlag();
}

COP2MicroFinishPass::COP2MicroFinishPass() = default;

COP2MicroFinishPass::~COP2MicroFinishPass() = default;

void COP2MicroFinishPass::Run(u32 start, u32 end, EEINST* inst_cache)
{
	bool needs_vu0_sync = true;
	bool needs_vu0_finish = true;
	bool block_interlocked = CHECK_FULLVU0SYNCHACK;

	// First pass through the block to find out if it's interlocked or not. If it is, we need to use tighter
	// synchronization on all COP2 instructions, otherwise Crash Twinsanity breaks.
	ForEachInstruction(start, end, inst_cache, [&block_interlocked](u32 apc, EEINST* inst) {
		if (_Opcode_ == 022 && (_Rs_ == 001 || _Rs_ == 002 || _Rs_ == 005 || _Rs_ == 006) && cpuRegs.code & 1)
		{
			block_interlocked = true;
			return false;
		}
		return true;
	});

	ForEachInstruction(start, end, inst_cache, [this, start, end, inst_cache, &needs_vu0_sync, &needs_vu0_finish, block_interlocked](u32 apc, EEINST* inst) {
		// Catch SQ/SB/SH/SW/SD to potential DMA->VIF0->VU0 exec.
		// Also VCALLMS/VCALLMSR, that can start a micro, so the next instruction needs to finish it.
		// This is very unlikely in a cop2 chain.
		if (_Opcode_ == 050 || _Opcode_ == 051 || _Opcode_ == 053 || _Opcode_ == 077 || (_Opcode_ == 022 && _Rs_ >= 020 && (_Funct_ == 070 || _Funct_ == 071)))
		{
			// If we started a micro, we'll need to finish it before the first COP2 instruction.
			needs_vu0_sync = true;
			needs_vu0_finish = true;
			inst->info |= EEINST_COP2_FLUSH_VU0_REGISTERS;
			return true;
		}

		// LQC2/SQC2 - these don't interlock with VU0, but still sync, so we can persist the cached registers
		// for a LQC2..COP2 sequence. If there's no COP2 instructions following, don't bother, just yolo it.
		// We do either a sync or a finish here depending on which COP2 instruction follows - we don't want
		// to run the program until end if there's nothing which would actually trigger that.
		//
		// In essence, what we're doing is moving the finish from the COP2 instruction to the LQC2 in a LQC2..COP2
		// chain, so that we can preserve the cached registers and not need to reload them.
		//
		const bool is_lqc_sqc = (_Opcode_ == 066 || _Opcode_ == 076);
		const bool is_non_interlocked_move = (_Opcode_ == 022 && _Rs_ < 020 && ((cpuRegs.code & 1) == 0));
		// Moving zero to the VU registers, so likely removing a loop/lock.
		const bool likely_clear = _Opcode_ == 022 && _Rs_ < 020 && _Rs_ > 004 && _Rt_ == 000;
		if ((needs_vu0_sync && (is_lqc_sqc || is_non_interlocked_move)) || likely_clear)
		{
			bool following_needs_finish = false;
			// No-offset look-ahead: instruction at (apc + 4) maps to &inst_cache[(apc + 4 - start) >> 2]
			// (x86 used the placeholder inst_cache + 1 here; the EEINST* is unused in this lambda, but
			// we keep the producer on the same convention as the consumer — see header note).
			ForEachInstruction(apc + 4, end, &inst_cache[(apc + 4 - start) >> 2], [&following_needs_finish](u32 apc2, EEINST* inst2) {
				if (_Opcode_ == 022)
				{
					// For VCALLMS/VCALLMSR, we only sync, because the VCALLMS in itself will finish.
					// Since we're paying the cost of syncing anyway, better to be less risky.
					if (_Rs_ >= 020 && (_Funct_ == 070 || _Funct_ == 071))
						return false;

					// Allow the finish from COP2 to be moved to the first LQC2 of LQC2..QMTC2..COP2.
					// Otherwise, keep searching for a finishing COP2.
					following_needs_finish = _Rs_ >= 020;
					if (following_needs_finish)
						return false;
				}

				return true;
			});
			if (following_needs_finish && !block_interlocked)
			{
				inst->info |= EEINST_COP2_FLUSH_VU0_REGISTERS | EEINST_COP2_FINISH_VU0;
				needs_vu0_sync = false;
				needs_vu0_finish = false;
			}
			else
			{
				inst->info |= EEINST_COP2_FLUSH_VU0_REGISTERS | EEINST_COP2_SYNC_VU0;
				needs_vu0_sync = block_interlocked || (is_non_interlocked_move && likely_clear);
				needs_vu0_finish = true;
			}

			return true;
		}

		// Look for COP2 instructions.
		if (_Opcode_ != 022)
			return true;

		// Set the flag on the current instruction, and clear it for the next.
		if (_Rs_ >= 020 && needs_vu0_finish)
		{
			inst->info |= EEINST_COP2_FLUSH_VU0_REGISTERS | EEINST_COP2_FINISH_VU0;
			needs_vu0_finish = false;
			needs_vu0_sync = false;
		}
		else if (needs_vu0_sync)
		{
			// Starting a sync-free block!
			inst->info |= EEINST_COP2_FLUSH_VU0_REGISTERS | EEINST_COP2_SYNC_VU0;
			needs_vu0_sync = block_interlocked;
		}

		return true;
	});
}

// --------------------------------------------------------------------------------------
//  M1.3 — env-gated annotated dump (ARM64 verification aid)
// --------------------------------------------------------------------------------------
// x86 gates its DumpAnnotatedBlock behind compile-time #if 0; on ARM64 we expose a
// runtime env switch (EE_COP2_DUMP=1) so a block's computed COP2 flags can be spot-
// checked against an x86 build without a recompile. Pure diagnostic — no behavior change.
// Iterates the same no-offset inst-cache the passes wrote (instruction at pc ↔
// inst_cache[(pc-start)>>2]).
void eeDumpCOP2AnnotatedBlock(u32 start, u32 end, EEINST* inst_cache)
{
	static const bool enabled = (std::getenv("EE_COP2_DUMP") != nullptr);
	if (!enabled)
		return;

	Console.WriteLn("-- COP2 block %08X - %08X --", start, end);
	std::string d;
	EEINST* eeinst = inst_cache;
	for (u32 apc = start; apc < end; apc += 4, eeinst++)
	{
		const u32 code = memRead32(apc);
		d.clear();
		disR5900Fasm(d, code, apc, false);
		if (eeinst->info & EEINST_COP2_DENORMALIZE_STATUS_FLAG)
			d.append(" COP2_DENORMALIZE_STATUS_FLAG");
		if (eeinst->info & EEINST_COP2_NORMALIZE_STATUS_FLAG)
			d.append(" COP2_NORMALIZE_STATUS_FLAG");
		if (eeinst->info & EEINST_COP2_STATUS_FLAG)
			d.append(" COP2_STATUS_FLAG");
		if (eeinst->info & EEINST_COP2_MAC_FLAG)
			d.append(" COP2_MAC_FLAG");
		if (eeinst->info & EEINST_COP2_CLIP_FLAG)
			d.append(" COP2_CLIP_FLAG");
		if (eeinst->info & EEINST_COP2_SYNC_VU0)
			d.append(" COP2_SYNC_VU0");
		if (eeinst->info & EEINST_COP2_FINISH_VU0)
			d.append(" COP2_FINISH_VU0");
		if (eeinst->info & EEINST_COP2_FLUSH_VU0_REGISTERS)
			d.append(" COP2_FLUSH_VU0_REGISTERS");
		Console.WriteLn("  %08X %08X %s", apc, code, d.c_str());
	}
}
