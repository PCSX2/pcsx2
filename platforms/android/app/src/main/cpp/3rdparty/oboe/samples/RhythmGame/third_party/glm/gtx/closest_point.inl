/// @ref gtx_closest_point
/// @file glm/gtx/closest_point.inl

namespace glm
{
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> closestPointOnLine
	(
		tvec3<T, P> const & point,
		tvec3<T, P> const & a,
		tvec3<T, P> const & b
	)
	{
		T LineLength = distance(a, b);
		tvec3<T, P> Vector = point - a;
		tvec3<T, P> LineDirection = (b - a) / LineLength;

		// Project Vector to LineDirection to get the distance of point from a
		T Distance = dot(Vector, LineDirection);

		if(Distance <= T(0)) return a;
		if(Distance >= LineLength) return b;
		return a + LineDirection * Distance;
	}
	
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec2<T, P> closestPointOnLine
	(
		tvec2<T, P> const & point,
		tvec2<T, P> const & a,
		tvec2<T, P> const & b
	)
	{
		T LineLength = distance(a, b);
		tvec2<T, P> Vector = point - a;
		tvec2<T, P> LineDirection = (b - a) / LineLength;

		// Project Vector to LineDirection to get the distance of point from a
		T Distance = dot(Vector, LineDirection);

		if(Distance <= T(0)) return a;
		if(Distance >= LineLength) return b;
		return a + LineDirection * Distance;
	}
	
}//namespace glm
