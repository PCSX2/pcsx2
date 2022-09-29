/* Copyright (C) 2007-2008 Jean-Marc Valin
 * Copyright (C) 2008 Thorvald Natvig
 * Copyright (C) 2011 Texas Instruments
 *               author Jyri Sarha
 */
/**
   @file resample_neon.h
   @brief Resampler functions (NEON version)
*/
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <arm_neon.h>

#ifdef FIXED_POINT
#ifdef __thumb2__
static inline int32_t saturate_32bit_to_16bit(int32_t a) {
    int32_t ret;
    asm ("ssat %[ret], #16, %[a]"
         : [ret] "=&r" (ret)
         : [a] "r" (a)
         : );
    return ret;
}
#else
static inline int32_t saturate_32bit_to_16bit(int32_t a) {
    int32_t ret;
    asm ("vmov.s32 d0[0], %[a]\n"
         "vqmovn.s32 d0, q0\n"
         "vmov.s16 %[ret], d0[0]\n"
         : [ret] "=&r" (ret)
         : [a] "r" (a)
         : "q0");
    return ret;
}
#endif
#undef WORD2INT
#define WORD2INT(x) (saturate_32bit_to_16bit(x))

#define OVERRIDE_INNER_PRODUCT_SINGLE
/* Only works when len % 4 == 0 */
static inline int32_t inner_product_single(const int16_t *a, const int16_t *b, unsigned int len)
{
    int32_t ret;
    uint32_t remainder = len % 16;
    len = len - remainder;

    asm volatile ("	 cmp %[len], #0\n"
		  "	 bne 1f\n"
		  "	 vld1.16 {d16}, [%[b]]!\n"
		  "	 vld1.16 {d20}, [%[a]]!\n"
		  "	 subs %[remainder], %[remainder], #4\n"
		  "	 vmull.s16 q0, d16, d20\n"
		  "      beq 5f\n"
		  "	 b 4f\n"
		  "1:"
		  "	 vld1.16 {d16, d17, d18, d19}, [%[b]]!\n"
		  "	 vld1.16 {d20, d21, d22, d23}, [%[a]]!\n"
		  "	 subs %[len], %[len], #16\n"
		  "	 vmull.s16 q0, d16, d20\n"
		  "	 vmlal.s16 q0, d17, d21\n"
		  "	 vmlal.s16 q0, d18, d22\n"
		  "	 vmlal.s16 q0, d19, d23\n"
		  "	 beq 3f\n"
		  "2:"
		  "	 vld1.16 {d16, d17, d18, d19}, [%[b]]!\n"
		  "	 vld1.16 {d20, d21, d22, d23}, [%[a]]!\n"
		  "	 subs %[len], %[len], #16\n"
		  "	 vmlal.s16 q0, d16, d20\n"
		  "	 vmlal.s16 q0, d17, d21\n"
		  "	 vmlal.s16 q0, d18, d22\n"
		  "	 vmlal.s16 q0, d19, d23\n"
		  "	 bne 2b\n"
		  "3:"
		  "	 cmp %[remainder], #0\n"
		  "	 beq 5f\n"
		  "4:"
		  "	 vld1.16 {d16}, [%[b]]!\n"
		  "	 vld1.16 {d20}, [%[a]]!\n"
		  "	 subs %[remainder], %[remainder], #4\n"
		  "	 vmlal.s16 q0, d16, d20\n"
		  "	 bne 4b\n"
		  "5:"
		  "	 vaddl.s32 q0, d0, d1\n"
		  "	 vadd.s64 d0, d0, d1\n"
		  "	 vqmovn.s64 d0, q0\n"
		  "	 vqrshrn.s32 d0, q0, #15\n"
		  "	 vmov.s16 %[ret], d0[0]\n"
		  : [ret] "=&r" (ret), [a] "+r" (a), [b] "+r" (b),
		    [len] "+r" (len), [remainder] "+r" (remainder)
		  :
		  : "cc", "q0",
		    "d16", "d17", "d18", "d19",
		    "d20", "d21", "d22", "d23");

    return ret;
}
#elif defined(FLOATING_POINT)

static inline int32_t saturate_float_to_16bit(float a) {
    int32_t ret;
    asm ("vmov.f32 d0[0], %[a]\n"
         "vcvt.s32.f32 d0, d0, #15\n"
         "vqrshrn.s32 d0, q0, #15\n"
         "vmov.s16 %[ret], d0[0]\n"
         : [ret] "=&r" (ret)
         : [a] "r" (a)
         : "q0");
    return ret;
}
#undef WORD2INT
#define WORD2INT(x) (saturate_float_to_16bit(x))

#define OVERRIDE_INNER_PRODUCT_SINGLE
/* Only works when len % 4 == 0 */
static inline float inner_product_single(const float *a, const float *b, unsigned int len)
{
    float ret;
    uint32_t remainder = len % 16;
    len = len - remainder;

    asm volatile ("	 cmp %[len], #0\n"
		  "	 bne 1f\n"
		  "	 vld1.32 {q4}, [%[b]]!\n"
		  "	 vld1.32 {q8}, [%[a]]!\n"
		  "	 subs %[remainder], %[remainder], #4\n"
		  "	 vmul.f32 q0, q4, q8\n"
		  "      bne 4f\n"
		  "	 b 5f\n"
		  "1:"
		  "	 vld1.32 {q4, q5}, [%[b]]!\n"
		  "	 vld1.32 {q8, q9}, [%[a]]!\n"
		  "	 vld1.32 {q6, q7}, [%[b]]!\n"
		  "	 vld1.32 {q10, q11}, [%[a]]!\n"
		  "	 subs %[len], %[len], #16\n"
		  "	 vmul.f32 q0, q4, q8\n"
		  "	 vmul.f32 q1, q5, q9\n"
		  "	 vmul.f32 q2, q6, q10\n"
		  "	 vmul.f32 q3, q7, q11\n"
		  "	 beq 3f\n"
		  "2:"
		  "	 vld1.32 {q4, q5}, [%[b]]!\n"
		  "	 vld1.32 {q8, q9}, [%[a]]!\n"
		  "	 vld1.32 {q6, q7}, [%[b]]!\n"
		  "	 vld1.32 {q10, q11}, [%[a]]!\n"
		  "	 subs %[len], %[len], #16\n"
		  "	 vmla.f32 q0, q4, q8\n"
		  "	 vmla.f32 q1, q5, q9\n"
		  "	 vmla.f32 q2, q6, q10\n"
		  "	 vmla.f32 q3, q7, q11\n"
		  "	 bne 2b\n"
		  "3:"
		  "	 vadd.f32 q4, q0, q1\n"
		  "	 vadd.f32 q5, q2, q3\n"
		  "	 cmp %[remainder], #0\n"
		  "	 vadd.f32 q0, q4, q5\n"
		  "	 beq 5f\n"
		  "4:"
		  "	 vld1.32 {q6}, [%[b]]!\n"
		  "	 vld1.32 {q10}, [%[a]]!\n"
		  "	 subs %[remainder], %[remainder], #4\n"
		  "	 vmla.f32 q0, q6, q10\n"
		  "	 bne 4b\n"
		  "5:"
		  "	 vadd.f32 d0, d0, d1\n"
		  "	 vpadd.f32 d0, d0, d0\n"
		  "	 vmov.f32 %[ret], d0[0]\n"
		  : [ret] "=&r" (ret), [a] "+r" (a), [b] "+r" (b),
		    [len] "+l" (len), [remainder] "+l" (remainder)
		  :
		  : "cc", "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "q8",
                    "q9", "q10", "q11");
    return ret;
}
#endif
