/// @ref gtc_reciprocal
/// @file glm/gtc/reciprocal.inl

#include "../trigonometric.hpp"
#include <limits>

namespace glm
{
	// sec
	template <typename genType>
	GLM_FUNC_QUALIFIER genType sec(genType angle)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'sec' only accept floating-point values");
		return genType(1) / glm::cos(angle);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> sec(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'sec' only accept floating-point inputs");
		return detail::functor1<T, T, P, vecType>::call(sec, x);
	}

	// csc
	template <typename genType>
	GLM_FUNC_QUALIFIER genType csc(genType angle)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'csc' only accept floating-point values");
		return genType(1) / glm::sin(angle);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> csc(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'csc' only accept floating-point inputs");
		return detail::functor1<T, T, P, vecType>::call(csc, x);
	}

	// cot
	template <typename genType>
	GLM_FUNC_QUALIFIER genType cot(genType angle)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'cot' only accept floating-point values");
	
		genType const pi_over_2 = genType(3.1415926535897932384626433832795 / 2.0);
		return glm::tan(pi_over_2 - angle);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> cot(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'cot' only accept floating-point inputs");
		return detail::functor1<T, T, P, vecType>::call(cot, x);
	}

	// asec
	template <typename genType>
	GLM_FUNC_QUALIFIER genType asec(genType x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'asec' only accept floating-point values");
		return acos(genType(1) / x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> asec(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'asec' only accept floating-point inputs");
		return detail::functor1<T, T, P, vecType>::call(asec, x);
	}

	// acsc
	template <typename genType>
	GLM_FUNC_QUALIFIER genType acsc(genType x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'acsc' only accept floating-point values");
		return asin(genType(1) / x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> acsc(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'acsc' only accept floating-point inputs");
		return detail::functor1<T, T, P, vecType>::call(acsc, x);
	}

	// acot
	template <typename genType>
	GLM_FUNC_QUALIFIER genType acot(genType x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'acot' only accept floating-point values");

		genType const pi_over_2 = genType(3.1415926535897932384626433832795 / 2.0);
		return pi_over_2 - atan(x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> acot(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'acot' only accept floating-point inputs");
		return detail::functor1<T, T, P, vecType>::call(acot, x);
	}

	// sech
	template <typename genType>
	GLM_FUNC_QUALIFIER genType sech(genType angle)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'sech' only accept floating-point values");
		return genType(1) / glm::cosh(angle);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> sech(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'sech' only accept floating-point inputs");
		return detail::functor1<T, T, P, vecType>::call(sech, x);
	}

	// csch
	template <typename genType>
	GLM_FUNC_QUALIFIER genType csch(genType angle)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'csch' only accept floating-point values");
		return genType(1) / glm::sinh(angle);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> csch(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'csch' only accept floating-point inputs");
		return detail::functor1<T, T, P, vecType>::call(csch, x);
	}

	// coth
	template <typename genType>
	GLM_FUNC_QUALIFIER genType coth(genType angle)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'coth' only accept floating-point values");
		return glm::cosh(angle) / glm::sinh(angle);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> coth(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'coth' only accept floating-point inputs");
		return detail::functor1<T, T, P, vecType>::call(coth, x);
	}

	// asech
	template <typename genType>
	GLM_FUNC_QUALIFIER genType asech(genType x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'asech' only accept floating-point values");
		return acosh(genType(1) / x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> asech(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'asech' only accept floating-point inputs");
		return detail::functor1<T, T, P, vecType>::call(asech, x);
	}

	// acsch
	template <typename genType>
	GLM_FUNC_QUALIFIER genType acsch(genType x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'acsch' only accept floating-point values");
		return acsch(genType(1) / x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> acsch(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'acsch' only accept floating-point inputs");
		return detail::functor1<T, T, P, vecType>::call(acsch, x);
	}

	// acoth
	template <typename genType>
	GLM_FUNC_QUALIFIER genType acoth(genType x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'acoth' only accept floating-point values");
		return atanh(genType(1) / x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> acoth(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'acoth' only accept floating-point inputs");
		return detail::functor1<T, T, P, vecType>::call(acoth, x);
	}
}//namespace glm
