/// @ref core
/// @file glm/detail/_noise.hpp

#pragma once

#include "../vec2.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"
#include "../common.hpp"

namespace glm{
namespace detail
{
	template <typename T>
	GLM_FUNC_QUALIFIER T mod289(T const & x)
	{
		return x - floor(x * static_cast<T>(1.0) / static_cast<T>(289.0)) * static_cast<T>(289.0);
	}

	template <typename T>
	GLM_FUNC_QUALIFIER T permute(T const & x)
	{
		return mod289(((x * static_cast<T>(34)) + static_cast<T>(1)) * x);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec2<T, P> permute(tvec2<T, P> const & x)
	{
		return mod289(((x * static_cast<T>(34)) + static_cast<T>(1)) * x);
	}
	
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> permute(tvec3<T, P> const & x)
	{
		return mod289(((x * static_cast<T>(34)) + static_cast<T>(1)) * x);
	}
	
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec4<T, P> permute(tvec4<T, P> const & x)
	{
		return mod289(((x * static_cast<T>(34)) + static_cast<T>(1)) * x);
	}
/*
	template <typename T, precision P, template<typename> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> permute(vecType<T, P> const & x)
	{
		return mod289(((x * T(34)) + T(1)) * x);
	}
*/
	template <typename T>
	GLM_FUNC_QUALIFIER T taylorInvSqrt(T const & r)
	{
		return T(1.79284291400159) - T(0.85373472095314) * r;
	}
	
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec2<T, P> taylorInvSqrt(tvec2<T, P> const & r)
	{
		return T(1.79284291400159) - T(0.85373472095314) * r;
	}
	
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> taylorInvSqrt(tvec3<T, P> const & r)
	{
		return T(1.79284291400159) - T(0.85373472095314) * r;
	}
	
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec4<T, P> taylorInvSqrt(tvec4<T, P> const & r)
	{
		return T(1.79284291400159) - T(0.85373472095314) * r;
	}
/*
	template <typename T, precision P, template<typename> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> taylorInvSqrt(vecType<T, P> const & r)
	{
		return T(1.79284291400159) - T(0.85373472095314) * r;
	}
*/
	
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec2<T, P> fade(tvec2<T, P> const & t)
	{
		return (t * t * t) * (t * (t * T(6) - T(15)) + T(10));
	}
	
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> fade(tvec3<T, P> const & t)
	{
		return (t * t * t) * (t * (t * T(6) - T(15)) + T(10));
	}
	
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec4<T, P> fade(tvec4<T, P> const & t)
	{
		return (t * t * t) * (t * (t * T(6) - T(15)) + T(10));
	}
/*
	template <typename T, precision P, template <typename> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> fade(vecType<T, P> const & t)
	{
		return (t * t * t) * (t * (t * T(6) - T(15)) + T(10));
	}
*/
}//namespace detail
}//namespace glm

