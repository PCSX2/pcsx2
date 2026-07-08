/// @ref gtx_matrix_cross_product
/// @file glm/gtx/matrix_cross_product.inl

namespace glm
{
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> matrixCross3
	(
		tvec3<T, P> const & x
	)
	{
		tmat3x3<T, P> Result(T(0));
		Result[0][1] = x.z;
		Result[1][0] = -x.z;
		Result[0][2] = -x.y;
		Result[2][0] = x.y;
		Result[1][2] = x.x;
		Result[2][1] = -x.x;
		return Result;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat4x4<T, P> matrixCross4
	(
		tvec3<T, P> const & x
	)
	{
		tmat4x4<T, P> Result(T(0));
		Result[0][1] = x.z;
		Result[1][0] = -x.z;
		Result[0][2] = -x.y;
		Result[2][0] = x.y;
		Result[1][2] = x.x;
		Result[2][1] = -x.x;
		return Result;
	}

}//namespace glm
