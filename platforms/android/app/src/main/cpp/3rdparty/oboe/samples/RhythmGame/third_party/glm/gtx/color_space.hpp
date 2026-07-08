/// @ref gtx_color_space
/// @file glm/gtx/color_space.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_color_space GLM_GTX_color_space
/// @ingroup gtx
///
/// @brief Related to RGB to HSV conversions and operations.
///
/// <glm/gtx/color_space.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_color_space extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_color_space
	/// @{

	/// Converts a color from HSV color space to its color in RGB color space.
	/// @see gtx_color_space
	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> rgbColor(
		tvec3<T, P> const & hsvValue);

	/// Converts a color from RGB color space to its color in HSV color space.
	/// @see gtx_color_space
	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> hsvColor(
		tvec3<T, P> const & rgbValue);
		
	/// Build a saturation matrix.
	/// @see gtx_color_space
	template <typename T>
	GLM_FUNC_DECL tmat4x4<T, defaultp> saturation(
		T const s);

	/// Modify the saturation of a color.
	/// @see gtx_color_space
	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> saturation(
		T const s,
		tvec3<T, P> const & color);
		
	/// Modify the saturation of a color.
	/// @see gtx_color_space
	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<T, P> saturation(
		T const s,
		tvec4<T, P> const & color);
		
	/// Compute color luminosity associating ratios (0.33, 0.59, 0.11) to RGB canals.
	/// @see gtx_color_space
	template <typename T, precision P>
	GLM_FUNC_DECL T luminosity(
		tvec3<T, P> const & color);

	/// @}
}//namespace glm

#include "color_space.inl"
