#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0+
#
# bucket_perf.py — turn a `perf report --stdio` dump into a PCSX2 subsystem
# bottleneck ranking. Part of the M2-first profiling rig (plan: Step 0 of the
# neither cherry-pick funnel). Pure stdlib; runs identically on M2 Asahi and SD865.
#
# Input is the ALREADY-DUMPED text report (the wrapper runs `perf report` once and
# redirects to a file — see tools/perf/profile_run.sh — because a raw 500-700 MB
# perf.data is expensive to traverse). This script never invokes perf and never
# holds the .data; it only parses text.
#
# Bucketing has two axes:
#   1. SUBSYSTEM — JIT blocks by their Perf::Group symbol prefix
#      (EE_/VU0_/VU1_/IOP_/VIF_, from common/Perf.cpp) + native code by symbol regex.
#   2. THREAD comm (CPU=EE thread, MTVU, GS, Audio) — a secondary breakdown so the
#      EE-vs-MTVU split is visible and bucket attribution can be sanity-checked
#      (e.g. VU1-JIT should land mostly on MTVU under async MTGS).
#
# Anything unmatched falls into the visible `unattributed` bucket — never silently
# dropped, so a large value flags a missing rule rather than a clean-looking lie.
#
# Usage:
#   perf report -i perf.jit.data --stdio --percent-limit 0 -g none > report.txt
#   bucket_perf.py report.txt            # human table + @BUCKET@ grep lines
#   bucket_perf.py --json report.txt     # machine JSON (the wrapper medians these)
#   cat report.txt | bucket_perf.py      # stdin also works

import argparse
import json
import re
import sys

# --- Subsystem bucketing -----------------------------------------------------
# JIT symbol prefixes emitted by Perf::Group (common/Perf.cpp). Checked first.
JIT_PREFIX = re.compile(r"^(EE|VU0|VU1|IOP|VIF)_")
JIT_BUCKET = {"EE": "EE-JIT", "VU0": "VU0-JIT", "VU1": "VU1-JIT",
              "IOP": "IOP-JIT", "VIF": "VIF"}

# --- GPU-driver quarantine (checked BEFORE kernel) ---------------------------
# The host GPU stack (userspace Vulkan driver + DRM kernel + GPU memory manager).
# On the M2/Asahi box this dominates the GS thread (libvulkan_asahi + [asahi] Rust
# driver + drm_mm allocator: ~38% kernel + ~2% userspace at go/no-go #1). It is
# entirely HOST-SPECIFIC — it tells you nothing about SD865 (Adreno) CPU cost, so it
# gets its own visible bucket instead of polluting kernel/other and unattributed.
# Matches both the userspace driver dso AND the explicit DRM/GPU-MM kernel symbols;
# generic kernel routines (memset/mutex/malloc) stay in kernel/other on purpose
# (can't honestly attribute a generic memcpy to the GPU by symbol alone — the
# --renderer null run is the clean way to drop the whole GS/GPU path).
GPU_DRIVER_DSO = re.compile(r"asahi|libvulkan|libVkLayer|_mesa|libdrm|radeonsi|"
                            r"libGLX|libEGL|nvidia|anv_|\btu\b|panfrost|libmali")
GPU_DRIVER_SYM = re.compile(r"^drm_|drm_mm|^add_hole|^rm_hole|^hk_|_mesa_|^vk_|"
                            r"gpu_|RunVertex|RunFragment|FwCtlChannel|HeapAllocator")

