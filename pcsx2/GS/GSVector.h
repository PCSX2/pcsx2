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

#pragma once

enum Align_Mode
{
	Align_Outside,
	Align_Inside,
	Align_NegInf,
	Align_PosInf
};

enum Round_Mode
{
	Round_NearestInt = 8,
	Round_NegInf = 9,
	Round_PosInf = 10,
	Round_Truncate = 11
};

#pragma pack(push, 1)

template <class T>
class GSVector2T
{
public:
	union
	{
		struct { T x, y; };
		struct { T r, g; };
		struct { T v[2]; };
	};

	GSVector2T()
	{
	}

	GSVector2T(T x)
	{
		this->x = x;
		this->y = x;
	}

	GSVector2T(T x, T y)
	{
		this->x = x;
		this->y = y;
	}

	bool operator==(const GSVector2T& v) const
	{
		return x == v.x && y == v.y;
	}

	bool operator!=(const GSVector2T& v) const
	{
		return x != v.x || y != v.y;
	}
};

typedef GSVector2T<float> GSVector2;
typedef GSVector2T<int> GSVector2i;

class GSVector4;
class GSVector4i;

#if _M_SSE >= 0x500

class GSVector8;

#endif

#if _M_SSE >= 0x501

class GSVector8i;

#endif

// Position and order is important
#include "GSVector4i.h"
#include "GSVector4.h"
#include "GSVector8i.h"
#include "GSVector8.h"

// conversion

__forceinline GSVector4i::GSVector4i(const GSVector4& v, bool truncate)
{
	m = truncate ? _mm_cvttps_epi32(v) : _mm_cvtps_epi32(v);
}

__forceinline GSVector4::GSVector4(const GSVector4i& v)
{
	m = _mm_cvtepi32_ps(v);
}

#if _M_SSE >= 0x501

__forceinline GSVector8i::GSVector8i(const GSVector8& v, bool truncate)
{
	m = truncate ? _mm256_cvttps_epi32(v) : _mm256_cvtps_epi32(v);
}

__forceinline GSVector8::GSVector8(const GSVector8i& v)
{
	m = _mm256_cvtepi32_ps(v);
}

#endif

// casting

__forceinline GSVector4i GSVector4i::cast(const GSVector4& v)
{
	return GSVector4i(_mm_castps_si128(v.m));
}

__forceinline GSVector4 GSVector4::cast(const GSVector4i& v)
{
	return GSVector4(_mm_castsi128_ps(v.m));
}

#if _M_SSE >= 0x500

__forceinline GSVector4i GSVector4i::cast(const GSVector8& v)
{
	return GSVector4i(_mm_castps_si128(_mm256_castps256_ps128(v)));
}

__forceinline GSVector4 GSVector4::cast(const GSVector8& v)
{
	return GSVector4(_mm256_castps256_ps128(v));
}

__forceinline GSVector8 GSVector8::cast(const GSVector4i& v)
{
	return GSVector8(_mm256_castps128_ps256(_mm_castsi128_ps(v.m)));
}

__forceinline GSVector8 GSVector8::cast(const GSVector4& v)
{
	return GSVector8(_mm256_castps128_ps256(v.m));
}

#endif

#if _M_SSE >= 0x501

__forceinline GSVector4i GSVector4i::cast(const GSVector8i& v)
{
	return GSVector4i(_mm256_castsi256_si128(v));
}

__forceinline GSVector4 GSVector4::cast(const GSVector8i& v)
{
	return GSVector4(_mm_castsi128_ps(_mm256_castsi256_si128(v)));
}

__forceinline GSVector8i GSVector8i::cast(const GSVector4i& v)
{
	return GSVector8i(_mm256_castsi128_si256(v.m));
}

__forceinline GSVector8i GSVector8i::cast(const GSVector4& v)
{
	return GSVector8i(_mm256_castsi128_si256(_mm_castps_si128(v.m)));
}

__forceinline GSVector8i GSVector8i::cast(const GSVector8& v)
{
	return GSVector8i(_mm256_castps_si256(v.m));
}

__forceinline GSVector8 GSVector8::cast(const GSVector8i& v)
{
	return GSVector8(_mm256_castsi256_ps(v.m));
}

#endif

#pragma pack(pop)
