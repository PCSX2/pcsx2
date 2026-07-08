// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
//
// SPU2/spu2_neon_reverb_ex.h — Extended NEON optimizations for reverb processing.
//
// Provides vectorized versions of the inner DoReverb computations:
//   - Comb filter accumulation (4 independent MULs → 1 NEON op)
//   - IIR filter (same/diff computed in parallel)
//   - APF (all-pass filter) computation
//   - Prefetch for all reverb memory accesses
//
// Integration: Call these helpers from ReaVerb.cpp's DoReverb() method.
// On non-ARM64, falls back to scalar code matching original behavior.

#pragma once

#include <cstdint>

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define SPU2_REVERB_EX_HAS_NEON 1
#else
#define SPU2_REVERB_EX_HAS_NEON 0
#endif

// Include prefetch macros
#include "SPU2/spu2_optimize.h"

namespace spu2_neon {

// ============================================================================
// Comb Filter Accumulation — 4 independent (mem * vol) >> 15 summed
// ============================================================================
// Original scalar code in DoReverb:
//   out = MUL(COMB1_VOL, mem[comb1]) + MUL(COMB2_VOL, mem[comb2])
//       + MUL(COMB3_VOL, mem[comb3]) + MUL(COMB4_VOL, mem[comb4]);
//
// All 4 MULs are independent → perfect for NEON parallelism.
// NEON: 2 vmull_s32 + 2 vshrn + 2 vadd + 1 vpadd = 7 ops
// Scalar: 4 multiply + 4 shift + 3 add = 11 ops (plus potential stalls)

static __forceinline int32_t CombFilterAccum4(
    int32_t comb1_vol, int32_t mem1,
    int32_t comb2_vol, int32_t mem2,
    int32_t comb3_vol, int32_t mem3,
    int32_t comb4_vol, int32_t mem4)
{
#if SPU2_REVERB_EX_HAS_NEON
    // Pack volumes and memory values into pairs
    int32x2_t mem_lo = {mem1, mem2};
    int32x2_t mem_hi = {mem3, mem4};
    int32x2_t vol_lo = {comb1_vol, comb2_vol};
    int32x2_t vol_hi = {comb3_vol, comb4_vol};

    // 32x32→64 multiply
    int64x2_t prod_lo = vmull_s32(mem_lo, vol_lo);
    int64x2_t prod_hi = vmull_s32(mem_hi, vol_hi);

    // Shift right by 15 and narrow to s32
    int32x2_t res_lo = vshrn_n_s64(prod_lo, 15);
    int32x2_t res_hi = vshrn_n_s64(prod_hi, 15);

    // Sum all 4 results
    int32x2_t sum_pair = vadd_s32(res_lo, res_hi);
    int32x2_t final_sum = vpadd_s32(sum_pair, sum_pair);
    return vget_lane_s32(final_sum, 0);
#else
    auto MUL = [](int32_t x, int32_t y) -> int32_t {
        return static_cast<int32_t>((static_cast<int64_t>(x) * y) >> 15);
    };
    return MUL(comb1_vol, mem1) + MUL(comb2_vol, mem2)
         + MUL(comb3_vol, mem3) + MUL(comb4_vol, mem4);
#endif
}

// ============================================================================
// IIR Filter — Same/Diff computed in parallel
// ============================================================================
// Original scalar code:
//   same = MUL(IIR_VOL, in + MUL(WALL_VOL, mem[same_src]) - mem[same_prv]) + mem[same_prv];
//   diff = MUL(IIR_VOL, in + MUL(WALL_VOL, mem[diff_src]) - mem[diff_prv]) + mem[diff_prv];
//
// Same and diff are independent → compute in parallel with NEON.

struct IIRResult {
    int32_t same;
    int32_t diff;
};

static __forceinline IIRResult IIRFilterParallel(
    int32_t in,
    int32_t iir_vol, int32_t wall_vol,
    int32_t same_src_mem, int32_t same_prv_mem,
    int32_t diff_src_mem, int32_t diff_prv_mem)
{
#if SPU2_REVERB_EX_HAS_NEON
    // Pack same and diff data into pairs
    int32x2_t wall_mem = {same_src_mem, diff_src_mem};
    int32x2_t prv_mem  = {same_prv_mem, diff_prv_mem};
    int32x2_t wall_v   = {wall_vol, wall_vol};
    int32x2_t iir_v    = {iir_vol, iir_vol};
    int32x2_t in_v     = {in, in};

    // Inner MUL: (wall_vol * mem[src]) >> 15
    int64x2_t wall_prod = vmull_s32(wall_mem, wall_v);
    int32x2_t wall_res  = vshrn_n_s64(wall_prod, 15);

    // temp = in + wall_res - prv
    int32x2_t temp = vadd_s32(in_v, vsub_s32(wall_res, prv_mem));

    // Outer MUL: (iir_vol * temp) >> 15
    int64x2_t iir_prod = vmull_s32(temp, iir_v);
    int32x2_t iir_res  = vshrn_n_s64(iir_prod, 15);

    // Final: iir_res + prv
    int32x2_t result = vadd_s32(iir_res, prv_mem);

    return {vget_lane_s32(result, 0), vget_lane_s32(result, 1)};
#else
    auto MUL = [](int32_t x, int32_t y) -> int32_t {
        return static_cast<int32_t>((static_cast<int64_t>(x) * y) >> 15);
    };
    return {
        MUL(iir_vol, in + MUL(wall_vol, same_src_mem) - same_prv_mem) + same_prv_mem,
        MUL(iir_vol, in + MUL(wall_vol, diff_src_mem) - diff_prv_mem) + diff_prv_mem
    };
#endif
}

// ============================================================================
// APF (All-Pass Filter) — Sequential but can use NEON MUL
// ============================================================================
// Original:
//   apf1 = out - MUL(APF1_VOL, mem[apf1_src]);
//   out  = mem[apf1_src] + MUL(APF1_VOL, apf1);
//   apf2 = out - MUL(APF2_VOL, mem[apf2_src]);
//   out  = mem[apf2_src] + MUL(APF2_VOL, apf2);
//
// APF1 and APF2 have a dependency (apf2 uses apf1's result).
// But we can still use NEON for the MUL itself.

static __forceinline void AllPassFilter2(
    int32_t in_out,
    int32_t apf1_vol, int32_t apf1_src_mem,
    int32_t apf2_vol, int32_t apf2_src_mem,
    int32_t& apf1_out, int32_t& final_out)
{
    auto MUL = [](int32_t x, int32_t y) -> int32_t {
#if SPU2_REVERB_EX_HAS_NEON
        int32x2_t va = {x, 0};
        int32x2_t vb = {y, 0};
        int64x2_t prod = vmull_s32(va, vb);
        int32x2_t res = vshrn_n_s64(prod, 15);
        return vget_lane_s32(res, 0);
#else
        return static_cast<int32_t>((static_cast<int64_t>(x) * y) >> 15);
#endif
    };

    int32_t apf1 = in_out - MUL(apf1_vol, apf1_src_mem);
    int32_t out  = apf1_src_mem + MUL(apf1_vol, apf1);
    int32_t apf2 = out - MUL(apf2_vol, apf2_src_mem);
    final_out    = apf2_src_mem + MUL(apf2_vol, apf2);
    apf1_out     = apf1;
}

// ============================================================================
// Reverb Prefetch — Issue prefetches for all reverb memory accesses
// ============================================================================
// DoReverb reads/writes ~14 different addresses in _spu2mem.
// On MT6899, issuing prefetches early hides L2 latency (~12ns).
// Call this function at the START of DoReverb, after computing indices.

static __forceinline void PrefetchReverbBuffers(
    const void* same_src, const void* same_prv, const void* same_dst,
    const void* diff_src, const void* diff_prv, const void* diff_dst,
    const void* comb1, const void* comb2, const void* comb3, const void* comb4,
    const void* apf1_src, const void* apf1_dst,
    const void* apf2_src, const void* apf2_dst)
{
    // Read prefetches — data we'll read soon
    SPU2_PREFETCH_R(same_src);
    SPU2_PREFETCH_R(same_prv);
    SPU2_PREFETCH_R(diff_src);
    SPU2_PREFETCH_R(diff_prv);
    SPU2_PREFETCH_R(comb1);
    SPU2_PREFETCH_R(comb2);
    SPU2_PREFETCH_R(comb3);
    SPU2_PREFETCH_R(comb4);
    SPU2_PREFETCH_R(apf1_src);
    SPU2_PREFETCH_R(apf2_src);

    // Write prefetches — data we'll write soon
    SPU2_PREFETCH_W(same_dst);
    SPU2_PREFETCH_W(diff_dst);
    SPU2_PREFETCH_W(apf1_dst);
    SPU2_PREFETCH_W(apf2_dst);
}

// ============================================================================
// Batch Clamp + Mix — Process 4 s32 values through clamp_mix
// ============================================================================
// After computing same, diff, apf1, apf2, they all need clamp_mix.
// Can process all 4 in parallel with NEON.

static __forceinline void ClampMix4(
    int32_t& v0, int32_t& v1, int32_t& v2, int32_t& v3)
{
#if SPU2_REVERB_EX_HAS_NEON
    int32x4_t vals = {v0, v1, v2, v3};
    int32x4_t lo   = vdupq_n_s32(-0x8000);
    int32x4_t hi   = vdupq_n_s32(0x7FFF);
    vals = vmaxq_s32(vals, lo);
    vals = vminq_s32(vals, hi);
    v0 = vgetq_lane_s32(vals, 0);
    v1 = vgetq_lane_s32(vals, 1);
    v2 = vgetq_lane_s32(vals, 2);
    v3 = vgetq_lane_s32(vals, 3);
#else
    v0 = std::clamp(v0, -0x8000, 0x7FFF);
    v1 = std::clamp(v1, -0x8000, 0x7FFF);
    v2 = std::clamp(v2, -0x8000, 0x7FFF);
    v3 = std::clamp(v3, -0x8000, 0x7FFF);
#endif
}

} // namespace spu2_neon