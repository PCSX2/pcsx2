// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/MultiISA.h"

MULTI_ISA_DEF(extern void yuv2rgb_reference();)

#if defined(_M_X86)

#define yuv2rgb yuv2rgb_sse2
MULTI_ISA_DEF(extern void yuv2rgb_sse2();)

#elif defined(_M_ARM64)

#define yuv2rgb yuv2rgb_neon
MULTI_ISA_DEF(extern void yuv2rgb_neon();)

#endif
