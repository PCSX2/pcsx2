/// @ref gtx_vector_angle
/// @file glm/gtx/vector_angle.hpp
///
/// @see core (dependence)
/// @see gtx_quaternion (dependence)
/// @see gtx_epsilon (dependence)
///
/// @defgroup gtx_vector_angle GLM_GTX_vector_angle
/// @ingroup gtx
///
/// @brief Compute angle between vectors
///
/// <glm/gtx/vector_angle.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtc/epsilon.hpp"
#include "../gtx/quaternion.hpp"
#include "../gtx/rotate_vector.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_vector_angle extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_vector_angle
	/// @{

	//! Returns the absolute angle between two vectors.
	//! Parameters need to be normalized.
	/// @see gtx_vector_angle extension.
	template <typename vecType>
	GLM_FUNC_DECL typename vecType::value_type angle(
		vecType const & x, 
		vecType const & y);

	//! Returns the oriented angle between two 2d vectors.
	//! Parameters need to be normalized.
	/// @see gtx_vector_angle extension.
	template <typename T, precision P>
	GLM_FUNC_DECL T orientedAngle(
		tvec2<T, P> const & x,
		tvec2<T, P> const & y);

	//! Returns the oriented angle between two 3d vectors based from a reference axis.
	//! Parameters need to be normalized.
	/// @see gtx_vector_angle extension.
	template <typename T, precision P>
	GLM_FUNC_DECL T orientedAngle(
		tvec3<T, P> const & x,
		tvec3<T, P> const & y,
		tvec3<T, P> const & ref);

	/// @}
}// namespace glm

#include "vector_angle.inl"
