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

#include <cassert>

#if _M_SSE >= 0x500

class alignas(32) GSVector8
{
	struct cxpr_init_tag {};
	static constexpr cxpr_init_tag cxpr_init{};

	constexpr GSVector8(cxpr_init_tag, float x0, float y0, float z0, float w0, float x1, float y1, float z1, float w1)
		: F32{x0, y0, z0, w0, x1, y1, z1, w1}
	{
	}

	constexpr GSVector8(cxpr_init_tag, int x0, int y0, int z0, int w0, int x1, int y1, int z1, int w1)
		: I32{x0, y0, z0, w0, x1, y1, z1, w1}
	{
	}

	constexpr GSVector8(cxpr_init_tag, u64 x, u64 y, u64 z, u64 w)
		: U64{x, y, z, w}
	{
	}

public:
	union
	{
		struct { float x0, y0, z0, w0, x1, y1, z1, w1; };
		struct { float r0, g0, b0, a0, r1, g1, b1, a1; };
		float v[8];
		float F32[8];
		double F64[4];
		s8  I8[32];
		s16 I16[16];
		s32 I32[8];
		s64 I64[4];
		u8  U8[32];
		u16 U16[16];
		u32 U32[8];
		u64 U64[4];
		__m256 m;
		__m128 m0, m1;
	};

	static const GSVector8 m_half;
	static const GSVector8 m_one;
	static const GSVector8 m_x7fffffff;
	static const GSVector8 m_x80000000;
	static const GSVector8 m_x4b000000;
	static const GSVector8 m_x4f800000;
	static const GSVector8 m_xc1e00000000fffff;
	static const GSVector8 m_max;
	static const GSVector8 m_min;

	GSVector8() = default;

	static constexpr GSVector8 cxpr(float x0, float y0, float z0, float w0, float x1, float y1, float z1, float w1)
	{
		return GSVector8(cxpr_init, x0, y0, z0, w0, x1, y1, z1, w1);
	}

	static constexpr GSVector8 cxpr(float x)
	{
		return GSVector8(cxpr_init, x, x, x, x, x, x, x, x);
	}

	static constexpr GSVector8 cxpr(int x0, int y0, int z0, int w0, int x1, int y1, int z1, int w1)
	{
		return GSVector8(cxpr_init, x0, y0, z0, w0, x1, y1, z1, w1);
	}

	static constexpr GSVector8 cxpr(int x)
	{
		return GSVector8(cxpr_init, x, x, x, x, x, x, x, x);
	}

	static constexpr GSVector8 cxpr(u32 x)
	{
		return cxpr(static_cast<int>(x));
	}

	constexpr static GSVector8 cxpr64(u64 x, u64 y, u64 z, u64 w)
	{
		return GSVector8(cxpr_init, x, y, z, w);
	}

	constexpr static GSVector8 cxpr64(u64 x)
	{
		return GSVector8(cxpr_init, x, x, x, x);
	}

	__forceinline GSVector8(float x0, float y0, float z0, float w0, float x1, float y1, float z1, float w1)
	{
		m = _mm256_set_ps(w1, z1, y1, x1, w0, z0, y0, x0);
	}

	__forceinline GSVector8(int x0, int y0, int z0, int w0, int x1, int y1, int z1, int w1)
	{
		m = _mm256_cvtepi32_ps(_mm256_set_epi32(w1, z1, y1, x1, w0, z0, y0, x0));
	}

	__forceinline GSVector8(__m128 m0, __m128 m1)
	{
#if 0 // _MSC_VER >= 1700 
		
		this->m = _mm256_permute2f128_ps(_mm256_castps128_ps256(m0), _mm256_castps128_ps256(m1), 0x20);

#else

		this->m = zero().insert<0>(m0).insert<1>(m1);

#endif
	}

	constexpr GSVector8(const GSVector8& v) = default;

	__forceinline explicit GSVector8(float f)
	{
		*this = f;
	}

	__forceinline explicit GSVector8(int i)
	{
#if _M_SSE >= 0x501

		m = _mm256_cvtepi32_ps(_mm256_broadcastd_epi32(_mm_cvtsi32_si128(i)));

#else

		GSVector4i v((int)i);

		*this = GSVector4(v);

#endif
	}

	__forceinline explicit GSVector8(__m128 m)
	{
		*this = m;
	}

