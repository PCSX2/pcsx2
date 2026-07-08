/// @ref core
/// @file glm/detail/type_tvec4_simd.inl

#if GLM_ARCH & GLM_ARCH_SSE2_BIT

namespace glm{
namespace detail
{
#	if GLM_SWIZZLE == GLM_SWIZZLE_ENABLED
	template <precision P, int E0, int E1, int E2, int E3>
	struct _swizzle_base1<4, float, P, glm::tvec4, E0,E1,E2,E3, true> : public _swizzle_base0<float, 4>
	{ 
		GLM_FUNC_QUALIFIER tvec4<float, P> operator ()()  const
		{
			__m128 data = *reinterpret_cast<__m128 const*>(&this->_buffer);

			tvec4<float, P> Result(uninitialize);
#			if GLM_ARCH & GLM_ARCH_AVX_BIT
				Result.data = _mm_permute_ps(data, _MM_SHUFFLE(E3, E2, E1, E0));
#			else
				Result.data = _mm_shuffle_ps(data, data, _MM_SHUFFLE(E3, E2, E1, E0));
#			endif
			return Result;
		}
	};

	template <precision P, int E0, int E1, int E2, int E3>
	struct _swizzle_base1<4, int32, P, glm::tvec4, E0,E1,E2,E3, true> : public _swizzle_base0<int32, 4>
	{ 
		GLM_FUNC_QUALIFIER tvec4<int32, P> operator ()()  const
		{
			__m128i data = *reinterpret_cast<__m128i const*>(&this->_buffer);

			tvec4<int32, P> Result(uninitialize);
			Result.data = _mm_shuffle_epi32(data, _MM_SHUFFLE(E3, E2, E1, E0));
			return Result;
		}
	};

	template <precision P, int E0, int E1, int E2, int E3>
	struct _swizzle_base1<4, uint32, P, glm::tvec4, E0,E1,E2,E3, true> : public _swizzle_base0<uint32, 4>
	{ 
		GLM_FUNC_QUALIFIER tvec4<uint32, P> operator ()()  const
		{
			__m128i data = *reinterpret_cast<__m128i const*>(&this->_buffer);

			tvec4<uint32, P> Result(uninitialize);
			Result.data = _mm_shuffle_epi32(data, _MM_SHUFFLE(E3, E2, E1, E0));
			return Result;
		}
	};
#	endif// GLM_SWIZZLE == GLM_SWIZZLE_ENABLED

	template <precision P>
	struct compute_vec4_add<float, P, true>
	{
		static tvec4<float, P> call(tvec4<float, P> const & a, tvec4<float, P> const & b)
		{
			tvec4<float, P> Result(uninitialize);
			Result.data = _mm_add_ps(a.data, b.data);
			return Result;
		}
	};

#	if GLM_ARCH & GLM_ARCH_AVX_BIT
	template <precision P>
	struct compute_vec4_add<double, P, true>
	{
		static tvec4<double, P> call(tvec4<double, P> const & a, tvec4<double, P> const & b)
		{
			tvec4<double, P> Result(uninitialize);
			Result.data = _mm256_add_pd(a.data, b.data);
			return Result;
		}
	};
#	endif

	template <precision P>
	struct compute_vec4_sub<float, P, true>
	{
		static tvec4<float, P> call(tvec4<float, P> const & a, tvec4<float, P> const & b)
		{
			tvec4<float, P> Result(uninitialize);
			Result.data = _mm_sub_ps(a.data, b.data);
			return Result;
		}
	};

#	if GLM_ARCH & GLM_ARCH_AVX_BIT
	template <precision P>
	struct compute_vec4_sub<double, P, true>
	{
		static tvec4<double, P> call(tvec4<double, P> const & a, tvec4<double, P> const & b)
		{
			tvec4<double, P> Result(uninitialize);
			Result.data = _mm256_sub_pd(a.data, b.data);
			return Result;
		}
	};
#	endif

