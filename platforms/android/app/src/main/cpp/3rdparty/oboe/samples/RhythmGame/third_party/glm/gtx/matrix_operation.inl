/// @ref gtx_matrix_operation
/// @file glm/gtx/matrix_operation.inl

namespace glm
{
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat2x2<T, P> diagonal2x2
	(
		tvec2<T, P> const & v
	)
	{
		tmat2x2<T, P> Result(static_cast<T>(1));
		Result[0][0] = v[0];
		Result[1][1] = v[1];
		return Result;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat2x3<T, P> diagonal2x3
	(
		tvec2<T, P> const & v
	)
	{
		tmat2x3<T, P> Result(static_cast<T>(1));
		Result[0][0] = v[0];
		Result[1][1] = v[1];
		return Result;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat2x4<T, P> diagonal2x4
	(
		tvec2<T, P> const & v
	)
	{
		tmat2x4<T, P> Result(static_cast<T>(1));
		Result[0][0] = v[0];
		Result[1][1] = v[1];
		return Result;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x2<T, P> diagonal3x2
	(
		tvec2<T, P> const & v
	)
	{
		tmat3x2<T, P> Result(static_cast<T>(1));
		Result[0][0] = v[0];
		Result[1][1] = v[1];
		return Result;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> diagonal3x3
	(
		tvec3<T, P> const & v
	)
	{
		tmat3x3<T, P> Result(static_cast<T>(1));
		Result[0][0] = v[0];
		Result[1][1] = v[1];
		Result[2][2] = v[2];
		return Result;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x4<T, P> diagonal3x4
	(
		tvec3<T, P> const & v
	)
	{
		tmat3x4<T, P> Result(static_cast<T>(1));
		Result[0][0] = v[0];
		Result[1][1] = v[1];
		Result[2][2] = v[2];
		return Result;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat4x4<T, P> diagonal4x4
	(
		tvec4<T, P> const & v
	)
	{
		tmat4x4<T, P> Result(static_cast<T>(1));
		Result[0][0] = v[0];
		Result[1][1] = v[1];
		Result[2][2] = v[2];
		Result[3][3] = v[3];
		return Result;		
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat4x3<T, P> diagonal4x3
	(
		tvec3<T, P> const & v
	)
	{
		tmat4x3<T, P> Result(static_cast<T>(1));
		Result[0][0] = v[0];
		Result[1][1] = v[1];
		Result[2][2] = v[2];
		return Result;		
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat4x2<T, P> diagonal4x2
	(
		tvec2<T, P> const & v
	)
	{
		tmat4x2<T, P> Result(static_cast<T>(1));
		Result[0][0] = v[0];
		Result[1][1] = v[1];
		return Result;		
	}
}//namespace glm
