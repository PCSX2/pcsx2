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

#include "GSRegs.h"
#include "GSVector.h"
#include "GSTables.h"
#include "GSAlignedClass.h"

class GSLocalMemory;

class alignas(32) GSClut : public GSAlignedClass<32>
{
	static const GSVector4i m_bm;
	static const GSVector4i m_gm;
	static const GSVector4i m_rm;

	GSLocalMemory* m_mem;

	u32 m_CBP[2];
	u16* m_clut;
	u32* m_buff32;
	u64* m_buff64;

	struct alignas(32) WriteState
	{
		GIFRegTEX0 TEX0;
		GIFRegTEXCLUT TEXCLUT;
		bool dirty;
		bool IsDirty(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);
	} m_write;

	struct alignas(32) ReadState
	{
		GIFRegTEX0 TEX0;
		GIFRegTEXA TEXA;
		bool dirty;
		bool adirty;
		int amin, amax;
		bool IsDirty(const GIFRegTEX0& TEX0);
		bool IsDirty(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA);
	} m_read;

	typedef void (GSClut::*writeCLUT)(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);

	writeCLUT m_wc[2][16][64];

	void WriteCLUT32_I8_CSM1(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);
	void WriteCLUT32_I4_CSM1(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);
	void WriteCLUT16_I8_CSM1(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);
	void WriteCLUT16_I4_CSM1(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);
	void WriteCLUT16S_I8_CSM1(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);
	void WriteCLUT16S_I4_CSM1(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);

	template <int n>
	void WriteCLUT32_CSM2(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);
	template <int n>
	void WriteCLUT16_CSM2(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);
	template <int n>
	void WriteCLUT16S_CSM2(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);

	void WriteCLUT_NULL(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);

	static void WriteCLUT_T32_I8_CSM1(const u32* RESTRICT src, u16* RESTRICT clut, u16 offset);
	static void WriteCLUT_T32_I4_CSM1(const u32* RESTRICT src, u16* RESTRICT clut);
	static void WriteCLUT_T16_I8_CSM1(const u16* RESTRICT src, u16* RESTRICT clut);
	static void WriteCLUT_T16_I4_CSM1(const u16* RESTRICT src, u16* RESTRICT clut);
	static void ReadCLUT_T32_I8(const u16* RESTRICT clut, u32* RESTRICT dst, int offset);
	static void ReadCLUT_T32_I4(const u16* RESTRICT clut, u32* RESTRICT dst);
	//static void ReadCLUT_T32_I4(const u16* RESTRICT clut, u32* RESTRICT dst32, u64* RESTRICT dst64);
	//static void ReadCLUT_T16_I8(const u16* RESTRICT clut, u32* RESTRICT dst);
	//static void ReadCLUT_T16_I4(const u16* RESTRICT clut, u32* RESTRICT dst);
	//static void ReadCLUT_T16_I4(const u16* RESTRICT clut, u32* RESTRICT dst32, u64* RESTRICT dst64);
public:
	static void ExpandCLUT64_T32_I8(const u32* RESTRICT src, u64* RESTRICT dst);

private:
	static void ExpandCLUT64_T32(const GSVector4i& hi, const GSVector4i& lo0, const GSVector4i& lo1, const GSVector4i& lo2, const GSVector4i& lo3, GSVector4i* dst);
	static void ExpandCLUT64_T32(const GSVector4i& hi, const GSVector4i& lo, GSVector4i* dst);
	//static void ExpandCLUT64_T16_I8(const u32* RESTRICT src, u64* RESTRICT dst);
	static void ExpandCLUT64_T16(const GSVector4i& hi, const GSVector4i& lo0, const GSVector4i& lo1, const GSVector4i& lo2, const GSVector4i& lo3, GSVector4i* dst);
	static void ExpandCLUT64_T16(const GSVector4i& hi, const GSVector4i& lo, GSVector4i* dst);

	static void Expand16(const u16* RESTRICT src, u32* RESTRICT dst, int w, const GIFRegTEXA& TEXA);

public:
	GSClut(GSLocalMemory* mem);
	virtual ~GSClut();

	void Invalidate();
	void Invalidate(u32 block);
	bool WriteTest(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);
	void Write(const GIFRegTEX0& TEX0, const GIFRegTEXCLUT& TEXCLUT);
	//void Read(const GIFRegTEX0& TEX0);
	void Read32(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA);
	void GetAlphaMinMax32(int& amin, int& amax);

	u32 operator[](size_t i) const { return m_buff32[i]; }

	operator const u32*() const { return m_buff32; }
	operator const u64*() const { return m_buff64; }
};