	template <precision P>
	struct compute_vec4_mul<float, P, true>
	{
		static tvec4<float, P> call(tvec4<float, P> const & a, tvec4<float, P> const & b)
		{
			tvec4<float, P> Result(uninitialize);
			Result.data = _mm_mul_ps(a.data, b.data);
			return Result;
		}
	};

#	if GLM_ARCH & GLM_ARCH_AVX_BIT
	template <precision P>
	struct compute_vec4_mul<double, P, true>
	{
		static tvec4<double, P> call(tvec4<double, P> const & a, tvec4<double, P> const & b)
		{
			tvec4<double, P> Result(uninitialize);
			Result.data = _mm256_mul_pd(a.data, b.data);
			return Result;
		}
	};
#	endif

	template <precision P>
	struct compute_vec4_div<float, P, true>
	{
		static tvec4<float, P> call(tvec4<float, P> const & a, tvec4<float, P> const & b)
		{
			tvec4<float, P> Result(uninitialize);
			Result.data = _mm_div_ps(a.data, b.data);
			return Result;
		}
	};

	#	if GLM_ARCH & GLM_ARCH_AVX_BIT
	template <precision P>
	struct compute_vec4_div<double, P, true>
	{
		static tvec4<double, P> call(tvec4<double, P> const & a, tvec4<double, P> const & b)
		{
			tvec4<double, P> Result(uninitialize);
			Result.data = _mm256_div_pd(a.data, b.data);
			return Result;
		}
	};
#	endif

	template <>
	struct compute_vec4_div<float, aligned_lowp, true>
	{
		static tvec4<float, aligned_lowp> call(tvec4<float, aligned_lowp> const & a, tvec4<float, aligned_lowp> const & b)
		{
			tvec4<float, aligned_lowp> Result(uninitialize);
			Result.data = _mm_mul_ps(a.data, _mm_rcp_ps(b.data));
			return Result;
		}
	};

	template <typename T, precision P>
	struct compute_vec4_and<T, P, true, 32, true>
	{
		static tvec4<T, P> call(tvec4<T, P> const& a, tvec4<T, P> const& b)
		{
			tvec4<T, P> Result(uninitialize);
			Result.data = _mm_and_si128(a.data, b.data);
			return Result;
		}
	};

#	if GLM_ARCH & GLM_ARCH_AVX2_BIT
	template <typename T, precision P>
	struct compute_vec4_and<T, P, true, 64, true>
	{
		static tvec4<T, P> call(tvec4<T, P> const& a, tvec4<T, P> const& b)
		{
			tvec4<T, P> Result(uninitialize);
			Result.data = _mm256_and_si256(a.data, b.data);
			return Result;
		}
	};
#	endif

	template <typename T, precision P>
	struct compute_vec4_or<T, P, true, 32, true>
	{
		static tvec4<T, P> call(tvec4<T, P> const& a, tvec4<T, P> const& b)
		{
			tvec4<T, P> Result(uninitialize);
			Result.data = _mm_or_si128(a.data, b.data);
			return Result;
		}
	};

#	if GLM_ARCH & GLM_ARCH_AVX2_BIT
	template <typename T, precision P>
	struct compute_vec4_or<T, P, true, 64, true>
	{
		static tvec4<T, P> call(tvec4<T, P> const& a, tvec4<T, P> const& b)
		{
			tvec4<T, P> Result(uninitialize);
			Result.data = _mm256_or_si256(a.data, b.data);
			return Result;
		}
	};
#	endif

	template <typename T, precision P>
	struct compute_vec4_xor<T, P, true, 32, true>
	{
		static tvec4<T, P> call(tvec4<T, P> const& a, tvec4<T, P> const& b)
		{
			tvec4<T, P> Result(uninitialize);
			Result.data = _mm_xor_si128(a.data, b.data);
			return Result;
		}
	};

#	if GLM_ARCH & GLM_ARCH_AVX2_BIT
	template <typename T, precision P>
	struct compute_vec4_xor<T, P, true, 64, true>
	{
		static tvec4<T, P> call(tvec4<T, P> const& a, tvec4<T, P> const& b)
		{
			tvec4<T, P> Result(uninitialize);
			Result.data = _mm256_xor_si256(a.data, b.data);
			return Result;
		}
	};
#	endif

