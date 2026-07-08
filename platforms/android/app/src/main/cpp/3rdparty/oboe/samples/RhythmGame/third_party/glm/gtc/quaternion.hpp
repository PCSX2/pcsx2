/// @ref gtc_quaternion
/// @file glm/gtc/quaternion.hpp
///
/// @see core (dependence)
/// @see gtc_half_float (dependence)
/// @see gtc_constants (dependence)
///
/// @defgroup gtc_quaternion GLM_GTC_quaternion
/// @ingroup gtc
///
/// @brief Defines a templated quaternion type and several quaternion operations.
///
/// <glm/gtc/quaternion.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../mat3x3.hpp"
#include "../mat4x4.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"
#include "../gtc/constants.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_quaternion extension included")
#endif

namespace glm
{
	/// @addtogroup gtc_quaternion
	/// @{

	template <typename T, precision P = defaultp>
	struct tquat
	{
		// -- Implementation detail --

		typedef tquat<T, P> type;
		typedef T value_type;

		// -- Data --

#		if GLM_HAS_ALIGNED_TYPE
#			if GLM_COMPILER & GLM_COMPILER_GCC
#				pragma GCC diagnostic push
#				pragma GCC diagnostic ignored "-Wpedantic"
#			endif
#			if GLM_COMPILER & GLM_COMPILER_CLANG
#				pragma clang diagnostic push
#				pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#				pragma clang diagnostic ignored "-Wnested-anon-types"
#			endif
		
			union
			{
				struct { T x, y, z, w;};
				typename detail::storage<T, sizeof(T) * 4, detail::is_aligned<P>::value>::type data;
			};
		
#			if GLM_COMPILER & GLM_COMPILER_CLANG
#				pragma clang diagnostic pop
#			endif
#			if GLM_COMPILER & GLM_COMPILER_GCC
#				pragma GCC diagnostic pop
#			endif
#		else
			T x, y, z, w;
#		endif

		// -- Component accesses --

		typedef length_t length_type;
		/// Return the count of components of a quaternion
		GLM_FUNC_DECL static length_type length(){return 4;}

		GLM_FUNC_DECL T & operator[](length_type i);
		GLM_FUNC_DECL T const & operator[](length_type i) const;

		// -- Implicit basic constructors --

		GLM_FUNC_DECL GLM_CONSTEXPR tquat() GLM_DEFAULT_CTOR;
		GLM_FUNC_DECL GLM_CONSTEXPR tquat(tquat<T, P> const & q) GLM_DEFAULT;
		template <precision Q>
		GLM_FUNC_DECL GLM_CONSTEXPR tquat(tquat<T, Q> const & q);

		// -- Explicit basic constructors --

		GLM_FUNC_DECL GLM_CONSTEXPR_CTOR explicit tquat(ctor);
		GLM_FUNC_DECL GLM_CONSTEXPR tquat(T const & s, tvec3<T, P> const & v);
		GLM_FUNC_DECL GLM_CONSTEXPR tquat(T const & w, T const & x, T const & y, T const & z);

		// -- Conversion constructors --

		template <typename U, precision Q>
		GLM_FUNC_DECL GLM_CONSTEXPR GLM_EXPLICIT tquat(tquat<U, Q> const & q);

		/// Explicit conversion operators
#		if GLM_HAS_EXPLICIT_CONVERSION_OPERATORS
			GLM_FUNC_DECL explicit operator tmat3x3<T, P>();
			GLM_FUNC_DECL explicit operator tmat4x4<T, P>();
#		endif

		/// Create a quaternion from two normalized axis
		///
		/// @param u A first normalized axis
		/// @param v A second normalized axis
		/// @see gtc_quaternion
		/// @see http://lolengine.net/blog/2013/09/18/beautiful-maths-quaternion-from-vectors
		GLM_FUNC_DECL tquat(tvec3<T, P> const & u, tvec3<T, P> const & v);

		/// Build a quaternion from euler angles (pitch, yaw, roll), in radians.
		GLM_FUNC_DECL GLM_EXPLICIT tquat(tvec3<T, P> const & eulerAngles);
		GLM_FUNC_DECL GLM_EXPLICIT tquat(tmat3x3<T, P> const & m);
		GLM_FUNC_DECL GLM_EXPLICIT tquat(tmat4x4<T, P> const & m);

