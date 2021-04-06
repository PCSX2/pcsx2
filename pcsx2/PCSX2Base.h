/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

/// Base defines and typedefs that are needed by all code in PCSX2
/// Prefer this over including Pcsx2Defs.h to make sure everyone gets all the defines, as missing defines fail silently

#pragma once

#include "common/Pcsx2Defs.h"
#include "GS/config.h"

#if defined(__GNUC__)
	// Convert gcc see define into GS (windows) define
	#if defined(__AVX2__)
		#define _M_SSE 0x501
	#elif defined(__AVX__)
		#define _M_SSE 0x500
	#elif defined(__SSE4_1__)
		#define _M_SSE 0x401
	#else
		#error PCSX2 requires compiling for at least SSE 4.1
	#endif
#elif _M_SSE < 0x401
	#error PCSX2 requires compiling for at least SSE 4.1
#endif

// Starting with AVX, processors have fast unaligned loads
// Reduce code duplication by not compiling multiple versions
#if _M_SSE >= 0x500
	#define FAST_UNALIGNED 1
#else
	#define FAST_UNALIGNED 0
#endif
