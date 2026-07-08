// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
//
// SPU2/spu2_sve2_fir.h — SVE2-optimized FIR filters for reverb.
//
// SVE2 (Scalable Vector Extension 2) is available on ARMv9.2-A cores
// including the Cortex-X925 in the MT6899 (Dimensity 9400).
//
// Key advantages over NEON for this workload:
//   - Predicated operations (no scalar tail loops needed)
//   - Wider vectors on future cores (currently 128-bit on X925)
//   - smlalb/smlalt instructions for efficient multiply-accumulate
//
// IMPORTANT: Requires runtime detection. The function pointers are only
// overridden if SVE2 is actually available on the device.
//
// Compiler requirements:
//   NDK r26+ with Clang 17+
//   Compile with: -march=armv8.2-a+sve2 (or armv9-a)
//
// Note: If your build system doesn't support SVE2 compilation flags,
// this file gracefully falls back to NEON. SVE2 is purely optional.

#pragma once

#include <cstdint>

// SVE2 detection — requires both compiler support and runtime availability
#if defined(__aarch64__) || defined(_M_ARM64)

// Check if SVE2 intrinsics are available (compiler must support them)
#if defined(__ARM_FEATURE_SVE2) || defined(__ARM_FEATURE_SVE2_BITPERM)
#define SPU2_HAS_SVE2_COMPILER 1
#include <arm_sve.h>
#else
#define SPU2_HAS_SVE2_COMPILER 0
#endif

#include "SPU2/spu2_mt6899_tuning.h"
#include "SPU2/defs.h"
#include <array>
#include <algorithm>

namespace spu2_neon {

// ============================================================================
// SVE2 FIR Downsample — 39-tap half-band filter
// ============================================================================
// Processes 39 s16 samples with 39 s16 coefficients.
// Uses SVE2's predicated loads for clean handling of the 39-element count
// (not a power of 2, no scalar tail needed).

#if SPU2_HAS_SVE2_COMPILER
static int32_t ReverbDownsample_sve2(V_Core& core, bool right)
{
    static constexpr int NUM_TAPS = 39;

    // Coefficients (same as ReverbResample.cpp)
    static constexpr int16_t coefs[NUM_TAPS] = {
        -1, 0, 2, 0, -10, 0, 35, 0,
        -103, 0, 266, 0, -616, 0, 1332, 0,
        -2960, 0, 10246, 16384, 10246, 0, -2960, 0,
        1332, 0, -616, 0, 266, 0, -103, 0,
        35, 0, -10, 0, 2, 0, -1,
    };

    const int index = (core.RevbSampleBufPos - NUM_TAPS) & 63;
    const int16_t* src = &core.RevbDownBuf[right][index];

    // Get the number of 16-bit elements per SVE vector
    const int vl = svcntw() * 2; // svcnth() equivalent — actually svcnth()

    // Use SVE2 predicated loop
    svint32_t acc = svdup_s32(0);
    svbool_t pred;

    int i = 0;
    for (; i < NUM_TAPS; i += svcnth()) {
        // Create predicate for remaining elements
        pred = svwhilelt_b16(i, NUM_TAPS);

        // Load coefficients and source samples
        svint16_t c = svld1_s16(pred, &coefs[i]);
        svint16_t s = svld1_s16(pred, &src[i]);

        // Multiply-accumulate: widen to s32 and accumulate
        // smlalb: multiply-add long bottom (even elements)
        // smlalt: multiply-add long top (odd elements)
        acc = svmlalt_s32(svmlalb_s32(acc, s, c), s, c);
    }

    // Horizontal sum of accumulator
    int32_t sum = svaddv_s32(svptrue_b32(), acc);

    return clamp_mix(sum);
}
#endif // SPU2_HAS_SVE2_COMPILER

// ============================================================================
// SVE2 FIR Upsample — 39-tap, both L/R channels
// ============================================================================

#if SPU2_HAS_SVE2_COMPILER
static StereoOut32 ReverbUpsample_sve2(V_Core& core)
{
    static constexpr int NUM_TAPS = 39;

    static constexpr int16_t coefs[NUM_TAPS] = {
        -2, 0, 4, 0, -20, 0, 70, 0,
        -206, 0, 532, 0, -1232, 0, 2664, 0,
        -5920, 0, 20492, 32768, 20492, 0, -5920, 0,
        2664, 0, -1232, 0, 532, 0, -206, 0,
        70, 0, -20, 0, 4, 0, -2,
    };

    const int index = (core.RevbSampleBufPos - NUM_TAPS) & 63;
    const int16_t* srcL = &core.RevbUpBuf[0][index];
    const int16_t* srcR = &core.RevbUpBuf[1][index];

    svint32_t accL = svdup_s32(0);
    svint32_t accR = svdup_s32(0);

    for (int i = 0; i < NUM_TAPS; i += svcnth()) {
        svbool_t pred = svwhilelt_b16(i, NUM_TAPS);
        svint16_t c  = svld1_s16(pred, &coefs[i]);
        svint16_t sL = svld1_s16(pred, &srcL[i]);
        svint16_t sR = svld1_s16(pred, &srcR[i]);

        accL = svmlalt_s32(svmlalb_s32(accL, sL, c), sL, c);
        accR = svmlalt_s32(svmlalb_s32(accR, sR, c), sR, c);
    }

    int32_t sumL = svaddv_s32(svptrue_b32(), accL);
    int32_t sumR = svaddv_s32(svptrue_b32(), accR);

    return {clamp_mix(sumL), clamp_mix(sumR)};
}
#endif // SPU2_HAS_SVE2_COMPILER

// ============================================================================
// SVE2 Registration — Runtime detection and function pointer override
// ============================================================================

#if SPU2_HAS_SVE2_COMPILER
inline bool TryRegisterSVE2FIR() {
    // Runtime check: is SVE2 actually available?
    if (!spu2_mt6899::GetFeatures().sve2)
        return false;

    // Override function pointers with SVE2 implementations
    ReverbDownsample = ReverbDownsample_sve2;
    ReverbUpsample   = ReverbUpsample_sve2;
    return true;
}
#endif // SPU2_HAS_SVE2_COMPILER

} // namespace spu2_neon

#endif // __aarch64__