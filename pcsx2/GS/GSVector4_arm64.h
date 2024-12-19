// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

class alignas(16) GSVector4
{
	struct cxpr_init_tag
	{
	};
	static constexpr cxpr_init_tag cxpr_init{};

	constexpr GSVector4(cxpr_init_tag, float x, float y, float z, float w)
		: F32{x, y, z, w}
	{
	}

	constexpr GSVector4(cxpr_init_tag, int x, int y, int z, int w)
		: I32{x, y, z, w}
	{
	}

	constexpr GSVector4(cxpr_init_tag, u64 x, u64 y)
		: U64{x, y}
	{
	}

public:
	union
	{
		struct { float x, y, z, w; };
		struct { float r, g, b, a; };
		struct { float left, top, right, bottom; };
		float v[4];
		float F32[4];
		double F64[2];
		s8 I8[16];
		s16 I16[8];
		s32 I32[4];
		s64 I64[2];
		u8 U8[16];
		u16 U16[8];
		u32 U32[4];
		u64 U64[2];
		float32x4_t v4s;
	};

	static const GSVector4 m_ps0123;
	static const GSVector4 m_ps4567;
	static const GSVector4 m_half;
	static const GSVector4 m_one;
	static const GSVector4 m_two;
	static const GSVector4 m_four;
	static const GSVector4 m_x4b000000;
	static const GSVector4 m_x4f800000;
	static const GSVector4 m_xc1e00000000fffff;
	static const GSVector4 m_max;
	static const GSVector4 m_min;

	GSVector4() = default;

	constexpr static GSVector4 cxpr(float x, float y, float z, float w)
	{
		return GSVector4(cxpr_init, x, y, z, w);
	}

	constexpr static GSVector4 cxpr(float x)
	{
		return GSVector4(cxpr_init, x, x, x, x);
	}

	constexpr static GSVector4 cxpr(int x, int y, int z, int w)
	{
		return GSVector4(cxpr_init, x, y, z, w);
	}

	constexpr static GSVector4 cxpr(int x)
	{
		return GSVector4(cxpr_init, x, x, x, x);
	}

	constexpr static GSVector4 cxpr64(u64 x, u64 y)
	{
		return GSVector4(cxpr_init, x, y);
	}

	constexpr static GSVector4 cxpr64(u64 x)
	{
		return GSVector4(cxpr_init, x, x);
	}

	__forceinline GSVector4(float x, float y, float z, float w)
	{
		const float arr[4] = { x, y, z, w };
		v4s = vld1q_f32(arr);
	}

	__forceinline GSVector4(float x, float y)
	{
		v4s = vzip1q_f32(vsetq_lane_f32(x, vdupq_n_f32(0.0f), 0), vsetq_lane_f32(y, vdupq_n_f32(0.0f), 0));
	}

	__forceinline GSVector4(int x, int y, int z, int w)
	{
		const int arr[4] = { x, y, z, w };
		v4s = vcvtq_f32_s32(vld1q_s32(arr));
	}

	__forceinline GSVector4(int x, int y)
	{
		v4s = vcvtq_f32_s32(vzip1q_s32(vsetq_lane_s32(x, vdupq_n_s32(0), 0), vsetq_lane_s32(y, vdupq_n_s32(0), 0)));
	}

	__forceinline explicit GSVector4(const GSVector2& v)
	{
		v4s = vcombine_f32(vld1_f32(v.v), vcreate_f32(0));
	}

	__forceinline explicit GSVector4(const GSVector2i& v)
	{
		v4s = vcvtq_f32_s32(vcombine_s32(vld1_s32(v.v), vcreate_s32(0)));
	}

	__forceinline constexpr explicit GSVector4(float32x4_t m)
		: v4s(m)
	{
	}

	__forceinline explicit GSVector4(float f)
	{
		v4s = vdupq_n_f32(f);
	}

	__forceinline explicit GSVector4(int i)
	{
		v4s = vcvtq_f32_s32(vdupq_n_s32(i));
	}

	__forceinline explicit GSVector4(u32 u)
	{
		GSVector4i v((int)u);

		*this = GSVector4(v) + (m_x4f800000 & GSVector4::cast(v.sra32<31>()));
	}

	__forceinline explicit GSVector4(const GSVector4i& v);

	__forceinline static GSVector4 cast(const GSVector4i& v);

	__forceinline static GSVector4 f64(double x, double y)
	{
		return GSVector4(vreinterpretq_f32_f64(vsetq_lane_f64(y, vdupq_n_f64(x), 1)));
	}

