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

#include "GS/GSLocalMemory.h"
#include "GS/GSVector.h"

union GSScanlineSelector
{
	struct
	{
		u32 fpsm  : 2; // 0
		u32 zpsm  : 2; // 2
		u32 ztst  : 2; // 4 (0: off, 1: write, 2: test (ge), 3: test (g))
		u32 atst  : 3; // 6
		u32 afail : 2; // 9
		u32 iip   : 1; // 11
		u32 tfx   : 3; // 12
		u32 tcc   : 1; // 15
		u32 fst   : 1; // 16
		u32 ltf   : 1; // 17
		u32 tlu   : 1; // 18
		u32 fge   : 1; // 19
		u32 date  : 1; // 20
		u32 abe   : 1; // 21
		u32 aba   : 2; // 22
		u32 abb   : 2; // 24
		u32 abc   : 2; // 26
		u32 abd   : 2; // 28
		u32 pabe  : 1; // 30
		u32 aa1   : 1; // 31

		u32 fwrite    : 1; // 32
		u32 ftest     : 1; // 33
		u32 rfb       : 1; // 34
		u32 zwrite    : 1; // 35
		u32 ztest     : 1; // 36
		u32 zoverflow : 1; // 37 (z max >= 0x80000000)
		u32 zclamp    : 1; // 38
		u32 wms       : 2; // 39
		u32 wmt       : 2; // 41
		u32 datm      : 1; // 43
		u32 colclamp  : 1; // 44
		u32 fba       : 1; // 45
		u32 dthe      : 1; // 46
		u32 prim      : 2; // 47

		u32 edge   : 1; // 49
		u32 tw     : 3; // 50 (encodes values between 3 -> 10, texture cache makes sure it is at least 3)
		u32 lcm    : 1; // 53
		u32 mmin   : 2; // 54
		u32 notest : 1; // 55 (no ztest, no atest, no date, no scissor test, and horizontally aligned to 4 pixels)
		// TODO: 1D texture flag? could save 2 texture reads and 4 lerps with bilinear, and also the texture coordinate clamp/wrap code in one direction
		u32 zequal : 1; // 56
		u32 breakpoint : 1; // Insert a trap to stop the program, helpful to stop debugger on a program
	};

	struct
	{
		u32 _pad1  : 22;
		u32 ababcd :  8;
		u32 _pad2  :  2;

		u32 fb    : 2;
		u32 _pad3 : 1;
		u32 zb    : 2;
	};

	struct
	{
		u32 lo;
		u32 hi;
	};

	u64 key;

	GSScanlineSelector() = default;
	GSScanlineSelector(u64 k)
		: key(k)
	{
	}

	operator u32() const { return lo; }
	operator u64() const { return key; }

	bool IsSolidRect() const
	{
		return prim == GS_SPRITE_CLASS && iip == 0 && tfx == TFX_NONE && abe == 0 && ztst <= 1 && atst <= 1 && date == 0 && fge == 0;
	}

	std::string to_string() const
	{
		char str[1024];
		sprintf(str,
			"fpsm:%d zpsm:%d ztst:%d ztest:%d atst:%d afail:%d iip:%d rfb:%d fb:%d zb:%d zw:%d "
			"tfx:%d tcc:%d fst:%d ltf:%d tlu:%d wms:%d wmt:%d mmin:%d lcm:%d tw:%d "
			"fba:%d cclamp:%d date:%d datm:%d "
			"prim:%d abe:%d %d%d%d%d fge:%d dthe:%d notest:%d pabe:%d aa1:%d "
			"fwrite:%d ftest:%d zoverflow:%d zclamp:%d edge:%d",
			fpsm, zpsm, ztst, ztest, atst, afail, iip, rfb, fb, zb, zwrite,
			tfx, tcc, fst, ltf, tlu, wms, wmt, mmin, lcm, tw,
			fba, colclamp, date, datm,
			prim, abe, aba, abb, abc, abd, fge, dthe, notest, pabe, aa1,
			fwrite, ftest, zoverflow, zclamp, edge);
		return str;
	}

	void Print() const
	{
		fprintf(stderr, "%s\n", to_string().c_str());
	}
};

