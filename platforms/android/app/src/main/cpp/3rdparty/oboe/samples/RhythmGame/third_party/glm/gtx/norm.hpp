/// @ref gtx_norm
/// @file glm/gtx/norm.hpp
///
/// @see core (dependence)
/// @see gtx_quaternion (dependence)
///
/// @defgroup gtx_norm GLM_GTX_norm
/// @ingroup gtx
///
/// @brief Various ways to compute vector norms.
/// 
/// <glm/gtx/norm.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../detail/func_geometric.hpp"
#include "../gtx/quaternion.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_norm extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_norm
	/// @{

	/// Returns the squared length of x.
	/// From GLM_GTX_norm extension.
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL T length2(
		vecType<T, P> const & x);

	/// Returns the squared distance between p0 and p1, i.e., length2(p0 - p1).
	/// From GLM_GTX_norm extension.
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL T distance2(
		vecType<T, P> const & p0,
		vecType<T, P> const & p1);

	//! Returns the L1 norm between x and y.
	//! From GLM_GTX_norm extension.
	template <typename T, precision P>
	GLM_FUNC_DECL T l1Norm(
		tvec3<T, P> const & x,
		tvec3<T, P> const & y);
		
	//! Returns the L1 norm of v.
	//! From GLM_GTX_norm extension.
	template <typename T, precision P>
	GLM_FUNC_DECL T l1Norm(
		tvec3<T, P> const & v);
		
	//! Returns the L2 norm between x and y.
	//! From GLM_GTX_norm extension.
	template <typename T, precision P>
	GLM_FUNC_DECL T l2Norm(
		tvec3<T, P> const & x,
		tvec3<T, P> const & y);
		
	//! Returns the L2 norm of v.
	//! From GLM_GTX_norm extension.
	template <typename T, precision P>
	GLM_FUNC_DECL T l2Norm(
		tvec3<T, P> const & x);
		
	//! Returns the L norm between x and y.
	//! From GLM_GTX_norm extension.
	template <typename T, precision P>
	GLM_FUNC_DECL T lxNorm(
		tvec3<T, P> const & x,
		tvec3<T, P> const & y,
		unsigned int Depth);

	//! Returns the L norm of v.
	//! From GLM_GTX_norm extension.
	template <typename T, precision P>
	GLM_FUNC_DECL T lxNorm(
		tvec3<T, P> const & x,
		unsigned int Depth);

	/// @}
}//namespace glm

#include "norm.inl"