	__forceinline void operator=(float f)
	{
		v4s = vdupq_n_f32(f);
	}

	__forceinline void operator=(float32x4_t m)
	{
		v4s = m;
	}

	__forceinline operator float32x4_t() const
	{
		return v4s;
	}

	/// Makes Clang think that the whole vector is needed, preventing it from changing shuffles around because it thinks we don't need the whole vector
	/// Useful for e.g. preventing clang from optimizing shuffles that remove possibly-denormal garbage data from vectors before computing with them
	__forceinline GSVector4 noopt()
	{
		// Note: Clang is currently the only compiler that attempts to optimize vector intrinsics, if that changes in the future the implementation should be updated
#ifdef __clang__
		// __asm__("":"+x"(m)::);
#endif
		return *this;
	}

	__forceinline u32 rgba32() const
	{
		return GSVector4i(*this).rgba32();
	}

	__forceinline static GSVector4 rgba32(u32 rgba)
	{
		return GSVector4(GSVector4i::load((int)rgba).u8to32());
	}

	__forceinline static GSVector4 rgba32(u32 rgba, int shift)
	{
		return GSVector4(GSVector4i::load((int)rgba).u8to32() << shift);
	}

	__forceinline static GSVector4 unorm8(u32 rgba)
	{
		return rgba32(rgba) * GSVector4::cxpr(1.0f / 255.0f);
	}

	__forceinline GSVector4 abs() const
	{
		return GSVector4(vabsq_f32(v4s));
	}

	__forceinline GSVector4 neg() const
	{
		return GSVector4(vnegq_f32(v4s));
	}

	__forceinline GSVector4 rcp() const
	{
		return GSVector4(vrecpeq_f32(v4s));
	}

	__forceinline GSVector4 rcpnr() const
	{
		float32x4_t recip = vrecpeq_f32(v4s);
		recip = vmulq_f32(recip, vrecpsq_f32(recip, v4s));
		return GSVector4(recip);
	}

	template <int mode>
	__forceinline GSVector4 round() const
	{
		if constexpr (mode == Round_NegInf)
			return floor();
		else if constexpr (mode == Round_PosInf)
			return ceil();
		else if constexpr (mode == Round_NearestInt)
			return GSVector4(vrndnq_f32(v4s));
    else
      return GSVector4(vrndq_f32(v4s));
	}

	__forceinline GSVector4 floor() const
	{
		return GSVector4(vrndmq_f32(v4s));
	}

	__forceinline GSVector4 ceil() const
	{
		return GSVector4(vrndpq_f32(v4s));
	}

	// http://jrfonseca.blogspot.com/2008/09/fast-sse2-pow-tables-or-polynomials.html

#define LOG_POLY0(x, c0) GSVector4(c0)
#define LOG_POLY1(x, c0, c1) (LOG_POLY0(x, c1).madd(x, GSVector4(c0)))
#define LOG_POLY2(x, c0, c1, c2) (LOG_POLY1(x, c1, c2).madd(x, GSVector4(c0)))
#define LOG_POLY3(x, c0, c1, c2, c3) (LOG_POLY2(x, c1, c2, c3).madd(x, GSVector4(c0)))
#define LOG_POLY4(x, c0, c1, c2, c3, c4) (LOG_POLY3(x, c1, c2, c3, c4).madd(x, GSVector4(c0)))
#define LOG_POLY5(x, c0, c1, c2, c3, c4, c5) (LOG_POLY4(x, c1, c2, c3, c4, c5).madd(x, GSVector4(c0)))

	__forceinline GSVector4 log2(int precision = 5) const
	{
		// NOTE: sign bit ignored, safe to pass negative numbers

		// The idea behind this algorithm is to split the float into two parts, log2(m * 2^e) => log2(m) + log2(2^e) => log2(m) + e,
		// and then approximate the logarithm of the mantissa (it's 1.x when normalized, a nice short range).

		GSVector4 one = m_one;

		GSVector4i i = GSVector4i::cast(*this);

		GSVector4 e = GSVector4(((i << 1) >> 24) - GSVector4i::x0000007f());
		GSVector4 m = GSVector4::cast((i << 9) >> 9) | one;

		GSVector4 p;

		// Minimax polynomial fit of log2(x)/(x - 1), for x in range [1, 2[

		switch (precision)
		{
			case 3:
				p = LOG_POLY2(m, 2.28330284476918490682f, -1.04913055217340124191f, 0.204446009836232697516f);
				break;
			case 4:
				p = LOG_POLY3(m, 2.61761038894603480148f, -1.75647175389045657003f, 0.688243882994381274313f, -0.107254423828329604454f);
				break;
			default:
			case 5:
				p = LOG_POLY4(m, 2.8882704548164776201f, -2.52074962577807006663f, 1.48116647521213171641f, -0.465725644288844778798f, 0.0596515482674574969533f);
				break;
			case 6:
				p = LOG_POLY5(m, 3.1157899f, -3.3241990f, 2.5988452f, -1.2315303f, 3.1821337e-1f, -3.4436006e-2f);
				break;
		}

		// This effectively increases the polynomial degree by one, but ensures that log2(1) == 0

		p = p * (m - one);

		return p + e;
	}