	__forceinline constexpr explicit GSVector8(__m256 m)
		: m(m)
	{
	}

	__forceinline explicit GSVector8(__m256d m)
		: m(_mm256_castpd_ps(m))
	{
	}

#if _M_SSE >= 0x501

	__forceinline explicit GSVector8(const GSVector8i& v);

	__forceinline static GSVector8 cast(const GSVector8i& v);

#endif

	__forceinline static GSVector8 cast(const GSVector4& v);
	__forceinline static GSVector8 cast(const GSVector4i& v);

	__forceinline void operator=(const GSVector8& v)
	{
		m = v.m;
	}

	__forceinline void operator=(float f)
	{
#if _M_SSE >= 0x501

		m = _mm256_broadcastss_ps(_mm_load_ss(&f));

#else

		m = _mm256_set1_ps(f);

#endif
	}

	__forceinline void operator=(__m128 m)
	{
		this->m = _mm256_insertf128_ps(_mm256_castps128_ps256(m), m, 1);
	}

	__forceinline void operator=(__m256 m)
	{
		this->m = m;
	}

	__forceinline operator __m256() const
	{
		return m;
	}

	__forceinline GSVector8 abs() const
	{
#if _M_SSE >= 0x501

		return *this & cast(GSVector8i::x7fffffff());

#else

		return *this & m_x7fffffff;

#endif
	}

	__forceinline GSVector8 neg() const
	{
#if _M_SSE >= 0x501

		return *this ^ cast(GSVector8i::x80000000());

#else

		return *this ^ m_x80000000;

#endif
	}

	__forceinline GSVector8 rcp() const
	{
		return GSVector8(_mm256_rcp_ps(m));
	}

	__forceinline GSVector8 rcpnr() const
	{
		GSVector8 v = rcp();

		return (v + v) - (v * v) * *this;
	}

	template <int mode>
	__forceinline GSVector8 round() const
	{
		return GSVector8(_mm256_round_ps(m, mode));
	}

	__forceinline GSVector8 floor() const
	{
		return round<Round_NegInf>();
	}

	__forceinline GSVector8 ceil() const
	{
		return round<Round_PosInf>();
	}

#if _M_SSE >= 0x501

#define LOG8_POLY0(x, c0) GSVector8(c0)
#define LOG8_POLY1(x, c0, c1) (LOG8_POLY0(x, c1).madd(x, GSVector8(c0)))
#define LOG8_POLY2(x, c0, c1, c2) (LOG8_POLY1(x, c1, c2).madd(x, GSVector8(c0)))
#define LOG8_POLY3(x, c0, c1, c2, c3) (LOG8_POLY2(x, c1, c2, c3).madd(x, GSVector8(c0)))
#define LOG8_POLY4(x, c0, c1, c2, c3, c4) (LOG8_POLY3(x, c1, c2, c3, c4).madd(x, GSVector8(c0)))
#define LOG8_POLY5(x, c0, c1, c2, c3, c4, c5) (LOG8_POLY4(x, c1, c2, c3, c4, c5).madd(x, GSVector8(c0)))

	__forceinline GSVector8 log2(int precision = 5) const
	{
		// NOTE: see GSVector4::log2

		GSVector8 one = m_one;

		GSVector8i i = GSVector8i::cast(*this);

		GSVector8 e = GSVector8(((i << 1) >> 24) - GSVector8i::x0000007f());
		GSVector8 m = GSVector8::cast((i << 9) >> 9) | one;

		GSVector8 p;

		switch (precision)
		{
			case 3:
				p = LOG8_POLY2(m, 2.28330284476918490682f, -1.04913055217340124191f, 0.204446009836232697516f);
				break;
			case 4:
				p = LOG8_POLY3(m, 2.61761038894603480148f, -1.75647175389045657003f, 0.688243882994381274313f, -0.107254423828329604454f);
				break;
			default:
			case 5:
				p = LOG8_POLY4(m, 2.8882704548164776201f, -2.52074962577807006663f, 1.48116647521213171641f, -0.465725644288844778798f, 0.0596515482674574969533f);
				break;
			case 6:
				p = LOG8_POLY5(m, 3.1157899f, -3.3241990f, 2.5988452f, -1.2315303f, 3.1821337e-1f, -3.4436006e-2f);
				break;
		}

		// This effectively increases the polynomial degree by one, but ensures that log2(1) == 0

		p = p * (m - one);

		return p + e;
	}

#endif

