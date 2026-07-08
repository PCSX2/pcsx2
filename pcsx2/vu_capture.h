// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#ifdef PCSX2_RECOMPILER_TESTS

#include "common/Pcsx2Defs.h"

#include <cstdio>
#include <string>
#include <vector>

struct VURegs;

// VU microprogram capture format. Used by the live capture probe in
// mVUexecute (vu_capture::MaybeCapture) to dump the microcode + entry
// register/memory state of executing VU programs to disk, and by
// pcsx2-vurunner / VuTestHarness::LoadFromFile to replay them in a tight
// codegen-iteration loop without having to boot a game.
//
// The whole module is gated by PCSX2_RECOMPILER_TESTS — release builds omit
// it entirely (zero symbol leakage, like gif_test_hooks).
namespace vu_capture
{
	// File magic 'PCSX2VUC' (no NUL — exactly 8 bytes).
	inline constexpr char kMagic[8] = {'P', 'C', 'S', 'X', '2', 'V', 'U', 'C'};

	// Bump on any on-disk layout change. Readers reject mismatches.
	inline constexpr u32 kVersion = 1;

	// Architecturally-significant subset of VURegs serialized to disk.
	// Mirrors the fields VuSnapshot.h treats as architectural; VURegs's
	// pipeline-modeling and dispatcher-bookkeeping fields are omitted.
	//
	// Layout is fixed across builds (plain trivial types, no VECTOR /
	// REG_VI) so a capture written today round-trips against a future
	// CapturedState as long as kVersion stays the same.
	#pragma pack(push, 1)
	struct CapturedState
	{
		// VF[32] — 4 lanes × 32 regs, written as little-endian u32 lanes.
		u32 VF[32][4];
		// VI[32] — full 32-bit value (only low 16 bits architectural for
		// most regs; REG_R/Q/P/I/STATUS/MAC/CLIP/TPC use all 32 bits).
		u32 VI[32];
		// Accumulator — 4 lanes.
		u32 ACC[4];
		// Scalar pipeline stages (also live in VI[REG_Q]/VI[REG_P], but
		// captured separately for round-trip safety).
		u32 q;
		u32 p;
		// Interpreter staging scalars — microVU commits them at E-bit.
		u32 pending_q;
		u32 pending_p;
		// microVU shadow flag pipelines.
		u32 micro_macflags[4];
		u32 micro_clipflags[4];
		u32 micro_statusflags[4];
		// XGKICK state (VU1 only — populated regardless; all-zero for VU0).
		u32 xgkickaddr;
		u32 xgkickdiff;
		u32 xgkicksizeremaining;
		u64 xgkicklastcycle;
		u32 xgkickcyclecount;
		u32 xgkickenable;
		u32 xgkickendpacket;
	};
	#pragma pack(pop)
	static_assert(sizeof(CapturedState) == 32 * 16 + 32 * 4 + 16 + 4 + 4 + 4 + 4 + 48 + 24 + 8,
		"CapturedState layout drifted — bump kVersion if intentional");

	// One captured execution of one VU microprogram.
	struct CaptureRecord
	{
		u8 vu_index = 0;       // 0 or 1
		u32 start_pc = 0;      // entry microPC (byte offset into Micro)
		u32 cycle_budget = 0;  // cycles arg passed to mVUexecute
		// Whole-program-memory snapshot. Size = VU0_PROGSIZE (4 KB) for
		// vu_index 0, VU1_PROGSIZE (16 KB) for vu_index 1.
		std::vector<u8> microcode;
		// Whole VU data memory snapshot. Size = VU0_MEMSIZE (4 KB) for
		// vu_index 0, VU1_MEMSIZE (16 KB) for vu_index 1.
		std::vector<u8> vumem;
		CapturedState state{};
	};

	// On-disk header (packed; matches the bytes WriteToFile emits).
	#pragma pack(push, 1)
	struct FileHeader
	{
		char magic[8];        // kMagic
		u32 version;          // kVersion
		u8 vu_index;
		u8 _pad[3];
		u32 start_pc;
		u32 cycle_budget;
		u32 microcode_size;   // payload bytes
		u32 vumem_size;       // payload bytes
	};
	#pragma pack(pop)
	static_assert(sizeof(FileHeader) == 32, "FileHeader must be exactly 32 bytes");

	// Layout: [FileHeader][microcode bytes][vumem bytes][CapturedState bytes].
	// Returns false on I/O failure; the file is left in whatever state fwrite
	// produced (callers are expected to write to a temp path then rename if
	// atomicity matters).
	bool WriteToFile(const std::string& path, const CaptureRecord& rec);

	// Reads a record. Returns false on I/O failure, magic mismatch, version
	// mismatch, sane-size violation, or short read. On success, rec_out is
	// fully populated; on failure, contents are unspecified.
	bool ReadFromFile(const std::string& path, CaptureRecord& rec_out);

	// Pulls the architectural-only fields out of a live VURegs into a
	// CapturedState. Called by the dispatcher probe; exposed for tests.
	void SnapshotState(const VURegs& regs, CapturedState& out);

	// Writes the captured fields back to a live VURegs. Preserves the live
	// Mem / Micro pointers and any pipeline-modeling fields not in the
	// captured set. Called by VuTestHarness::LoadFromFile.
	void RestoreState(const CapturedState& state, VURegs& regs);

	// Dispatcher capture probe. Called once per mVUexecute entry. First call
	// reads PCSX2_VU_CAPTURE_DIR / PCSX2_VU_CAPTURE_MAX / PCSX2_VU_RANK_OUT
	// env vars; if all three are unset the probe is permanently disabled
	// and all subsequent calls are a single relaxed-atomic load + branch.
	//
	// Capture mode (PCSX2_VU_CAPTURE_DIR set): reservoir sampling per
	// (vu_index, start_pc), keep up to PCSX2_VU_CAPTURE_MAX (default 32)
	// captures per program as <dir>/vu<idx>_pc<8hex>_seq<3d>.vucap.
	//
	// Rank mode (PCSX2_VU_RANK_OUT set): maintain in-memory execution
	// count per (vu_index, start_pc); on process exit (atexit handler),
	// write sorted top-N to PCSX2_VU_RANK_OUT. Use this to discover which
	// programs are hot before deciding what to capture in detail.
	//
	// Trajectory mode (PCSX2_VU_TRAJ_OUT set): append ONE text line per VU
	// dispatch (every call, no reservoir) to the named file:
	//   seq vu_index pc cycle_budget cpu_cycle vu_cycle state_hash vumem_hash
	// seq is a global monotonic counter over BOTH VU0 and VU1 so the line
	// order is the true dispatch order. state_hash/vumem_hash are FNV-1a over
	// the architectural register surface and the whole VU data memory. Because
	// both the JIT probe (mVUexecute) and the interp probe (InterpVU1::Execute)
	// funnel through here, two runs from the same save-state (e.g. VU1-JIT that
	// hangs vs VU1-interp that plays) produce directly line-diffable logs: the
	// first line whose (pc, state_hash) diverges fingerprints a carried-state
	// bug, while matching hashes with drifting cpu_cycle fingerprints a pure
	// timing/pacing wedge. The file is flushed per line so a force-killed hang
	// still leaves a complete trajectory.
	//
	// The modes can be active simultaneously.
	void MaybeCapture(int vu_index, u32 start_pc, u32 cycle_budget,
		const u8* microcode_ptr, u32 microcode_size,
		const u8* vumem_ptr, u32 vumem_size,
		const VURegs& regs);

} // namespace vu_capture

#endif // PCSX2_RECOMPILER_TESTS
