/// @ref gtx_orthonormalize
/// @file glm/gtx/orthonormalize.inl

namespace glm
{
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> orthonormalize(tmat3x3<T, P> const & m)
	{
		tmat3x3<T, P> r = m;

		r[0] = normalize(r[0]);

		T d0 = dot(r[0], r[1]);
		r[1] -= r[0] * d0;
		r[1] = normalize(r[1]);

		T d1 = dot(r[1], r[2]);
		d0 = dot(r[0], r[2]);
		r[2] -= r[0] * d0 + r[1] * d1;
		r[2] = normalize(r[2]);

		return r;
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tvec3<T, P> orthonormalize(tvec3<T, P> const & x, tvec3<T, P> const & y)
	{
		return normalize(x - y * dot(y, x));
	}
}//namespace glm
