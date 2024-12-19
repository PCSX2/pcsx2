// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "IPU/IPU.h"
#include "IPU/IPU_MultiISA.h"
#include "IPU/yuv2rgb.h"

// The IPU's colour space conversion conforms to ITU-R Recommendation BT.601 if anyone wants to make a
// faster or "more accurate" implementation, but this is the precise documented integer method used by
// the hardware and is fast enough with SSE2.

#define IPU_Y_BIAS    16
#define IPU_C_BIAS    128
#define IPU_Y_COEFF   0x95	//  1.1640625
#define IPU_GCR_COEFF (-0x68)	// -0.8125
#define IPU_GCB_COEFF (-0x32)	// -0.390625
#define IPU_RCR_COEFF 0xcc	//  1.59375
#define IPU_BCB_COEFF 0x102	//  2.015625

MULTI_ISA_UNSHARED_START

// conforming implementation for reference, do not optimise
void yuv2rgb_reference(void)
{
	const macroblock_8& mb8 = decoder.mb8;
	macroblock_rgb32& rgb32 = decoder.rgb32;

	for (int y = 0; y < 16; y++)
		for (int x = 0; x < 16; x++)
		{
			s32 lum = (IPU_Y_COEFF * (std::max(0, (s32)mb8.Y[y][x] - IPU_Y_BIAS))) >> 6;
			s32 rcr = (IPU_RCR_COEFF * ((s32)mb8.Cr[y>>1][x>>1] - 128)) >> 6;
			s32 gcr = (IPU_GCR_COEFF * ((s32)mb8.Cr[y>>1][x>>1] - 128)) >> 6;
			s32 gcb = (IPU_GCB_COEFF * ((s32)mb8.Cb[y>>1][x>>1] - 128)) >> 6;
			s32 bcb = (IPU_BCB_COEFF * ((s32)mb8.Cb[y>>1][x>>1] - 128)) >> 6;

			rgb32.c[y][x].r = std::max(0, std::min(255, (lum + rcr + 1) >> 1));
			rgb32.c[y][x].g = std::max(0, std::min(255, (lum + gcr + gcb + 1) >> 1));
			rgb32.c[y][x].b = std::max(0, std::min(255, (lum + bcb + 1) >> 1));
			rgb32.c[y][x].a = 0x80; // the norm to save doing this on the alpha pass
		}
}

#if defined(_M_X86)

