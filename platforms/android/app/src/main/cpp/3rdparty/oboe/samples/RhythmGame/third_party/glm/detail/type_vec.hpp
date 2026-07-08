/// @ref core
/// @file glm/detail/type_vec.hpp

#pragma once

#include "precision.hpp"
#include "type_int.hpp"

namespace glm{
namespace detail
{
	template <typename T, std::size_t size, bool aligned>
	struct storage
	{
		typedef struct type {
			uint8 data[size];
		} type;
	};

	#define GLM_ALIGNED_STORAGE_TYPE_STRUCT(x) \
		template <typename T> \
		struct storage<T, x, true> { \
			GLM_ALIGNED_STRUCT(x) type { \
				uint8 data[x]; \
			}; \
		};

	GLM_ALIGNED_STORAGE_TYPE_STRUCT(1)
	GLM_ALIGNED_STORAGE_TYPE_STRUCT(2)
	GLM_ALIGNED_STORAGE_TYPE_STRUCT(4)
	GLM_ALIGNED_STORAGE_TYPE_STRUCT(8)
	GLM_ALIGNED_STORAGE_TYPE_STRUCT(16)
	GLM_ALIGNED_STORAGE_TYPE_STRUCT(32)
	GLM_ALIGNED_STORAGE_TYPE_STRUCT(64)
		
#	if GLM_ARCH & GLM_ARCH_SSE2_BIT
		template <>
		struct storage<float, 16, true>
		{
			typedef glm_vec4 type;
		};

		template <>
		struct storage<int, 16, true>
		{
			typedef glm_ivec4 type;
		};

		template <>
		struct storage<unsigned int, 16, true>
		{
			typedef glm_uvec4 type;
		};
/*
#	else
		typedef union __declspec(align(16)) glm_128
		{
			unsigned __int8 data[16];
		} glm_128;

		template <>
		struct storage<float, 16, true>
		{
			typedef glm_128 type;
		};

		template <>
		struct storage<int, 16, true>
		{
			typedef glm_128 type;
		};

		template <>
		struct storage<unsigned int, 16, true>
		{
			typedef glm_128 type;
		};
*/
#	endif

#	if (GLM_ARCH & GLM_ARCH_AVX_BIT)
		template <>
		struct storage<double, 32, true>
		{
			typedef glm_dvec4 type;
		};
#	endif

#	if (GLM_ARCH & GLM_ARCH_AVX2_BIT)
		template <>
		struct storage<int64, 32, true>
		{
			typedef glm_i64vec4 type;
		};

		template <>
		struct storage<uint64, 32, true>
		{
			typedef glm_u64vec4 type;
		};
#	endif
}//namespace detail

	template <typename T, precision P> struct tvec1;
	template <typename T, precision P> struct tvec2;
	template <typename T, precision P> struct tvec3;
	template <typename T, precision P> struct tvec4;

	typedef tvec1<float, highp>		highp_vec1_t;
	typedef tvec1<float, mediump>	mediump_vec1_t;
	typedef tvec1<float, lowp>		lowp_vec1_t;
	typedef tvec1<double, highp>	highp_dvec1_t;
	typedef tvec1<double, mediump>	mediump_dvec1_t;
	typedef tvec1<double, lowp>		lowp_dvec1_t;
	typedef tvec1<int, highp>		highp_ivec1_t;
	typedef tvec1<int, mediump>		mediump_ivec1_t;
	typedef tvec1<int, lowp>		lowp_ivec1_t;
	typedef tvec1<uint, highp>		highp_uvec1_t;
	typedef tvec1<uint, mediump>	mediump_uvec1_t;
	typedef tvec1<uint, lowp>		lowp_uvec1_t;
	typedef tvec1<bool, highp>		highp_bvec1_t;
	typedef tvec1<bool, mediump>	mediump_bvec1_t;
	typedef tvec1<bool, lowp>		lowp_bvec1_t;

	/// @addtogroup core_precision
	/// @{

	/// 2 components vector of high single-precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<float, highp>		highp_vec2;

	/// 2 components vector of medium single-precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<float, mediump>	mediump_vec2;

