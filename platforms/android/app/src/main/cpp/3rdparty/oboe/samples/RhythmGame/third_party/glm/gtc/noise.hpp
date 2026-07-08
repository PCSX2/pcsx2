/// @ref gtc_noise
/// @file glm/gtc/noise.hpp
///
/// @see core (dependence)
///
/// @defgroup gtc_noise GLM_GTC_noise
/// @ingroup gtc
///
/// Defines 2D, 3D and 4D procedural noise functions 
/// Based on the work of Stefan Gustavson and Ashima Arts on "webgl-noise": 
/// https://github.com/ashima/webgl-noise 
/// Following Stefan Gustavson's paper "Simplex noise demystified": 
/// http://www.itn.liu.se/~stegu/simplexnoise/simplexnoise.pdf
/// <glm/gtc/noise.hpp> need to be included to use these functionalities.

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/precision.hpp"
#include "../detail/_noise.hpp"
#include "../geometric.hpp"
#include "../common.hpp"
#include "../vector_relational.hpp"
#include "../vec2.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_noise extension included")
#endif

namespace glm
{
	/// @addtogroup gtc_noise
	/// @{

	/// Classic perlin noise.
	/// @see gtc_noise
	template <typename T, precision P, template<typename, precision> class vecType>
	GLM_FUNC_DECL T perlin(
		vecType<T, P> const & p);
		
	/// Periodic perlin noise.
	/// @see gtc_noise
	template <typename T, precision P, template<typename, precision> class vecType>
	GLM_FUNC_DECL T perlin(
		vecType<T, P> const & p,
		vecType<T, P> const & rep);

	/// Simplex noise.
	/// @see gtc_noise
	template <typename T, precision P, template<typename, precision> class vecType>
	GLM_FUNC_DECL T simplex(
		vecType<T, P> const & p);

	/// @}
}//namespace glm

#include "noise.inl"
