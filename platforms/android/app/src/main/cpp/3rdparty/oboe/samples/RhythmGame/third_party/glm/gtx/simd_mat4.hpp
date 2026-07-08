/// @ref gtx_simd_mat4
/// @file glm/gtx/simd_mat4.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_simd_mat4 GLM_GTX_simd_mat4
/// @ingroup gtx
///
/// @brief SIMD implementation of mat4 type.
///
/// <glm/gtx/simd_mat4.hpp> need to be included to use these functionalities.

#pragma once

// Dependencies
#include "../detail/setup.hpp"

#if(GLM_ARCH != GLM_ARCH_PURE)

#if(GLM_ARCH & GLM_ARCH_SSE2_BIT)
#	include "../detail/intrinsic_matrix.hpp"
#	include "../gtx/simd_vec4.hpp"
#else
#	error "GLM: GLM_GTX_simd_mat4 requires compiler support of SSE2 through intrinsics"
#endif

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_simd_mat4 extension included")
#	pragma message("GLM: GLM_GTX_simd_mat4 extension is deprecated and will be removed in GLM 0.9.9. Use mat4 instead and use compiler SIMD arguments.")
#endif

namespace glm{
namespace detail
{
	/// 4x4 Matrix implemented using SIMD SEE intrinsics.
	/// \ingroup gtx_simd_mat4
	GLM_ALIGNED_STRUCT(16) fmat4x4SIMD
	{
		typedef float value_type;
		typedef fvec4SIMD col_type;
		typedef fvec4SIMD row_type;
		typedef std::size_t size_type;
		typedef fmat4x4SIMD type;
		typedef fmat4x4SIMD transpose_type;

		typedef tmat4x4<float, defaultp> pure_type;
		typedef tvec4<float, defaultp> pure_row_type;
		typedef tvec4<float, defaultp> pure_col_type;
		typedef tmat4x4<float, defaultp> pure_transpose_type;

		GLM_FUNC_DECL length_t length() const;

		fvec4SIMD Data[4];

		//////////////////////////////////////
		// Constructors

		fmat4x4SIMD() GLM_DEFAULT_CTOR;
		explicit fmat4x4SIMD(float const & s);
		explicit fmat4x4SIMD(
			float const & x0, float const & y0, float const & z0, float const & w0,
			float const & x1, float const & y1, float const & z1, float const & w1,
			float const & x2, float const & y2, float const & z2, float const & w2,
			float const & x3, float const & y3, float const & z3, float const & w3);
		explicit fmat4x4SIMD(
			fvec4SIMD const & v0,
			fvec4SIMD const & v1,
			fvec4SIMD const & v2,
			fvec4SIMD const & v3);
		explicit fmat4x4SIMD(
			mat4x4 const & m);
		explicit fmat4x4SIMD(
			__m128 const in[4]);

		// Conversions
		//template <typename U>
		//explicit tmat4x4(tmat4x4<U> const & m);

		//explicit tmat4x4(tmat2x2<T> const & x);
		//explicit tmat4x4(tmat3x3<T> const & x);
		//explicit tmat4x4(tmat2x3<T> const & x);
		//explicit tmat4x4(tmat3x2<T> const & x);
		//explicit tmat4x4(tmat2x4<T> const & x);
		//explicit tmat4x4(tmat4x2<T> const & x);
		//explicit tmat4x4(tmat3x4<T> const & x);
		//explicit tmat4x4(tmat4x3<T> const & x);

		// Accesses
		fvec4SIMD & operator[](length_t i);
		fvec4SIMD const & operator[](length_t i) const;

		// Unary updatable operators
		fmat4x4SIMD & operator= (fmat4x4SIMD const & m) GLM_DEFAULT;
		fmat4x4SIMD & operator+= (float const & s);
		fmat4x4SIMD & operator+= (fmat4x4SIMD const & m);
		fmat4x4SIMD & operator-= (float const & s);
		fmat4x4SIMD & operator-= (fmat4x4SIMD const & m);
		fmat4x4SIMD & operator*= (float const & s);
		fmat4x4SIMD & operator*= (fmat4x4SIMD const & m);
		fmat4x4SIMD & operator/= (float const & s);
		fmat4x4SIMD & operator/= (fmat4x4SIMD const & m);
		fmat4x4SIMD & operator++ ();
		fmat4x4SIMD & operator-- ();
	};

