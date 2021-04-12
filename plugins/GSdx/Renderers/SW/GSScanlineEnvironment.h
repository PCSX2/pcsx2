/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
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

#include "GSLocalMemory.h"
#include "GSVector.h"

union GSScanlineSelector
{
	struct
	{
		uint32 fpsm  : 2; // 0
		uint32 zpsm  : 2; // 2
		uint32 ztst  : 2; // 4 (0: off, 1: write, 2: test (ge), 3: test (g))
		uint32 atst  : 3; // 6
		uint32 afail : 2; // 9
		uint32 iip   : 1; // 11
		uint32 tfx   : 3; // 12
		uint32 tcc   : 1; // 15
		uint32 fst   : 1; // 16
		uint32 ltf   : 1; // 17
		uint32 tlu   : 1; // 18
		uint32 fge   : 1; // 19
		uint32 date  : 1; // 20
		uint32 abe   : 1; // 21
		uint32 aba   : 2; // 22
		uint32 abb   : 2; // 24
		uint32 abc   : 2; // 26
		uint32 abd   : 2; // 28
		uint32 pabe  : 1; // 30
		uint32 aa1   : 1; // 31

		uint32 fwrite    : 1; // 32
		uint32 ftest     : 1; // 33
		uint32 rfb       : 1; // 34
		uint32 zwrite    : 1; // 35
		uint32 ztest     : 1; // 36
		uint32 zoverflow : 1; // 37 (z max >= 0x80000000)
		uint32 zclamp    : 1; // 38
		uint32 wms       : 2; // 39
		uint32 wmt       : 2; // 41
		uint32 datm      : 1; // 43
		uint32 colclamp  : 1; // 44
		uint32 fba       : 1; // 45
		uint32 dthe      : 1; // 46
		uint32 prim      : 2; // 47

		uint32 edge   : 1; // 49
		uint32 tw     : 3; // 50 (encodes values between 3 -> 10, texture cache makes sure it is at least 3)
		uint32 lcm    : 1; // 53
		uint32 mmin   : 2; // 54
		uint32 notest : 1; // 55 (no ztest, no atest, no date, no scissor test, and horizontally aligned to 4 pixels)
		// TODO: 1D texture flag? could save 2 texture reads and 4 lerps with bilinear, and also the texture coordinate clamp/wrap code in one direction

		uint32 breakpoint : 1; // Insert a trap to stop the program, helpful to stop debugger on a program
	};

	struct
	{
		uint32 _pad1  : 22;
		uint32 ababcd :  8;
		uint32 _pad2  :  2;

		uint32 fb    : 2;
		uint32 _pad3 : 1;
		uint32 zb    : 2;
	};

	struct
	{
		uint32 lo;
		uint32 hi;
	};

	uint64 key;

	GSScanlineSelector() = default;
	GSScanlineSelector(uint64 k)
		: key(k)
	{
	}

	operator uint32() const { return lo; }
	operator uint64() const { return key; }

	bool IsSolidRect() const
	{
		return prim == GS_SPRITE_CLASS && iip == 0 && tfx == TFX_NONE && abe == 0 && ztst <= 1 && atst <= 1 && date == 0 && fge == 0;
	}

	void Print() const
	{
		fprintf(stderr, "fpsm:%d zpsm:%d ztst:%d ztest:%d atst:%d afail:%d iip:%d rfb:%d fb:%d zb:%d zw:%d "
		                "tfx:%d tcc:%d fst:%d ltf:%d tlu:%d wms:%d wmt:%d mmin:%d lcm:%d tw:%d "
		                "fba:%d cclamp:%d date:%d datm:%d "
		                "prim:%d abe:%d %d%d%d%d fge:%d dthe:%d notest:%d\n",
		        fpsm, zpsm, ztst, ztest, atst, afail, iip, rfb, fb, zb, zwrite,
		        tfx, tcc, fst, ltf, tlu, wms, wmt, mmin, lcm, tw,
		        fba, colclamp, date, datm,
		        prim, abe, aba, abb, abc, abd, fge, dthe, notest);
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
	uint32* clut;
	GSVector4i* dimx;

	const int* fbr;
	const int* zbr;
	const int* fbc;
	const int* zbc;
	const GSVector2i* fzbr;
	const GSVector2i* fzbc;

	GSVector4i aref;
	GSVector4i afix;
	struct { GSVector4i min, max, minmax, mask, invmask; } t; // [u] x 4 [v] x 4

#if _M_SSE >= 0x501

	uint32 fm, zm;
	uint32 frb, fga;
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
	struct step { GSVector4 stq; struct { uint32 rb, ga; } c; struct { uint32 z, f; } p; } d8;
	struct { GSVector8i rb, ga; } c;
	struct { uint32 z, f; } p;

	// these should be stored on stack as normal local variables (no free regs to use, esp cannot be saved to anywhere, and we need an aligned stack)

	struct
	{
		GSVector8 z, zo;
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
		GSVector4 z, zo;
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
	alignas(32) uint8 m_test_256b[16][8];
	alignas(32) float m_shift_256b[9][8];
	alignas(32) float m_log2_coef_256b[4][8];

	alignas(16) uint32 m_test_128b[8][4];
	alignas(16) float m_shift_128b[5][4];
	alignas(16) float m_log2_coef_128b[4][4];

	GSScanlineConstantData() {}

	// GCC will be clever enough to stick some AVX instruction here
	// So it must be defered to post global constructor
	void Init()
	{
		uint8 I_hate_vs2013_m_test_256b[16][8] = {
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

		uint32 I_hate_vs2013_m_test_128b[8][4] = {
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

		for (size_t n = 0; n < countof(log2_coef); ++n)
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