	/// 2 components vector of low single-precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<float, lowp>		lowp_vec2;

	/// 2 components vector of high double-precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<double, highp>	highp_dvec2;

	/// 2 components vector of medium double-precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<double, mediump>	mediump_dvec2;

	/// 2 components vector of low double-precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<double, lowp>		lowp_dvec2;

	/// 2 components vector of high precision signed integer numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<int, highp>		highp_ivec2;

	/// 2 components vector of medium precision signed integer numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<int, mediump>		mediump_ivec2;

	/// 2 components vector of low precision signed integer numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<int, lowp>		lowp_ivec2;

	/// 2 components vector of high precision unsigned integer numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<uint, highp>		highp_uvec2;

	/// 2 components vector of medium precision unsigned integer numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<uint, mediump>	mediump_uvec2;

	/// 2 components vector of low precision unsigned integer numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<uint, lowp>		lowp_uvec2;

	/// 2 components vector of high precision bool numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<bool, highp>		highp_bvec2;

	/// 2 components vector of medium precision bool numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<bool, mediump>	mediump_bvec2;

	/// 2 components vector of low precision bool numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec2<bool, lowp>		lowp_bvec2;

	/// @}

	/// @addtogroup core_precision
	/// @{

	/// 3 components vector of high single-precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<float, highp>		highp_vec3;

	/// 3 components vector of medium single-precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<float, mediump>	mediump_vec3;

	/// 3 components vector of low single-precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<float, lowp>		lowp_vec3;

	/// 3 components vector of high double-precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<double, highp>	highp_dvec3;

	/// 3 components vector of medium double-precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<double, mediump>	mediump_dvec3;

	/// 3 components vector of low double-precision floating-point numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<double, lowp>		lowp_dvec3;

	/// 3 components vector of high precision signed integer numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<int, highp>		highp_ivec3;

	/// 3 components vector of medium precision signed integer numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<int, mediump>		mediump_ivec3;

	/// 3 components vector of low precision signed integer numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<int, lowp>		lowp_ivec3;

	/// 3 components vector of high precision unsigned integer numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<uint, highp>		highp_uvec3;

	/// 3 components vector of medium precision unsigned integer numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<uint, mediump>	mediump_uvec3;

	/// 3 components vector of low precision unsigned integer numbers.
	/// There is no guarantee on the actual precision.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<uint, lowp>		lowp_uvec3;

	/// 3 components vector of high precision bool numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<bool, highp>		highp_bvec3;

	/// 3 components vector of medium precision bool numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<bool, mediump>	mediump_bvec3;

	/// 3 components vector of low precision bool numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec3<bool, lowp>		lowp_bvec3;

	/// @}

	/// @addtogroup core_precision
	/// @{

	/// 4 components vector of high single-precision floating-point numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<float, highp>		highp_vec4;

	/// 4 components vector of medium single-precision floating-point numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<float, mediump>	mediump_vec4;

	/// 4 components vector of low single-precision floating-point numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<float, lowp>		lowp_vec4;

	/// 4 components vector of high double-precision floating-point numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<double, highp>	highp_dvec4;

	/// 4 components vector of medium double-precision floating-point numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<double, mediump>	mediump_dvec4;

	/// 4 components vector of low double-precision floating-point numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<double, lowp>		lowp_dvec4;

	/// 4 components vector of high precision signed integer numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<int, highp>		highp_ivec4;

	/// 4 components vector of medium precision signed integer numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<int, mediump>		mediump_ivec4;

	/// 4 components vector of low precision signed integer numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<int, lowp>		lowp_ivec4;

	/// 4 components vector of high precision unsigned integer numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<uint, highp>		highp_uvec4;

	/// 4 components vector of medium precision unsigned integer numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<uint, mediump>	mediump_uvec4;

	/// 4 components vector of low precision unsigned integer numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<uint, lowp>		lowp_uvec4;

	/// 4 components vector of high precision bool numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<bool, highp>		highp_bvec4;

	/// 4 components vector of medium precision bool numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<bool, mediump>	mediump_bvec4;

	/// 4 components vector of low precision bool numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.7.2 Precision Qualifier</a>
	typedef tvec4<bool, lowp>		lowp_bvec4;

