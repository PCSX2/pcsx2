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

#if _M_SSE >= 0x501

class alignas(32) GSVector8i
{
	static const GSVector8i m_xff[33];
	static const GSVector8i m_x0f[33];

	struct cxpr_init_tag {};
	static constexpr cxpr_init_tag cxpr_init{};

	constexpr GSVector8i(cxpr_init_tag, int x0, int y0, int z0, int w0, int x1, int y1, int z1, int w1)
		: I32{x0, y0, z0, w0, x1, y1, z1, w1}
	{
	}

public:
	union
	{
		struct { int x0, y0, z0, w0, x1, y1, z1, w1; };
		struct { int r0, g0, b0, a0, r1, g1, b1, a1; };
		int v[8];
		float F32[8];
		s8  I8[32];
		s16 I16[16];
		s32 I32[8];
		s64 I64[4];
		u8  U8[32];
		u16 U16[16];
		u32 U32[8];
		u64 U64[4];
		__m256i m;
		__m128i m0, m1;
	};

	GSVector8i() = default;

	static constexpr GSVector8i cxpr(int x0, int y0, int z0, int w0, int x1, int y1, int z1, int w1)
	{
		return GSVector8i(cxpr_init, x0, y0, z0, w0, x1, y1, z1, w1);
	}

	static constexpr GSVector8i cxpr(int x)
	{
		return GSVector8i(cxpr_init, x, x, x, x, x, x, x, x);
	}

	__forceinline explicit GSVector8i(const GSVector8& v, bool truncate = true);

	__forceinline static GSVector8i cast(const GSVector8& v);
	__forceinline static GSVector8i cast(const GSVector4& v);
	__forceinline static GSVector8i cast(const GSVector4i& v);

	__forceinline GSVector8i(int x0, int y0, int z0, int w0, int x1, int y1, int z1, int w1)
	{
		m = _mm256_set_epi32(x0, y0, z0, w0, x1, y1, z1, w1);
	}

	__forceinline GSVector8i(
		short s0, short s1, short s2, short s3, short s4, short s5, short s6, short s7,
		short s8, short s9, short s10, short s11, short s12, short s13, short s14, short s15)
	{
		m = _mm256_set_epi16(s15, s14, s13, s12, s11, s10, s9, s8, s7, s6, s5, s4, s3, s2, s1, s0);
	}

	constexpr GSVector8i(
		char b0, char b1, char b2, char b3, char b4, char b5, char b6, char b7,
		char b8, char b9, char b10, char b11, char b12, char b13, char b14, char b15,
		char b16, char b17, char b18, char b19, char b20, char b21, char b22, char b23,
		char b24, char b25, char b26, char b27, char b28, char b29, char b30, char b31)
		: I8{b0,  b1,  b2,  b3,  b4,  b5,  b6,  b7,  b8,  b9,  b10, b11, b12, b13, b14, b15,
		     b16, b17, b18, b19, b20, b21, b22, b23, b24, b25, b26, b27, b28, b29, b30, b31}
	{
	}

	__forceinline GSVector8i(__m128i m0, __m128i m1)
	{
#if 0 // _MSC_VER >= 1700 
		
		this->m = _mm256_permute2x128_si256(_mm256_castsi128_si256(m0), _mm256_castsi128_si256(m1), 0);

#else

		*this = zero().insert<0>(m0).insert<1>(m1);

#endif
	}

	GSVector8i(const GSVector8i& v) = default;

	__forceinline explicit GSVector8i(int i)
	{
		*this = i;
	}

	__forceinline explicit GSVector8i(__m128i m)
	{
		*this = m;
	}

	__forceinline constexpr explicit GSVector8i(__m256i m)
		: m(m)
	{
	}

	__forceinline void operator=(const GSVector8i& v)
	{
		m = v.m;
	}

