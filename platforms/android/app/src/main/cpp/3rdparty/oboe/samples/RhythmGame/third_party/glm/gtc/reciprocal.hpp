/// @ref gtc_reciprocal
/// @file glm/gtc/reciprocal.hpp
///
/// @see core (dependence)
///
/// @defgroup gtc_reciprocal GLM_GTC_reciprocal
/// @ingroup gtc
///
/// @brief Define secant, cosecant and cotangent functions.
///
/// <glm/gtc/reciprocal.hpp> need to be included to use these features.

#pragma once

// Dependencies
#include "../detail/setup.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_reciprocal extension included")
#endif

namespace glm
{
	/// @addtogroup gtc_reciprocal
	/// @{

	/// Secant function.
	/// hypotenuse / adjacent or 1 / cos(x)
	/// 
	/// @tparam genType Floating-point scalar or vector types.
	/// 
	/// @see gtc_reciprocal
	template <typename genType>
	GLM_FUNC_DECL genType sec(genType angle);

	/// Cosecant function.
	/// hypotenuse / opposite or 1 / sin(x)
	/// 
	/// @tparam genType Floating-point scalar or vector types.
	/// 
	/// @see gtc_reciprocal
	template <typename genType> 
	GLM_FUNC_DECL genType csc(genType angle);
		
	/// Cotangent function.
	/// adjacent / opposite or 1 / tan(x)
	/// 
	/// @tparam genType Floating-point scalar or vector types.
	/// 
	/// @see gtc_reciprocal
	template <typename genType>
	GLM_FUNC_DECL genType cot(genType angle);

	/// Inverse secant function.
	/// 
	/// @return Return an angle expressed in radians.
	/// @tparam genType Floating-point scalar or vector types.
	/// 
	/// @see gtc_reciprocal
	template <typename genType>
	GLM_FUNC_DECL genType asec(genType x);

	/// Inverse cosecant function.
	/// 
	/// @return Return an angle expressed in radians.
	/// @tparam genType Floating-point scalar or vector types.
	/// 
	/// @see gtc_reciprocal
	template <typename genType>
	GLM_FUNC_DECL genType acsc(genType x);
		
	/// Inverse cotangent function.
	/// 
	/// @return Return an angle expressed in radians.
	/// @tparam genType Floating-point scalar or vector types.
	/// 
	/// @see gtc_reciprocal
	template <typename genType>
	GLM_FUNC_DECL genType acot(genType x);

	/// Secant hyperbolic function.
	/// 
	/// @tparam genType Floating-point scalar or vector types.
	/// 
	/// @see gtc_reciprocal
	template <typename genType>
	GLM_FUNC_DECL genType sech(genType angle);

	/// Cosecant hyperbolic function.
	/// 
	/// @tparam genType Floating-point scalar or vector types.
	/// 
	/// @see gtc_reciprocal
	template <typename genType>
	GLM_FUNC_DECL genType csch(genType angle);
		
	/// Cotangent hyperbolic function.
	/// 
	/// @tparam genType Floating-point scalar or vector types.
	/// 
	/// @see gtc_reciprocal
	template <typename genType>
	GLM_FUNC_DECL genType coth(genType angle);

	/// Inverse secant hyperbolic function.
	/// 
	/// @return Return an angle expressed in radians.
	/// @tparam genType Floating-point scalar or vector types.
	/// 
	/// @see gtc_reciprocal
	template <typename genType>
	GLM_FUNC_DECL genType asech(genType x);

	/// Inverse cosecant hyperbolic function.
	/// 
	/// @return Return an angle expressed in radians.
	/// @tparam genType Floating-point scalar or vector types.
	/// 
	/// @see gtc_reciprocal
	template <typename genType>
	GLM_FUNC_DECL genType acsch(genType x);
		
	/// Inverse cotangent hyperbolic function.
	/// 
	/// @return Return an angle expressed in radians.
	/// @tparam genType Floating-point scalar or vector types.
	/// 
	/// @see gtc_reciprocal
	template <typename genType>
	GLM_FUNC_DECL genType acoth(genType x);

	/// @}
}//namespace glm

#include "reciprocal.inl"
