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

__ri void ipu_dither(const macroblock_rgb32 &rgb32, macroblock_rgb16 &rgb16, int dte)
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