	__forceinline void operator=(int i)
	{
		m = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(i)); // m = _mm256_set1_epi32(i);
	}

	__forceinline void operator=(__m128i m)
	{
		this->m = _mm256_inserti128_si256(_mm256_castsi128_si256(m), m, 1);
	}

	__forceinline void operator=(__m256i m)
	{
		this->m = m;
	}

	__forceinline operator __m256i() const
	{
		return m;
	}

	//

	__forceinline GSVector8i sat_i8(const GSVector8i& a, const GSVector8i& b) const
	{
		return max_i8(a).min_i8(b);
	}

	__forceinline GSVector8i sat_i8(const GSVector8i& a) const
	{
		return max_i8(a.xyxy()).min_i8(a.zwzw());
	}

	__forceinline GSVector8i sat_i16(const GSVector8i& a, const GSVector8i& b) const
	{
		return max_i16(a).min_i16(b);
	}

	__forceinline GSVector8i sat_i16(const GSVector8i& a) const
	{
		return max_i16(a.xyxy()).min_i16(a.zwzw());
	}

	__forceinline GSVector8i sat_i32(const GSVector8i& a, const GSVector8i& b) const
	{
		return max_i32(a).min_i32(b);
	}

	__forceinline GSVector8i sat_i32(const GSVector8i& a) const
	{
		return max_i32(a.xyxy()).min_i32(a.zwzw());
	}

	__forceinline GSVector8i sat_u8(const GSVector8i& a, const GSVector8i& b) const
	{
		return max_u8(a).min_u8(b);
	}

	__forceinline GSVector8i sat_u8(const GSVector8i& a) const
	{
		return max_u8(a.xyxy()).min_u8(a.zwzw());
	}

	__forceinline GSVector8i sat_u16(const GSVector8i& a, const GSVector8i& b) const
	{
		return max_u16(a).min_u16(b);
	}

	__forceinline GSVector8i sat_u16(const GSVector8i& a) const
	{
		return max_u16(a.xyxy()).min_u16(a.zwzw());
	}

	__forceinline GSVector8i sat_u32(const GSVector8i& a, const GSVector8i& b) const
	{
		return max_u32(a).min_u32(b);
	}

	__forceinline GSVector8i sat_u32(const GSVector8i& a) const
	{
		return max_u32(a.xyxy()).min_u32(a.zwzw());
	}

	__forceinline GSVector8i min_i8(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_min_epi8(m, a));
	}

	__forceinline GSVector8i max_i8(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_max_epi8(m, a));
	}

	__forceinline GSVector8i min_i16(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_min_epi16(m, a));
	}

	__forceinline GSVector8i max_i16(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_max_epi16(m, a));
	}

	__forceinline GSVector8i min_i32(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_min_epi32(m, a));
	}

	__forceinline GSVector8i max_i32(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_max_epi32(m, a));
	}

	__forceinline GSVector8i min_u8(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_min_epu8(m, a));
	}

	__forceinline GSVector8i max_u8(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_max_epu8(m, a));
	}

	__forceinline GSVector8i min_u16(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_min_epu16(m, a));
	}

	__forceinline GSVector8i max_u16(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_max_epu16(m, a));
	}

	__forceinline GSVector8i min_u32(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_min_epu32(m, a));
	}

	__forceinline GSVector8i max_u32(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_max_epu32(m, a));
	}

	__forceinline GSVector8i clamp8() const
	{
		return pu16().upl8();
	}

	__forceinline GSVector8i blend8(const GSVector8i& a, const GSVector8i& mask) const
	{
		return GSVector8i(_mm256_blendv_epi8(m, a, mask));
	}

	template <int mask>
	__forceinline GSVector8i blend16(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_blend_epi16(m, a, mask));
	}

	template <int mask>
	__forceinline GSVector8i blend32(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_blend_epi32(m, a, mask));
	}

	__forceinline GSVector8i blend(const GSVector8i& a, const GSVector8i& mask) const
	{
		return GSVector8i(_mm256_or_si256(_mm256_andnot_si256(mask, m), _mm256_and_si256(mask, a)));
	}

	/// Equivalent to blend with the given mask broadcasted across the vector
	/// May be faster than blend in some cases
	template <u32 mask>
	__forceinline GSVector8i smartblend(const GSVector8i& a) const
	{
		if (mask == 0)
			return *this;
		if (mask == 0xffffffff)
			return a;

		if (mask == 0x0000ffff)
			return blend16<0x55>(a);
		if (mask == 0xffff0000)
			return blend16<0xaa>(a);

		for (int i = 0; i < 32; i += 8)
		{
			u8 byte = (mask >> i) & 0xff;
			if (byte != 0xff && byte != 0)
				return blend(a, GSVector8i(mask));
		}

		return blend8(a, GSVector8i(mask));
	}

	__forceinline GSVector8i mix16(const GSVector8i& a) const
	{
		return blend16<0xaa>(a);
	}

	__forceinline GSVector8i shuffle8(const GSVector8i& mask) const
	{
		return GSVector8i(_mm256_shuffle_epi8(m, mask));
	}

	__forceinline GSVector8i ps16(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_packs_epi16(m, a));
	}

	__forceinline GSVector8i ps16() const
	{
		return GSVector8i(_mm256_packs_epi16(m, m));
	}

	__forceinline GSVector8i pu16(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_packus_epi16(m, a));
	}

	__forceinline GSVector8i pu16() const
	{
		return GSVector8i(_mm256_packus_epi16(m, m));
	}

	__forceinline GSVector8i ps32(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_packs_epi32(m, a));
	}

	__forceinline GSVector8i ps32() const
	{
		return GSVector8i(_mm256_packs_epi32(m, m));
	}

	__forceinline GSVector8i pu32(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_packus_epi32(m, a));
	}

	__forceinline GSVector8i pu32() const
	{
		return GSVector8i(_mm256_packus_epi32(m, m));
	}

	__forceinline GSVector8i upl8(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_unpacklo_epi8(m, a));
	}

	__forceinline GSVector8i uph8(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_unpackhi_epi8(m, a));
	}

	__forceinline GSVector8i upl16(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_unpacklo_epi16(m, a));
	}

	__forceinline GSVector8i uph16(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_unpackhi_epi16(m, a));
	}

	__forceinline GSVector8i upl32(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_unpacklo_epi32(m, a));
	}

	__forceinline GSVector8i uph32(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_unpackhi_epi32(m, a));
	}

	__forceinline GSVector8i upl64(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_unpacklo_epi64(m, a));
	}

	__forceinline GSVector8i uph64(const GSVector8i& a) const
	{
		return GSVector8i(_mm256_unpackhi_epi64(m, a));
	}

	__forceinline GSVector8i upl8() const
	{
		return GSVector8i(_mm256_unpacklo_epi8(m, _mm256_setzero_si256()));
	}

	__forceinline GSVector8i uph8() const
	{
		return GSVector8i(_mm256_unpackhi_epi8(m, _mm256_setzero_si256()));
	}

	__forceinline GSVector8i upl16() const
	{
		return GSVector8i(_mm256_unpacklo_epi16(m, _mm256_setzero_si256()));
	}

	__forceinline GSVector8i uph16() const
	{
		return GSVector8i(_mm256_unpackhi_epi16(m, _mm256_setzero_si256()));
	}

	__forceinline GSVector8i upl32() const
	{
		return GSVector8i(_mm256_unpacklo_epi32(m, _mm256_setzero_si256()));
	}

	__forceinline GSVector8i uph32() const
	{
		return GSVector8i(_mm256_unpackhi_epi32(m, _mm256_setzero_si256()));
	}

	__forceinline GSVector8i upl64() const
	{
		return GSVector8i(_mm256_unpacklo_epi64(m, _mm256_setzero_si256()));
	}

	__forceinline GSVector8i uph64() const
	{
		return GSVector8i(_mm256_unpackhi_epi64(m, _mm256_setzero_si256()));
	}

	// cross lane! from 128-bit to full 256-bit range

	static __forceinline GSVector8i i8to16(const GSVector4i& v)
	{
		return GSVector8i(_mm256_cvtepi8_epi16(v.m));
	}

	static __forceinline GSVector8i u8to16(const GSVector4i& v)
	{
		return GSVector8i(_mm256_cvtepu8_epi16(v.m));
	}

	static __forceinline GSVector8i i8to32(const GSVector4i& v)
	{
		return GSVector8i(_mm256_cvtepi8_epi32(v.m));
	}

	static __forceinline GSVector8i u8to32(const GSVector4i& v)
	{
		return GSVector8i(_mm256_cvtepu8_epi32(v.m));
	}

	static __forceinline GSVector8i i8to64(const GSVector4i& v)
	{
		return GSVector8i(_mm256_cvtepi8_epi64(v.m));
	}

	static __forceinline GSVector8i u8to64(const GSVector4i& v)
	{
		return GSVector8i(_mm256_cvtepu16_epi64(v.m));
	}

	static __forceinline GSVector8i i16to32(const GSVector4i& v)
	{
		return GSVector8i(_mm256_cvtepi16_epi32(v.m));
	}

	static __forceinline GSVector8i u16to32(const GSVector4i& v)
	{
		return GSVector8i(_mm256_cvtepu16_epi32(v.m));
	}

	static __forceinline GSVector8i i16to64(const GSVector4i& v)
	{
		return GSVector8i(_mm256_cvtepi16_epi64(v.m));
	}

	static __forceinline GSVector8i u16to64(const GSVector4i& v)
	{
		return GSVector8i(_mm256_cvtepu16_epi64(v.m));
	}

	static __forceinline GSVector8i i32to64(const GSVector4i& v)
	{
		return GSVector8i(_mm256_cvtepi32_epi64(v.m));
	}

	static __forceinline GSVector8i u32to64(const GSVector4i& v)
	{
		return GSVector8i(_mm256_cvtepu32_epi64(v.m));
	}

	//

	static __forceinline GSVector8i i8to16(const void* p)
	{
		return GSVector8i(_mm256_cvtepi8_epi16(_mm_load_si128((__m128i*)p)));
	}

	static __forceinline GSVector8i u8to16(const void* p)
	{
		return GSVector8i(_mm256_cvtepu8_epi16(_mm_load_si128((__m128i*)p)));
	}

	static __forceinline GSVector8i i8to32(const void* p)
	{
		return GSVector8i(_mm256_cvtepi8_epi32(_mm_loadl_epi64((__m128i*)p)));
	}

	static __forceinline GSVector8i u8to32(const void* p)
	{
		return GSVector8i(_mm256_cvtepu8_epi32(_mm_loadl_epi64((__m128i*)p)));
	}

	static __forceinline GSVector8i i8to64(int i)
	{
		return GSVector8i(_mm256_cvtepi8_epi64(_mm_cvtsi32_si128(i)));
	}

	static __forceinline GSVector8i u8to64(int i)
	{
		return GSVector8i(_mm256_cvtepu8_epi64(_mm_cvtsi32_si128(i)));
	}

	static __forceinline GSVector8i i16to32(const void* p)
	{
		return GSVector8i(_mm256_cvtepi16_epi32(_mm_load_si128((__m128i*)p)));
	}

	static __forceinline GSVector8i u16to32(const void* p)
	{
		return GSVector8i(_mm256_cvtepu16_epi32(_mm_load_si128((__m128i*)p)));
	}

	static __forceinline GSVector8i i16to64(const void* p)
	{
		return GSVector8i(_mm256_cvtepi16_epi64(_mm_loadl_epi64((__m128i*)p)));
	}

	static __forceinline GSVector8i u16to64(const void* p)
	{
		return GSVector8i(_mm256_cvtepu16_epi64(_mm_loadl_epi64((__m128i*)p)));
	}

	static __forceinline GSVector8i i32to64(const void* p)
	{
		return GSVector8i(_mm256_cvtepi32_epi64(_mm_load_si128((__m128i*)p)));
	}

	static __forceinline GSVector8i u32to64(const void* p)
	{
		return GSVector8i(_mm256_cvtepu32_epi64(_mm_load_si128((__m128i*)p)));
	}

	//

	template <int i>
	__forceinline GSVector8i srl() const
	{
		return GSVector8i(_mm256_srli_si256(m, i));
	}

	template <int i>
	__forceinline GSVector8i srl(const GSVector8i& v)
	{
		return GSVector8i(_mm256_alignr_epi8(v.m, m, i));
	}

	template <int i>
	__forceinline GSVector8i sll() const
	{
		return GSVector8i(_mm256_slli_si256(m, i));
		//return GSVector8i(_mm256_slli_si128(m, i));
	}

	__forceinline GSVector8i sra16(int i) const
	{
		return GSVector8i(_mm256_srai_epi16(m, i));
	}

	__forceinline GSVector8i sra16(__m128i i) const
	{
		return GSVector8i(_mm256_sra_epi16(m, i));
	}

	__forceinline GSVector8i sra16(__m256i i) const
	{
		return GSVector8i(_mm256_sra_epi16(m, _mm256_castsi256_si128(i)));
	}

	__forceinline GSVector8i sra32(int i) const
	{
		return GSVector8i(_mm256_srai_epi32(m, i));
	}

	__forceinline GSVector8i sra32(__m128i i) const
	{
		return GSVector8i(_mm256_sra_epi32(m, i));
	}

	__forceinline GSVector8i sra32(__m256i i) const
	{
		return GSVector8i(_mm256_sra_epi32(m, _mm256_castsi256_si128(i)));
	}

	__forceinline GSVector8i srav32(__m256i i) const
	{
		return GSVector8i(_mm256_srav_epi32(m, i));
	}

	__forceinline GSVector8i sll16(int i) const
	{
		return GSVector8i(_mm256_slli_epi16(m, i));
	}

	__forceinline GSVector8i sll16(__m128i i) const
	{
		return GSVector8i(_mm256_sll_epi16(m, i));
	}

	__forceinline GSVector8i sll16(__m256i i) const
	{
		return GSVector8i(_mm256_sll_epi16(m, _mm256_castsi256_si128(i)));
	}

	__forceinline GSVector8i sll32(int i) const
	{
		return GSVector8i(_mm256_slli_epi32(m, i));
	}

	__forceinline GSVector8i sll32(__m128i i) const
	{
		return GSVector8i(_mm256_sll_epi32(m, i));
	}

	__forceinline GSVector8i sll32(__m256i i) const
	{
		return GSVector8i(_mm256_sll_epi32(m, _mm256_castsi256_si128(i)));
	}

	__forceinline GSVector8i sllv32(__m256i i) const
	{
		return GSVector8i(_mm256_sllv_epi32(m, i));
	}

	__forceinline GSVector8i sll64(int i) const
	{
		return GSVector8i(_mm256_slli_epi64(m, i));
	}

	__forceinline GSVector8i sll64(__m128i i) const
	{
		return GSVector8i(_mm256_sll_epi64(m, i));
	}

	__forceinline GSVector8i sll64(__m256i i) const
	{
		return GSVector8i(_mm256_sll_epi64(m, _mm256_castsi256_si128(i)));
	}

	__forceinline GSVector8i sllv64(__m256i i) const
	{
		return GSVector8i(_mm256_sllv_epi64(m, i));
	}

	__forceinline GSVector8i srl16(int i) const
	{
		return GSVector8i(_mm256_srli_epi16(m, i));
	}

	__forceinline GSVector8i srl16(__m128i i) const
	{
		return GSVector8i(_mm256_srl_epi16(m, i));
	}

	__forceinline GSVector8i srl16(__m256i i) const
	{
		return GSVector8i(_mm256_srl_epi16(m, _mm256_castsi256_si128(i)));
	}

	__forceinline GSVector8i srl32(int i) const
	{
		return GSVector8i(_mm256_srli_epi32(m, i));
	}

	__forceinline GSVector8i srl32(__m128i i) const
	{
		return GSVector8i(_mm256_srl_epi32(m, i));
	}

	__forceinline GSVector8i srl32(__m256i i) const
	{
		return GSVector8i(_mm256_srl_epi32(m, _mm256_castsi256_si128(i)));
	}

	__forceinline GSVector8i srlv32(__m256i i) const
	{
		return GSVector8i(_mm256_srlv_epi32(m, i));
	}

	__forceinline GSVector8i srl64(int i) const
	{
		return GSVector8i(_mm256_srli_epi64(m, i));
	}

	__forceinline GSVector8i srl64(__m128i i) const
	{
		return GSVector8i(_mm256_srl_epi64(m, i));
	}

	__forceinline GSVector8i srl64(__m256i i) const
	{
		return GSVector8i(_mm256_srl_epi64(m, _mm256_castsi256_si128(i)));
	}

	__forceinline GSVector8i srlv64(__m256i i) const
	{
		return GSVector8i(_mm256_srlv_epi64(m, i));
	}

	__forceinline GSVector8i add8(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_add_epi8(m, v.m));
	}

	__forceinline GSVector8i add16(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_add_epi16(m, v.m));
	}

	__forceinline GSVector8i add32(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_add_epi32(m, v.m));
	}

	__forceinline GSVector8i adds8(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_adds_epi8(m, v.m));
	}

	__forceinline GSVector8i adds16(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_adds_epi16(m, v.m));
	}

	__forceinline GSVector8i addus8(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_adds_epu8(m, v.m));
	}

	__forceinline GSVector8i addus16(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_adds_epu16(m, v.m));
	}

	__forceinline GSVector8i sub8(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_sub_epi8(m, v.m));
	}

	__forceinline GSVector8i sub16(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_sub_epi16(m, v.m));
	}

	__forceinline GSVector8i sub32(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_sub_epi32(m, v.m));
	}

	__forceinline GSVector8i subs8(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_subs_epi8(m, v.m));
	}

	__forceinline GSVector8i subs16(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_subs_epi16(m, v.m));
	}

	__forceinline GSVector8i subus8(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_subs_epu8(m, v.m));
	}

	__forceinline GSVector8i subus16(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_subs_epu16(m, v.m));
	}

	__forceinline GSVector8i avg8(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_avg_epu8(m, v.m));
	}

	__forceinline GSVector8i avg16(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_avg_epu16(m, v.m));
	}

	__forceinline GSVector8i mul16hs(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_mulhi_epi16(m, v.m));
	}

	__forceinline GSVector8i mul16hu(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_mulhi_epu16(m, v.m));
	}

	__forceinline GSVector8i mul16l(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_mullo_epi16(m, v.m));
	}

	__forceinline GSVector8i mul16hrs(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_mulhrs_epi16(m, v.m));
	}

	GSVector8i madd(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_madd_epi16(m, v.m));
	}

	template <int shift>
	__forceinline GSVector8i lerp16(const GSVector8i& a, const GSVector8i& f) const
	{
		// (a - this) * f << shift + this

		return add16(a.sub16(*this).modulate16<shift>(f));
	}

	template <int shift>
	__forceinline static GSVector8i lerp16(const GSVector8i& a, const GSVector8i& b, const GSVector8i& c)
	{
		// (a - b) * c << shift

		return a.sub16(b).modulate16<shift>(c);
	}

	template <int shift>
	__forceinline static GSVector8i lerp16(const GSVector8i& a, const GSVector8i& b, const GSVector8i& c, const GSVector8i& d)
	{
		// (a - b) * c << shift + d

		return d.add16(a.sub16(b).modulate16<shift>(c));
	}

	__forceinline GSVector8i lerp16_4(const GSVector8i& a, const GSVector8i& f) const
	{
		// (a - this) * f >> 4 + this (a, this: 8-bit, f: 4-bit)

		return add16(a.sub16(*this).mul16l(f).sra16(4));
	}

	template <int shift>
	__forceinline GSVector8i modulate16(const GSVector8i& f) const
	{
		// a * f << shift

		if (shift == 0)
		{
			return mul16hrs(f);
		}

		return sll16(shift + 1).mul16hs(f);
	}

	__forceinline bool eq(const GSVector8i& v) const
	{
		GSVector8i t = *this ^ v;

		return _mm256_testz_si256(t, t) != 0;
	}

	__forceinline GSVector8i eq8(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_cmpeq_epi8(m, v.m));
	}

	__forceinline GSVector8i eq16(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_cmpeq_epi16(m, v.m));
	}

	__forceinline GSVector8i eq32(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_cmpeq_epi32(m, v.m));
	}

	__forceinline GSVector8i neq8(const GSVector8i& v) const
	{
		return ~eq8(v);
	}

	__forceinline GSVector8i neq16(const GSVector8i& v) const
	{
		return ~eq16(v);
	}

	__forceinline GSVector8i neq32(const GSVector8i& v) const
	{
		return ~eq32(v);
	}

	__forceinline GSVector8i gt8(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_cmpgt_epi8(m, v.m));
	}

	__forceinline GSVector8i gt16(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_cmpgt_epi16(m, v.m));
	}

	__forceinline GSVector8i gt32(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_cmpgt_epi32(m, v.m));
	}

	__forceinline GSVector8i lt8(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_cmpgt_epi8(v.m, m));
	}

	__forceinline GSVector8i lt16(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_cmpgt_epi16(v.m, m));
	}

	__forceinline GSVector8i lt32(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_cmpgt_epi32(v.m, m));
	}

	__forceinline GSVector8i andnot(const GSVector8i& v) const
	{
		return GSVector8i(_mm256_andnot_si256(v.m, m));
	}

	__forceinline int mask() const
	{
		return _mm256_movemask_epi8(m);
	}

	__forceinline bool alltrue() const
	{
		return mask() == (int)0xffffffff;
	}

	__forceinline bool allfalse() const
	{
		return _mm256_testz_si256(m, m) != 0;
	}

	// TODO: extract/insert

	template <int i>
	__forceinline int extract8() const
	{
		ASSERT(i < 32);

		GSVector4i v = extract<i / 16>();

		return v.extract8<i & 15>();
	}

	template <int i>
	__forceinline int extract16() const
	{
		ASSERT(i < 16);

		GSVector4i v = extract<i / 8>();

		return v.extract16<i & 8>();
	}

	template <int i>
	__forceinline int extract32() const
	{
		ASSERT(i < 8);

		GSVector4i v = extract<i / 4>();

		if ((i & 3) == 0)
			return GSVector4i::store(v);

		return v.extract32<i & 3>();
	}

	template <int i>
	__forceinline GSVector4i extract() const
	{
		ASSERT(i < 2);

		if (i == 0)
			return GSVector4i(_mm256_castsi256_si128(m));

		return GSVector4i(_mm256_extracti128_si256(m, i));
	}

	template <int i>
	__forceinline GSVector8i insert(__m128i m) const
	{
		ASSERT(i < 2);

		return GSVector8i(_mm256_inserti128_si256(this->m, m, i));
	}

	// TODO: gather

	template <class T>
	__forceinline GSVector8i gather32_32(const T* ptr) const
	{
		GSVector4i v0;
		GSVector4i v1;

		GSVector4i a0 = extract<0>();
		GSVector4i a1 = extract<1>();

		v0 = GSVector4i::load((int)ptr[a0.extract32<0>()]);
		v0 = v0.insert32<1>((int)ptr[a0.extract32<1>()]);
		v0 = v0.insert32<2>((int)ptr[a0.extract32<2>()]);
		v0 = v0.insert32<3>((int)ptr[a0.extract32<3>()]);

		v1 = GSVector4i::load((int)ptr[a1.extract32<0>()]);
		v1 = v1.insert32<1>((int)ptr[a1.extract32<1>()]);
		v1 = v1.insert32<2>((int)ptr[a1.extract32<2>()]);
		v1 = v1.insert32<3>((int)ptr[a1.extract32<3>()]);

		return cast(v0).insert<1>(v1);
	}

	__forceinline GSVector8i gather32_32(const u8* ptr) const
	{
		return GSVector8i(_mm256_i32gather_epi32((const int*)ptr, m, 1)) & GSVector8i::x000000ff();
	}

	__forceinline GSVector8i gather32_32(const u16* ptr) const
	{
		return GSVector8i(_mm256_i32gather_epi32((const int*)ptr, m, 2)) & GSVector8i::x0000ffff();
	}

	__forceinline GSVector8i gather32_32(const u32* ptr) const
	{
		return GSVector8i(_mm256_i32gather_epi32((const int*)ptr, m, 4));
	}

	template <class T1, class T2>
	__forceinline GSVector8i gather32_32(const T1* ptr1, const T2* ptr2) const
	{
		GSVector4i v0;
		GSVector4i v1;

		GSVector4i a0 = extract<0>();
		GSVector4i a1 = extract<1>();

		v0 = GSVector4i::load((int)ptr2[ptr1[a0.extract32<0>()]]);
		v0 = v0.insert32<1>((int)ptr2[ptr1[a0.extract32<1>()]]);
		v0 = v0.insert32<2>((int)ptr2[ptr1[a0.extract32<2>()]]);
		v0 = v0.insert32<3>((int)ptr2[ptr1[a0.extract32<3>()]]);

		v1 = GSVector4i::load((int)ptr2[ptr1[a1.extract32<0>()]]);
		v1 = v1.insert32<1>((int)ptr2[ptr1[a1.extract32<1>()]]);
		v1 = v1.insert32<2>((int)ptr2[ptr1[a1.extract32<2>()]]);
		v1 = v1.insert32<3>((int)ptr2[ptr1[a1.extract32<3>()]]);

		return cast(v0).insert<1>(v1);
	}

	__forceinline GSVector8i gather32_32(const u8* ptr1, const u32* ptr2) const
	{
		return gather32_32<u8>(ptr1).gather32_32<u32>(ptr2);
	}

	__forceinline GSVector8i gather32_32(const u32* ptr1, const u32* ptr2) const
	{
		return gather32_32<u32>(ptr1).gather32_32<u32>(ptr2);
	}

	template <class T>
	__forceinline void gather32_32(const T* RESTRICT ptr, GSVector8i* RESTRICT dst) const
	{
		dst[0] = gather32_32<>(ptr);
	}

	//

	__forceinline static GSVector8i loadnt(const void* p)
	{
		return GSVector8i(_mm256_stream_load_si256((__m256i*)p));
	}

	__forceinline static GSVector8i loadl(const void* p)
	{
		return GSVector8i(_mm256_castsi128_si256(_mm_load_si128((__m128i*)p)));
	}

	__forceinline static GSVector8i loadh(const void* p)
	{
		return GSVector8i(_mm256_inserti128_si256(_mm256_setzero_si256(), _mm_load_si128((__m128i*)p), 1));

		/* TODO: this may be faster
		__m256i m = _mm256_castsi128_si256(_mm_load_si128((__m128i*)p));
		return GSVector8i(_mm256_permute2x128_si256(m, m, 0x08));
		*/
	}

	__forceinline static GSVector8i loadh(const void* p, const GSVector8i& v)
	{
		return GSVector8i(_mm256_inserti128_si256(v, _mm_load_si128((__m128i*)p), 1));
	}

	__forceinline static GSVector8i load(const void* pl, const void* ph)
	{
		return loadh(ph, loadl(pl));

		/* TODO: this may be faster
		__m256 m0 = _mm256_castsi128_si256(_mm_load_si128((__m128*)pl));
		__m256 m1 = _mm256_castsi128_si256(_mm_load_si128((__m128*)ph));
		return GSVector8i(_mm256_permute2x128_si256(m0, m1, 0x20));
		*/
	}

	__forceinline static GSVector8i load(const void* pll, const void* plh, const void* phl, const void* phh)
	{
		GSVector4i l = GSVector4i::load(pll, plh);
		GSVector4i h = GSVector4i::load(phl, phh);

		return cast(l).ac(cast(h));

		// return GSVector8i(l).insert<1>(h);
	}

	template <bool aligned>
	__forceinline static GSVector8i load(const void* p)
	{
		return GSVector8i(aligned ? _mm256_load_si256((__m256i*)p) : _mm256_loadu_si256((__m256i*)p));
	}

	__forceinline static GSVector8i load(int i)
	{
		return cast(GSVector4i::load(i));
	}