	__forceinline GSVector8 madd(const GSVector8& a, const GSVector8& b) const
	{
#if 0 //_M_SSE >= 0x501

		return GSVector8(_mm256_fmadd_ps(m, a, b));

#else

		return *this * a + b;

#endif
	}

	__forceinline GSVector8 msub(const GSVector8& a, const GSVector8& b) const
	{
#if 0 //_M_SSE >= 0x501

		return GSVector8(_mm256_fmsub_ps(m, a, b));

#else

		return *this * a - b;

#endif
	}

	__forceinline GSVector8 nmadd(const GSVector8& a, const GSVector8& b) const
	{
#if 0 //_M_SSE >= 0x501

		return GSVector8(_mm256_fnmadd_ps(m, a, b));

#else

		return b - *this * a;

#endif
	}

	__forceinline GSVector8 nmsub(const GSVector8& a, const GSVector8& b) const
	{
#if 0 //_M_SSE >= 0x501

		return GSVector8(_mm256_fnmsub_ps(m, a, b));

#else

		return -b - *this * a;

#endif
	}

	__forceinline GSVector8 addm(const GSVector8& a, const GSVector8& b) const
	{
		return a.madd(b, *this); // *this + a * b
	}

	__forceinline GSVector8 subm(const GSVector8& a, const GSVector8& b) const
	{
		return a.nmadd(b, *this); // *this - a * b
	}

	__forceinline GSVector8 hadd() const
	{
		return GSVector8(_mm256_hadd_ps(m, m));
	}

	__forceinline GSVector8 hadd(const GSVector8& v) const
	{
		return GSVector8(_mm256_hadd_ps(m, v.m));
	}

	__forceinline GSVector8 hsub() const
	{
		return GSVector8(_mm256_hsub_ps(m, m));
	}

	__forceinline GSVector8 hsub(const GSVector8& v) const
	{
		return GSVector8(_mm256_hsub_ps(m, v.m));
	}

	template <int i>
	__forceinline GSVector8 dp(const GSVector8& v) const
	{
		return GSVector8(_mm256_dp_ps(m, v.m, i));
	}

	__forceinline GSVector8 sat(const GSVector8& a, const GSVector8& b) const
	{
		return GSVector8(_mm256_min_ps(_mm256_max_ps(m, a), b));
	}

	__forceinline GSVector8 sat(const GSVector8& a) const
	{
		return GSVector8(_mm256_min_ps(_mm256_max_ps(m, a.xyxy()), a.zwzw()));
	}

	__forceinline GSVector8 sat(const float scale = 255) const
	{
		return sat(zero(), GSVector8(scale));
	}

	__forceinline GSVector8 clamp(const float scale = 255) const
	{
		return min(GSVector8(scale));
	}

	__forceinline GSVector8 min(const GSVector8& a) const
	{
		return GSVector8(_mm256_min_ps(m, a));
	}

	__forceinline GSVector8 max(const GSVector8& a) const
	{
		return GSVector8(_mm256_max_ps(m, a));
	}

	template <int mask>
	__forceinline GSVector8 blend32(const GSVector8& a) const
	{
		return GSVector8(_mm256_blend_ps(m, a, mask));
	}

	__forceinline GSVector8 blend32(const GSVector8& a, const GSVector8& mask) const
	{
		return GSVector8(_mm256_blendv_ps(m, a, mask));
	}

	__forceinline GSVector8 upl(const GSVector8& a) const
	{
		return GSVector8(_mm256_unpacklo_ps(m, a));
	}

	__forceinline GSVector8 uph(const GSVector8& a) const
	{
		return GSVector8(_mm256_unpackhi_ps(m, a));
	}

	__forceinline GSVector8 upl64(const GSVector8& a) const
	{
		return GSVector8(_mm256_castpd_ps(_mm256_unpacklo_pd(_mm256_castps_pd(m), _mm256_castps_pd(a))));
	}

	__forceinline GSVector8 uph64(const GSVector8& a) const
	{
		return GSVector8(_mm256_castpd_ps(_mm256_unpackhi_pd(_mm256_castps_pd(m), _mm256_castps_pd(a))));
	}

