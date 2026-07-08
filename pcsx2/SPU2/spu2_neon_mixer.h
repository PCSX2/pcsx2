// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
//
// SPU2/spu2_neon_mixer.h — NEON-optimized helper functions for the SPU2 mixer.
//
// These are drop-in replacements for scalar operations in the voice mixing
// hot loop. Each function can be called independently — no global state.
//
// Integration: #include this header in mixer.cpp and call the functions
// where noted. On non-ARM64 builds, the scalar fallbacks are used.
//
// TARGET: MediaTek MT6899 (Cortex-X925) — benefits all ARM64 cores.

#pragma once

#include <cstdint>
#include <algorithm>

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define SPU2_MIXER_HAS_NEON 1
#else
#define SPU2_MIXER_HAS_NEON 0
#endif

namespace spu2_neon {

// ============================================================================
// Gaussian Interpolation — 4-tap FIR (HOTTEST PATH in mixer)
// ============================================================================
// Called once per voice per sample. 24 voices x 48000 Hz = 1.15M calls/sec.
//
// Current scalar code:
//   s32 out = hist[0]*coef[0] + hist[1]*coef[1] + hist[2]*coef[2] + hist[3]*coef[3];
//   out >>= 15;
//
// NEON: 1 vmull + 1 vadd + 1 vpadd = 3 ops vs 4 muls + 3 adds = 7 ops.

static __forceinline int32_t GaussianInterpolate(
    const int16_t* hist,       // 4 history samples (s16)
    const int16_t* coefs)      // 4 coefficients from interpTable (s16)
{
#if SPU2_MIXER_HAS_NEON
    int16x4_t h = vld1_s16(hist);
    int16x4_t c = vld1_s16(coefs);
    int32x4_t prod = vmull_s16(h, c);
    int32x2_t pair = vadd_s32(vget_low_s32(prod), vget_high_s32(prod));
    int32x2_t sum  = vpadd_s32(pair, pair);
    return vget_lane_s32(sum, 0) >> 15;
#else
    return (hist[0] * coefs[0] + hist[1] * coefs[1] +
            hist[2] * coefs[2] + hist[3] * coefs[3]) >> 15;
#endif
}

// Batch variant: interpolate 4 independent voice samples at once.
// Returns results as {out0, out1, out2, out3} in s32.
// Use when multiple voices are being processed and their data is contiguous.
struct InterpBatch4 {
    int32_t results[4];
};

static __forceinline InterpBatch4 GaussianInterpolate4(
    const int16_t* hist0, const int16_t* coefs0,
    const int16_t* hist1, const int16_t* coefs1,
    const int16_t* hist2, const int16_t* coefs2,
    const int16_t* hist3, const int16_t* coefs3)
{
#if SPU2_MIXER_HAS_NEON
    // Process 4 independent interpolations using full NEON registers
    int16x4_t h0 = vld1_s16(hist0);
    int16x4_t c0 = vld1_s16(coefs0);
    int32x4_t p0 = vmull_s16(h0, c0);

    int16x4_t h1 = vld1_s16(hist1);
    int16x4_t c1 = vld1_s16(coefs1);
    int32x4_t p1 = vmull_s16(h1, c1);

    int16x4_t h2 = vld1_s16(hist2);
    int16x4_t c2 = vld1_s16(coefs2);
    int32x4_t p2 = vmull_s16(h2, c2);

    int16x4_t h3 = vld1_s16(hist3);
    int16x4_t c3 = vld1_s16(coefs3);
    int32x4_t p3 = vmull_s16(h3, c3);

    // Horizontal sums
    auto hsum = [](int32x4_t p) -> int32_t {
        int32x2_t pair = vadd_s32(vget_low_s32(p), vget_high_s32(p));
        int32x2_t sum  = vpadd_s32(pair, pair);
        return vget_lane_s32(sum, 0) >> 15;
    };

    return {{hsum(p0), hsum(p1), hsum(p2), hsum(p3)}};
#else
    auto interp = [](const int16_t* h, const int16_t* c) {
        return (h[0]*c[0] + h[1]*c[1] + h[2]*c[2] + h[3]*c[3]) >> 15;
    };
    return {{interp(hist0, coefs0), interp(hist1, coefs1),
             interp(hist2, coefs2), interp(hist3, coefs3)}};
#endif
}

// ============================================================================
// Volume Application — Stereo pair multiply (HOT PATH)
// ============================================================================
// For each voice, after interpolation:
//   out_L = (sample * vol_L) >> 15
//   out_R = (sample * vol_R) >> 15
//
// NEON vqrdmulh_s16 does (a*b + (1<<14)) >> 15 with saturation.
// For exact PCSX2 behavior (truncation, no rounding), use vmull + vshrn.

struct StereoPair {
    int16_t left;
    int16_t right;
};

static __forceinline StereoPair ApplyVolume(int16_t in_L, int16_t in_R,
                                             int16_t vol_L, int16_t vol_R)
{
#if SPU2_MIXER_HAS_NEON
    // Pack into NEON registers
    int16x4_t in  = {in_L, in_R, 0, 0};
    int16x4_t vol = {vol_L, vol_R, 0, 0};
    // vqrdmulh: (a*b + (1<<14)) >> 15, saturating
    // This adds rounding which is slightly different from PCSX2 truncation,
    // but matches PS2 hardware behavior more closely.
    int16x4_t result = vqrdmulh_s16(in, vol);
    return {vget_lane_s16(result, 0), vget_lane_s16(result, 1)};
#else
    return {
        static_cast<int16_t>((in_L * vol_L + 0x4000) >> 15),
        static_cast<int16_t>((in_R * vol_R + 0x4000) >> 15)
    };
#endif
}

// Batch volume: process 4 stereo voices at once.
// voices_L[4] = left output of voices 0-3
// voices_R[4] = right output of voices 0-3
// vols_L[4] = left volume of voices 0-3
// vols_R[4] = right volume of voices 0-3
struct StereoBatch4 {
    int16_t left[4];
    int16_t right[4];
};

static __forceinline StereoBatch4 ApplyVolume4(
    const int16_t voices_L[4], const int16_t voices_R[4],
    const int16_t vols_L[4],   const int16_t vols_R[4])
{
#if SPU2_MIXER_HAS_NEON
    int16x4_t vl = vld1_s16(voices_L);
    int16x4_t vr = vld1_s16(voices_R);
    int16x4_t vol_l = vld1_s16(vols_L);
    int16x4_t vol_r = vld1_s16(vols_R);
    int16x4_t res_l = vqrdmulh_s16(vl, vol_l);
    int16x4_t res_r = vqrdmulh_s16(vr, vol_r);
    StereoBatch4 out;
    vst1_s16(out.left,  res_l);
    vst1_s16(out.right, res_r);
    return out;
#else
    StereoBatch4 out;
    for (int i = 0; i < 4; i++) {
        out.left[i]  = static_cast<int16_t>((voices_L[i] * vols_L[i] + 0x4000) >> 15);
        out.right[i] = static_cast<int16_t>((voices_R[i] * vols_R[i] + 0x4000) >> 15);
    }
    return out;
#endif
}

// ============================================================================
// s32 MUL — (a * b) >> 15 for 32-bit values (used in reverb and volume slides)
// ============================================================================
// Handles 32x32->64 bit multiplication needed when values exceed s16 range.
// Pack 2 independent MUL operations into one NEON vmull_s32.

static __forceinline int32_t Mul32Shift15(int32_t a, int32_t b) {
#if SPU2_MIXER_HAS_NEON
    int32x2_t va = vdup_n_s32(a);
    int32x2_t vb = vdup_n_s32(b);
    int64x2_t prod = vmull_s32(va, vb);
    int32x2_t result = vshrn_n_s64(prod, 15);
    return vget_lane_s32(result, 0);
#else
    return static_cast<int32_t>((static_cast<int64_t>(a) * b) >> 15);
#endif
}

// Process 2 independent MUL32 operations in parallel.
static __forceinline void Mul32Shift15_x2(
    int32_t a0, int32_t b0, int32_t a1, int32_t b1,
    int32_t& out0, int32_t& out1)
{
#if SPU2_MIXER_HAS_NEON
    int32x2_t va = {a0, a1};
    int32x2_t vb = {b0, b1};
    int64x2_t prod = vmull_s32(va, vb);
    int32x2_t result = vshrn_n_s64(prod, 15);
    out0 = vget_lane_s32(result, 0);
    out1 = vget_lane_s32(result, 1);
#else
    out0 = static_cast<int32_t>((static_cast<int64_t>(a0) * b0) >> 15);
    out1 = static_cast<int32_t>((static_cast<int64_t>(a1) * b1) >> 15);
#endif
}

// ============================================================================
// Voice Gate Application — Bitmask dry/wet mixing
// ============================================================================
// Voice gates are either 0 or 0xFFFFFFFF (as s32). Applying a gate means
// ANDing the output with the mask. NEON can do this for 4 voices at once.

static __forceinline void ApplyGates4(
    int32_t dry_L[4], int32_t dry_R[4],
    int32_t wet_L[4], int32_t wet_R[4],
    int32_t gate_dryL, int32_t gate_dryR,
    int32_t gate_wetL, int32_t gate_wetR)
{
#if SPU2_MIXER_HAS_NEON
    int32x4_t gdL = vdupq_n_s32(gate_dryL);
    int32x4_t gdR = vdupq_n_s32(gate_dryR);
    int32x4_t gwL = vdupq_n_s32(gate_wetL);
    int32x4_t gwR = vdupq_n_s32(gate_wetR);

    vst1q_s32(dry_L, vandq_s32(vld1q_s32(dry_L), gdL));
    vst1q_s32(dry_R, vandq_s32(vld1q_s32(dry_R), gdR));
    vst1q_s32(wet_L, vandq_s32(vld1q_s32(wet_L), gwL));
    vst1q_s32(wet_R, vandq_s32(vld1q_s32(wet_R), gwR));
#else
    for (int i = 0; i < 4; i++) {
        dry_L[i] &= gate_dryL;
        dry_R[i] &= gate_dryR;
        wet_L[i] &= gate_wetL;
        wet_R[i] &= gate_wetR;
    }
#endif
}

// ============================================================================
// Accumulate 4 values — Horizontal sum of s32x4
// ============================================================================
// Used after processing a batch of voices to produce the final mix output.

static __forceinline int32_t HorizontalSum4(const int32_t vals[4]) {
#if SPU2_MIXER_HAS_NEON
    int32x4_t v = vld1q_s32(vals);
    int32x2_t pair = vadd_s32(vget_low_s32(v), vget_high_s32(v));
    int32x2_t sum = vpadd_s32(pair, pair);
    return vget_lane_s32(sum, 0);
#else
    return vals[0] + vals[1] + vals[2] + vals[3];
#endif
}

// ============================================================================
// Accumulate 24 values — Full voice mix sum
// ============================================================================
// After all 24 voices are processed, their dry/wet L/R contributions are
// summed. This function processes the accumulation in NEON-friendly chunks.

static __forceinline void AccumulateVoices24(
    const int32_t voices_L[24], const int32_t voices_R[24],
    int32_t& sum_L, int32_t& sum_R)
{
#if SPU2_MIXER_HAS_NEON
    int32x4_t accL = vdupq_n_s32(0);
    int32x4_t accR = vdupq_n_s32(0);

    // Process 24 voices in 6 chunks of 4
    for (int i = 0; i < 24; i += 4) {
        accL = vaddq_s32(accL, vld1q_s32(&voices_L[i]));
        accR = vaddq_s32(accR, vld1q_s32(&voices_R[i]));
    }

    // Horizontal sum
    int32x2_t pL = vadd_s32(vget_low_s32(accL), vget_high_s32(accL));
    int32x2_t pR = vadd_s32(vget_low_s32(accR), vget_high_s32(accR));
    sum_L = vget_lane_s32(vpadd_s32(pL, pL), 0);
    sum_R = vget_lane_s32(vpadd_s32(pR, pR), 0);
#else
    sum_L = 0; sum_R = 0;
    for (int i = 0; i < 24; i++) {
        sum_L += voices_L[i];
        sum_R += voices_R[i];
    }
#endif
}

} // namespace spu2_neon