#ifdef _M_AMD64

	__forceinline static GSVector8i loadq(s64 i)
	{
		return cast(GSVector4i::loadq(i));
	}

#endif

	__forceinline static void storent(void* p, const GSVector8i& v)
	{
		_mm256_stream_si256((__m256i*)p, v.m);
	}

	__forceinline static void storel(void* p, const GSVector8i& v)
	{
		_mm_store_si128((__m128i*)p, _mm256_extracti128_si256(v.m, 0));
	}

	__forceinline static void storeh(void* p, const GSVector8i& v)
	{
		_mm_store_si128((__m128i*)p, _mm256_extracti128_si256(v.m, 1));
	}

	__forceinline static void store(void* pl, void* ph, const GSVector8i& v)
	{
		GSVector8i::storel(pl, v);
		GSVector8i::storeh(ph, v);
	}

	template <bool aligned>
	__forceinline static void store(void* p, const GSVector8i& v)
	{
		if (aligned)
			_mm256_store_si256((__m256i*)p, v.m);
		else
			_mm256_storeu_si256((__m256i*)p, v.m);
	}

	__forceinline static int store(const GSVector8i& v)
	{
		return GSVector4i::store(GSVector4i::cast(v));
	}

#ifdef _M_AMD64

	__forceinline static s64 storeq(const GSVector8i& v)
	{
		return GSVector4i::storeq(GSVector4i::cast(v));
	}

