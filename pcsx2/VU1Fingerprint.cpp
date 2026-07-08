// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "PrecompiledHeader.h"

#include "VU1Fingerprint.h"
#include "Config.h"
#include "GS/GSXXH.h"
#include "VMManager.h"
#include "VUmicro.h"

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fmt/format.h>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace
{
    // VU1 micro-mem is 16 KiB = 2048 pairs. Per-pair (8-byte) cache.
    constexpr u32 kSlotCount = 2048;
    constexpr u32 kVU1MicroSize = 0x4000;

    // Dispatch-frequency dump tuning.
    //   - sample interval: check the wall clock every N dispatches (cheap)
    //   - dump interval:   emit a top-N block every M seconds
    //   - top N:           how many entries to print
    constexpr u32 kDispatchSampleInterval = 4096;
    constexpr int kDumpIntervalSec = 10;
    constexpr size_t kTopN = 10;

    // Per-slot cache. The dispatcher pays just a u64 read + 2-way compare
    // when the live VU1.Micro[pc] head hasn't changed since last visit.
    // dispatch_count_period accumulates per cache slot; the periodic dump
    // walks all slots, aggregates by hash, and resets.
    struct CacheEntry
    {
        u64 head_u64 = 0;
        u64 hash = 0;
        u32 extent_bytes = 0;
        u32 epoch = 0;
        const VU1Fingerprint::KernelEntry* result = nullptr;
        u64 dispatch_count_period = 0;
    };

    alignas(64) std::array<CacheEntry, kSlotCount> s_lookup_cache;
    u32 s_cache_epoch = 1;

    // Dispatch sample throttle + last-dump timestamp. Touched only from the
    // VU dispatcher thread (MTVU thread in MTVU mode, EE thread otherwise).
    u32 s_dispatches_since_check = 0;
    std::chrono::steady_clock::time_point s_last_dump =
        std::chrono::steady_clock::now();

    // Upload-side dedup. OnUpload runs on the same thread as the dispatcher
    // for any given mode (MTVU upload + dispatch both on VU thread; non-MTVU
    // both on EE thread). Mutex is paranoid — kept in case future code
    // changes split the upload path across threads.
    std::mutex s_dump_mutex;
    std::unordered_set<u64> s_dumped_upload_hashes;

    // Phase 1.7: empty kernel database. The Phase 1 telemetry infrastructure
    // remains live (HOT logs every 10s, binary dumps for top-3 programs to
    // <EmuFolders::Cache>/vu1_progs/) so the dispatcher cost stays a single
    // u64 read + 2-way compare on every block dispatch. Kernels go elsewhere
    // now — JIT-level NEON peephole batching (matrix*vec FMA cluster) applies
    // across all games rather than per-engine. See memory armsx2-vu1-fingerprint-phase3.
    constexpr std::array<VU1Fingerprint::KernelEntry, 0> g_kernels = {};


    // Hex-dump the first N bytes of a region into a flat string for logging.
    std::string HexPrefix(const u8* code, size_t bytes)
    {
        constexpr size_t kPrefixBytes = 64;
        const size_t n = std::min(bytes, kPrefixBytes);
        std::string out;
        out.reserve(n * 2);
        constexpr char kHex[] = "0123456789abcdef";
        for (size_t i = 0; i < n; ++i)
        {
            out.push_back(kHex[(code[i] >> 4) & 0xF]);
            out.push_back(kHex[code[i] & 0xF]);
        }
        return out;
    }

    // Walk VU1.Micro from `pc` looking for the first pair with the E-bit set
    // in the upper instruction. Returns byte length covering [pc, E-bit pair
    // + delay-slot pair]. When no E-bit is found within micro-mem, returns
    // the remaining-bytes-to-end-of-mem so the hash is at least bounded.
    //
    // E-bit detection mirrors PairHasEbit in iVU1micro_arm64.cpp:
    //   upper_word = *(u32*)(VU1.Micro + pc + 4); ebit = (upper >> 30) & 1.
    u32 WalkProgramExtent(u32 pc)
    {
        if (pc >= kVU1MicroSize)
            return 0;

        u32 cur = pc;
        while (cur + 8 <= kVU1MicroSize)
        {
            const u32 upper = *reinterpret_cast<const u32*>(VU1.Micro + cur + 4);
            cur += 8;
            if ((upper >> 30) & 1u)
            {
                // Include the delay-slot pair (one pair after the E-bit pair
                // is what AnalyzeBlock walks).
                if (cur + 8 <= kVU1MicroSize)
                    cur += 8;
                return cur - pc;
            }
        }
        return cur - pc;
    }

    const VU1Fingerprint::KernelEntry* LookupKernel(u64 hash, u32 size_bytes)
    {
        for (const VU1Fingerprint::KernelEntry& entry : g_kernels)
        {
            if (entry.hash == hash && entry.size_bytes == size_bytes)
                return &entry;
        }
        return nullptr;
    }

    // Full-bytecode dump dedup. Each hot hash (top-3 in any window) writes
    // its raw bytes to a one-per-hash file under EmuFolders::Cache/vu1_progs/
    // exactly once per process lifetime. Originally tried logcat (one
    // [VU1FP-CODE] line per 32-byte chunk) but a 2272B program is 73+ lines
    // and Android's per-process log rate-limit silently dropped the burst.
    // File output dodges that entirely and gives a binary disassembler can
    // consume directly.
    std::unordered_set<u64> s_dumped_code_hashes;

    void DumpProgramCode(u64 hash, u32 pc, u32 extent_bytes)
    {
        if (!s_dumped_code_hashes.insert(hash).second)
            return;

        const std::string dir = Path::Combine(EmuFolders::Cache, "vu1_progs");
        FileSystem::EnsureDirectoryExists(dir.c_str(), false);
        const std::string filename =
            fmt::format("vu1_{:016x}_pc{:04x}_{}b.bin", hash, pc, extent_bytes);
        const std::string filepath = Path::Combine(dir, filename);
        const bool ok = FileSystem::WriteBinaryFile(filepath.c_str(),
            VU1.Micro + pc, extent_bytes);

        // One-line log so frequency capture confirms the dump happened.
        Console.WriteLnFmt("[VU1FP-CODE] hash=0x{:016x} pc=0x{:04x} extent={}B file={} ({})",
            hash, pc, extent_bytes, filename, ok ? "ok" : "WRITE FAILED");
    }

    // Aggregate dispatch counts across all live slots, print the top-N, and
    // reset per-slot counters. Called from OnDispatch every ~10s.
    void DumpTopHotPrograms()
    {
        struct Hit
        {
            u64 hash;
            u32 extent_bytes;
            u32 pc;
            u64 count;
        };
        std::vector<Hit> hits;
        hits.reserve(64);

        for (u32 slot_idx = 0; slot_idx < kSlotCount; ++slot_idx)
        {
            CacheEntry& slot = s_lookup_cache[slot_idx];
            if (slot.epoch != s_cache_epoch || slot.dispatch_count_period == 0)
                continue;
            hits.push_back({slot.hash, slot.extent_bytes, slot_idx << 3, slot.dispatch_count_period});
            slot.dispatch_count_period = 0;
        }
        if (hits.empty())
            return;

        const size_t n = std::min(kTopN, hits.size());
        std::partial_sort(hits.begin(), hits.begin() + n, hits.end(),
            [](const Hit& a, const Hit& b) { return a.count > b.count; });

        u64 total = 0;
        for (const Hit& h : hits)
            total += h.count;

        Console.WriteLnFmt("[VU1FP-HOT] last {}s: {} dispatches across {} programs (showing top {})",
            kDumpIntervalSec, total, hits.size(), n);
        for (size_t i = 0; i < n; ++i)
        {
            const Hit& h = hits[i];
            const double pct = 100.0 * static_cast<double>(h.count) / static_cast<double>(total);
            Console.WriteLnFmt("[VU1FP-HOT]   #{} hash=0x{:016x} extent={}B dispatches={} ({:.1f}%)",
                i + 1, h.hash, h.extent_bytes, h.count, pct);
        }

        // Dump full bytecode for the top-3 programs (once per hash per
        // process lifetime). This is the disassembly input for Phase 2
        // kernel implementation — we need the actual VU instructions, not
        // just hash + size.
        const size_t code_n = std::min<size_t>(3, n);
        for (size_t i = 0; i < code_n; ++i)
        {
            const Hit& h = hits[i];
            if (h.extent_bytes > 0 && h.pc + h.extent_bytes <= kVU1MicroSize)
                DumpProgramCode(h.hash, h.pc, h.extent_bytes);
        }
    }

    // Throttled by sample interval so the wall-clock query is rare. After
    // every kDispatchSampleInterval dispatches we check; if kDumpIntervalSec
    // has elapsed we emit + reset.
    __fi void MaybeDumpHotPrograms()
    {
        if (++s_dispatches_since_check < kDispatchSampleInterval)
            return;
        s_dispatches_since_check = 0;

        const auto now = std::chrono::steady_clock::now();
        if (now - s_last_dump < std::chrono::seconds(kDumpIntervalSec))
            return;
        s_last_dump = now;

        DumpTopHotPrograms();
    }
} // anonymous namespace

