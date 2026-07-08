/// @ref gtx_fast_trigonometry
/// @file glm/gtx/fast_trigonometry.inl

namespace glm{
namespace detail
{
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> taylorCos(vecType<T, P> const & x)
	{
		return static_cast<T>(1)
			- (x * x) / 2.f
			+ (x * x * x * x) / 24.f
			- (x * x * x * x * x * x) / 720.f
			+ (x * x * x * x * x * x * x * x) / 40320.f;
	}

	template <typename T>
	GLM_FUNC_QUALIFIER T cos_52s(T x)
	{
		T const xx(x * x);
		return (T(0.9999932946) + xx * (T(-0.4999124376) + xx * (T(0.0414877472) + xx * T(-0.0012712095))));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> cos_52s(vecType<T, P> const & x)
	{
		return detail::functor1<T, T, P, vecType>::call(cos_52s, x);
	}
}//namespace detail

	// wrapAngle
	template <typename T>
	GLM_FUNC_QUALIFIER T wrapAngle(T angle)
	{
		return abs<T>(mod<T>(angle, two_pi<T>()));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> wrapAngle(vecType<T, P> const & x)
	{
		return detail::functor1<T, T, P, vecType>::call(wrapAngle, x);
	}

	// cos
	template <typename T> 
	GLM_FUNC_QUALIFIER T fastCos(T x)
	{
		T const angle(wrapAngle<T>(x));

		if(angle < half_pi<T>())
			return detail::cos_52s(angle);
		if(angle < pi<T>())
			return -detail::cos_52s(pi<T>() - angle);
		if(angle < (T(3) * half_pi<T>()))
			return -detail::cos_52s(angle - pi<T>());

		return detail::cos_52s(two_pi<T>() - angle);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> fastCos(vecType<T, P> const & x)
	{
		return detail::functor1<T, T, P, vecType>::call(fastCos, x);
	}

	// sin
	template <typename T> 
	GLM_FUNC_QUALIFIER T fastSin(T x)
	{
		return fastCos<T>(half_pi<T>() - x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> fastSin(vecType<T, P> const & x)
	{
		return detail::functor1<T, T, P, vecType>::call(fastSin, x);
	}

	// tan
	template <typename T> 
	GLM_FUNC_QUALIFIER T fastTan(T x)
	{
		return x + (x * x * x * T(0.3333333333)) + (x * x * x * x * x * T(0.1333333333333)) + (x * x * x * x * x * x * x * T(0.0539682539));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> fastTan(vecType<T, P> const & x)
	{
		return detail::functor1<T, T, P, vecType>::call(fastTan, x);
	}

	// asin
	template <typename T> 
	GLM_FUNC_QUALIFIER T fastAsin(T x)
	{
		return x + (x * x * x * T(0.166666667)) + (x * x * x * x * x * T(0.075)) + (x * x * x * x * x * x * x * T(0.0446428571)) + (x * x * x * x * x * x * x * x * x * T(0.0303819444));// + (x * x * x * x * x * x * x * x * x * x * x * T(0.022372159));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> fastAsin(vecType<T, P> const & x)
	{
		return detail::functor1<T, T, P, vecType>::call(fastAsin, x);
	}

	// acos
	template <typename T> 
	GLM_FUNC_QUALIFIER T fastAcos(T x)
	{
		return T(1.5707963267948966192313216916398) - fastAsin(x); //(PI / 2)
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> fastAcos(vecType<T, P> const & x)
	{
		return detail::functor1<T, T, P, vecType>::call(fastAcos, x);
	}

	// atan
	template <typename T> 
	GLM_FUNC_QUALIFIER T fastAtan(T y, T x)
	{
		T sgn = sign(y) * sign(x);
		return abs(fastAtan(y / x)) * sgn;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> fastAtan(vecType<T, P> const & y, vecType<T, P> const & x)
	{
		return detail::functor2<T, P, vecType>::call(fastAtan, y, x);
	}

	template <typename T> 
	GLM_FUNC_QUALIFIER T fastAtan(T x)
	{
		return x - (x * x * x * T(0.333333333333)) + (x * x * x * x * x * T(0.2)) - (x * x * x * x * x * x * x * T(0.1428571429)) + (x * x * x * x * x * x * x * x * x * T(0.111111111111)) - (x * x * x * x * x * x * x * x * x * x * x * T(0.0909090909));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> fastAtan(vecType<T, P> const & x)
	{
		return detail::functor1<T, T, P, vecType>::call(fastAtan, x);
	}
}//namespace glm
