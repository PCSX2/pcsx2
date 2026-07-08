/// @ref gtx_intersect
/// @file glm/gtx/intersect.hpp
///
/// @see core (dependence)
/// @see gtx_closest_point (dependence)
///
/// @defgroup gtx_intersect GLM_GTX_intersect
/// @ingroup gtx
///
/// @brief Add intersection functions
///
/// <glm/gtx/intersect.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include <cfloat>
#include <limits>
#include "../glm.hpp"
#include "../geometric.hpp"
#include "../gtx/closest_point.hpp"
#include "../gtx/vector_query.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_closest_point extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_intersect
	/// @{

	//! Compute the intersection of a ray and a plane.
	//! Ray direction and plane normal must be unit length.
	//! From GLM_GTX_intersect extension.
	template <typename genType>
	GLM_FUNC_DECL bool intersectRayPlane(
		genType const & orig, genType const & dir,
		genType const & planeOrig, genType const & planeNormal,
		typename genType::value_type & intersectionDistance);

	//! Compute the intersection of a ray and a triangle.
	//! From GLM_GTX_intersect extension.
	template <typename genType>
	GLM_FUNC_DECL bool intersectRayTriangle(
		genType const & orig, genType const & dir,
		genType const & vert0, genType const & vert1, genType const & vert2,
		genType & baryPosition);

	//! Compute the intersection of a line and a triangle.
	//! From GLM_GTX_intersect extension.
	template <typename genType>
	GLM_FUNC_DECL bool intersectLineTriangle(
		genType const & orig, genType const & dir,
		genType const & vert0, genType const & vert1, genType const & vert2,
		genType & position);

	//! Compute the intersection distance of a ray and a sphere. 
	//! The ray direction vector is unit length.
	//! From GLM_GTX_intersect extension.
	template <typename genType>
	GLM_FUNC_DECL bool intersectRaySphere(
		genType const & rayStarting, genType const & rayNormalizedDirection,
		genType const & sphereCenter, typename genType::value_type const sphereRadiusSquered,
		typename genType::value_type & intersectionDistance);

	//! Compute the intersection of a ray and a sphere.
	//! From GLM_GTX_intersect extension.
	template <typename genType>
	GLM_FUNC_DECL bool intersectRaySphere(
		genType const & rayStarting, genType const & rayNormalizedDirection,
		genType const & sphereCenter, const typename genType::value_type sphereRadius,
		genType & intersectionPosition, genType & intersectionNormal);

	//! Compute the intersection of a line and a sphere.
	//! From GLM_GTX_intersect extension
	template <typename genType>
	GLM_FUNC_DECL bool intersectLineSphere(
		genType const & point0, genType const & point1,
		genType const & sphereCenter, typename genType::value_type sphereRadius,
		genType & intersectionPosition1, genType & intersectionNormal1, 
		genType & intersectionPosition2 = genType(), genType & intersectionNormal2 = genType());

	/// @}
}//namespace glm

#include "intersect.inl"
