// QAProbe — implementation. See QAProbe.h for design.

#include "QAProbe.h"
#include "common/Console.h"
#include "MTGS.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

extern uint32_t getVif1CmdUnpack();   // Vif_Transfer.cpp

namespace
{
    // -------- env-driven config (parsed once, on first vsync) --------
    // All defaults OFF: release ship 構成では QA probe を全 dormant 化、
    // env で明示的に opt-in した場合のみ counter / log / SS が動作する。
    bool         s_inited            = false;
    std::set<u32> s_ss_vs;
    std::string  s_tag               = "run";
    bool         s_gif_dump          = false;
    u64          s_gif_dump_quota    = 2000000;
    bool         s_qa_log            = false;

    // -------- cumulative counters --------
    std::atomic<u64> s_cum_gif_pkt{0};
    std::atomic<u64> s_cum_d_prim{0};
    std::atomic<u64> s_cum_d_unp{0};
    std::atomic<u64> s_cum_d_bytes{0};
    std::atomic<u64> s_gif_dump_emitted{0};

    // per-vsync deltas (reset each VSyncEnd)
    std::atomic<u64> s_vs_gif_pkt{0};
    std::atomic<u64> s_vs_d_prim{0};
    std::atomic<u64> s_vs_d_unp{0};
    std::atomic<u64> s_vs_d_bytes{0};

    void parse_csv_u32(const char* env, std::set<u32>& out)
    {
        if (!env || !*env) return;
        const char* p = env;
        while (*p)
        {
            char* end = nullptr;
            unsigned long v = std::strtoul(p, &end, 10);
            if (end == p) break;
            out.insert(static_cast<u32>(v));
            p = end;
            while (*p == ',' || *p == ' ') ++p;
        }
    }

    void init_once()
    {
        if (s_inited) return;
        s_inited = true;

        if (const char* e = std::getenv("iPSX2_QA_TAG"); e && *e) s_tag = e;
        if (const char* e = std::getenv("iPSX2_SS_AT_VS"); e && *e) parse_csv_u32(e, s_ss_vs);
        if (const char* e = std::getenv("iPSX2_GIF_DUMP"); e && *e == '1') s_gif_dump = true;
        if (const char* e = std::getenv("iPSX2_GIF_DUMP_QUOTA"); e && *e)
            s_gif_dump_quota = std::strtoull(e, nullptr, 10);
        if (const char* e = std::getenv("iPSX2_QA_LOG"); e && *e == '1') s_qa_log = true;

        // Init log only emitted when any QA feature is active (= one-time, never spams).
        if (s_qa_log || s_gif_dump || !s_ss_vs.empty())
        {
            Console.WriteLn("@@QA_INIT@@ tag=%s ss_vs_count=%zu gif_dump=%d quota=%llu qa_log=%d",
                s_tag.c_str(), s_ss_vs.size(), (int)s_gif_dump,
                (unsigned long long)s_gif_dump_quota, (int)s_qa_log);
        }
    }

    // FNV1a-32 over packet bytes (fast, deterministic)
    u32 fnv1a32(const u8* p, u32 n)
    {
        u32 h = 0x811c9dc5u;
        for (u32 i = 0; i < n; ++i) { h ^= p[i]; h *= 0x01000193u; }
        return h;
    }

    // Best-effort RGBA dump: 8-byte header [w,h u32 LE] + W*H*4 bytes.
    void dump_ss(u32 vs)
    {
        u32 w = 0, h = 0;
        std::vector<u32> pixels;
        if (!MTGS::SaveMemorySnapshot(640, 480, true, false, &w, &h, &pixels))
        {
            Console.WriteLn("@@QA_SS@@ vs=%u snapshot FAILED", vs);
            return;
        }
        char path[256];
        std::snprintf(path, sizeof(path), "/tmp/qa_ss_%s_vs%u.rgba", s_tag.c_str(), vs);
        FILE* f = std::fopen(path, "wb");
        if (!f) { Console.WriteLn("@@QA_SS@@ vs=%u fopen failed: %s", vs, path); return; }
        u32 hdr[2] = {w, h};
        std::fwrite(hdr, sizeof(u32), 2, f);
        std::fwrite(pixels.data(), 4, (size_t)w * h, f);
        std::fclose(f);
        Console.WriteLn("@@QA_SS@@ vs=%u path=%s size=%ux%u", vs, path, w, h);
    }
}