	/// @}

	/// @addtogroup core_types
	/// @{

	// -- Default float definition --

#if(defined(GLM_PRECISION_LOWP_FLOAT))
	typedef lowp_vec2			vec2;
	typedef lowp_vec3			vec3;
	typedef lowp_vec4			vec4;
#elif(defined(GLM_PRECISION_MEDIUMP_FLOAT))
	typedef mediump_vec2		vec2;
	typedef mediump_vec3		vec3;
	typedef mediump_vec4		vec4;
#else //defined(GLM_PRECISION_HIGHP_FLOAT)
	/// 2 components vector of floating-point numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_vec2			vec2;

	//! 3 components vector of floating-point numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_vec3			vec3;

	//! 4 components vector of floating-point numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_vec4			vec4;
#endif//GLM_PRECISION

	// -- Default double definition --

#if(defined(GLM_PRECISION_LOWP_DOUBLE))
	typedef lowp_dvec2			dvec2;
	typedef lowp_dvec3			dvec3;
	typedef lowp_dvec4			dvec4;
#elif(defined(GLM_PRECISION_MEDIUMP_DOUBLE))
	typedef mediump_dvec2		dvec2;
	typedef mediump_dvec3		dvec3;
	typedef mediump_dvec4		dvec4;
#else //defined(GLM_PRECISION_HIGHP_DOUBLE)
	/// 2 components vector of double-precision floating-point numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_dvec2			dvec2;

	//! 3 components vector of double-precision floating-point numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_dvec3			dvec3;

	//! 4 components vector of double-precision floating-point numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_dvec4			dvec4;
#endif//GLM_PRECISION

	// -- Signed integer definition --

#if(defined(GLM_PRECISION_LOWP_INT))
	typedef lowp_ivec2			ivec2;
	typedef lowp_ivec3			ivec3;
	typedef lowp_ivec4			ivec4;
#elif(defined(GLM_PRECISION_MEDIUMP_INT))
	typedef mediump_ivec2		ivec2;
	typedef mediump_ivec3		ivec3;
	typedef mediump_ivec4		ivec4;
#else //defined(GLM_PRECISION_HIGHP_INT)
	/// 2 components vector of signed integer numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_ivec2			ivec2;

	/// 3 components vector of signed integer numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_ivec3			ivec3;

	/// 4 components vector of signed integer numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_ivec4			ivec4;
#endif//GLM_PRECISION

	// -- Unsigned integer definition --

#if(defined(GLM_PRECISION_LOWP_UINT))
	typedef lowp_uvec2			uvec2;
	typedef lowp_uvec3			uvec3;
	typedef lowp_uvec4			uvec4;
#elif(defined(GLM_PRECISION_MEDIUMP_UINT))
	typedef mediump_uvec2		uvec2;
	typedef mediump_uvec3		uvec3;
	typedef mediump_uvec4		uvec4;
#else //defined(GLM_PRECISION_HIGHP_UINT)
	/// 2 components vector of unsigned integer numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_uvec2			uvec2;

	/// 3 components vector of unsigned integer numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_uvec3			uvec3;

	/// 4 components vector of unsigned integer numbers.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_uvec4			uvec4;
#endif//GLM_PRECISION

	// -- Boolean definition --

#if(defined(GLM_PRECISION_LOWP_BOOL))
	typedef lowp_bvec2			bvec2;
	typedef lowp_bvec3			bvec3;
	typedef lowp_bvec4			bvec4;
#elif(defined(GLM_PRECISION_MEDIUMP_BOOL))
	typedef mediump_bvec2		bvec2;
	typedef mediump_bvec3		bvec3;
	typedef mediump_bvec4		bvec4;
#else //defined(GLM_PRECISION_HIGHP_BOOL)
	/// 2 components vector of boolean.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_bvec2			bvec2;

	/// 3 components vector of boolean.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_bvec3			bvec3;

	/// 4 components vector of boolean.
	///
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 4.1.5 Vectors</a>
	typedef highp_bvec4			bvec4;
#endif//GLM_PRECISION

	/// @}
}//namespace glm
