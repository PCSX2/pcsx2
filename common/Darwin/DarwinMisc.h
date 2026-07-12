// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#ifdef __APPLE__

#include <string>
#include <vector>

#include "common/Pcsx2Types.h"

namespace DarwinMisc {
    extern int iPSX2_CRASH_DIAG;
    extern int iPSX2_REC_DIAG;
    // [V34] Internal flag — no longer env-driven. Set to 1 only by ios_main.mm
    // JIT entitlement fallback (CS_DEBUGGED check fail). Consumed by Vif_Unpack /
    // VMManager dVifReset gates to disable newVifDynaRec when JIT unavailable.
    extern int iPSX2_FORCE_EE_INTERP;
    extern int iPSX2_FORCE_JIT_VERIFY;
    extern int iPSX2_CALL_TGT_X9;
    extern int iPSX2_CRASH_PACK;
    extern int iPSX2_WX_TRACE;
	extern int iPSX2_CALLPROBE;
	extern int iPSX2_JIT_HLE;        // [P11] JIT modeの HLE enabled/disabled (default=1=enabled, 0=disabled)

	// [ARMSX2 iOS] Compatibility Lab flags ported from unsigned29/iPSX2.
	// Names intentionally stay source-compatible with that patch series.
	extern int iPSX2_BISECT_COP1_EVERYTHING_ONLY;
	extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_LOADSTORE;
	extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_MMI;
	extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_COP2_VU;
	extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_MULTDIV;
	extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_SHIFTS;
	extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_MOVES;
	extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_INTEGER_ALU;
	extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_BRANCHES;

	// [iPSX2] Indirect Branch Probe
    // extern volatile u64 g_last_indirect_target; // Deprecated
    // extern volatile u64 g_last_indirect_site;   // Deprecated

    struct IndirectEvent {
        u64 site;
        u64 target;
        u32 insn;
        u32 kind; // 1=BLR, 2=BR, 3=RET
        u64 pad;  // Pad to 32 bytes (stride used by ASM)
    };
    extern volatile IndirectEvent g_ie[8];
    extern volatile u32 g_ie_idx;

    // [iPSX2] W^X Trace Event
    struct WXTraceEvent {
        u64 tid;
        u64 caller;
        int write; // 0=RX, 1=RW
        int depth;
    };
    extern volatile WXTraceEvent g_wx_events[16];
    extern volatile u32 g_wx_idx;

    // [iPSX2] JIT Call Emit Probe
    struct EmitEvent {
        u64 pc;      // Guest PC or nearby tag
        u64 ptr;     // Target address
        u64 sym;     // Symbol address if resolved (or 0)
        u64 tid;     // Thread ID
        u64 caller;  // Caller of armEmitCall
    };
    extern volatile EmitEvent g_emit_events[32];
    extern volatile u32 g_emit_idx;

    // [iPSX2] W^X State Tracker (0=RX, 1=RW)
    extern volatile int g_jit_write_state;
    // [iPSX2] Recompiler Stage Tracker
    extern volatile int g_rec_stage;

    void SetCrashLogFD(int fd);

struct CPUClass {
	std::string name;
	u32 num_physical;
	u32 num_logical;
};


	// JIT Context for Signal Handler
	void SetJitRange(void* base, size_t size);
	void SetLastGuestPC(u32 pc);
	void SetLastRecPtr(void* ptr);

    // JIT Context Getters
    uintptr_t GetJitBase();
    uintptr_t GetJitEnd();
    u32 GetLastGuestPC();
    uintptr_t GetLastRecPtr();

	std::vector<CPUClass> GetCPUClasses();

    // [iPSX2] DYLD Main Base Getter
    void LogDyldMain();

    // [iPSX2] JIT Block Mapping Service
    void RecordJitBlock(u32 guest_pc, void* recptr, u32 size);
    bool FindJitBlock(uintptr_t site, u32* out_guest_pc, void** out_recptr);

    // [P42] JIT availability detection for real iOS devices
    bool IsJITAvailable();

    /// Re-checks whether JIT is still usable after initial acquisition.
    /// Combines a CS_DEBUGGED re-probe with a canary write/read to the RW alias.
    /// Returns false if iOS has revoked the JIT grant since boot.
    bool ValidateJITAlive();

    // [P43] iOS 26 Dual-Mapping JIT
    enum class JitMode {
        Simulator,    // MAP_JIT + pthread_jit_write_protect_np
        LuckTXM,      // brk #0x69 + vm_remap dual-mapping (iOS 26, A15+)
        LuckNoTXM,    // vm_remap dual-mapping (iOS 26, non-TXM)
        Legacy,        // mprotect toggle (iOS 18 and earlier)
    };

    JitMode DetectJitMode();
    JitMode GetJitMode();

    // RW offset for dual-mapping: write to (rx_ptr + offset), execute at rx_ptr
    // Simulator: 0 (same address for both)
    // Real device dual-mapping: rw_base - rx_base
    extern ptrdiff_t g_code_rw_offset;
    extern uintptr_t g_code_rw_base; // RW region start (0 if no dual-mapping)
    extern size_t    g_code_rw_size; // RW region size

    // Allocate executable memory with dual-mapping support
    // Returns RX pointer. For writing, use (rx_ptr + g_code_rw_offset).
    void* MmapCodeDualMap(size_t size);

    // Free dual-mapped code memory
    void MunmapCodeDualMap(void* rx_ptr, size_t size);

    // [P49] Legacy lazy toggle: flip RW→RX + icache flush before JIT dispatch.
    // Call this once before entering JIT code (e.g., in recExecute dispatcher).
    // No-op on non-Legacy modes.
    void LegacyEnsureExecutable();

}

#endif
