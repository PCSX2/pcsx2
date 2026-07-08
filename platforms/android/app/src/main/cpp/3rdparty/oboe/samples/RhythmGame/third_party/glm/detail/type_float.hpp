/// @ref core
/// @file glm/detail/type_float.hpp

#pragma once

#include "setup.hpp"

namespace glm{
namespace detail
{
	typedef float				float32;
	typedef double				float64;
}//namespace detail
	
	typedef float				lowp_float_t;
	typedef float				mediump_float_t;
	typedef double				highp_float_t;

	/// @addtogroup core_precision
	/// @{

	/// Low precision floating-point numbers. 
	/// There is no guarantee on the actual precision.
	/// 
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.4 Floats</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef lowp_float_t		lowp_float;

	/// Medium precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	/// 
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.4 Floats</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef mediump_float_t		mediump_float;

	/// High precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	/// 
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.4 Floats</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef highp_float_t		highp_float;

#if(!defined(GLM_PRECISION_HIGHP_FLOAT) && !defined(GLM_PRECISION_MEDIUMP_FLOAT) && !defined(GLM_PRECISION_LOWP_FLOAT))
	typedef mediump_float		float_t;
#elif(defined(GLM_PRECISION_HIGHP_FLOAT) && !defined(GLM_PRECISION_MEDIUMP_FLOAT) && !defined(GLM_PRECISION_LOWP_FLOAT))
	typedef highp_float			float_t;
#elif(!defined(GLM_PRECISION_HIGHP_FLOAT) && defined(GLM_PRECISION_MEDIUMP_FLOAT) && !defined(GLM_PRECISION_LOWP_FLOAT))
	typedef mediump_float		float_t;
#elif(!defined(GLM_PRECISION_HIGHP_FLOAT) && !defined(GLM_PRECISION_MEDIUMP_FLOAT) && defined(GLM_PRECISION_LOWP_FLOAT))
	typedef lowp_float			float_t;
#else
#	error "GLM error: multiple default precision requested for floating-point types"
#endif

	typedef float				float32;
	typedef double				float64;

////////////////////
// check type sizes
#ifndef GLM_STATIC_ASSERT_NULL
	GLM_STATIC_ASSERT(sizeof(glm::float32) == 4, "float32 size isn't 4 bytes on this platform");
	GLM_STATIC_ASSERT(sizeof(glm::float64) == 8, "float64 size isn't 8 bytes on this platform");
#endif//GLM_STATIC_ASSERT_NULL

	/// @}

}//namespace glm
