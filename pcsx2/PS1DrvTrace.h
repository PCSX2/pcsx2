// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Rate-limited / categorized trace helpers for PS1-mode runtime debugging.
// Toggles live in arm64/InterpFlags.h: PS1DRV_TRACE_<CAT> where CAT is one
// of CDROM / DMA / IRQ / GPU / SIO / MDEC. Logs prefixed with
// [PS1DRV-<CAT>] for grep-friendly filtering.
//
// Capture: `adb logcat -s STDOUT:W | grep PS1DRV` while the device runs.
//
// Macros (no-op when toggle off):
//   PS1DRV_TRACE_LOG(CAT, fmt, ...)
//     Unconditional log if PS1DRV_TRACE_<CAT> is defined.
//
//   PS1DRV_TRACE_RATE(CAT, key, every_n_cycles, fmt, ...)
//     Log only if at least every_n_cycles IOP cycles have elapsed since the
//     last hit at this `key` (a string literal — used as the dedupe key).
//     Use for events that fire every frame / every block.
//
//   PS1DRV_TRACE_CHANGE(CAT, key, var, fmt, ...)
//     Log only when `var` differs from the value seen at the previous call
//     with the same `key`. Use to surface state transitions without
//     spamming on no-change polls.

#pragma once

#include "Common.h"
#include "R3000A.h"
#include "arm64/InterpFlags.h"

#include <unordered_map>
#include <cstdint>
#include <string>

namespace PS1DrvTrace
{
	struct RateState
	{
		u64 last_cycle = 0;
	};

	struct ChangeState
	{
		bool seen = false;
		u64 last_value = 0;
	};

	inline std::unordered_map<std::string, RateState>& rateMap()
	{
		static std::unordered_map<std::string, RateState> m;
		return m;
	}

	inline std::unordered_map<std::string, ChangeState>& changeMap()
	{
		static std::unordered_map<std::string, ChangeState> m;
		return m;
	}

	inline bool rateAllow(const char* key, u64 every_n_cycles)
	{
		auto& s = rateMap()[key];
		const u64 now = psxRegs.cycle;
		if (now - s.last_cycle < every_n_cycles && s.last_cycle != 0)
			return false;
		s.last_cycle = now;
		return true;
	}

	inline bool changeAllow(const char* key, u64 value)
	{
		auto& s = changeMap()[key];
		if (s.seen && s.last_value == value)
			return false;
		s.seen = true;
		s.last_value = value;
		return true;
	}
}

#define PS1DRV_TRACE_LOG_IMPL(CAT, ...) \
	Console.WriteLn("[PS1DRV-" CAT "] " __VA_ARGS__)

#define PS1DRV_TRACE_RATE_IMPL(CAT, key, every_n, ...) \
	do { if (PS1DrvTrace::rateAllow("PS1DRV-" CAT ":" key, (every_n))) \
		Console.WriteLn("[PS1DRV-" CAT "] " __VA_ARGS__); } while (0)

#define PS1DRV_TRACE_CHANGE_IMPL(CAT, key, var, ...) \
	do { if (PS1DrvTrace::changeAllow("PS1DRV-" CAT ":" key, static_cast<u64>(var))) \
		Console.WriteLn("[PS1DRV-" CAT "] " __VA_ARGS__); } while (0)

#if defined(PS1DRV_TRACE_CDROM)
	#define PS1DRV_LOG_CDROM(...)        PS1DRV_TRACE_LOG_IMPL("CDROM", __VA_ARGS__)
	#define PS1DRV_RATE_CDROM(k, n, ...) PS1DRV_TRACE_RATE_IMPL("CDROM", k, n, __VA_ARGS__)
	#define PS1DRV_CHG_CDROM(k, v, ...)  PS1DRV_TRACE_CHANGE_IMPL("CDROM", k, v, __VA_ARGS__)
#else
	#define PS1DRV_LOG_CDROM(...) do {} while (0)
	#define PS1DRV_RATE_CDROM(k, n, ...) do {} while (0)
	#define PS1DRV_CHG_CDROM(k, v, ...) do {} while (0)
