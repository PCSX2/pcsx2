/// @ref gtc_round
/// @file glm/gtc/round.inl

#include "../detail/func_integer.hpp"

namespace glm{
namespace detail
{
	template <typename T, precision P, template <typename, precision> class vecType, bool compute = false>
	struct compute_ceilShift
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & v, T)
		{
			return v;
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType>
	struct compute_ceilShift<T, P, vecType, true>
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & v, T Shift)
		{
			return v | (v >> Shift);
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType, bool isSigned = true>
	struct compute_ceilPowerOfTwo
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x)
		{
			GLM_STATIC_ASSERT(!std::numeric_limits<T>::is_iec559, "'ceilPowerOfTwo' only accept integer scalar or vector inputs");

			vecType<T, P> const Sign(sign(x));

			vecType<T, P> v(abs(x));

			v = v - static_cast<T>(1);
			v = v | (v >> static_cast<T>(1));
			v = v | (v >> static_cast<T>(2));
			v = v | (v >> static_cast<T>(4));
			v = compute_ceilShift<T, P, vecType, sizeof(T) >= 2>::call(v, 8);
			v = compute_ceilShift<T, P, vecType, sizeof(T) >= 4>::call(v, 16);
			v = compute_ceilShift<T, P, vecType, sizeof(T) >= 8>::call(v, 32);
			return (v + static_cast<T>(1)) * Sign;
		}
	};

	template <typename T, precision P, template <typename, precision> class vecType>
	struct compute_ceilPowerOfTwo<T, P, vecType, false>
	{
		GLM_FUNC_QUALIFIER static vecType<T, P> call(vecType<T, P> const & x)
		{
			GLM_STATIC_ASSERT(!std::numeric_limits<T>::is_iec559, "'ceilPowerOfTwo' only accept integer scalar or vector inputs");

			vecType<T, P> v(x);

			v = v - static_cast<T>(1);
			v = v | (v >> static_cast<T>(1));
			v = v | (v >> static_cast<T>(2));
			v = v | (v >> static_cast<T>(4));
			v = compute_ceilShift<T, P, vecType, sizeof(T) >= 2>::call(v, 8);
			v = compute_ceilShift<T, P, vecType, sizeof(T) >= 4>::call(v, 16);
			v = compute_ceilShift<T, P, vecType, sizeof(T) >= 8>::call(v, 32);
			return v + static_cast<T>(1);
		}
	};

	template <bool is_float, bool is_signed>
	struct compute_ceilMultiple{};

	template <>
	struct compute_ceilMultiple<true, true>
	{
		template <typename genType>
		GLM_FUNC_QUALIFIER static genType call(genType Source, genType Multiple)
		{
			if(Source > genType(0))
				return Source + (Multiple - std::fmod(Source, Multiple));
			else
				return Source + std::fmod(-Source, Multiple);
		}
	};

	template <>
	struct compute_ceilMultiple<false, false>
	{
		template <typename genType>
		GLM_FUNC_QUALIFIER static genType call(genType Source, genType Multiple)
		{
			genType Tmp = Source - genType(1);
			return Tmp + (Multiple - (Tmp % Multiple));
		}
	};

	template <>
	struct compute_ceilMultiple<false, true>
	{
		template <typename genType>
		GLM_FUNC_QUALIFIER static genType call(genType Source, genType Multiple)
		{
			if(Source > genType(0))
			{
				genType Tmp = Source - genType(1);
				return Tmp + (Multiple - (Tmp % Multiple));
			}
			else
				return Source + (-Source % Multiple);
		}
	};

	template <bool is_float, bool is_signed>
	struct compute_floorMultiple{};

	template <>
	struct compute_floorMultiple<true, true>
	{
		template <typename genType>
		GLM_FUNC_QUALIFIER static genType call(genType Source, genType Multiple)
		{
			if(Source >= genType(0))
				return Source - std::fmod(Source, Multiple);
			else
				return Source - std::fmod(Source, Multiple) - Multiple;
		}
	};

	template <>
	struct compute_floorMultiple<false, false>
	{
		template <typename genType>
		GLM_FUNC_QUALIFIER static genType call(genType Source, genType Multiple)
		{
			if(Source >= genType(0))
				return Source - Source % Multiple;
			else
			{
				genType Tmp = Source + genType(1);
				return Tmp - Tmp % Multiple - Multiple;
			}
		}
	};

	template <>
	struct compute_floorMultiple<false, true>
	{
		template <typename genType>
		GLM_FUNC_QUALIFIER static genType call(genType Source, genType Multiple)
		{
			if(Source >= genType(0))
				return Source - Source % Multiple;
			else
			{
				genType Tmp = Source + genType(1);
				return Tmp - Tmp % Multiple - Multiple;
			}
		}
	};

	template <bool is_float, bool is_signed>
	struct compute_roundMultiple{};

	template <>
	struct compute_roundMultiple<true, true>
	{
		template <typename genType>
		GLM_FUNC_QUALIFIER static genType call(genType Source, genType Multiple)
		{
			if(Source >= genType(0))
				return Source - std::fmod(Source, Multiple);
			else
			{
				genType Tmp = Source + genType(1);
				return Tmp - std::fmod(Tmp, Multiple) - Multiple;
			}
		}
	};

	template <>
	struct compute_roundMultiple<false, false>
	{
		template <typename genType>
		GLM_FUNC_QUALIFIER static genType call(genType Source, genType Multiple)
		{
			if(Source >= genType(0))
				return Source - Source % Multiple;
			else
			{
				genType Tmp = Source + genType(1);
				return Tmp - Tmp % Multiple - Multiple;
			}
		}
	};

	template <>
	struct compute_roundMultiple<false, true>
	{
		template <typename genType>
		GLM_FUNC_QUALIFIER static genType call(genType Source, genType Multiple)
		{
			if(Source >= genType(0))
				return Source - Source % Multiple;
			else
			{
				genType Tmp = Source + genType(1);
				return Tmp - Tmp % Multiple - Multiple;
			}
		}
	};
}//namespace detail

	////////////////
	// isPowerOfTwo

	template <typename genType>
	GLM_FUNC_QUALIFIER bool isPowerOfTwo(genType Value)
	{
		genType const Result = glm::abs(Value);
		return !(Result & (Result - 1));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> isPowerOfTwo(vecType<T, P> const & Value)
	{
		vecType<T, P> const Result(abs(Value));
		return equal(Result & (Result - 1), vecType<T, P>(0));
	}

	//////////////////
	// ceilPowerOfTwo

	template <typename genType>
	GLM_FUNC_QUALIFIER genType ceilPowerOfTwo(genType value)
	{
		return detail::compute_ceilPowerOfTwo<genType, defaultp, tvec1, std::numeric_limits<genType>::is_signed>::call(tvec1<genType, defaultp>(value)).x;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> ceilPowerOfTwo(vecType<T, P> const & v)
	{
		return detail::compute_ceilPowerOfTwo<T, P, vecType, std::numeric_limits<T>::is_signed>::call(v);
	}

	///////////////////
	// floorPowerOfTwo

	template <typename genType>
	GLM_FUNC_QUALIFIER genType floorPowerOfTwo(genType value)
	{
		return isPowerOfTwo(value) ? value : static_cast<genType>(1) << findMSB(value);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> floorPowerOfTwo(vecType<T, P> const & v)
	{
		return detail::functor1<T, T, P, vecType>::call(floorPowerOfTwo, v);
	}

	///////////////////
	// roundPowerOfTwo

	template <typename genIUType>
	GLM_FUNC_QUALIFIER genIUType roundPowerOfTwo(genIUType value)
	{
		if(isPowerOfTwo(value))
			return value;

		genIUType const prev = static_cast<genIUType>(1) << findMSB(value);
		genIUType const next = prev << static_cast<genIUType>(1);
		return (next - value) < (value - prev) ? next : prev;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> roundPowerOfTwo(vecType<T, P> const & v)
	{
		return detail::functor1<T, T, P, vecType>::call(roundPowerOfTwo, v);
	}

	////////////////
	// isMultiple

	template <typename genType>
	GLM_FUNC_QUALIFIER bool isMultiple(genType Value, genType Multiple)
	{
		return isMultiple(tvec1<genType>(Value), tvec1<genType>(Multiple)).x;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> isMultiple(vecType<T, P> const & Value, T Multiple)
	{
		return (Value % Multiple) == vecType<T, P>(0);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<bool, P> isMultiple(vecType<T, P> const & Value, vecType<T, P> const & Multiple)
	{
		return (Value % Multiple) == vecType<T, P>(0);
	}

	//////////////////////
	// ceilMultiple

	template <typename genType>
	GLM_FUNC_QUALIFIER genType ceilMultiple(genType Source, genType Multiple)
	{
		return detail::compute_ceilMultiple<std::numeric_limits<genType>::is_iec559, std::numeric_limits<genType>::is_signed>::call(Source, Multiple);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> ceilMultiple(vecType<T, P> const & Source, vecType<T, P> const & Multiple)
	{
		return detail::functor2<T, P, vecType>::call(ceilMultiple, Source, Multiple);
	}

	//////////////////////
	// floorMultiple

	template <typename genType>
	GLM_FUNC_QUALIFIER genType floorMultiple(genType Source, genType Multiple)
	{
		return detail::compute_floorMultiple<std::numeric_limits<genType>::is_iec559, std::numeric_limits<genType>::is_signed>::call(Source, Multiple);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> floorMultiple(vecType<T, P> const & Source, vecType<T, P> const & Multiple)
	{
		return detail::functor2<T, P, vecType>::call(floorMultiple, Source, Multiple);
	}

	//////////////////////
	// roundMultiple

	template <typename genType>
	GLM_FUNC_QUALIFIER genType roundMultiple(genType Source, genType Multiple)
	{
		return detail::compute_roundMultiple<std::numeric_limits<genType>::is_iec559, std::numeric_limits<genType>::is_signed>::call(Source, Multiple);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> roundMultiple(vecType<T, P> const & Source, vecType<T, P> const & Multiple)
	{
		return detail::functor2<T, P, vecType>::call(roundMultiple, Source, Multiple);
	}
}//namespace glm
