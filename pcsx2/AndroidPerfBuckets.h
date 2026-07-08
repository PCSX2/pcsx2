// SPDX-FileCopyrightText: 2026 ARMSX2
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Console.h"
#include "common/Pcsx2Defs.h"

#include <atomic>
#include <chrono>

// Master switch for the Android EE/VU/GS/VIF perf-bucket instrumentation.
//
// When enabled this wraps hot per-op paths with two steady_clock reads + relaxed
// atomic adds and a periodic report check. The heaviest of these is the EE
// interpreter single-step thunk (recInterpStepThunk), which fires millions of
// times per second — so the instrumentation tanks real performance and MUST stay
// compiled out for any build whose FPS matters.
//
// The 2026-06-17 diagnostic run already used these buckets to pin the dominant EE
// cost on ee_interp_step (the EE single-stepping un-compilable ops through the
// interpreter); see the @@ANDROID_PERF_BUCKETS@@ markers. Flip this to 1 — or
// build with -DARMSX2_ANDROID_PERF_BUCKETS=1 — to collect another window. The
// atomic counters and every call site stay compiled in either way; only their
// bodies are gated, so re-enabling is a one-line change with no churn.
#ifndef ARMSX2_ANDROID_PERF_BUCKETS
#define ARMSX2_ANDROID_PERF_BUCKETS 0
#endif

namespace AndroidPerfBuckets
{
#if defined(__ANDROID__)
static constexpr u64 REPORT_WINDOW_US = 5'000'000;

inline std::atomic<u64> s_window_start_us{0};

inline std::atomic<u64> s_ee_recompile_count{0};
inline std::atomic<u64> s_ee_recompile_us{0};
inline std::atomic<u64> s_ee_recompile_interp_blocks{0};
inline std::atomic<u64> s_ee_recompile_ops{0};
inline std::atomic<u64> s_ee_interp_inline_count{0};
inline std::atomic<u64> s_ee_interp_inline_us{0};
inline std::atomic<u64> s_ee_interp_step_count{0};
inline std::atomic<u64> s_ee_interp_step_us{0};

inline std::atomic<u64> s_vu0_compile_count{0};
inline std::atomic<u64> s_vu0_compile_us{0};
inline std::atomic<u64> s_vu1_compile_count{0};
inline std::atomic<u64> s_vu1_compile_us{0};
inline std::atomic<u64> s_vu0_execute_count{0};
inline std::atomic<u64> s_vu0_execute_us{0};
inline std::atomic<u64> s_vu1_execute_count{0};
inline std::atomic<u64> s_vu1_execute_us{0};

inline std::atomic<u64> s_mtvu_wait_count{0};
inline std::atomic<u64> s_mtvu_wait_us{0};
inline std::atomic<u64> s_mtvu_exec_count{0};
inline std::atomic<u64> s_mtvu_exec_us{0};
inline std::atomic<u64> s_mtvu_reserve_wait_count{0};
inline std::atomic<u64> s_mtvu_reserve_wait_us{0};
inline std::atomic<u64> s_mtvu_reserve_spin_iters{0};
inline std::atomic<u64> s_waitvu_count{0};
inline std::atomic<u64> s_waitvu_us{0};

inline std::atomic<u64> s_xgkick_wait_count{0};
inline std::atomic<u64> s_xgkick_wait_us{0};
inline std::atomic<u64> s_mtgs_wait_count{0};
inline std::atomic<u64> s_mtgs_wait_us{0};
inline std::atomic<u64> s_mtgs_weak_spin_iters{0};

inline std::atomic<u64> s_vif_cpu_dyn_count{0};
inline std::atomic<u64> s_vif_cpu_dyn_us{0};
inline std::atomic<u64> s_vif_cpu_int_count{0};
inline std::atomic<u64> s_vif_cpu_int_us{0};
inline std::atomic<u64> s_vif_mtvu_queue_count{0};
inline std::atomic<u64> s_vif_mtvu_dyn_count{0};
inline std::atomic<u64> s_vif_mtvu_dyn_us{0};
inline std::atomic<u64> s_vif_mtvu_int_count{0};
inline std::atomic<u64> s_vif_mtvu_int_us{0};
inline std::atomic<u64> s_vif_dyn_overflow_fallback_count{0};
inline std::atomic<u64> s_vif_dyn_overflow_fallback_us{0};

#if ARMSX2_ANDROID_PERF_BUCKETS
inline u64 NowUs()
{
	using clock = std::chrono::steady_clock;
	return static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(
		clock::now().time_since_epoch()).count());
}

inline void Add(std::atomic<u64>& counter, u64 amount = 1)
{
	counter.fetch_add(amount, std::memory_order_relaxed);
}

