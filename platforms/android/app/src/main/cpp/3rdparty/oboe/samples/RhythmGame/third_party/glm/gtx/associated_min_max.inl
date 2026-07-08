/// @ref gtx_associated_min_max
/// @file glm/gtx/associated_min_max.inl

namespace glm{

// Min comparison between 2 variables
template<typename T, typename U, precision P>
GLM_FUNC_QUALIFIER U associatedMin(T x, U a, T y, U b)
{
	return x < y ? a : b;
}

template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER tvec2<U, P> associatedMin
(
	vecType<T, P> const & x, vecType<U, P> const & a,
	vecType<T, P> const & y, vecType<U, P> const & b
)
{
	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
		Result[i] = x[i] < y[i] ? a[i] : b[i];
	return Result;
}

template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<U, P> associatedMin
(
	T x, const vecType<U, P>& a,
	T y, const vecType<U, P>& b
)
{
	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
		Result[i] = x < y ? a[i] : b[i];
	return Result;
}

template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<U, P> associatedMin
(
	vecType<T, P> const & x, U a,
	vecType<T, P> const & y, U b
)
{
	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
		Result[i] = x[i] < y[i] ? a : b;
	return Result;
}

// Min comparison between 3 variables
template<typename T, typename U>
GLM_FUNC_QUALIFIER U associatedMin
(
	T x, U a,
	T y, U b,
	T z, U c
)
{
	U Result = x < y ? (x < z ? a : c) : (y < z ? b : c);
	return Result;
}

template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<U, P> associatedMin
(
	vecType<T, P> const & x, vecType<U, P> const & a,
	vecType<T, P> const & y, vecType<U, P> const & b,
	vecType<T, P> const & z, vecType<U, P> const & c
)
{
	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
		Result[i] = x[i] < y[i] ? (x[i] < z[i] ? a[i] : c[i]) : (y[i] < z[i] ? b[i] : c[i]);
	return Result;
}

// Min comparison between 4 variables
template<typename T, typename U>
GLM_FUNC_QUALIFIER U associatedMin
(
	T x, U a,
	T y, U b,
	T z, U c,
	T w, U d
)
{
	T Test1 = min(x, y);
	T Test2 = min(z, w);;
	U Result1 = x < y ? a : b;
	U Result2 = z < w ? c : d;
	U Result = Test1 < Test2 ? Result1 : Result2;
	return Result;
}

// Min comparison between 4 variables
template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<U, P> associatedMin
(
	vecType<T, P> const & x, vecType<U, P> const & a,
	vecType<T, P> const & y, vecType<U, P> const & b,
	vecType<T, P> const & z, vecType<U, P> const & c,
	vecType<T, P> const & w, vecType<U, P> const & d
)
{
	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
	{
		T Test1 = min(x[i], y[i]);
		T Test2 = min(z[i], w[i]);
		U Result1 = x[i] < y[i] ? a[i] : b[i];
		U Result2 = z[i] < w[i] ? c[i] : d[i];
		Result[i] = Test1 < Test2 ? Result1 : Result2;
	}
	return Result;
}

// Min comparison between 4 variables
template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<U, P> associatedMin
(
	T x, vecType<U, P> const & a,
	T y, vecType<U, P> const & b,
	T z, vecType<U, P> const & c,
	T w, vecType<U, P> const & d
)
{
	T Test1 = min(x, y);
	T Test2 = min(z, w);

	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
	{
		U Result1 = x < y ? a[i] : b[i];
		U Result2 = z < w ? c[i] : d[i];
		Result[i] = Test1 < Test2 ? Result1 : Result2;
	}
	return Result;
}

// Min comparison between 4 variables
template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<U, P> associatedMin
(
	vecType<T, P> const & x, U a,
	vecType<T, P> const & y, U b,
	vecType<T, P> const & z, U c,
	vecType<T, P> const & w, U d
)
{
	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
	{
		T Test1 = min(x[i], y[i]);
		T Test2 = min(z[i], w[i]);;
		U Result1 = x[i] < y[i] ? a : b;
		U Result2 = z[i] < w[i] ? c : d;
		Result[i] = Test1 < Test2 ? Result1 : Result2;
	}
	return Result;
}

// Max comparison between 2 variables
template<typename T, typename U>
GLM_FUNC_QUALIFIER U associatedMax(T x, U a, T y, U b)
{
	return x > y ? a : b;
}

// Max comparison between 2 variables
template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER tvec2<U, P> associatedMax
(
	vecType<T, P> const & x, vecType<U, P> const & a,
	vecType<T, P> const & y, vecType<U, P> const & b
)
{
	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
		Result[i] = x[i] > y[i] ? a[i] : b[i];
	return Result;
}

// Max comparison between 2 variables
template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<T, P> associatedMax
(
	T x, vecType<U, P> const & a,
	T y, vecType<U, P> const & b
)
{
	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
		Result[i] = x > y ? a[i] : b[i];
	return Result;
}

// Max comparison between 2 variables
template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<U, P> associatedMax
(
	vecType<T, P> const & x, U a,
	vecType<T, P> const & y, U b
)
{
	vecType<T, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
		Result[i] = x[i] > y[i] ? a : b;
	return Result;
}

// Max comparison between 3 variables
template<typename T, typename U>
GLM_FUNC_QUALIFIER U associatedMax
(
	T x, U a,
	T y, U b,
	T z, U c
)
{
	U Result = x > y ? (x > z ? a : c) : (y > z ? b : c);
	return Result;
}

// Max comparison between 3 variables
template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<U, P> associatedMax
(
	vecType<T, P> const & x, vecType<U, P> const & a,
	vecType<T, P> const & y, vecType<U, P> const & b,
	vecType<T, P> const & z, vecType<U, P> const & c
)
{
	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
		Result[i] = x[i] > y[i] ? (x[i] > z[i] ? a[i] : c[i]) : (y[i] > z[i] ? b[i] : c[i]);
	return Result;
}

// Max comparison between 3 variables
template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<T, P> associatedMax
(
	T x, vecType<U, P> const & a,
	T y, vecType<U, P> const & b,
	T z, vecType<U, P> const & c
)
{
	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
		Result[i] = x > y ? (x > z ? a[i] : c[i]) : (y > z ? b[i] : c[i]);
	return Result;
}

// Max comparison between 3 variables
template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<U, P> associatedMax
(
	vecType<T, P> const & x, U a,
	vecType<T, P> const & y, U b,
	vecType<T, P> const & z, U c
)
{
	vecType<T, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
		Result[i] = x[i] > y[i] ? (x[i] > z[i] ? a : c) : (y[i] > z[i] ? b : c);
	return Result;
}

// Max comparison between 4 variables
template<typename T, typename U>
GLM_FUNC_QUALIFIER U associatedMax
(
	T x, U a,
	T y, U b,
	T z, U c,
	T w, U d
)
{
	T Test1 = max(x, y);
	T Test2 = max(z, w);;
	U Result1 = x > y ? a : b;
	U Result2 = z > w ? c : d;
	U Result = Test1 > Test2 ? Result1 : Result2;
	return Result;
}

// Max comparison between 4 variables
template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<U, P> associatedMax
(
	vecType<T, P> const & x, vecType<U, P> const & a,
	vecType<T, P> const & y, vecType<U, P> const & b,
	vecType<T, P> const & z, vecType<U, P> const & c,
	vecType<T, P> const & w, vecType<U, P> const & d
)
{
	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
	{
		T Test1 = max(x[i], y[i]);
		T Test2 = max(z[i], w[i]);
		U Result1 = x[i] > y[i] ? a[i] : b[i];
		U Result2 = z[i] > w[i] ? c[i] : d[i];
		Result[i] = Test1 > Test2 ? Result1 : Result2;
	}
	return Result;
}

// Max comparison between 4 variables
template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<U, P> associatedMax
(
	T x, vecType<U, P> const & a,
	T y, vecType<U, P> const & b,
	T z, vecType<U, P> const & c,
	T w, vecType<U, P> const & d
)
{
	T Test1 = max(x, y);
	T Test2 = max(z, w);

	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
	{
		U Result1 = x > y ? a[i] : b[i];
		U Result2 = z > w ? c[i] : d[i];
		Result[i] = Test1 > Test2 ? Result1 : Result2;
	}
	return Result;
}

// Max comparison between 4 variables
template<typename T, typename U, precision P, template <typename, precision> class vecType>
GLM_FUNC_QUALIFIER vecType<U, P> associatedMax
(
	vecType<T, P> const & x, U a,
	vecType<T, P> const & y, U b,
	vecType<T, P> const & z, U c,
	vecType<T, P> const & w, U d
)
{
	vecType<U, P> Result(uninitialize);
	for(length_t i = 0, n = Result.length(); i < n; ++i)
	{
		T Test1 = max(x[i], y[i]);
		T Test2 = max(z[i], w[i]);;
		U Result1 = x[i] > y[i] ? a : b;
		U Result2 = z[i] > w[i] ? c : d;
		Result[i] = Test1 > Test2 ? Result1 : Result2;
	}
	return Result;
}
}//namespace glm