	// Binary operators
	fmat4x4SIMD operator+ (fmat4x4SIMD const & m, float const & s);
	fmat4x4SIMD operator+ (float const & s, fmat4x4SIMD const & m);
	fmat4x4SIMD operator+ (fmat4x4SIMD const & m1, fmat4x4SIMD const & m2);

	fmat4x4SIMD operator- (fmat4x4SIMD const & m, float const & s);
	fmat4x4SIMD operator- (float const & s, fmat4x4SIMD const & m);
	fmat4x4SIMD operator- (fmat4x4SIMD const & m1, fmat4x4SIMD const & m2);

	fmat4x4SIMD operator* (fmat4x4SIMD const & m, float const & s);
	fmat4x4SIMD operator* (float const & s, fmat4x4SIMD const & m);

	fvec4SIMD operator* (fmat4x4SIMD const & m, fvec4SIMD const & v);
	fvec4SIMD operator* (fvec4SIMD const & v, fmat4x4SIMD const & m);

	fmat4x4SIMD operator* (fmat4x4SIMD const & m1, fmat4x4SIMD const & m2);

	fmat4x4SIMD operator/ (fmat4x4SIMD const & m, float const & s);
	fmat4x4SIMD operator/ (float const & s, fmat4x4SIMD const & m);

	fvec4SIMD operator/ (fmat4x4SIMD const & m, fvec4SIMD const & v);
	fvec4SIMD operator/ (fvec4SIMD const & v, fmat4x4SIMD const & m);

	fmat4x4SIMD operator/ (fmat4x4SIMD const & m1, fmat4x4SIMD const & m2);

	// Unary constant operators
	fmat4x4SIMD const operator-  (fmat4x4SIMD const & m);
	fmat4x4SIMD const operator-- (fmat4x4SIMD const & m, int);
	fmat4x4SIMD const operator++ (fmat4x4SIMD const & m, int);
}//namespace detail

	typedef detail::fmat4x4SIMD simdMat4;

	/// @addtogroup gtx_simd_mat4
	/// @{

	//! Convert a simdMat4 to a mat4.
	//! (From GLM_GTX_simd_mat4 extension)
	mat4 mat4_cast(
		detail::fmat4x4SIMD const & x);

	//! Multiply matrix x by matrix y component-wise, i.e.,
	//! result[i][j] is the scalar product of x[i][j] and y[i][j].
	//! (From GLM_GTX_simd_mat4 extension).
	detail::fmat4x4SIMD matrixCompMult(
		detail::fmat4x4SIMD const & x,
		detail::fmat4x4SIMD const & y);

	//! Treats the first parameter c as a column vector
	//! and the second parameter r as a row vector
	//! and does a linear algebraic matrix multiply c * r.
	//! (From GLM_GTX_simd_mat4 extension).
	detail::fmat4x4SIMD outerProduct(
		detail::fvec4SIMD const & c,
		detail::fvec4SIMD const & r);

	//! Returns the transposed matrix of x
	//! (From GLM_GTX_simd_mat4 extension).
	detail::fmat4x4SIMD transpose(
		detail::fmat4x4SIMD const & x);

	//! Return the determinant of a mat4 matrix.
	//! (From GLM_GTX_simd_mat4 extension).
	float determinant(
		detail::fmat4x4SIMD const & m);

	//! Return the inverse of a mat4 matrix.
	//! (From GLM_GTX_simd_mat4 extension).
	detail::fmat4x4SIMD inverse(
		detail::fmat4x4SIMD const & m);

	/// @}
}// namespace glm

#include "simd_mat4.inl"

#endif//(GLM_ARCH != GLM_ARCH_PURE)
