/// @ref gtx_matrix_interpolation
/// @file glm/gtx/matrix_interpolation.hpp
/// @author Ghenadii Ursachi (the.asteroth@gmail.com)
///
/// @see core (dependence)
///
/// @defgroup gtx_matrix_interpolation GLM_GTX_matrix_interpolation
/// @ingroup gtx
///
/// @brief Allows to directly interpolate two exiciting matrices.
///
/// <glm/gtx/matrix_interpolation.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_matrix_interpolation extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_matrix_interpolation
	/// @{

	/// Get the axis and angle of the rotation from a matrix.
	/// From GLM_GTX_matrix_interpolation extension.
	template <typename T, precision P>
	GLM_FUNC_DECL void axisAngle(
		tmat4x4<T, P> const & mat,
		tvec3<T, P> & axis,
		T & angle);

	/// Build a matrix from axis and angle.
	/// From GLM_GTX_matrix_interpolation extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x4<T, P> axisAngleMatrix(
		tvec3<T, P> const & axis,
		T const angle);

	/// Extracts the rotation part of a matrix.
	/// From GLM_GTX_matrix_interpolation extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x4<T, P> extractMatrixRotation(
		tmat4x4<T, P> const & mat);

	/// Build a interpolation of 4 * 4 matrixes.
	/// From GLM_GTX_matrix_interpolation extension.
	/// Warning! works only with rotation and/or translation matrixes, scale will generate unexpected results.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x4<T, P> interpolate(
		tmat4x4<T, P> const & m1,
		tmat4x4<T, P> const & m2,
		T const delta);

	/// @}
}//namespace glm

#include "matrix_interpolation.inl"