	template <typename T, precision P>
	struct compute_vec4_shift_left<T, P, true, 32, true>
	{
		static tvec4<T, P> call(tvec4<T, P> const& a, tvec4<T, P> const& b)
		{
			tvec4<T, P> Result(uninitialize);
			Result.data = _mm_sll_epi32(a.data, b.data);
			return Result;
		}
	};

#	if GLM_ARCH & GLM_ARCH_AVX2_BIT
	template <typename T, precision P>
	struct compute_vec4_shift_left<T, P, true, 64, true>
	{
		static tvec4<T, P> call(tvec4<T, P> const& a, tvec4<T, P> const& b)
		{
			tvec4<T, P> Result(uninitialize);
			Result.data = _mm256_sll_epi64(a.data, b.data);
			return Result;
		}
	};
#	endif

	template <typename T, precision P>
	struct compute_vec4_shift_right<T, P, true, 32, true>
	{
		static tvec4<T, P> call(tvec4<T, P> const& a, tvec4<T, P> const& b)
		{
			tvec4<T, P> Result(uninitialize);
			Result.data = _mm_srl_epi32(a.data, b.data);
			return Result;
		}
	};

#	if GLM_ARCH & GLM_ARCH_AVX2_BIT
	template <typename T, precision P>
	struct compute_vec4_shift_right<T, P, true, 64, true>
	{
		static tvec4<T, P> call(tvec4<T, P> const& a, tvec4<T, P> const& b)
		{
			tvec4<T, P> Result(uninitialize);
			Result.data = _mm256_srl_epi64(a.data, b.data);
			return Result;
		}
	};
#	endif

	template <typename T, precision P>
	struct compute_vec4_bitwise_not<T, P, true, 32, true>
	{
		static tvec4<T, P> call(tvec4<T, P> const & v)
		{
			tvec4<T, P> Result(uninitialize);
			Result.data = _mm_xor_si128(v.data, _mm_set1_epi32(-1));
			return Result;
		}
	};

#	if GLM_ARCH & GLM_ARCH_AVX2_BIT
	template <typename T, precision P>
	struct compute_vec4_bitwise_not<T, P, true, 64, true>
	{
		static tvec4<T, P> call(tvec4<T, P> const & v)
		{
			tvec4<T, P> Result(uninitialize);
			Result.data = _mm256_xor_si256(v.data, _mm_set1_epi32(-1));
			return Result;
		}
	};
#	endif

	template <precision P>
	struct compute_vec4_equal<float, P, false, 32, true>
	{
		static bool call(tvec4<float, P> const & v1, tvec4<float, P> const & v2)
		{
			return _mm_movemask_ps(_mm_cmpeq_ps(v1.data, v2.data)) != 0;
		}
	};

	template <precision P>
	struct compute_vec4_equal<int32, P, true, 32, true>
	{
		static bool call(tvec4<int32, P> const & v1, tvec4<int32, P> const & v2)
		{
			return _mm_movemask_epi8(_mm_cmpeq_epi32(v1.data, v2.data)) != 0;
		}
	};

	template <precision P>
	struct compute_vec4_nequal<float, P, false, 32, true>
	{
		static bool call(tvec4<float, P> const & v1, tvec4<float, P> const & v2)
		{
			return _mm_movemask_ps(_mm_cmpneq_ps(v1.data, v2.data)) != 0;
		}
	};

	template <precision P>
	struct compute_vec4_nequal<int32, P, true, 32, true>
	{
		static bool call(tvec4<int32, P> const & v1, tvec4<int32, P> const & v2)
		{
			return _mm_movemask_epi8(_mm_cmpneq_epi32(v1.data, v2.data)) != 0;
		}
	};
}//namespace detail

#	if !GLM_HAS_DEFAULTED_FUNCTIONS
		template <>
		GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<float, aligned_lowp>::tvec4()
#			ifndef GLM_FORCE_NO_CTOR_INIT
				: data(_mm_setzero_ps())
#			endif
		{}

