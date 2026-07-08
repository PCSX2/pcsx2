/// @ref gtx_gradient_paint
/// @file glm/gtx/gradient_paint.hpp
///
/// @see core (dependence)
/// @see gtx_optimum_pow (dependence)
///
/// @defgroup gtx_gradient_paint GLM_GTX_gradient_paint
/// @ingroup gtx
///
/// @brief Functions that return the color of procedural gradient for specific coordinates.
/// <glm/gtx/gradient_paint.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtx/optimum_pow.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_gradient_paint extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_gradient_paint
	/// @{

	/// Return a color from a radial gradient.
	/// @see - gtx_gradient_paint
	template <typename T, precision P>
	GLM_FUNC_DECL T radialGradient(
		tvec2<T, P> const & Center,
		T const & Radius,
		tvec2<T, P> const & Focal,
		tvec2<T, P> const & Position);

	/// Return a color from a linear gradient.
	/// @see - gtx_gradient_paint
	template <typename T, precision P>
	GLM_FUNC_DECL T linearGradient(
		tvec2<T, P> const & Point0,
		tvec2<T, P> const & Point1,
		tvec2<T, P> const & Position);

	/// @}
}// namespace glm

#include "gradient_paint.inl"
