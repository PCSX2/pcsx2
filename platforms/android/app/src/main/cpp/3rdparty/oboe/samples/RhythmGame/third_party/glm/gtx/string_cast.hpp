/// @ref gtx_string_cast
/// @file glm/gtx/string_cast.hpp
///
/// @see core (dependence)
/// @see gtc_half_float (dependence)
/// @see gtx_integer (dependence)
/// @see gtx_quaternion (dependence)
///
/// @defgroup gtx_string_cast GLM_GTX_string_cast
/// @ingroup gtx
///
/// @brief Setup strings for GLM type values
///
/// <glm/gtx/string_cast.hpp> need to be included to use these functionalities.
/// This extension is not supported with CUDA

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtc/type_precision.hpp"
#include "../gtc/quaternion.hpp"
#include "../gtx/dual_quaternion.hpp"
#include <string>

#if(GLM_COMPILER & GLM_COMPILER_CUDA)
#	error "GLM_GTX_string_cast is not supported on CUDA compiler"
#endif

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_string_cast extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_string_cast
	/// @{

	/// Create a string from a GLM vector or matrix typed variable.
	/// @see gtx_string_cast extension.
	template <template <typename, precision> class matType, typename T, precision P>
	GLM_FUNC_DECL std::string to_string(matType<T, P> const & x);

	/// @}
}//namespace glm

#include "string_cast.inl"
