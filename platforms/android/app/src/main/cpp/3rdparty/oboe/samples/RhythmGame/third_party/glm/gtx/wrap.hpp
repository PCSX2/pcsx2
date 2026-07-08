/// @ref gtx_wrap
/// @file glm/gtx/wrap.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_wrap GLM_GTX_wrap
/// @ingroup gtx
///
/// @brief Wrapping mode of texture coordinates.
///
/// <glm/gtx/wrap.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtc/vec1.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_wrap extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_wrap
	/// @{

	/// Simulate GL_CLAMP OpenGL wrap mode
	/// @see gtx_wrap extension.
	template <typename genType>
	GLM_FUNC_DECL genType clamp(genType const& Texcoord);

	/// Simulate GL_REPEAT OpenGL wrap mode
	/// @see gtx_wrap extension.
	template <typename genType>
	GLM_FUNC_DECL genType repeat(genType const& Texcoord);

	/// Simulate GL_MIRRORED_REPEAT OpenGL wrap mode
	/// @see gtx_wrap extension.
	template <typename genType>
	GLM_FUNC_DECL genType mirrorClamp(genType const& Texcoord);

	/// Simulate GL_MIRROR_REPEAT OpenGL wrap mode
	/// @see gtx_wrap extension.
	template <typename genType>
	GLM_FUNC_DECL genType mirrorRepeat(genType const& Texcoord);

	/// @}
}// namespace glm

#include "wrap.inl"
