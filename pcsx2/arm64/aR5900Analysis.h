// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) analysis support — Phase 7.9 (macro mode).
//
// Arch-neutral EE instruction-info (EEINST) plumbing + the COP2 flag decode,
// mirrored from the x86 recompiler so the macro-mode COP2 analysis passes
// (COP2FlagHackPass / COP2MicroFinishPass, ported in M1) can be run by the ARM64
// EE rec. These declarations carry NO x86 specifics — they are the same liveness
// substrate the x86 rec uses (pcsx2/x86/iCore.h:170-220, ix86-32/iR5900.cpp:1384).
//
// Only the COP2_* flag bits are consumed on ARM64 today (M3+). The general
// liveness fields (regs/fpuregs/vfregs/viregs, read/write sets) are kept so the
// ported passes match the x86 source 1:1; they are not yet populated by a
// backward-liveness pass on ARM64 (that is a later perf phase — see
// MACRO_MODE_PLAN.md §2).

#pragma once

#include "common/Pcsx2Defs.h"

// --------------------------------------------------------------------------------------
//  Instruction info (mirrors pcsx2/x86/iCore.h:170-220)
// --------------------------------------------------------------------------------------
// Liveness flags (general). On ARM64 only the COP2_* bits below are used so far.
#define EEINST_LIVE    1 // if var is ever used (read or write)
#define EEINST_LASTUSE 8 // if var isn't written/read anymore
#define EEINST_XMM  0x20 // var will be used in xmm ops
#define EEINST_USED 0x40

// COP2 macro-mode analysis bits — set by the M1 passes, consumed by the M2/M3 emit.
#define EEINST_COP2_DENORMALIZE_STATUS_FLAG 0x100
#define EEINST_COP2_NORMALIZE_STATUS_FLAG   0x200
#define EEINST_COP2_STATUS_FLAG             0x400
#define EEINST_COP2_MAC_FLAG                0x800
#define EEINST_COP2_CLIP_FLAG               0x1000
#define EEINST_COP2_SYNC_VU0                0x2000
#define EEINST_COP2_FINISH_VU0              0x4000
#define EEINST_COP2_FLUSH_VU0_REGISTERS     0x8000


struct EEINST
{
	u16 info; // extra info, if 1 inst is COP1, 2 inst is COP2. Also uses EEINST_XMM
	u8 regs[34]; // includes HI/LO (HI=32, LO=33)
	u8 fpuregs[33]; // ACC=32
	u8 vfregs[34]; // ACC=32, I=33
	u8 viregs[16];

	// uses XMMTYPE_ flags; if type == XMMTYPE_TEMP, not used
	u8 writeType[3], writeReg[3]; // reg written in this inst, 0 if no reg
	u8 readType[4], readReg[4];
};

// info for the cur instruction (set per-op in recRecompile; mirrors x86 g_pCurInstInfo)
extern EEINST* g_pCurInstInfo;

// --------------------------------------------------------------------------------------
//  COP2 flag decode (faithful port of cop2flags(), ix86-32/iR5900.cpp:1384)
// --------------------------------------------------------------------------------------
// Returns which VU0 flags opcode 'code' modifies:
//   1: status   2: MAC   4: clip
int cop2flags(u32 code);

// --------------------------------------------------------------------------------------
//  COP2 macro-mode analysis passes (faithful port of pcsx2/x86/iR5900Analysis.cpp)
// --------------------------------------------------------------------------------------
// These passes emit NO code — they are pure analysis over the per-block EEINST
// inst-cache, setting the EEINST_COP2_* bits that the macro-mode emit (M2/M3) reads.
// Ported 1:1 from the x86 rec (Phase 7.9 M1).
//
// ⚠ Indexing convention differs from x86 by design. x86 maps instruction at `pc` to
// inst_cache[(pc-start)>>2 + 1] — it calls these passes with `s_pInstCache + 1` and
// reserves slot 0 as the backward-liveness "previous" boundary. ARM64 has no liveness
// backprop pass (deferred — see MACRO_MODE_PLAN.md §2), so it uses a *no-offset* map:
// instruction at `pc` ↔ inst_cache[(pc-start)>>2]. recRecompile calls Run() with the
// base `s_instCache` (no +1), matching the per-op `g_pCurInstInfo` it hands the emit
// loop. The SPECIAL2 look-ahead in COP2MicroFinishPass therefore re-bases to
// &inst_cache[(apc+4-start)>>2] instead of x86's `inst_cache + 1`.
namespace R5900
{
	class AnalysisPass
	{
	public:
		AnalysisPass();
		virtual ~AnalysisPass();

		/// Runs the actual pass.
		virtual void Run(u32 start, u32 end, EEINST* inst_cache);

	protected:
		/// Takes a functor of bool(pc, EEINST*), returning false if iteration should stop.
		template <class F>
		void ForEachInstruction(u32 start, u32 end, EEINST* inst_cache, const F& func);

		/// Dumps the block to the console, calling the functor void(pc, EEINST*, std::string&) for each instruction.
		template <class F>
		void DumpAnnotatedBlock(u32 start, u32 end, EEINST* inst_cache, const F& func);
	};

	class COP2FlagHackPass final : public AnalysisPass
	{
	public:
		COP2FlagHackPass();
		~COP2FlagHackPass();

		void Run(u32 start, u32 end, EEINST* inst_cache) override;

	private:
		void DumpAnnotatedBlock(u32 start, u32 end, EEINST* inst_cache);

		void CommitStatusFlag();
		void CommitMACFlag();
		void CommitClipFlag();
		void CommitAllFlags();

		bool m_status_denormalized = false;
		EEINST* m_last_status_write = nullptr;
		EEINST* m_last_mac_write = nullptr;
		EEINST* m_last_clip_write = nullptr;

		u32 m_cfc2_pc = 0;
	};

	class COP2MicroFinishPass final : public AnalysisPass
	{
	public:
		COP2MicroFinishPass();
		~COP2MicroFinishPass();

		void Run(u32 start, u32 end, EEINST* inst_cache) override;
	};
} // namespace R5900

// M1.3 — env-gated (EE_COP2_DUMP=1) annotated dump of a block's computed COP2 flags.
// Pure diagnostic; no-op unless the env var is set. (No behavior change.)
void eeDumpCOP2AnnotatedBlock(u32 start, u32 end, EEINST* inst_cache);

