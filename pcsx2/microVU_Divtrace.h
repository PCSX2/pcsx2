// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// Per-microVU-op state-snapshot diagnostic.
//
// When mvu_divtrace::g_enabled is true, both the arm64 microVU JIT and
// the interpreter snapshot vuRegs[g_vu_index] after every microVU
// instruction. A VU capture replay driver runs the same capture through
// both paths and reports the first divergent op, with full context
// (microcode decode, allocator state at compile time, host-code
// disassembly, surrounding-op window).
//
// Consumers:
//   pcsx2/VU{0,1}microInterp.cpp           — interp snapshot site (after Step)
//   pcsx2/arm64/microVU_Compile-arm64.inl  — JIT compile-time emit (flushAll + Brk)
//   pcsx2/arm64/microVU_IR-arm64.h         — allocator-state snapshot helper
//
// Snapshots are populated in two paths:
//   - Interp:  direct memcpy after each Step()
//   - JIT:     emitted code does flushAll() then `brk #op_idx`; the replay
//              driver's SIGTRAP handler decodes the brk imm and memcpys vuRegs.

#include "common/Pcsx2Defs.h"
#include "VU.h"

#include <array>
#include <atomic>
#include <vector>

namespace mvu_divtrace
{
	// Sized to match microVU_IR-arm64.h: neonAllocTotal=28, gprAllocCount=32.
	// Kept here as plain ints rather than including the arm64 header, so the
	// interp + non-arm64 builds compile without dragging in vixl.
	constexpr int kNeonSlots = 28;
	constexpr int kGprSlots  = 32;

	struct AllocSnapshot
	{
		struct NeonSlot
		{
			int  vfreg;     // -1=temp/free, 0=VF0, 1-31=VF, 32=ACC, 33=I
			int  xyzw;      // 0=clean, 0xF=fully dirty, partial=other
			int  count;     // LRU
			bool isNeeded;
			bool isZero;
		};
		struct GprSlot
		{
			int  vireg;     // -1=unused, 0-15=VI
			int  count;
			bool isNeeded;
			bool dirty;
			bool isZeroExtended;
			bool usable;
		};
		std::array<NeonSlot, kNeonSlots> neon{};
		std::array<GprSlot,  kGprSlots>  gpr{};
	};

	// Per-op metadata recorded at JIT compile time.
	struct OpMeta
	{
		u16             op_idx;     // matches brk imm16
		u32             microvu_pc; // microVU PC (byte offset; what xPC macro yields)
		u32             opcode;     // mVU.code raw 32-bit value
		const u8*       host_lo;   // first host byte emitted for this op
		const u8*       host_hi;   // first host byte after the brk
		AllocSnapshot   alloc;     // allocator state immediately before flushAll+brk
	};

	// Per-op state snapshot — VURegs payload + bookkeeping. JIT and interp
	// each append one entry per op-execution, so a loop body executed K
	// times produces K consecutive entries (not one overwritten K times).
	// Compare jit_snaps[i] vs interp_snaps[i] for op-aligned divergence.
	struct StateSnap
	{
		VURegs regs;
		u16    meta_idx;     // JIT side: index into g_meta (= brk imm16). Interp: 0xFFFF.
		u32    pre_xPC;      // PC of the just-executed op (for xPC alignment cross-check).
	};

	// Globals. Definitions in microVU_Divtrace.cpp.
	//
	// Set by the replay driver before invoking the JIT or interp:
	//   1. Reset()
	//   2. g_vu_index = capture's vu_index
	//   3. g_enabled = true
	//   4. invoke JIT (populates g_meta + g_jit_snaps)
	//   5. restore pre-state, run interp (populates g_interp_snaps)
	//   6. g_enabled = false
	//   7. compare g_jit_snaps[i] vs g_interp_snaps[i]
	extern std::atomic<bool>          g_enabled;
	extern int                        g_vu_index;
	extern std::vector<OpMeta>        g_meta;
	extern std::vector<StateSnap>     g_jit_snaps;     // windowed full snaps; idx-g_full_lo
	extern std::vector<StateSnap>     g_interp_snaps;  // windowed full snaps; idx-g_full_lo
	extern std::atomic<u32>           g_jit_snap_idx;  // bumped by SIGTRAP handler
	extern u32                        g_interp_op_idx; // bumped by interp Step loop

	// Compact per-op fingerprint streams (one u64 hash + one xPC per executed
	// op), keyed by the same execution counter as the full snaps. These scale
	// to millions of ops (~12 B/op) where the 3 KB/op full StateSnap stream
	// overflows at kSnapCapacity. The fingerprint hashes the architecturally
	// meaningful state — VF (4 lanes raw), ACC (4 lanes raw), and VI[i] masked
	// (16-bit unless full-width) for i outside the ignored set {16,17,18,22,23,26}.
	// Those ignored VI registers are the pipeline-state slots (flag/Q/P/TPC) that
	// carry timing-dependent noise, so masking them means a fingerprint mismatch
	// is a genuine architectural divergence rather than pipeline noise.
	extern std::vector<u64>           g_jit_fps;
	extern std::vector<u64>           g_interp_fps;
	extern std::vector<u32>           g_jit_xpc;       // pre_xPC per executed op
	extern std::vector<u32>           g_interp_xpc;

	// Full-StateSnap recording window [g_full_lo, g_full_lo+len). Writers
	// store a full StateSnap into g_*_snaps[idx - g_full_lo] only when the
	// execution index idx falls in the window; fingerprints are always
	// recorded. Pass 1 sets a zero-length window (fingerprints only); pass 2
	// sets a small window around the first divergence for the detailed report.
	extern u32                        g_full_lo;
	extern u32                        g_full_hi;       // == g_full_lo + g_jit_snaps.size()

	// Fingerprint one VURegs over its architecturally meaningful state (see the
	// fingerprint-stream note above for the masked/ignored VI set).
	u64 FingerprintRegs(const VURegs& r);

	// Set the full-snapshot window: g_full_lo=lo, snap buffers sized to `len`
	// (cleared). Call between passes. Does not touch fingerprint streams.
	void ConfigureFullWindow(u32 lo, u32 len);

	// Reset per-replay counters (and zero previously-written full snaps).
	// Fingerprint streams are overwritten by index, so no explicit clear.
	void Reset();

	// Install/remove the SIGTRAP handler that snapshots vuRegs[g_vu_index]
	// on each JIT-emitted brk and skips it. EnterMode also sets g_enabled
	// and g_vu_index; ExitMode clears g_enabled and restores the prior
	// SIGTRAP disposition. Safe to call multiple times.
	//
	// The handler decodes brk #imm16 from the trapping instruction, treats
	// imm16 as the op index, and writes vuRegs[g_vu_index] into
	// g_jit_snaps[op_idx]. Out-of-range op indices are reported and the
	// process aborts (this would indicate a brk emit/handler mismatch).
	void EnterMode(int vu_index);
	void ExitMode();
} // namespace mvu_divtrace