# Native code, ordered — first match wins. Tuned against real demangled symbol names
# from R&C UYA captures (2026-06-24). Many hot natives are under the `isa_native::`
# MultiISA-dispatch namespace, so anchors match substrings, not leading symbols.
# Kept broad on purpose — re-tighten if a bucket starts swallowing unrelated symbols.
NATIVE_RULES = [
    # One-time costs (savestate/ISO decompress, shader compile, gamedb parse). Kept
    # separate so the steady-state emulation ranking isn't inflated by startup. Note:
    # over a bounded liverun this is a fixed cost — its share shrinks as FRAMES grows.
    ("startup/io",      re.compile(r"shaderc|glslang|spirv|SPIRV|libzstd|ZSTD|HUF_|"
                                   r"c4::yml|ryml|ParseEngine|GameDatabase|"
                                   r"Decompress|inflate|lzma|LZ4")),
    ("IPU/video",       re.compile(r"yuv2rgb|_DCT|IDCT|idct|IPU|ipu|getBits|[Mm]dec")),
    # VU dispatch envelope (program lookup/search, dispatcher entry/exit, sync-ahead,
    # block clear/compile) — distinct from the jitted VU bodies (VU0_/VU1_). This is
    # the "VU cache duplication / lookup" cost the stale RK3562 notes flagged.
    ("VU-glue",         re.compile(r"mVUlookupProg|mVUsearchProg|mVUcompile|mVUdispatch|"
                                   r"VU0StartFunc|VU1StartFunc|vu0SyncRunAhead|"
                                   r"vu1SyncRunAhead|recMicroVU|BaseVUmicroCPU|"
                                   r"mVUexecute|mVUreset|mVUcleanUp|microVU.*[Dd]ispatch|"
                                   r"vu0ExecMicro|vu1ExecMicro|vuExecMicro")),
    # VIF (unpack dynarec front-end + native transfer/interrupt). Jitted VIF_ blocks
    # land in the VIF bucket via JIT_PREFIX; these are the native halves.
    ("VIF",             re.compile(r"vifTransfer|VIF0transfer|VIF1transfer|vif0Interrupt|"
                                   r"vif1Interrupt|dVifUnpack|vifUnpack|VifUnpack|Vif_|"
                                   r"VIFunpack|vifCode|dVifsetVUptr|vifExecQueue|"
                                   r"_VIF[01]chain|VIF[01]chain")),
    ("GS",              re.compile(r"GSXXH|XXH_INLINE|XXH3|GSLocalMemory|GSRenderer|GSState|"
                                   r"GSDevice|GSDraw|GSRasteriz|GSVertex|GSTextureCache|"
                                   r"GSClut|GSGet|GSLookup|GSVector|GSBlock|GSClip|::GS|"
                                   r"GS[A-Z][a-z]|Gif_Unit|Gif_|GIFTag|GIFPath|GIFPackedReg")),
    ("SPU2/audio",      re.compile(r"[Ss][Pp][Uu]2|SndOut|[Ss]oundtouch|cubeb|TimeStretch|"
                                   r"ReverbDo|V_Volume|V_Core|VolumeSlide|V_ADSR|ADSR")),
    ("vtlb/mem",        re.compile(r"vtlb|[Mm]em[RW]rite|[Mm]em[Rr]ead|GetMemPtr|iopMem|"
                                   r"eeMem|recMemory|RecMemcheck|GoemonUnloadTlb")),
    ("EE/IOP-glue",     re.compile(r"cpuEventTest|iopEventTest|CPU_INT|recClear|"
                                   r"Arm64BaseBlocks|ExecuteBlock|"
                                   r"psxBranchTest|intcInterrupt|dmacInterrupt|hwIntc|"
                                   r"hwDmac|cpuException|psxException|eeloadHook|_cpuTest|"
                                   r"psxRcnt|psxCounter|rcntUpdate|EEcnt|hwRead|hwWrite|"
                                   r"dmaExec|dmacWrite|dmacRead|dmaGetAddr|eeHw|DMAVerbose|"
                                   r"_dmaGIF|_dmaVIF|sif[01]|EEsif")),
    ("dispatcher/glue", re.compile(r"Dispatcher|recExecute|recRecompile|iopRecRecompile|"
                                   r"JITCompile|recompileNextInstruction|recCall|dyna_|"
                                   r"sync_cache_range|__clear_cache|FlushInstructionCache")),
    ("memops",          re.compile(r"__memcpy|__memset|__memmove|__pi_mem|memcpy_|memset_|"
                                   r"memcpy@|memset@|memmove|crc32|"
                                   r"_int_malloc|_int_free|\bmalloc\b|\bfree\b|cfree|"
                                   r"malloc_consolidate|operator new|operator delete")),
    ("sync/mtgs/mtvu",  re.compile(r"pthread_mutex|pthread_cond|futex|__lll_|"
                                   r"condition_variable|Semaphore|WaitForBits|"
                                   r"std::.*mutex|Threading::|sem_post|sem_wait|"
                                   r"spin_on_owner|raw_spin|MTGS|MTVU|VU_Thread|"
                                   r"ThreadEntryPoint|Get_MTVUChanges|mtvu|GIFPath_|"
                                   r"ExecuteRingBuffer|ExecuteGSPacket")),
]

# Buckets we always print even at 0% (so the ranking shape is stable run-to-run).
ALL_BUCKETS = ["EE-JIT", "VU0-JIT", "VU1-JIT", "IOP-JIT", "VIF",
               "VU-glue", "GS", "IPU/video", "SPU2/audio", "vtlb/mem", "EE/IOP-glue",
               "dispatcher/glue", "memops", "sync/mtgs/mtvu", "startup/io",
               "JIT-other", "GPU-driver", "kernel/other", "unattributed"]

# A leading percent column, e.g. "    41.23%". `perf report -g none` emits one
# Overhead column; if a Children column sneaks in there are two — we take the LAST
# leading percent as self%.
PCT = re.compile(r"^\s*((?:\d+\.\d+%\s+)+)(.*)$")
# The symbol-type marker splits "comm dso" from "symbol": [.] user, [k] kernel, etc.
SYMMARK = re.compile(r"\s\[[.kguHh]\]\s")