	__forceinline GSVector4 madd(const GSVector4& a, const GSVector4& b) const
	{
		return *this * a + b;
	}

	__forceinline GSVector4 msub(const GSVector4& a, const GSVector4& b) const
	{
		return *this * a - b;
	}

	__forceinline GSVector4 nmadd(const GSVector4& a, const GSVector4& b) const
	{
		return b - *this * a;
	}

	__forceinline GSVector4 nmsub(const GSVector4& a, const GSVector4& b) const
	{
		return -b - *this * a;
	}

	__forceinline GSVector4 addm(const GSVector4& a, const GSVector4& b) const
	{
		return a.madd(b, *this); // *this + a * b
	}

	__forceinline GSVector4 subm(const GSVector4& a, const GSVector4& b) const
	{
		return a.nmadd(b, *this); // *this - a * b
	}

	__forceinline GSVector4 hadd() const
	{
		return GSVector4(vpaddq_f32(v4s, v4s));
	}

	__forceinline GSVector4 hadd(const GSVector4& v) const
	{
		return GSVector4(vpaddq_f32(v4s, v.v4s));
	}

	__forceinline GSVector4 hsub() const
	{
		return GSVector4(vsubq_f32(vuzp1q_f32(v4s, v4s), vuzp2q_f32(v4s, v4s)));
	}

	__forceinline GSVector4 hsub(const GSVector4& v) const
	{
		return GSVector4(vsubq_f32(vuzp1q_f32(v4s, v.v4s), vuzp2q_f32(v4s, v.v4s)));
	}

	__forceinline GSVector4 sat(const GSVector4& a, const GSVector4& b) const
	{
		return max(a).min(b);
	}

	__forceinline GSVector4 sat(const GSVector4& a) const
	{
		const GSVector4 minv(vreinterpretq_f32_f64(vdupq_laneq_f64(vreinterpretq_f64_f32(a.v4s), 0)));
		const GSVector4 maxv(vreinterpretq_f32_f64(vdupq_laneq_f64(vreinterpretq_f64_f32(a.v4s), 1)));
		return sat(minv, maxv);
	}

	__forceinline GSVector4 sat(const float scale = 255) const
	{
		return sat(zero(), GSVector4(scale));
	}

	__forceinline GSVector4 clamp(const float scale = 255) const
	{
		return min(GSVector4(scale));
	}

	__forceinline GSVector4 min(const GSVector4& a) const
	{
		return GSVector4(vminq_f32(v4s, a.v4s));
	}

	__forceinline GSVector4 max(const GSVector4& a) const
	{
    return GSVector4(vmaxq_f32(v4s, a.v4s));
	}

	template <int mask>
	__forceinline GSVector4 blend32(const GSVector4& a) const
	{
		return GSVector4(__builtin_shufflevector(v4s, a.v4s, (mask & 1) ? 4 : 0, (mask & 2) ? 5 : 1, (mask & 4) ? 6 : 2, (mask & 8) ? 7 : 3));
	}

	__forceinline GSVector4 blend32(const GSVector4& a, const GSVector4& mask) const
	{
		// duplicate sign bit across and bit select
		const uint32x4_t bitmask = vreinterpretq_u32_s32(vshrq_n_s32(vreinterpretq_s32_f32(mask.v4s), 31));
		return GSVector4(vbslq_f32(bitmask, a.v4s, v4s));
	}

	__forceinline GSVector4 upl(const GSVector4& a) const
	{
		return GSVector4(vzip1q_f32(v4s, a.v4s));
	}

	__forceinline GSVector4 uph(const GSVector4& a) const
	{
		return GSVector4(vzip2q_f32(v4s, a.v4s));
	}

