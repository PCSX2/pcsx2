/// @ref gtc_epsilon
/// @file glm/gtc/epsilon.hpp
/// 
/// @see core (dependence)
/// @see gtc_half_float (dependence)
/// @see gtc_quaternion (dependence)
///
/// @defgroup gtc_epsilon GLM_GTC_epsilon
/// @ingroup gtc
/// 
/// @brief Comparison functions for a user defined epsilon values.
/// 
/// <glm/gtc/epsilon.hpp> need to be included to use these functionalities.

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/precision.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_epsilon extension included")
#endif

namespace glm
{
	/// @addtogroup gtc_epsilon
	/// @{

	/// Returns the component-wise comparison of |x - y| < epsilon.
	/// True if this expression is satisfied.
	///
	/// @see gtc_epsilon
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<bool, P> epsilonEqual(
		vecType<T, P> const & x,
		vecType<T, P> const & y,
		T const & epsilon);

	/// Returns the component-wise comparison of |x - y| < epsilon.
	/// True if this expression is satisfied.
	///
	/// @see gtc_epsilon
	template <typename genType>
	GLM_FUNC_DECL bool epsilonEqual(
		genType const & x,
		genType const & y,
		genType const & epsilon);

	/// Returns the component-wise comparison of |x - y| < epsilon.
	/// True if this expression is not satisfied.
	///
	/// @see gtc_epsilon
	template <typename genType>
	GLM_FUNC_DECL typename genType::boolType epsilonNotEqual(
		genType const & x,
		genType const & y,
		typename genType::value_type const & epsilon);

	/// Returns the component-wise comparison of |x - y| >= epsilon.
	/// True if this expression is not satisfied.
	///
	/// @see gtc_epsilon
	template <typename genType>
	GLM_FUNC_DECL bool epsilonNotEqual(
		genType const & x,
		genType const & y,
		genType const & epsilon);

	/// @}
}//namespace glm

#include "epsilon.inl"