def classify(dso, symbol, is_kernel):
    """Return the subsystem bucket for one report row. Order: JIT prefix → GPU-driver
    (host-specific, before kernel so the GPU stack's kernel symbols are quarantined) →
    generic kernel → native rules → unattributed (raw 0x addrs fall through)."""
    m = JIT_PREFIX.match(symbol)
    if m:
        return JIT_BUCKET[m.group(1)]
    # Unsymbolized JIT continuation blocks: perf inject names only the program-entry
    # block (per a41d849f4), so sub-blocks show up as `[JIT] tid N  0x...` raw addrs.
    # Label them JIT-other rather than letting them sink into unattributed — they ARE
    # guest JIT execution, just unnamed (which JIT engine is unknowable from the addr).
    if dso.startswith("[JIT]"):
        return "JIT-other"
    if GPU_DRIVER_DSO.search(dso) or GPU_DRIVER_SYM.search(symbol):
        return "GPU-driver"
    if is_kernel or dso == "[kernel.kallsyms]":
        return "kernel/other"
    for name, rx in NATIVE_RULES:
        if rx.search(symbol):
            return name
    return "unattributed"


def parse(lines):
    """Parse a perf report --stdio dump -> (bucket->pct, comm->pct, total_pct)."""
    buckets = {b: 0.0 for b in ALL_BUCKETS}
    comms = {}
    total = 0.0
    for line in lines:
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        m = PCT.match(line)
        if not m:
            continue
        # last leading percent = self%
        pct = float(m.group(1).split("%")[-2].split()[-1])
        rest = m.group(2)
        sm = SYMMARK.search(rest)
        if sm:
            left = rest[:sm.start()]
            symbol = rest[sm.end():].strip()
            mark = rest[sm.start():sm.end()].strip()
        else:
            # no marker (rare) — treat whole remainder as "comm ... symbol"
            left, symbol, mark = rest, rest.split()[-1] if rest.split() else "", ""
        # Columns are separated by 2+ spaces; comm itself may contain a single
        # space (e.g. "CPU Thread"), so split on runs of >=2 spaces, not any space.
        parts = re.split(r"\s{2,}", left.strip())
        comm = parts[0] if parts else "?"
        dso = parts[-1] if len(parts) > 1 else ""
        is_kernel = (mark == "[k]") or (dso == "[kernel.kallsyms]")
        bucket = classify(dso, symbol, is_kernel)
        buckets[bucket] += pct
        comms[comm] = comms.get(comm, 0.0) + pct
        total += pct
    return buckets, comms, total


def main():
    ap = argparse.ArgumentParser(description="Bucket a perf report into PCSX2 subsystems.")
    ap.add_argument("report", nargs="?", help="perf report --stdio dump (default stdin)")
    ap.add_argument("--json", action="store_true", help="emit JSON instead of a table")
    args = ap.parse_args()

    src = open(args.report) if args.report else sys.stdin
    with src:
        buckets, comms, total = parse(src)

    # Normalize to the parsed total so shares sum to 100% regardless of perf quirks
    # (e.g. a multi-PMU recording would otherwise sum to ~200%). Shares, not absolute
    # percentages, are the comparison currency (per the methodology rule).
    if total > 0:
        buckets = {k: v / total * 100.0 for k, v in buckets.items()}
        comms = {k: v / total * 100.0 for k, v in comms.items()}

    ranked = sorted(buckets.items(), key=lambda kv: kv[1], reverse=True)
    comm_ranked = sorted(comms.items(), key=lambda kv: kv[1], reverse=True)

    if args.json:
        json.dump({"total_pct": round(total, 2),
                   "buckets": {k: round(v, 3) for k, v in ranked},
                   "comms": {k: round(v, 3) for k, v in comm_ranked}},
                  sys.stdout, indent=2)
        sys.stdout.write("\n")
        return

    print(f"# subsystem ranking (self%, total accounted = {total:.1f}%)")
    print(f"{'BUCKET':<18}{'SELF%':>8}")
    for name, pct in ranked:
        print(f"{name:<18}{pct:>8.2f}")
    print()
    print(f"# by thread comm")
    for name, pct in comm_ranked:
        print(f"{name:<18}{pct:>8.2f}")
    print()
    # grep-friendly one-liners for the wrapper / quick scraping.
    for name, pct in ranked:
        print(f"@BUCKET@ {name} {pct:.2f}")
    if buckets["unattributed"] > 5.0:
        print(f"# WARNING: unattributed {buckets['unattributed']:.1f}% > 5% — "
              f"add/Tune a NATIVE_RULES regex.", file=sys.stderr)


if __name__ == "__main__":
    main()
