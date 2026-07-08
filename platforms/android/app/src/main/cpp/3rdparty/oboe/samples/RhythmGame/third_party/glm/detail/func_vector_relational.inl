/// @ref core
/// @file glm/detail/func_vector_relational.inl

#include <limits>

namespace glm
{
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> lessThan(vecType<T, P> const & x, vecType<T, P> const & y)
	{
		assert(x.length() == y.length());

		vecType<bool, P> Result(uninitialize);
		for(length_t i = 0; i < x.length(); ++i)
			Result[i] = x[i] < y[i];

		return Result;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> lessThanEqual(vecType<T, P> const & x, vecType<T, P> const & y)
	{
		assert(x.length() == y.length());

		vecType<bool, P> Result(uninitialize);
		for(length_t i = 0; i < x.length(); ++i)
			Result[i] = x[i] <= y[i];
		return Result;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> greaterThan(vecType<T, P> const & x, vecType<T, P> const & y)
	{
		assert(x.length() == y.length());

		vecType<bool, P> Result(uninitialize);
		for(length_t i = 0; i < x.length(); ++i)
			Result[i] = x[i] > y[i];
		return Result;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> greaterThanEqual(vecType<T, P> const & x, vecType<T, P> const & y)
	{
		assert(x.length() == y.length());

		vecType<bool, P> Result(uninitialize);
		for(length_t i = 0; i < x.length(); ++i)
			Result[i] = x[i] >= y[i];
		return Result;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> equal(vecType<T, P> const & x, vecType<T, P> const & y)
	{
		assert(x.length() == y.length());

		vecType<bool, P> Result(uninitialize);
		for(length_t i = 0; i < x.length(); ++i)
			Result[i] = x[i] == y[i];
		return Result;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> notEqual(vecType<T, P> const & x, vecType<T, P> const & y)
	{
		assert(x.length() == y.length());

		vecType<bool, P> Result(uninitialize);
		for(length_t i = 0; i < x.length(); ++i)
			Result[i] = x[i] != y[i];
		return Result;
	}

	template <precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER bool any(vecType<bool, P> const & v)
	{
		bool Result = false;
		for(length_t i = 0; i < v.length(); ++i)
			Result = Result || v[i];
		return Result;
	}

	template <precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER bool all(vecType<bool, P> const & v)
	{
		bool Result = true;
		for(length_t i = 0; i < v.length(); ++i)
			Result = Result && v[i];
		return Result;
	}

	template <precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> not_(vecType<bool, P> const & v)
	{
		vecType<bool, P> Result(uninitialize);
		for(length_t i = 0; i < v.length(); ++i)
			Result[i] = !v[i];
		return Result;
	}
}//namespace glm

#if GLM_ARCH != GLM_ARCH_PURE && GLM_HAS_UNRESTRICTED_UNIONS
#	include "func_vector_relational_simd.inl"
#endif
