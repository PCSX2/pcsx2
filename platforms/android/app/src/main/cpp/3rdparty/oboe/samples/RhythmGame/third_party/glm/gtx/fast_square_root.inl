/// @ref gtx_fast_square_root
/// @file glm/gtx/fast_square_root.inl

namespace glm
{
	// fastSqrt
	template <typename genType>
	GLM_FUNC_QUALIFIER genType fastSqrt(genType x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'fastSqrt' only accept floating-point input");

		return genType(1) / fastInverseSqrt(x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> fastSqrt(vecType<T, P> const & x)
	{
		return detail::functor1<T, T, P, vecType>::call(fastSqrt, x);
	}

	// fastInversesqrt
	template <typename genType>
	GLM_FUNC_QUALIFIER genType fastInverseSqrt(genType x)
	{
#		ifdef __CUDACC__ // Wordaround for a CUDA compiler bug up to CUDA6
			tvec1<T, P> tmp(detail::compute_inversesqrt<tvec1, genType, lowp, detail::is_aligned<lowp>::value>::call(tvec1<genType, lowp>(x)));
			return tmp.x;
#		else
			return detail::compute_inversesqrt<tvec1, genType, highp, detail::is_aligned<highp>::value>::call(tvec1<genType, lowp>(x)).x;
#		endif
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> fastInverseSqrt(vecType<T, P> const & x)
	{
		return detail::compute_inversesqrt<vecType, T, P, detail::is_aligned<P>::value>::call(x);
	}

	// fastLength
	template <typename genType>
	GLM_FUNC_QUALIFIER genType fastLength(genType x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'fastLength' only accept floating-point inputs");

		return abs(x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER T fastLength(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fastLength' only accept floating-point inputs");

		return fastSqrt(dot(x, x));
	}

	// fastDistance
	template <typename genType>
	GLM_FUNC_QUALIFIER genType fastDistance(genType x, genType y)
	{
		return fastLength(y - x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER T fastDistance(vecType<T, P> const & x, vecType<T, P> const & y)
	{
		return fastLength(y - x);
	}

	// fastNormalize
	template <typename genType>
	GLM_FUNC_QUALIFIER genType fastNormalize(genType x)
	{
		return x > genType(0) ? genType(1) : -genType(1);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> fastNormalize(vecType<T, P> const & x)
	{
		return x * fastInverseSqrt(dot(x, x));
	}
}//namespace glm
