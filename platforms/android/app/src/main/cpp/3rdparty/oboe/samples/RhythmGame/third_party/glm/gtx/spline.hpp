/// @ref gtx_spline
/// @file glm/gtx/spline.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_spline GLM_GTX_spline
/// @ingroup gtx
///
/// @brief Spline functions
///
/// <glm/gtx/spline.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtx/optimum_pow.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_spline extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_spline
	/// @{

	/// Return a point from a catmull rom curve.
	/// @see gtx_spline extension.
	template <typename genType> 
	GLM_FUNC_DECL genType catmullRom(
		genType const & v1, 
		genType const & v2, 
		genType const & v3, 
		genType const & v4, 
		typename genType::value_type const & s);
		
	/// Return a point from a hermite curve.
	/// @see gtx_spline extension.
	template <typename genType> 
	GLM_FUNC_DECL genType hermite(
		genType const & v1, 
		genType const & t1, 
		genType const & v2, 
		genType const & t2, 
		typename genType::value_type const & s);
		
	/// Return a point from a cubic curve. 
	/// @see gtx_spline extension.
	template <typename genType> 
	GLM_FUNC_DECL genType cubic(
		genType const & v1, 
		genType const & v2, 
		genType const & v3, 
		genType const & v4, 
		typename genType::value_type const & s);

	/// @}
}//namespace glm

#include "spline.inl"
