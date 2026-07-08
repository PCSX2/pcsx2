/// @ref core
/// @file glm/detail/_vectorize.hpp

#pragma once

#include "type_vec1.hpp"
#include "type_vec2.hpp"
#include "type_vec3.hpp"
#include "type_vec4.hpp"

namespace glm{
namespace detail
{
	template <typename R, typename T, precision P, template <typename, precision> class vecType>
	struct functor1{};

	template <typename R, typename T, precision P>
	struct functor1<R, T, P, tvec1>
	{
		GLM_FUNC_QUALIFIER static tvec1<R, P> call(R (*Func) (T x), tvec1<T, P> const & v)
		{
			return tvec1<R, P>(Func(v.x));
		}
	};

	template <typename R, typename T, precision P>
	struct functor1<R, T, P, tvec2>
	{
		GLM_FUNC_QUALIFIER static tvec2<R, P> call(R (*Func) (T x), tvec2<T, P> const & v)
		{
			return tvec2<R, P>(Func(v.x), Func(v.y));
		}
	};

	template <typename R, typename T, precision P>
	struct functor1<R, T, P, tvec3>
	{
		GLM_FUNC_QUALIFIER static tvec3<R, P> call(R (*Func) (T x), tvec3<T, P> const & v)
		{
			return tvec3<R, P>(Func(v.x), Func(v.y), Func(v.z));
		}
	};

	template <typename R, typename T, precision P>
	struct functor1<R, T, P, tvec4>
	{
		GLM_FUNC_QUALIFIER static tvec4<R, P> call(R (*Func) (T x), tvec4<T, P> const & v)
		{
			return tvec4<R, P>(Func(v.x), Func(v.y), Func(v.z), Func(v.w));
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType>
	struct functor2{};

	template <typename T, precision P>
	struct functor2<T, P, tvec1>
	{
		GLM_FUNC_QUALIFIER static tvec1<T, P> call(T (*Func) (T x, T y), tvec1<T, P> const & a, tvec1<T, P> const & b)
		{
			return tvec1<T, P>(Func(a.x, b.x));
		}
	};

	template <typename T, precision P>
	struct functor2<T, P, tvec2>
	{
		GLM_FUNC_QUALIFIER static tvec2<T, P> call(T (*Func) (T x, T y), tvec2<T, P> const & a, tvec2<T, P> const & b)
		{
			return tvec2<T, P>(Func(a.x, b.x), Func(a.y, b.y));
		}
	};

	template <typename T, precision P>
	struct functor2<T, P, tvec3>
	{
		GLM_FUNC_QUALIFIER static tvec3<T, P> call(T (*Func) (T x, T y), tvec3<T, P> const & a, tvec3<T, P> const & b)
		{
			return tvec3<T, P>(Func(a.x, b.x), Func(a.y, b.y), Func(a.z, b.z));
		}
	};

	template <typename T, precision P>
	struct functor2<T, P, tvec4>
	{
		GLM_FUNC_QUALIFIER static tvec4<T, P> call(T (*Func) (T x, T y), tvec4<T, P> const & a, tvec4<T, P> const & b)
		{
			return tvec4<T, P>(Func(a.x, b.x), Func(a.y, b.y), Func(a.z, b.z), Func(a.w, b.w));
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType>
	struct functor2_vec_sca{};

	template <typename T, precision P>
	struct functor2_vec_sca<T, P, tvec1>
	{
		GLM_FUNC_QUALIFIER static tvec1<T, P> call(T (*Func) (T x, T y), tvec1<T, P> const & a, T b)
		{
			return tvec1<T, P>(Func(a.x, b));
		}
	};

	template <typename T, precision P>
	struct functor2_vec_sca<T, P, tvec2>
	{
		GLM_FUNC_QUALIFIER static tvec2<T, P> call(T (*Func) (T x, T y), tvec2<T, P> const & a, T b)
		{
			return tvec2<T, P>(Func(a.x, b), Func(a.y, b));
		}
	};

	template <typename T, precision P>
	struct functor2_vec_sca<T, P, tvec3>
	{
		GLM_FUNC_QUALIFIER static tvec3<T, P> call(T (*Func) (T x, T y), tvec3<T, P> const & a, T b)
		{
			return tvec3<T, P>(Func(a.x, b), Func(a.y, b), Func(a.z, b));
		}
	};

	template <typename T, precision P>
	struct functor2_vec_sca<T, P, tvec4>
	{
		GLM_FUNC_QUALIFIER static tvec4<T, P> call(T (*Func) (T x, T y), tvec4<T, P> const & a, T b)
		{
			return tvec4<T, P>(Func(a.x, b), Func(a.y, b), Func(a.z, b), Func(a.w, b));
		}
	};
}//namespace detail
}//namespace glm
