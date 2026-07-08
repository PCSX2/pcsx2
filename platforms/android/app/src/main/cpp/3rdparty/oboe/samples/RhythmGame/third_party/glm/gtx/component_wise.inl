/// @ref gtx_component_wise
/// @file glm/gtx/component_wise.inl

#include <limits>

namespace glm{
namespace detail
{
	template <typename T, typename floatType, precision P, template <typename, precision> class vecType, bool isInteger, bool signedType>
	struct compute_compNormalize
	{};

	template <typename T, typename floatType, precision P, template <typename, precision> class vecType>
	struct compute_compNormalize<T, floatType, P, vecType, true, true>
	{
		GLM_FUNC_QUALIFIER static vecType<floatType, P> call(vecType<T, P> const & v)
		{
			floatType const Min = static_cast<floatType>(std::numeric_limits<T>::min());
			floatType const Max = static_cast<floatType>(std::numeric_limits<T>::max());
			return (vecType<floatType, P>(v) - Min) / (Max - Min) * static_cast<floatType>(2) - static_cast<floatType>(1);
		}
	};

	template <typename T, typename floatType, precision P, template <typename, precision> class vecType>
	struct compute_compNormalize<T, floatType, P, vecType, true, false>
	{
		GLM_FUNC_QUALIFIER static vecType<floatType, P> call(vecType<T, P> const & v)
		{
			return vecType<floatType, P>(v) / static_cast<floatType>(std::numeric_limits<T>::max());
		}
	};

	template <typename T, typename floatType, precision P, template <typename, precision> class vecType>
	struct compute_compNormalize<T, floatType, P, vecType, false, true>
	{
		GLM_FUNC_QUALIFIER static vecType<floatType, P> call(vecType<T, P> const & v)
		{
			return v;
		}
	};

	template <typename T, typename floatType, precision P, template <typename, precision> class vecType, bool isInteger, bool signedType>
	struct compute_compScale
	{};

	template <typename T, typename floatType, precision P, template <typename, precision> class vecType>
	struct compute_compScale<T, floatType, P, vecType, true, true>
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<floatType, P> const & v)
		{
			floatType const Max = static_cast<floatType>(std::numeric_limits<T>::max()) + static_cast<floatType>(0.5);
			vecType<floatType, P> const Scaled(v * Max);
			vecType<T, P> const Result(Scaled - static_cast<floatType>(0.5));
			return Result;
		}
	};

	template <typename T, typename floatType, precision P, template <typename, precision> class vecType>
	struct compute_compScale<T, floatType, P, vecType, true, false>
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<floatType, P> const & v)
		{
			return vecType<T, P>(vecType<floatType, P>(v) * static_cast<floatType>(std::numeric_limits<T>::max()));
		}
	};

	template <typename T, typename floatType, precision P, template <typename, precision> class vecType>
	struct compute_compScale<T, floatType, P, vecType, false, true>
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<floatType, P> const & v)
		{
			return v;
		}
	};
}//namespace detail

	template <typename floatType, typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<floatType, P> compNormalize(vecType<T, P> const & v)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<floatType>::is_iec559, "'compNormalize' accepts only floating-point types for 'floatType' template parameter");

		return detail::compute_compNormalize<T, floatType, P, vecType, std::numeric_limits<T>::is_integer, std::numeric_limits<T>::is_signed>::call(v);
	}

	template <typename T, typename floatType, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> compScale(vecType<floatType, P> const & v)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<floatType>::is_iec559, "'compScale' accepts only floating-point types for 'floatType' template parameter");

		return detail::compute_compScale<T, floatType, P, vecType, std::numeric_limits<T>::is_integer, std::numeric_limits<T>::is_signed>::call(v);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER T compAdd(vecType<T, P> const & v)
	{
		T Result(0);
		for(length_t i = 0, n = v.length(); i < n; ++i)
			Result += v[i];
		return Result;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER T compMul(vecType<T, P> const & v)
	{
		T Result(1);
		for(length_t i = 0, n = v.length(); i < n; ++i)
			Result *= v[i];
		return Result;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER T compMin(vecType<T, P> const & v)
	{
		T Result(v[0]);
		for(length_t i = 1, n = v.length(); i < n; ++i)
			Result = min(Result, v[i]);
		return Result;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER T compMax(vecType<T, P> const & v)
	{
		T Result(v[0]);
		for(length_t i = 1, n = v.length(); i < n; ++i)
			Result = max(Result, v[i]);
		return Result;
	}
}//namespace glm
