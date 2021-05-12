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
#include "GSDrawScanlineCodeGenerator.h"

#if _M_SSE < 0x501 && (defined(_M_AMD64) || defined(_WIN64))

// It is useless to port the code to SSEx, better use the faster 32 bits version instead
void GSDrawScanlineCodeGenerator::Generate_SSE()
{
	// Avoid a crash if someone want to use it
	ret();
}

void GSDrawScanlineCodeGenerator::Init_SSE()
{
}

void GSDrawScanlineCodeGenerator::Step_SSE()
{
}

void GSDrawScanlineCodeGenerator::TestZ_SSE(const Xmm& temp1, const Xmm& temp2)
{
}

void GSDrawScanlineCodeGenerator::SampleTexture_SSE()
{
}

void GSDrawScanlineCodeGenerator::Wrap_SSE(const Xmm& uv)
{
}

void GSDrawScanlineCodeGenerator::Wrap_SSE(const Xmm& uv0, const Xmm& uv1)
{
}

void GSDrawScanlineCodeGenerator::AlphaTFX_SSE()
{
}

void GSDrawScanlineCodeGenerator::ReadMask_SSE()
{
}

void GSDrawScanlineCodeGenerator::TestAlpha_SSE()
{
}

void GSDrawScanlineCodeGenerator::ColorTFX_SSE()
{
}

void GSDrawScanlineCodeGenerator::Fog_SSE()
{
}

void GSDrawScanlineCodeGenerator::ReadFrame_SSE()
{
}

void GSDrawScanlineCodeGenerator::TestDestAlpha_SSE()
{
}

void GSDrawScanlineCodeGenerator::WriteMask_SSE()
{
}

void GSDrawScanlineCodeGenerator::WriteZBuf_SSE()
{
}

void GSDrawScanlineCodeGenerator::AlphaBlend_SSE()
{
}

void GSDrawScanlineCodeGenerator::WriteFrame_SSE()
{
}

void GSDrawScanlineCodeGenerator::ReadPixel_SSE(const Xmm& dst, const RegLong& addr)
{
}

void GSDrawScanlineCodeGenerator::WritePixel_SSE(const Xmm& src, const RegLong& addr, const Reg8& mask, bool fast, int psm, int fz)
{
}

//static const int s_offsets[4] = {0, 2, 8, 10};

void GSDrawScanlineCodeGenerator::WritePixel_SSE(const Xmm& src, const RegLong& addr, uint8 i, int psm)
{
}

void GSDrawScanlineCodeGenerator::ReadTexel_SSE(int pixels, int mip_offset)
{
}

void GSDrawScanlineCodeGenerator::ReadTexel_SSE(const Xmm& dst, const Xmm& addr, uint8 i)
{
}

#endif