	__forceinline GSVector4 upld(const GSVector4& a) const
	{
		return GSVector4(vreinterpretq_f32_f64(vzip1q_f64(vreinterpretq_f64_f32(v4s), vreinterpretq_f64_f32(a.v4s))));
	}

	__forceinline GSVector4 uphd(const GSVector4& a) const
	{
		return GSVector4(vreinterpretq_f32_f64(vzip2q_f64(vreinterpretq_f64_f32(v4s), vreinterpretq_f64_f32(a.v4s))));
	}

	__forceinline GSVector4 l2h(const GSVector4& a) const
	{
		return GSVector4(vcombine_f32(vget_low_f32(v4s), vget_low_f32(a.v4s)));
	}

	__forceinline GSVector4 h2l(const GSVector4& a) const
	{
		return GSVector4(vcombine_f32(vget_high_f32(v4s), vget_high_f32(a.v4s)));
	}

	__forceinline GSVector4 andnot(const GSVector4& v) const
	{
		return GSVector4(vreinterpretq_f32_s32(vbicq_s32(vreinterpretq_s32_f32(v4s), vreinterpretq_s32_f32(v.v4s))));
	}

	__forceinline int mask() const
	{
    static const int32_t shifts[] = { 0, 1, 2, 3 };
    return static_cast<int>(vaddvq_u32(vshlq_u32(vshrq_n_u32(vreinterpretq_u32_f32(v4s), 31), vld1q_s32(shifts))));
	}

	__forceinline bool alltrue() const
	{
		// return mask() == 0xf;
		return ~(vgetq_lane_u64(vreinterpretq_u64_f32(v4s), 0) & vgetq_lane_u64(vreinterpretq_u64_f32(v4s), 1)) == 0;
	}

	__forceinline bool allfalse() const
	{
		return (vgetq_lane_u64(vreinterpretq_u64_f32(v4s), 0) | vgetq_lane_u64(vreinterpretq_u64_f32(v4s), 1)) == 0;
	}

	__forceinline GSVector4 replace_nan(const GSVector4& v) const
	{
		return v.blend32(*this, *this == *this);
	}

	template <int src, int dst>
	__forceinline GSVector4 insert32(const GSVector4& v) const
	{
		return GSVector4(vcopyq_laneq_f32(v4s, dst, v.v4s, src));
	}

	template <int i>
	__forceinline int extract32() const
	{
		return vgetq_lane_s32(vreinterpretq_s32_f32(v4s), i);
	}

	__forceinline static GSVector4 zero()
	{
		return GSVector4(vdupq_n_f32(0.0f));
	}

	__forceinline static GSVector4 xffffffff()
	{
		return GSVector4(vreinterpretq_f32_u32(vdupq_n_u32(0xFFFFFFFFu)));
	}

	__forceinline static GSVector4 ps0123()
	{
		return GSVector4(m_ps0123);
	}

	__forceinline static GSVector4 ps4567()
	{
		return GSVector4(m_ps4567);
	}

	__forceinline static GSVector4 loadl(const void* p)
	{
		return GSVector4(vcombine_f32(vld1_f32((const float*)p), vcreate_f32(0)));
	}

	__forceinline static GSVector4 load(float f)
	{
		return GSVector4(vsetq_lane_f32(f, vmovq_n_f32(0.0f), 0));
	}

	__forceinline static GSVector4 load(u32 u)
	{
		GSVector4i v = GSVector4i::load((int)u);

		return GSVector4(v) + (m_x4f800000 & GSVector4::cast(v.sra32<31>()));
	}

	template <bool aligned>
	__forceinline static GSVector4 load(const void* p)
	{
		return GSVector4(vld1q_f32((const float*)p));
	}

	__forceinline static void storent(void* p, const GSVector4& v)
	{
    vst1q_f32((float*)p, v.v4s);
	}

	__forceinline static void storel(void* p, const GSVector4& v)
	{
		vst1_f64((double*)p, vget_low_f64(vreinterpretq_f64_f32(v.v4s)));
	}

	__forceinline static void storeh(void* p, const GSVector4& v)
	{
		vst1_f64((double*)p, vget_high_f64(vreinterpretq_f64_f32(v.v4s)));
	}

	template <bool aligned>
	__forceinline static void store(void* p, const GSVector4& v)
	{
		vst1q_f32((float*)p, v.v4s);
	}

	__forceinline static void store(float* p, const GSVector4& v)
	{
		vst1q_lane_f32(p, v.v4s, 0);
	}

