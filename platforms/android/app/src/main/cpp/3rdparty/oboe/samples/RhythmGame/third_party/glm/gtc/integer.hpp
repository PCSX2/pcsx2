/// @ref gtc_integer
/// @file glm/gtc/integer.hpp
///
/// @see core (dependence)
/// @see gtc_integer (dependence)
///
/// @defgroup gtc_integer GLM_GTC_integer
/// @ingroup gtc
///
/// @brief Allow to perform bit operations on integer values
///
/// <glm/gtc/integer.hpp> need to be included to use these functionalities.

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/precision.hpp"
#include "../detail/func_common.hpp"
#include "../detail/func_integer.hpp"
#include "../detail/func_exponential.hpp"
#include <limits>

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_integer extension included")
#endif

namespace glm
{
	/// @addtogroup gtc_integer
	/// @{

	/// Returns the log2 of x for integer values. Can be reliably using to compute mipmap count from the texture size.
	/// @see gtc_integer
	template <typename genIUType>
	GLM_FUNC_DECL genIUType log2(genIUType x);

	/// Modulus. Returns x % y
	/// for each component in x using the floating point value y.
	///
	/// @tparam genIUType Integer-point scalar or vector types.
	///
	/// @see gtc_integer
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/mod.xml">GLSL mod man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.3 Common Functions</a>
	template <typename genIUType>
	GLM_FUNC_DECL genIUType mod(genIUType x, genIUType y);

	/// Modulus. Returns x % y
	/// for each component in x using the floating point value y.
	///
	/// @tparam T Integer scalar types.
	/// @tparam vecType vector types.
	///
	/// @see gtc_integer
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/mod.xml">GLSL mod man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.3 Common Functions</a>
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<T, P> mod(vecType<T, P> const & x, T y);

	/// Modulus. Returns x % y
	/// for each component in x using the floating point value y.
	///
	/// @tparam T Integer scalar types.
	/// @tparam vecType vector types.
	///
	/// @see gtc_integer
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/mod.xml">GLSL mod man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.3 Common Functions</a>
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<T, P> mod(vecType<T, P> const & x, vecType<T, P> const & y);

	/// Returns a value equal to the nearest integer to x.
	/// The fraction 0.5 will round in a direction chosen by the
	/// implementation, presumably the direction that is fastest.
	/// 
	/// @param x The values of the argument must be greater or equal to zero.
	/// @tparam T floating point scalar types.
	/// @tparam vecType vector types.
	/// 
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/round.xml">GLSL round man page</a>
	/// @see gtc_integer
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<int, P> iround(vecType<T, P> const & x);

	/// Returns a value equal to the nearest integer to x.
	/// The fraction 0.5 will round in a direction chosen by the
	/// implementation, presumably the direction that is fastest.
	/// 
	/// @param x The values of the argument must be greater or equal to zero.
	/// @tparam T floating point scalar types.
	/// @tparam vecType vector types.
	/// 
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/round.xml">GLSL round man page</a>
	/// @see gtc_integer
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<uint, P> uround(vecType<T, P> const & x);

	/// @}
} //namespace glm

#include "integer.inl"
