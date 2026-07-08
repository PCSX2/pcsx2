// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
//
// SPU2/spu2_neon_dcfilter.h — NEON-optimized DC blocking filter.
//
// The DC filter is a simple first-order IIR high-pass filter:
//   output[n] = input[n] - input[n-1] + 0.995 * output[n-1]
//
// This is applied per-sample to both L and R channels.
// NEON processes both channels simultaneously.
//
// On MT6899 (Cortex-X925), this replaces 2 scalar subtracts + 2 FMUL + 2 FADD
// with 1 NEON vector operation.

#pragma once

#include <cstdint>

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define SPU2_DCFILTER_HAS_NEON 1
#else
#define SPU2_DCFILTER_HAS_NEON 0
#endif

namespace spu2_neon {

// ============================================================================
// DC Filter State — Holds previous input/output for both channels
// ============================================================================
// Aligned to 16 bytes for optimal NEON loads/stores.

struct DCFilterState {
#if SPU2_DCFILTER_HAS_NEON
    alignas(16) float prev_in[2]  = {0.0f, 0.0f};
    alignas(16) float prev_out[2] = {0.0f, 0.0f};
#else
    float prev_in[2]  = {0.0f, 0.0f};
    float prev_out[2] = {0.0f, 0.0f};
#endif

    void reset() {
        prev_in[0] = prev_in[1] = 0.0f;
        prev_out[0] = prev_out[1] = 0.0f;
    }
};

// ============================================================================
// DC Filter — Process one stereo sample
// ============================================================================
// Replaces the scalar code in spu2.cpp's DCFilter():
//   output[0] = input[0] - DCFilterIn[0] + 0.995f * DCFilterOut[0];
//   output[1] = input[1] - DCFilterIn[1] + 0.995f * DCFilterOut[1];
//
// NEON processes both channels in a single fused multiply-accumulate.

static __forceinline void DCFilterStereo(float input[2], DCFilterState& state)
{
#if SPU2_DCFILTER_HAS_NEON
    float32x2_t inp     = vld1_f32(input);
    float32x2_t prev_in = vld1_f32(state.prev_in);
    float32x2_t prev_out= vld1_f32(state.prev_out);
    float32x2_t coeff   = vdup_n_f32(0.995f);

    // output = (input - prev_in) + 0.995 * prev_out
    float32x2_t diff = vsub_f32(inp, prev_in);
    float32x2_t out  = vmla_f32(diff, prev_out, coeff); // diff + coeff*prev_out

    // Store results
    vst1_f32(input, out);
    vst1_f32(state.prev_in, inp);
    vst1_f32(state.prev_out, out);
#else
    float out0 = input[0] - state.prev_in[0] + 0.995f * state.prev_out[0];
    float out1 = input[1] - state.prev_in[1] + 0.995f * state.prev_out[1];
    state.prev_in[0]  = input[0];
    state.prev_in[1]  = input[1];
    state.prev_out[0] = out0;
    state.prev_out[1] = out1;
    input[0] = out0;
    input[1] = out1;
#endif
}

// ============================================================================
// Batch DC Filter — Process multiple stereo samples at once
// ============================================================================
// For the output chunk pipeline, processes N stereo frames.
// Each frame still has the IIR dependency, but L/R are parallel.

static __forceinline void DCFilterBatch(float* interleaved_stereo, uint32_t frame_count,
                                         DCFilterState& state)
{
    for (uint32_t i = 0; i < frame_count; i++) {
        DCFilterStereo(&interleaved_stereo[i * 2], state);
    }
}

// ============================================================================
// Batch s16-to-float conversion with DC filter — single pass
// ============================================================================
// Combines clamping, s16→float conversion, and DC filtering in one pass.
// Avoids writing intermediate values to memory.

static __forceinline void ConvertClampDCFilter(
    int32_t raw_L, int32_t raw_R,    // Raw mixer output (s32)
    float* out_L, float* out_R,      // Output float samples
    DCFilterState& state)
{
    // Clamp to s16 range
#if SPU2_DCFILTER_HAS_NEON
    int32x2_t raw = {raw_L, raw_R};
    int32x2_t lo  = vdup_n_s32(-0x8000);
    int32x2_t hi  = vdup_n_s32(0x7FFF);
    raw = vmax_s32(raw, lo);
    raw = vmin_s32(raw, hi);

    // s32 → f32
    float32x2_t fval = vcvt_f32_s32(raw);
    fval = vmul_n_f32(fval, 1.0f / 32767.0f);

    // DC filter
    float conv[2];
    vst1_f32(conv, fval);
    DCFilterStereo(conv, state);
    *out_L = conv[0];
    *out_R = conv[1];
#else
    float clamped_L = static_cast<float>(std::clamp(raw_L, -0x8000, 0x7FFF)) / 32767.0f;
    float clamped_R = static_cast<float>(std::clamp(raw_R, -0x8000, 0x7FFF)) / 32767.0f;
    float conv[2] = {clamped_L, clamped_R};
    DCFilterStereo(conv, state);
    *out_L = conv[0];
    *out_R = conv[1];
#endif
}

} // namespace spu2_neon