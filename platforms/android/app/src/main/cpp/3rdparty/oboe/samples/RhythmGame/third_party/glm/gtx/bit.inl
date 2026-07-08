/// @ref gtx_bit
/// @file glm/gtx/bit.inl

namespace glm
{
	///////////////////
	// highestBitValue

	template <typename genIUType>
	GLM_FUNC_QUALIFIER genIUType highestBitValue(genIUType Value)
	{
		genIUType tmp = Value;
		genIUType result = genIUType(0);
		while(tmp)
		{
			result = (tmp & (~tmp + 1)); // grab lowest bit
			tmp &= ~result; // clear lowest bit
		}
		return result;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> highestBitValue(vecType<T, P> const & v)
	{
		return detail::functor1<T, T, P, vecType>::call(highestBitValue, v);
	}

	///////////////////
	// lowestBitValue

	template <typename genIUType>
	GLM_FUNC_QUALIFIER genIUType lowestBitValue(genIUType Value)
	{
		return (Value & (~Value + 1));
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> lowestBitValue(vecType<T, P> const & v)
	{
		return detail::functor1<T, T, P, vecType>::call(lowestBitValue, v);
	}

	///////////////////
	// powerOfTwoAbove

	template <typename genType>
	GLM_FUNC_QUALIFIER genType powerOfTwoAbove(genType value)
	{
		return isPowerOfTwo(value) ? value : highestBitValue(value) << 1;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> powerOfTwoAbove(vecType<T, P> const & v)
	{
		return detail::functor1<T, T, P, vecType>::call(powerOfTwoAbove, v);
	}

	///////////////////
	// powerOfTwoBelow

	template <typename genType>
	GLM_FUNC_QUALIFIER genType powerOfTwoBelow(genType value)
	{
		return isPowerOfTwo(value) ? value : highestBitValue(value);
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> powerOfTwoBelow(vecType<T, P> const & v)
	{
		return detail::functor1<T, T, P, vecType>::call(powerOfTwoBelow, v);
	}

	/////////////////////
	// powerOfTwoNearest

	template <typename genType>
	GLM_FUNC_QUALIFIER genType powerOfTwoNearest(genType value)
	{
		if(isPowerOfTwo(value))
			return value;

		genType const prev = highestBitValue(value);
		genType const next = prev << 1;
		return (next - value) < (value - prev) ? next : prev;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> powerOfTwoNearest(vecType<T, P> const & v)
	{
		return detail::functor1<T, T, P, vecType>::call(powerOfTwoNearest, v);
	}

}//namespace glm