	__forceinline GSVector8 l2h() const
	{
		return xyxy();
	}

	__forceinline GSVector8 h2l() const
	{
		return zwzw();
	}

	__forceinline GSVector8 andnot(const GSVector8& v) const
	{
		return GSVector8(_mm256_andnot_ps(v.m, m));
	}

	__forceinline int mask() const
	{
		return _mm256_movemask_ps(m);
	}

	__forceinline bool alltrue() const
	{
		return mask() == 0xff;
	}

	__forceinline bool allfalse() const
	{
		return _mm256_testz_ps(m, m) != 0;
	}

	__forceinline GSVector8 replace_nan(const GSVector8& v) const
	{
		return v.blend32(*this, *this == *this);
	}

	template <int src, int dst>
	__forceinline GSVector8 insert32(const GSVector8& v) const
	{
		// TODO: use blendps when src == dst

		ASSERT(src < 4 && dst < 4); // not cross lane like extract32()

		switch (dst)
		{
			case 0:
				switch (src)
				{
					case 0: return yyxx(v).zxzw(*this);
					case 1: return yyyy(v).zxzw(*this);
					case 2: return yyzz(v).zxzw(*this);
					case 3: return yyww(v).zxzw(*this);
					default: __assume(0);
				}
				break;
			case 1:
				switch (src)
				{
					case 0: return xxxx(v).xzzw(*this);
					case 1: return xxyy(v).xzzw(*this);
					case 2: return xxzz(v).xzzw(*this);
					case 3: return xxww(v).xzzw(*this);
					default: __assume(0);
				}
				break;
			case 2:
				switch (src)
				{
					case 0: return xyzx(wwxx(v));
					case 1: return xyzx(wwyy(v));
					case 2: return xyzx(wwzz(v));
					case 3: return xyzx(wwww(v));
					default: __assume(0);
				}
				break;
			case 3:
				switch (src)
				{
					case 0: return xyxz(zzxx(v));
					case 1: return xyxz(zzyy(v));
					case 2: return xyxz(zzzz(v));
					case 3: return xyxz(zzww(v));
					default: __assume(0);
				}
				break;
			default:
				__assume(0);
		}

		return *this;
	}

	template <int i>
	__forceinline int extract32() const
	{
		ASSERT(i < 8);

		return extract<i / 4>().template extract32<i & 3>();
	}

	template <int i>
	__forceinline GSVector8 insert(__m128 m) const
	{
		ASSERT(i < 2);

		return GSVector8(_mm256_insertf128_ps(this->m, m, i));
	}

	template <int i>
	__forceinline GSVector4 extract() const
	{
		ASSERT(i < 2);

		if (i == 0)
			return GSVector4(_mm256_castps256_ps128(m));

		return GSVector4(_mm256_extractf128_ps(m, i));
	}

	__forceinline static GSVector8 zero()
	{
		return GSVector8(_mm256_setzero_ps());
	}

	__forceinline static GSVector8 xffffffff()
	{
		return zero() == zero();
	}

	// TODO

	__forceinline static GSVector8 loadl(const void* p)
	{
		return GSVector8(_mm256_castps128_ps256(_mm_load_ps((float*)p)));
	}

	__forceinline static GSVector8 loadh(const void* p)
	{
		return zero().insert<1>(_mm_load_ps((float*)p));
	}

	__forceinline static GSVector8 loadh(const void* p, const GSVector8& v)
	{
		return GSVector8(_mm256_insertf128_ps(v, _mm_load_ps((float*)p), 1));
	}

	__forceinline static GSVector8 load(const void* pl, const void* ph)
	{
		return loadh(ph, loadl(pl));
	}

	template <bool aligned>
	__forceinline static GSVector8 load(const void* p)
	{
		return GSVector8(aligned ? _mm256_load_ps((const float*)p) : _mm256_loadu_ps((const float*)p));
	}

	// TODO

	__forceinline static void storel(void* p, const GSVector8& v)
	{
		_mm_store_ps((float*)p, _mm256_extractf128_ps(v.m, 0));
	}

	__forceinline static void storeh(void* p, const GSVector8& v)
	{
		_mm_store_ps((float*)p, _mm256_extractf128_ps(v.m, 1));
	}