		// -- Unary arithmetic operators --

		GLM_FUNC_DECL tquat<T, P> & operator=(tquat<T, P> const & m) GLM_DEFAULT;

		template <typename U>
		GLM_FUNC_DECL tquat<T, P> & operator=(tquat<U, P> const & m);
		template <typename U>
		GLM_FUNC_DECL tquat<T, P> & operator+=(tquat<U, P> const & q);
		template <typename U>
		GLM_FUNC_DECL tquat<T, P> & operator-=(tquat<U, P> const & q);
		template <typename U>
		GLM_FUNC_DECL tquat<T, P> & operator*=(tquat<U, P> const & q);
		template <typename U>
		GLM_FUNC_DECL tquat<T, P> & operator*=(U s);
		template <typename U>
		GLM_FUNC_DECL tquat<T, P> & operator/=(U s);
	};

	// -- Unary bit operators --

	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> operator+(tquat<T, P> const & q);

	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> operator-(tquat<T, P> const & q);

	// -- Binary operators --

	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> operator+(tquat<T, P> const & q, tquat<T, P> const & p);

	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> operator*(tquat<T, P> const & q, tquat<T, P> const & p);

	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> operator*(tquat<T, P> const & q, tvec3<T, P> const & v);

	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> operator*(tvec3<T, P> const & v, tquat<T, P> const & q);

	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<T, P> operator*(tquat<T, P> const & q, tvec4<T, P> const & v);

	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<T, P> operator*(tvec4<T, P> const & v, tquat<T, P> const & q);

	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> operator*(tquat<T, P> const & q, T const & s);

	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> operator*(T const & s, tquat<T, P> const & q);

	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> operator/(tquat<T, P> const & q, T const & s);

	// -- Boolean operators --

	template <typename T, precision P>
	GLM_FUNC_DECL bool operator==(tquat<T, P> const & q1, tquat<T, P> const & q2);

	template <typename T, precision P>
	GLM_FUNC_DECL bool operator!=(tquat<T, P> const & q1, tquat<T, P> const & q2);

	/// Returns the length of the quaternion.
	/// 
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL T length(tquat<T, P> const & q);

	/// Returns the normalized quaternion.
	/// 
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> normalize(tquat<T, P> const & q);
		
	/// Returns dot product of q1 and q2, i.e., q1[0] * q2[0] + q1[1] * q2[1] + ...
	/// 
	/// @see gtc_quaternion
	template <typename T, precision P, template <typename, precision> class quatType>
	GLM_FUNC_DECL T dot(quatType<T, P> const & x, quatType<T, P> const & y);

	/// Spherical linear interpolation of two quaternions.
	/// The interpolation is oriented and the rotation is performed at constant speed.
	/// For short path spherical linear interpolation, use the slerp function.
	/// 
	/// @param x A quaternion
	/// @param y A quaternion
	/// @param a Interpolation factor. The interpolation is defined beyond the range [0, 1].
	/// @tparam T Value type used to build the quaternion. Supported: half, float or double.
	/// @see gtc_quaternion
	/// @see - slerp(tquat<T, P> const & x, tquat<T, P> const & y, T const & a)
	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> mix(tquat<T, P> const & x, tquat<T, P> const & y, T a);

	/// Linear interpolation of two quaternions.
	/// The interpolation is oriented.
	/// 
	/// @param x A quaternion
	/// @param y A quaternion
	/// @param a Interpolation factor. The interpolation is defined in the range [0, 1].
	/// @tparam T Value type used to build the quaternion. Supported: half, float or double.
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> lerp(tquat<T, P> const & x, tquat<T, P> const & y, T a);

	/// Spherical linear interpolation of two quaternions.
	/// The interpolation always take the short path and the rotation is performed at constant speed.
	/// 
	/// @param x A quaternion
	/// @param y A quaternion
	/// @param a Interpolation factor. The interpolation is defined beyond the range [0, 1].
	/// @tparam T Value type used to build the quaternion. Supported: half, float or double.
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> slerp(tquat<T, P> const & x, tquat<T, P> const & y, T a);

	/// Returns the q conjugate.
	/// 
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> conjugate(tquat<T, P> const & q);

