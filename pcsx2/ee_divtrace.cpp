// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ee_divtrace.h"

#include "Memory.h"
#include "MemoryTypes.h"
#include "VUmicro.h"

#include <cstring>

// Self-contained xxh3 (same guard pattern as GS/GSXXH.h) for the memory hash.
#ifndef XXH_versionNumber
	#define XXH_STATIC_LINKING_ONLY 1
	#define XXH_INLINE_ALL 1
	#include <xxhash.h>
#endif

namespace ee_divtrace
{
	std::atomic<bool>       g_enabled{false};
	bool                    g_emit_block_hook = false;
	bool                    g_fp_exclude = false;
	bool                    g_vu0_include = false;
	bool                    g_skip_flushcache_syscall = false;
	std::vector<Sample>     g_stream;
	std::vector<FullSnap>   g_snaps;
	u32                     g_full_lo = 0;
	u32                     g_full_hi = 0;

	// FNV-1a style mix, matching microVU_Divtrace so the two diagnostics read
	// the same way.
	static inline u64 hash_mix(u64 h, u32 v)
	{
		h ^= static_cast<u64>(v);
		h *= 1099511628211ull;
		return h;
	}
	static inline u64 hash_mix64(u64 h, u64 v)
	{
		h = hash_mix(h, static_cast<u32>(v));
		h = hash_mix(h, static_cast<u32>(v >> 32));
		return h;
	}

	u64 FingerprintCpu()
	{
		u64 h = 1469598103934665603ull;
		// GPRs — full 128 bits each (.UD[0]/.UD[1]).
		for (int i = 0; i < 32; ++i)
		{
			// Skip k0/k1 (26/27): the EE kernel exception handler loads EPC/Cause
			// into these reserved scratch registers, so they inherit the same
			// cycle-phase interrupt noise as EPC and linger into user code until
			// overwritten; they reconverge by frame end. Excluding them from the
			// alignment fingerprint lets the zoom walk through every interrupt
			// handler. (The frame-level full-snapshot comparison still reports them.)
			if (i == 26 || i == 27)
				continue;
			h = hash_mix64(h, cpuRegs.GPR.r[i].UD[0]);
			h = hash_mix64(h, cpuRegs.GPR.r[i].UD[1]);
		}
		h = hash_mix64(h, cpuRegs.HI.UD[0]);
		h = hash_mix64(h, cpuRegs.HI.UD[1]);
		h = hash_mix64(h, cpuRegs.LO.UD[0]);
		h = hash_mix64(h, cpuRegs.LO.UD[1]);
		// CP0 — skip the dispatcher-driven counters:
		//   1 Random, 9 Count, 11 Compare, plus 13 Cause + 14 EPC (interrupt
		//   TIMING: EPC/Cause IP bits are a pure function of cpuRegs.cycle phase,
		//   so JIT/interp take the same interrupt a few cycles apart in an idle
		//   poll loop; excluding them keeps the streams aligned across every
		//   interrupt instead of breaking at each one).
		for (int i = 0; i < 32; ++i)
		{
			if (i == 1 || i == 9 || i == 11 || i == 13 || i == 14)
				continue;
			h = hash_mix(h, cpuRegs.CP0.r[i]);
		}
		// FPU register file + accumulator (control regs are excluded). Omitted
		// when g_fp_exclude is set, so the alignment walk skips past FP-register
		// divergences to hunt a non-FP (integer/control) divergence.
		if (!g_fp_exclude)
		{
			for (int i = 0; i < 32; ++i)
				h = hash_mix(h, fpuRegs.fpr[i].UL);
			h = hash_mix(h, fpuRegs.ACC.UL);
		}
		// VU0 macro-visible state — opt-in (EERUNNER_VU0FP): catches COP2 macro
		// divergence at the producing block instead of downstream in GPRs/memory.
		if (g_vu0_include)
		{
			for (int i = 0; i < 32; ++i)
			{
				h = hash_mix64(h, VU0.VF[i].UD[0]);
				h = hash_mix64(h, VU0.VF[i].UD[1]);
			}
			h = hash_mix64(h, VU0.ACC.UD[0]);
			h = hash_mix64(h, VU0.ACC.UD[1]);
			h = hash_mix(h, VU0.VI[REG_STATUS_FLAG].UL);
			h = hash_mix(h, VU0.VI[REG_MAC_FLAG].UL);
			h = hash_mix(h, VU0.VI[REG_CLIP_FLAG].UL);
		}
		h = hash_mix(h, cpuRegs.pc);
		h = hash_mix(h, cpuRegs.sa);
		return h;
	}

	void RecordSample(u32 pc)
	{
		const size_t idx = g_stream.size();
		Sample s;
		s.cycle = cpuRegs.cycle;
		s.fp    = FingerprintCpu();
		s.pc    = pc;
		s._pad  = 0;
		g_stream.push_back(s);

		if (idx >= g_full_lo && idx < g_full_hi)
		{
			FullSnap& fs = g_snaps[idx - g_full_lo];
			std::memcpy(&fs.cpu, &cpuRegs, sizeof(cpuRegisters));
			std::memcpy(&fs.fpu, &fpuRegs, sizeof(fpuRegisters));
			std::memcpy(fs.vu0_vf, VU0.VF, 32 * 16);
			std::memcpy(fs.vu0_vf + 32 * 16, &VU0.ACC, 16);
			fs.vu0_vi_status = VU0.VI[REG_STATUS_FLAG].UL;
			fs.vu0_vi_mac    = VU0.VI[REG_MAC_FLAG].UL;
			fs.vu0_vi_clip   = VU0.VI[REG_CLIP_FLAG].UL;
			fs._pad0 = 0;
			fs.cycle = cpuRegs.cycle;
			fs.pc    = pc;
			fs._pad  = 0;
		}
	}

	u64 HashMemory()
	{
		if (!eeMem)
			return 0;
		// Real PS2 main RAM is 32 MB (TotalRam is the 128 MB address-wrap span;
		// only the first MainRam bytes are physical). Scratchpad is 16 KB.
		u64 h = XXH3_64bits(eeMem->Main, Ps2MemSize::MainRam);
		h = XXH3_64bits_withSeed(eeMem->Scratch, Ps2MemSize::Scratch, h);
		return h;
	}

	void Reset()
	{
		g_stream.clear();
		if (!g_snaps.empty())
			g_snaps.assign(g_snaps.size(), FullSnap{});
	}

	void ConfigureFullWindow(u32 lo, u32 len)
	{
		g_full_lo = lo;
		g_full_hi = lo + len;
		g_snaps.assign(len, FullSnap{});
	}

	void ReserveStream(size_t samples)
	{
		g_stream.reserve(samples);
	}
} // namespace ee_divtrace

extern "C" void ee_divtrace_jit_block_hook(u32 startpc)
{
	if (ee_divtrace::g_enabled.load(std::memory_order_relaxed))
		ee_divtrace::RecordSample(startpc);
}
