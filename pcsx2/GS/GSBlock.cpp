// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GSBlock.h"

MULTI_ISA_UNSHARED_IMPL;

constinit const GSVector4i GSBlock::m_r16mask(0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15);
constinit const GSVector4i GSBlock::m_r8mask(0, 4, 2, 6, 8, 12, 10, 14, 1, 5, 3, 7, 9, 13, 11, 15);
constinit const GSVector4i GSBlock::m_r4mask(0, 8, 4, 12, 1, 9, 5, 13, 2, 10, 6, 14, 3, 11, 7, 15);
constinit const GSVector4i GSBlock::m_w4mask(0, 4, 8, 12, 2, 6, 10, 14, 1, 5, 9, 13, 3, 7, 11, 15);
constinit const GSVector4i GSBlock::m_r4hmask(0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15);
constinit const GSVector4i GSBlock::m_r4hmask_avx2(0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15);
constinit const GSVector4i GSBlock::m_palvec_mask(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15);

constinit const GSVector4i GSBlock::m_avx2_r8mask1(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15);
constinit const GSVector4i GSBlock::m_avx2_r8mask2(1, 5, 9, 13, 0, 4, 8, 12, 3, 7, 11, 15, 2, 6, 10, 14);
constinit const GSVector4i GSBlock::m_avx2_w8mask1(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15);
constinit const GSVector4i GSBlock::m_avx2_w8mask2(4, 0, 12, 8, 5, 1, 13, 9, 6, 2, 14, 10, 7, 3, 15, 11);

constinit const GSVector4i GSBlock::m_uw8hmask0(0, 0, 0, 0, 1, 1, 1, 1, 8, 8, 8, 8, 9, 9, 9, 9);
constinit const GSVector4i GSBlock::m_uw8hmask1(2, 2, 2, 2, 3, 3, 3, 3, 10, 10, 10, 10, 11, 11, 11, 11);
constinit const GSVector4i GSBlock::m_uw8hmask2(4, 4, 4, 4, 5, 5, 5, 5, 12, 12, 12, 12, 13, 13, 13, 13);
constinit const GSVector4i GSBlock::m_uw8hmask3(6, 6, 6, 6, 7, 7, 7, 7, 14, 14, 14, 14, 15, 15, 15, 15);
