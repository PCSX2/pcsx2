/// @ref core
/// @file glm/detail/func_common.inl

#include "func_vector_relational.hpp"
#include "type_vec2.hpp"
#include "type_vec3.hpp"
#include "type_vec4.hpp"
#include "_vectorize.hpp"
#include <limits>

namespace glm
{
	// min
	template <typename genType>
	GLM_FUNC_QUALIFIER genType min(genType x, genType y)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559 || std::numeric_limits<genType>::is_integer || GLM_UNRESTRICTED_GENTYPE, "'min' only accept floating-point or integer inputs");
		return x < y ? x : y;
	}

	// max
	template <typename genType>
	GLM_FUNC_QUALIFIER genType max(genType x, genType y)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559 || std::numeric_limits<genType>::is_integer || GLM_UNRESTRICTED_GENTYPE, "'max' only accept floating-point or integer inputs");

		return x > y ? x : y;
	}

	// abs
	template <>
	GLM_FUNC_QUALIFIER int32 abs(int32 x)
	{
		int32 const y = x >> 31;
		return (x ^ y) - y;
	}

	// round
#	if GLM_HAS_CXX11_STL
		using ::std::round;
#	else
		template <typename genType>
		GLM_FUNC_QUALIFIER genType round(genType x)
		{
			GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'round' only accept floating-point inputs");

			return x < static_cast<genType>(0) ? static_cast<genType>(int(x - static_cast<genType>(0.5))) : static_cast<genType>(int(x + static_cast<genType>(0.5)));
		}
#	endif

	// trunc
#	if GLM_HAS_CXX11_STL
		using ::std::trunc;
#	else
		template <typename genType>
		GLM_FUNC_QUALIFIER genType trunc(genType x)
		{
			GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'trunc' only accept floating-point inputs");

			return x < static_cast<genType>(0) ? -std::floor(-x) : std::floor(x);
		}
#	endif

}//namespace glm

namespace glm{
namespace detail
{
	template <typename genFIType, bool /*signed*/>
	struct compute_abs
	{};

	template <typename genFIType>
	struct compute_abs<genFIType, true>
	{
		GLM_FUNC_QUALIFIER static genFIType call(genFIType x)
		{
			GLM_STATIC_ASSERT(
				std::numeric_limits<genFIType>::is_iec559 || std::numeric_limits<genFIType>::is_signed || GLM_UNRESTRICTED_GENTYPE,
				"'abs' only accept floating-point and integer scalar or vector inputs");

			return x >= genFIType(0) ? x : -x;
			// TODO, perf comp with: *(((int *) &x) + 1) &= 0x7fffffff;
		}
	};

	#if GLM_COMPILER & GLM_COMPILER_CUDA
	template <>
	struct compute_abs<float, true>
	{
		GLM_FUNC_QUALIFIER static float call(float x)
		{
			return fabsf(x);
		}
	};
	#endif

	template <typename genFIType>
	struct compute_abs<genFIType, false>
	{
		GLM_FUNC_QUALIFIER static genFIType call(genFIType x)
		{
			GLM_STATIC_ASSERT(
				(!std::numeric_limits<genFIType>::is_signed && std::numeric_limits<genFIType>::is_integer) || GLM_UNRESTRICTED_GENTYPE,
				"'abs' only accept floating-point and integer scalar or vector inputs");
			return x;
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_abs_vector
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x)
		{
			return detail::functor1<T, T, P, vecType>::call(abs, x);
		}
	};

	template <typename T, typename U, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_mix_vector
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x, vecType<T, P> const & y, vecType<U, P> const & a)
		{
			GLM_STATIC_ASSERT(std::numeric_limits<U>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'mix' only accept floating-point inputs for the interpolator a");

			return vecType<T, P>(vecType<U, P>(x) + a * vecType<U, P>(y - x));
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_mix_vector<T, bool, P, vecType, Aligned>
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x, vecType<T, P> const & y, vecType<bool, P> const & a)
		{
			vecType<T, P> Result(uninitialize);
			for(length_t i = 0; i < x.length(); ++i)
				Result[i] = a[i] ? y[i] : x[i];
			return Result;
		}
	};

