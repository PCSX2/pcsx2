/// @ref gtx_rotate_vector
/// @file glm/gtx/rotate_vector.hpp
///
/// @see core (dependence)
/// @see gtx_transform (dependence)
///
/// @defgroup gtx_rotate_vector GLM_GTX_rotate_vector
/// @ingroup gtx
///
/// @brief Function to directly rotate a vector
///
/// <glm/gtx/rotate_vector.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtx/transform.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_rotate_vector extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_rotate_vector
	/// @{

	/// Returns Spherical interpolation between two vectors
	/// 
	/// @param x A first vector
	/// @param y A second vector
	/// @param a Interpolation factor. The interpolation is defined beyond the range [0, 1].
	/// 
	/// @see gtx_rotate_vector
	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> slerp(
		tvec3<T, P> const & x,
		tvec3<T, P> const & y,
		T const & a);

	//! Rotate a two dimensional vector.
	//! From GLM_GTX_rotate_vector extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tvec2<T, P> rotate(
		tvec2<T, P> const & v,
		T const & angle);
		
	//! Rotate a three dimensional vector around an axis.
	//! From GLM_GTX_rotate_vector extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> rotate(
		tvec3<T, P> const & v,
		T const & angle,
		tvec3<T, P> const & normal);
		
	//! Rotate a four dimensional vector around an axis.
	//! From GLM_GTX_rotate_vector extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<T, P> rotate(
		tvec4<T, P> const & v,
		T const & angle,
		tvec3<T, P> const & normal);
		
	//! Rotate a three dimensional vector around the X axis.
	//! From GLM_GTX_rotate_vector extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> rotateX(
		tvec3<T, P> const & v,
		T const & angle);

	//! Rotate a three dimensional vector around the Y axis.
	//! From GLM_GTX_rotate_vector extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> rotateY(
		tvec3<T, P> const & v,
		T const & angle);
		
	//! Rotate a three dimensional vector around the Z axis.
	//! From GLM_GTX_rotate_vector extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> rotateZ(
		tvec3<T, P> const & v,
		T const & angle);
		
	//! Rotate a four dimentionnals vector around the X axis.
	//! From GLM_GTX_rotate_vector extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<T, P> rotateX(
		tvec4<T, P> const & v,
		T const & angle);
		
	//! Rotate a four dimensional vector around the X axis.
	//! From GLM_GTX_rotate_vector extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<T, P> rotateY(
		tvec4<T, P> const & v,
		T const & angle);
		
	//! Rotate a four dimensional vector around the X axis.
	//! From GLM_GTX_rotate_vector extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<T, P> rotateZ(
		tvec4<T, P> const & v,
		T const & angle);
		
	//! Build a rotation matrix from a normal and a up vector.
	//! From GLM_GTX_rotate_vector extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x4<T, P> orientation(
		tvec3<T, P> const & Normal,
		tvec3<T, P> const & Up);

	/// @}
}//namespace glm

#include "rotate_vector.inl"
