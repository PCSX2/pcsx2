/// @ref gtx_matrix_cross_product
/// @file glm/gtx/matrix_cross_product.hpp
///
/// @see core (dependence)
/// @see gtx_extented_min_max (dependence)
///
/// @defgroup gtx_matrix_cross_product GLM_GTX_matrix_cross_product
/// @ingroup gtx
///
/// @brief Build cross product matrices
///
/// <glm/gtx/matrix_cross_product.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_matrix_cross_product extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_matrix_cross_product
	/// @{

	//! Build a cross product matrix.
	//! From GLM_GTX_matrix_cross_product extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> matrixCross3(
		tvec3<T, P> const & x);
		
	//! Build a cross product matrix.
	//! From GLM_GTX_matrix_cross_product extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x4<T, P> matrixCross4(
		tvec3<T, P> const & x);

	/// @}
}//namespace glm

#include "matrix_cross_product.inl"