struct alignas(32) GSScanlineGlobalData // per batch variables, this is like a pixel shader constant buffer
{
	GSScanlineSelector sel;

	// - the data of vm, tex may change, multi-threaded drawing must be finished before that happens, clut and dimx are copies
	// - tex is a cached texture, it may be recycled to free up memory, its absolute address cannot be compiled into code
	// - row and column pointers are allocated once and never change or freed, thier address can be used directly

	void* vm;
	const void* tex[7];
	u32* clut;
	GSVector4i* dimx;

	GSOffset fbo;
	GSOffset zbo;
	const GSVector2i* fzbr;
	const GSVector2i* fzbc;

	GSVector4i aref;
	GSVector4i afix;
	struct { GSVector4i min, max, minmax, mask, invmask; } t; // [u] x 4 [v] x 4

#if _M_SSE >= 0x501

	u32 fm, zm;
	u32 frb, fga;
	GSVector8 mxl;
	GSVector8 k; // TEX1.K * 0x10000
	GSVector8 l; // TEX1.L * -0x10000
	struct { GSVector8i i, f; } lod; // lcm == 1

#else

	GSVector4i fm, zm;
	GSVector4i frb, fga;
	GSVector4 mxl;
	GSVector4 k; // TEX1.K * 0x10000
	GSVector4 l; // TEX1.L * -0x10000
	struct { GSVector4i i, f; } lod; // lcm == 1

#endif
};

struct alignas(32) GSScanlineLocalData // per prim variables, each thread has its own
{
#if _M_SSE >= 0x501

	struct skip { GSVector8 z, s, t, q; GSVector8i rb, ga, f, _pad; } d[8];
	struct step { GSVector4 stq; struct { u32 rb, ga; } c; struct { u64 z; u32 f; } p; } d8;
	struct { u32 z, f; } p;
	struct { GSVector8i rb, ga; } c;

	// these should be stored on stack as normal local variables (no free regs to use, esp cannot be saved to anywhere, and we need an aligned stack)

	struct
	{
		GSVector8 z0, z1;
		GSVector8i f;
		GSVector8 s, t, q;
		GSVector8i rb, ga;
		GSVector8i zs, zd;
		GSVector8i uf, vf;
		GSVector8i cov;

		// mipmapping

		struct { GSVector8i i, f; } lod;
		GSVector8i uv[2];
		GSVector8i uv_minmax[2];
		GSVector8i trb, tga;
		GSVector8i test;
	} temp;

#else

	struct skip { GSVector4 z, s, t, q; GSVector4i rb, ga, f, _pad; } d[4];
	struct step { GSVector4 z, stq; GSVector4i c, f; } d4;
	struct { GSVector4i rb, ga; } c;
	struct { GSVector4i z, f; } p;

	// these should be stored on stack as normal local variables (no free regs to use, esp cannot be saved to anywhere, and we need an aligned stack)

	struct
	{
		GSVector4 z0, z1;
		GSVector4i f;
		GSVector4 s, t, q;
		GSVector4i rb, ga;
		GSVector4i zs, zd;
		GSVector4i uf, vf;
		GSVector4i cov;

		// mipmapping

		struct { GSVector4i i, f; } lod;
		GSVector4i uv[2];
		GSVector4i uv_minmax[2];
		GSVector4i trb, tga;
		GSVector4i test;
	} temp;

#endif

	//

	const GSScanlineGlobalData* gd;
};

// Constant shared by all threads (to reduce cache miss)
//
// Note: Avoid GSVector* to support all ISA at once
//
// WARNING: Don't use static storage. Static variables are relocated to random
// location (above 2GB). Small allocation on the heap could be below 2GB, this way we can use
// absolute addressing. Otherwise we need to store a base address in a register.
struct GSScanlineConstantData : public GSAlignedClass<32>
{
	alignas(32) u8 m_test_256b[16][8];
	alignas(32) float m_shift_256b[9][8];
	alignas(32) float m_log2_coef_256b[4][8];

	alignas(16) u32 m_test_128b[8][4];
	alignas(16) float m_shift_128b[5][4];
	alignas(16) float m_log2_coef_128b[4][4];