	template <typename T, typename U, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_mix_scalar
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x, vecType<T, P> const & y, U const & a)
		{
			GLM_STATIC_ASSERT(std::numeric_limits<U>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'mix' only accept floating-point inputs for the interpolator a");

			return vecType<T, P>(vecType<U, P>(x) + a * vecType<U, P>(y - x));
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_mix_scalar<T, bool, P, vecType, Aligned>
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x, vecType<T, P> const & y, bool const & a)
		{
			return a ? y : x;
		}
	};

	template <typename T, typename U>
	struct compute_mix
	{
		GLM_FUNC_QUALIFIER static T call(T const & x, T const & y, U const & a)
		{
			GLM_STATIC_ASSERT(std::numeric_limits<U>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'mix' only accept floating-point inputs for the interpolator a");

			return static_cast<T>(static_cast<U>(x) + a * static_cast<U>(y - x));
		}
	};

	template <typename T>
	struct compute_mix<T, bool>
	{
		GLM_FUNC_QUALIFIER static T call(T const & x, T const & y, bool const & a)
		{
			return a ? y : x;
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool isFloat, bool Aligned>
	struct compute_sign
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x)
		{
			return vecType<T, P>(glm::lessThan(vecType<T, P>(0), x)) - vecType<T, P>(glm::lessThan(x, vecType<T, P>(0)));
		}
	};

#	if GLM_ARCH == GLM_ARCH_X86
	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_sign<T, P, vecType, false, Aligned>
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x)
		{
			T const Shift(static_cast<T>(sizeof(T) * 8 - 1));
			vecType<T, P> const y(vecType<typename make_unsigned<T>::type, P>(-x) >> typename make_unsigned<T>::type(Shift));

			return (x >> Shift) | y;
		}
	};
