/// @ref gtx_orthonormalize
/// @file glm/gtx/orthonormalize.hpp
///
/// @see core (dependence)
/// @see gtx_extented_min_max (dependence)
///
/// @defgroup gtx_orthonormalize GLM_GTX_orthonormalize
/// @ingroup gtx
///
/// @brief Orthonormalize matrices.
///
/// <glm/gtx/orthonormalize.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../vec3.hpp"
#include "../mat3x3.hpp"
#include "../geometric.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_orthonormalize extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_orthonormalize
	/// @{

	/// Returns the orthonormalized matrix of m.
	///
	/// @see gtx_orthonormalize
	template <typename T, precision P> 
	GLM_FUNC_DECL tmat3x3<T, P> orthonormalize(tmat3x3<T, P> const & m);
		
	/// Orthonormalizes x according y.
	///
	/// @see gtx_orthonormalize
	template <typename T, precision P> 
	GLM_FUNC_DECL tvec3<T, P> orthonormalize(tvec3<T, P> const & x, tvec3<T, P> const & y);

	/// @}
}//namespace glm

#include "orthonormalize.inl"
