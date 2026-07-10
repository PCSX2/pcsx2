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

	// Bump on any on-disk layout change. Writers always emit kVersion;
	// readers accept [kMinReadVersion, kVersion] (v1 files simply lack the
	// CapturedConfig block, which reads back zeroed / not-valid).
	//
	// v1: [FileHeader][microcode][vumem][CapturedState]
	// v2: [FileHeader][CapturedConfig][microcode][vumem][CapturedState]
	inline constexpr u32 kVersion = 2;
	inline constexpr u32 kMinReadVersion = 1;

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

	// Live-config snapshot recorded at capture time (v2+). The microVU JIT's
	// COMPILED SHAPE depends on config the old format never carried —
	// CHECK_XGKICKHACK changes every VU1 block's XGKICK/E-bit emission, the
	// clamp bools gate mVUclamp emission, and the VU FPCRs pick the rounding
	// the arithmetic runs under. Replaying under the harness's pinned config
	// therefore validated DIFFERENT code than the live game ran (this exact
	// blindness hid the Crash Twinsanity mVU_XGKICK_SYNC lost-write bug: the
	// GameDB forces XGKickHack=true for that game, every offline oracle
	// pinned it false). The snapshot captures the live effective config —
	// defaults + GameDB + user INI, as actually running — so offline replay
	// can compile the same shape the game did.
	//
	// The gamefixes bitmask is indexed by GamefixId; enum order is therefore
	// frozen for on-disk compat (pinned by VuCaptureFormat.GamefixBitOrderIsFrozen).
	#pragma pack(push, 1)
	struct CapturedConfig
	{
		u32 flags;         // kConfigValid when recorded from a live EmuConfig;
		                   // 0 in synthetic/test records — replayers must ignore
		                   // the rest of the block when the bit is clear.
		char serial[16];   // NUL-padded disc serial ("SLES-52568"); may be empty
		                   // (headless capture) even when the snapshot is valid.
		u32 disc_crc;      // VMManager::GetDiscCRC() at capture; 0 if unknown.
		u32 gamefixes;     // bit i = GamefixId(i) enabled.
		u32 speedhacks;    // kSpeedhack* bits. Provenance only: replayers re-pin
		                   // vuThread/vu1Instant/vuFlagHack (harness constraints).
		u32 vu_clamp;      // kClamp* bits (the six Cpu.Recompiler.vu{0,1}* bools).
		u32 vu0_fpcr;      // kFpcr* encoding (round mode + FTZ + DAZ). NOT the raw
		                   // FPControlRegister::bitmask — that representation is
		                   // arch-specific (MXCSR on x86, FPCR on aarch64) and
		                   // captures must replay cross-arch.
		u32 vu1_fpcr;
	};
	#pragma pack(pop)
	static_assert(sizeof(CapturedConfig) == 4 + 16 + 6 * 4,
		"CapturedConfig layout drifted — bump kVersion if intentional");

	inline constexpr u32 kConfigValid = 1u << 0;

	inline constexpr u32 kSpeedhackVuFlagHack = 1u << 0;
	inline constexpr u32 kSpeedhackVuThread = 1u << 1;
	inline constexpr u32 kSpeedhackVu1Instant = 1u << 2;

	inline constexpr u32 kClampVu0Overflow = 1u << 0;
	inline constexpr u32 kClampVu0ExtraOverflow = 1u << 1;
	inline constexpr u32 kClampVu0SignOverflow = 1u << 2;
	inline constexpr u32 kClampVu1Overflow = 1u << 4;
	inline constexpr u32 kClampVu1ExtraOverflow = 1u << 5;
	inline constexpr u32 kClampVu1SignOverflow = 1u << 6;

	// Portable FPCR encoding for CapturedConfig::vu{0,1}_fpcr.
	inline constexpr u32 kFpcrRoundMask = 3u; // FPRoundMode enum value
	inline constexpr u32 kFpcrFlushToZero = 1u << 2;
	inline constexpr u32 kFpcrDenormalsAreZero = 1u << 3;

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
		// Zeroed (flags == 0, not valid) for v1 files and default-constructed
		// records; populated by the live probe via SnapshotConfig.
		CapturedConfig config{};
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

	// Layout (v2): [FileHeader][CapturedConfig][microcode][vumem][CapturedState].
	// Returns false on I/O failure; the file is left in whatever state fwrite
	// produced (callers are expected to write to a temp path then rename if
	// atomicity matters).
	bool WriteToFile(const std::string& path, const CaptureRecord& rec);

	// Reads a record (accepts versions kMinReadVersion..kVersion; v1 files
	// yield a zeroed, not-valid config). Returns false on I/O failure, magic
	// mismatch, unsupported version, sane-size violation, or short read. On
	// success, rec_out is fully populated; on failure, contents are
	// unspecified.
	bool ReadFromFile(const std::string& path, CaptureRecord& rec_out);

	// Snapshots the live effective config (EmuConfig + VMManager disc
	// identity) into a CapturedConfig with kConfigValid set. Called by the
	// dispatcher probe; exposed for tests.
	void SnapshotConfig(CapturedConfig& out);

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
	//   seq vu_index pc cycle_budget cpu_cycle vu_cycle state_hash vumem_hash core_hash
	// core_hash covers only pure arithmetic/control state (VF[32] + integer
	// VI[0..15] + ACC), excluding the cycle-timed fields (Q/P, flag pipelines,
	// xgkick* incl. xgkicklastcycle) that state_hash includes — so a core_hash
	// divergence at matched cpu_cycle is a real arithmetic bug, not a
	// cycle-model (the -3) artifact.
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