#	endif

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_floor
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x)
		{
			return detail::functor1<T, T, P, vecType>::call(std::floor, x);
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_ceil
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x)
		{
			return detail::functor1<T, T, P, vecType>::call(std::ceil, x);
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_fract
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x)
		{
			return x - floor(x);
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_trunc
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x)
		{
			return detail::functor1<T, T, P, vecType>::call(trunc, x);
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_round
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x)
		{
			return detail::functor1<T, T, P, vecType>::call(round, x);
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_mod
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & a, vecType<T, P> const & b)
		{
			GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'mod' only accept floating-point inputs. Include <glm/gtc/integer.hpp> for integer inputs.");
			return a - b * floor(a / b);
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_min_vector
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x, vecType<T, P> const & y)
		{
			return detail::functor2<T, P, vecType>::call(min, x, y);
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_max_vector
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x, vecType<T, P> const & y)
		{
			return detail::functor2<T, P, vecType>::call(max, x, y);
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_clamp_vector
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x, vecType<T, P> const & minVal, vecType<T, P> const & maxVal)
		{
			return min(max(x, minVal), maxVal);
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_step_vector
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & edge, vecType<T, P> const & x)
		{
			return mix(vecType<T, P>(1), vecType<T, P>(0), glm::lessThan(x, edge));
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool Aligned>
	struct compute_smoothstep_vector
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & edge0, vecType<T, P> const & edge1, vecType<T, P> const & x)
		{
			GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'step' only accept floating-point inputs");
			vecType<T, P> const tmp(clamp((x - edge0) / (edge1 - edge0), static_cast<T>(0), static_cast<T>(1)));
			return tmp * tmp * (static_cast<T>(3) - static_cast<T>(2) * tmp);
		}
	};
}//namespace detail

	template <typename genFIType>
	GLM_FUNC_QUALIFIER genFIType abs(genFIType x)
	{
		return detail::compute_abs<genFIType, std::numeric_limits<genFIType>::is_signed>::call(x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> abs(vecType<T, P> const & x)
	{
		return detail::compute_abs_vector<T, P, vecType, detail::is_aligned<P>::value>::call(x);
	}

	// sign
	// fast and works for any type
	template <typename genFIType> 
	GLM_FUNC_QUALIFIER genFIType sign(genFIType x)
	{
		GLM_STATIC_ASSERT(
			std::numeric_limits<genFIType>::is_iec559 || (std::numeric_limits<genFIType>::is_signed && std::numeric_limits<genFIType>::is_integer),
			"'sign' only accept signed inputs");
		
		return detail::compute_sign<genFIType, defaultp, tvec1, std::numeric_limits<genFIType>::is_iec559, highp>::call(tvec1<genFIType>(x)).x;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> sign(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(
			std::numeric_limits<T>::is_iec559 || (std::numeric_limits<T>::is_signed && std::numeric_limits<T>::is_integer),
			"'sign' only accept signed inputs");

		return detail::compute_sign<T, P, vecType, std::numeric_limits<T>::is_iec559, detail::is_aligned<P>::value>::call(x);
	}

	// floor
	using ::std::floor;
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> floor(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'floor' only accept floating-point inputs.");
		return detail::compute_floor<T, P, vecType, detail::is_aligned<P>::value>::call(x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> trunc(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'trunc' only accept floating-point inputs");
		return detail::compute_trunc<T, P, vecType, detail::is_aligned<P>::value>::call(x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> round(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'round' only accept floating-point inputs");
		return detail::compute_round<T, P, vecType, detail::is_aligned<P>::value>::call(x);
	}

/*
	// roundEven
	template <typename genType>
	GLM_FUNC_QUALIFIER genType roundEven(genType const& x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'roundEven' only accept floating-point inputs");

		return genType(int(x + genType(int(x) % 2)));
	}
*/

	// roundEven
	template <typename genType>
	GLM_FUNC_QUALIFIER genType roundEven(genType x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'roundEven' only accept floating-point inputs");
		
		int Integer = static_cast<int>(x);
		genType IntegerPart = static_cast<genType>(Integer);
		genType FractionalPart = fract(x);

		if(FractionalPart > static_cast<genType>(0.5) || FractionalPart < static_cast<genType>(0.5))
		{
			return round(x);
		}
		else if((Integer % 2) == 0)
		{
			return IntegerPart;
		}
		else if(x <= static_cast<genType>(0)) // Work around... 
		{
			return IntegerPart - static_cast<genType>(1);
		}
		else
		{
			return IntegerPart + static_cast<genType>(1);
		}
		//else // Bug on MinGW 4.5.2
		//{
		//	return mix(IntegerPart + genType(-1), IntegerPart + genType(1), x <= genType(0));
		//}
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> roundEven(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'roundEven' only accept floating-point inputs");
		return detail::functor1<T, T, P, vecType>::call(roundEven, x);
	}

	// ceil
	using ::std::ceil;
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> ceil(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'ceil' only accept floating-point inputs");
		return detail::compute_ceil<T, P, vecType, detail::is_aligned<P>::value>::call(x);
	}

	// fract
	template <typename genType>
	GLM_FUNC_QUALIFIER genType fract(genType x)
	{
		return fract(tvec1<genType>(x)).x;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> fract(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'fract' only accept floating-point inputs");
		return detail::compute_fract<T, P, vecType, detail::is_aligned<P>::value>::call(x);
	}

	// mod
	template <typename genType>
	GLM_FUNC_QUALIFIER genType mod(genType x, genType y)
	{
#		if GLM_COMPILER & GLM_COMPILER_CUDA
			// Another Cuda compiler bug https://github.com/g-truc/glm/issues/530
			tvec1<genType, defaultp> Result(mod(tvec1<genType, defaultp>(x), y));
			return Result.x;
#		else
			return mod(tvec1<genType, defaultp>(x), y).x;
#		endif
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> mod(vecType<T, P> const & x, T y)
	{
		return detail::compute_mod<T, P, vecType, detail::is_aligned<P>::value>::call(x, vecType<T, P>(y));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> mod(vecType<T, P> const & x, vecType<T, P> const & y)
	{
		return detail::compute_mod<T, P, vecType, detail::is_aligned<P>::value>::call(x, y);
	}

	// modf
	template <typename genType>
	GLM_FUNC_QUALIFIER genType modf(genType x, genType & i)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'modf' only accept floating-point inputs");
		return std::modf(x, &i);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec1<T, P> modf(tvec1<T, P> const & x, tvec1<T, P> & i)
	{
		return tvec1<T, P>(
			modf(x.x, i.x));
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec2<T, P> modf(tvec2<T, P> const & x, tvec2<T, P> & i)
	{
		return tvec2<T, P>(
			modf(x.x, i.x),
			modf(x.y, i.y));
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> modf(tvec3<T, P> const & x, tvec3<T, P> & i)
	{
		return tvec3<T, P>(
			modf(x.x, i.x),
			modf(x.y, i.y),
			modf(x.z, i.z));
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec4<T, P> modf(tvec4<T, P> const & x, tvec4<T, P> & i)
	{
		return tvec4<T, P>(
			modf(x.x, i.x),
			modf(x.y, i.y),
			modf(x.z, i.z),
			modf(x.w, i.w));
	}

	//// Only valid if (INT_MIN <= x-y <= INT_MAX)
	//// min(x,y)
	//r = y + ((x - y) & ((x - y) >> (sizeof(int) *
	//CHAR_BIT - 1)));
	//// max(x,y)
	//r = x - ((x - y) & ((x - y) >> (sizeof(int) *
	//CHAR_BIT - 1)));

	// min
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> min(vecType<T, P> const & a, T b)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'min' only accept floating-point inputs for the interpolator a");
		return detail::compute_min_vector<T, P, vecType, detail::is_aligned<P>::value>::call(a, vecType<T, P>(b));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> min(vecType<T, P> const & a, vecType<T, P> const & b)
	{
		return detail::compute_min_vector<T, P, vecType, detail::is_aligned<P>::value>::call(a, b);
	}

	// max
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> max(vecType<T, P> const & a, T b)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'max' only accept floating-point inputs for the interpolator a");
		return detail::compute_max_vector<T, P, vecType, detail::is_aligned<P>::value>::call(a, vecType<T, P>(b));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> max(vecType<T, P> const & a, vecType<T, P> const & b)
	{
		return detail::compute_max_vector<T, P, vecType, detail::is_aligned<P>::value>::call(a, b);
	}

	// clamp
	template <typename genType>
	GLM_FUNC_QUALIFIER genType clamp(genType x, genType minVal, genType maxVal)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559 || std::numeric_limits<genType>::is_integer || GLM_UNRESTRICTED_GENTYPE, "'clamp' only accept floating-point or integer inputs");
		return min(max(x, minVal), maxVal);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> clamp(vecType<T, P> const & x, T minVal, T maxVal)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || std::numeric_limits<T>::is_integer || GLM_UNRESTRICTED_GENTYPE, "'clamp' only accept floating-point or integer inputs");
		return detail::compute_clamp_vector<T, P, vecType, detail::is_aligned<P>::value>::call(x, vecType<T, P>(minVal), vecType<T, P>(maxVal));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> clamp(vecType<T, P> const & x, vecType<T, P> const & minVal, vecType<T, P> const & maxVal)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || std::numeric_limits<T>::is_integer || GLM_UNRESTRICTED_GENTYPE, "'clamp' only accept floating-point or integer inputs");
		return detail::compute_clamp_vector<T, P, vecType, detail::is_aligned<P>::value>::call(x, minVal, maxVal);
	}

	template <typename genTypeT, typename genTypeU>
	GLM_FUNC_QUALIFIER genTypeT mix(genTypeT x, genTypeT y, genTypeU a)
	{
		return detail::compute_mix<genTypeT, genTypeU>::call(x, y, a);
	}

	template <typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> mix(vecType<T, P> const & x, vecType<T, P> const & y, U a)
	{
		return detail::compute_mix_scalar<T, U, P, vecType, detail::is_aligned<P>::value>::call(x, y, a);
	}
	
	template <typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> mix(vecType<T, P> const & x, vecType<T, P> const & y, vecType<U, P> const & a)
	{
		return detail::compute_mix_vector<T, U, P, vecType, detail::is_aligned<P>::value>::call(x, y, a);
	}

	// step
	template <typename genType>
	GLM_FUNC_QUALIFIER genType step(genType edge, genType x)
	{
		return mix(static_cast<genType>(1), static_cast<genType>(0), glm::lessThan(x, edge));
	}

	template <template <typename, precision> class vecType, typename T, precision P>
	GLM_FUNC_QUALIFIER vecType<T, P> step(T edge, vecType<T, P> const & x)
	{
		return detail::compute_step_vector<T, P, vecType, detail::is_aligned<P>::value>::call(vecType<T, P>(edge), x);
	}

	template <template <typename, precision> class vecType, typename T, precision P>
	GLM_FUNC_QUALIFIER vecType<T, P> step(vecType<T, P> const & edge, vecType<T, P> const & x)
	{
		return detail::compute_step_vector<T, P, vecType, detail::is_aligned<P>::value>::call(edge, x);
	}

	// smoothstep
	template <typename genType>
	GLM_FUNC_QUALIFIER genType smoothstep(genType edge0, genType edge1, genType x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'smoothstep' only accept floating-point inputs");

		genType const tmp(clamp((x - edge0) / (edge1 - edge0), genType(0), genType(1)));
		return tmp * tmp * (genType(3) - genType(2) * tmp);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> smoothstep(T edge0, T edge1, vecType<T, P> const & x)
	{
		return detail::compute_smoothstep_vector<T, P, vecType, detail::is_aligned<P>::value>::call(vecType<T, P>(edge0), vecType<T, P>(edge1), x);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> smoothstep(vecType<T, P> const & edge0, vecType<T, P> const & edge1, vecType<T, P> const & x)
	{
		return detail::compute_smoothstep_vector<T, P, vecType, detail::is_aligned<P>::value>::call(edge0, edge1, x);
	}

#	if GLM_HAS_CXX11_STL
		using std::isnan;
#	else
		template <typename genType> 
		GLM_FUNC_QUALIFIER bool isnan(genType x)
		{
			GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'isnan' only accept floating-point inputs");

#			if GLM_HAS_CXX11_STL
				return std::isnan(x);
#			elif GLM_COMPILER & GLM_COMPILER_VC
				return _isnan(x) != 0;
#			elif GLM_COMPILER & GLM_COMPILER_INTEL
#				if GLM_PLATFORM & GLM_PLATFORM_WINDOWS
					return _isnan(x) != 0;
#				else
					return ::isnan(x) != 0;
#				endif
#			elif (GLM_COMPILER & (GLM_COMPILER_GCC | GLM_COMPILER_CLANG)) && (GLM_PLATFORM & GLM_PLATFORM_ANDROID) && __cplusplus < 201103L
				return _isnan(x) != 0;
#			elif GLM_COMPILER & GLM_COMPILER_CUDA
				return isnan(x) != 0;
#			else
				return std::isnan(x);
#			endif
		}
#	endif

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> isnan(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'isnan' only accept floating-point inputs");

		return detail::functor1<bool, T, P, vecType>::call(isnan, x);
	}

#	if GLM_HAS_CXX11_STL
		using std::isinf;
#	else
		template <typename genType> 
		GLM_FUNC_QUALIFIER bool isinf(genType x)
		{
			GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'isinf' only accept floating-point inputs");

#			if GLM_HAS_CXX11_STL
				return std::isinf(x);
#			elif GLM_COMPILER & (GLM_COMPILER_INTEL | GLM_COMPILER_VC)
#				if(GLM_PLATFORM & GLM_PLATFORM_WINDOWS)
					return _fpclass(x) == _FPCLASS_NINF || _fpclass(x) == _FPCLASS_PINF;
#				else
					return ::isinf(x);
#				endif
#			elif GLM_COMPILER & (GLM_COMPILER_GCC | GLM_COMPILER_CLANG)
#				if(GLM_PLATFORM & GLM_PLATFORM_ANDROID && __cplusplus < 201103L)
					return _isinf(x) != 0;
#				else
					return std::isinf(x);
#				endif
#			elif GLM_COMPILER & GLM_COMPILER_CUDA
				// http://developer.download.nvidia.com/compute/cuda/4_2/rel/toolkit/docs/online/group__CUDA__MATH__DOUBLE_g13431dd2b40b51f9139cbb7f50c18fab.html#g13431dd2b40b51f9139cbb7f50c18fab
				return isinf(double(x)) != 0;
#			else
				return std::isinf(x);
#			endif
	}
#	endif

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> isinf(vecType<T, P> const & x)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'isnan' only accept floating-point inputs");

		return detail::functor1<bool, T, P, vecType>::call(isinf, x);
	}

	GLM_FUNC_QUALIFIER int floatBitsToInt(float const & v)
	{
		return reinterpret_cast<int&>(const_cast<float&>(v));
	}

	template <template <typename, precision> class vecType, precision P>
	GLM_FUNC_QUALIFIER vecType<int, P> floatBitsToInt(vecType<float, P> const & v)
	{
		return reinterpret_cast<vecType<int, P>&>(const_cast<vecType<float, P>&>(v));
	}

	GLM_FUNC_QUALIFIER uint floatBitsToUint(float const & v)
	{
		return reinterpret_cast<uint&>(const_cast<float&>(v));
	}

	template <template <typename, precision> class vecType, precision P>
	GLM_FUNC_QUALIFIER vecType<uint, P> floatBitsToUint(vecType<float, P> const & v)
	{
		return reinterpret_cast<vecType<uint, P>&>(const_cast<vecType<float, P>&>(v));
	}

	GLM_FUNC_QUALIFIER float intBitsToFloat(int const & v)
	{
		return reinterpret_cast<float&>(const_cast<int&>(v));
	}

	template <template <typename, precision> class vecType, precision P>
	GLM_FUNC_QUALIFIER vecType<float, P> intBitsToFloat(vecType<int, P> const & v)
	{
		return reinterpret_cast<vecType<float, P>&>(const_cast<vecType<int, P>&>(v));
	}

	GLM_FUNC_QUALIFIER float uintBitsToFloat(uint const & v)
	{
		return reinterpret_cast<float&>(const_cast<uint&>(v));
	}

	template <template <typename, precision> class vecType, precision P>
	GLM_FUNC_QUALIFIER vecType<float, P> uintBitsToFloat(vecType<uint, P> const & v)
	{
		return reinterpret_cast<vecType<float, P>&>(const_cast<vecType<uint, P>&>(v));
	}
	
	template <typename genType>
	GLM_FUNC_QUALIFIER genType fma(genType const & a, genType const & b, genType const & c)
	{
		return a * b + c;
	}

	template <typename genType>
	GLM_FUNC_QUALIFIER genType frexp(genType x, int & exp)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'frexp' only accept floating-point inputs");

		return std::frexp(x, &exp);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec1<T, P> frexp(tvec1<T, P> const & x, tvec1<int, P> & exp)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'frexp' only accept floating-point inputs");

		return tvec1<T, P>(std::frexp(x.x, &exp.x));
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec2<T, P> frexp(tvec2<T, P> const & x, tvec2<int, P> & exp)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'frexp' only accept floating-point inputs");

		return tvec2<T, P>(
			frexp(x.x, exp.x),
			frexp(x.y, exp.y));
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> frexp(tvec3<T, P> const & x, tvec3<int, P> & exp)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'frexp' only accept floating-point inputs");

		return tvec3<T, P>(
			frexp(x.x, exp.x),
			frexp(x.y, exp.y),
			frexp(x.z, exp.z));
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec4<T, P> frexp(tvec4<T, P> const & x, tvec4<int, P> & exp)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'frexp' only accept floating-point inputs");

		return tvec4<T, P>(
			frexp(x.x, exp.x),
			frexp(x.y, exp.y),
			frexp(x.z, exp.z),
			frexp(x.w, exp.w));
	}

	template <typename genType>
	GLM_FUNC_QUALIFIER genType ldexp(genType const & x, int const & exp)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'ldexp' only accept floating-point inputs");

		return std::ldexp(x, exp);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec1<T, P> ldexp(tvec1<T, P> const & x, tvec1<int, P> const & exp)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'ldexp' only accept floating-point inputs");

		return tvec1<T, P>(
			ldexp(x.x, exp.x));
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec2<T, P> ldexp(tvec2<T, P> const & x, tvec2<int, P> const & exp)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'ldexp' only accept floating-point inputs");

		return tvec2<T, P>(
			ldexp(x.x, exp.x),
			ldexp(x.y, exp.y));
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> ldexp(tvec3<T, P> const & x, tvec3<int, P> const & exp)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'ldexp' only accept floating-point inputs");

		return tvec3<T, P>(
			ldexp(x.x, exp.x),
			ldexp(x.y, exp.y),
			ldexp(x.z, exp.z));
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec4<T, P> ldexp(tvec4<T, P> const & x, tvec4<int, P> const & exp)
	{
		GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559 || GLM_UNRESTRICTED_GENTYPE, "'ldexp' only accept floating-point inputs");

		return tvec4<T, P>(
			ldexp(x.x, exp.x),
			ldexp(x.y, exp.y),
			ldexp(x.z, exp.z),
			ldexp(x.w, exp.w));
	}
}//namespace glm

#if GLM_ARCH != GLM_ARCH_PURE && GLM_HAS_UNRESTRICTED_UNIONS
#	include "func_common_simd.inl"
#endif
