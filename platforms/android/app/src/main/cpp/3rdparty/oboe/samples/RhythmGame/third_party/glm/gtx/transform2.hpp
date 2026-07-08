/// @ref gtx_transform2
/// @file glm/gtx/transform2.hpp
///
/// @see core (dependence)
/// @see gtx_transform (dependence)
///
/// @defgroup gtx_transform2 GLM_GTX_transform2
/// @ingroup gtx
///
/// @brief Add extra transformation matrices
///
/// <glm/gtx/transform2.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtx/transform.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_transform2 extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_transform2
	/// @{

	//! Transforms a matrix with a shearing on X axis.
	//! From GLM_GTX_transform2 extension.
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> shearX2D(
		tmat3x3<T, P> const & m, 
		T y);

	//! Transforms a matrix with a shearing on Y axis.
	//! From GLM_GTX_transform2 extension.
	template <typename T, precision P> 
	GLM_FUNC_DECL tmat3x3<T, P> shearY2D(
		tmat3x3<T, P> const & m, 
		T x);

	//! Transforms a matrix with a shearing on X axis
	//! From GLM_GTX_transform2 extension.
	template <typename T, precision P> 
	GLM_FUNC_DECL tmat4x4<T, P> shearX3D(
		const tmat4x4<T, P> & m,
		T y, 
		T z);

	//! Transforms a matrix with a shearing on Y axis.
	//! From GLM_GTX_transform2 extension.
	template <typename T, precision P> 
	GLM_FUNC_DECL tmat4x4<T, P> shearY3D(
		const tmat4x4<T, P> & m, 
		T x, 
		T z);

	//! Transforms a matrix with a shearing on Z axis. 
	//! From GLM_GTX_transform2 extension.
	template <typename T, precision P> 
	GLM_FUNC_DECL tmat4x4<T, P> shearZ3D(
		const tmat4x4<T, P> & m, 
		T x, 
		T y);

	//template <typename T> GLM_FUNC_QUALIFIER tmat4x4<T, P> shear(const tmat4x4<T, P> & m, shearPlane, planePoint, angle)
	// Identity + tan(angle) * cross(Normal, OnPlaneVector)     0
	// - dot(PointOnPlane, normal) * OnPlaneVector              1

	// Reflect functions seem to don't work
	//template <typename T> tmat3x3<T, P> reflect2D(const tmat3x3<T, P> & m, const tvec3<T, P>& normal){return reflect2DGTX(m, normal);}									//!< \brief Build a reflection matrix (from GLM_GTX_transform2 extension)
	//template <typename T> tmat4x4<T, P> reflect3D(const tmat4x4<T, P> & m, const tvec3<T, P>& normal){return reflect3DGTX(m, normal);}									//!< \brief Build a reflection matrix (from GLM_GTX_transform2 extension)
		
	//! Build planar projection matrix along normal axis.
	//! From GLM_GTX_transform2 extension.
	template <typename T, precision P> 
	GLM_FUNC_DECL tmat3x3<T, P> proj2D(
		const tmat3x3<T, P> & m, 
		const tvec3<T, P>& normal);

	//! Build planar projection matrix along normal axis.
	//! From GLM_GTX_transform2 extension.
	template <typename T, precision P> 
	GLM_FUNC_DECL tmat4x4<T, P> proj3D(
		const tmat4x4<T, P> & m, 
		const tvec3<T, P>& normal);

	//! Build a scale bias matrix. 
	//! From GLM_GTX_transform2 extension.
	template <typename valType, precision P> 
	GLM_FUNC_DECL tmat4x4<valType, P> scaleBias(
		valType scale, 
		valType bias);

	//! Build a scale bias matrix.
	//! From GLM_GTX_transform2 extension.
	template <typename valType, precision P> 
	GLM_FUNC_DECL tmat4x4<valType, P> scaleBias(
		tmat4x4<valType, P> const & m, 
		valType scale, 
		valType bias);

	/// @}
}// namespace glm

#include "transform2.inl"
