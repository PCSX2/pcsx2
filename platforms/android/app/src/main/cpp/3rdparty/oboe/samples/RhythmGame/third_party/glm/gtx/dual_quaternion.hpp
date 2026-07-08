/// @ref gtx_dual_quaternion
/// @file glm/gtx/dual_quaternion.hpp
/// @author Maksim Vorobiev (msomeone@gmail.com)
///
/// @see core (dependence)
/// @see gtc_half_float (dependence)
/// @see gtc_constants (dependence)
/// @see gtc_quaternion (dependence)
///
/// @defgroup gtx_dual_quaternion GLM_GTX_dual_quaternion
/// @ingroup gtx
///
/// @brief Defines a templated dual-quaternion type and several dual-quaternion operations.
///
/// <glm/gtx/dual_quaternion.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtc/constants.hpp"
#include "../gtc/quaternion.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_dual_quaternion extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_dual_quaternion
	/// @{

	template <typename T, precision P = defaultp>
	struct tdualquat
	{
		// -- Implementation detail --

		typedef T value_type;
		typedef glm::tquat<T, P> part_type;

		// -- Data --

		glm::tquat<T, P> real, dual;

		// -- Component accesses --

		typedef length_t length_type;
		/// Return the count of components of a dual quaternion
		GLM_FUNC_DECL static length_type length(){return 2;}

		GLM_FUNC_DECL part_type & operator[](length_type i);
		GLM_FUNC_DECL part_type const & operator[](length_type i) const;

		// -- Implicit basic constructors --

		GLM_FUNC_DECL GLM_CONSTEXPR tdualquat() GLM_DEFAULT_CTOR;
		GLM_FUNC_DECL GLM_CONSTEXPR tdualquat(tdualquat<T, P> const & d) GLM_DEFAULT;
		template <precision Q>
		GLM_FUNC_DECL GLM_CONSTEXPR tdualquat(tdualquat<T, Q> const & d);

		// -- Explicit basic constructors --

		GLM_FUNC_DECL GLM_CONSTEXPR_CTOR explicit tdualquat(ctor);
		GLM_FUNC_DECL GLM_CONSTEXPR tdualquat(tquat<T, P> const & real);
		GLM_FUNC_DECL GLM_CONSTEXPR tdualquat(tquat<T, P> const & orientation, tvec3<T, P> const & translation);
		GLM_FUNC_DECL GLM_CONSTEXPR tdualquat(tquat<T, P> const & real, tquat<T, P> const & dual);

		// -- Conversion constructors --

		template <typename U, precision Q>
		GLM_FUNC_DECL GLM_CONSTEXPR GLM_EXPLICIT tdualquat(tdualquat<U, Q> const & q);

		GLM_FUNC_DECL GLM_EXPLICIT tdualquat(tmat2x4<T, P> const & holder_mat);
		GLM_FUNC_DECL GLM_EXPLICIT tdualquat(tmat3x4<T, P> const & aug_mat);

		// -- Unary arithmetic operators --

		GLM_FUNC_DECL tdualquat<T, P> & operator=(tdualquat<T, P> const & m) GLM_DEFAULT;

		template <typename U>
		GLM_FUNC_DECL tdualquat<T, P> & operator=(tdualquat<U, P> const & m);
		template <typename U>
		GLM_FUNC_DECL tdualquat<T, P> & operator*=(U s);
		template <typename U>
		GLM_FUNC_DECL tdualquat<T, P> & operator/=(U s);
	};

	// -- Unary bit operators --

	template <typename T, precision P>
	GLM_FUNC_DECL tdualquat<T, P> operator+(tdualquat<T, P> const & q);

	template <typename T, precision P>
	GLM_FUNC_DECL tdualquat<T, P> operator-(tdualquat<T, P> const & q);

	// -- Binary operators --

	template <typename T, precision P>
	GLM_FUNC_DECL tdualquat<T, P> operator+(tdualquat<T, P> const & q, tdualquat<T, P> const & p);

	template <typename T, precision P>
	GLM_FUNC_DECL tdualquat<T, P> operator*(tdualquat<T, P> const & q, tdualquat<T, P> const & p);

	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> operator*(tdualquat<T, P> const & q, tvec3<T, P> const & v);

	template <typename T, precision P>
	GLM_FUNC_DECL tvec3<T, P> operator*(tvec3<T, P> const & v, tdualquat<T, P> const & q);

	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<T, P> operator*(tdualquat<T, P> const & q, tvec4<T, P> const & v);

	template <typename T, precision P>
	GLM_FUNC_DECL tvec4<T, P> operator*(tvec4<T, P> const & v, tdualquat<T, P> const & q);

	template <typename T, precision P>
	GLM_FUNC_DECL tdualquat<T, P> operator*(tdualquat<T, P> const & q, T const & s);

	template <typename T, precision P>
	GLM_FUNC_DECL tdualquat<T, P> operator*(T const & s, tdualquat<T, P> const & q);

	template <typename T, precision P>
	GLM_FUNC_DECL tdualquat<T, P> operator/(tdualquat<T, P> const & q, T const & s);

	// -- Boolean operators --

	template <typename T, precision P>
	GLM_FUNC_DECL bool operator==(tdualquat<T, P> const & q1, tdualquat<T, P> const & q2);

	template <typename T, precision P>
	GLM_FUNC_DECL bool operator!=(tdualquat<T, P> const & q1, tdualquat<T, P> const & q2);

	/// Returns the normalized quaternion.
	///
	/// @see gtx_dual_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tdualquat<T, P> normalize(tdualquat<T, P> const & q);

	/// Returns the linear interpolation of two dual quaternion.
	///
	/// @see gtc_dual_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tdualquat<T, P> lerp(tdualquat<T, P> const & x, tdualquat<T, P> const & y, T const & a);

	/// Returns the q inverse.
	///
	/// @see gtx_dual_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tdualquat<T, P> inverse(tdualquat<T, P> const & q);

	/// Converts a quaternion to a 2 * 4 matrix.
	///
	/// @see gtx_dual_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x4<T, P> mat2x4_cast(tdualquat<T, P> const & x);

	/// Converts a quaternion to a 3 * 4 matrix.
	///
	/// @see gtx_dual_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x4<T, P> mat3x4_cast(tdualquat<T, P> const & x);

	/// Converts a 2 * 4 matrix (matrix which holds real and dual parts) to a quaternion.
	///
	/// @see gtx_dual_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tdualquat<T, P> dualquat_cast(tmat2x4<T, P> const & x);

	/// Converts a 3 * 4 matrix (augmented matrix rotation + translation) to a quaternion.
	///
	/// @see gtx_dual_quaternion
	template <typename T, precision P>
	GLM_FUNC_DECL tdualquat<T, P> dualquat_cast(tmat3x4<T, P> const & x);


	/// Dual-quaternion of low single-precision floating-point numbers.
	///
	/// @see gtx_dual_quaternion
	typedef tdualquat<float, lowp>		lowp_dualquat;

	/// Dual-quaternion of medium single-precision floating-point numbers.
	///
	/// @see gtx_dual_quaternion
	typedef tdualquat<float, mediump>	mediump_dualquat;

	/// Dual-quaternion of high single-precision floating-point numbers.
	///
	/// @see gtx_dual_quaternion
	typedef tdualquat<float, highp>		highp_dualquat;


	/// Dual-quaternion of low single-precision floating-point numbers.
	///
	/// @see gtx_dual_quaternion
	typedef tdualquat<float, lowp>		lowp_fdualquat;

	/// Dual-quaternion of medium single-precision floating-point numbers.
	///
	/// @see gtx_dual_quaternion
	typedef tdualquat<float, mediump>	mediump_fdualquat;

	/// Dual-quaternion of high single-precision floating-point numbers.
	///
	/// @see gtx_dual_quaternion
	typedef tdualquat<float, highp>		highp_fdualquat;


	/// Dual-quaternion of low double-precision floating-point numbers.
	///
	/// @see gtx_dual_quaternion
	typedef tdualquat<double, lowp>		lowp_ddualquat;

	/// Dual-quaternion of medium double-precision floating-point numbers.
	///
	/// @see gtx_dual_quaternion
	typedef tdualquat<double, mediump>	mediump_ddualquat;

	/// Dual-quaternion of high double-precision floating-point numbers.
	///
	/// @see gtx_dual_quaternion
	typedef tdualquat<double, highp>	highp_ddualquat;