	__forceinline static void expand(const GSVector4i& v, GSVector4& a, GSVector4& b, GSVector4& c, GSVector4& d)
	{
		GSVector4i mask = GSVector4i::x000000ff();

		a = GSVector4(v & mask);
		b = GSVector4((v >> 8) & mask);
		c = GSVector4((v >> 16) & mask);
		d = GSVector4((v >> 24));
	}

	__forceinline static void transpose(GSVector4& a, GSVector4& b, GSVector4& c, GSVector4& d)
	{
		GSVector4 v0 = a.xyxy(b);
		GSVector4 v1 = c.xyxy(d);

		GSVector4 e = v0.xzxz(v1);
		GSVector4 f = v0.ywyw(v1);

		GSVector4 v2 = a.zwzw(b);
		GSVector4 v3 = c.zwzw(d);

		GSVector4 g = v2.xzxz(v3);
		GSVector4 h = v2.ywyw(v3);

		a = e;
		b = f;
		c = g;
		d = h;
	}

	__forceinline GSVector4 operator-() const
	{
		return neg();
	}

	__forceinline void operator+=(const GSVector4& v)
	{
		v4s = vaddq_f32(v4s, v.v4s);
	}

	__forceinline void operator-=(const GSVector4& v)
	{
		v4s = vsubq_f32(v4s, v.v4s);
	}

	__forceinline void operator*=(const GSVector4& v)
	{
		v4s = vmulq_f32(v4s, v.v4s);
	}

	__forceinline void operator/=(const GSVector4& v)
	{
		v4s = vdivq_f32(v4s, v.v4s);
	}

	__forceinline void operator+=(float f)
	{
		*this += GSVector4(f);
	}

	__forceinline void operator-=(float f)
	{
		*this -= GSVector4(f);
	}

	__forceinline void operator*=(float f)
	{
		*this *= GSVector4(f);
	}

	__forceinline void operator/=(float f)
	{
		*this /= GSVector4(f);
	}

