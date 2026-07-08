/// @ref gtc_color_space
/// @file glm/gtc/color_space.hpp
///
/// @see core (dependence)
/// @see gtc_color_space (dependence)
///
/// @defgroup gtc_color_space GLM_GTC_color_space
/// @ingroup gtc
///
/// @brief Allow to perform bit operations on integer values
///
/// <glm/gtc/color.hpp> need to be included to use these functionalities.

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/precision.hpp"
#include "../exponential.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"
#include <limits>

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_color_space extension included")
#endif

namespace glm
{
	/// @addtogroup gtc_color_space
	/// @{

	/// Convert a linear color to sRGB color using a standard gamma correction.
	/// IEC 61966-2-1:1999 specification https://www.w3.org/Graphics/Color/srgb
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<T, P> convertLinearToSRGB(vecType<T, P> const & ColorLinear);

	/// Convert a linear color to sRGB color using a custom gamma correction.
	/// IEC 61966-2-1:1999 specification https://www.w3.org/Graphics/Color/srgb
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<T, P> convertLinearToSRGB(vecType<T, P> const & ColorLinear, T Gamma);

	/// Convert a sRGB color to linear color using a standard gamma correction.
	/// IEC 61966-2-1:1999 specification https://www.w3.org/Graphics/Color/srgb
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<T, P> convertSRGBToLinear(vecType<T, P> const & ColorSRGB);

	/// Convert a sRGB color to linear color using a custom gamma correction.
	// IEC 61966-2-1:1999 specification https://www.w3.org/Graphics/Color/srgb
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<T, P> convertSRGBToLinear(vecType<T, P> const & ColorSRGB, T Gamma);

	/// @}
} //namespace glm

#include "color_space.inl"
