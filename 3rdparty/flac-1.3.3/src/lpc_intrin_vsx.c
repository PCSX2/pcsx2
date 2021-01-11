/* libFLAC - Free Lossless Audio Codec library
 * Copyright (C) 2000-2009  Josh Coalson
 * Copyright (C) 2011-2016  Xiph.Org Foundation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the Xiph.org Foundation nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef FLAC__INTEGER_ONLY_LIBRARY
#ifndef FLAC__NO_ASM
#if defined(FLAC__CPU_PPC64) && defined(FLAC__USE_VSX)

#include "private/cpu.h"
#include "private/lpc.h"
#include "FLAC/assert.h"
#include "FLAC/format.h"

#include <altivec.h>

#ifdef FLAC__HAS_TARGET_POWER8
__attribute__((target("cpu=power8")))
void FLAC__lpc_compute_autocorrelation_intrin_power8_vsx_lag_16(const FLAC__real data[], uint32_t data_len, uint32_t lag, FLAC__real autoc[])
{
	long i;
	long limit = (long)data_len - 16;
	const FLAC__real *base;
	vector float sum0 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum1 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum2 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum3 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum10 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum11 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum12 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum13 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum20 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum21 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum22 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum23 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum30 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum31 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum32 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum33 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float d0, d1, d2, d3, d4;
#if WORDS_BIGENDIAN
	vector unsigned int vsel1 = { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF };
	vector unsigned int vsel2 = { 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vsel3 = { 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vperm1 = { 0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213 };
	vector unsigned int vperm2 = { 0x08090A0B, 0x0C0D0E0F, 0x10111213, 0x14151617 };
	vector unsigned int vperm3 = { 0x0C0D0E0F, 0x10111213, 0x14151617, 0x18191A1B };
#else
	vector unsigned int vsel1 = { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 };
	vector unsigned int vsel2 = { 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000 };
	vector unsigned int vsel3 = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 };
	vector unsigned int vperm1 = { 0x07060504, 0x0B0A0908, 0x0F0E0D0C, 0x13121110 };
	vector unsigned int vperm2 = { 0x0B0A0908, 0x0F0E0D0C, 0x13121110, 0x17161514 };
	vector unsigned int vperm3 = { 0x0F0E0D0C, 0x13121110, 0x17161514, 0x1B1A1918 };
#endif

	(void) lag;
	FLAC__ASSERT(lag <= 16);
	FLAC__ASSERT(lag <= data_len);

	base = data;

	d0 = vec_vsx_ld(0, base);
	d1 = vec_vsx_ld(16, base);
	d2 = vec_vsx_ld(32, base);
	d3 = vec_vsx_ld(48, base);

	base += 16;

	for (i = 0; i <= (limit-4); i += 4) {
		vector float d, d0_orig = d0;

		d4 = vec_vsx_ld(0, base);
		base += 4;

		d = vec_splat(d0_orig, 0);
		sum0 += d0 * d;
		sum1 += d1 * d;
		sum2 += d2 * d;
		sum3 += d3 * d;

		d = vec_splat(d0_orig, 1);
		d0 = vec_sel(d0_orig, d4, vsel1);
		sum10 += d0 * d;
		sum11 += d1 * d;
		sum12 += d2 * d;
		sum13 += d3 * d;

		d = vec_splat(d0_orig, 2);
		d0 = vec_sel(d0_orig, d4, vsel2);
		sum20 += d0 * d;
		sum21 += d1 * d;
		sum22 += d2 * d;
		sum23 += d3 * d;

		d = vec_splat(d0_orig, 3);
		d0 = vec_sel(d0_orig, d4, vsel3);
		sum30 += d0 * d;
		sum31 += d1 * d;
		sum32 += d2 * d;
		sum33 += d3 * d;

		d0 = d1;
		d1 = d2;
		d2 = d3;
		d3 = d4;
	}

	sum0 += vec_perm(sum10, sum11, (vector unsigned char)vperm1);
	sum1 += vec_perm(sum11, sum12, (vector unsigned char)vperm1);
	sum2 += vec_perm(sum12, sum13, (vector unsigned char)vperm1);
	sum3 += vec_perm(sum13, sum10, (vector unsigned char)vperm1);

	sum0 += vec_perm(sum20, sum21, (vector unsigned char)vperm2);
	sum1 += vec_perm(sum21, sum22, (vector unsigned char)vperm2);
	sum2 += vec_perm(sum22, sum23, (vector unsigned char)vperm2);
	sum3 += vec_perm(sum23, sum20, (vector unsigned char)vperm2);

	sum0 += vec_perm(sum30, sum31, (vector unsigned char)vperm3);
	sum1 += vec_perm(sum31, sum32, (vector unsigned char)vperm3);
	sum2 += vec_perm(sum32, sum33, (vector unsigned char)vperm3);
	sum3 += vec_perm(sum33, sum30, (vector unsigned char)vperm3);

	for (; i <= limit; i++) {
		vector float d;

		d0 = vec_vsx_ld(0, data+i);
		d1 = vec_vsx_ld(16, data+i);
		d2 = vec_vsx_ld(32, data+i);
		d3 = vec_vsx_ld(48, data+i);

		d = vec_splat(d0, 0);
		sum0 += d0 * d;
		sum1 += d1 * d;
		sum2 += d2 * d;
		sum3 += d3 * d;
	}

	vec_vsx_st(sum0, 0, autoc);
	vec_vsx_st(sum1, 16, autoc);
	vec_vsx_st(sum2, 32, autoc);
	vec_vsx_st(sum3, 48, autoc);

	for (; i < (long)data_len; i++) {
		uint32_t coeff;

		FLAC__real d = data[i];
		for (coeff = 0; coeff < data_len - i; coeff++)
			autoc[coeff] += d * data[i+coeff];
	}
}

__attribute__((target("cpu=power8")))
void FLAC__lpc_compute_autocorrelation_intrin_power8_vsx_lag_12(const FLAC__real data[], uint32_t data_len, uint32_t lag, FLAC__real autoc[])
{
	long i;
	long limit = (long)data_len - 12;
	const FLAC__real *base;
	vector float sum0 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum1 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum2 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum10 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum11 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum12 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum20 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum21 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum22 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum30 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum31 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum32 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float d0, d1, d2, d3;
#if WORDS_BIGENDIAN
	vector unsigned int vsel1 = { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF };
	vector unsigned int vsel2 = { 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vsel3 = { 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vperm1 = { 0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213 };
	vector unsigned int vperm2 = { 0x08090A0B, 0x0C0D0E0F, 0x10111213, 0x14151617 };
	vector unsigned int vperm3 = { 0x0C0D0E0F, 0x10111213, 0x14151617, 0x18191A1B };
#else
	vector unsigned int vsel1 = { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 };
	vector unsigned int vsel2 = { 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000 };
	vector unsigned int vsel3 = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 };
	vector unsigned int vperm1 = { 0x07060504, 0x0B0A0908, 0x0F0E0D0C, 0x13121110 };
	vector unsigned int vperm2 = { 0x0B0A0908, 0x0F0E0D0C, 0x13121110, 0x17161514 };
	vector unsigned int vperm3 = { 0x0F0E0D0C, 0x13121110, 0x17161514, 0x1B1A1918 };
#endif

	(void) lag;
	FLAC__ASSERT(lag <= 12);
	FLAC__ASSERT(lag <= data_len);

	base = data;

	d0 = vec_vsx_ld(0, base);
	d1 = vec_vsx_ld(16, base);
	d2 = vec_vsx_ld(32, base);

	base += 12;

	for (i = 0; i <= (limit-3); i += 4) {
		vector float d, d0_orig = d0;

		d3 = vec_vsx_ld(0, base);
		base += 4;

		d = vec_splat(d0_orig, 0);
		sum0 += d0 * d;
		sum1 += d1 * d;
		sum2 += d2 * d;

		d = vec_splat(d0_orig, 1);
		d0 = vec_sel(d0_orig, d3, vsel1);
		sum10 += d0 * d;
		sum11 += d1 * d;
		sum12 += d2 * d;

		d = vec_splat(d0_orig, 2);
		d0 = vec_sel(d0_orig, d3, vsel2);
		sum20 += d0 * d;
		sum21 += d1 * d;
		sum22 += d2 * d;

		d = vec_splat(d0_orig, 3);
		d0 = vec_sel(d0_orig, d3, vsel3);
		sum30 += d0 * d;
		sum31 += d1 * d;
		sum32 += d2 * d;

		d0 = d1;
		d1 = d2;
		d2 = d3;
	}

	sum0 += vec_perm(sum10, sum11, (vector unsigned char)vperm1);
	sum1 += vec_perm(sum11, sum12, (vector unsigned char)vperm1);
	sum2 += vec_perm(sum12, sum10, (vector unsigned char)vperm1);

	sum0 += vec_perm(sum20, sum21, (vector unsigned char)vperm2);
	sum1 += vec_perm(sum21, sum22, (vector unsigned char)vperm2);
	sum2 += vec_perm(sum22, sum20, (vector unsigned char)vperm2);

	sum0 += vec_perm(sum30, sum31, (vector unsigned char)vperm3);
	sum1 += vec_perm(sum31, sum32, (vector unsigned char)vperm3);
	sum2 += vec_perm(sum32, sum30, (vector unsigned char)vperm3);

	for (; i <= limit; i++) {
		vector float d;

		d0 = vec_vsx_ld(0, data+i);
		d1 = vec_vsx_ld(16, data+i);
		d2 = vec_vsx_ld(32, data+i);

		d = vec_splat(d0, 0);
		sum0 += d0 * d;
		sum1 += d1 * d;
		sum2 += d2 * d;
	}

	vec_vsx_st(sum0, 0, autoc);
	vec_vsx_st(sum1, 16, autoc);
	vec_vsx_st(sum2, 32, autoc);

	for (; i < (long)data_len; i++) {
		uint32_t coeff;

		FLAC__real d = data[i];
		for (coeff = 0; coeff < data_len - i; coeff++)
			autoc[coeff] += d * data[i+coeff];
	}
}

__attribute__((target("cpu=power8")))
void FLAC__lpc_compute_autocorrelation_intrin_power8_vsx_lag_8(const FLAC__real data[], uint32_t data_len, uint32_t lag, FLAC__real autoc[])
{
	long i;
	long limit = (long)data_len - 8;
	const FLAC__real *base;
	vector float sum0 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum1 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum10 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum11 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum20 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum21 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum30 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum31 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float d0, d1, d2;
#if WORDS_BIGENDIAN
	vector unsigned int vsel1 = { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF };
	vector unsigned int vsel2 = { 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vsel3 = { 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vperm1 = { 0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213 };
	vector unsigned int vperm2 = { 0x08090A0B, 0x0C0D0E0F, 0x10111213, 0x14151617 };
	vector unsigned int vperm3 = { 0x0C0D0E0F, 0x10111213, 0x14151617, 0x18191A1B };
#else
	vector unsigned int vsel1 = { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 };
	vector unsigned int vsel2 = { 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000 };
	vector unsigned int vsel3 = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 };
	vector unsigned int vperm1 = { 0x07060504, 0x0B0A0908, 0x0F0E0D0C, 0x13121110 };
	vector unsigned int vperm2 = { 0x0B0A0908, 0x0F0E0D0C, 0x13121110, 0x17161514 };
	vector unsigned int vperm3 = { 0x0F0E0D0C, 0x13121110, 0x17161514, 0x1B1A1918 };
#endif

	(void) lag;
	FLAC__ASSERT(lag <= 8);
	FLAC__ASSERT(lag <= data_len);

	base = data;

	d0 = vec_vsx_ld(0, base);
	d1 = vec_vsx_ld(16, base);

	base += 8;

	for (i = 0; i <= (limit-2); i += 4) {
		vector float d, d0_orig = d0;

		d2 = vec_vsx_ld(0, base);
		base += 4;

		d = vec_splat(d0_orig, 0);
		sum0 += d0 * d;
		sum1 += d1 * d;

		d = vec_splat(d0_orig, 1);
		d0 = vec_sel(d0_orig, d2, vsel1);
		sum10 += d0 * d;
		sum11 += d1 * d;

		d = vec_splat(d0_orig, 2);
		d0 = vec_sel(d0_orig, d2, vsel2);
		sum20 += d0 * d;
		sum21 += d1 * d;

		d = vec_splat(d0_orig, 3);
		d0 = vec_sel(d0_orig, d2, vsel3);
		sum30 += d0 * d;
		sum31 += d1 * d;

		d0 = d1;
		d1 = d2;
	}

	sum0 += vec_perm(sum10, sum11, (vector unsigned char)vperm1);
	sum1 += vec_perm(sum11, sum10, (vector unsigned char)vperm1);

	sum0 += vec_perm(sum20, sum21, (vector unsigned char)vperm2);
	sum1 += vec_perm(sum21, sum20, (vector unsigned char)vperm2);

	sum0 += vec_perm(sum30, sum31, (vector unsigned char)vperm3);
	sum1 += vec_perm(sum31, sum30, (vector unsigned char)vperm3);

	for (; i <= limit; i++) {
		vector float d;

		d0 = vec_vsx_ld(0, data+i);
		d1 = vec_vsx_ld(16, data+i);

		d = vec_splat(d0, 0);
		sum0 += d0 * d;
		sum1 += d1 * d;
	}

	vec_vsx_st(sum0, 0, autoc);
	vec_vsx_st(sum1, 16, autoc);

	for (; i < (long)data_len; i++) {
		uint32_t coeff;

		FLAC__real d = data[i];
		for (coeff = 0; coeff < data_len - i; coeff++)
			autoc[coeff] += d * data[i+coeff];
	}
}

__attribute__((target("cpu=power8")))
void FLAC__lpc_compute_autocorrelation_intrin_power8_vsx_lag_4(const FLAC__real data[], uint32_t data_len, uint32_t lag, FLAC__real autoc[])
{
	long i;
	long limit = (long)data_len - 4;
	const FLAC__real *base;
	vector float sum0 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum10 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum20 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum30 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float d0, d1;
#if WORDS_BIGENDIAN
	vector unsigned int vsel1 = { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF };
	vector unsigned int vsel2 = { 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vsel3 = { 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vperm1 = { 0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213 };
	vector unsigned int vperm2 = { 0x08090A0B, 0x0C0D0E0F, 0x10111213, 0x14151617 };
	vector unsigned int vperm3 = { 0x0C0D0E0F, 0x10111213, 0x14151617, 0x18191A1B };
#else
	vector unsigned int vsel1 = { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 };
	vector unsigned int vsel2 = { 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000 };
	vector unsigned int vsel3 = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 };
	vector unsigned int vperm1 = { 0x07060504, 0x0B0A0908, 0x0F0E0D0C, 0x13121110 };
	vector unsigned int vperm2 = { 0x0B0A0908, 0x0F0E0D0C, 0x13121110, 0x17161514 };
	vector unsigned int vperm3 = { 0x0F0E0D0C, 0x13121110, 0x17161514, 0x1B1A1918 };
#endif

	(void) lag;
	FLAC__ASSERT(lag <= 4);
	FLAC__ASSERT(lag <= data_len);

	base = data;

	d0 = vec_vsx_ld(0, base);

	base += 4;

	for (i = 0; i <= (limit-1); i += 4) {
		vector float d, d0_orig = d0;

		d1 = vec_vsx_ld(0, base);
		base += 4;

		d = vec_splat(d0_orig, 0);
		sum0 += d0 * d;

		d = vec_splat(d0_orig, 1);
		d0 = vec_sel(d0_orig, d1, vsel1);
		sum10 += d0 * d;

		d = vec_splat(d0_orig, 2);
		d0 = vec_sel(d0_orig, d1, vsel2);
		sum20 += d0 * d;

		d = vec_splat(d0_orig, 3);
		d0 = vec_sel(d0_orig, d1, vsel3);
		sum30 += d0 * d;

		d0 = d1;
	}

	sum0 += vec_perm(sum10, sum10, (vector unsigned char)vperm1);

	sum0 += vec_perm(sum20, sum20, (vector unsigned char)vperm2);

	sum0 += vec_perm(sum30, sum30, (vector unsigned char)vperm3);

	for (; i <= limit; i++) {
		vector float d;

		d0 = vec_vsx_ld(0, data+i);

		d = vec_splat(d0, 0);
		sum0 += d0 * d;
	}

	vec_vsx_st(sum0, 0, autoc);

	for (; i < (long)data_len; i++) {
		uint32_t coeff;

		FLAC__real d = data[i];
		for (coeff = 0; coeff < data_len - i; coeff++)
			autoc[coeff] += d * data[i+coeff];
	}
}
#endif /* FLAC__HAS_TARGET_POWER8 */

