// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace GSShaderCompileIndicator
{
	inline constexpr std::uint64_t OSD_RECENT_COMPILE_NS = 500'000'000ULL;

	inline std::atomic<std::uint32_t> s_active_compilations{0};
	inline std::atomic<std::uint64_t> s_last_compile_activity_ns{0};

	namespace detail
	{
		inline std::uint64_t steady_now_ns()
		{
			return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now().time_since_epoch())
					.count());
		}
	} // namespace detail

	inline void TouchCompileActivity()
	{
		s_last_compile_activity_ns.store(detail::steady_now_ns(), std::memory_order_relaxed);
	}

	inline void BeginCompilation()
	{
		TouchCompileActivity();
		s_active_compilations.fetch_add(1, std::memory_order_relaxed);
	}

	inline void EndCompilation()
	{
		s_active_compilations.fetch_sub(1, std::memory_order_relaxed);
		TouchCompileActivity();
	}

	inline std::uint32_t GetActiveCompilationCount()
	{
		return s_active_compilations.load(std::memory_order_relaxed);
	}

	inline bool ShouldShowOnOSD(std::uint64_t hold_ns = OSD_RECENT_COMPILE_NS)
	{
		if (GetActiveCompilationCount() != 0)
			return true;

		const std::uint64_t last = s_last_compile_activity_ns.load(std::memory_order_relaxed);
		if (last == 0)
			return false;

		const std::uint64_t now = detail::steady_now_ns();
		return now > last && (now - last) < hold_ns;
	}

	inline float GetPostCompileFadeAlpha(std::uint64_t hold_ns = OSD_RECENT_COMPILE_NS)
	{
		if (GetActiveCompilationCount() != 0)
			return 1.0f;

		const std::uint64_t last = s_last_compile_activity_ns.load(std::memory_order_relaxed);
		if (last == 0)
			return 0.0f;

		const std::uint64_t now = detail::steady_now_ns();
		if (now <= last)
			return 1.0f;

		const std::uint64_t elapsed = now - last;
		if (elapsed >= hold_ns)
			return 0.0f;

		return 1.0f - static_cast<float>(elapsed) / static_cast<float>(hold_ns);
	}

	struct ScopedCompilation
	{
		ScopedCompilation() { BeginCompilation(); }
		~ScopedCompilation() { EndCompilation(); }
		ScopedCompilation(const ScopedCompilation&) = delete;
		ScopedCompilation& operator=(const ScopedCompilation&) = delete;
	};
} // namespace GSShaderCompileIndicator
