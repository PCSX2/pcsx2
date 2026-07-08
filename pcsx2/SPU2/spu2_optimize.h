// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
//
// SPU2/spu2_optimize.h — Performance optimization utilities for SPU2.
// Purely additive. Does NOT modify any existing struct layouts or APIs.
// All existing code continues to compile unchanged.

#pragma once

#include <cstdint>
#include <cstring>
#include <array>
#include <utility>
#include <algorithm>

// ============================================================================
// Compiler Abstraction — Branch hints, prefetch, feature detection
// ============================================================================

#if defined(__GNUC__) || defined(__clang__)
    #define SPU2_LIKELY(x)       __builtin_expect(!!(x), 1)
    #define SPU2_UNLIKELY(x)     __builtin_expect(!!(x), 0)
    #define SPU2_PREFETCH_R(ptr) __builtin_prefetch((ptr), 0, 3)
    #define SPU2_PREFETCH_W(ptr) __builtin_prefetch((ptr), 1, 3)
#elif defined(_MSC_VER)
    #include <xmmintrin.h>
    #define SPU2_LIKELY(x)       (x)
    #define SPU2_UNLIKELY(x)     (x)
    #define SPU2_PREFETCH_R(ptr) _mm_prefetch((const char*)(ptr), _MM_HINT_T0)
    #define SPU2_PREFETCH_W(ptr) ((void)(ptr))
#else
    #define SPU2_LIKELY(x)       (x)
    #define SPU2_UNLIKELY(x)     (x)
    #define SPU2_PREFETCH_R(ptr) ((void)(ptr))
    #define SPU2_PREFETCH_W(ptr) ((void)(ptr))
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
    #define SPU2_HAS_NEON 1
#else
    #define SPU2_HAS_NEON 0
#endif

// ============================================================================
// ADSR / VolumeSlide Counter-Increment Lookup Table
// ============================================================================
// Replaces the variable-count shift:
//   u32 counter_inc = 0x8000 >> std::max(0, Shift - 11);
//   s32 level_inc   = Step << std::max(0, 11 - Shift);
// with a single table lookup indexed by Shift (0..31).
// Table is 32 entries x 8 bytes = 256 bytes in .rodata.
//
// NOTE: The table stores RAW shifted values. For shift >= 27, the raw
// value is 0. The calling code clamps it to 1 via std::max<u32>(1, ...).
// This matches the original PCSX2 behavior exactly.

namespace spu2_opt {

struct ADSRShiftEntry {
    uint32_t counter_inc;
    int32_t  level_shift;
};

static constexpr ADSRShiftEntry make_adsr_entry(int shift) {
    int rhs = (shift > 11) ? (shift - 11) : 0;
    int lhs = (shift < 11) ? (11 - shift) : 0;
    return ADSRShiftEntry{
        static_cast<uint32_t>(0x8000u >> rhs),
        lhs
    };
}

template<int... Is>
static constexpr std::array<ADSRShiftEntry, sizeof...(Is)>
make_table(std::integer_sequence<int, Is...>) {
    return {{ make_adsr_entry(Is)... }};
}

static constexpr auto kADSRShiftTable =
    make_table(std::make_integer_sequence<int, 32>{});

// Compile-time verification.
// counter_inc stores the RAW value from 0x8000 >> max(0, shift-11).
// For large shifts the result is 0; runtime code applies std::max(1,...).
static_assert(kADSRShiftTable[0].counter_inc  == 0x8000u, "shift=0:  0x8000>>0  = 0x8000");
static_assert(kADSRShiftTable[11].counter_inc == 0x8000u, "shift=11: 0x8000>>0  = 0x8000");
static_assert(kADSRShiftTable[15].counter_inc == 0x0800u, "shift=15: 0x8000>>4  = 0x0800");
static_assert(kADSRShiftTable[27].counter_inc == 0u,       "shift=27: 0x8000>>16 = 0");
static_assert(kADSRShiftTable[31].counter_inc == 0u,       "shift=31: 0x8000>>20 = 0 (clamped to 1 at runtime)");
static_assert(kADSRShiftTable[0].level_shift  == 11,       "shift=0:  11-0  = 11");
static_assert(kADSRShiftTable[11].level_shift == 0,         "shift=11: 11-11 = 0");
static_assert(kADSRShiftTable[15].level_shift == 0,         "shift=15: max(0,11-15) = 0");

// Safe accessor — masked to 5 bits (Shift field is 5 bits wide in ADSR/VolSlide)
inline const ADSRShiftEntry& GetShiftEntry(uint8_t shift) {
    return kADSRShiftTable[shift & 0x1F];
}

// Prefetch helper for next voice data in mixer loop.
// On ARM64 (L1 miss -> L2 ~10 cycles), this hides memory latency.
// Safe: prefetch is a hint, cannot cause side effects.
inline void PrefetchNextVoiceData(const void* ptr) {
    SPU2_PREFETCH_R(ptr);
}

// Branchless clamp matching existing clamp_mix behavior
inline int32_t fast_clamp_mix(int32_t x) {
    return std::clamp(x, static_cast<int32_t>(-0x8000),
                         static_cast<int32_t>(0x7fff));
}

} // namespace spu2_opt