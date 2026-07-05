// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

// EE guest-GPR register-heat collector (EE-SRA campaign, stage S0).
//
// Measures how dynamic guest-GPR references distribute across the 32 EE
// GPRs per game, to drive the static pin-assignment table (which guest regs
// earn the N pinned host registers) and to predict pin coverage — the EE
// analog of the access-coverage curve in Zurstraßen et al., DATE 2025.
//
// Off unless the environment variable PCSX2_EE_REGHEAT_DIR names a writable
// directory. When off: no emitted code, no records, and the only residual
// cost is one null-pointer check per backprop register reference at
// compile time.
//
// When enabled:
//  - recRecompile allocates one record per compiled block and emits a
//    short exec-counter increment at block entry. Every entry passes
//    through block_fnptr — dispatcher and statically-linked alike — so the
//    counter sees all executions.
//  - The liveness backprop pass reports every guest-GPR reference through
//    the recBackpropSetGPR* macros (EEREGHEAT_REF in iR5900Analysis.cpp),
//    split read/write and 64-bit/128-bit. 128-bit refs can never be served
//    by a 64-bit pin (the upper half is not mirrored), so they are tallied
//    separately.
//  - recResetRaw()/recShutdown() append all records to a CSV in the dump
//    directory and clear the arena (records die with the code cache; a
//    block recompiled after SMC simply produces another record for the
//    same startpc — the offline aggregator sums by pc).
//
// All entry points run on the EE/CPU thread only (compile + reset seams);
// there is no locking.
//
// Offline: tools/perf/ee_reg_heat_report.py aggregates the CSVs into the
// weighted histogram, the top-N coverage curve, and a pin-ladder proposal.

namespace EERegHeat
{
	bool IsEnabled();

	// Block-entry seam in recRecompile: opens the record for startpc and
	// returns the exec-counter cell the block's entry code must increment,
	// or nullptr when disabled. Also selects the record that subsequent
	// Ref() calls accumulate into. An unclosed prior record (aborted
	// compile) is finalized as-is.
	u64* BeginBlock(u32 startpc);

	// Static-reference tally from the backprop macros ($zero never arrives;
	// the macros guard it). No-op when no record is open. Counts saturate
	// at 0xFFFF per register per block.
	void Ref(u32 gpr, bool write, bool is128);

	// Closes the record opened by BeginBlock once the analysed extent is
	// known. No-op when no record is open.
	void EndBlock(u32 insns);

	// Appends all records to the dump file and clears the arena. `reason`
	// lands in the section header. When disabled, clears silently.
	void DumpAndReset(const char* reason);

	// ---- Test hooks ----

	// Force-enable with an explicit directory; nullptr restores the
	// environment-derived state (typically: disabled).
	void OverrideDirForTesting(const char* dir);

	struct RecordView
	{
		u32 startpc;
		u32 insns;
		u64 exec;
		u16 r64[32];
		u16 w64[32];
		u16 r128[32];
		u16 w128[32];
	};

	// Copies the most recent record for startpc into *out. Returns false
	// when no record exists.
	bool FindRecordForTesting(u32 startpc, RecordView* out);
} // namespace EERegHeat