	__forceinline void operator&=(const GSVector4& v)
	{
		v4s = vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(v4s), vreinterpretq_u32_f32(v.v4s)));
	}

	__forceinline void operator|=(const GSVector4& v)
	{
		v4s = vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(v4s), vreinterpretq_u32_f32(v.v4s)));
	}

	__forceinline void operator^=(const GSVector4& v)
	{
		v4s = vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(v4s), vreinterpretq_u32_f32(v.v4s)));
	}

	__forceinline friend GSVector4 operator+(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(vaddq_f32(v1.v4s, v2.v4s));
	}

	__forceinline friend GSVector4 operator-(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(vsubq_f32(v1.v4s, v2.v4s));
	}

	__forceinline friend GSVector4 operator*(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(vmulq_f32(v1.v4s, v2.v4s));
	}

	__forceinline friend GSVector4 operator/(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(vdivq_f32(v1.v4s, v2.v4s));
	}

	__forceinline friend GSVector4 operator+(const GSVector4& v, float f)
	{
		return v + GSVector4(f);
	}

	__forceinline friend GSVector4 operator-(const GSVector4& v, float f)
	{
		return v - GSVector4(f);
	}

	__forceinline friend GSVector4 operator*(const GSVector4& v, float f)
	{
		return v * GSVector4(f);
	}

	__forceinline friend GSVector4 operator/(const GSVector4& v, float f)
	{
		return v / GSVector4(f);
	}

	__forceinline friend GSVector4 operator&(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(v1.v4s), vreinterpretq_u32_f32(v2.v4s))));
	}

	__forceinline friend GSVector4 operator|(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(v1.v4s), vreinterpretq_u32_f32(v2.v4s))));
	}

	__forceinline friend GSVector4 operator^(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(v1.v4s), vreinterpretq_u32_f32(v2.v4s))));
	}

	__forceinline friend GSVector4 operator==(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(vreinterpretq_f32_u32(vceqq_f32(v1.v4s, v2.v4s)));
	}

	__forceinline friend GSVector4 operator!=(const GSVector4& v1, const GSVector4& v2)
	{
		// NEON has no !=
		return GSVector4(vreinterpretq_f32_u32(vmvnq_u32(vceqq_f32(v1.v4s, v2.v4s))));
	}

	__forceinline friend GSVector4 operator>(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(vreinterpretq_f32_u32(vcgtq_f32(v1.v4s, v2.v4s)));
	}

	__forceinline friend GSVector4 operator<(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(vreinterpretq_f32_u32(vcltq_f32(v1.v4s, v2.v4s)));
	}

	__forceinline friend GSVector4 operator>=(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(vreinterpretq_f32_u32(vcgeq_f32(v1.v4s, v2.v4s)));
	}

	__forceinline friend GSVector4 operator<=(const GSVector4& v1, const GSVector4& v2)
	{
		return GSVector4(vreinterpretq_f32_u32(vcleq_f32(v1.v4s, v2.v4s)));
	}

	__forceinline GSVector4 mul64(const GSVector4& v) const
	{
		return GSVector4(vmulq_f64(vreinterpretq_f64_f32(v4s), vreinterpretq_f64_f32(v.v4s)));
	}

	__forceinline GSVector4 add64(const GSVector4& v) const
	{
		return GSVector4(vaddq_f64(vreinterpretq_f64_f32(v4s), vreinterpretq_f64_f32(v.v4s)));
	}

	__forceinline GSVector4 sub64(const GSVector4& v) const
	{
		return GSVector4(vsubq_f64(vreinterpretq_f64_f32(v4s), vreinterpretq_f64_f32(v.v4s)));
	}

	__forceinline static GSVector4 f32to64(const GSVector4& v)
	{
		return GSVector4(vreinterpretq_f32_f64(vcvt_f64_f32(vget_low_f32(v.v4s))));
	}

	__forceinline static GSVector4 f32to64(const void* p)
	{
		return GSVector4(vreinterpretq_f32_f64(vcvt_f64_f32(vld1_f32(static_cast<const float*>(p)))));
	}

	__forceinline GSVector4i f64toi32(bool truncate = true) const
	{
		const float64x2_t r = truncate ? v4s : vrndiq_f64(vreinterpretq_f64_f32(v4s));
		const s32 low = static_cast<s32>(vgetq_lane_f64(r, 0));
		const s32 high = static_cast<s32>(vgetq_lane_f64(r, 1));
		return GSVector4i(vsetq_lane_s32(high, vsetq_lane_s32(low, vdupq_n_s32(0), 0), 1));
	}

	// clang-format off

	#define VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, ws, wn) \
		__forceinline GSVector4 xs##ys##zs##ws() const { return GSVector4(__builtin_shufflevector(v4s, v4s, xn, yn, zn, wn)); } \
		__forceinline GSVector4 xs##ys##zs##ws(const GSVector4& v) const { return GSVector4(__builtin_shufflevector(v4s, v.v4s, xn, yn, 4 + zn, 4 + wn)); }

	#define VECTOR4_SHUFFLE_3(xs, xn, ys, yn, zs, zn) \
		VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, x, 0) \
		VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, y, 1) \
		VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, z, 2) \
		VECTOR4_SHUFFLE_4(xs, xn, ys, yn, zs, zn, w, 3) \

	#define VECTOR4_SHUFFLE_2(xs, xn, ys, yn) \
		VECTOR4_SHUFFLE_3(xs, xn, ys, yn, x, 0) \
		VECTOR4_SHUFFLE_3(xs, xn, ys, yn, y, 1) \
		VECTOR4_SHUFFLE_3(xs, xn, ys, yn, z, 2) \
		VECTOR4_SHUFFLE_3(xs, xn, ys, yn, w, 3) \

	#define VECTOR4_SHUFFLE_1(xs, xn) \
		VECTOR4_SHUFFLE_2(xs, xn, x, 0) \
		VECTOR4_SHUFFLE_2(xs, xn, y, 1) \
		VECTOR4_SHUFFLE_2(xs, xn, z, 2) \
		VECTOR4_SHUFFLE_2(xs, xn, w, 3) \

	VECTOR4_SHUFFLE_1(x, 0)
	VECTOR4_SHUFFLE_1(y, 1)
	VECTOR4_SHUFFLE_1(z, 2)
	VECTOR4_SHUFFLE_1(w, 3)

	// clang-format on

	__forceinline GSVector4 broadcast32() const
	{
		return GSVector4(vdupq_laneq_f32(v4s, 0));
	}

	__forceinline static GSVector4 broadcast32(const GSVector4& v)
	{
		return GSVector4(vdupq_laneq_f32(v.v4s, 0));
	}

	__forceinline static GSVector4 broadcast32(const void* f)
	{
		return GSVector4(vld1q_dup_f32((const float*)f));
	}

	__forceinline static GSVector4 broadcast64(const void* f)
	{
		return GSVector4(vreinterpretq_f64_f32(vld1q_dup_f64((const double*)f)));
	}
};