namespace VU1Fingerprint
{

bool Enabled()
{
    static const bool s_enabled = [] {
        const char* value = std::getenv("ARMSX2_VU1_FINGERPRINT");
        return value && value[0] != '\0' && std::strcmp(value, "0") != 0;
    }();
    return s_enabled;
}

u64 ComputeHash(const u8* code, size_t bytes)
{
    return GSXXH3_64bits(code, bytes);
}

void OnUpload(u32 vu_idx, u32 addr, const u8* code, size_t bytes)
{
    if (!Enabled())
        return;

    // Invalidate the lookup cache unconditionally — the dispatcher will
    // re-hash on next visit to any affected PC. Epoch bump is cheap.
    s_cache_epoch++;

    if (bytes == 0)
        return;

    const u64 hash = ComputeHash(code, bytes);

    {
        std::lock_guard<std::mutex> lk(s_dump_mutex);
        if (!s_dumped_upload_hashes.insert(hash).second)
            return;
    }

    const std::string prefix = HexPrefix(code, bytes);
    const std::string serial = VMManager::GetDiscSerial();
    Console.WriteLnFmt("[VU{}FP] new program: hash=0x{:016x} bytes={} addr=0x{:04x} serial={} prefix={}",
        vu_idx, hash, bytes, addr, serial.empty() ? "?" : serial, prefix);
}

const KernelEntry* OnDispatch(u32 pc)
{
    if (!Enabled())
        return nullptr;

    if (pc >= kVU1MicroSize) [[unlikely]]
        return nullptr;

    const u32 slot_idx = pc >> 3;
    CacheEntry& cache = s_lookup_cache[slot_idx];
    const u64 live_head = *reinterpret_cast<const u64*>(VU1.Micro + pc);

    if (cache.epoch != s_cache_epoch || cache.head_u64 != live_head) [[unlikely]]
    {
        // Cache miss — walk the program extent, hash, and look up. This
        // path runs once per (pc, head, epoch) tuple; steady state is the
        // hot-path branch below.
        const u32 extent = WalkProgramExtent(pc);
        cache.epoch = s_cache_epoch;
        cache.head_u64 = live_head;
        cache.extent_bytes = extent;
        cache.hash = (extent > 0) ? ComputeHash(VU1.Micro + pc, extent) : 0;
        cache.result = (extent > 0) ? LookupKernel(cache.hash, extent) : nullptr;
        cache.dispatch_count_period = 0;
    }

    cache.dispatch_count_period++;
    MaybeDumpHotPrograms();
    return cache.result;
}

} // namespace VU1Fingerprint
