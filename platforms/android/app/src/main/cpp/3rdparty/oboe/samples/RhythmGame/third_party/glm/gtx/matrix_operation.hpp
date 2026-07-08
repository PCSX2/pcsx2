/// @ref gtx_matrix_operation
/// @file glm/gtx/matrix_operation.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_matrix_operation GLM_GTX_matrix_operation
/// @ingroup gtx
///
/// @brief Build diagonal matrices from vectors.
///
/// <glm/gtx/matrix_operation.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_matrix_operation extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_matrix_operation
	/// @{

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> diagonal2x2(
		tvec2<T, P> const & v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x3<T, P> diagonal2x3(
		tvec2<T, P> const & v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x4<T, P> diagonal2x4(
		tvec2<T, P> const & v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x2<T, P> diagonal3x2(
		tvec2<T, P> const & v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> diagonal3x3(
		tvec3<T, P> const & v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x4<T, P> diagonal3x4(
		tvec3<T, P> const & v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x2<T, P> diagonal4x2(
		tvec2<T, P> const & v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x3<T, P> diagonal4x3(
		tvec3<T, P> const & v);

	//! Build a diagonal matrix.
	//! From GLM_GTX_matrix_operation extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x4<T, P> diagonal4x4(
		tvec4<T, P> const & v);

	/// @}
}//namespace glm

#include "matrix_operation.inl"
