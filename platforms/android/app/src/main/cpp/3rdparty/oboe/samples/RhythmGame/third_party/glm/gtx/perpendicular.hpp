/// @ref gtx_perpendicular
/// @file glm/gtx/perpendicular.hpp
///
/// @see core (dependence)
/// @see gtx_projection (dependence)
///
/// @defgroup gtx_perpendicular GLM_GTX_perpendicular
/// @ingroup gtx
///
/// @brief Perpendicular of a vector from other one
///
/// <glm/gtx/perpendicular.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtx/projection.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_perpendicular extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_perpendicular
	/// @{

	//! Projects x a perpendicular axis of Normal.
	//! From GLM_GTX_perpendicular extension.
	template <typename vecType> 
	GLM_FUNC_DECL vecType perp(
		vecType const & x, 
		vecType const & Normal);

	/// @}
}//namespace glm

#include "perpendicular.inl"
