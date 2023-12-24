// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/Assertions.h"
#include "common/VectorIntrin.h"

#include <algorithm>
#include <cstring>

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

	GSVector2T() = default;

	constexpr GSVector2T(T x): x(x), y(x)
	{
	}

	constexpr GSVector2T(T x, T y): x(x), y(y)
	{
	}

	constexpr bool operator==(const GSVector2T& v) const
	{
		return (std::memcmp(this, &v, sizeof(*this)) == 0);
	}

	constexpr bool operator!=(const GSVector2T& v) const
	{
		return (std::memcmp(this, &v, sizeof(*this)) != 0);
	}

	constexpr GSVector2T operator*(const GSVector2T& v) const
	{
		return { x * v.x, y * v.y };
	}

	constexpr GSVector2T operator/(const GSVector2T& v) const
	{
		return { x / v.x, y / v.y };
	}

	GSVector2T min(const GSVector2T& v) const
	{
		return { std::min(x, v.x), std::min(y, v.y) };
	}

	GSVector2T max(const GSVector2T& v) const
	{
		return { std::max(x, v.x), std::max(y, v.y) };
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

__forceinline_odr GSVector4i::GSVector4i(const GSVector4& v, bool truncate)
{
	m = truncate ? _mm_cvttps_epi32(v) : _mm_cvtps_epi32(v);
}

__forceinline_odr GSVector4::GSVector4(const GSVector4i& v)
{
	m = _mm_cvtepi32_ps(v);
}

__forceinline_odr void GSVector4i::sw32_inv(GSVector4i& a, GSVector4i& b, GSVector4i& c, GSVector4i& d)
{
	GSVector4 af = GSVector4::cast(a);
	GSVector4 bf = GSVector4::cast(b);
	GSVector4 cf = GSVector4::cast(c);
	GSVector4 df = GSVector4::cast(d);

	a = GSVector4i::cast(af.xzxz(cf));
	b = GSVector4i::cast(af.ywyw(cf));
	c = GSVector4i::cast(bf.xzxz(df));
	d = GSVector4i::cast(bf.ywyw(df));
}

#if _M_SSE >= 0x501

__forceinline_odr GSVector8i::GSVector8i(const GSVector8& v, bool truncate)
{
	m = truncate ? _mm256_cvttps_epi32(v) : _mm256_cvtps_epi32(v);
}

__forceinline_odr GSVector8::GSVector8(const GSVector8i& v)
{
	m = _mm256_cvtepi32_ps(v);
}

__forceinline_odr void GSVector8i::sw32_inv(GSVector8i& a, GSVector8i& b)
{
	GSVector8 af = GSVector8::cast(a);
	GSVector8 bf = GSVector8::cast(b);
	a = GSVector8i::cast(af.xzxz(bf));
	b = GSVector8i::cast(af.ywyw(bf));
}

#endif

// casting

__forceinline_odr GSVector4i GSVector4i::cast(const GSVector4& v)
{
	return GSVector4i(_mm_castps_si128(v.m));
}

__forceinline_odr GSVector4 GSVector4::cast(const GSVector4i& v)
{
	return GSVector4(_mm_castsi128_ps(v.m));
}

#if _M_SSE >= 0x500

__forceinline_odr GSVector4i GSVector4i::cast(const GSVector8& v)
{
	return GSVector4i(_mm_castps_si128(_mm256_castps256_ps128(v)));
}

__forceinline_odr GSVector4 GSVector4::cast(const GSVector8& v)
{
	return GSVector4(_mm256_castps256_ps128(v));
}

__forceinline_odr GSVector8 GSVector8::cast(const GSVector4i& v)
{
	return GSVector8(_mm256_castps128_ps256(_mm_castsi128_ps(v.m)));
}

__forceinline_odr GSVector8 GSVector8::cast(const GSVector4& v)
{
	return GSVector8(_mm256_castps128_ps256(v.m));
}

#endif

#if _M_SSE >= 0x501

__forceinline_odr GSVector4i GSVector4i::cast(const GSVector8i& v)
{
	return GSVector4i(_mm256_castsi256_si128(v));
}

__forceinline_odr GSVector4 GSVector4::cast(const GSVector8i& v)
{
	return GSVector4(_mm_castsi128_ps(_mm256_castsi256_si128(v)));
}

__forceinline_odr GSVector8i GSVector8i::cast(const GSVector4i& v)
{
	return GSVector8i(_mm256_castsi128_si256(v.m));
}

__forceinline_odr GSVector8i GSVector8i::cast(const GSVector4& v)
{
	return GSVector8i(_mm256_castsi128_si256(_mm_castps_si128(v.m)));
}

__forceinline_odr GSVector8i GSVector8i::cast(const GSVector8& v)
{
	return GSVector8i(_mm256_castps_si256(v.m));
}

__forceinline_odr GSVector8 GSVector8::cast(const GSVector8i& v)
{
	return GSVector8(_mm256_castsi256_ps(v.m));
}

#endif

