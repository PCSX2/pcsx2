// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "microVU_Divtrace.h"

// Test-hook only: the whole TU is dead weight in a normal build. Every consumer
// (the arm64 JIT emit site, the VU0/VU1 interp sites, EnterMode via the replay
// driver) is already gated on PCSX2_RECOMPILER_TESTS. The header stays
// unconditional so microVU_IR-arm64.h::snapshotMaps() keeps the AllocSnapshot
// type. Mirrors VU1Trace.cpp.
#ifdef PCSX2_RECOMPILER_TESTS

#include "VU.h"
#include "VUmicro.h"

#include "common/Assertions.h" // pxFailRel (Windows EnterMode stub)

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
// No ucontext/sigaction on Windows — the SIGTRAP capture machinery below is
// compiled out entirely (see EnterMode). The shared globals and fingerprint
// helpers still build, so the interp/JIT consumer sites link unchanged.
#elif defined(__APPLE__)
// <ucontext.h> declares the deprecated getcontext/setcontext/... routines and
// #errors out unless _XOPEN_SOURCE is defined. We only need the ucontext_t
// type, which <sys/ucontext.h> provides without that guard. (Mirrors
// CrashHandler.cpp / DarwinMisc.cpp, which reach ucontext_t via <csignal>.)
#include <sys/ucontext.h>
#else
#include <ucontext.h>
#endif

extern VURegs vuRegs[2];

namespace mvu_divtrace
{
	std::atomic<bool>          g_enabled{false};
	int                        g_vu_index = 0;
	std::vector<OpMeta>        g_meta;
	std::vector<StateSnap>     g_jit_snaps;
	std::vector<StateSnap>     g_interp_snaps;
	std::atomic<u32>           g_jit_snap_idx{0};
	u32                        g_interp_op_idx = 0;
	std::vector<u64>           g_jit_fps;
	std::vector<u64>           g_interp_fps;
	std::vector<u32>           g_jit_xpc;
	std::vector<u32>           g_interp_xpc;
	u32                        g_full_lo = 0;
	u32                        g_full_hi = 0;

	// Default full-snapshot window length. Each StateSnap is ~3 KB; the
	// two-pass driver overrides this to a small window around the first
	// divergence, so this only matters for the single-pass path.
	constexpr u32 kSnapCapacity = 65536;

	// Fingerprint stream capacity (executions). u64 fp + u32 xpc = 12 B/op,
	// so 8 M ≈ 96 MB/side — covers multi-million-op vumain loops that overflow
	// the full-snapshot stream.
	constexpr u32 kFpCapacity = 8u * 1024u * 1024u;

	// FNV-style mix.
	static inline u64 hash_mix(u64 h, u32 v)
	{
		h ^= static_cast<u64>(v);
		h *= 1099511628211ull;
		return h;
	}

	static inline bool isFullWidthVi(int i)
	{
		return i == REG_R || i == REG_I || i == REG_Q || i == REG_P
		    || i == REG_STATUS_FLAG || i == REG_MAC_FLAG || i == REG_CLIP_FLAG
		    || i == REG_TPC || i == REG_FBRST || i == REG_VPU_STAT;
	}

	u64 FingerprintRegs(const VURegs& r)
	{
		u64 h = 1469598103934665603ull;
		for (int i = 0; i < 32; ++i)
		{
			h = hash_mix(h, r.VF[i].UL[0]);
			h = hash_mix(h, r.VF[i].UL[1]);
			h = hash_mix(h, r.VF[i].UL[2]);
			h = hash_mix(h, r.VF[i].UL[3]);
		}
		h = hash_mix(h, r.ACC.UL[0]);
		h = hash_mix(h, r.ACC.UL[1]);
		h = hash_mix(h, r.ACC.UL[2]);
		h = hash_mix(h, r.ACC.UL[3]);
		// Skip the pipeline-state VI slots {16,17,18,22,23,26} (flag/Q/P/TPC),
		// which carry timing-dependent noise, so a fingerprint mismatch is a
		// real architectural divergence.
		for (int i = 0; i < 32; ++i)
		{
			if (i == 16 || i == 17 || i == 18 || i == 22 || i == 23 || i == 26)
				continue;
			const u32 mask = isFullWidthVi(i) ? 0xFFFFFFFFu : 0x0000FFFFu;
			h = hash_mix(h, r.VI[i].UL & mask);
		}
		return h;
	}

	void ConfigureFullWindow(u32 lo, u32 len)
	{
		g_full_lo = lo;
		g_full_hi = lo + len;
		g_jit_snaps.assign(len, StateSnap{});
		g_interp_snaps.assign(len, StateSnap{});
	}

	void Reset()
	{
		g_meta.clear();
		const u32 prev_jit = g_jit_snap_idx.load(std::memory_order_relaxed);
		for (u32 i = 0; i < prev_jit && i < g_jit_snaps.size(); ++i)
			std::memset(&g_jit_snaps[i], 0, sizeof(StateSnap));
		for (u32 i = 0; i < g_interp_op_idx && i < g_interp_snaps.size(); ++i)
			std::memset(&g_interp_snaps[i], 0, sizeof(StateSnap));
		g_jit_snap_idx.store(0, std::memory_order_relaxed);
		g_interp_op_idx = 0;
	}

#ifndef _WIN32
	namespace
	{
		struct sigaction s_prev_handler{};
		bool             s_installed = false;

