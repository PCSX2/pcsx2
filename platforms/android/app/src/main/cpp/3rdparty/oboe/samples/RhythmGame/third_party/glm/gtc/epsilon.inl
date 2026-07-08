/// @ref gtc_epsilon
/// @file glm/gtc/epsilon.inl

// Dependency:
#include "quaternion.hpp"
#include "../vector_relational.hpp"
#include "../common.hpp"
#include "../vec2.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"

namespace glm
{
	template <>
	GLM_FUNC_QUALIFIER bool epsilonEqual
	(
		float const & x,
		float const & y,
		float const & epsilon
	)
	{
		return abs(x - y) < epsilon;
	}

	template <>
	GLM_FUNC_QUALIFIER bool epsilonEqual
	(
		double const & x,
		double const & y,
		double const & epsilon
	)
	{
		return abs(x - y) < epsilon;
	}

	template <>
	GLM_FUNC_QUALIFIER bool epsilonNotEqual
	(
		float const & x,
		float const & y,
		float const & epsilon
	)
	{
		return abs(x - y) >= epsilon;
	}

	template <>
	GLM_FUNC_QUALIFIER bool epsilonNotEqual
	(
		double const & x,
		double const & y,
		double const & epsilon
	)
	{
		return abs(x - y) >= epsilon;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> epsilonEqual
	(
		vecType<T, P> const & x,
		vecType<T, P> const & y,
		T const & epsilon
	)
	{
		return lessThan(abs(x - y), vecType<T, P>(epsilon));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> epsilonEqual
	(
		vecType<T, P> const & x,
		vecType<T, P> const & y,
		vecType<T, P> const & epsilon
	)
	{
		return lessThan(abs(x - y), vecType<T, P>(epsilon));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> epsilonNotEqual
	(
		vecType<T, P> const & x,
		vecType<T, P> const & y,
		T const & epsilon
	)
	{
		return greaterThanEqual(abs(x - y), vecType<T, P>(epsilon));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> epsilonNotEqual
	(
		vecType<T, P> const & x,
		vecType<T, P> const & y,
		vecType<T, P> const & epsilon
	)
	{
		return greaterThanEqual(abs(x - y), vecType<T, P>(epsilon));
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec4<bool, P> epsilonEqual
	(
		tquat<T, P> const & x,
		tquat<T, P> const & y,
		T const & epsilon
	)
	{
		tvec4<T, P> v(x.x - y.x, x.y - y.y, x.z - y.z, x.w - y.w);
		return lessThan(abs(v), tvec4<T, P>(epsilon));
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec4<bool, P> epsilonNotEqual
	(
		tquat<T, P> const & x,
		tquat<T, P> const & y,
		T const & epsilon
	)
	{
		tvec4<T, P> v(x.x - y.x, x.y - y.y, x.z - y.z, x.w - y.w);
		return greaterThanEqual(abs(v), tvec4<T, P>(epsilon));
	}
}//namespace glm
