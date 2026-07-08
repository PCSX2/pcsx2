/// @ref gtx_matrix_major_storage
/// @file glm/gtx/matrix_major_storage.hpp

namespace glm
{
	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat2x2<T, P> rowMajor2
	(
		tvec2<T, P> const & v1, 
		tvec2<T, P> const & v2
	)
	{
		tmat2x2<T, P> Result;
		Result[0][0] = v1.x;
		Result[1][0] = v1.y;
		Result[0][1] = v2.x;
		Result[1][1] = v2.y;
		return Result;
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat2x2<T, P> rowMajor2(
		const tmat2x2<T, P>& m)
	{
		tmat2x2<T, P> Result;
		Result[0][0] = m[0][0];
		Result[0][1] = m[1][0];
		Result[1][0] = m[0][1];
		Result[1][1] = m[1][1];
		return Result;
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> rowMajor3(
		const tvec3<T, P>& v1, 
		const tvec3<T, P>& v2, 
		const tvec3<T, P>& v3)
	{
		tmat3x3<T, P> Result;
		Result[0][0] = v1.x;
		Result[1][0] = v1.y;
		Result[2][0] = v1.z;
		Result[0][1] = v2.x;
		Result[1][1] = v2.y;
		Result[2][1] = v2.z;
		Result[0][2] = v3.x;
		Result[1][2] = v3.y;
		Result[2][2] = v3.z;
		return Result;
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> rowMajor3(
		const tmat3x3<T, P>& m)
	{
		tmat3x3<T, P> Result;
		Result[0][0] = m[0][0];
		Result[0][1] = m[1][0];
		Result[0][2] = m[2][0];
		Result[1][0] = m[0][1];
		Result[1][1] = m[1][1];
		Result[1][2] = m[2][1];
		Result[2][0] = m[0][2];
		Result[2][1] = m[1][2];
		Result[2][2] = m[2][2];
		return Result;
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat4x4<T, P> rowMajor4(
		const tvec4<T, P>& v1, 
		const tvec4<T, P>& v2, 
		const tvec4<T, P>& v3, 
		const tvec4<T, P>& v4)
	{
		tmat4x4<T, P> Result;
		Result[0][0] = v1.x;
		Result[1][0] = v1.y;
		Result[2][0] = v1.z;
		Result[3][0] = v1.w;
		Result[0][1] = v2.x;
		Result[1][1] = v2.y;
		Result[2][1] = v2.z;
		Result[3][1] = v2.w;
		Result[0][2] = v3.x;
		Result[1][2] = v3.y;
		Result[2][2] = v3.z;
		Result[3][2] = v3.w;
		Result[0][3] = v4.x;
		Result[1][3] = v4.y;
		Result[2][3] = v4.z;
		Result[3][3] = v4.w;
		return Result;
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat4x4<T, P> rowMajor4(
		const tmat4x4<T, P>& m)
	{
		tmat4x4<T, P> Result;
		Result[0][0] = m[0][0];
		Result[0][1] = m[1][0];
		Result[0][2] = m[2][0];
		Result[0][3] = m[3][0];
		Result[1][0] = m[0][1];
		Result[1][1] = m[1][1];
		Result[1][2] = m[2][1];
		Result[1][3] = m[3][1];
		Result[2][0] = m[0][2];
		Result[2][1] = m[1][2];
		Result[2][2] = m[2][2];
		Result[2][3] = m[3][2];
		Result[3][0] = m[0][3];
		Result[3][1] = m[1][3];
		Result[3][2] = m[2][3];
		Result[3][3] = m[3][3];
		return Result;
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat2x2<T, P> colMajor2(
		const tvec2<T, P>& v1, 
		const tvec2<T, P>& v2)
	{
		return tmat2x2<T, P>(v1, v2);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat2x2<T, P> colMajor2(
		const tmat2x2<T, P>& m)
	{
		return tmat2x2<T, P>(m);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> colMajor3(
		const tvec3<T, P>& v1, 
		const tvec3<T, P>& v2, 
		const tvec3<T, P>& v3)
	{
		return tmat3x3<T, P>(v1, v2, v3);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> colMajor3(
		const tmat3x3<T, P>& m)
	{
		return tmat3x3<T, P>(m);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat4x4<T, P> colMajor4(
		const tvec4<T, P>& v1, 
		const tvec4<T, P>& v2, 
		const tvec4<T, P>& v3, 
		const tvec4<T, P>& v4)
	{
		return tmat4x4<T, P>(v1, v2, v3, v4);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat4x4<T, P> colMajor4(
		const tmat4x4<T, P>& m)
	{
		return tmat4x4<T, P>(m);
	}
}//namespace glm
