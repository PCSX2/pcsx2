/*
 *	Copyright (C) 2020 PCSX2 Dev Team
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

enum class VectorISA { None, SSE4, AVX, AVX2 };
extern VectorISA currentISA;

#if defined(MULTI_ISA_UNSHARED_COMPILATION) || defined(MULTI_ISA_SHARED_COMPILATION)
# define MULTI_ISA_DEF(x) \
	namespace isa_sse4 { x } \
	namespace isa_avx  { x } \
	namespace isa_avx2 { x }

# define MULTI_ISA_SELECT(fn) (\
	::currentISA == VectorISA::AVX2 ? isa_avx2::fn : \
	::currentISA == VectorISA::AVX  ? isa_avx::fn  : \
	isa_sse4::fn)
#else
# define MULTI_ISA_DEF(x) namespace isa_native { x }
# define MULTI_ISA_SELECT(fn) (isa_native::fn)
#endif

class IGSRenderer;

MULTI_ISA_DEF(bool InitGConst();)
MULTI_ISA_DEF(IGSRenderer* makeRendererSW(int threads);)
MULTI_ISA_DEF(IGSRenderer* makeRendererOGL();)
MULTI_ISA_DEF(IGSRenderer* makeRendererNull();)
MULTI_ISA_DEF(void GSBenchmark();)
#ifdef _WIN32
MULTI_ISA_DEF(IGSRenderer* makeRendererDX11();)
#endif

#undef MULTI_ISA_DEF

// MARK: Global variables that would otherwise be in unshared regions but need to be shared for some reason

#include "GSCrc.h"

// TKR: These were in GSHwHack.cpp (unshared), I assume they were done this way so they would show up in a debugger so I moved them here instead of making them static
extern bool s_nativeres;
extern CRC::Region g_crc_region;

namespace GSStateISAShared
{
	/// Incremental draw call number
	/// (Used for debug purposes, mostly used by GSState but also touched by `GSDevice`s in shared area)
	extern int s_n;
} // namespace GSStateISAShared