inline u64 Take(std::atomic<u64>& counter)
{
	return counter.exchange(0, std::memory_order_relaxed);
}

inline void MaybeReport(const char* source)
{
	const u64 now = NowUs();
	u64 start = s_window_start_us.load(std::memory_order_relaxed);
	if (start == 0)
	{
		u64 expected = 0;
		s_window_start_us.compare_exchange_strong(expected, now, std::memory_order_relaxed);
		return;
	}

	if (now - start < REPORT_WINDOW_US)
		return;

	if (!s_window_start_us.compare_exchange_strong(start, now, std::memory_order_relaxed))
		return;

	Console.WriteLnFmt(
		"@@ANDROID_PERF_BUCKETS@@ src={} win_ms={} "
		"ee_recomp={}/{}us ee_recomp_ops={} ee_interp_blocks={} ee_interp_inline={}/{}us ee_interp_step={}/{}us "
		"vu0_compile={}/{}us vu1_compile={}/{}us vu0_exec={}/{}us vu1_exec={}/{}us "
		"mtvu_wait={}/{}us mtvu_exec={}/{}us mtvu_reserve_wait={}/{}us mtvu_reserve_spins={} waitvu={}/{}us "
		"xgkick_wait={}/{}us mtgs_wait={}/{}us mtgs_weak_spin_iters={} "
		"vif_cpu_dyn={}/{}us vif_cpu_int={}/{}us vif_mtvu_queue={} vif_mtvu_dyn={}/{}us vif_mtvu_int={}/{}us "
		"vif_dyn_overflow_fb={}/{}us",
		source, (now - start) / 1000,
		Take(s_ee_recompile_count), Take(s_ee_recompile_us),
		Take(s_ee_recompile_ops), Take(s_ee_recompile_interp_blocks),
		Take(s_ee_interp_inline_count), Take(s_ee_interp_inline_us),
		Take(s_ee_interp_step_count), Take(s_ee_interp_step_us),
		Take(s_vu0_compile_count), Take(s_vu0_compile_us),
		Take(s_vu1_compile_count), Take(s_vu1_compile_us),
		Take(s_vu0_execute_count), Take(s_vu0_execute_us),
		Take(s_vu1_execute_count), Take(s_vu1_execute_us),
		Take(s_mtvu_wait_count), Take(s_mtvu_wait_us),
		Take(s_mtvu_exec_count), Take(s_mtvu_exec_us),
		Take(s_mtvu_reserve_wait_count), Take(s_mtvu_reserve_wait_us),
		Take(s_mtvu_reserve_spin_iters),
		Take(s_waitvu_count), Take(s_waitvu_us),
		Take(s_xgkick_wait_count), Take(s_xgkick_wait_us),
		Take(s_mtgs_wait_count), Take(s_mtgs_wait_us),
		Take(s_mtgs_weak_spin_iters),
		Take(s_vif_cpu_dyn_count), Take(s_vif_cpu_dyn_us),
		Take(s_vif_cpu_int_count), Take(s_vif_cpu_int_us),
		Take(s_vif_mtvu_queue_count),
		Take(s_vif_mtvu_dyn_count), Take(s_vif_mtvu_dyn_us),
		Take(s_vif_mtvu_int_count), Take(s_vif_mtvu_int_us),
		Take(s_vif_dyn_overflow_fallback_count), Take(s_vif_dyn_overflow_fallback_us));
}

struct ScopedTimer
{
	std::atomic<u64>& count;
	std::atomic<u64>& total_us;
	const char* source;
	u64 start_us;

	ScopedTimer(std::atomic<u64>& count_, std::atomic<u64>& total_us_, const char* source_)
		: count(count_)
		, total_us(total_us_)
		, source(source_)
		, start_us(NowUs())
	{
	}

	~ScopedTimer()
	{
		Add(count);
		Add(total_us, NowUs() - start_us);
		MaybeReport(source);
	}
};
#else // __ANDROID__ but instrumentation compiled out — no-op so call sites still build.
inline u64 NowUs() { return 0; }
inline void Add(std::atomic<u64>&, u64 = 1) {}
inline void MaybeReport(const char*) {}

struct ScopedTimer
{
	ScopedTimer(std::atomic<u64>&, std::atomic<u64>&, const char*) {}
};
#endif // ARMSX2_ANDROID_PERF_BUCKETS

#else // !__ANDROID__
inline u64 NowUs() { return 0; }
inline void MaybeReport(const char*) {}
inline void Add(std::atomic<u64>&, u64 = 1) {}
#endif // __ANDROID__
} // namespace AndroidPerfBuckets
