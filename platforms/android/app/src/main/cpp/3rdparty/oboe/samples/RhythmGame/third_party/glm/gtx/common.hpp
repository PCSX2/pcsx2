/// @ref gtx_common
/// @file glm/gtx/common.hpp
///
/// @see core (dependence)
/// @see gtc_half_float (dependence)
///
/// @defgroup gtx_common GLM_GTX_common
/// @ingroup gtx
///
/// @brief Provide functions to increase the compatibility with Cg and HLSL languages
///
/// <glm/gtx/common.hpp> need to be included to use these functionalities.

#pragma once

// Dependencies:
#include "../vec2.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"
#include "../gtc/vec1.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_common extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_common
	/// @{

	/// Returns true if x is a denormalized number
	/// Numbers whose absolute value is too small to be represented in the normal format are represented in an alternate, denormalized format.
	/// This format is less precise but can represent values closer to zero.
	/// 
	/// @tparam genType Floating-point scalar or vector types.
	///
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/isnan.xml">GLSL isnan man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.3 Common Functions</a>
	template <typename genType> 
	GLM_FUNC_DECL typename genType::bool_type isdenormal(genType const & x);

	/// Similar to 'mod' but with a different rounding and integer support.
	/// Returns 'x - y * trunc(x/y)' instead of 'x - y * floor(x/y)'
	/// 
	/// @see <a href="http://stackoverflow.com/questions/7610631/glsl-mod-vs-hlsl-fmod">GLSL mod vs HLSL fmod</a>
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/mod.xml">GLSL mod man page</a>
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<T, P> fmod(vecType<T, P> const & v);

	/// @}
}//namespace glm

#include "common.inl"
