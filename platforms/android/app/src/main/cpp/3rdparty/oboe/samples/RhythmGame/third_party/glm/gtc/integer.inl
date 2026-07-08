/// @ref gtc_integer
/// @file glm/gtc/integer.inl

namespace glm{
namespace detail
{
	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_log2<T, P, vecType, false, Aligned>
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & vec)
		{
			//Equivalent to return findMSB(vec); but save one function call in ASM with VC
			//return findMSB(vec);
			return vecType<T, P>(detail::compute_findMSB_vec<T, P, vecType, sizeof(T) * 8>::call(vec));
		}
	};

#	if GLM_HAS_BITSCAN_WINDOWS
		template <precision P, bool Aligned>
		struct compute_log2<int, P, tvec4, false, Aligned>
		{
			GLM_FUNC_QUALIFIER static tvec4<int, P> call(tvec4<int, P> const & vec)
			{
				tvec4<int, P> Result(glm::uninitialize);

				_BitScanReverse(reinterpret_cast<unsigned long*>(&Result.x), vec.x);
				_BitScanReverse(reinterpret_cast<unsigned long*>(&Result.y), vec.y);
				_BitScanReverse(reinterpret_cast<unsigned long*>(&Result.z), vec.z);
				_BitScanReverse(reinterpret_cast<unsigned long*>(&Result.w), vec.w);

				return Result;
			}
		};
#	endif//GLM_HAS_BITSCAN_WINDOWS
}//namespace detail
	template <typename genType>
	GLM_FUNC_QUALIFIER int iround(genType x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'iround' only accept floating-point inputs");
		assert(static_cast<genType>(0.0) <= x);

		return static_cast<int>(x + static_cast<genType>(0.5));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<int, P> iround(vecType<T, P> const& x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'iround' only accept floating-point inputs");
		assert(all(lessThanEqual(vecType<T, P>(0), x)));

		return vecType<int, P>(x + static_cast<T>(0.5));
	}

	template <typename genType>
	GLM_FUNC_QUALIFIER uint uround(genType x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'uround' only accept floating-point inputs");
		assert(static_cast<genType>(0.0) <= x);

		return static_cast<uint>(x + static_cast<genType>(0.5));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<uint, P> uround(vecType<T, P> const& x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'uround' only accept floating-point inputs");
		assert(all(lessThanEqual(vecType<T, P>(0), x)));

		return vecType<uint, P>(x + static_cast<T>(0.5));
	}
}//namespace glm
