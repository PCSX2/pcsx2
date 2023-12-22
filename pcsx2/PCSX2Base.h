// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

/// Base defines and typedefs that are needed by all code in PCSX2
/// Prefer this over including Pcsx2Defs.h to make sure everyone gets all the defines, as missing defines fail silently

#pragma once

#include "common/Pcsx2Defs.h"

#if defined(__AVX2__)
	#define _M_SSE 0x501
#elif defined(__AVX__)
	#define _M_SSE 0x500
#elif defined(__SSE4_1__)
	#define _M_SSE 0x401
#else
	#error PCSX2 requires compiling for at least SSE 4.1
#endif

// Starting with AVX, processors have fast unaligned loads
// Reduce code duplication by not compiling multiple versions
#if _M_SSE >= 0x500
	#define FAST_UNALIGNED 1
#else
	#define FAST_UNALIGNED 0
#endif
