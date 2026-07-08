/// @ref core
/// @file glm/detail/type_mat4x2.hpp

#pragma once

#include "../fwd.hpp"
#include "type_vec2.hpp"
#include "type_vec4.hpp"
#include "type_mat.hpp"
#include <limits>
#include <cstddef>

namespace glm
{
	template <typename T, precision P = defaultp>
	struct tmat4x2
	{
		typedef tvec2<T, P> col_type;
		typedef tvec4<T, P> row_type;
		typedef tmat4x2<T, P> type;
		typedef tmat2x4<T, P> transpose_type;
		typedef T value_type;

	private:
		col_type value[4];

	public:
		// -- Constructors --

		GLM_FUNC_DECL tmat4x2() GLM_DEFAULT_CTOR;
		GLM_FUNC_DECL tmat4x2(tmat4x2<T, P> const & m) GLM_DEFAULT;
		template <precision Q>
		GLM_FUNC_DECL tmat4x2(tmat4x2<T, Q> const & m);

		GLM_FUNC_DECL GLM_CONSTEXPR_CTOR explicit tmat4x2(ctor);
		GLM_FUNC_DECL explicit tmat4x2(T scalar);
		GLM_FUNC_DECL tmat4x2(
			T x0, T y0,
			T x1, T y1,
			T x2, T y2,
			T x3, T y3);
		GLM_FUNC_DECL tmat4x2(
			col_type const & v0,
			col_type const & v1,
			col_type const & v2,
			col_type const & v3);

		// -- Conversions --

		template <
			typename X1, typename Y1,
			typename X2, typename Y2,
			typename X3, typename Y3,
			typename X4, typename Y4>
		GLM_FUNC_DECL tmat4x2(
			X1 x1, Y1 y1,
			X2 x2, Y2 y2,
			X3 x3, Y3 y3,
			X4 x4, Y4 y4);

		template <typename V1, typename V2, typename V3, typename V4>
		GLM_FUNC_DECL tmat4x2(
			tvec2<V1, P> const & v1,
			tvec2<V2, P> const & v2,
			tvec2<V3, P> const & v3,
			tvec2<V4, P> const & v4);

		// -- Matrix conversions --

		template <typename U, precision Q>
		GLM_FUNC_DECL GLM_EXPLICIT tmat4x2(tmat4x2<U, Q> const & m);

		GLM_FUNC_DECL GLM_EXPLICIT tmat4x2(tmat2x2<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat4x2(tmat3x3<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat4x2(tmat4x4<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat4x2(tmat2x3<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat4x2(tmat3x2<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat4x2(tmat2x4<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat4x2(tmat4x3<T, P> const & x);
		GLM_FUNC_DECL GLM_EXPLICIT tmat4x2(tmat3x4<T, P> const & x);

		// -- Accesses --

		typedef length_t length_type;
		GLM_FUNC_DECL static length_type length(){return 4;}

		GLM_FUNC_DECL col_type & operator[](length_type i);
		GLM_FUNC_DECL col_type const & operator[](length_type i) const;

		// -- Unary arithmetic operators --

		GLM_FUNC_DECL tmat4x2<T, P> & operator=(tmat4x2<T, P> const & m) GLM_DEFAULT;

		template <typename U>
		GLM_FUNC_DECL tmat4x2<T, P> & operator=(tmat4x2<U, P> const & m);
		template <typename U>
		GLM_FUNC_DECL tmat4x2<T, P> & operator+=(U s);
		template <typename U>
		GLM_FUNC_DECL tmat4x2<T, P> & operator+=(tmat4x2<U, P> const & m);
		template <typename U>
		GLM_FUNC_DECL tmat4x2<T, P> & operator-=(U s);
		template <typename U>
		GLM_FUNC_DECL tmat4x2<T, P> & operator-=(tmat4x2<U, P> const & m);
		template <typename U>
		GLM_FUNC_DECL tmat4x2<T, P> & operator*=(U s);
		template <typename U>
		GLM_FUNC_DECL tmat4x2<T, P> & operator/=(U s);

		// -- Increment and decrement operators --

		GLM_FUNC_DECL tmat4x2<T, P> & operator++ ();
		GLM_FUNC_DECL tmat4x2<T, P> & operator-- ();
		GLM_FUNC_DECL tmat4x2<T, P> operator++(int);
		GLM_FUNC_DECL tmat4x2<T, P> operator--(int);
	};

	// -- Unary operators --

	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x2<T, P> operator+(tmat4x2<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x2<T, P> operator-(tmat4x2<T, P> const & m);

	// -- Binary operators --

	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x2<T, P> operator+(tmat4x2<T, P> const & m, T scalar);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x2<T, P> operator+(tmat4x2<T, P> const & m1, tmat4x2<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x2<T, P> operator-(tmat4x2<T, P> const & m, T scalar);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x2<T, P> operator-(tmat4x2<T, P> const & m1,	tmat4x2<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x2<T, P> operator*(tmat4x2<T, P> const & m, T scalar);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x2<T, P> operator*(T scalar, tmat4x2<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL typename tmat4x2<T, P>::col_type operator*(tmat4x2<T, P> const & m, typename tmat4x2<T, P>::row_type const & v);

	template <typename T, precision P>
	GLM_FUNC_DECL typename tmat4x2<T, P>::row_type operator*(typename tmat4x2<T, P>::col_type const & v, tmat4x2<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> operator*(tmat4x2<T, P> const & m1, tmat2x4<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x2<T, P> operator*(tmat4x2<T, P> const & m1, tmat3x4<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x2<T, P> operator*(tmat4x2<T, P> const & m1, tmat4x4<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x2<T, P> operator/(tmat4x2<T, P> const & m, T scalar);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x2<T, P> operator/(T scalar, tmat4x2<T, P> const & m);

	// -- Boolean operators --

	template <typename T, precision P>
	GLM_FUNC_DECL bool operator==(tmat4x2<T, P> const & m1, tmat4x2<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL bool operator!=(tmat4x2<T, P> const & m1, tmat4x2<T, P> const & m2);
}//namespace glm

#ifndef GLM_EXTERNAL_TEMPLATE
#include "type_mat4x2.inl"
#endif