	GSScanlineConstantData() {}

	// GCC will be clever enough to stick some AVX instruction here
	// So it must be defered to post global constructor
	void Init()
	{
		u8 I_hate_vs2013_m_test_256b[16][8] = {
			{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
			{0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
			{0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
			{0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00},
			{0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00},
			{0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00},
			{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00},
			{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00},
			{0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
			{0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
			{0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff},
			{0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff},
			{0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff},
			{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff},
			{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff},
			{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
		};

		u32 I_hate_vs2013_m_test_128b[8][4] = {
			{0x00000000, 0x00000000, 0x00000000, 0x00000000},
			{0xffffffff, 0x00000000, 0x00000000, 0x00000000},
			{0xffffffff, 0xffffffff, 0x00000000, 0x00000000},
			{0xffffffff, 0xffffffff, 0xffffffff, 0x00000000},
			{0x00000000, 0xffffffff, 0xffffffff, 0xffffffff},
			{0x00000000, 0x00000000, 0xffffffff, 0xffffffff},
			{0x00000000, 0x00000000, 0x00000000, 0xffffffff},
			{0x00000000, 0x00000000, 0x00000000, 0x00000000}
		};

		float I_hate_vs2013_m_shift_256b[9][8] = {
			{ 8.0f  , 8.0f  , 8.0f  , 8.0f  , 8.0f  , 8.0f  , 8.0f  , 8.0f},
			{ 0.0f  , 1.0f  , 2.0f  , 3.0f  , 4.0f  , 5.0f  , 6.0f  , 7.0f},
			{ -1.0f , 0.0f  , 1.0f  , 2.0f  , 3.0f  , 4.0f  , 5.0f  , 6.0f},
			{ -2.0f , -1.0f , 0.0f  , 1.0f  , 2.0f  , 3.0f  , 4.0f  , 5.0f},
			{ -3.0f , -2.0f , -1.0f , 0.0f  , 1.0f  , 2.0f  , 3.0f  , 4.0f},
			{ -4.0f , -3.0f , -2.0f , -1.0f , 0.0f  , 1.0f  , 2.0f  , 3.0f},
			{ -5.0f , -4.0f , -3.0f , -2.0f , -1.0f , 0.0f  , 1.0f  , 2.0f},
			{ -6.0f , -5.0f , -4.0f , -3.0f , -2.0f , -1.0f , 0.0f  , 1.0f},
			{ -7.0f , -6.0f , -5.0f , -4.0f , -3.0f , -2.0f , -1.0f , 0.0f}
		};

		float I_hate_vs2013_m_shift_128b[5][4] = {
			{ 4.0f  , 4.0f  , 4.0f  , 4.0f},
			{ 0.0f  , 1.0f  , 2.0f  , 3.0f},
			{ -1.0f , 0.0f  , 1.0f  , 2.0f},
			{ -2.0f , -1.0f , 0.0f  , 1.0f},
			{ -3.0f , -2.0f , -1.0f , 0.0f}
		};

		memcpy(m_test_256b, I_hate_vs2013_m_test_256b, sizeof(I_hate_vs2013_m_test_256b));
		memcpy(m_test_128b, I_hate_vs2013_m_test_128b, sizeof(I_hate_vs2013_m_test_128b));
		memcpy(m_shift_256b, I_hate_vs2013_m_shift_256b, sizeof(I_hate_vs2013_m_shift_256b));
		memcpy(m_shift_128b, I_hate_vs2013_m_shift_128b, sizeof(I_hate_vs2013_m_shift_128b));

		float log2_coef[] = {
			0.204446009836232697516f,
			-1.04913055217340124191f,
			2.28330284476918490682f,
			1.0f
		};

		for (size_t n = 0; n < std::size(log2_coef); ++n)
		{
			for (size_t i = 0; i < 4; ++i)
			{
				m_log2_coef_128b[n][i] = log2_coef[n];
				m_log2_coef_256b[n][i] = log2_coef[n];
				m_log2_coef_256b[n][i + 4] = log2_coef[n];
			}
		}
	}
};

extern std::unique_ptr<GSScanlineConstantData> g_const;
