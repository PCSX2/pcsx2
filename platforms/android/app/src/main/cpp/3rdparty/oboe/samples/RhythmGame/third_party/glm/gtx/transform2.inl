/// @ref gtx_transform2
/// @file glm/gtx/transform2.inl

namespace glm
{
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> shearX2D(tmat3x3<T, P> const& m, T s)
	{
		tmat3x3<T, P> r(1);
		r[1][0] = s;
		return m * r;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> shearY2D(tmat3x3<T, P> const& m, T s)
	{
		tmat3x3<T, P> r(1);
		r[0][1] = s;
		return m * r;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat4x4<T, P> shearX3D(tmat4x4<T, P> const& m, T s, T t)
	{
		tmat4x4<T, P> r(1);
		r[0][1] = s;
		r[0][2] = t;
		return m * r;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat4x4<T, P> shearY3D(tmat4x4<T, P> const& m, T s, T t)
	{
		tmat4x4<T, P> r(1);
		r[1][0] = s;
		r[1][2] = t;
		return m * r;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat4x4<T, P> shearZ3D(tmat4x4<T, P> const& m, T s, T t)
	{
		tmat4x4<T, P> r(1);
		r[2][0] = s;
		r[2][1] = t;
		return m * r;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> reflect2D(tmat3x3<T, P> const& m, tvec3<T, P> const& normal)
	{
		tmat3x3<T, P> r(static_cast<T>(1));
		r[0][0] = static_cast<T>(1) - static_cast<T>(2) * normal.x * normal.x;
		r[0][1] = -static_cast<T>(2) * normal.x * normal.y;
		r[1][0] = -static_cast<T>(2) * normal.x * normal.y;
		r[1][1] = static_cast<T>(1) - static_cast<T>(2) * normal.y * normal.y;
		return m * r;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat4x4<T, P> reflect3D(tmat4x4<T, P> const& m, tvec3<T, P> const& normal)
	{
		tmat4x4<T, P> r(static_cast<T>(1));
		r[0][0] = static_cast<T>(1) - static_cast<T>(2) * normal.x * normal.x;
		r[0][1] = -static_cast<T>(2) * normal.x * normal.y;
		r[0][2] = -static_cast<T>(2) * normal.x * normal.z;

		r[1][0] = -static_cast<T>(2) * normal.x * normal.y;
		r[1][1] = static_cast<T>(1) - static_cast<T>(2) * normal.y * normal.y;
		r[1][2] = -static_cast<T>(2) * normal.y * normal.z;

		r[2][0] = -static_cast<T>(2) * normal.x * normal.z;
		r[2][1] = -static_cast<T>(2) * normal.y * normal.z;
		r[2][2] = static_cast<T>(1) - static_cast<T>(2) * normal.z * normal.z;
		return m * r;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> proj2D(
		const tmat3x3<T, P>& m, 
		const tvec3<T, P>& normal)
	{
		tmat3x3<T, P> r(static_cast<T>(1));
		r[0][0] = static_cast<T>(1) - normal.x * normal.x;
		r[0][1] = - normal.x * normal.y;
		r[1][0] = - normal.x * normal.y;
		r[1][1] = static_cast<T>(1) - normal.y * normal.y;
		return m * r;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat4x4<T, P> proj3D(
		const tmat4x4<T, P>& m, 
		const tvec3<T, P>& normal)
	{
		tmat4x4<T, P> r(static_cast<T>(1));
		r[0][0] = static_cast<T>(1) - normal.x * normal.x;
		r[0][1] = - normal.x * normal.y;
		r[0][2] = - normal.x * normal.z;
		r[1][0] = - normal.x * normal.y;
		r[1][1] = static_cast<T>(1) - normal.y * normal.y;
		r[1][2] = - normal.y * normal.z;
		r[2][0] = - normal.x * normal.z;
		r[2][1] = - normal.y * normal.z;
		r[2][2] = static_cast<T>(1) - normal.z * normal.z;
		return m * r;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat4x4<T, P> scaleBias(T scale, T bias)
	{
		tmat4x4<T, P> result;
		result[3] = tvec4<T, P>(tvec3<T, P>(bias), static_cast<T>(1));
		result[0][0] = scale;
		result[1][1] = scale;
		result[2][2] = scale;
		return result;
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat4x4<T, P> scaleBias(tmat4x4<T, P> const& m, T scale, T bias)
	{
		return m * scaleBias(scale, bias);
	}
}//namespace glm