	template <bool aligned>
	__forceinline static void store(void* p, const GSVector8& v)
	{
		if (aligned)
			_mm256_store_ps((float*)p, v.m);
		else
			_mm256_storeu_ps((float*)p, v.m);
	}

	//

	__forceinline static void zeroupper()
	{
		_mm256_zeroupper();
	}

	__forceinline static void zeroall()
	{
		_mm256_zeroall();
	}

	//

	__forceinline GSVector8 operator-() const
	{
		return neg();
	}

	__forceinline void operator+=(const GSVector8& v)
	{
		m = _mm256_add_ps(m, v);
	}

	__forceinline void operator-=(const GSVector8& v)
	{
		m = _mm256_sub_ps(m, v);
	}

	__forceinline void operator*=(const GSVector8& v)
	{
		m = _mm256_mul_ps(m, v);
	}

	__forceinline void operator/=(const GSVector8& v)
	{
		m = _mm256_div_ps(m, v);
	}

	__forceinline void operator+=(float f)
	{
		*this += GSVector8(f);
	}

	__forceinline void operator-=(float f)
	{
		*this -= GSVector8(f);
	}

	__forceinline void operator*=(float f)
	{
		*this *= GSVector8(f);
	}

	__forceinline void operator/=(float f)
	{
		*this /= GSVector8(f);
	}

	__forceinline void operator&=(const GSVector8& v)
	{
		m = _mm256_and_ps(m, v);
	}

	__forceinline void operator|=(const GSVector8& v)
	{
		m = _mm256_or_ps(m, v);
	}

	__forceinline void operator^=(const GSVector8& v)
	{
		m = _mm256_xor_ps(m, v);
	}

	__forceinline friend GSVector8 operator+(const GSVector8& v1, const GSVector8& v2)
	{
		return GSVector8(_mm256_add_ps(v1, v2));
	}

	__forceinline friend GSVector8 operator-(const GSVector8& v1, const GSVector8& v2)
	{
		return GSVector8(_mm256_sub_ps(v1, v2));
	}

	__forceinline friend GSVector8 operator*(const GSVector8& v1, const GSVector8& v2)
	{
		return GSVector8(_mm256_mul_ps(v1, v2));
	}

	__forceinline friend GSVector8 operator/(const GSVector8& v1, const GSVector8& v2)
	{
		return GSVector8(_mm256_div_ps(v1, v2));
	}

	__forceinline friend GSVector8 operator+(const GSVector8& v, float f)
	{
		return v + GSVector8(f);
	}

	__forceinline friend GSVector8 operator-(const GSVector8& v, float f)
	{
		return v - GSVector8(f);
	}

	__forceinline friend GSVector8 operator*(const GSVector8& v, float f)
	{
		return v * GSVector8(f);
	}

	__forceinline friend GSVector8 operator/(const GSVector8& v, float f)
	{
		return v / GSVector8(f);
	}

	__forceinline friend GSVector8 operator&(const GSVector8& v1, const GSVector8& v2)
	{
		return GSVector8(_mm256_and_ps(v1, v2));
	}

	__forceinline friend GSVector8 operator|(const GSVector8& v1, const GSVector8& v2)
	{
		return GSVector8(_mm256_or_ps(v1, v2));
	}

	__forceinline friend GSVector8 operator^(const GSVector8& v1, const GSVector8& v2)
	{
		return GSVector8(_mm256_xor_ps(v1, v2));
	}

	__forceinline friend GSVector8 operator==(const GSVector8& v1, const GSVector8& v2)
	{
		return GSVector8(_mm256_cmp_ps(v1, v2, _CMP_EQ_OQ));
	}

	__forceinline friend GSVector8 operator!=(const GSVector8& v1, const GSVector8& v2)
	{
		return GSVector8(_mm256_cmp_ps(v1, v2, _CMP_NEQ_OQ));
	}

	__forceinline friend GSVector8 operator>(const GSVector8& v1, const GSVector8& v2)
	{
		return GSVector8(_mm256_cmp_ps(v1, v2, _CMP_GT_OQ));
	}

	__forceinline friend GSVector8 operator<(const GSVector8& v1, const GSVector8& v2)
	{
		return GSVector8(_mm256_cmp_ps(v1, v2, _CMP_LT_OQ));
	}

