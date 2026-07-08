/// @ref gtx_matrix_transform_2d
/// @file glm/gtc/matrix_transform_2d.inl
/// @author Miguel Ángel Pérez Martínez

#include "../trigonometric.hpp"

namespace glm
{
	
	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> translate(
		tmat3x3<T, P> const & m,
		tvec2<T, P> const & v)
	{
		tmat3x3<T, P> Result(m);
		Result[2] = m[0] * v[0] + m[1] * v[1] + m[2];
		return Result;
	}


	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> rotate(
		tmat3x3<T, P> const & m,
		T angle)
	{
		T const a = angle;
		T const c = cos(a);
		T const s = sin(a);

		tmat3x3<T, P> Result(uninitialize);
		Result[0] = m[0] * c + m[1] * s;
		Result[1] = m[0] * -s + m[1] * c;
		Result[2] = m[2];
		return Result;
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> scale(
		tmat3x3<T, P> const & m,
		tvec2<T, P> const & v)
	{
		tmat3x3<T, P> Result(uninitialize);
		Result[0] = m[0] * v[0];
		Result[1] = m[1] * v[1];
		Result[2] = m[2];
		return Result;
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> shearX(
		tmat3x3<T, P> const & m,
		T y)
	{
		tmat3x3<T, P> Result(1);
		Result[0][1] = y;
		return m * Result;
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> shearY(
		tmat3x3<T, P> const & m,
		T x)
	{
		tmat3x3<T, P> Result(1);
		Result[1][0] = x;
		return m * Result;
	}

}//namespace glm
