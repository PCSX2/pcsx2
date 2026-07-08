/// @ref gtx_polar_coordinates
/// @file glm/gtx/polar_coordinates.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_polar_coordinates GLM_GTX_polar_coordinates
/// @ingroup gtx
///
/// @brief Conversion from Euclidean space to polar space and revert.
///
/// <glm/gtx/polar_coordinates.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_polar_coordinates extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_polar_coordinates
	/// @{

	/// Convert Euclidean to Polar coordinates, x is the xz distance, y, the latitude and z the longitude.
	///
	/// @see gtx_polar_coordinates
	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> polar(
		tvec3<T, P> const & euclidean);

	/// Convert Polar to Euclidean coordinates.
	///
	/// @see gtx_polar_coordinates
	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> euclidean(
		tvec2<T, P> const & polar);

	/// @}
}//namespace glm

#include "polar_coordinates.inl"