	__forceinline friend GSVector8 operator>=(const GSVector8& v1, const GSVector8& v2)
	{
		return GSVector8(_mm256_cmp_ps(v1, v2, _CMP_GE_OQ));
	}

	__forceinline friend GSVector8 operator<=(const GSVector8& v1, const GSVector8& v2)
	{
		return GSVector8(_mm256_cmp_ps(v1, v2, _CMP_LE_OQ));
	}

	__forceinline GSVector8 mul64(const GSVector8& v) const
	{
		return GSVector8(_mm256_mul_pd(_mm256_castps_pd(m), _mm256_castps_pd(v.m)));
	}

	__forceinline GSVector8 add64(const GSVector8& v) const
	{
		return GSVector8(_mm256_add_pd(_mm256_castps_pd(m), _mm256_castps_pd(v.m)));
	}

	__forceinline GSVector8 sub64(const GSVector8& v) const
	{
		return GSVector8(_mm256_sub_pd(_mm256_castps_pd(m), _mm256_castps_pd(v.m)));
	}

	__forceinline static GSVector8 f32to64(const GSVector4& v)
	{
		return GSVector8(_mm256_cvtps_pd(v.m));
	}

	__forceinline static GSVector8 f32to64(const void* p)
	{
		return GSVector8(_mm256_cvtps_pd(_mm_load_ps(static_cast<const float*>(p))));
	}

	__forceinline GSVector4i f64toi32(bool truncate = true) const
	{
		return GSVector4i(truncate ? _mm256_cvttpd_epi32(_mm256_castps_pd(m)) : _mm256_cvtpd_epi32(_mm256_castps_pd(m)));
	}

	// clang-format off

	// x = v[31:0] / v[159:128]
	// y = v[63:32] / v[191:160]
	// z = v[95:64] / v[223:192]
	// w = v[127:96] / v[255:224]

	#define VECTOR8_SHUFFLE_4(xs, xn, ys, yn, zs, zn, ws, wn) \
		__forceinline GSVector8 xs##ys##zs##ws() const { return GSVector8(_mm256_shuffle_ps(m, m, _MM_SHUFFLE(wn, zn, yn, xn))); } \
		__forceinline GSVector8 xs##ys##zs##ws(const GSVector8& v) const { return GSVector8(_mm256_shuffle_ps(m, v.m, _MM_SHUFFLE(wn, zn, yn, xn))); }

		// vs2012u3 cannot reuse the result of equivalent shuffles when it is done with _mm256_permute_ps (write v.xxxx() twice, and it will do it twice), but with _mm256_shuffle_ps it can.
		//__forceinline GSVector8 xs##ys##zs##ws() const { return GSVector8(_mm256_permute_ps(m, _MM_SHUFFLE(wn, zn, yn, xn))); }

	#define VECTOR8_SHUFFLE_3(xs, xn, ys, yn, zs, zn) \
		VECTOR8_SHUFFLE_4(xs, xn, ys, yn, zs, zn, x, 0) \
		VECTOR8_SHUFFLE_4(xs, xn, ys, yn, zs, zn, y, 1) \
		VECTOR8_SHUFFLE_4(xs, xn, ys, yn, zs, zn, z, 2) \
		VECTOR8_SHUFFLE_4(xs, xn, ys, yn, zs, zn, w, 3) \

	#define VECTOR8_SHUFFLE_2(xs, xn, ys, yn) \
		VECTOR8_SHUFFLE_3(xs, xn, ys, yn, x, 0) \
		VECTOR8_SHUFFLE_3(xs, xn, ys, yn, y, 1) \
		VECTOR8_SHUFFLE_3(xs, xn, ys, yn, z, 2) \
		VECTOR8_SHUFFLE_3(xs, xn, ys, yn, w, 3) \

	#define VECTOR8_SHUFFLE_1(xs, xn) \
		VECTOR8_SHUFFLE_2(xs, xn, x, 0) \
		VECTOR8_SHUFFLE_2(xs, xn, y, 1) \
		VECTOR8_SHUFFLE_2(xs, xn, z, 2) \
		VECTOR8_SHUFFLE_2(xs, xn, w, 3) \

	VECTOR8_SHUFFLE_1(x, 0)
	VECTOR8_SHUFFLE_1(y, 1)
	VECTOR8_SHUFFLE_1(z, 2)
	VECTOR8_SHUFFLE_1(w, 3)

