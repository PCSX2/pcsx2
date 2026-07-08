/// @ref gtc_functions
/// @file glm/gtc/functions.hpp
/// 
/// @see core (dependence)
/// @see gtc_half_float (dependence)
/// @see gtc_quaternion (dependence)
///
/// @defgroup gtc_functions GLM_GTC_functions
/// @ingroup gtc
/// 
/// @brief List of useful common functions.
/// 
/// <glm/gtc/functions.hpp> need to be included to use these functionalities.

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/precision.hpp"
#include "../detail/type_vec2.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_functions extension included")
#endif

namespace glm
{
	/// @addtogroup gtc_functions
	/// @{

	/// 1D gauss function
	///
	/// @see gtc_epsilon
	template <typename T>
	GLM_FUNC_DECL T gauss(
		T x,
		T ExpectedValue,
		T StandardDeviation);

	/// 2D gauss function
	///
	/// @see gtc_epsilon
	template <typename T, precision P>
	GLM_FUNC_DECL T gauss(
		tvec2<T, P> const& Coord,
		tvec2<T, P> const& ExpectedValue,
		tvec2<T, P> const& StandardDeviation);

	/// @}
}//namespace glm

#include "functions.inl"

