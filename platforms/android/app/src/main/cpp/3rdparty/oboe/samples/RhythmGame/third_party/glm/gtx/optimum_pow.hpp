/// @ref gtx_optimum_pow
/// @file glm/gtx/optimum_pow.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_optimum_pow GLM_GTX_optimum_pow
/// @ingroup gtx
///
/// @brief Integer exponentiation of power functions.
///
/// <glm/gtx/optimum_pow.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_optimum_pow extension included")
#endif

namespace glm{
namespace gtx
{
	/// @addtogroup gtx_optimum_pow
	/// @{

	/// Returns x raised to the power of 2.
	///
	/// @see gtx_optimum_pow
	template <typename genType>
	GLM_FUNC_DECL genType pow2(genType const & x);

	/// Returns x raised to the power of 3.
	///
	/// @see gtx_optimum_pow
	template <typename genType>
	GLM_FUNC_DECL genType pow3(genType const & x);

	/// Returns x raised to the power of 4.
	///
	/// @see gtx_optimum_pow
	template <typename genType>
	GLM_FUNC_DECL genType pow4(genType const & x);

	/// @}
}//namespace gtx
}//namespace glm

#include "optimum_pow.inl"