	// a = v0[127:0]
	// b = v0[255:128]
	// c = v1[127:0]
	// d = v1[255:128]
	// _ = 0

	#define VECTOR8_PERMUTE128_2(as, an, bs, bn) \
		__forceinline GSVector8 as##bs() const { return GSVector8(_mm256_permute2f128_ps(m, m, an | (bn << 4))); } \
		__forceinline GSVector8 as##bs(const GSVector8& v) const { return GSVector8(_mm256_permute2f128_ps(m, v.m, an | (bn << 4))); } \

	#define VECTOR8_PERMUTE128_1(as, an) \
		VECTOR8_PERMUTE128_2(as, an, a, 0) \
		VECTOR8_PERMUTE128_2(as, an, b, 1) \
		VECTOR8_PERMUTE128_2(as, an, c, 2) \
		VECTOR8_PERMUTE128_2(as, an, d, 3) \
		VECTOR8_PERMUTE128_2(as, an, _, 8) \

	VECTOR8_PERMUTE128_1(a, 0)
	VECTOR8_PERMUTE128_1(b, 1)
	VECTOR8_PERMUTE128_1(c, 2)
	VECTOR8_PERMUTE128_1(d, 3)
	VECTOR8_PERMUTE128_1(_, 8)

#if _M_SSE >= 0x501

	// a = v[63:0]
	// b = v[127:64]
	// c = v[191:128]
	// d = v[255:192]

	#define VECTOR8_PERMUTE64_4(as, an, bs, bn, cs, cn, ds, dn) \
		__forceinline GSVector8 as##bs##cs##ds() const { return GSVector8(_mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(m), _MM_SHUFFLE(dn, cn, bn, an)))); } \

	#define VECTOR8_PERMUTE64_3(as, an, bs, bn, cs, cn) \
		VECTOR8_PERMUTE64_4(as, an, bs, bn, cs, cn, a, 0) \
		VECTOR8_PERMUTE64_4(as, an, bs, bn, cs, cn, b, 1) \
		VECTOR8_PERMUTE64_4(as, an, bs, bn, cs, cn, c, 2) \
		VECTOR8_PERMUTE64_4(as, an, bs, bn, cs, cn, d, 3) \

	#define VECTOR8_PERMUTE64_2(as, an, bs, bn) \
		VECTOR8_PERMUTE64_3(as, an, bs, bn, a, 0) \
		VECTOR8_PERMUTE64_3(as, an, bs, bn, b, 1) \
		VECTOR8_PERMUTE64_3(as, an, bs, bn, c, 2) \
		VECTOR8_PERMUTE64_3(as, an, bs, bn, d, 3) \

	#define VECTOR8_PERMUTE64_1(as, an) \
		VECTOR8_PERMUTE64_2(as, an, a, 0) \
		VECTOR8_PERMUTE64_2(as, an, b, 1) \
		VECTOR8_PERMUTE64_2(as, an, c, 2) \
		VECTOR8_PERMUTE64_2(as, an, d, 3) \

	VECTOR8_PERMUTE64_1(a, 0)
	VECTOR8_PERMUTE64_1(b, 1)
	VECTOR8_PERMUTE64_1(c, 2)
	VECTOR8_PERMUTE64_1(d, 3)

	// clang-format on

	__forceinline GSVector8 permute32(const GSVector8i& mask) const
	{
		return GSVector8(_mm256_permutevar8x32_ps(m, mask));
	}

	__forceinline GSVector8 broadcast32() const
	{
		return GSVector8(_mm256_broadcastss_ps(_mm256_castps256_ps128(m)));
	}

	__forceinline static GSVector8 broadcast32(const GSVector4& v)
	{
		return GSVector8(_mm256_broadcastss_ps(v.m));
	}

	__forceinline static GSVector8 broadcast32(const void* f)
	{
		return GSVector8(_mm256_broadcastss_ps(_mm_load_ss((const float*)f)));
	}

	__forceinline static GSVector8 broadcast64(const void* d)
	{
		return GSVector8(_mm256_broadcast_sd(static_cast<const double*>(d)));
	}

	// TODO: v.(x0|y0|z0|w0|x1|y1|z1|w1) // broadcast element

#endif
};

#endif