		template <>
		GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<float, aligned_mediump>::tvec4()
#			ifndef GLM_FORCE_NO_CTOR_INIT
			: data(_mm_setzero_ps())
#			endif
		{}

		template <>
		GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<float, aligned_highp>::tvec4()
#			ifndef GLM_FORCE_NO_CTOR_INIT
			: data(_mm_setzero_ps())
#			endif
		{}
#	endif//!GLM_HAS_DEFAULTED_FUNCTIONS

	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<float, aligned_lowp>::tvec4(float s) :
		data(_mm_set1_ps(s))
	{}

	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<float, aligned_mediump>::tvec4(float s) :
		data(_mm_set1_ps(s))
	{}

	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<float, aligned_highp>::tvec4(float s) :
		data(_mm_set1_ps(s))
	{}

#	if GLM_ARCH & GLM_ARCH_AVX_BIT
	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<double, aligned_lowp>::tvec4(double s) :
		data(_mm256_set1_pd(s))
	{}

	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<double, aligned_mediump>::tvec4(double s) :
		data(_mm256_set1_pd(s))
	{}

	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<double, aligned_highp>::tvec4(double s) :
		data(_mm256_set1_pd(s))
	{}
#	endif

	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<int32, aligned_lowp>::tvec4(int32 s) :
		data(_mm_set1_epi32(s))
	{}

	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<int32, aligned_mediump>::tvec4(int32 s) :
		data(_mm_set1_epi32(s))
	{}

	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<int32, aligned_highp>::tvec4(int32 s) :
		data(_mm_set1_epi32(s))
	{}

#	if GLM_ARCH & GLM_ARCH_AVX2_BIT
	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<int64, aligned_lowp>::tvec4(int64 s) :
		data(_mm256_set1_epi64x(s))
	{}

	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<int64, aligned_mediump>::tvec4(int64 s) :
		data(_mm256_set1_epi64x(s))
	{}

	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<int64, aligned_highp>::tvec4(int64 s) :
		data(_mm256_set1_epi64x(s))
	{}
#	endif

	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<float, aligned_lowp>::tvec4(float a, float b, float c, float d) :
		data(_mm_set_ps(d, c, b, a))
	{}

	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<float, aligned_mediump>::tvec4(float a, float b, float c, float d) :
		data(_mm_set_ps(d, c, b, a))
	{}

	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<float, aligned_highp>::tvec4(float a, float b, float c, float d) :
		data(_mm_set_ps(d, c, b, a))
	{}

	template <>
	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<int32, aligned_lowp>::tvec4(int32 a, int32 b, int32 c, int32 d) :
		data(_mm_set_epi32(d, c, b, a))
	{}

	template <>
	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<int32, aligned_mediump>::tvec4(int32 a, int32 b, int32 c, int32 d) :
		data(_mm_set_epi32(d, c, b, a))
	{}

	template <>
	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<int32, aligned_highp>::tvec4(int32 a, int32 b, int32 c, int32 d) :
		data(_mm_set_epi32(d, c, b, a))
	{}

	template <>
	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<float, aligned_lowp>::tvec4(int32 a, int32 b, int32 c, int32 d) :
		data(_mm_castsi128_ps(_mm_set_epi32(d, c, b, a)))
	{}

	template <>
	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<float, aligned_mediump>::tvec4(int32 a, int32 b, int32 c, int32 d) :
		data(_mm_castsi128_ps(_mm_set_epi32(d, c, b, a)))
	{}

	template <>
	template <>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_SIMD tvec4<float, aligned_highp>::tvec4(int32 a, int32 b, int32 c, int32 d) :
		data(_mm_castsi128_ps(_mm_set_epi32(d, c, b, a)))
	{}
}//namespace glm

#endif//GLM_ARCH & GLM_ARCH_SSE2_BIT