#endif

#if defined(PS1DRV_TRACE_DMA)
	#define PS1DRV_LOG_DMA(...)          PS1DRV_TRACE_LOG_IMPL("DMA", __VA_ARGS__)
	#define PS1DRV_RATE_DMA(k, n, ...)   PS1DRV_TRACE_RATE_IMPL("DMA", k, n, __VA_ARGS__)
	#define PS1DRV_CHG_DMA(k, v, ...)    PS1DRV_TRACE_CHANGE_IMPL("DMA", k, v, __VA_ARGS__)
#else
	#define PS1DRV_LOG_DMA(...) do {} while (0)
	#define PS1DRV_RATE_DMA(k, n, ...) do {} while (0)
	#define PS1DRV_CHG_DMA(k, v, ...) do {} while (0)
#endif

#if defined(PS1DRV_TRACE_IRQ)
	#define PS1DRV_LOG_IRQ(...)          PS1DRV_TRACE_LOG_IMPL("IRQ", __VA_ARGS__)
	#define PS1DRV_RATE_IRQ(k, n, ...)   PS1DRV_TRACE_RATE_IMPL("IRQ", k, n, __VA_ARGS__)
	#define PS1DRV_CHG_IRQ(k, v, ...)    PS1DRV_TRACE_CHANGE_IMPL("IRQ", k, v, __VA_ARGS__)
#else
	#define PS1DRV_LOG_IRQ(...) do {} while (0)
	#define PS1DRV_RATE_IRQ(k, n, ...) do {} while (0)
	#define PS1DRV_CHG_IRQ(k, v, ...) do {} while (0)
#endif

#if defined(PS1DRV_TRACE_GPU)
	#define PS1DRV_LOG_GPU(...)          PS1DRV_TRACE_LOG_IMPL("GPU", __VA_ARGS__)
	#define PS1DRV_RATE_GPU(k, n, ...)   PS1DRV_TRACE_RATE_IMPL("GPU", k, n, __VA_ARGS__)
	#define PS1DRV_CHG_GPU(k, v, ...)    PS1DRV_TRACE_CHANGE_IMPL("GPU", k, v, __VA_ARGS__)
#else
	#define PS1DRV_LOG_GPU(...) do {} while (0)
	#define PS1DRV_RATE_GPU(k, n, ...) do {} while (0)
	#define PS1DRV_CHG_GPU(k, v, ...) do {} while (0)
#endif

#if defined(PS1DRV_TRACE_SIO)
	#define PS1DRV_LOG_SIO(...)          PS1DRV_TRACE_LOG_IMPL("SIO", __VA_ARGS__)
	#define PS1DRV_RATE_SIO(k, n, ...)   PS1DRV_TRACE_RATE_IMPL("SIO", k, n, __VA_ARGS__)
	#define PS1DRV_CHG_SIO(k, v, ...)    PS1DRV_TRACE_CHANGE_IMPL("SIO", k, v, __VA_ARGS__)
#else
	#define PS1DRV_LOG_SIO(...) do {} while (0)
	#define PS1DRV_RATE_SIO(k, n, ...) do {} while (0)
	#define PS1DRV_CHG_SIO(k, v, ...) do {} while (0)
#endif

#if defined(PS1DRV_TRACE_MDEC)
	#define PS1DRV_LOG_MDEC(...)         PS1DRV_TRACE_LOG_IMPL("MDEC", __VA_ARGS__)
	#define PS1DRV_RATE_MDEC(k, n, ...)  PS1DRV_TRACE_RATE_IMPL("MDEC", k, n, __VA_ARGS__)
	#define PS1DRV_CHG_MDEC(k, v, ...)   PS1DRV_TRACE_CHANGE_IMPL("MDEC", k, v, __VA_ARGS__)
#else
	#define PS1DRV_LOG_MDEC(...) do {} while (0)
	#define PS1DRV_RATE_MDEC(k, n, ...) do {} while (0)
	#define PS1DRV_CHG_MDEC(k, v, ...) do {} while (0)
#endif