		// brk #imm16 encoding (AArch64 BRK exception): 0xD4200000 | (imm16 << 5).
		void SigtrapHandler(int /*signo*/, siginfo_t* /*info*/, void* ucontext_v)
		{
#if defined(__aarch64__)
			if (!g_enabled.load(std::memory_order_relaxed))
			{
				// Not our trap — restore the disposition we saved at install
				// time and re-raise, so the prior handler (a debugger's, or
				// the default abort) actually gets it. Resetting to SIG_DFL
				// here would silently drop any pre-existing SIGTRAP handler.
				sigaction(SIGTRAP, &s_prev_handler, nullptr);
				std::raise(SIGTRAP);
				return;
			}
			auto* uc = static_cast<ucontext_t*>(ucontext_v);
#if defined(__APPLE__)
			// Darwin: uc_mcontext is a pointer and PC lives in __ss.__pc.
			auto& mc_pc = uc->uc_mcontext->__ss.__pc;
#else
			auto& mc_pc = uc->uc_mcontext.pc;
#endif
			const u64 pc = mc_pc;
			u32 insn = 0;
			std::memcpy(&insn, reinterpret_cast<const void*>(pc), sizeof(insn));
			// Validate it's actually a BRK (top 16 bits == 0xD420).
			if ((insn & 0xFFE0001Fu) != 0xD4200000u)
			{
				std::fprintf(stderr,
					"divtrace: SIGTRAP at pc=0x%lx but instruction 0x%08x is not BRK; aborting\n",
					static_cast<unsigned long>(pc), insn);
				std::abort();
			}
			const u16 brk_imm = static_cast<u16>((insn >> 5) & 0xFFFFu);
			const u32 idx = g_jit_snap_idx.fetch_add(1, std::memory_order_relaxed);
			if (idx >= g_jit_fps.size())
			{
				std::fprintf(stderr,
					"divtrace: jit fingerprint stream overflow (capacity=%zu); aborting\n",
					g_jit_fps.size());
				std::abort();
			}
			const u32 xpc = (brk_imm < g_meta.size())
				? g_meta[brk_imm].microvu_pc
				: 0xFFFFFFFFu;
			g_jit_fps[idx] = FingerprintRegs(vuRegs[g_vu_index]);
			g_jit_xpc[idx] = xpc;
			// Full StateSnap only inside the detail window.
			if (idx >= g_full_lo && idx < g_full_hi)
			{
				StateSnap& snap = g_jit_snaps[idx - g_full_lo];
				std::memcpy(&snap.regs, &vuRegs[g_vu_index], sizeof(VURegs));
				snap.meta_idx = brk_imm;
				snap.pre_xPC  = xpc;
			}
			mc_pc += 4; // skip the brk
#else
			// divtrace SIGTRAP capture is aarch64-only: it relies on the JIT
			// emitting brk instructions and on decoding them via mcontext.pc,
			// which only exists on AArch64. On other arches fall back to the
			// default SIGTRAP disposition so this TU still compiles.
			(void)ucontext_v;
			// Chain to the saved disposition rather than dropping it to
			// SIG_DFL (see the aarch64 branch above).
			sigaction(SIGTRAP, &s_prev_handler, nullptr);
			std::raise(SIGTRAP);
#endif
		}
	}
#endif // !_WIN32

	void EnterMode(int vu_index)
	{
#ifdef _WIN32
		// The capture path is SIGTRAP + ucontext (the JIT emits brk, the
		// handler snapshots and skips it) — there is no Windows port. Only
		// the offline replay drivers call this, and they don't build on
		// Windows; fail loudly rather than letting an unhandled brk look
		// like a JIT crash if that ever changes.
		(void)vu_index;
		pxFailRel("mvu_divtrace::EnterMode: SIGTRAP capture is not supported on Windows");
#else
		g_vu_index = vu_index;
		Reset();
		// Fingerprint streams: large, always recorded — these are what scale
		// to multi-million-op loops.
		g_jit_fps.assign(kFpCapacity, 0);
		g_interp_fps.assign(kFpCapacity, 0);
		g_jit_xpc.assign(kFpCapacity, 0);
		g_interp_xpc.assign(kFpCapacity, 0);
		// Default full-snapshot window: full snaps for the first kSnapCapacity
		// ops. The two-pass driver overrides this via ConfigureFullWindow
		// between passes.
		ConfigureFullWindow(0, kSnapCapacity);
		if (!s_installed)
		{
			struct sigaction sa{};
			sa.sa_sigaction = &SigtrapHandler;
			sa.sa_flags = SA_SIGINFO;
			sigemptyset(&sa.sa_mask);
			sigaction(SIGTRAP, &sa, &s_prev_handler);
			s_installed = true;
		}
		g_enabled.store(true, std::memory_order_release);
#endif
	}

	void ExitMode()
	{
		g_enabled.store(false, std::memory_order_release);
#ifndef _WIN32
		if (s_installed)
		{
			sigaction(SIGTRAP, &s_prev_handler, nullptr);
			s_installed = false;
		}
#endif
	}
} // namespace mvu_divtrace

#endif // PCSX2_RECOMPILER_TESTS