namespace QAProbe
{
    void on_vsync_end(u32 frame_count)
    {
        init_once();

        // Fast path: when QA log + SS dump 両方 OFF なら全 skip。
        // release ship default ではこの path で常時 early return (= per-vsync ゼロ cost)。
        if (!s_qa_log && s_ss_vs.empty()) return;

        const u32 vs = frame_count + 1; // VSyncEnd increments g_FrameCount; align to "vs about to start"

        if (s_qa_log)
        {
            const u64 cum_pkt   = s_cum_gif_pkt.load(std::memory_order_relaxed);
            const u64 cum_prim  = s_cum_d_prim.load(std::memory_order_relaxed);
            const u64 d_pkt     = s_vs_gif_pkt.exchange(0, std::memory_order_relaxed);
            const u64 d_prim    = s_vs_d_prim.exchange(0, std::memory_order_relaxed);
            const u64 d_unp     = s_vs_d_unp.exchange(0, std::memory_order_relaxed);
            const u64 d_bytes   = s_vs_d_bytes.exchange(0, std::memory_order_relaxed);
            const u32 vif1_unp  = getVif1CmdUnpack();
            (void)cum_prim;

            Console.WriteLn(
                "@@BL_FRAME@@ vs=%u gif_pkt=%llu d_pkt=%llu d_prim=%llu d_unp=%llu d_bytes=%llu vif1_unpack=%u",
                vs,
                (unsigned long long)cum_pkt,
                (unsigned long long)d_pkt,
                (unsigned long long)d_prim,
                (unsigned long long)d_unp,
                (unsigned long long)d_bytes,
                vif1_unp);
        }

        if (!s_ss_vs.empty() && s_ss_vs.count(vs)) dump_ss(vs);
    }

    void on_gif_transfer(u32 tran_type, u32 path, const u8* pMem, u32 size)
    {
        // Fast path: env で QA log も GIF dump も off の場合は全 skip。
        // release ship default ではこの path で常時 early return (= per-transfer ゼロ cost)。
        if (!s_qa_log && !s_gif_dump) return;

        if (s_qa_log)
        {
            // Cumulative + per-vsync counters (only when @@BL_FRAME@@ log consumer exists)
            s_cum_gif_pkt.fetch_add(1, std::memory_order_relaxed);
            s_vs_gif_pkt.fetch_add(1, std::memory_order_relaxed);
            s_cum_d_bytes.fetch_add(size, std::memory_order_relaxed);
            s_vs_d_bytes.fetch_add(size, std::memory_order_relaxed);
            s_cum_d_prim.fetch_add(1, std::memory_order_relaxed);
            s_vs_d_prim.fetch_add(1, std::memory_order_relaxed);
            if (path == 1)
            {
                s_cum_d_unp.fetch_add(size, std::memory_order_relaxed);
                s_vs_d_unp.fetch_add(size, std::memory_order_relaxed);
            }
        }

        // Optional verbose GIF packet dump (env-gated).
        if (!s_gif_dump || !pMem || !size) return;
        const u64 emitted = s_gif_dump_emitted.fetch_add(1, std::memory_order_relaxed);
        if (emitted >= s_gif_dump_quota) return;
        const u32 crc = fnv1a32(pMem, size);
        Console.WriteLn("@@GIF_PKT@@ seq=%llu path=%u tran=%u size=%u crc=%08x",
            (unsigned long long)emitted, path, tran_type, size, crc);
    }
}