#ifdef FLAC__HAS_TARGET_POWER9
__attribute__((target("cpu=power9")))
void FLAC__lpc_compute_autocorrelation_intrin_power9_vsx_lag_16(const FLAC__real data[], uint32_t data_len, uint32_t lag, FLAC__real autoc[])
{
	long i;
	long limit = (long)data_len - 16;
	const FLAC__real *base;
	vector float sum0 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum1 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum2 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum3 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum10 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum11 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum12 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum13 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum20 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum21 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum22 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum23 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum30 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum31 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum32 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum33 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float d0, d1, d2, d3, d4;
#if WORDS_BIGENDIAN
	vector unsigned int vsel1 = { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF };
	vector unsigned int vsel2 = { 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vsel3 = { 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vperm1 = { 0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213 };
	vector unsigned int vperm2 = { 0x08090A0B, 0x0C0D0E0F, 0x10111213, 0x14151617 };
	vector unsigned int vperm3 = { 0x0C0D0E0F, 0x10111213, 0x14151617, 0x18191A1B };
#else
	vector unsigned int vsel1 = { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 };
	vector unsigned int vsel2 = { 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000 };
	vector unsigned int vsel3 = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 };
	vector unsigned int vperm1 = { 0x07060504, 0x0B0A0908, 0x0F0E0D0C, 0x13121110 };
	vector unsigned int vperm2 = { 0x0B0A0908, 0x0F0E0D0C, 0x13121110, 0x17161514 };
	vector unsigned int vperm3 = { 0x0F0E0D0C, 0x13121110, 0x17161514, 0x1B1A1918 };
#endif

	(void) lag;
	FLAC__ASSERT(lag <= 16);
	FLAC__ASSERT(lag <= data_len);

	base = data;

	d0 = vec_vsx_ld(0, base);
	d1 = vec_vsx_ld(16, base);
	d2 = vec_vsx_ld(32, base);
	d3 = vec_vsx_ld(48, base);

	base += 16;

	for (i = 0; i <= (limit-4); i += 4) {
		vector float d, d0_orig = d0;

		d4 = vec_vsx_ld(0, base);
		base += 4;

		d = vec_splat(d0_orig, 0);
		sum0 += d0 * d;
		sum1 += d1 * d;
		sum2 += d2 * d;
		sum3 += d3 * d;

		d = vec_splat(d0_orig, 1);
		d0 = vec_sel(d0_orig, d4, vsel1);
		sum10 += d0 * d;
		sum11 += d1 * d;
		sum12 += d2 * d;
		sum13 += d3 * d;

		d = vec_splat(d0_orig, 2);
		d0 = vec_sel(d0_orig, d4, vsel2);
		sum20 += d0 * d;
		sum21 += d1 * d;
		sum22 += d2 * d;
		sum23 += d3 * d;

		d = vec_splat(d0_orig, 3);
		d0 = vec_sel(d0_orig, d4, vsel3);
		sum30 += d0 * d;
		sum31 += d1 * d;
		sum32 += d2 * d;
		sum33 += d3 * d;

		d0 = d1;
		d1 = d2;
		d2 = d3;
		d3 = d4;
	}

	sum0 += vec_perm(sum10, sum11, (vector unsigned char)vperm1);
	sum1 += vec_perm(sum11, sum12, (vector unsigned char)vperm1);
	sum2 += vec_perm(sum12, sum13, (vector unsigned char)vperm1);
	sum3 += vec_perm(sum13, sum10, (vector unsigned char)vperm1);

	sum0 += vec_perm(sum20, sum21, (vector unsigned char)vperm2);
	sum1 += vec_perm(sum21, sum22, (vector unsigned char)vperm2);
	sum2 += vec_perm(sum22, sum23, (vector unsigned char)vperm2);
	sum3 += vec_perm(sum23, sum20, (vector unsigned char)vperm2);

	sum0 += vec_perm(sum30, sum31, (vector unsigned char)vperm3);
	sum1 += vec_perm(sum31, sum32, (vector unsigned char)vperm3);
	sum2 += vec_perm(sum32, sum33, (vector unsigned char)vperm3);
	sum3 += vec_perm(sum33, sum30, (vector unsigned char)vperm3);

	for (; i <= limit; i++) {
		vector float d;

		d0 = vec_vsx_ld(0, data+i);
		d1 = vec_vsx_ld(16, data+i);
		d2 = vec_vsx_ld(32, data+i);
		d3 = vec_vsx_ld(48, data+i);

		d = vec_splat(d0, 0);
		sum0 += d0 * d;
		sum1 += d1 * d;
		sum2 += d2 * d;
		sum3 += d3 * d;
	}

	vec_vsx_st(sum0, 0, autoc);
	vec_vsx_st(sum1, 16, autoc);
	vec_vsx_st(sum2, 32, autoc);
	vec_vsx_st(sum3, 48, autoc);

	for (; i < (long)data_len; i++) {
		uint32_t coeff;

		FLAC__real d = data[i];
		for (coeff = 0; coeff < data_len - i; coeff++)
			autoc[coeff] += d * data[i+coeff];
	}
}

__attribute__((target("cpu=power9")))
void FLAC__lpc_compute_autocorrelation_intrin_power9_vsx_lag_12(const FLAC__real data[], uint32_t data_len, uint32_t lag, FLAC__real autoc[])
{
	long i;
	long limit = (long)data_len - 12;
	const FLAC__real *base;
	vector float sum0 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum1 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum2 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum10 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum11 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum12 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum20 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum21 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum22 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum30 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum31 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum32 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float d0, d1, d2, d3;
#if WORDS_BIGENDIAN
	vector unsigned int vsel1 = { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF };
	vector unsigned int vsel2 = { 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vsel3 = { 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vperm1 = { 0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213 };
	vector unsigned int vperm2 = { 0x08090A0B, 0x0C0D0E0F, 0x10111213, 0x14151617 };
	vector unsigned int vperm3 = { 0x0C0D0E0F, 0x10111213, 0x14151617, 0x18191A1B };
#else
	vector unsigned int vsel1 = { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 };
	vector unsigned int vsel2 = { 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000 };
	vector unsigned int vsel3 = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 };
	vector unsigned int vperm1 = { 0x07060504, 0x0B0A0908, 0x0F0E0D0C, 0x13121110 };
	vector unsigned int vperm2 = { 0x0B0A0908, 0x0F0E0D0C, 0x13121110, 0x17161514 };
	vector unsigned int vperm3 = { 0x0F0E0D0C, 0x13121110, 0x17161514, 0x1B1A1918 };
#endif

	(void) lag;
	FLAC__ASSERT(lag <= 12);
	FLAC__ASSERT(lag <= data_len);

	base = data;

	d0 = vec_vsx_ld(0, base);
	d1 = vec_vsx_ld(16, base);
	d2 = vec_vsx_ld(32, base);

	base += 12;

	for (i = 0; i <= (limit-3); i += 4) {
		vector float d, d0_orig = d0;

		d3 = vec_vsx_ld(0, base);
		base += 4;

		d = vec_splat(d0_orig, 0);
		sum0 += d0 * d;
		sum1 += d1 * d;
		sum2 += d2 * d;

		d = vec_splat(d0_orig, 1);
		d0 = vec_sel(d0_orig, d3, vsel1);
		sum10 += d0 * d;
		sum11 += d1 * d;
		sum12 += d2 * d;

		d = vec_splat(d0_orig, 2);
		d0 = vec_sel(d0_orig, d3, vsel2);
		sum20 += d0 * d;
		sum21 += d1 * d;
		sum22 += d2 * d;

		d = vec_splat(d0_orig, 3);
		d0 = vec_sel(d0_orig, d3, vsel3);
		sum30 += d0 * d;
		sum31 += d1 * d;
		sum32 += d2 * d;

		d0 = d1;
		d1 = d2;
		d2 = d3;
	}

	sum0 += vec_perm(sum10, sum11, (vector unsigned char)vperm1);
	sum1 += vec_perm(sum11, sum12, (vector unsigned char)vperm1);
	sum2 += vec_perm(sum12, sum10, (vector unsigned char)vperm1);

	sum0 += vec_perm(sum20, sum21, (vector unsigned char)vperm2);
	sum1 += vec_perm(sum21, sum22, (vector unsigned char)vperm2);
	sum2 += vec_perm(sum22, sum20, (vector unsigned char)vperm2);

	sum0 += vec_perm(sum30, sum31, (vector unsigned char)vperm3);
	sum1 += vec_perm(sum31, sum32, (vector unsigned char)vperm3);
	sum2 += vec_perm(sum32, sum30, (vector unsigned char)vperm3);

	for (; i <= limit; i++) {
		vector float d;

		d0 = vec_vsx_ld(0, data+i);
		d1 = vec_vsx_ld(16, data+i);
		d2 = vec_vsx_ld(32, data+i);

		d = vec_splat(d0, 0);
		sum0 += d0 * d;
		sum1 += d1 * d;
		sum2 += d2 * d;
	}

	vec_vsx_st(sum0, 0, autoc);
	vec_vsx_st(sum1, 16, autoc);
	vec_vsx_st(sum2, 32, autoc);

	for (; i < (long)data_len; i++) {
		uint32_t coeff;

		FLAC__real d = data[i];
		for (coeff = 0; coeff < data_len - i; coeff++)
			autoc[coeff] += d * data[i+coeff];
	}
}

__attribute__((target("cpu=power9")))
void FLAC__lpc_compute_autocorrelation_intrin_power9_vsx_lag_8(const FLAC__real data[], uint32_t data_len, uint32_t lag, FLAC__real autoc[])
{
	long i;
	long limit = (long)data_len - 8;
	const FLAC__real *base;
	vector float sum0 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum1 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum10 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum11 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum20 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum21 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum30 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum31 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float d0, d1, d2;
#if WORDS_BIGENDIAN
	vector unsigned int vsel1 = { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF };
	vector unsigned int vsel2 = { 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vsel3 = { 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vperm1 = { 0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213 };
	vector unsigned int vperm2 = { 0x08090A0B, 0x0C0D0E0F, 0x10111213, 0x14151617 };
	vector unsigned int vperm3 = { 0x0C0D0E0F, 0x10111213, 0x14151617, 0x18191A1B };
#else
	vector unsigned int vsel1 = { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 };
	vector unsigned int vsel2 = { 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000 };
	vector unsigned int vsel3 = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 };
	vector unsigned int vperm1 = { 0x07060504, 0x0B0A0908, 0x0F0E0D0C, 0x13121110 };
	vector unsigned int vperm2 = { 0x0B0A0908, 0x0F0E0D0C, 0x13121110, 0x17161514 };
	vector unsigned int vperm3 = { 0x0F0E0D0C, 0x13121110, 0x17161514, 0x1B1A1918 };
#endif

	(void) lag;
	FLAC__ASSERT(lag <= 8);
	FLAC__ASSERT(lag <= data_len);

	base = data;

	d0 = vec_vsx_ld(0, base);
	d1 = vec_vsx_ld(16, base);

	base += 8;

	for (i = 0; i <= (limit-2); i += 4) {
		vector float d, d0_orig = d0;

		d2 = vec_vsx_ld(0, base);
		base += 4;

		d = vec_splat(d0_orig, 0);
		sum0 += d0 * d;
		sum1 += d1 * d;

		d = vec_splat(d0_orig, 1);
		d0 = vec_sel(d0_orig, d2, vsel1);
		sum10 += d0 * d;
		sum11 += d1 * d;

		d = vec_splat(d0_orig, 2);
		d0 = vec_sel(d0_orig, d2, vsel2);
		sum20 += d0 * d;
		sum21 += d1 * d;

		d = vec_splat(d0_orig, 3);
		d0 = vec_sel(d0_orig, d2, vsel3);
		sum30 += d0 * d;
		sum31 += d1 * d;

		d0 = d1;
		d1 = d2;
	}

	sum0 += vec_perm(sum10, sum11, (vector unsigned char)vperm1);
	sum1 += vec_perm(sum11, sum10, (vector unsigned char)vperm1);

	sum0 += vec_perm(sum20, sum21, (vector unsigned char)vperm2);
	sum1 += vec_perm(sum21, sum20, (vector unsigned char)vperm2);

	sum0 += vec_perm(sum30, sum31, (vector unsigned char)vperm3);
	sum1 += vec_perm(sum31, sum30, (vector unsigned char)vperm3);

	for (; i <= limit; i++) {
		vector float d;

		d0 = vec_vsx_ld(0, data+i);
		d1 = vec_vsx_ld(16, data+i);

		d = vec_splat(d0, 0);
		sum0 += d0 * d;
		sum1 += d1 * d;
	}

	vec_vsx_st(sum0, 0, autoc);
	vec_vsx_st(sum1, 16, autoc);

	for (; i < (long)data_len; i++) {
		uint32_t coeff;

		FLAC__real d = data[i];
		for (coeff = 0; coeff < data_len - i; coeff++)
			autoc[coeff] += d * data[i+coeff];
	}
}

__attribute__((target("cpu=power9")))
void FLAC__lpc_compute_autocorrelation_intrin_power9_vsx_lag_4(const FLAC__real data[], uint32_t data_len, uint32_t lag, FLAC__real autoc[])
{
	long i;
	long limit = (long)data_len - 4;
	const FLAC__real *base;
	vector float sum0 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum10 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum20 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float sum30 = { 0.0f, 0.0f, 0.0f, 0.0f};
	vector float d0, d1;
#if WORDS_BIGENDIAN
	vector unsigned int vsel1 = { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF };
	vector unsigned int vsel2 = { 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vsel3 = { 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	vector unsigned int vperm1 = { 0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213 };
	vector unsigned int vperm2 = { 0x08090A0B, 0x0C0D0E0F, 0x10111213, 0x14151617 };
	vector unsigned int vperm3 = { 0x0C0D0E0F, 0x10111213, 0x14151617, 0x18191A1B };
#else
	vector unsigned int vsel1 = { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 };
	vector unsigned int vsel2 = { 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000 };
	vector unsigned int vsel3 = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 };
	vector unsigned int vperm1 = { 0x07060504, 0x0B0A0908, 0x0F0E0D0C, 0x13121110 };
	vector unsigned int vperm2 = { 0x0B0A0908, 0x0F0E0D0C, 0x13121110, 0x17161514 };
	vector unsigned int vperm3 = { 0x0F0E0D0C, 0x13121110, 0x17161514, 0x1B1A1918 };
#endif

	(void) lag;
	FLAC__ASSERT(lag <= 4);
	FLAC__ASSERT(lag <= data_len);

	base = data;

	d0 = vec_vsx_ld(0, base);

	base += 4;

	for (i = 0; i <= (limit-1); i += 4) {
		vector float d, d0_orig = d0;

		d1 = vec_vsx_ld(0, base);
		base += 4;

		d = vec_splat(d0_orig, 0);
		sum0 += d0 * d;

		d = vec_splat(d0_orig, 1);
		d0 = vec_sel(d0_orig, d1, vsel1);
		sum10 += d0 * d;

		d = vec_splat(d0_orig, 2);
		d0 = vec_sel(d0_orig, d1, vsel2);
		sum20 += d0 * d;

		d = vec_splat(d0_orig, 3);
		d0 = vec_sel(d0_orig, d1, vsel3);
		sum30 += d0 * d;

		d0 = d1;
	}

	sum0 += vec_perm(sum10, sum10, (vector unsigned char)vperm1);

	sum0 += vec_perm(sum20, sum20, (vector unsigned char)vperm2);

	sum0 += vec_perm(sum30, sum30, (vector unsigned char)vperm3);

	for (; i <= limit; i++) {
		vector float d;

		d0 = vec_vsx_ld(0, data+i);

		d = vec_splat(d0, 0);
		sum0 += d0 * d;
	}

	vec_vsx_st(sum0, 0, autoc);

	for (; i < (long)data_len; i++) {
		uint32_t coeff;

		FLAC__real d = data[i];
		for (coeff = 0; coeff < data_len - i; coeff++)
			autoc[coeff] += d * data[i+coeff];
	}
}
#endif /* FLAC__HAS_TARGET_POWER9 */

#endif /* FLAC__CPU_PPC64 && FLAC__USE_VSX */
#endif /* FLAC__NO_ASM */
#endif /* FLAC__INTEGER_ONLY_LIBRARY */
