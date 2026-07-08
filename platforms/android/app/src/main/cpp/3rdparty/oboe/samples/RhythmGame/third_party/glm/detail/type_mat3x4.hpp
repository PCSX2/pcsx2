/// @ref core
/// @file glm/detail/type_mat3x4.hpp

#pragma once

#include "../fwd.hpp"
#include "type_vec3.hpp"
#include "type_vec4.hpp"
#include "type_mat.hpp"
#include <limits>
#include <cstddef>

namespace glm
{
	template <typename T, precision P = defaultp>
	struct tmat3x4
	{
		typedef tvec4<T, P> col_type;
		typedef tvec3<T, P> row_type;
		typedef tmat3x4<T, P> type;
		typedef tmat4x3<T, P> transpose_type;
		typedef T value_type;

	private:
		col_type value[3];

	public:
		// -- Constructors --

		GLM_FUNC_DECL tmat3x4() GLM_DEFAULT_CTOR;
		GLM_FUNC_DECL tmat3x4(tmat3x4<T, P> const & m) GLM_DEFAULT;
		template <precision Q>
		GLM_FUNC_DECL tmat3x4(tmat3x4<T, Q> const & m);

		GLM_FUNC_DECL GLM_CONSTEXPR_CTOR explicit tmat3x4(ctor);
		GLM_FUNC_DECL explicit tmat3x4(T scalar);
		GLM_FUNC_DECL tmat3x4(
			T x0, T y0, T z0, T w0,
			T x1, T y1, T z1, T w1,
			T x2, T y2, T z2, T w2);
		GLM_FUNC_DECL tmat3x4(
			col_type const & v0,
			col_type const & v1,
			col_type const & v2);

		// -- Conversions --

		template<
			typename X1, typename Y1, typename Z1, typename W1,
			typename X2, typename Y2, typename Z2, typename W2,
			typename X3, typename Y3, typename Z3, typename W3>
		GLM_FUNC_DECL tmat3x4(
			X1 x1, Y1 y1, Z1 z1, W1 w1,
			X2 x2, Y2 y2, Z2 z2, W2 w2,
			X3 x3, Y3 y3, Z3 z3, W3 w3);

		template <typename V1, typename V2, typename V3>
		GLM_FUNC_DECL tmat3x4(
			tvec4<V1, P> const & v1,
			tvec4<V2, P> const & v2,
			tvec4<V3, P> const & v3);

		// -- Matrix conversions --

		template <typename U, precision Q>
		GLM_FUNC_DECL GLM_EXPLICIT tmat3x4(tmat3x4<U, Q> const & m);

		GLM_FUNC_DECL GLM_EXPLICIT tmat3x4(tmat2x2<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat3x4(tmat3x3<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat3x4(tmat4x4<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat3x4(tmat2x3<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat3x4(tmat3x2<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat3x4(tmat2x4<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat3x4(tmat4x2<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat3x4(tmat4x3<T, P> const & x);

		// -- Accesses --

		typedef length_t length_type;
		GLM_FUNC_DECL static length_type length(){return 3;}

		GLM_FUNC_DECL col_type & operator[](length_type i);
		GLM_FUNC_DECL col_type const & operator[](length_type i) const;

		// -- Unary arithmetic operators --

		GLM_FUNC_DECL tmat3x4<T, P> & operator=(tmat3x4<T, P> const & m) GLM_DEFAULT;

		template <typename U>
		GLM_FUNC_DECL tmat3x4<T, P> & operator=(tmat3x4<U, P> const & m);
		template <typename U>
		GLM_FUNC_DECL tmat3x4<T, P> & operator+=(U s);
		template <typename U>
		GLM_FUNC_DECL tmat3x4<T, P> & operator+=(tmat3x4<U, P> const & m);
		template <typename U>
		GLM_FUNC_DECL tmat3x4<T, P> & operator-=(U s);
		template <typename U>
		GLM_FUNC_DECL tmat3x4<T, P> & operator-=(tmat3x4<U, P> const & m);
		template <typename U>
		GLM_FUNC_DECL tmat3x4<T, P> & operator*=(U s);
		template <typename U>
		GLM_FUNC_DECL tmat3x4<T, P> & operator/=(U s);

		// -- Increment and decrement operators --

		GLM_FUNC_DECL tmat3x4<T, P> & operator++();
		GLM_FUNC_DECL tmat3x4<T, P> & operator--();
		GLM_FUNC_DECL tmat3x4<T, P> operator++(int);
		GLM_FUNC_DECL tmat3x4<T, P> operator--(int);
	};

	// -- Unary operators --

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x4<T, P> operator+(tmat3x4<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x4<T, P> operator-(tmat3x4<T, P> const & m);

	// -- Binary operators --

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x4<T, P> operator+(tmat3x4<T, P> const & m, T scalar);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x4<T, P> operator+(tmat3x4<T, P> const & m1, tmat3x4<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x4<T, P> operator-(tmat3x4<T, P> const & m, T scalar);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x4<T, P> operator-(tmat3x4<T, P> const & m1, tmat3x4<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x4<T, P> operator*(tmat3x4<T, P> const & m, T scalar);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x4<T, P> operator*(T scalar, tmat3x4<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL typename tmat3x4<T, P>::col_type operator*(tmat3x4<T, P> const & m, typename tmat3x4<T, P>::row_type const & v);

	template <typename T, precision P>
	GLM_FUNC_DECL typename tmat3x4<T, P>::row_type operator*(typename tmat3x4<T, P>::col_type const & v, tmat3x4<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x4<T, P> operator*(tmat3x4<T, P> const & m1,	tmat4x3<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x4<T, P> operator*(tmat3x4<T, P> const & m1, tmat2x3<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x4<T, P> operator*(tmat3x4<T, P> const & m1,	tmat3x3<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x4<T, P> operator/(tmat3x4<T, P> const & m, T scalar);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x4<T, P> operator/(T scalar, tmat3x4<T, P> const & m);

	// -- Boolean operators --

	template <typename T, precision P>
	GLM_FUNC_DECL bool operator==(tmat3x4<T, P> const & m1, tmat3x4<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL bool operator!=(tmat3x4<T, P> const & m1, tmat3x4<T, P> const & m2);
}//namespace glm

#ifndef GLM_EXTERNAL_TEMPLATE
#include "type_mat3x4.inl"
#endif