// Suikoden Tactics FMV speed results: Reference - ~72fps, SSE2 - ~120fps
// An AVX2 version is only slightly faster than an SSE2 version (+2-3fps)
// (or I'm a poor optimiser), though it might be worth attempting again
// once we've ported to 64 bits (the extra registers should help).
__ri void yuv2rgb_sse2()
{
	const __m128i c_bias = _mm_set1_epi8(s8(IPU_C_BIAS));
	const __m128i y_bias = _mm_set1_epi8(IPU_Y_BIAS);
	const __m128i y_mask = _mm_set1_epi16(s16(0xFF00));
	// Specifying round off instead of round down as everywhere else
	// implies that this is right
	const __m128i round_1bit = _mm_set1_epi16(0x0001);;

	const __m128i y_coefficient = _mm_set1_epi16(s16(IPU_Y_COEFF << 2));
	const __m128i gcr_coefficient = _mm_set1_epi16(s16(u16(IPU_GCR_COEFF) << 2));
	const __m128i gcb_coefficient = _mm_set1_epi16(s16(u16(IPU_GCB_COEFF) << 2));
	const __m128i rcr_coefficient = _mm_set1_epi16(s16(IPU_RCR_COEFF << 2));
	const __m128i bcb_coefficient = _mm_set1_epi16(s16(IPU_BCB_COEFF << 2));

	// Alpha set to 0x80 here. The threshold stuff is done later.
	const __m128i& alpha = c_bias;

	for (int n = 0; n < 8; ++n) {
		// could skip the loadl_epi64 but most SSE instructions require 128-bit
		// alignment so two versions would be needed.
		__m128i cb = _mm_loadl_epi64(reinterpret_cast<__m128i*>(&decoder.mb8.Cb[n][0]));
		__m128i cr = _mm_loadl_epi64(reinterpret_cast<__m128i*>(&decoder.mb8.Cr[n][0]));

		// (Cb - 128) << 8, (Cr - 128) << 8
		cb = _mm_xor_si128(cb, c_bias);
		cr = _mm_xor_si128(cr, c_bias);
		cb = _mm_unpacklo_epi8(_mm_setzero_si128(), cb);
		cr = _mm_unpacklo_epi8(_mm_setzero_si128(), cr);

		__m128i rc = _mm_mulhi_epi16(cr, rcr_coefficient);
		__m128i gc = _mm_adds_epi16(_mm_mulhi_epi16(cr, gcr_coefficient), _mm_mulhi_epi16(cb, gcb_coefficient));
		__m128i bc = _mm_mulhi_epi16(cb, bcb_coefficient);

		for (int m = 0; m < 2; ++m) {
			__m128i y = _mm_load_si128(reinterpret_cast<__m128i*>(&decoder.mb8.Y[n * 2 + m][0]));
			y = _mm_subs_epu8(y, y_bias);
			// Y << 8 for pixels 0, 2, 4, 6, 8, 10, 12, 14
			__m128i y_even = _mm_slli_epi16(y, 8);
			// Y << 8 for pixels 1, 3, 5, 7 ,9, 11, 13, 15
			__m128i y_odd = _mm_and_si128(y, y_mask);

			y_even = _mm_mulhi_epu16(y_even, y_coefficient);
			y_odd  = _mm_mulhi_epu16(y_odd,  y_coefficient);

			__m128i r_even = _mm_adds_epi16(rc, y_even);
			__m128i r_odd  = _mm_adds_epi16(rc, y_odd);
			__m128i g_even = _mm_adds_epi16(gc, y_even);
			__m128i g_odd  = _mm_adds_epi16(gc, y_odd);
			__m128i b_even = _mm_adds_epi16(bc, y_even);
			__m128i b_odd  = _mm_adds_epi16(bc, y_odd);

			// round
			r_even = _mm_srai_epi16(_mm_add_epi16(r_even, round_1bit), 1);
			r_odd  = _mm_srai_epi16(_mm_add_epi16(r_odd,  round_1bit), 1);
			g_even = _mm_srai_epi16(_mm_add_epi16(g_even, round_1bit), 1);
			g_odd  = _mm_srai_epi16(_mm_add_epi16(g_odd,  round_1bit), 1);
			b_even = _mm_srai_epi16(_mm_add_epi16(b_even, round_1bit), 1);
			b_odd  = _mm_srai_epi16(_mm_add_epi16(b_odd,  round_1bit), 1);

			// combine even and odd bytes in original order
			__m128i r = _mm_packus_epi16(r_even, r_odd);
			__m128i g = _mm_packus_epi16(g_even, g_odd);
			__m128i b = _mm_packus_epi16(b_even, b_odd);

			r = _mm_unpacklo_epi8(r, _mm_shuffle_epi32(r, _MM_SHUFFLE(3, 2, 3, 2)));
			g = _mm_unpacklo_epi8(g, _mm_shuffle_epi32(g, _MM_SHUFFLE(3, 2, 3, 2)));
			b = _mm_unpacklo_epi8(b, _mm_shuffle_epi32(b, _MM_SHUFFLE(3, 2, 3, 2)));

			// Create RGBA (we could generate A here, but we don't) quads
			__m128i rg_l = _mm_unpacklo_epi8(r, g);
			__m128i ba_l = _mm_unpacklo_epi8(b, alpha);
			__m128i rgba_ll = _mm_unpacklo_epi16(rg_l, ba_l);
			__m128i rgba_lh = _mm_unpackhi_epi16(rg_l, ba_l);

			__m128i rg_h = _mm_unpackhi_epi8(r, g);
			__m128i ba_h = _mm_unpackhi_epi8(b, alpha);
			__m128i rgba_hl = _mm_unpacklo_epi16(rg_h, ba_h);
			__m128i rgba_hh = _mm_unpackhi_epi16(rg_h, ba_h);

			_mm_store_si128(reinterpret_cast<__m128i*>(&decoder.rgb32.c[n * 2 + m][0]), rgba_ll);
			_mm_store_si128(reinterpret_cast<__m128i*>(&decoder.rgb32.c[n * 2 + m][4]), rgba_lh);
			_mm_store_si128(reinterpret_cast<__m128i*>(&decoder.rgb32.c[n * 2 + m][8]), rgba_hl);
			_mm_store_si128(reinterpret_cast<__m128i*>(&decoder.rgb32.c[n * 2 + m][12]), rgba_hh);
		}
	}
}

