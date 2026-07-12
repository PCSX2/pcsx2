// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
//
// SPU2/spu2_neon.cpp — ARM64 backend registration for SPU2.
//
// Registers NEON (and optionally SVE2) optimized implementations by
// overriding the global function pointers at runtime.
// Called once from SPU2::Open() after Multi-ISA default init.
//
// Only compiled on ARM64 targets. On x86, this file produces no object code.

#if defined(__aarch64__) || defined(_M_ARM64)

#include "SPU2/spu2_neon.h"
// spu2_sve2_fir.h defines SPU2_HAS_SVE2_COMPILER and, when the compiler actually
// targets SVE2, TryRegisterSVE2FIR(); it also pulls in spu2_mt6899_tuning.h for
// runtime CPU feature detection. defs.h provides V_Core / StereoOut32 /
// clamp_mix and the ReverbDownsample/ReverbUpsample function pointers.
#include "SPU2/spu2_sve2_fir.h"
#include "SPU2/defs.h"
// NOTE: the spu2_neon_mixer / _reverb_ex / _dcfilter helper headers are
// intentionally NOT included. They hold drop-in SIMD helpers meant to be called
// from mixer.cpp / ReaVerb.cpp, but that integration hasn't been wired up — this
// TU only needs the reverb FIR below plus the SVE2 hook. Pulling them in would
// compile a pile of currently-unused (and not-yet-portable) code on every arm64
// target (they use the MSVC-only __forceinline keyword unguarded).

#include <arm_neon.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>

// ============================================================================
// Reverb FIR — NEON implementations (from original spu2_neon.cpp)
// ============================================================================

static constexpr int NEON_NUM_TAPS = 39;

static constexpr std::array<int16_t, 48> neon_down_coefs alignas(16) = {
	-1, 0, 2, 0, -10, 0, 35, 0,
	-103, 0, 266, 0, -616, 0, 1332, 0,
	-2960, 0, 10246, 16384, 10246, 0, -2960, 0,
	1332, 0, -616, 0, 266, 0, -103, 0,
	35, 0, -10, 0, 2, 0, -1,
};

static constexpr std::array<int16_t, 48> make_neon_up_coefs()
{
	std::array<int16_t, 48> ret = {};
	for (int i = 0; i < NEON_NUM_TAPS; i++)
	{
		ret[i] = static_cast<int16_t>(
			std::clamp<int32_t>(neon_down_coefs[i] * 2, INT16_MIN, INT16_MAX));
	}
	return ret;
}

static constexpr std::array<int16_t, 48> neon_up_coefs alignas(16) = make_neon_up_coefs();

// NEON ReverbDownsample — 39-tap FIR
static int32_t ReverbDownsample_neon(V_Core& core, bool right)
{
	const int index = (core.RevbSampleBufPos - NEON_NUM_TAPS) & 63;
	int16x8_t acc = vdupq_n_s16(0);

	int16x8_t coef, samp;

	coef = vld1q_s16(&neon_down_coefs[0]);
	samp = vld1q_s16(&core.RevbDownBuf[right][index]);
	acc = vqaddq_s16(acc, vqrdmulhq_s16(samp, coef));

	coef = vld1q_s16(&neon_down_coefs[8]);
	samp = vld1q_s16(&core.RevbDownBuf[right][index + 8]);
	acc = vqaddq_s16(acc, vqrdmulhq_s16(samp, coef));

	coef = vld1q_s16(&neon_down_coefs[16]);
	samp = vld1q_s16(&core.RevbDownBuf[right][index + 16]);
	acc = vqaddq_s16(acc, vqrdmulhq_s16(samp, coef));

	coef = vld1q_s16(&neon_down_coefs[24]);
	samp = vld1q_s16(&core.RevbDownBuf[right][index + 24]);
	acc = vqaddq_s16(acc, vqrdmulhq_s16(samp, coef));

	coef = vld1q_s16(&neon_down_coefs[32]);
	samp = vld1q_s16(&core.RevbDownBuf[right][index + 32]);
	acc = vqaddq_s16(acc, vqrdmulhq_s16(samp, coef));

	int32x4_t sum32 = vpaddlq_s16(acc);
	int32x2_t pair  = vadd_s32(vget_low_s32(sum32), vget_high_s32(sum32));
	int32_t sum     = vget_lane_s32(pair, 0) + vget_lane_s32(pair, 1);

	return clamp_mix(sum);
}

// NEON ReverbUpsample — 39-tap FIR, L/R channels
static StereoOut32 ReverbUpsample_neon(V_Core& core)
{
	const int index = (core.RevbSampleBufPos - NEON_NUM_TAPS) & 63;
	int16x8_t l_acc = vdupq_n_s16(0);
	int16x8_t r_acc = vdupq_n_s16(0);

	struct { int offset; } groups[] = {{0}, {8}, {16}, {24}, {32}};

	for (auto& g : groups)
	{
		int16x8_t coef = vld1q_s16(&neon_up_coefs[g.offset]);
		int16x8_t l_s  = vld1q_s16(&core.RevbUpBuf[0][index + g.offset]);
		int16x8_t r_s  = vld1q_s16(&core.RevbUpBuf[1][index + g.offset]);
		l_acc = vqaddq_s16(l_acc, vqrdmulhq_s16(l_s, coef));
		r_acc = vqaddq_s16(r_acc, vqrdmulhq_s16(r_s, coef));
	}

	int32x4_t l_s32 = vpaddlq_s16(l_acc);
	int32x2_t l_p   = vadd_s32(vget_low_s32(l_s32), vget_high_s32(l_s32));
	int32_t l       = vget_lane_s32(l_p, 0) + vget_lane_s32(l_p, 1);

	int32x4_t r_s32 = vpaddlq_s16(r_acc);
	int32x2_t r_p   = vadd_s32(vget_low_s32(r_s32), vget_high_s32(r_s32));
	int32_t r       = vget_lane_s32(r_p, 0) + vget_lane_s32(r_p, 1);

	return {clamp_mix(l), clamp_mix(r)};
}

// ============================================================================
// Registration — override global function pointers
// ============================================================================
//
// NOTE (ARMSX2): the original contributor build also pinned the calling thread
// to a fixed "prime" core (sched_setaffinity to CPU 0-3) and forced SCHED_FIFO.
// That is dropped here: this runs on the VM/EE thread (not the Oboe audio
// thread), the CPU-topology assumption is device-specific, and ARMSX2 already
// manages audio-thread affinity. Only the FIR pointer override is kept.
//
// SVE2 is intentionally left out of this build: it is gated by
// SPU2_HAS_SVE2_COMPILER (off on the default arm64-v8a target), and the
// contributor's SVE2 upsample coefficients overflow s16. Only the NEON FIR
// path is wired in.

namespace SPU2
{
	void RegisterNEONBackend()
	{
		bool using_sve2 = false;
#if SPU2_HAS_SVE2_COMPILER
		using_sve2 = spu2_neon::TryRegisterSVE2FIR();
#endif

		if (!using_sve2)
		{
			ReverbDownsample = ReverbDownsample_neon;
			ReverbUpsample   = ReverbUpsample_neon;
		}
	}
} // namespace SPU2

#endif // __aarch64__ || _M_ARM64