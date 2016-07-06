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

#include "stdafx.h"
#include "GSVector.h"

GSVector4i GSVector4i::m_xff[17];

GSVector4i GSVector4i::m_x0f[17];

GSVector4 GSVector4::m_ps0123;
GSVector4 GSVector4::m_ps4567;
GSVector4 GSVector4::m_half;
GSVector4 GSVector4::m_one;
GSVector4 GSVector4::m_two;
GSVector4 GSVector4::m_four;
GSVector4 GSVector4::m_x4b000000;
GSVector4 GSVector4::m_x4f800000;
GSVector4 GSVector4::m_max;
GSVector4 GSVector4::m_min;

#if _M_SSE >= 0x500

GSVector8 GSVector8::m_half(0.5f);
GSVector8 GSVector8::m_one(1.0f);
GSVector8 GSVector8::m_x7fffffff(_mm256_castsi256_ps(_mm256_set1_epi32(0x7fffffff)));
GSVector8 GSVector8::m_x80000000(_mm256_castsi256_ps(_mm256_set1_epi32(0x80000000)));
GSVector8 GSVector8::m_x4b000000(_mm256_castsi256_ps(_mm256_set1_epi32(0x4b000000)));
GSVector8 GSVector8::m_x4f800000(_mm256_castsi256_ps(_mm256_set1_epi32(0x4f800000)));
GSVector8 GSVector8::m_max(FLT_MAX);
GSVector8 GSVector8::m_min(FLT_MIN);

#endif

#if _M_SSE >= 0x501

GSVector8i GSVector8i::m_xff[33];

GSVector8i GSVector8i::m_x0f[33];

#endif

GSVector4i GSVector4i::fit(int arx, int ary) const
{
	GSVector4i r = *this;

	if(arx > 0 && ary > 0)
	{
		int w = width();
		int h = height();

		if(w * ary > h * arx)
		{
			w = h * arx / ary;
			r.left = (r.left + r.right - w) >> 1;
			if(r.left & 1) r.left++;
			r.right = r.left + w;
		}
		else
		{
			h = w * ary / arx;
			r.top = (r.top + r.bottom - h) >> 1;
			if(r.top & 1) r.top++;
			r.bottom = r.top + h;
		}

		r = r.rintersect(*this);
	}
	else
	{
		r = *this;
	}

	return r;
}

static const int s_ar[][2] = {{0, 0}, {4, 3}, {16, 9}};

GSVector4i GSVector4i::fit(int preset) const
{
	GSVector4i r;

	if(preset > 0 && preset < (int)countof(s_ar))
	{
		r = fit(s_ar[preset][0], s_ar[preset][1]);
	}
	else
	{
		r = *this;
	}

	return r;
}

void GSVector4i::Init()
{
	m_xff[0] = GSVector4i(0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[1] = GSVector4i(0x000000ff, 0x00000000, 0x00000000, 0x00000000);
	m_xff[2] = GSVector4i(0x0000ffff, 0x00000000, 0x00000000, 0x00000000);
	m_xff[3] = GSVector4i(0x00ffffff, 0x00000000, 0x00000000, 0x00000000);
	m_xff[4] = GSVector4i(0xffffffff, 0x00000000, 0x00000000, 0x00000000);
	m_xff[5] = GSVector4i(0xffffffff, 0x000000ff, 0x00000000, 0x00000000);
	m_xff[6] = GSVector4i(0xffffffff, 0x0000ffff, 0x00000000, 0x00000000);
	m_xff[7] = GSVector4i(0xffffffff, 0x00ffffff, 0x00000000, 0x00000000);
	m_xff[8] = GSVector4i(0xffffffff, 0xffffffff, 0x00000000, 0x00000000);
	m_xff[9] = GSVector4i(0xffffffff, 0xffffffff, 0x000000ff, 0x00000000);
	m_xff[10] = GSVector4i(0xffffffff, 0xffffffff, 0x0000ffff, 0x00000000);
	m_xff[11] = GSVector4i(0xffffffff, 0xffffffff, 0x00ffffff, 0x00000000);
	m_xff[12] = GSVector4i(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);
	m_xff[13] = GSVector4i(0xffffffff, 0xffffffff, 0xffffffff, 0x000000ff);
	m_xff[14] = GSVector4i(0xffffffff, 0xffffffff, 0xffffffff, 0x0000ffff);
	m_xff[15] = GSVector4i(0xffffffff, 0xffffffff, 0xffffffff, 0x00ffffff);
	m_xff[16] = GSVector4i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);

	m_x0f[0] = GSVector4i(0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[1] = GSVector4i(0x0000000f, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[2] = GSVector4i(0x00000f0f, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[3] = GSVector4i(0x000f0f0f, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[4] = GSVector4i(0x0f0f0f0f, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[5] = GSVector4i(0x0f0f0f0f, 0x0000000f, 0x00000000, 0x00000000);
	m_x0f[6] = GSVector4i(0x0f0f0f0f, 0x00000f0f, 0x00000000, 0x00000000);
	m_x0f[7] = GSVector4i(0x0f0f0f0f, 0x000f0f0f, 0x00000000, 0x00000000);
	m_x0f[8] = GSVector4i(0x0f0f0f0f, 0x0f0f0f0f, 0x00000000, 0x00000000);
	m_x0f[9] = GSVector4i(0x0f0f0f0f, 0x0f0f0f0f, 0x0000000f, 0x00000000);
	m_x0f[10] = GSVector4i(0x0f0f0f0f, 0x0f0f0f0f, 0x00000f0f, 0x00000000);
	m_x0f[11] = GSVector4i(0x0f0f0f0f, 0x0f0f0f0f, 0x000f0f0f, 0x00000000);
	m_x0f[12] = GSVector4i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x00000000);
	m_x0f[13] = GSVector4i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0000000f);
	m_x0f[14] = GSVector4i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x00000f0f);
	m_x0f[15] = GSVector4i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x000f0f0f);
	m_x0f[16] = GSVector4i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f);
}

