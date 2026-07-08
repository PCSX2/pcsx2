/// @ref gtx_extend
/// @file glm/gtx/extend.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_extend GLM_GTX_extend
/// @ingroup gtx
///
/// @brief Extend a position from a source to a position at a defined length.
///
/// <glm/gtx/extend.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_extend extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_extend
	/// @{

	/// Extends of Length the Origin position using the (Source - Origin) direction.
	/// @see gtx_extend
	template <typename genType> 
	GLM_FUNC_DECL genType extend(
		genType const & Origin, 
		genType const & Source, 
		typename genType::value_type const Length);

	/// @}
}//namespace glm

#include "extend.inl"