#if(!defined(GLM_PRECISION_HIGHP_FLOAT) && !defined(GLM_PRECISION_MEDIUMP_FLOAT) && !defined(GLM_PRECISION_LOWP_FLOAT))
	/// Dual-quaternion of floating-point numbers.
	///
	/// @see gtx_dual_quaternion
	typedef highp_fdualquat			dualquat;

	/// Dual-quaternion of single-precision floating-point numbers.
	///
	/// @see gtx_dual_quaternion
	typedef highp_fdualquat			fdualquat;
#elif(defined(GLM_PRECISION_HIGHP_FLOAT) && !defined(GLM_PRECISION_MEDIUMP_FLOAT) && !defined(GLM_PRECISION_LOWP_FLOAT))
	typedef highp_fdualquat			dualquat;
	typedef highp_fdualquat			fdualquat;
#elif(!defined(GLM_PRECISION_HIGHP_FLOAT) && defined(GLM_PRECISION_MEDIUMP_FLOAT) && !defined(GLM_PRECISION_LOWP_FLOAT))
	typedef mediump_fdualquat		dualquat;
	typedef mediump_fdualquat		fdualquat;
#elif(!defined(GLM_PRECISION_HIGHP_FLOAT) && !defined(GLM_PRECISION_MEDIUMP_FLOAT) && defined(GLM_PRECISION_LOWP_FLOAT))
	typedef lowp_fdualquat			dualquat;
	typedef lowp_fdualquat			fdualquat;
#else
#	error "GLM error: multiple default precision requested for single-precision floating-point types"
#endif


#if(!defined(GLM_PRECISION_HIGHP_DOUBLE) && !defined(GLM_PRECISION_MEDIUMP_DOUBLE) && !defined(GLM_PRECISION_LOWP_DOUBLE))
	/// Dual-quaternion of default double-precision floating-point numbers.
	///
	/// @see gtx_dual_quaternion
	typedef highp_ddualquat			ddualquat;
#elif(defined(GLM_PRECISION_HIGHP_DOUBLE) && !defined(GLM_PRECISION_MEDIUMP_DOUBLE) && !defined(GLM_PRECISION_LOWP_DOUBLE))
	typedef highp_ddualquat			ddualquat;
#elif(!defined(GLM_PRECISION_HIGHP_DOUBLE) && defined(GLM_PRECISION_MEDIUMP_DOUBLE) && !defined(GLM_PRECISION_LOWP_DOUBLE))
	typedef mediump_ddualquat		ddualquat;
#elif(!defined(GLM_PRECISION_HIGHP_DOUBLE) && !defined(GLM_PRECISION_MEDIUMP_DOUBLE) && defined(GLM_PRECISION_LOWP_DOUBLE))
	typedef lowp_ddualquat			ddualquat;
#else
#	error "GLM error: Multiple default precision requested for double-precision floating-point types"
#endif

	/// @}
} //namespace glm

#include "dual_quaternion.inl"
