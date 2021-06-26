/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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
#include "GSBlock.h"

CONSTINIT const GSVector4i GSBlock::m_r16mask(0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15);
CONSTINIT const GSVector4i GSBlock::m_r8mask(0, 4, 2, 6, 8, 12, 10, 14, 1, 5, 3, 7, 9, 13, 11, 15);
CONSTINIT const GSVector4i GSBlock::m_r4mask(0, 8, 4, 12, 1, 9, 5, 13, 2, 10, 6, 14, 3, 11, 7, 15);
CONSTINIT const GSVector4i GSBlock::m_w4mask(0, 4, 8, 12, 2, 6, 10, 14, 1, 5, 9, 13, 3, 7, 11, 15);
CONSTINIT const GSVector4i GSBlock::m_r4hmask(0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15);
CONSTINIT const GSVector4i GSBlock::m_r4hmask_avx2(0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15);
CONSTINIT const GSVector4i GSBlock::m_palvec_mask(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15);

CONSTINIT const GSVector4i GSBlock::m_avx2_r8mask1(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15);
CONSTINIT const GSVector4i GSBlock::m_avx2_r8mask2(1, 5, 9, 13, 0, 4, 8, 12, 3, 7, 11, 15, 2, 6, 10, 14);
CONSTINIT const GSVector4i GSBlock::m_avx2_w8mask1(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15);
CONSTINIT const GSVector4i GSBlock::m_avx2_w8mask2(4, 0, 12, 8, 5, 1, 13, 9, 6, 2, 14, 10, 7, 3, 15, 11);

CONSTINIT const GSVector4i GSBlock::m_uw8hmask0(0, 0, 0, 0, 1, 1, 1, 1, 8, 8, 8, 8, 9, 9, 9, 9);
CONSTINIT const GSVector4i GSBlock::m_uw8hmask1(2, 2, 2, 2, 3, 3, 3, 3, 10, 10, 10, 10, 11, 11, 11, 11);
CONSTINIT const GSVector4i GSBlock::m_uw8hmask2(4, 4, 4, 4, 5, 5, 5, 5, 12, 12, 12, 12, 13, 13, 13, 13);
CONSTINIT const GSVector4i GSBlock::m_uw8hmask3(6, 6, 6, 6, 7, 7, 7, 7, 14, 14, 14, 14, 15, 15, 15, 15);
