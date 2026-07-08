/// @ref gtx_handed_coordinate_space
/// @file glm/gtx/handed_coordinate_space.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_handed_coordinate_space GLM_GTX_handed_coordinate_space
/// @ingroup gtx
///
/// @brief To know if a set of three basis vectors defines a right or left-handed coordinate system.
///
/// <glm/gtx/handed_coordinate_system.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_handed_coordinate_space extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_handed_coordinate_space
	/// @{

	//! Return if a trihedron right handed or not.
	//! From GLM_GTX_handed_coordinate_space extension.
	template <typename T, precision P>
	GLM_FUNC_DECL bool rightHanded(
		tvec3<T, P> const & tangent,
		tvec3<T, P> const & binormal,
		tvec3<T, P> const & normal);

	//! Return if a trihedron left handed or not.
	//! From GLM_GTX_handed_coordinate_space extension.
	template <typename T, precision P>
	GLM_FUNC_DECL bool leftHanded(
		tvec3<T, P> const & tangent,
		tvec3<T, P> const & binormal,
		tvec3<T, P> const & normal);

	/// @}
}// namespace glm

#include "handed_coordinate_space.inl"