#elif defined(_M_ARM64)

#if defined(_MSC_VER) && !defined(__clang__)
#include <arm64_neon.h>
#else
#include <arm_neon.h>
#endif

#define MULHI16(a, b) vshrq_n_s16(vqdmulhq_s16((a), (b)), 1)

__ri void yuv2rgb_neon()
{
	const int8x16_t c_bias = vdupq_n_s8(s8(IPU_C_BIAS));
	const uint8x16_t y_bias = vdupq_n_u8(IPU_Y_BIAS);
	const int16x8_t y_mask = vdupq_n_s16(s16(0xFF00));
	// Specifying round off instead of round down as everywhere else
	// implies that this is right
	const int16x8_t round_1bit = vdupq_n_s16(0x0001);
	;

	const int16x8_t y_coefficient = vdupq_n_s16(s16(IPU_Y_COEFF << 2));
	const int16x8_t gcr_coefficient = vdupq_n_s16(s16(u16(IPU_GCR_COEFF) << 2));
	const int16x8_t gcb_coefficient = vdupq_n_s16(s16(u16(IPU_GCB_COEFF) << 2));
	const int16x8_t rcr_coefficient = vdupq_n_s16(s16(IPU_RCR_COEFF << 2));
	const int16x8_t bcb_coefficient = vdupq_n_s16(s16(IPU_BCB_COEFF << 2));

	// Alpha set to 0x80 here. The threshold stuff is done later.
	const uint8x16_t alpha = vreinterpretq_u8_s8(c_bias);

	for (int n = 0; n < 8; ++n)
	{
		// could skip the loadl_epi64 but most SSE instructions require 128-bit
		// alignment so two versions would be needed.
		int8x16_t cb = vcombine_s8(vld1_s8(reinterpret_cast<s8*>(&decoder.mb8.Cb[n][0])), vdup_n_s8(0));
		int8x16_t cr = vcombine_s8(vld1_s8(reinterpret_cast<s8*>(&decoder.mb8.Cr[n][0])), vdup_n_s8(0));

		// (Cb - 128) << 8, (Cr - 128) << 8
		cb = veorq_s8(cb, c_bias);
		cr = veorq_s8(cr, c_bias);
		cb = vzip1q_s8(vdupq_n_s8(0), cb);
		cr = vzip1q_s8(vdupq_n_s8(0), cr);

		int16x8_t rc = MULHI16(vreinterpretq_s16_s8(cr), rcr_coefficient);
		int16x8_t gc = vqaddq_s16(MULHI16(vreinterpretq_s16_s8(cr), gcr_coefficient), MULHI16(vreinterpretq_s16_s8(cb), gcb_coefficient));
		int16x8_t bc = MULHI16(vreinterpretq_s16_s8(cb), bcb_coefficient);

		for (int m = 0; m < 2; ++m)
		{
			uint8x16_t y = vld1q_u8(&decoder.mb8.Y[n * 2 + m][0]);
			y = vqsubq_u8(y, y_bias);
			// Y << 8 for pixels 0, 2, 4, 6, 8, 10, 12, 14
			int16x8_t y_even = vshlq_n_s16(vreinterpretq_s16_u8(y), 8);
			// Y << 8 for pixels 1, 3, 5, 7 ,9, 11, 13, 15
			int16x8_t y_odd = vandq_s16(vreinterpretq_s16_u8(y), y_mask);

			// y_even = _mm_mulhi_epu16(y_even, y_coefficient);
			// y_odd = _mm_mulhi_epu16(y_odd, y_coefficient);

			uint16x4_t a3210 = vget_low_u16(vreinterpretq_u16_s16(y_even));
			uint16x4_t b3210 = vget_low_u16(vreinterpretq_u16_s16(y_coefficient));
			uint32x4_t ab3210 = vmull_u16(a3210, b3210);
			uint32x4_t ab7654 = vmull_high_u16(vreinterpretq_u16_s16(y_even), vreinterpretq_u16_s16(y_coefficient));
			y_even = vreinterpretq_s16_u16(vuzp2q_u16(vreinterpretq_u16_u32(ab3210), vreinterpretq_u16_u32(ab7654)));

			a3210 = vget_low_u16(vreinterpretq_u16_s16(y_odd));
			b3210 = vget_low_u16(vreinterpretq_u16_s16(y_coefficient));
			ab3210 = vmull_u16(a3210, b3210);
			ab7654 = vmull_high_u16(vreinterpretq_u16_s16(y_odd), vreinterpretq_u16_s16(y_coefficient));
			y_odd = vreinterpretq_s16_u16(vuzp2q_u16(vreinterpretq_u16_u32(ab3210), vreinterpretq_u16_u32(ab7654)));

			int16x8_t r_even = vqaddq_s16(rc, y_even);
			int16x8_t r_odd = vqaddq_s16(rc, y_odd);
			int16x8_t g_even = vqaddq_s16(gc, y_even);
			int16x8_t g_odd = vqaddq_s16(gc, y_odd);
			int16x8_t b_even = vqaddq_s16(bc, y_even);
			int16x8_t b_odd = vqaddq_s16(bc, y_odd);

			// round
			r_even = vshrq_n_s16(vaddq_s16(r_even, round_1bit), 1);
			r_odd = vshrq_n_s16(vaddq_s16(r_odd, round_1bit), 1);
			g_even = vshrq_n_s16(vaddq_s16(g_even, round_1bit), 1);
			g_odd = vshrq_n_s16(vaddq_s16(g_odd, round_1bit), 1);
			b_even = vshrq_n_s16(vaddq_s16(b_even, round_1bit), 1);
			b_odd = vshrq_n_s16(vaddq_s16(b_odd, round_1bit), 1);

			// combine even and odd bytes in original order
			uint8x16_t r = vcombine_u8(vqmovun_s16(r_even), vqmovun_s16(r_odd));
			uint8x16_t g = vcombine_u8(vqmovun_s16(g_even), vqmovun_s16(g_odd));
			uint8x16_t b = vcombine_u8(vqmovun_s16(b_even), vqmovun_s16(b_odd));

			r = vzip1q_u8(r, vreinterpretq_u8_u64(vdupq_laneq_u64(vreinterpretq_u64_u8(r), 1)));
			g = vzip1q_u8(g, vreinterpretq_u8_u64(vdupq_laneq_u64(vreinterpretq_u64_u8(g), 1)));
			b = vzip1q_u8(b, vreinterpretq_u8_u64(vdupq_laneq_u64(vreinterpretq_u64_u8(b), 1)));

			// Create RGBA (we could generate A here, but we don't) quads
			uint8x16_t rg_l = vzip1q_u8(r, g);
			uint8x16_t ba_l = vzip1q_u8(b, alpha);
			uint16x8_t rgba_ll = vzip1q_u16(vreinterpretq_u16_u8(rg_l), vreinterpretq_u16_u8(ba_l));
			uint16x8_t rgba_lh = vzip2q_u16(vreinterpretq_u16_u8(rg_l), vreinterpretq_u16_u8(ba_l));

			uint8x16_t rg_h = vzip2q_u8(r, g);
			uint8x16_t ba_h = vzip2q_u8(b, alpha);
			uint16x8_t rgba_hl = vzip1q_u16(vreinterpretq_u16_u8(rg_h), vreinterpretq_u16_u8(ba_h));
			uint16x8_t rgba_hh = vzip2q_u16(vreinterpretq_u16_u8(rg_h), vreinterpretq_u16_u8(ba_h));

			vst1q_u8(reinterpret_cast<u8*>(&decoder.rgb32.c[n * 2 + m][0]), vreinterpretq_u8_u16(rgba_ll));
			vst1q_u8(reinterpret_cast<u8*>(&decoder.rgb32.c[n * 2 + m][4]), vreinterpretq_u8_u16(rgba_lh));
			vst1q_u8(reinterpret_cast<u8*>(&decoder.rgb32.c[n * 2 + m][8]), vreinterpretq_u8_u16(rgba_hl));
			vst1q_u8(reinterpret_cast<u8*>(&decoder.rgb32.c[n * 2 + m][12]), vreinterpretq_u8_u16(rgba_hh));
		}
	}
}

#undef MULHI16

#endif

MULTI_ISA_UNSHARED_END
