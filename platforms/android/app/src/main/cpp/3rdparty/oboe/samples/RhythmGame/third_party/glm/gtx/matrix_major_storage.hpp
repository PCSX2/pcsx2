/// @ref gtx_matrix_major_storage
/// @file glm/gtx/matrix_major_storage.hpp
///
/// @see core (dependence)
/// @see gtx_extented_min_max (dependence)
///
/// @defgroup gtx_matrix_major_storage GLM_GTX_matrix_major_storage
/// @ingroup gtx
///
/// @brief Build matrices with specific matrix order, row or column
///
/// <glm/gtx/matrix_major_storage.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_matrix_major_storage extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_matrix_major_storage
	/// @{

	//! Build a row major matrix from row vectors.
	//! From GLM_GTX_matrix_major_storage extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> rowMajor2(
		tvec2<T, P> const & v1, 
		tvec2<T, P> const & v2);
		
	//! Build a row major matrix from other matrix.
	//! From GLM_GTX_matrix_major_storage extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> rowMajor2(
		tmat2x2<T, P> const & m);

	//! Build a row major matrix from row vectors.
	//! From GLM_GTX_matrix_major_storage extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> rowMajor3(
		tvec3<T, P> const & v1, 
		tvec3<T, P> const & v2, 
		tvec3<T, P> const & v3);

	//! Build a row major matrix from other matrix.
	//! From GLM_GTX_matrix_major_storage extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> rowMajor3(
		tmat3x3<T, P> const & m);

	//! Build a row major matrix from row vectors.
	//! From GLM_GTX_matrix_major_storage extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x4<T, P> rowMajor4(
		tvec4<T, P> const & v1, 
		tvec4<T, P> const & v2,
		tvec4<T, P> const & v3, 
		tvec4<T, P> const & v4);

	//! Build a row major matrix from other matrix.
	//! From GLM_GTX_matrix_major_storage extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x4<T, P> rowMajor4(
		tmat4x4<T, P> const & m);

	//! Build a column major matrix from column vectors.
	//! From GLM_GTX_matrix_major_storage extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> colMajor2(
		tvec2<T, P> const & v1, 
		tvec2<T, P> const & v2);
		
	//! Build a column major matrix from other matrix.
	//! From GLM_GTX_matrix_major_storage extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> colMajor2(
		tmat2x2<T, P> const & m);

	//! Build a column major matrix from column vectors.
	//! From GLM_GTX_matrix_major_storage extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> colMajor3(
		tvec3<T, P> const & v1, 
		tvec3<T, P> const & v2, 
		tvec3<T, P> const & v3);
		
	//! Build a column major matrix from other matrix.
	//! From GLM_GTX_matrix_major_storage extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> colMajor3(
		tmat3x3<T, P> const & m);
		
	//! Build a column major matrix from column vectors.
	//! From GLM_GTX_matrix_major_storage extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x4<T, P> colMajor4(
		tvec4<T, P> const & v1, 
		tvec4<T, P> const & v2, 
		tvec4<T, P> const & v3, 
		tvec4<T, P> const & v4);
				
	//! Build a column major matrix from other matrix.
	//! From GLM_GTX_matrix_major_storage extension.
	template <typename T, precision P> 
	GLM_FUNC_DECL tmat4x4<T, P> colMajor4(
		tmat4x4<T, P> const & m);

	/// @}
}//namespace glm

#include "matrix_major_storage.inl"