	/// Returns the q inverse.
	/// 
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> inverse(tquat<T, P> const & q);

	/// Rotates a quaternion from a vector of 3 components axis and an angle.
	/// 
	/// @param q Source orientation
	/// @param angle Angle expressed in radians.
	/// @param axis Axis of the rotation
	/// 
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> rotate(tquat<T, P> const & q, T const & angle, tvec3<T, P> const & axis);

	/// Returns euler angles, pitch as x, yaw as y, roll as z.
	/// The result is expressed in radians if GLM_FORCE_RADIANS is defined or degrees otherwise.
	/// 
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> eulerAngles(tquat<T, P> const & x);

	/// Returns roll value of euler angles expressed in radians.
	///
	/// @see gtx_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL T roll(tquat<T, P> const & x);

	/// Returns pitch value of euler angles expressed in radians.
	///
	/// @see gtx_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL T pitch(tquat<T, P> const & x);

	/// Returns yaw value of euler angles expressed in radians.
	///
	/// @see gtx_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL T yaw(tquat<T, P> const & x);

	/// Converts a quaternion to a 3 * 3 matrix.
	/// 
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> mat3_cast(tquat<T, P> const & x);

	/// Converts a quaternion to a 4 * 4 matrix.
	/// 
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x4<T, P> mat4_cast(tquat<T, P> const & x);

	/// Converts a 3 * 3 matrix to a quaternion.
	/// 
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> quat_cast(tmat3x3<T, P> const & x);

	/// Converts a 4 * 4 matrix to a quaternion.
	/// 
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> quat_cast(tmat4x4<T, P> const & x);

	/// Returns the quaternion rotation angle.
	///
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL T angle(tquat<T, P> const & x);

	/// Returns the q rotation axis.
	///
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> axis(tquat<T, P> const & x);

	/// Build a quaternion from an angle and a normalized axis.
	///
	/// @param angle Angle expressed in radians.
	/// @param axis Axis of the quaternion, must be normalized.
	///
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tquat<T, P> angleAxis(T const & angle, tvec3<T, P> const & axis);

	/// Returns the component-wise comparison result of x < y.
	/// 
	/// @tparam quatType Floating-point quaternion types.
	///
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<bool, P> lessThan(tquat<T, P> const & x, tquat<T, P> const & y);

	/// Returns the component-wise comparison of result x <= y.
	///
	/// @tparam quatType Floating-point quaternion types.
	///
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<bool, P> lessThanEqual(tquat<T, P> const & x, tquat<T, P> const & y);

	/// Returns the component-wise comparison of result x > y.
	///
	/// @tparam quatType Floating-point quaternion types.
	///
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<bool, P> greaterThan(tquat<T, P> const & x, tquat<T, P> const & y);

	/// Returns the component-wise comparison of result x >= y.
	///
	/// @tparam quatType Floating-point quaternion types.
	///
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<bool, P> greaterThanEqual(tquat<T, P> const & x, tquat<T, P> const & y);

	/// Returns the component-wise comparison of result x == y.
	///
	/// @tparam quatType Floating-point quaternion types.
	///
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<bool, P> equal(tquat<T, P> const & x, tquat<T, P> const & y);

	/// Returns the component-wise comparison of result x != y.
	/// 
	/// @tparam quatType Floating-point quaternion types.
	///
	/// @see gtc_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<bool, P> notEqual(tquat<T, P> const & x, tquat<T, P> const & y);

	/// Returns true if x holds a NaN (not a number)
	/// representation in the underlying implementation's set of
	/// floating point representations. Returns false otherwise,
	/// including for implementations with no NaN
	/// representations.
	/// 
	/// /!\ When using compiler fast math, this function may fail.
	/// 
	/// @tparam genType Floating-point scalar or vector types.
	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<bool, P> isnan(tquat<T, P> const & x);

	/// Returns true if x holds a positive infinity or negative
	/// infinity representation in the underlying implementation's
	/// set of floating point representations. Returns false
	/// otherwise, including for implementations with no infinity
	/// representations.
	/// 
	/// @tparam genType Floating-point scalar or vector types.
	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<bool, P> isinf(tquat<T, P> const & x);

	/// @}
} //namespace glm

#include "quaternion.inl"