#endif

	__forceinline static void storent(void* RESTRICT dst, const void* RESTRICT src, size_t size)
	{
		const GSVector8i* s = (const GSVector8i*)src;
		GSVector8i* d = (GSVector8i*)dst;

		if (size == 0)
			return;

		size_t i = 0;
		size_t j = size >> 7;

		for (; i < j; i++, s += 4, d += 4)
		{
			storent(&d[0], s[0]);
			storent(&d[1], s[1]);
			storent(&d[2], s[2]);
			storent(&d[3], s[3]);
		}

		size &= 127;

		if (size == 0)
			return;

		memcpy(d, s, size);
	}

	// TODO: swizzling

	__forceinline static void mix4(GSVector8i& a, GSVector8i& b)
	{
		GSVector8i mask(_mm256_set1_epi32(0x0f0f0f0f));

		GSVector8i c = (b << 4).blend(a, mask);
		GSVector8i d = b.blend(a >> 4, mask);
		a = c;
		b = d;
	}

	__forceinline static void sw8(GSVector8i& a, GSVector8i& b)
	{
		GSVector8i c = a;
		GSVector8i d = b;

		a = c.upl8(d);
		b = c.uph8(d);
	}

	__forceinline static void sw16(GSVector8i& a, GSVector8i& b)
	{
		GSVector8i c = a;
		GSVector8i d = b;

		a = c.upl16(d);
		b = c.uph16(d);
	}

	__forceinline static void sw32(GSVector8i& a, GSVector8i& b)
	{
		GSVector8i c = a;
		GSVector8i d = b;

		a = c.upl32(d);
		b = c.uph32(d);
	}

	__forceinline static void sw32_inv(GSVector8i& a, GSVector8i& b);

	__forceinline static void sw64(GSVector8i& a, GSVector8i& b)
	{
		GSVector8i c = a;
		GSVector8i d = b;

		a = c.upl64(d);
		b = c.uph64(d);
	}

	__forceinline static void sw128(GSVector8i& a, GSVector8i& b)
	{
		GSVector8i c = a;
		GSVector8i d = b;

		a = c.insert<1>(d.extract<0>()); // Should become a single vinserti128, faster on Zen+
		b = c.bd(d);
	}

	__forceinline static void sw4(GSVector8i& a, GSVector8i& b, GSVector8i& c, GSVector8i& d)
	{
		const __m256i epi32_0f0f0f0f = _mm256_set1_epi32(0x0f0f0f0f);

		GSVector8i mask(epi32_0f0f0f0f);

		GSVector8i e = (b << 4).blend(a, mask);
		GSVector8i f = b.blend(a >> 4, mask);
		GSVector8i g = (d << 4).blend(c, mask);
		GSVector8i h = d.blend(c >> 4, mask);

		a = e.upl8(f);
		c = e.uph8(f);
		b = g.upl8(h);
		d = g.uph8(h);
	}

	__forceinline static void sw8(GSVector8i& a, GSVector8i& b, GSVector8i& c, GSVector8i& d)
	{
		GSVector8i e = a;
		GSVector8i f = c;

		a = e.upl8(b);
		c = e.uph8(b);
		b = f.upl8(d);
		d = f.uph8(d);
	}

	__forceinline static void sw16(GSVector8i& a, GSVector8i& b, GSVector8i& c, GSVector8i& d)
	{
		GSVector8i e = a;
		GSVector8i f = c;

		a = e.upl16(b);
		c = e.uph16(b);
		b = f.upl16(d);
		d = f.uph16(d);
	}

	__forceinline static void sw32(GSVector8i& a, GSVector8i& b, GSVector8i& c, GSVector8i& d)
	{
		GSVector8i e = a;
		GSVector8i f = c;

		a = e.upl32(b);
		c = e.uph32(b);
		b = f.upl32(d);
		d = f.uph32(d);
	}

	__forceinline static void sw64(GSVector8i& a, GSVector8i& b, GSVector8i& c, GSVector8i& d)
	{
		GSVector8i e = a;
		GSVector8i f = c;

		a = e.upl64(b);
		c = e.uph64(b);
		b = f.upl64(d);
		d = f.uph64(d);
	}

	__forceinline static void sw128(GSVector8i& a, GSVector8i& b, GSVector8i& c, GSVector8i& d)
	{
		GSVector8i e = a;
		GSVector8i f = c;

		a = e.ac(b);
		c = e.bd(b);
		b = f.ac(d);
		d = f.bd(d);
	}

	__forceinline void operator+=(const GSVector8i& v)
	{
		m = _mm256_add_epi32(m, v);
	}

	__forceinline void operator-=(const GSVector8i& v)
	{
		m = _mm256_sub_epi32(m, v);
	}

	__forceinline void operator+=(int i)
	{
		*this += GSVector8i(i);
	}

	__forceinline void operator-=(int i)
	{
		*this -= GSVector8i(i);
	}

	__forceinline void operator<<=(const int i)
	{
		m = _mm256_slli_epi32(m, i);
	}

	__forceinline void operator>>=(const int i)
	{
		m = _mm256_srli_epi32(m, i);
	}

	__forceinline void operator&=(const GSVector8i& v)
	{
		m = _mm256_and_si256(m, v);
	}

	__forceinline void operator|=(const GSVector8i& v)
	{
		m = _mm256_or_si256(m, v);
	}

	__forceinline void operator^=(const GSVector8i& v)
	{
		m = _mm256_xor_si256(m, v);
	}

	__forceinline friend GSVector8i operator+(const GSVector8i& v1, const GSVector8i& v2)
	{
		return GSVector8i(_mm256_add_epi32(v1, v2));
	}

	__forceinline friend GSVector8i operator-(const GSVector8i& v1, const GSVector8i& v2)
	{
		return GSVector8i(_mm256_sub_epi32(v1, v2));
	}

	__forceinline friend GSVector8i operator+(const GSVector8i& v, int i)
	{
		return v + GSVector8i(i);
	}

	__forceinline friend GSVector8i operator-(const GSVector8i& v, int i)
	{
		return v - GSVector8i(i);
	}

	__forceinline friend GSVector8i operator<<(const GSVector8i& v, const int i)
	{
		return GSVector8i(_mm256_slli_epi32(v, i));
	}

	__forceinline friend GSVector8i operator>>(const GSVector8i& v, const int i)
	{
		return GSVector8i(_mm256_srli_epi32(v, i));
	}

	__forceinline friend GSVector8i operator&(const GSVector8i& v1, const GSVector8i& v2)
	{
		return GSVector8i(_mm256_and_si256(v1, v2));
	}

	__forceinline friend GSVector8i operator|(const GSVector8i& v1, const GSVector8i& v2)
	{
		return GSVector8i(_mm256_or_si256(v1, v2));
	}

	__forceinline friend GSVector8i operator^(const GSVector8i& v1, const GSVector8i& v2)
	{
		return GSVector8i(_mm256_xor_si256(v1, v2));
	}

	__forceinline friend GSVector8i operator&(const GSVector8i& v, int i)
	{
		return v & GSVector8i(i);
	}

	__forceinline friend GSVector8i operator|(const GSVector8i& v, int i)
	{
		return v | GSVector8i(i);
	}

	__forceinline friend GSVector8i operator^(const GSVector8i& v, int i)
	{
		return v ^ GSVector8i(i);
	}

	__forceinline friend GSVector8i operator~(const GSVector8i& v)
	{
		return v ^ (v == v);
	}

	__forceinline friend GSVector8i operator==(const GSVector8i& v1, const GSVector8i& v2)
	{
		return GSVector8i(_mm256_cmpeq_epi32(v1, v2));
	}

	__forceinline friend GSVector8i operator!=(const GSVector8i& v1, const GSVector8i& v2)
	{
		return ~(v1 == v2);
	}

	__forceinline friend GSVector8i operator>(const GSVector8i& v1, const GSVector8i& v2)
	{
		return GSVector8i(_mm256_cmpgt_epi32(v1, v2));
	}

	__forceinline friend GSVector8i operator<(const GSVector8i& v1, const GSVector8i& v2)
	{
		return GSVector8i(_mm256_cmpgt_epi32(v2, v1));
	}

	__forceinline friend GSVector8i operator>=(const GSVector8i& v1, const GSVector8i& v2)
	{
		return (v1 > v2) | (v1 == v2);
	}

	__forceinline friend GSVector8i operator<=(const GSVector8i& v1, const GSVector8i& v2)
	{
		return (v1 < v2) | (v1 == v2);
	}

	// clang-format off

	// x = v[31:0] / v[159:128]
	// y = v[63:32] / v[191:160]
	// z = v[95:64] / v[223:192]
	// w = v[127:96] / v[255:224]

	#define VECTOR8i_SHUFFLE_4(xs, xn, ys, yn, zs, zn, ws, wn) \
		__forceinline GSVector8i xs##ys##zs##ws() const { return GSVector8i(_mm256_shuffle_epi32(m, _MM_SHUFFLE(wn, zn, yn, xn))); } \
		__forceinline GSVector8i xs##ys##zs##ws##l() const { return GSVector8i(_mm256_shufflelo_epi16(m, _MM_SHUFFLE(wn, zn, yn, xn))); } \
		__forceinline GSVector8i xs##ys##zs##ws##h() const { return GSVector8i(_mm256_shufflehi_epi16(m, _MM_SHUFFLE(wn, zn, yn, xn))); } \
		__forceinline GSVector8i xs##ys##zs##ws##lh() const { return GSVector8i(_mm256_shufflehi_epi16(_mm256_shufflelo_epi16(m, _MM_SHUFFLE(wn, zn, yn, xn)), _MM_SHUFFLE(wn, zn, yn, xn))); } \

	#define VECTOR8i_SHUFFLE_3(xs, xn, ys, yn, zs, zn) \
		VECTOR8i_SHUFFLE_4(xs, xn, ys, yn, zs, zn, x, 0) \
		VECTOR8i_SHUFFLE_4(xs, xn, ys, yn, zs, zn, y, 1) \
		VECTOR8i_SHUFFLE_4(xs, xn, ys, yn, zs, zn, z, 2) \
		VECTOR8i_SHUFFLE_4(xs, xn, ys, yn, zs, zn, w, 3) \

	#define VECTOR8i_SHUFFLE_2(xs, xn, ys, yn) \
		VECTOR8i_SHUFFLE_3(xs, xn, ys, yn, x, 0) \
		VECTOR8i_SHUFFLE_3(xs, xn, ys, yn, y, 1) \
		VECTOR8i_SHUFFLE_3(xs, xn, ys, yn, z, 2) \
		VECTOR8i_SHUFFLE_3(xs, xn, ys, yn, w, 3) \

	#define VECTOR8i_SHUFFLE_1(xs, xn) \
		VECTOR8i_SHUFFLE_2(xs, xn, x, 0) \
		VECTOR8i_SHUFFLE_2(xs, xn, y, 1) \
		VECTOR8i_SHUFFLE_2(xs, xn, z, 2) \
		VECTOR8i_SHUFFLE_2(xs, xn, w, 3) \

	VECTOR8i_SHUFFLE_1(x, 0)
	VECTOR8i_SHUFFLE_1(y, 1)
	VECTOR8i_SHUFFLE_1(z, 2)
	VECTOR8i_SHUFFLE_1(w, 3)

	// a = v0[127:0]
	// b = v0[255:128]
	// c = v1[127:0]
	// d = v1[255:128]
	// _ = 0

	#define VECTOR8i_PERMUTE128_2(as, an, bs, bn) \
		__forceinline GSVector8i as##bs() const { return GSVector8i(_mm256_permute2x128_si256(m, m, an | (bn << 4))); } \
		__forceinline GSVector8i as##bs(const GSVector8i& v) const { return GSVector8i(_mm256_permute2x128_si256(m, v.m, an | (bn << 4))); } \

	#define VECTOR8i_PERMUTE128_1(as, an) \
		VECTOR8i_PERMUTE128_2(as, an, a, 0) \
		VECTOR8i_PERMUTE128_2(as, an, b, 1) \
		VECTOR8i_PERMUTE128_2(as, an, c, 2) \
		VECTOR8i_PERMUTE128_2(as, an, d, 3) \
		VECTOR8i_PERMUTE128_2(as, an, _, 8) \

	VECTOR8i_PERMUTE128_1(a, 0)
	VECTOR8i_PERMUTE128_1(b, 1)
	VECTOR8i_PERMUTE128_1(c, 2)
	VECTOR8i_PERMUTE128_1(d, 3)
	VECTOR8i_PERMUTE128_1(_, 8)

	// a = v[63:0]
	// b = v[127:64]
	// c = v[191:128]
	// d = v[255:192]

	#define VECTOR8i_PERMUTE64_4(as, an, bs, bn, cs, cn, ds, dn) \
		__forceinline GSVector8i as##bs##cs##ds() const { return GSVector8i(_mm256_permute4x64_epi64(m, _MM_SHUFFLE(dn, cn, bn, an))); } \

	#define VECTOR8i_PERMUTE64_3(as, an, bs, bn, cs, cn) \
		VECTOR8i_PERMUTE64_4(as, an, bs, bn, cs, cn, a, 0) \
		VECTOR8i_PERMUTE64_4(as, an, bs, bn, cs, cn, b, 1) \
		VECTOR8i_PERMUTE64_4(as, an, bs, bn, cs, cn, c, 2) \
		VECTOR8i_PERMUTE64_4(as, an, bs, bn, cs, cn, d, 3) \

	#define VECTOR8i_PERMUTE64_2(as, an, bs, bn) \
		VECTOR8i_PERMUTE64_3(as, an, bs, bn, a, 0) \
		VECTOR8i_PERMUTE64_3(as, an, bs, bn, b, 1) \
		VECTOR8i_PERMUTE64_3(as, an, bs, bn, c, 2) \
		VECTOR8i_PERMUTE64_3(as, an, bs, bn, d, 3) \

	#define VECTOR8i_PERMUTE64_1(as, an) \
		VECTOR8i_PERMUTE64_2(as, an, a, 0) \
		VECTOR8i_PERMUTE64_2(as, an, b, 1) \
		VECTOR8i_PERMUTE64_2(as, an, c, 2) \
		VECTOR8i_PERMUTE64_2(as, an, d, 3) \

	VECTOR8i_PERMUTE64_1(a, 0)
	VECTOR8i_PERMUTE64_1(b, 1)
	VECTOR8i_PERMUTE64_1(c, 2)
	VECTOR8i_PERMUTE64_1(d, 3)

	// clang-format on

	__forceinline GSVector8i permute32(const GSVector8i& mask) const
	{
		return GSVector8i(_mm256_permutevar8x32_epi32(m, mask));
	}

	__forceinline GSVector8i broadcast8() const
	{
		return GSVector8i(_mm256_broadcastb_epi8(_mm256_castsi256_si128(m)));
	}

	__forceinline GSVector8i broadcast16() const
	{
		return GSVector8i(_mm256_broadcastw_epi16(_mm256_castsi256_si128(m)));
	}

	__forceinline GSVector8i broadcast32() const
	{
		return GSVector8i(_mm256_broadcastd_epi32(_mm256_castsi256_si128(m)));
	}

	__forceinline GSVector8i broadcast64() const
	{
		return GSVector8i(_mm256_broadcastq_epi64(_mm256_castsi256_si128(m)));
	}

	__forceinline static GSVector8i broadcast8(const GSVector4i& v)
	{
		return GSVector8i(_mm256_broadcastb_epi8(v.m));
	}

	__forceinline static GSVector8i broadcast16(const GSVector4i& v)
	{
		return GSVector8i(_mm256_broadcastw_epi16(v.m));
	}

	__forceinline static GSVector8i broadcast32(const GSVector4i& v)
	{
		return GSVector8i(_mm256_broadcastd_epi32(v.m));
	}

	__forceinline static GSVector8i broadcast64(const GSVector4i& v)
	{
		return GSVector8i(_mm256_broadcastq_epi64(v.m));
	}

	__forceinline static GSVector8i broadcast128(const GSVector4i& v)
	{
		// this one only has m128 source op, it will be saved to a temp on stack if the compiler is not smart enough and use the address of v directly (<= vs2012u3rc2)

		return GSVector8i(_mm256_broadcastsi128_si256(v)); // fastest
		// return GSVector8i(v); // almost as fast as broadcast
		// return cast(v).insert<1>(v); // slow
		// return cast(v).aa(); // slowest
	}

	__forceinline static GSVector8i broadcast8(const void* p)
	{
		return GSVector8i(_mm256_broadcastb_epi8(_mm_cvtsi32_si128(*(const int*)p)));
	}

	__forceinline static GSVector8i broadcast16(const void* p)
	{
		return GSVector8i(_mm256_broadcastw_epi16(_mm_cvtsi32_si128(*(const int*)p)));
	}

	__forceinline static GSVector8i broadcast32(const void* p)
	{
		return GSVector8i(_mm256_broadcastd_epi32(_mm_cvtsi32_si128(*(const int*)p)));
	}

	__forceinline static GSVector8i broadcast64(const void* p)
	{
		return GSVector8i(_mm256_broadcastq_epi64(_mm_loadl_epi64((const __m128i*)p)));
	}

	__forceinline static GSVector8i broadcast128(const void* p)
	{
		return GSVector8i(_mm256_broadcastsi128_si256(*(const __m128i*)p));
	}

	__forceinline static GSVector8i zero() { return GSVector8i(_mm256_setzero_si256()); }

	__forceinline static GSVector8i xffffffff() { return zero() == zero(); }

	__forceinline static GSVector8i x00000001() { return xffffffff().srl32(31); }
	__forceinline static GSVector8i x00000003() { return xffffffff().srl32(30); }
	__forceinline static GSVector8i x00000007() { return xffffffff().srl32(29); }
	__forceinline static GSVector8i x0000000f() { return xffffffff().srl32(28); }
	__forceinline static GSVector8i x0000001f() { return xffffffff().srl32(27); }
	__forceinline static GSVector8i x0000003f() { return xffffffff().srl32(26); }
	__forceinline static GSVector8i x0000007f() { return xffffffff().srl32(25); }
	__forceinline static GSVector8i x000000ff() { return xffffffff().srl32(24); }
	__forceinline static GSVector8i x000001ff() { return xffffffff().srl32(23); }
	__forceinline static GSVector8i x000003ff() { return xffffffff().srl32(22); }
	__forceinline static GSVector8i x000007ff() { return xffffffff().srl32(21); }
	__forceinline static GSVector8i x00000fff() { return xffffffff().srl32(20); }
	__forceinline static GSVector8i x00001fff() { return xffffffff().srl32(19); }
	__forceinline static GSVector8i x00003fff() { return xffffffff().srl32(18); }
	__forceinline static GSVector8i x00007fff() { return xffffffff().srl32(17); }
	__forceinline static GSVector8i x0000ffff() { return xffffffff().srl32(16); }
	__forceinline static GSVector8i x0001ffff() { return xffffffff().srl32(15); }
	__forceinline static GSVector8i x0003ffff() { return xffffffff().srl32(14); }
	__forceinline static GSVector8i x0007ffff() { return xffffffff().srl32(13); }
	__forceinline static GSVector8i x000fffff() { return xffffffff().srl32(12); }
	__forceinline static GSVector8i x001fffff() { return xffffffff().srl32(11); }
	__forceinline static GSVector8i x003fffff() { return xffffffff().srl32(10); }
	__forceinline static GSVector8i x007fffff() { return xffffffff().srl32( 9); }
	__forceinline static GSVector8i x00ffffff() { return xffffffff().srl32( 8); }
	__forceinline static GSVector8i x01ffffff() { return xffffffff().srl32( 7); }
	__forceinline static GSVector8i x03ffffff() { return xffffffff().srl32( 6); }
	__forceinline static GSVector8i x07ffffff() { return xffffffff().srl32( 5); }
	__forceinline static GSVector8i x0fffffff() { return xffffffff().srl32( 4); }
	__forceinline static GSVector8i x1fffffff() { return xffffffff().srl32( 3); }
	__forceinline static GSVector8i x3fffffff() { return xffffffff().srl32( 2); }
	__forceinline static GSVector8i x7fffffff() { return xffffffff().srl32( 1); }

	__forceinline static GSVector8i x80000000() { return xffffffff().sll32(31); }
	__forceinline static GSVector8i xc0000000() { return xffffffff().sll32(30); }
	__forceinline static GSVector8i xe0000000() { return xffffffff().sll32(29); }
	__forceinline static GSVector8i xf0000000() { return xffffffff().sll32(28); }
	__forceinline static GSVector8i xf8000000() { return xffffffff().sll32(27); }
	__forceinline static GSVector8i xfc000000() { return xffffffff().sll32(26); }
	__forceinline static GSVector8i xfe000000() { return xffffffff().sll32(25); }
	__forceinline static GSVector8i xff000000() { return xffffffff().sll32(24); }
	__forceinline static GSVector8i xff800000() { return xffffffff().sll32(23); }
	__forceinline static GSVector8i xffc00000() { return xffffffff().sll32(22); }
	__forceinline static GSVector8i xffe00000() { return xffffffff().sll32(21); }
	__forceinline static GSVector8i xfff00000() { return xffffffff().sll32(20); }
	__forceinline static GSVector8i xfff80000() { return xffffffff().sll32(19); }
	__forceinline static GSVector8i xfffc0000() { return xffffffff().sll32(18); }
	__forceinline static GSVector8i xfffe0000() { return xffffffff().sll32(17); }
	__forceinline static GSVector8i xffff0000() { return xffffffff().sll32(16); }
	__forceinline static GSVector8i xffff8000() { return xffffffff().sll32(15); }
	__forceinline static GSVector8i xffffc000() { return xffffffff().sll32(14); }
	__forceinline static GSVector8i xffffe000() { return xffffffff().sll32(13); }
	__forceinline static GSVector8i xfffff000() { return xffffffff().sll32(12); }
	__forceinline static GSVector8i xfffff800() { return xffffffff().sll32(11); }
	__forceinline static GSVector8i xfffffc00() { return xffffffff().sll32(10); }
	__forceinline static GSVector8i xfffffe00() { return xffffffff().sll32( 9); }
	__forceinline static GSVector8i xffffff00() { return xffffffff().sll32( 8); }
	__forceinline static GSVector8i xffffff80() { return xffffffff().sll32( 7); }
	__forceinline static GSVector8i xffffffc0() { return xffffffff().sll32( 6); }
	__forceinline static GSVector8i xffffffe0() { return xffffffff().sll32( 5); }
	__forceinline static GSVector8i xfffffff0() { return xffffffff().sll32( 4); }
	__forceinline static GSVector8i xfffffff8() { return xffffffff().sll32( 3); }
	__forceinline static GSVector8i xfffffffc() { return xffffffff().sll32( 2); }
	__forceinline static GSVector8i xfffffffe() { return xffffffff().sll32( 1); }

	__forceinline static GSVector8i x0001() { return xffffffff().srl16(15); }
	__forceinline static GSVector8i x0003() { return xffffffff().srl16(14); }
	__forceinline static GSVector8i x0007() { return xffffffff().srl16(13); }
	__forceinline static GSVector8i x000f() { return xffffffff().srl16(12); }
	__forceinline static GSVector8i x001f() { return xffffffff().srl16(11); }
	__forceinline static GSVector8i x003f() { return xffffffff().srl16(10); }
	__forceinline static GSVector8i x007f() { return xffffffff().srl16( 9); }
	__forceinline static GSVector8i x00ff() { return xffffffff().srl16( 8); }
	__forceinline static GSVector8i x01ff() { return xffffffff().srl16( 7); }
	__forceinline static GSVector8i x03ff() { return xffffffff().srl16( 6); }
	__forceinline static GSVector8i x07ff() { return xffffffff().srl16( 5); }
	__forceinline static GSVector8i x0fff() { return xffffffff().srl16( 4); }
	__forceinline static GSVector8i x1fff() { return xffffffff().srl16( 3); }
	__forceinline static GSVector8i x3fff() { return xffffffff().srl16( 2); }
	__forceinline static GSVector8i x7fff() { return xffffffff().srl16( 1); }

	__forceinline static GSVector8i x8000() { return xffffffff().sll16(15); }
	__forceinline static GSVector8i xc000() { return xffffffff().sll16(14); }
	__forceinline static GSVector8i xe000() { return xffffffff().sll16(13); }
	__forceinline static GSVector8i xf000() { return xffffffff().sll16(12); }
	__forceinline static GSVector8i xf800() { return xffffffff().sll16(11); }
	__forceinline static GSVector8i xfc00() { return xffffffff().sll16(10); }
	__forceinline static GSVector8i xfe00() { return xffffffff().sll16( 9); }
	__forceinline static GSVector8i xff00() { return xffffffff().sll16( 8); }
	__forceinline static GSVector8i xff80() { return xffffffff().sll16( 7); }
	__forceinline static GSVector8i xffc0() { return xffffffff().sll16( 6); }
	__forceinline static GSVector8i xffe0() { return xffffffff().sll16( 5); }
	__forceinline static GSVector8i xfff0() { return xffffffff().sll16( 4); }
	__forceinline static GSVector8i xfff8() { return xffffffff().sll16( 3); }
	__forceinline static GSVector8i xfffc() { return xffffffff().sll16( 2); }
	__forceinline static GSVector8i xfffe() { return xffffffff().sll16( 1); }

	__forceinline static GSVector8i xffffffff(const GSVector8i& v) { return v == v; }

	__forceinline static GSVector8i x00000001(const GSVector8i& v) { return xffffffff(v).srl32(31); }
	__forceinline static GSVector8i x00000003(const GSVector8i& v) { return xffffffff(v).srl32(30); }
	__forceinline static GSVector8i x00000007(const GSVector8i& v) { return xffffffff(v).srl32(29); }
	__forceinline static GSVector8i x0000000f(const GSVector8i& v) { return xffffffff(v).srl32(28); }
	__forceinline static GSVector8i x0000001f(const GSVector8i& v) { return xffffffff(v).srl32(27); }
	__forceinline static GSVector8i x0000003f(const GSVector8i& v) { return xffffffff(v).srl32(26); }
	__forceinline static GSVector8i x0000007f(const GSVector8i& v) { return xffffffff(v).srl32(25); }
	__forceinline static GSVector8i x000000ff(const GSVector8i& v) { return xffffffff(v).srl32(24); }
	__forceinline static GSVector8i x000001ff(const GSVector8i& v) { return xffffffff(v).srl32(23); }
	__forceinline static GSVector8i x000003ff(const GSVector8i& v) { return xffffffff(v).srl32(22); }
	__forceinline static GSVector8i x000007ff(const GSVector8i& v) { return xffffffff(v).srl32(21); }
	__forceinline static GSVector8i x00000fff(const GSVector8i& v) { return xffffffff(v).srl32(20); }
	__forceinline static GSVector8i x00001fff(const GSVector8i& v) { return xffffffff(v).srl32(19); }
	__forceinline static GSVector8i x00003fff(const GSVector8i& v) { return xffffffff(v).srl32(18); }
	__forceinline static GSVector8i x00007fff(const GSVector8i& v) { return xffffffff(v).srl32(17); }
	__forceinline static GSVector8i x0000ffff(const GSVector8i& v) { return xffffffff(v).srl32(16); }
	__forceinline static GSVector8i x0001ffff(const GSVector8i& v) { return xffffffff(v).srl32(15); }
	__forceinline static GSVector8i x0003ffff(const GSVector8i& v) { return xffffffff(v).srl32(14); }
	__forceinline static GSVector8i x0007ffff(const GSVector8i& v) { return xffffffff(v).srl32(13); }
	__forceinline static GSVector8i x000fffff(const GSVector8i& v) { return xffffffff(v).srl32(12); }
	__forceinline static GSVector8i x001fffff(const GSVector8i& v) { return xffffffff(v).srl32(11); }
	__forceinline static GSVector8i x003fffff(const GSVector8i& v) { return xffffffff(v).srl32(10); }
	__forceinline static GSVector8i x007fffff(const GSVector8i& v) { return xffffffff(v).srl32( 9); }
	__forceinline static GSVector8i x00ffffff(const GSVector8i& v) { return xffffffff(v).srl32( 8); }
	__forceinline static GSVector8i x01ffffff(const GSVector8i& v) { return xffffffff(v).srl32( 7); }
	__forceinline static GSVector8i x03ffffff(const GSVector8i& v) { return xffffffff(v).srl32( 6); }
	__forceinline static GSVector8i x07ffffff(const GSVector8i& v) { return xffffffff(v).srl32( 5); }
	__forceinline static GSVector8i x0fffffff(const GSVector8i& v) { return xffffffff(v).srl32( 4); }
	__forceinline static GSVector8i x1fffffff(const GSVector8i& v) { return xffffffff(v).srl32( 3); }
	__forceinline static GSVector8i x3fffffff(const GSVector8i& v) { return xffffffff(v).srl32( 2); }
	__forceinline static GSVector8i x7fffffff(const GSVector8i& v) { return xffffffff(v).srl32( 1); }

	__forceinline static GSVector8i x80000000(const GSVector8i& v) { return xffffffff(v).sll32(31); }
	__forceinline static GSVector8i xc0000000(const GSVector8i& v) { return xffffffff(v).sll32(30); }
	__forceinline static GSVector8i xe0000000(const GSVector8i& v) { return xffffffff(v).sll32(29); }
	__forceinline static GSVector8i xf0000000(const GSVector8i& v) { return xffffffff(v).sll32(28); }
	__forceinline static GSVector8i xf8000000(const GSVector8i& v) { return xffffffff(v).sll32(27); }
	__forceinline static GSVector8i xfc000000(const GSVector8i& v) { return xffffffff(v).sll32(26); }
	__forceinline static GSVector8i xfe000000(const GSVector8i& v) { return xffffffff(v).sll32(25); }
	__forceinline static GSVector8i xff000000(const GSVector8i& v) { return xffffffff(v).sll32(24); }
	__forceinline static GSVector8i xff800000(const GSVector8i& v) { return xffffffff(v).sll32(23); }
	__forceinline static GSVector8i xffc00000(const GSVector8i& v) { return xffffffff(v).sll32(22); }
	__forceinline static GSVector8i xffe00000(const GSVector8i& v) { return xffffffff(v).sll32(21); }
	__forceinline static GSVector8i xfff00000(const GSVector8i& v) { return xffffffff(v).sll32(20); }
	__forceinline static GSVector8i xfff80000(const GSVector8i& v) { return xffffffff(v).sll32(19); }
	__forceinline static GSVector8i xfffc0000(const GSVector8i& v) { return xffffffff(v).sll32(18); }
	__forceinline static GSVector8i xfffe0000(const GSVector8i& v) { return xffffffff(v).sll32(17); }
	__forceinline static GSVector8i xffff0000(const GSVector8i& v) { return xffffffff(v).sll32(16); }
	__forceinline static GSVector8i xffff8000(const GSVector8i& v) { return xffffffff(v).sll32(15); }
	__forceinline static GSVector8i xffffc000(const GSVector8i& v) { return xffffffff(v).sll32(14); }
	__forceinline static GSVector8i xffffe000(const GSVector8i& v) { return xffffffff(v).sll32(13); }
	__forceinline static GSVector8i xfffff000(const GSVector8i& v) { return xffffffff(v).sll32(12); }
	__forceinline static GSVector8i xfffff800(const GSVector8i& v) { return xffffffff(v).sll32(11); }
	__forceinline static GSVector8i xfffffc00(const GSVector8i& v) { return xffffffff(v).sll32(10); }
	__forceinline static GSVector8i xfffffe00(const GSVector8i& v) { return xffffffff(v).sll32( 9); }
	__forceinline static GSVector8i xffffff00(const GSVector8i& v) { return xffffffff(v).sll32( 8); }
	__forceinline static GSVector8i xffffff80(const GSVector8i& v) { return xffffffff(v).sll32( 7); }
	__forceinline static GSVector8i xffffffc0(const GSVector8i& v) { return xffffffff(v).sll32( 6); }
	__forceinline static GSVector8i xffffffe0(const GSVector8i& v) { return xffffffff(v).sll32( 5); }
	__forceinline static GSVector8i xfffffff0(const GSVector8i& v) { return xffffffff(v).sll32( 4); }
	__forceinline static GSVector8i xfffffff8(const GSVector8i& v) { return xffffffff(v).sll32( 3); }
	__forceinline static GSVector8i xfffffffc(const GSVector8i& v) { return xffffffff(v).sll32( 2); }
	__forceinline static GSVector8i xfffffffe(const GSVector8i& v) { return xffffffff(v).sll32( 1); }

	__forceinline static GSVector8i x0001(const GSVector8i& v) { return xffffffff(v).srl16(15); }
	__forceinline static GSVector8i x0003(const GSVector8i& v) { return xffffffff(v).srl16(14); }
	__forceinline static GSVector8i x0007(const GSVector8i& v) { return xffffffff(v).srl16(13); }
	__forceinline static GSVector8i x000f(const GSVector8i& v) { return xffffffff(v).srl16(12); }
	__forceinline static GSVector8i x001f(const GSVector8i& v) { return xffffffff(v).srl16(11); }
	__forceinline static GSVector8i x003f(const GSVector8i& v) { return xffffffff(v).srl16(10); }
	__forceinline static GSVector8i x007f(const GSVector8i& v) { return xffffffff(v).srl16( 9); }
	__forceinline static GSVector8i x00ff(const GSVector8i& v) { return xffffffff(v).srl16( 8); }
	__forceinline static GSVector8i x01ff(const GSVector8i& v) { return xffffffff(v).srl16( 7); }
	__forceinline static GSVector8i x03ff(const GSVector8i& v) { return xffffffff(v).srl16( 6); }
	__forceinline static GSVector8i x07ff(const GSVector8i& v) { return xffffffff(v).srl16( 5); }
	__forceinline static GSVector8i x0fff(const GSVector8i& v) { return xffffffff(v).srl16( 4); }
	__forceinline static GSVector8i x1fff(const GSVector8i& v) { return xffffffff(v).srl16( 3); }
	__forceinline static GSVector8i x3fff(const GSVector8i& v) { return xffffffff(v).srl16( 2); }
	__forceinline static GSVector8i x7fff(const GSVector8i& v) { return xffffffff(v).srl16( 1); }

	__forceinline static GSVector8i x8000(const GSVector8i& v) { return xffffffff(v).sll16(15); }
	__forceinline static GSVector8i xc000(const GSVector8i& v) { return xffffffff(v).sll16(14); }
	__forceinline static GSVector8i xe000(const GSVector8i& v) { return xffffffff(v).sll16(13); }
	__forceinline static GSVector8i xf000(const GSVector8i& v) { return xffffffff(v).sll16(12); }
	__forceinline static GSVector8i xf800(const GSVector8i& v) { return xffffffff(v).sll16(11); }
	__forceinline static GSVector8i xfc00(const GSVector8i& v) { return xffffffff(v).sll16(10); }
	__forceinline static GSVector8i xfe00(const GSVector8i& v) { return xffffffff(v).sll16( 9); }
	__forceinline static GSVector8i xff00(const GSVector8i& v) { return xffffffff(v).sll16( 8); }
	__forceinline static GSVector8i xff80(const GSVector8i& v) { return xffffffff(v).sll16( 7); }
	__forceinline static GSVector8i xffc0(const GSVector8i& v) { return xffffffff(v).sll16( 6); }
	__forceinline static GSVector8i xffe0(const GSVector8i& v) { return xffffffff(v).sll16( 5); }
	__forceinline static GSVector8i xfff0(const GSVector8i& v) { return xffffffff(v).sll16( 4); }
	__forceinline static GSVector8i xfff8(const GSVector8i& v) { return xffffffff(v).sll16( 3); }
	__forceinline static GSVector8i xfffc(const GSVector8i& v) { return xffffffff(v).sll16( 2); }
	__forceinline static GSVector8i xfffe(const GSVector8i& v) { return xffffffff(v).sll16( 1); }

	__forceinline static GSVector8i xff(int n) { return m_xff[n]; }
	__forceinline static GSVector8i x0f(int n) { return m_x0f[n]; }
};

#endif
