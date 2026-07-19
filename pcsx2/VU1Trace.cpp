// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "VU1Trace.h"

#ifdef PCSX2_RECOMPILER_TESTS

#include "Common.h"
#include "VUmicro.h"
#include "DebugTools/Debug.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdarg>

namespace vu1_trace {

Ring g_vu1_ring;

// Master gate. begin() is hoisted behind this at every call site so the
// disabled path is a single relaxed load + predicted-not-taken branch.
std::atomic<bool> g_enabled{false};

// Disabled by default — set fields and flip `enabled` from gdb when
// debugging a specific divergence, then run until the tripwire fires
// to capture the N dispatches leading up to the bad state.
Tripwire g_vu1_tripwire = {
    /*enabled*/ false,
    /*start_pc*/ 0xFFFFFFFFu,                    // any
    /*a_vf14_w*/ 0x00000000, /*a_vf15_w*/ 0x00000000,
    /*b_vf14_w*/ 0x00000000, /*b_vf15_w*/ 0x00000000,
};

static void check_tripwire(const Entry* e)
{
    if (!g_vu1_tripwire.enabled)
        return;
    if (g_vu1_ring.frozen.load(std::memory_order_relaxed))
        return;
    if (g_vu1_tripwire.start_pc != 0xFFFFFFFFu && e->start_pc != g_vu1_tripwire.start_pc)
        return;

    const u32 vf14w = e->vf_out[14 * 4 + 3];
    const u32 vf15w = e->vf_out[15 * 4 + 3];

    // Both pairs guarded by "set both to 0 to disable" so the default
    // all-zero tripwire isn't a footgun if someone toggles `enabled`
    // without filling values in.
    const bool match_a = (g_vu1_tripwire.a_vf14_w || g_vu1_tripwire.a_vf15_w) &&
                         (vf14w == g_vu1_tripwire.a_vf14_w && vf15w == g_vu1_tripwire.a_vf15_w);
    const bool match_b = (g_vu1_tripwire.b_vf14_w || g_vu1_tripwire.b_vf15_w) &&
                         (vf14w == g_vu1_tripwire.b_vf14_w && vf15w == g_vu1_tripwire.b_vf15_w);

    if (match_a || match_b)
    {
        g_vu1_ring.frozen.store(true, std::memory_order_relaxed);
        std::fprintf(stderr,
            "vu1_trace: TRIPWIRE FIRED at seq=%u start_pc=0x%04x mode=%c (vf14.w=%08x vf15.w=%08x) — ring frozen\n",
            e->seq, e->start_pc, e->mode ? e->mode : '?', vf14w, vf15w);
    }
}

void reset()
{
    for (u32 i = 0; i < kRingSize; i++)
        g_vu1_ring.entries[i].seq = 0;
    g_vu1_ring.next_seq.store(0, std::memory_order_relaxed);
    g_vu1_ring.frozen.store(false, std::memory_order_relaxed);
}

static void snapshot_entry_state(Entry* e)
{
    std::memcpy(e->vf_in, &VU1.VF[0], sizeof(e->vf_in));
    for (int i = 0; i < 16; i++)
        e->vi_in[i] = VU1.VI[i].UL;
    std::memcpy(e->acc_in, &VU1.ACC, sizeof(e->acc_in));
    e->q_in = VU1.VI[REG_Q].UL;
    e->mac_in = VU1.VI[REG_MAC_FLAG].UL;
    e->clip_in = VU1.VI[REG_CLIP_FLAG].UL;
    e->status_in = VU1.VI[REG_STATUS_FLAG].UL;
    e->cycles_at_entry = VU1.cycle;
    std::memcpy(e->mem_in, VU1.Mem, kDataMemCap);
}

Entry* begin(char mode, u32 start_pc, u32 cycles)
{
    if (g_vu1_ring.frozen.load(std::memory_order_relaxed))
        return nullptr;

    // Single ticket → slot index and seq both derive from it. seq=0 is
    // reserved for "empty slot", so seq = ticket + 1.
    const u32 ticket = g_vu1_ring.next_seq.fetch_add(1, std::memory_order_relaxed);
    const u32 idx = ticket % kRingSize;
    Entry* e = &g_vu1_ring.entries[idx];

    e->seq = ticket + 1;
    e->mode = mode;
    e->start_pc = start_pc;
    e->end_pc = 0;
    e->cycles_in = cycles;
    e->cycles_at_exit = 0;

    // Copy microprogram bytes from VU1.Micro starting at start_pc.
    const u32 pc_masked = start_pc & VU1_PROGMASK;
    const u32 avail = (pc_masked < VU1_PROGSIZE) ? (VU1_PROGSIZE - pc_masked) : 0;
    const u32 to_copy = avail < kProgramCap ? avail : kProgramCap;
    if (to_copy)
        std::memcpy(e->program, &VU1.Micro[pc_masked], to_copy);
    if (to_copy < kProgramCap)
        std::memset(e->program + to_copy, 0, kProgramCap - to_copy);
    e->program_size = to_copy;

    snapshot_entry_state(e);
    return e;
}

void finish(Entry* e)
{
    if (!e)
        return;
    std::memcpy(e->vf_out, &VU1.VF[0], sizeof(e->vf_out));
    for (int i = 0; i < 16; i++)
        e->vi_out[i] = VU1.VI[i].UL;
    std::memcpy(e->acc_out, &VU1.ACC, sizeof(e->acc_out));
    e->q_out = VU1.VI[REG_Q].UL;
    e->mac_out = VU1.VI[REG_MAC_FLAG].UL;
    e->clip_out = VU1.VI[REG_CLIP_FLAG].UL;
    e->status_out = VU1.VI[REG_STATUS_FLAG].UL;
    e->end_pc = VU1.VI[REG_TPC].UL;
    e->cycles_at_exit = VU1.cycle;
    std::memcpy(e->mem_out, VU1.Mem, kDataMemCap);
    check_tripwire(e);
}

static void fp(FILE* f, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(f, fmt, ap);
    va_end(ap);
}

void dump_vu1_entry(FILE* f, const Entry* e, bool with_disasm)
{
    if (!e || e->seq == 0)
    {
        fp(f, "(empty entry)\n");
        return;
    }

    fp(f, "=== seq=%u mode=%c start_pc=0x%04x end_pc=0x%04x cycles_in=%u cycles_used=%llu prog_bytes=%u ===\n",
        e->seq, e->mode ? e->mode : '?', e->start_pc, e->end_pc, e->cycles_in,
        (unsigned long long)(e->cycles_at_exit - e->cycles_at_entry), e->program_size);

    fp(f, "VF entry:\n");
    for (int i = 0; i < 32; i++)
    {
        const u32* v = &e->vf_in[i * 4];
        fp(f, "  VF%02d: %08x %08x %08x %08x\n", i, v[0], v[1], v[2], v[3]);
    }
    fp(f, "VI entry: ");
    for (int i = 0; i < 16; i++)
        fp(f, "VI%02d=%08x%s", i, e->vi_in[i], (i & 3) == 3 ? "\n          " : " ");
    fp(f, "\n");
    fp(f, "ACC entry: %08x %08x %08x %08x  Q=%08x MAC=%08x CLIP=%08x STATUS=%08x\n",
        e->acc_in[0], e->acc_in[1], e->acc_in[2], e->acc_in[3],
        e->q_in, e->mac_in, e->clip_in, e->status_in);

    fp(f, "MEM entry (first %u bytes / 16 qwords as x y z w):\n", kDataMemCap);
    {
        const u32* mem = reinterpret_cast<const u32*>(e->mem_in);
        for (u32 i = 0; i < kDataMemCap / 16; i++)
            fp(f, "  m%02u: %08x %08x %08x %08x\n", i,
                mem[i * 4 + 0], mem[i * 4 + 1], mem[i * 4 + 2], mem[i * 4 + 3]);
    }

    fp(f, "VF exit:\n");
    for (int i = 0; i < 32; i++)
    {
        const u32* v = &e->vf_out[i * 4];
        fp(f, "  VF%02d: %08x %08x %08x %08x\n", i, v[0], v[1], v[2], v[3]);
    }
    fp(f, "VI exit:  ");
    for (int i = 0; i < 16; i++)
        fp(f, "VI%02d=%08x%s", i, e->vi_out[i], (i & 3) == 3 ? "\n          " : " ");
    fp(f, "\n");
    fp(f, "ACC exit:  %08x %08x %08x %08x  Q=%08x MAC=%08x CLIP=%08x STATUS=%08x\n",
        e->acc_out[0], e->acc_out[1], e->acc_out[2], e->acc_out[3],
        e->q_out, e->mac_out, e->clip_out, e->status_out);

    fp(f, "MEM exit  (first %u bytes / 16 qwords; * = changed since entry):\n", kDataMemCap);
    {
        const u32* mem_in  = reinterpret_cast<const u32*>(e->mem_in);
        const u32* mem_out = reinterpret_cast<const u32*>(e->mem_out);
        for (u32 i = 0; i < kDataMemCap / 16; i++)
        {
            const bool changed =
                mem_in[i * 4 + 0] != mem_out[i * 4 + 0] ||
                mem_in[i * 4 + 1] != mem_out[i * 4 + 1] ||
                mem_in[i * 4 + 2] != mem_out[i * 4 + 2] ||
                mem_in[i * 4 + 3] != mem_out[i * 4 + 3];
            fp(f, " %cm%02u: %08x %08x %08x %08x\n", changed ? '*' : ' ', i,
                mem_out[i * 4 + 0], mem_out[i * 4 + 1], mem_out[i * 4 + 2], mem_out[i * 4 + 3]);
        }
    }

    if (with_disasm && e->program_size >= 8)
    {
        fp(f, "Program disasm:\n");
        const u32* words = reinterpret_cast<const u32*>(e->program);
        const u32 nwords = e->program_size / 4;
        bool saw_ebit = false;
        for (u32 i = 0; i + 1 < nwords; i += 2)
        {
            const u32 lower = words[i];
            const u32 upper = words[i + 1];
            const u32 pc = e->start_pc + i * 4;
            const bool has_i = (upper & (1u << 31)) != 0;
            const char ebit = (upper & (1u << 30)) ? 'E' : '-';
            const char mbit = (upper & (1u << 29)) ? 'M' : '-';
            const char dbit = (upper & (1u << 28)) ? 'D' : '-';
            const char tbit = (upper & (1u << 27)) ? 'T' : '-';
            const char ibit = has_i ? 'I' : '-';

            // disVU1MicroUF/LF use a static buffer; copy each result before
            // the next call clobbers it.
            char ubuf[256], lbuf[256];
            const char* u = disVU1MicroUF(upper, pc + 4);
            std::strncpy(ubuf, u ? u : "?", sizeof(ubuf) - 1);
            ubuf[sizeof(ubuf) - 1] = 0;

            // I-bit: lower word is a 32-bit float literal loaded into VI[REG_I],
            // not an opcode. Render the float instead of running the disasm.
            if (has_i)
            {
                float fval;
                std::memcpy(&fval, &lower, sizeof(fval));
                std::snprintf(lbuf, sizeof(lbuf), "I = %g (0x%08x)", (double)fval, lower);
            }
            else
            {
                const char* l = disVU1MicroLF(lower, pc);
                std::strncpy(lbuf, l ? l : "?", sizeof(lbuf) - 1);
                lbuf[sizeof(lbuf) - 1] = 0;
            }

            fp(f, "  %04x: %08x %08x [%c%c%c%c%c] U:%s L:%s\n",
                pc, lower, upper, ibit, ebit, mbit, dbit, tbit, ubuf, lbuf);

            // The E-bit fires a two-step countdown (VU0microInterp.cpp: ebit=2
            // on the E-bit op, terminates after the *next* pair executes), so
            // print one more pair after the E-bit op — otherwise the last
            // visible op isn't the last executed one and the trace misleads.
            if (saw_ebit)
                break;
            if (ebit == 'E')
                saw_ebit = true;
        }
    }
    fp(f, "\n");
}

void dump_vu1_trace(FILE* f, u32 last_n)
{
    if (last_n == 0 || last_n > kRingSize)
        last_n = kRingSize;

    // Rank by seq so concurrent writes (MTGS / MTVU racing the dump) can't
    // reorder the output. Iterate all slots, sort non-empty by seq desc,
    // print the top last_n.
    const u32 head = g_vu1_ring.next_seq.load(std::memory_order_relaxed);

    std::array<u32, kRingSize> idxs;
    u32 nfilled = 0;
    u32 nrec = 0, ninterp = 0;
    for (u32 i = 0; i < kRingSize; i++)
    {
        const Entry& e = g_vu1_ring.entries[i];
        if (e.seq == 0)
            continue;
        idxs[nfilled++] = i;
        if (e.mode == 'r') nrec++;
        else if (e.mode == 'i') ninterp++;
    }

    // Pick a filename if the caller didn't provide a FILE*.
    bool owns_file = false;
    if (!f)
    {
        const char* path;
        if (nrec > 0 && ninterp == 0)
            path = "/tmp/vu1_trace_jit.log";
        else if (ninterp > 0 && nrec == 0)
            path = "/tmp/vu1_trace_interp.log";
        else if (nrec >= ninterp)
            path = "/tmp/vu1_trace_jit.log";
        else
            path = "/tmp/vu1_trace_interp.log";

        f = std::fopen(path, "w");
        if (!f)
        {
            std::fprintf(stderr, "vu1_trace: failed to open %s\n", path);
            return;
        }
        std::fprintf(stderr, "vu1_trace: writing %u entries (rec=%u interp=%u) to %s\n",
            std::min(last_n, nfilled), nrec, ninterp, path);
        owns_file = true;
    }

    fp(f, "vu1_trace: head=%u last_n=%u rec=%u interp=%u frozen=%d\n",
        head, last_n, nrec, ninterp, g_vu1_ring.frozen.load(std::memory_order_relaxed) ? 1 : 0);

    std::sort(idxs.begin(), idxs.begin() + nfilled,
        [](u32 a, u32 b) {
            return g_vu1_ring.entries[a].seq > g_vu1_ring.entries[b].seq;
        });

    const u32 n = std::min(last_n, nfilled);
    for (u32 k = 0; k < n; k++)
        dump_vu1_entry(f, &g_vu1_ring.entries[idxs[k]], /*with_disasm=*/true);

    std::fflush(f);
    if (owns_file)
        std::fclose(f);
}

}  // namespace vu1_trace

// gdb-friendly no-arg wrapper. C linkage avoids name mangling and default-arg
// resolution issues with `call` from gdb.
extern "C" void dump_vu1_trace()
{
    vu1_trace::dump_vu1_trace(nullptr, 32);
}

extern "C" void vu1_trace_reset()
{
    vu1_trace::reset();
}

extern "C" void vu1_trace_enable()
{
    vu1_trace::g_enabled.store(true, std::memory_order_relaxed);
}

extern "C" void vu1_trace_disable()
{
    vu1_trace::g_enabled.store(false, std::memory_order_relaxed);
}

#endif // PCSX2_RECOMPILER_TESTS
