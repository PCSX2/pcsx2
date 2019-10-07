/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2019  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "Common.h"

#include "IPU.h"
#include "IPUdma.h"
#include "yuv2rgb.h"
#include "mpeg2lib/Mpeg.h"

void ipu_dither_reference(const macroblock_rgb32 &rgb32, macroblock_rgb16 &rgb16, int dte);
void ipu_dither_sse2(const macroblock_rgb32 &rgb32, macroblock_rgb16 &rgb16, int dte);

__ri void ipu_dither(const macroblock_rgb32 &rgb32, macroblock_rgb16 &rgb16, int dte)
{
    ipu_dither_sse2(rgb32, rgb16, dte);
}

__ri void ipu_dither_reference(const macroblock_rgb32 &rgb32, macroblock_rgb16 &rgb16, int dte)
{
    if (dte) {
        // I'm guessing values are rounded down when clamping.
        const int dither_coefficient[4][4] = {
            {-4, 0, -3, 1},
            {2, -2, 3, -1},
            {-3, 1, -4, 0},
            {3, -1, 2, -2},
        };
        for (int i = 0; i < 16; ++i) {
            for (int j = 0; j < 16; ++j) {
                const int dither = dither_coefficient[i & 3][j & 3];
                const int r = std::max(0, std::min(rgb32.c[i][j].r + dither, 255));
                const int g = std::max(0, std::min(rgb32.c[i][j].g + dither, 255));
                const int b = std::max(0, std::min(rgb32.c[i][j].b + dither, 255));

                rgb16.c[i][j].r = r >> 3;
                rgb16.c[i][j].g = g >> 3;
                rgb16.c[i][j].b = b >> 3;
                rgb16.c[i][j].a = rgb32.c[i][j].a == 0x40;
            }
        }
    } else {
        for (int i = 0; i < 16; ++i) {
            for (int j = 0; j < 16; ++j) {
                rgb16.c[i][j].r = rgb32.c[i][j].r >> 3;
                rgb16.c[i][j].g = rgb32.c[i][j].g >> 3;
                rgb16.c[i][j].b = rgb32.c[i][j].b >> 3;
                rgb16.c[i][j].a = rgb32.c[i][j].a == 0x40;
            }
        }
    }
}

__ri void ipu_dither_sse2(const macroblock_rgb32 &rgb32, macroblock_rgb16 &rgb16, int dte)
{
    const __m128i alpha_test = _mm_set1_epi16(0x40);
    const __m128i dither_add_matrix[] = {
        _mm_setr_epi32(0x00000000, 0x00000000, 0x00000000, 0x00010101),
        _mm_setr_epi32(0x00020202, 0x00000000, 0x00030303, 0x00000000),
        _mm_setr_epi32(0x00000000, 0x00010101, 0x00000000, 0x00000000),
        _mm_setr_epi32(0x00030303, 0x00000000, 0x00020202, 0x00000000),
    };
    const __m128i dither_sub_matrix[] = {
        _mm_setr_epi32(0x00040404, 0x00000000, 0x00030303, 0x00000000),
        _mm_setr_epi32(0x00000000, 0x00020202, 0x00000000, 0x00010101),
        _mm_setr_epi32(0x00030303, 0x00000000, 0x00040404, 0x00000000),
        _mm_setr_epi32(0x00000000, 0x00010101, 0x00000000, 0x00020202),
    };
    for (int i = 0; i < 16; ++i) {
        const __m128i dither_add = dither_add_matrix[i & 3];
        const __m128i dither_sub = dither_sub_matrix[i & 3];
        for (int n = 0; n < 2; ++n) {
            __m128i rgba_8_0123 = _mm_load_si128(reinterpret_cast<const __m128i *>(&rgb32.c[i][n * 8]));
            __m128i rgba_8_4567 = _mm_load_si128(reinterpret_cast<const __m128i *>(&rgb32.c[i][n * 8 + 4]));

            // Dither and clamp
            if (dte) {
                rgba_8_0123 = _mm_adds_epu8(rgba_8_0123, dither_add);
                rgba_8_0123 = _mm_subs_epu8(rgba_8_0123, dither_sub);
                rgba_8_4567 = _mm_adds_epu8(rgba_8_4567, dither_add);
                rgba_8_4567 = _mm_subs_epu8(rgba_8_4567, dither_sub);
            }

            // Split into channel components and extend to 16 bits
            const __m128i rgba_16_0415 = _mm_unpacklo_epi8(rgba_8_0123, rgba_8_4567);
            const __m128i rgba_16_2637 = _mm_unpackhi_epi8(rgba_8_0123, rgba_8_4567);
            const __m128i rgba_32_0246 = _mm_unpacklo_epi8(rgba_16_0415, rgba_16_2637);
            const __m128i rgba_32_1357 = _mm_unpackhi_epi8(rgba_16_0415, rgba_16_2637);
            const __m128i rg_64_01234567 = _mm_unpacklo_epi8(rgba_32_0246, rgba_32_1357);
            const __m128i ba_64_01234567 = _mm_unpackhi_epi8(rgba_32_0246, rgba_32_1357);

            const __m128i zero = _mm_setzero_si128();
            __m128i r = _mm_unpacklo_epi8(rg_64_01234567, zero);
            __m128i g = _mm_unpackhi_epi8(rg_64_01234567, zero);
            __m128i b = _mm_unpacklo_epi8(ba_64_01234567, zero);
            __m128i a = _mm_unpackhi_epi8(ba_64_01234567, zero);

            // Create RGBA
            r = _mm_srli_epi16(r, 3);
            g = _mm_slli_epi16(_mm_srli_epi16(g, 3), 5);
            b = _mm_slli_epi16(_mm_srli_epi16(b, 3), 10);
            a = _mm_slli_epi16(_mm_cmpeq_epi16(a, alpha_test), 15);

            const __m128i rgba16 = _mm_or_si128(_mm_or_si128(r, g), _mm_or_si128(b, a));

            _mm_store_si128(reinterpret_cast<__m128i *>(&rgb16.c[i][n * 8]), rgba16);
        }
    }
}
