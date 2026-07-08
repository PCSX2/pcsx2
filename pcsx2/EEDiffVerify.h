// SPDX-License-Identifier: GPL-3.0+
//
// @@EEDIFF@@ THROWAWAY DIAGNOSTIC — EE recompiler-vs-interpreter differential verifier.
//
// Purpose: pin the exact ARM64 EE guest instruction whose recompiled result diverges
// from the C++ interpreter. Built to hunt the True Crime NYC (SLUS-21106) texture
// decompressor corruption (displaced paletted texture bytes) that GS-dump replay
// proved is an EE data-gen bug, not the renderer.
//
// This whole feature is gated behind g_ee_diff_verify (default false). When it is
// false there is ZERO overhead — no hooks are emitted into recompiled blocks, and the
// store-capture branch in vtlb_memWrite is a single never-taken bool test. Flip it on
// (Recompiler tab -> "EE Diff Verify (diag)") and the EE block cache is cleared so
// blocks recompile WITH the per-op verify hooks.
//
// How it works (see EEDiffVerify.cpp for detail):
//   * The recompiler, when compiling a straight-line op through the native generators
//     (recTranslateOp), emits around each op:  snapshot cpuRegs -> real op -> verify.
//   * eeDiffVerify() re-runs that ONE op on the C++ interpreter from the pre-op
//     snapshot, with interpreter stores CAPTURED (recorded, not applied — the rec
//     already wrote real memory), then compares the interpreter's post-state (regs +
//     each captured store vs the real memory the rec wrote) against the rec's post
//     state. First mismatch logs "@@EEDIFF@@ ... DIVERGE ...".
//
// Everything here is wrapped with // @@EEDIFF@@ so it is trivially greppable/revertable.

#pragma once

#include "common/Pcsx2Defs.h"

// Master enable. Set via the Recompiler-tab toggle -> NativeApp.setEeDiffVerify() ->
// eeDiffSetEnabled(). Read at recompile time (to decide whether to emit hooks) and by
// the vtlb store-capture fast-out. `volatile` because it is toggled from the UI thread
// while the EE thread reads it; the block-cache clear that the setter forces is the
// real synchronisation point.
extern volatile bool g_ee_diff_verify;

// Store-capture mode. Set to true ONLY for the brief window while eeDiffVerify re-runs
// the interpreter op; vtlb_memWrite* checks it and records {addr,size,value} into the
// capture log instead of touching real memory. Never true outside eeDiffVerify.
extern bool g_ee_diff_capture_stores;

// Record one interpreter store while g_ee_diff_capture_stores is set. Called from the
// vtlb_memWrite<T> / vtlb_memWrite128 hook. `bits` = 8/16/32/64/128. For 128-bit the
// value is passed as two u64 halves (lo, hi); narrower sizes pass the value in lo.
void eeDiffCaptureStore(u32 addr, u32 bits, u64 lo, u64 hi);

// Emitted-code entry points (called from recompiled blocks — plain C ABI, no args
// except pc/op which the rec bakes in as immediates).
extern "C" {
// Snapshot the whole guest register file (GPR[0..31], HI, LO, PC, sa) into g_diff_pre.
// Emitted BEFORE the real recompiled op runs.
void eeDiffSnapshotPre();
// Re-run `op` on the interpreter from g_diff_pre with stores captured, then compare
// against the rec's post-state (current cpuRegs + real memory). Emitted AFTER the op.
void eeDiffVerify(u32 pc, u32 op);
}

// UI/JNI setter: set the enable flag. Returns nothing; the caller (native-lib JNI) is
// responsible for also clearing the EE block cache so blocks recompile with/without the
// hooks. Kept separate so the header has no dependency on the recompiler internals.
void eeDiffSetEnabled(bool enabled);
bool eeDiffGetEnabled();