void GSVector4::Init()
{
	m_ps0123 = GSVector4(0.0f, 1.0f, 2.0f, 3.0f);
	m_ps4567 = GSVector4(4.0f, 5.0f, 6.0f, 7.0f);
	m_half = GSVector4(0.5f);
	m_one = GSVector4(1.0f);
	m_two = GSVector4(2.0f);
	m_four = GSVector4(4.0f);
	m_x4b000000 = GSVector4(_mm_castsi128_ps(_mm_set1_epi32(0x4b000000)));
	m_x4f800000 = GSVector4(_mm_castsi128_ps(_mm_set1_epi32(0x4f800000)));
	m_max = GSVector4(FLT_MAX);
	m_min = GSVector4(FLT_MIN);
}

#if _M_SSE >= 0x501

void GSVector8i::Init()
{
	m_xff[0] = GSVector8i(0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[1] = GSVector8i(0x000000ff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[2] = GSVector8i(0x0000ffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[3] = GSVector8i(0x00ffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[4] = GSVector8i(0xffffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[5] = GSVector8i(0xffffffff, 0x000000ff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[6] = GSVector8i(0xffffffff, 0x0000ffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[7] = GSVector8i(0xffffffff, 0x00ffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[8] = GSVector8i(0xffffffff, 0xffffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[9] = GSVector8i(0xffffffff, 0xffffffff, 0x000000ff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[10] = GSVector8i(0xffffffff, 0xffffffff, 0x0000ffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[11] = GSVector8i(0xffffffff, 0xffffffff, 0x00ffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[12] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[13] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0x000000ff, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[14] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0x0000ffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[15] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0x00ffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[16] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_xff[17] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x000000ff, 0x00000000, 0x00000000, 0x00000000);
	m_xff[18] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x0000ffff, 0x00000000, 0x00000000, 0x00000000);
	m_xff[19] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x00ffffff, 0x00000000, 0x00000000, 0x00000000);
	m_xff[20] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x00000000, 0x00000000, 0x00000000);
	m_xff[21] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x000000ff, 0x00000000, 0x00000000);
	m_xff[22] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x0000ffff, 0x00000000, 0x00000000);
	m_xff[23] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x00ffffff, 0x00000000, 0x00000000);
	m_xff[24] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x00000000, 0x00000000);
	m_xff[25] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x000000ff, 0x00000000);
	m_xff[26] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x0000ffff, 0x00000000);
	m_xff[27] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x00ffffff, 0x00000000);
	m_xff[28] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);
	m_xff[29] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x000000ff);
	m_xff[30] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x0000ffff);
	m_xff[31] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x00ffffff);
	m_xff[32] = GSVector8i(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);

	
	m_x0f[0] = GSVector8i(0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0000000f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x00000f0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x000f0f0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0000000f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x00000f0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x000f0f0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0000000f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x00000f0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x000f0f0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0000000f, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x00000f0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x000f0f0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0000000f, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x00000f0f, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x000f0f0f, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x00000000, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0000000f, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x00000f0f, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x000f0f0f, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x00000000, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0000000f, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x00000f0f, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x000f0f0f, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x00000000);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0000000f);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x00000f0f);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x000f0f0f);
	m_x0f[0] = GSVector8i(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f);
}

#endif

#if _M_SSE >= 0x500

void GSVector8::Init()
{
	m_half = GSVector8(0.5f);
	m_one = GSVector8(1.0f);
	m_x7fffffff = GSVector8(_mm256_castsi256_ps(_mm256_set1_epi32(0x7fffffff)));
	m_x80000000 = GSVector8(_mm256_castsi256_ps(_mm256_set1_epi32(0x80000000)));
	m_x4b000000 = GSVector8(_mm256_castsi256_ps(_mm256_set1_epi32(0x4b000000)));
	m_x4f800000 = GSVector8(_mm256_castsi256_ps(_mm256_set1_epi32(0x4f800000)));
	m_max = GSVector8(FLT_MAX);
	m_min = GSVector8(FLT_MIN);
}

#endif