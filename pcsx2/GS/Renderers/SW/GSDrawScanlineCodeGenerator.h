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

#pragma once

#include "GSScanlineEnvironment.h"
#include "GS/Renderers/Common/GSFunctionMap.h"
#include "GS/GSUtil.h"

#if defined(_M_AMD64) || defined(_WIN64)
#define RegLong Xbyak::Reg64
#else
#define RegLong Xbyak::Reg32
#endif

class GSDrawScanlineCodeGenerator : public GSCodeGenerator
{
	typedef Xbyak::Ymm Ymm;
	typedef Xbyak::Xmm Xmm;
	typedef Xbyak::Reg8 Reg8;
	typedef Xbyak::Operand Operand;

	void operator=(const GSDrawScanlineCodeGenerator&);

	GSScanlineSelector m_sel;
	GSScanlineLocalData& m_local;
	bool m_rip;

	void Generate();

#if _M_SSE >= 0x501

	void Init();
	void Step();
	void TestZ(const Ymm& temp1, const Ymm& temp2);
	void SampleTexture();
	void Wrap(const Ymm& uv0);
	void Wrap(const Ymm& uv0, const Ymm& uv1);
	void SampleTextureLOD();
	void WrapLOD(const Ymm& uv0);
	void WrapLOD(const Ymm& uv0, const Ymm& uv1);
	void AlphaTFX();
	void ReadMask();
	void TestAlpha();
	void ColorTFX();
	void Fog();
	void ReadFrame();
	void TestDestAlpha();
	void WriteMask();
	void WriteZBuf();
	void AlphaBlend();
	void WriteFrame();
	void ReadPixel(const Ymm& dst, const Ymm& temp, const RegLong& addr);
	void WritePixel(const Ymm& src, const Ymm& temp, const RegLong& addr, const Xbyak::Reg32& mask, bool fast, int psm, int fz);
	void WritePixel(const Xmm& src, const RegLong& addr, uint8 i, uint8 j, int psm);
	void ReadTexel(int pixels, int mip_offset = 0);
	void ReadTexel(const Ymm& dst, const Ymm& addr, uint8 i);

#else

	void Generate_SSE();
	void Init_SSE();
	void Step_SSE();
	void TestZ_SSE(const Xmm& temp1, const Xmm& temp2);
	void SampleTexture_SSE();
	void Wrap_SSE(const Xmm& uv0);
	void Wrap_SSE(const Xmm& uv0, const Xmm& uv1);
	void SampleTextureLOD_SSE();
	void WrapLOD_SSE(const Xmm& uv0);
	void WrapLOD_SSE(const Xmm& uv0, const Xmm& uv1);
	void AlphaTFX_SSE();
	void ReadMask_SSE();
	void TestAlpha_SSE();
	void ColorTFX_SSE();
	void Fog_SSE();
	void ReadFrame_SSE();
	void TestDestAlpha_SSE();
	void WriteMask_SSE();
	void WriteZBuf_SSE();
	void AlphaBlend_SSE();
	void WriteFrame_SSE();
	void ReadPixel_SSE(const Xmm& dst, const RegLong& addr);
	void WritePixel_SSE(const Xmm& src, const RegLong& addr, const Reg8& mask, bool fast, int psm, int fz);
	void WritePixel_SSE(const Xmm& src, const RegLong& addr, uint8 i, int psm);
	void ReadTexel_SSE(int pixels, int mip_offset = 0);
	void ReadTexel_SSE(const Xmm& dst, const Xmm& addr, uint8 i);

	void Generate_AVX();
	void Init_AVX();
	void Step_AVX();
	void TestZ_AVX(const Xmm& temp1, const Xmm& temp2);
	void SampleTexture_AVX();
	void Wrap_AVX(const Xmm& uv0);
	void Wrap_AVX(const Xmm& uv0, const Xmm& uv1);
	void SampleTextureLOD_AVX();
	void WrapLOD_AVX(const Xmm& uv0);
	void WrapLOD_AVX(const Xmm& uv0, const Xmm& uv1);
	void AlphaTFX_AVX();
	void ReadMask_AVX();
	void TestAlpha_AVX();
	void ColorTFX_AVX();
	void Fog_AVX();
	void ReadFrame_AVX();
	void TestDestAlpha_AVX();
	void WriteMask_AVX();
	void WriteZBuf_AVX();
	void AlphaBlend_AVX();
	void WriteFrame_AVX();
	void ReadPixel_AVX(const Xmm& dst, const RegLong& addr);
	void WritePixel_AVX(const Xmm& src, const RegLong& addr, const Reg8& mask, bool fast, int psm, int fz);
	void WritePixel_AVX(const Xmm& src, const RegLong& addr, uint8 i, int psm);
	void ReadTexel_AVX(int pixels, int mip_offset = 0);
	void ReadTexel_AVX(const Xmm& dst, const Xmm& addr, uint8 i);

#endif

	void modulate16(const Xmm& a, const Operand& f, uint8 shift);
	void lerp16(const Xmm& a, const Xmm& b, const Xmm& f, uint8 shift);
	void lerp16_4(const Xmm& a, const Xmm& b, const Xmm& f);
	void mix16(const Xmm& a, const Xmm& b, const Xmm& temp);
	void clamp16(const Xmm& a, const Xmm& temp);
	void alltrue(const Xmm& test);
	void blend(const Xmm& a, const Xmm& b, const Xmm& mask);
	void blendr(const Xmm& b, const Xmm& a, const Xmm& mask);
	void blend8(const Xmm& a, const Xmm& b);
	void blend8r(const Xmm& b, const Xmm& a);
	void split16_2x8(const Xmm& l, const Xmm& h, const Xmm& src);

public:
	GSDrawScanlineCodeGenerator(void* param, uint64 key, void* code, size_t maxsize);
};
