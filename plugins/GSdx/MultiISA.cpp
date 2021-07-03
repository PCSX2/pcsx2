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

#include "stdafx.h"
#include "MultiISA.h"
#include "GSUtil.h"

#ifdef _MSC_VER
#  define strcasecmp _stricmp
#endif

static Xbyak::util::Cpu s_cpu;

static VectorISA getCurrentISA()
{
	// For debugging
	if (const char* over = getenv("OVERRIDE_VECTOR_ISA"))
	{
		if (strcasecmp(over, "avx2") == 0)
		{
			fprintf(stderr, "Vector ISA Override: AVX2\n");
			return VectorISA::AVX2;
		}
		if (strcasecmp(over, "avx") == 0)
		{
			fprintf(stderr, "Vector ISA Override: AVX\n");
			return VectorISA::AVX;
		}
		if (strcasecmp(over, "sse4") == 0)
		{
			fprintf(stderr, "Vector ISA Override: SSE4\n");
			return VectorISA::SSE4;
		}
	}
	if (s_cpu.has(Xbyak::util::Cpu::tAVX2) && s_cpu.has(Xbyak::util::Cpu::tBMI1) && s_cpu.has(Xbyak::util::Cpu::tBMI2))
		return VectorISA::AVX2;
	else if (s_cpu.has(Xbyak::util::Cpu::tAVX))
		return VectorISA::AVX;
	else if (s_cpu.has(Xbyak::util::Cpu::tSSE41))
		return VectorISA::SSE4;
	else
		return VectorISA::None;
}

VectorISA currentISA = getCurrentISA();

bool s_nativeres;
CRC::Region g_crc_region = CRC::NoRegion;
int GSStateISAShared::s_n = 0;
