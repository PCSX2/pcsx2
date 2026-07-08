/// @ref gtc_random
/// @file glm/gtc/random.hpp
///
/// @see core (dependence)
/// @see gtc_half_float (dependence)
/// @see gtx_random (extended)
///
/// @defgroup gtc_random GLM_GTC_random
/// @ingroup gtc
///
/// @brief Generate random number from various distribution methods.
///
/// <glm/gtc/random.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../vec2.hpp"
#include "../vec3.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_random extension included")
#endif

namespace glm
{
	/// @addtogroup gtc_random
	/// @{
	
	/// Generate random numbers in the interval [Min, Max], according a linear distribution 
	/// 
	/// @param Min 
	/// @param Max 
	/// @tparam genType Value type. Currently supported: float or double scalars.
	/// @see gtc_random
	template <typename genTYpe>
	GLM_FUNC_DECL genTYpe linearRand(
		genTYpe Min,
		genTYpe Max);

	/// Generate random numbers in the interval [Min, Max], according a linear distribution 
	/// 
	/// @param Min 
	/// @param Max 
	/// @tparam T Value type. Currently supported: float or double.
	/// @tparam vecType A vertor type: tvec1, tvec2, tvec3, tvec4 or compatible
	/// @see gtc_random
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<T, P> linearRand(
		vecType<T, P> const & Min,
		vecType<T, P> const & Max);

	/// Generate random numbers in the interval [Min, Max], according a gaussian distribution 
	/// 
	/// @param Mean
	/// @param Deviation
	/// @see gtc_random
	template <typename genType>
	GLM_FUNC_DECL genType gaussRand(
		genType Mean,
		genType Deviation);
	
	/// Generate a random 2D vector which coordinates are regulary distributed on a circle of a given radius
	/// 
	/// @param Radius 
	/// @see gtc_random
	template <typename T>
	GLM_FUNC_DECL tvec2<T, defaultp> circularRand(
		T Radius);
	
	/// Generate a random 3D vector which coordinates are regulary distributed on a sphere of a given radius
	/// 
	/// @param Radius
	/// @see gtc_random
	template <typename T>
	GLM_FUNC_DECL tvec3<T, defaultp> sphericalRand(
		T Radius);
	
	/// Generate a random 2D vector which coordinates are regulary distributed within the area of a disk of a given radius
	/// 
	/// @param Radius
	/// @see gtc_random
	template <typename T>
	GLM_FUNC_DECL tvec2<T, defaultp> diskRand(
		T Radius);
	
	/// Generate a random 3D vector which coordinates are regulary distributed within the volume of a ball of a given radius
	/// 
	/// @param Radius
	/// @see gtc_random
	template <typename T>
	GLM_FUNC_DECL tvec3<T, defaultp> ballRand(
		T Radius);
	
	/// @}
}//namespace glm

#include "random.inl"
