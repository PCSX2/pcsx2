/// @ref gtx_normalize_dot
/// @file glm/gtx/normalize_dot.hpp
///
/// @see core (dependence)
/// @see gtx_fast_square_root (dependence)
///
/// @defgroup gtx_normalize_dot GLM_GTX_normalize_dot
/// @ingroup gtx
///
/// @brief Dot product of vectors that need to be normalize with a single square root.
///
/// <glm/gtx/normalized_dot.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../gtx/fast_square_root.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_normalize_dot extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_normalize_dot
	/// @{

	/// Normalize parameters and returns the dot product of x and y.
	/// It's faster that dot(normalize(x), normalize(y)).
	///
	/// @see gtx_normalize_dot extension.
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL T normalizeDot(vecType<T, P> const & x, vecType<T, P> const & y);

	/// Normalize parameters and returns the dot product of x and y.
	/// Faster that dot(fastNormalize(x), fastNormalize(y)).
	///
	/// @see gtx_normalize_dot extension.
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL T fastNormalizeDot(vecType<T, P> const & x, vecType<T, P> const & y);

	/// @}
}//namespace glm

#include "normalize_dot.inl"
