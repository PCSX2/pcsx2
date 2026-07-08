/// @ref gtx_quaternion
/// @file glm/gtx/quaternion.hpp
///
/// @see core (dependence)
/// @see gtx_extented_min_max (dependence)
///
/// @defgroup gtx_quaternion GLM_GTX_quaternion
/// @ingroup gtx
///
/// @brief Extented quaternion types and functions
///
/// <glm/gtx/quaternion.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtc/constants.hpp"
#include "../gtc/quaternion.hpp"
#include "../gtx/norm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_quaternion extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_quaternion
	/// @{

	/// Compute a cross product between a quaternion and a vector.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> cross(
		tquat<T, P> const & q,
		tvec3<T, P> const & v);

	//! Compute a cross product between a vector and a quaternion.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> cross(
		tvec3<T, P> const & v,
		tquat<T, P> const & q);

	//! Compute a point on a path according squad equation. 
	//! q1 and q2 are control points; s1 and s2 are intermediate control points.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> squad(
		tquat<T, P> const & q1,
		tquat<T, P> const & q2,
		tquat<T, P> const & s1,
		tquat<T, P> const & s2,
		T const & h);

	//! Returns an intermediate control point for squad interpolation.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> intermediate(
		tquat<T, P> const & prev,
		tquat<T, P> const & curr,
		tquat<T, P> const & next);

	//! Returns a exp of a quaternion.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> exp(
		tquat<T, P> const & q);

	//! Returns a log of a quaternion.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> log(
		tquat<T, P> const & q);

	/// Returns x raised to the y power.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> pow(
		tquat<T, P> const & x,
		T const & y);

	//! Returns quarternion square root.
	///
	/// @see gtx_quaternion
	//template<typename T, precision P>
	//tquat<T, P> sqrt(
	//	tquat<T, P> const & q);

	//! Rotates a 3 components vector by a quaternion.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> rotate(
		tquat<T, P> const & q,
		tvec3<T, P> const & v);

	/// Rotates a 4 components vector by a quaternion.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tvec4<T, P> rotate(
		tquat<T, P> const & q,
		tvec4<T, P> const & v);

	/// Extract the real component of a quaternion.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL T extractRealComponent(
		tquat<T, P> const & q);

	/// Converts a quaternion to a 3 * 3 matrix.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> toMat3(
		tquat<T, P> const & x){return mat3_cast(x);}

	/// Converts a quaternion to a 4 * 4 matrix.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tmat4x4<T, P> toMat4(
		tquat<T, P> const & x){return mat4_cast(x);}

	/// Converts a 3 * 3 matrix to a quaternion.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> toQuat(
		tmat3x3<T, P> const & x){return quat_cast(x);}

	/// Converts a 4 * 4 matrix to a quaternion.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> toQuat(
		tmat4x4<T, P> const & x){return quat_cast(x);}

	/// Quaternion interpolation using the rotation short path.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> shortMix(
		tquat<T, P> const & x,
		tquat<T, P> const & y,
		T const & a);

	/// Quaternion normalized linear interpolation.
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> fastMix(
		tquat<T, P> const & x,
		tquat<T, P> const & y,
		T const & a);

	/// Compute the rotation between two vectors.
	/// param orig vector, needs to be normalized
	/// param dest vector, needs to be normalized
	///
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> rotation(
		tvec3<T, P> const & orig, 
		tvec3<T, P> const & dest);

	/// Returns the squared length of x.
	/// 
	/// @see gtx_quaternion
	template<typename T, precision P>
	GLM_FUNC_DECL T length2(tquat<T, P> const & q);

	/// @}
}//namespace glm

#include "quaternion.inl"
