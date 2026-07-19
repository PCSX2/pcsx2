// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU1 microprogram trace ring buffer.
//
// Records each VU1 dispatch (rec or interp) with the full microprogram
// bytes, entry/exit register state, and cycle counts. Designed to be
// inspected from gdb during a stopped-frame debug session — pause the
// emulator while the frame of interest is rendering, then `call dump_vu1_trace()`
// to print the most recent dispatches with VU disassembly.

#pragma once

#ifdef PCSX2_RECOMPILER_TESTS

#include "common/Pcsx2Defs.h"
#include <atomic>
#include <cstdio>

namespace vu1_trace {

constexpr u32 kRingSize = 128;
constexpr u32 kProgramCap = 4096;  // bytes; covers most microprograms
constexpr u32 kDataMemCap = 256;   // bytes of VU1.Mem snapshotted (first 16 qwords)

struct Entry
{
    u32 seq;            // monotonic sequence number; 0 = empty slot
    char mode;          // 'r' = JIT rec, 'i' = interp, 0 = empty
    char _pad[3];
    u32 start_pc;       // input start PC (in bytes, post-shift)
    u32 end_pc;         // VI[REG_TPC] at exit (in bytes, post-shift)
    u32 cycles_in;      // input cycle budget
    u64 cycles_at_entry;
    u64 cycles_at_exit;
    u32 program_size;   // bytes copied into program[]
    u8  program[kProgramCap];

    // Entry state snapshot
    u32 vf_in[32 * 4];  // VFs as u32 quads
    u32 vi_in[16];
    u32 acc_in[4];
    u32 q_in;
    u32 mac_in;
    u32 clip_in;
    u32 status_in;
    u8  mem_in[kDataMemCap];   // first 256 bytes of VU1.Mem at entry

    // Exit state snapshot
    u32 vf_out[32 * 4];
    u32 vi_out[16];
    u32 acc_out[4];
    u32 q_out;
    u32 mac_out;
    u32 clip_out;
    u32 status_out;
    u8  mem_out[kDataMemCap];  // first 256 bytes of VU1.Mem at exit
};

struct Ring
{
    Entry entries[kRingSize];
    // Single ticket counter — slot index and seq are both derived from it,
    // so they can't drift apart under concurrent writes (MTGS / MTVU / EE).
    std::atomic<u32> next_seq;
    // Tripwire: once set, begin() returns nullptr so the ring stops
    // overwriting itself. Lets the dump capture the N dispatches leading
    // up to a known-bad state instead of the noise afterwards.
    std::atomic<bool> frozen;
};

extern Ring g_vu1_ring;

// Tripwire condition. When `enabled` is true, finish() freezes the ring
// after a dispatch whose start_pc + exit-VF state matches one of two
// (vf14.w, vf15.w) signature pairs — useful for capturing the N dispatches
// leading up to a divergence between two runs (e.g. JIT-broken signature
// in pair A, interp-correct signature in pair B; same binary catches
// either). Disabled by default; set the fields and flip `enabled` from
// gdb when debugging.
struct Tripwire
{
    bool enabled;
    u32  start_pc;        // match start_pc; 0xFFFFFFFF = any
    // Pair A — set both to 0 to disable this pair.
    u32  a_vf14_w;
    u32  a_vf15_w;
    // Pair B — set both to 0 to disable this pair.
    u32  b_vf14_w;
    u32  b_vf15_w;
};
extern Tripwire g_vu1_tripwire;

// Master gate. Default false — recording is ~5 KB of memcpy per dispatch,
// not free. Flip from gdb (`call vu1_trace_enable()`) when actively
// debugging a VU1 divergence; leave off in normal play and perf captures.
extern std::atomic<bool> g_enabled;

// Open a new entry; snapshots VU1 state and copies program bytes.
// Returns nullptr if tracing should be skipped (ring frozen).
Entry* begin(char mode, u32 start_pc, u32 cycles);

// Snapshot exit state into entry, then check tripwire. Safe to call with nullptr.
void finish(Entry* e);

// Re-arm: zero all entries, reset seq, clear frozen flag.
void reset();

// gdb-callable. Print the last `last_n` entries from the ring.
// Pass f=nullptr to auto-write to /tmp/vu1_trace_jit.log or
// /tmp/vu1_trace_interp.log based on which mode dominates the ring.
// Pass stderr/stdout/your-own-FILE* to override.
void dump_vu1_trace(FILE* f = nullptr, u32 last_n = 32);

}  // namespace vu1_trace

// True no-arg entry point — C linkage so gdb's `call` works without
// arg-resolving the C++ default args. Equivalent to dump_vu1_trace(nullptr, 32).
extern "C" void dump_vu1_trace();

// gdb-callable: re-arm the tripwire (clears ring + frozen flag).
extern "C" void vu1_trace_reset();

// gdb-callable: flip the master gate. Default-off; call enable() before
// running into the frame of interest, then dump_vu1_trace() once paused.
extern "C" void vu1_trace_enable();
extern "C" void vu1_trace_disable();

namespace vu1_trace {

// Print one entry with optional inline VU disasm.
void dump_vu1_entry(FILE* f, const Entry* e, bool with_disasm);

}  // namespace vu1_trace

#endif // PCSX2_RECOMPILER_TESTS
