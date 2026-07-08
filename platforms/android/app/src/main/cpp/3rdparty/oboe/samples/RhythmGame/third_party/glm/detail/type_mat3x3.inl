/// @ref core
/// @file glm/detail/type_mat3x3.inl

#include "func_matrix.hpp"

namespace glm
{
	// -- Constructors --

#	if !GLM_HAS_DEFAULTED_FUNCTIONS || !defined(GLM_FORCE_NO_CTOR_INIT)
		template <typename T, precision P>
		GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3()
		{
#			ifndef GLM_FORCE_NO_CTOR_INIT 
				this->value[0] = col_type(1, 0, 0);
				this->value[1] = col_type(0, 1, 0);
				this->value[2] = col_type(0, 0, 1);
#			endif
		}
#	endif

#	if !GLM_HAS_DEFAULTED_FUNCTIONS
		template <typename T, precision P>
		GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3(tmat3x3<T, P> const & m)
		{
			this->value[0] = m.value[0];
			this->value[1] = m.value[1];
			this->value[2] = m.value[2];
		}
#	endif//!GLM_HAS_DEFAULTED_FUNCTIONS

	template <typename T, precision P>
	template <precision Q>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3(tmat3x3<T, Q> const & m)
	{
		this->value[0] = m.value[0];
		this->value[1] = m.value[1];
		this->value[2] = m.value[2];
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER GLM_CONSTEXPR_CTOR tmat3x3<T, P>::tmat3x3(ctor)
	{}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3(T scalar)
	{
		this->value[0] = col_type(scalar, 0, 0);
		this->value[1] = col_type(0, scalar, 0);
		this->value[2] = col_type(0, 0, scalar);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3
	(
		T x0, T y0, T z0,
		T x1, T y1, T z1,
		T x2, T y2, T z2
	)
	{
		this->value[0] = col_type(x0, y0, z0);
		this->value[1] = col_type(x1, y1, z1);
		this->value[2] = col_type(x2, y2, z2);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3
	(
		col_type const & v0,
		col_type const & v1,
		col_type const & v2
	)
	{
		this->value[0] = v0;
		this->value[1] = v1;
		this->value[2] = v2;
	}

	// -- Conversion constructors --

	template <typename T, precision P>
	template <
		typename X1, typename Y1, typename Z1,
		typename X2, typename Y2, typename Z2,
		typename X3, typename Y3, typename Z3>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3
	(
		X1 x1, Y1 y1, Z1 z1,
		X2 x2, Y2 y2, Z2 z2,
		X3 x3, Y3 y3, Z3 z3
	)
	{
		this->value[0] = col_type(static_cast<T>(x1), value_type(y1), value_type(z1));
		this->value[1] = col_type(static_cast<T>(x2), value_type(y2), value_type(z2));
		this->value[2] = col_type(static_cast<T>(x3), value_type(y3), value_type(z3));
	}
	
	template <typename T, precision P>
	template <typename V1, typename V2, typename V3>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3
	(
		tvec3<V1, P> const & v1,
		tvec3<V2, P> const & v2,
		tvec3<V3, P> const & v3
	)
	{
		this->value[0] = col_type(v1);
		this->value[1] = col_type(v2);
		this->value[2] = col_type(v3);
	}

	// -- Matrix conversions --

	template <typename T, precision P>
	template <typename U, precision Q>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3(tmat3x3<U, Q> const & m)
	{
		this->value[0] = col_type(m[0]);
		this->value[1] = col_type(m[1]);
		this->value[2] = col_type(m[2]);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3(tmat2x2<T, P> const & m)
	{
		this->value[0] = col_type(m[0], 0);
		this->value[1] = col_type(m[1], 0);
		this->value[2] = col_type(0, 0, 1);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3(tmat4x4<T, P> const & m)
	{
		this->value[0] = col_type(m[0]);
		this->value[1] = col_type(m[1]);
		this->value[2] = col_type(m[2]);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3(tmat2x3<T, P> const & m)
	{
		this->value[0] = m[0];
		this->value[1] = m[1];
		this->value[2] = col_type(0, 0, 1);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3(tmat3x2<T, P> const & m)
	{
		this->value[0] = col_type(m[0], 0);
		this->value[1] = col_type(m[1], 0);
		this->value[2] = col_type(m[2], 1);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3(tmat2x4<T, P> const & m)
	{
		this->value[0] = col_type(m[0]);
		this->value[1] = col_type(m[1]);
		this->value[2] = col_type(0, 0, 1);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3(tmat4x2<T, P> const & m)
	{
		this->value[0] = col_type(m[0], 0);
		this->value[1] = col_type(m[1], 0);
		this->value[2] = col_type(m[2], 1);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3(tmat3x4<T, P> const & m)
	{
		this->value[0] = col_type(m[0]);
		this->value[1] = col_type(m[1]);
		this->value[2] = col_type(m[2]);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P>::tmat3x3(tmat4x3<T, P> const & m)
	{
		this->value[0] = m[0];
		this->value[1] = m[1];
		this->value[2] = m[2];
	}

	// -- Accesses --

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER typename tmat3x3<T, P>::col_type & tmat3x3<T, P>::operator[](typename tmat3x3<T, P>::length_type i)
	{
		assert(i < this->length());
		return this->value[i];
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER typename tmat3x3<T, P>::col_type const & tmat3x3<T, P>::operator[](typename tmat3x3<T, P>::length_type i) const
	{
		assert(i < this->length());
		return this->value[i];
	}

	// -- Unary updatable operators --

#	if !GLM_HAS_DEFAULTED_FUNCTIONS
		template <typename T, precision P>
		GLM_FUNC_QUALIFIER tmat3x3<T, P> & tmat3x3<T, P>::operator=(tmat3x3<T, P> const & m)
		{
			this->value[0] = m[0];
			this->value[1] = m[1];
			this->value[2] = m[2];
			return *this;
		}
#	endif//!GLM_HAS_DEFAULTED_FUNCTIONS

	template <typename T, precision P>
	template <typename U>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> & tmat3x3<T, P>::operator=(tmat3x3<U, P> const & m)
	{
		this->value[0] = m[0];
		this->value[1] = m[1];
		this->value[2] = m[2];
		return *this;
	}

	template <typename T, precision P>
	template <typename U>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> & tmat3x3<T, P>::operator+=(U s)
	{
		this->value[0] += s;
		this->value[1] += s;
		this->value[2] += s;
		return *this;
	}

	template <typename T, precision P>
	template <typename U>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> & tmat3x3<T, P>::operator+=(tmat3x3<U, P> const & m)
	{
		this->value[0] += m[0];
		this->value[1] += m[1];
		this->value[2] += m[2];
		return *this;
	}

	template <typename T, precision P>
	template <typename U>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> & tmat3x3<T, P>::operator-=(U s)
	{
		this->value[0] -= s;
		this->value[1] -= s;
		this->value[2] -= s;
		return *this;
	}

	template <typename T, precision P>
	template <typename U>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> & tmat3x3<T, P>::operator-=(tmat3x3<U, P> const & m)
	{
		this->value[0] -= m[0];
		this->value[1] -= m[1];
		this->value[2] -= m[2];
		return *this;
	}

	template <typename T, precision P>
	template <typename U>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> & tmat3x3<T, P>::operator*=(U s)
	{
		this->value[0] *= s;
		this->value[1] *= s;
		this->value[2] *= s;
		return *this;
	}

	template <typename T, precision P>
	template <typename U>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> & tmat3x3<T, P>::operator*=(tmat3x3<U, P> const & m)
	{
		return (*this = *this * m);
	}

	template <typename T, precision P>
	template <typename U>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> & tmat3x3<T, P>::operator/=(U s)
	{
		this->value[0] /= s;
		this->value[1] /= s;
		this->value[2] /= s;
		return *this;
	}

	template <typename T, precision P>
	template <typename U>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> & tmat3x3<T, P>::operator/=(tmat3x3<U, P> const & m)
	{
		return *this *= inverse(m);
	}

	// -- Increment and decrement operators --

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> & tmat3x3<T, P>::operator++()
	{
		++this->value[0];
		++this->value[1];
		++this->value[2];
		return *this;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> & tmat3x3<T, P>::operator--()
	{
		--this->value[0];
		--this->value[1];
		--this->value[2];
		return *this;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> tmat3x3<T, P>::operator++(int)
	{
		tmat3x3<T, P> Result(*this);
		++*this;
		return Result;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> tmat3x3<T, P>::operator--(int)
	{
		tmat3x3<T, P> Result(*this);
		--*this;
		return Result;
	}

	// -- Unary arithmetic operators --

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator+(tmat3x3<T, P> const & m)
	{
		return m;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator-(tmat3x3<T, P> const & m)
	{
		return tmat3x3<T, P>(
			-m[0], 
			-m[1],
			-m[2]);
	}

	// -- Binary arithmetic operators --

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator+(tmat3x3<T, P> const & m, T scalar)
	{
		return tmat3x3<T, P>(
			m[0] + scalar,
			m[1] + scalar,
			m[2] + scalar);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator+(T scalar, tmat3x3<T, P> const & m)
	{
		return tmat3x3<T, P>(
			m[0] + scalar,
			m[1] + scalar,
			m[2] + scalar);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator+(tmat3x3<T, P> const & m1, tmat3x3<T, P> const & m2)
	{
		return tmat3x3<T, P>(
			m1[0] + m2[0],
			m1[1] + m2[1],
			m1[2] + m2[2]);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator-(tmat3x3<T, P> const & m, T scalar)
	{
		return tmat3x3<T, P>(
			m[0] - scalar,
			m[1] - scalar,
			m[2] - scalar);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator-(T scalar, tmat3x3<T, P> const & m)
	{
		return tmat3x3<T, P>(
			scalar - m[0],
			scalar - m[1],
			scalar - m[2]);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator-(tmat3x3<T, P> const & m1, tmat3x3<T, P> const & m2)
	{
		return tmat3x3<T, P>(
			m1[0] - m2[0],
			m1[1] - m2[1],
			m1[2] - m2[2]);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator*(tmat3x3<T, P> const & m, T scalar)
	{
		return tmat3x3<T, P>(
			m[0] * scalar,
			m[1] * scalar,
			m[2] * scalar);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator*(T scalar, tmat3x3<T, P> const & m)
	{
		return tmat3x3<T, P>(
			m[0] * scalar,
			m[1] * scalar,
			m[2] * scalar);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER typename tmat3x3<T, P>::col_type operator*(tmat3x3<T, P> const & m, typename tmat3x3<T, P>::row_type const & v)
	{
		return typename tmat3x3<T, P>::col_type(
			m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z,
			m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z,
			m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER typename tmat3x3<T, P>::row_type operator*(typename tmat3x3<T, P>::col_type const & v, tmat3x3<T, P> const & m)
	{
		return typename tmat3x3<T, P>::row_type(
			m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
			m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
			m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z);
	}

	template <typename T, precision P> 
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator*(tmat3x3<T, P> const & m1, tmat3x3<T, P> const & m2)
	{
		T const SrcA00 = m1[0][0];
		T const SrcA01 = m1[0][1];
		T const SrcA02 = m1[0][2];
		T const SrcA10 = m1[1][0];
		T const SrcA11 = m1[1][1];
		T const SrcA12 = m1[1][2];
		T const SrcA20 = m1[2][0];
		T const SrcA21 = m1[2][1];
		T const SrcA22 = m1[2][2];

		T const SrcB00 = m2[0][0];
		T const SrcB01 = m2[0][1];
		T const SrcB02 = m2[0][2];
		T const SrcB10 = m2[1][0];
		T const SrcB11 = m2[1][1];
		T const SrcB12 = m2[1][2];
		T const SrcB20 = m2[2][0];
		T const SrcB21 = m2[2][1];
		T const SrcB22 = m2[2][2];

		tmat3x3<T, P> Result(uninitialize);
		Result[0][0] = SrcA00 * SrcB00 + SrcA10 * SrcB01 + SrcA20 * SrcB02;
		Result[0][1] = SrcA01 * SrcB00 + SrcA11 * SrcB01 + SrcA21 * SrcB02;
		Result[0][2] = SrcA02 * SrcB00 + SrcA12 * SrcB01 + SrcA22 * SrcB02;
		Result[1][0] = SrcA00 * SrcB10 + SrcA10 * SrcB11 + SrcA20 * SrcB12;
		Result[1][1] = SrcA01 * SrcB10 + SrcA11 * SrcB11 + SrcA21 * SrcB12;
		Result[1][2] = SrcA02 * SrcB10 + SrcA12 * SrcB11 + SrcA22 * SrcB12;
		Result[2][0] = SrcA00 * SrcB20 + SrcA10 * SrcB21 + SrcA20 * SrcB22;
		Result[2][1] = SrcA01 * SrcB20 + SrcA11 * SrcB21 + SrcA21 * SrcB22;
		Result[2][2] = SrcA02 * SrcB20 + SrcA12 * SrcB21 + SrcA22 * SrcB22;
		return Result;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat2x3<T, P> operator*(tmat3x3<T, P> const & m1, tmat2x3<T, P> const & m2)
	{
		return tmat2x3<T, P>(
			m1[0][0] * m2[0][0] + m1[1][0] * m2[0][1] + m1[2][0] * m2[0][2],
			m1[0][1] * m2[0][0] + m1[1][1] * m2[0][1] + m1[2][1] * m2[0][2],
			m1[0][2] * m2[0][0] + m1[1][2] * m2[0][1] + m1[2][2] * m2[0][2],
			m1[0][0] * m2[1][0] + m1[1][0] * m2[1][1] + m1[2][0] * m2[1][2],
			m1[0][1] * m2[1][0] + m1[1][1] * m2[1][1] + m1[2][1] * m2[1][2],
			m1[0][2] * m2[1][0] + m1[1][2] * m2[1][1] + m1[2][2] * m2[1][2]);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat4x3<T, P> operator*(tmat3x3<T, P> const & m1, tmat4x3<T, P> const & m2)
	{
		return tmat4x3<T, P>(
			m1[0][0] * m2[0][0] + m1[1][0] * m2[0][1] + m1[2][0] * m2[0][2],
			m1[0][1] * m2[0][0] + m1[1][1] * m2[0][1] + m1[2][1] * m2[0][2],
			m1[0][2] * m2[0][0] + m1[1][2] * m2[0][1] + m1[2][2] * m2[0][2],
			m1[0][0] * m2[1][0] + m1[1][0] * m2[1][1] + m1[2][0] * m2[1][2],
			m1[0][1] * m2[1][0] + m1[1][1] * m2[1][1] + m1[2][1] * m2[1][2],
			m1[0][2] * m2[1][0] + m1[1][2] * m2[1][1] + m1[2][2] * m2[1][2],
			m1[0][0] * m2[2][0] + m1[1][0] * m2[2][1] + m1[2][0] * m2[2][2],
			m1[0][1] * m2[2][0] + m1[1][1] * m2[2][1] + m1[2][1] * m2[2][2],
			m1[0][2] * m2[2][0] + m1[1][2] * m2[2][1] + m1[2][2] * m2[2][2],
			m1[0][0] * m2[3][0] + m1[1][0] * m2[3][1] + m1[2][0] * m2[3][2],
			m1[0][1] * m2[3][0] + m1[1][1] * m2[3][1] + m1[2][1] * m2[3][2],
			m1[0][2] * m2[3][0] + m1[1][2] * m2[3][1] + m1[2][2] * m2[3][2]);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator/(tmat3x3<T, P> const & m,	T scalar)
	{
		return tmat3x3<T, P>(
			m[0] / scalar,
			m[1] / scalar,
			m[2] / scalar);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator/(T scalar, tmat3x3<T, P> const & m)
	{
		return tmat3x3<T, P>(
			scalar / m[0],
			scalar / m[1],
			scalar / m[2]);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER typename tmat3x3<T, P>::col_type operator/(tmat3x3<T, P> const & m, typename tmat3x3<T, P>::row_type const & v)
	{
		return  inverse(m) * v;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER typename tmat3x3<T, P>::row_type operator/(typename tmat3x3<T, P>::col_type const & v, tmat3x3<T, P> const & m)
	{
		return v * inverse(m);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tmat3x3<T, P> operator/(tmat3x3<T, P> const & m1, tmat3x3<T, P> const & m2)
	{
		tmat3x3<T, P> m1_copy(m1);
		return m1_copy /= m2;
	}

	// -- Boolean operators --

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER bool operator==(tmat3x3<T, P> const & m1, tmat3x3<T, P> const & m2)
	{
		return (m1[0] == m2[0]) && (m1[1] == m2[1]) && (m1[2] == m2[2]);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER bool operator!=(tmat3x3<T, P> const & m1, tmat3x3<T, P> const & m2)
	{
		return (m1[0] != m2[0]) || (m1[1] != m2[1]) || (m1[2] != m2[2]);
	}
} //namespace glm
