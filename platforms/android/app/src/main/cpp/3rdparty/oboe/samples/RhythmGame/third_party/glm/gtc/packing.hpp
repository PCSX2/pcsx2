/// @ref gtc_packing
/// @file glm/gtc/packing.hpp
///
/// @see core (dependence)
///
/// @defgroup gtc_packing GLM_GTC_packing
/// @ingroup gtc
///
/// @brief This extension provides a set of function to convert vertors to packed
/// formats.
///
/// <glm/gtc/packing.hpp> need to be included to use these features.

#pragma once

// Dependency:
#include "type_precision.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_packing extension included")
#endif

namespace glm
{
	/// @addtogroup gtc_packing
	/// @{

	/// First, converts the normalized floating-point value v into a 8-bit integer value.
	/// Then, the results are packed into the returned 8-bit unsigned integer.
	///
	/// The conversion for component c of v to fixed point is done as follows:
	/// packUnorm1x8:	round(clamp(c, 0, +1) * 255.0)
	///
	/// @see gtc_packing
	/// @see uint16 packUnorm2x8(vec2 const & v)
	/// @see uint32 packUnorm4x8(vec4 const & v)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/packUnorm4x8.xml">GLSL packUnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL uint8 packUnorm1x8(float v);

	/// Convert a single 8-bit integer to a normalized floating-point value.
	/// 
	/// The conversion for unpacked fixed-point value f to floating point is done as follows:
	/// unpackUnorm4x8: f / 255.0
	/// 
	/// @see gtc_packing
	/// @see vec2 unpackUnorm2x8(uint16 p)
	/// @see vec4 unpackUnorm4x8(uint32 p)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/unpackUnorm4x8.xml">GLSL unpackUnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL float unpackUnorm1x8(uint8 p);

	/// First, converts each component of the normalized floating-point value v into 8-bit integer values.
	/// Then, the results are packed into the returned 16-bit unsigned integer.
	///
	/// The conversion for component c of v to fixed point is done as follows:
	/// packUnorm2x8:	round(clamp(c, 0, +1) * 255.0)
	///
	/// The first component of the vector will be written to the least significant bits of the output;
	/// the last component will be written to the most significant bits.
	///
	/// @see gtc_packing
	/// @see uint8 packUnorm1x8(float const & v)
	/// @see uint32 packUnorm4x8(vec4 const & v)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/packUnorm4x8.xml">GLSL packUnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL uint16 packUnorm2x8(vec2 const & v);

	/// First, unpacks a single 16-bit unsigned integer p into a pair of 8-bit unsigned integers. 
	/// Then, each component is converted to a normalized floating-point value to generate the returned two-component vector.
	/// 
	/// The conversion for unpacked fixed-point value f to floating point is done as follows:
	/// unpackUnorm4x8: f / 255.0
	/// 
	/// The first component of the returned vector will be extracted from the least significant bits of the input; 
	/// the last component will be extracted from the most significant bits.
	/// 
	/// @see gtc_packing
	/// @see float unpackUnorm1x8(uint8 v)
	/// @see vec4 unpackUnorm4x8(uint32 p)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/unpackUnorm4x8.xml">GLSL unpackUnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL vec2 unpackUnorm2x8(uint16 p);
	
	/// First, converts the normalized floating-point value v into 8-bit integer value.
	/// Then, the results are packed into the returned 8-bit unsigned integer.
	///
	/// The conversion to fixed point is done as follows:
	/// packSnorm1x8:	round(clamp(s, -1, +1) * 127.0)
	///
	/// @see gtc_packing
	/// @see uint16 packSnorm2x8(vec2 const & v)
	/// @see uint32 packSnorm4x8(vec4 const & v)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/packSnorm4x8.xml">GLSL packSnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL uint8 packSnorm1x8(float s);

	/// First, unpacks a single 8-bit unsigned integer p into a single 8-bit signed integers. 
	/// Then, the value is converted to a normalized floating-point value to generate the returned scalar.
	/// 
	/// The conversion for unpacked fixed-point value f to floating point is done as follows:
	/// unpackSnorm1x8: clamp(f / 127.0, -1, +1)
	/// 
	/// @see gtc_packing
	/// @see vec2 unpackSnorm2x8(uint16 p)
	/// @see vec4 unpackSnorm4x8(uint32 p)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/unpackSnorm4x8.xml">GLSL unpackSnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL float unpackSnorm1x8(uint8 p);
	
	/// First, converts each component of the normalized floating-point value v into 8-bit integer values.
	/// Then, the results are packed into the returned 16-bit unsigned integer.
	///
	/// The conversion for component c of v to fixed point is done as follows:
	/// packSnorm2x8:	round(clamp(c, -1, +1) * 127.0)
	///
	/// The first component of the vector will be written to the least significant bits of the output;
	/// the last component will be written to the most significant bits.
	///
	/// @see gtc_packing
	/// @see uint8 packSnorm1x8(float const & v)
	/// @see uint32 packSnorm4x8(vec4 const & v)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/packSnorm4x8.xml">GLSL packSnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL uint16 packSnorm2x8(vec2 const & v);

	/// First, unpacks a single 16-bit unsigned integer p into a pair of 8-bit signed integers. 
	/// Then, each component is converted to a normalized floating-point value to generate the returned two-component vector.
	/// 
	/// The conversion for unpacked fixed-point value f to floating point is done as follows:
	/// unpackSnorm2x8: clamp(f / 127.0, -1, +1)
	/// 
	/// The first component of the returned vector will be extracted from the least significant bits of the input; 
	/// the last component will be extracted from the most significant bits.
	/// 
	/// @see gtc_packing
	/// @see float unpackSnorm1x8(uint8 p)
	/// @see vec4 unpackSnorm4x8(uint32 p)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/unpackSnorm4x8.xml">GLSL unpackSnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL vec2 unpackSnorm2x8(uint16 p);
	
	/// First, converts the normalized floating-point value v into a 16-bit integer value.
	/// Then, the results are packed into the returned 16-bit unsigned integer.
	///
	/// The conversion for component c of v to fixed point is done as follows:
	/// packUnorm1x16:	round(clamp(c, 0, +1) * 65535.0)
	///
	/// @see gtc_packing
	/// @see uint16 packSnorm1x16(float const & v)
	/// @see uint64 packSnorm4x16(vec4 const & v)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/packUnorm4x8.xml">GLSL packUnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL uint16 packUnorm1x16(float v);

	/// First, unpacks a single 16-bit unsigned integer p into a of 16-bit unsigned integers. 
	/// Then, the value is converted to a normalized floating-point value to generate the returned scalar.
	/// 
	/// The conversion for unpacked fixed-point value f to floating point is done as follows:
	/// unpackUnorm1x16: f / 65535.0 
	/// 
	/// @see gtc_packing
	/// @see vec2 unpackUnorm2x16(uint32 p)
	/// @see vec4 unpackUnorm4x16(uint64 p)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/unpackUnorm2x16.xml">GLSL unpackUnorm2x16 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL float unpackUnorm1x16(uint16 p);

	/// First, converts each component of the normalized floating-point value v into 16-bit integer values.
	/// Then, the results are packed into the returned 64-bit unsigned integer.
	///
	/// The conversion for component c of v to fixed point is done as follows:
	/// packUnorm4x16:	round(clamp(c, 0, +1) * 65535.0)
	///
	/// The first component of the vector will be written to the least significant bits of the output;
	/// the last component will be written to the most significant bits.
	///
	/// @see gtc_packing
	/// @see uint16 packUnorm1x16(float const & v)
	/// @see uint32 packUnorm2x16(vec2 const & v)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/packUnorm4x8.xml">GLSL packUnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL uint64 packUnorm4x16(vec4 const & v);

	/// First, unpacks a single 64-bit unsigned integer p into four 16-bit unsigned integers. 
	/// Then, each component is converted to a normalized floating-point value to generate the returned four-component vector.
	/// 
	/// The conversion for unpacked fixed-point value f to floating point is done as follows:
	/// unpackUnormx4x16: f / 65535.0 
	/// 
	/// The first component of the returned vector will be extracted from the least significant bits of the input; 
	/// the last component will be extracted from the most significant bits.
	/// 
	/// @see gtc_packing
	/// @see float unpackUnorm1x16(uint16 p)
	/// @see vec2 unpackUnorm2x16(uint32 p)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/unpackUnorm2x16.xml">GLSL unpackUnorm2x16 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL vec4 unpackUnorm4x16(uint64 p);

	/// First, converts the normalized floating-point value v into 16-bit integer value.
	/// Then, the results are packed into the returned 16-bit unsigned integer.
	///
	/// The conversion to fixed point is done as follows:
	/// packSnorm1x8:	round(clamp(s, -1, +1) * 32767.0)
	///
	/// @see gtc_packing
	/// @see uint32 packSnorm2x16(vec2 const & v)
	/// @see uint64 packSnorm4x16(vec4 const & v)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/packSnorm4x8.xml">GLSL packSnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL uint16 packSnorm1x16(float v);

	/// First, unpacks a single 16-bit unsigned integer p into a single 16-bit signed integers. 
	/// Then, each component is converted to a normalized floating-point value to generate the returned scalar.
	/// 
	/// The conversion for unpacked fixed-point value f to floating point is done as follows:
	/// unpackSnorm1x16: clamp(f / 32767.0, -1, +1)
	/// 
	/// @see gtc_packing
	/// @see vec2 unpackSnorm2x16(uint32 p)
	/// @see vec4 unpackSnorm4x16(uint64 p)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/unpackSnorm1x16.xml">GLSL unpackSnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL float unpackSnorm1x16(uint16 p);

	/// First, converts each component of the normalized floating-point value v into 16-bit integer values.
	/// Then, the results are packed into the returned 64-bit unsigned integer.
	///
	/// The conversion for component c of v to fixed point is done as follows:
	/// packSnorm2x8:	round(clamp(c, -1, +1) * 32767.0)
	///
	/// The first component of the vector will be written to the least significant bits of the output;
	/// the last component will be written to the most significant bits.
	///
	/// @see gtc_packing
	/// @see uint16 packSnorm1x16(float const & v)
	/// @see uint32 packSnorm2x16(vec2 const & v)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/packSnorm4x8.xml">GLSL packSnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL uint64 packSnorm4x16(vec4 const & v);

	/// First, unpacks a single 64-bit unsigned integer p into four 16-bit signed integers. 
	/// Then, each component is converted to a normalized floating-point value to generate the returned four-component vector.
	/// 
	/// The conversion for unpacked fixed-point value f to floating point is done as follows:
	/// unpackSnorm4x16: clamp(f / 32767.0, -1, +1)
	/// 
	/// The first component of the returned vector will be extracted from the least significant bits of the input; 
	/// the last component will be extracted from the most significant bits.
	/// 
	/// @see gtc_packing
	/// @see float unpackSnorm1x16(uint16 p)
	/// @see vec2 unpackSnorm2x16(uint32 p)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/unpackSnorm2x16.xml">GLSL unpackSnorm4x8 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL vec4 unpackSnorm4x16(uint64 p);
	
	/// Returns an unsigned integer obtained by converting the components of a floating-point scalar
	/// to the 16-bit floating-point representation found in the OpenGL Specification,
	/// and then packing this 16-bit value into a 16-bit unsigned integer.
	///
	/// @see gtc_packing
	/// @see uint32 packHalf2x16(vec2 const & v)
	/// @see uint64 packHalf4x16(vec4 const & v)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/packHalf2x16.xml">GLSL packHalf2x16 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL uint16 packHalf1x16(float v);
	
	/// Returns a floating-point scalar with components obtained by unpacking a 16-bit unsigned integer into a 16-bit value,
	/// interpreted as a 16-bit floating-point number according to the OpenGL Specification,
	/// and converting it to 32-bit floating-point values.
	///
	/// @see gtc_packing
	/// @see vec2 unpackHalf2x16(uint32 const & v)
	/// @see vec4 unpackHalf4x16(uint64 const & v)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/unpackHalf2x16.xml">GLSL unpackHalf2x16 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL float unpackHalf1x16(uint16 v);

	/// Returns an unsigned integer obtained by converting the components of a four-component floating-point vector 
	/// to the 16-bit floating-point representation found in the OpenGL Specification, 
	/// and then packing these four 16-bit values into a 64-bit unsigned integer.
	/// The first vector component specifies the 16 least-significant bits of the result; 
	/// the forth component specifies the 16 most-significant bits.
	/// 
	/// @see gtc_packing
	/// @see uint16 packHalf1x16(float const & v)
	/// @see uint32 packHalf2x16(vec2 const & v)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/packHalf2x16.xml">GLSL packHalf2x16 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL uint64 packHalf4x16(vec4 const & v);
	
	/// Returns a four-component floating-point vector with components obtained by unpacking a 64-bit unsigned integer into four 16-bit values,
	/// interpreting those values as 16-bit floating-point numbers according to the OpenGL Specification, 
	/// and converting them to 32-bit floating-point values.
	/// The first component of the vector is obtained from the 16 least-significant bits of v; 
	/// the forth component is obtained from the 16 most-significant bits of v.
	/// 
	/// @see gtc_packing
	/// @see float unpackHalf1x16(uint16 const & v)
	/// @see vec2 unpackHalf2x16(uint32 const & v)
	/// @see <a href="http://www.opengl.org/sdk/docs/manglsl/xhtml/unpackHalf2x16.xml">GLSL unpackHalf2x16 man page</a>
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	GLM_FUNC_DECL vec4 unpackHalf4x16(uint64 p);

	/// Returns an unsigned integer obtained by converting the components of a four-component signed integer vector 
	/// to the 10-10-10-2-bit signed integer representation found in the OpenGL Specification, 
	/// and then packing these four values into a 32-bit unsigned integer.
	/// The first vector component specifies the 10 least-significant bits of the result; 
	/// the forth component specifies the 2 most-significant bits.
	/// 
	/// @see gtc_packing
	/// @see uint32 packI3x10_1x2(uvec4 const & v)
	/// @see uint32 packSnorm3x10_1x2(vec4 const & v)
	/// @see uint32 packUnorm3x10_1x2(vec4 const & v)
	/// @see ivec4 unpackI3x10_1x2(uint32 const & p)
	GLM_FUNC_DECL uint32 packI3x10_1x2(ivec4 const & v);

	/// Unpacks a single 32-bit unsigned integer p into three 10-bit and one 2-bit signed integers. 
	/// 
	/// The first component of the returned vector will be extracted from the least significant bits of the input; 
	/// the last component will be extracted from the most significant bits.
	/// 
	/// @see gtc_packing
	/// @see uint32 packU3x10_1x2(uvec4 const & v)
	/// @see vec4 unpackSnorm3x10_1x2(uint32 const & p);
	/// @see uvec4 unpackI3x10_1x2(uint32 const & p);
	GLM_FUNC_DECL ivec4 unpackI3x10_1x2(uint32 p);

	/// Returns an unsigned integer obtained by converting the components of a four-component unsigned integer vector 
	/// to the 10-10-10-2-bit unsigned integer representation found in the OpenGL Specification, 
	/// and then packing these four values into a 32-bit unsigned integer.
	/// The first vector component specifies the 10 least-significant bits of the result; 
	/// the forth component specifies the 2 most-significant bits.
	/// 
	/// @see gtc_packing
	/// @see uint32 packI3x10_1x2(ivec4 const & v)
	/// @see uint32 packSnorm3x10_1x2(vec4 const & v)
	/// @see uint32 packUnorm3x10_1x2(vec4 const & v)
	/// @see ivec4 unpackU3x10_1x2(uint32 const & p)
	GLM_FUNC_DECL uint32 packU3x10_1x2(uvec4 const & v);

	/// Unpacks a single 32-bit unsigned integer p into three 10-bit and one 2-bit unsigned integers. 
	/// 
	/// The first component of the returned vector will be extracted from the least significant bits of the input; 
	/// the last component will be extracted from the most significant bits.
	/// 
	/// @see gtc_packing
	/// @see uint32 packU3x10_1x2(uvec4 const & v)
	/// @see vec4 unpackSnorm3x10_1x2(uint32 const & p);
	/// @see uvec4 unpackI3x10_1x2(uint32 const & p);
	GLM_FUNC_DECL uvec4 unpackU3x10_1x2(uint32 p);

	/// First, converts the first three components of the normalized floating-point value v into 10-bit signed integer values.
	/// Then, converts the forth component of the normalized floating-point value v into 2-bit signed integer values.
	/// Then, the results are packed into the returned 32-bit unsigned integer.
	///
	/// The conversion for component c of v to fixed point is done as follows:
	/// packSnorm3x10_1x2(xyz):	round(clamp(c, -1, +1) * 511.0)
	/// packSnorm3x10_1x2(w):	round(clamp(c, -1, +1) * 1.0)
	///
	/// The first vector component specifies the 10 least-significant bits of the result; 
	/// the forth component specifies the 2 most-significant bits.
	///
	/// @see gtc_packing
	/// @see vec4 unpackSnorm3x10_1x2(uint32 const & p)
	/// @see uint32 packUnorm3x10_1x2(vec4 const & v)
	/// @see uint32 packU3x10_1x2(uvec4 const & v)
	/// @see uint32 packI3x10_1x2(ivec4 const & v)
	GLM_FUNC_DECL uint32 packSnorm3x10_1x2(vec4 const & v);

	/// First, unpacks a single 32-bit unsigned integer p into four 16-bit signed integers. 
	/// Then, each component is converted to a normalized floating-point value to generate the returned four-component vector.
	/// 
	/// The conversion for unpacked fixed-point value f to floating point is done as follows:
	/// unpackSnorm3x10_1x2(xyz): clamp(f / 511.0, -1, +1)
	/// unpackSnorm3x10_1x2(w): clamp(f / 511.0, -1, +1)
	/// 
	/// The first component of the returned vector will be extracted from the least significant bits of the input; 
	/// the last component will be extracted from the most significant bits.
	/// 
	/// @see gtc_packing
	/// @see uint32 packSnorm3x10_1x2(vec4 const & v)
	/// @see vec4 unpackUnorm3x10_1x2(uint32 const & p))
	/// @see uvec4 unpackI3x10_1x2(uint32 const & p)
	/// @see uvec4 unpackU3x10_1x2(uint32 const & p)
	GLM_FUNC_DECL vec4 unpackSnorm3x10_1x2(uint32 p);

	/// First, converts the first three components of the normalized floating-point value v into 10-bit unsigned integer values.
	/// Then, converts the forth component of the normalized floating-point value v into 2-bit signed uninteger values.
	/// Then, the results are packed into the returned 32-bit unsigned integer.
	///
	/// The conversion for component c of v to fixed point is done as follows:
	/// packUnorm3x10_1x2(xyz):	round(clamp(c, 0, +1) * 1023.0)
	/// packUnorm3x10_1x2(w):	round(clamp(c, 0, +1) * 3.0)
	///
	/// The first vector component specifies the 10 least-significant bits of the result; 
	/// the forth component specifies the 2 most-significant bits.
	///
	/// @see gtc_packing
	/// @see vec4 unpackUnorm3x10_1x2(uint32 const & p)
	/// @see uint32 packUnorm3x10_1x2(vec4 const & v)
	/// @see uint32 packU3x10_1x2(uvec4 const & v)
	/// @see uint32 packI3x10_1x2(ivec4 const & v)
	GLM_FUNC_DECL uint32 packUnorm3x10_1x2(vec4 const & v);

	/// First, unpacks a single 32-bit unsigned integer p into four 16-bit signed integers. 
	/// Then, each component is converted to a normalized floating-point value to generate the returned four-component vector.
	/// 
	/// The conversion for unpacked fixed-point value f to floating point is done as follows:
	/// unpackSnorm3x10_1x2(xyz): clamp(f / 1023.0, 0, +1)
	/// unpackSnorm3x10_1x2(w): clamp(f / 3.0, 0, +1)
	/// 
	/// The first component of the returned vector will be extracted from the least significant bits of the input; 
	/// the last component will be extracted from the most significant bits.
	/// 
	/// @see gtc_packing
	/// @see uint32 packSnorm3x10_1x2(vec4 const & v)
	/// @see vec4 unpackInorm3x10_1x2(uint32 const & p))
	/// @see uvec4 unpackI3x10_1x2(uint32 const & p)
	/// @see uvec4 unpackU3x10_1x2(uint32 const & p)
	GLM_FUNC_DECL vec4 unpackUnorm3x10_1x2(uint32 p);

	/// First, converts the first two components of the normalized floating-point value v into 11-bit signless floating-point values.
	/// Then, converts the third component of the normalized floating-point value v into a 10-bit signless floating-point value.
	/// Then, the results are packed into the returned 32-bit unsigned integer.
	///
	/// The first vector component specifies the 11 least-significant bits of the result; 
	/// the last component specifies the 10 most-significant bits.
	///
	/// @see gtc_packing
	/// @see vec3 unpackF2x11_1x10(uint32 const & p)
	GLM_FUNC_DECL uint32 packF2x11_1x10(vec3 const & v);

	/// First, unpacks a single 32-bit unsigned integer p into two 11-bit signless floating-point values and one 10-bit signless floating-point value . 
	/// Then, each component is converted to a normalized floating-point value to generate the returned three-component vector.
	/// 
	/// The first component of the returned vector will be extracted from the least significant bits of the input; 
	/// the last component will be extracted from the most significant bits.
	/// 
	/// @see gtc_packing
	/// @see uint32 packF2x11_1x10(vec3 const & v)
	GLM_FUNC_DECL vec3 unpackF2x11_1x10(uint32 p);


	/// First, converts the first two components of the normalized floating-point value v into 11-bit signless floating-point values.
	/// Then, converts the third component of the normalized floating-point value v into a 10-bit signless floating-point value.
	/// Then, the results are packed into the returned 32-bit unsigned integer.
	///
	/// The first vector component specifies the 11 least-significant bits of the result; 
	/// the last component specifies the 10 most-significant bits.
	///
	/// @see gtc_packing
	/// @see vec3 unpackF3x9_E1x5(uint32 const & p)
	GLM_FUNC_DECL uint32 packF3x9_E1x5(vec3 const & v);

	/// First, unpacks a single 32-bit unsigned integer p into two 11-bit signless floating-point values and one 10-bit signless floating-point value . 
	/// Then, each component is converted to a normalized floating-point value to generate the returned three-component vector.
	/// 
	/// The first component of the returned vector will be extracted from the least significant bits of the input; 
	/// the last component will be extracted from the most significant bits.
	/// 
	/// @see gtc_packing
	/// @see uint32 packF3x9_E1x5(vec3 const & v)
	GLM_FUNC_DECL vec3 unpackF3x9_E1x5(uint32 p);

	/// Returns an unsigned integer vector obtained by converting the components of a floating-point vector
	/// to the 16-bit floating-point representation found in the OpenGL Specification.
	/// The first vector component specifies the 16 least-significant bits of the result; 
	/// the forth component specifies the 16 most-significant bits.
	/// 
	/// @see gtc_packing
	/// @see vecType<float, P> unpackHalf(vecType<uint16, P> const & p)
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	template <precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<uint16, P> packHalf(vecType<float, P> const & v);

	/// Returns a floating-point vector with components obtained by reinterpreting an integer vector as 16-bit floating-point numbers and converting them to 32-bit floating-point values.
	/// The first component of the vector is obtained from the 16 least-significant bits of v;
	/// the forth component is obtained from the 16 most-significant bits of v.
	/// 
	/// @see gtc_packing
	/// @see vecType<uint16, P> packHalf(vecType<float, P> const & v)
	/// @see <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.pdf">GLSL 4.20.8 specification, section 8.4 Floating-Point Pack and Unpack Functions</a>
	template <precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<float, P> unpackHalf(vecType<uint16, P> const & p);

	/// Convert each component of the normalized floating-point vector into unsigned integer values.
	///
	/// @see gtc_packing
	/// @see vecType<floatType, P> unpackUnorm(vecType<intType, P> const & p);
	template <typename uintType, typename floatType, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<uintType, P> packUnorm(vecType<floatType, P> const & v);

	/// Convert each unsigned integer components of a vector to normalized floating-point values.
	/// 
	/// @see gtc_packing
	/// @see vecType<intType, P> packUnorm(vecType<floatType, P> const & v)
	template <typename uintType, typename floatType, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<floatType, P> unpackUnorm(vecType<uintType, P> const & v);

	/// Convert each component of the normalized floating-point vector into signed integer values.
	///
	/// @see gtc_packing
	/// @see vecType<floatType, P> unpackSnorm(vecType<intType, P> const & p);
	template <typename intType, typename floatType, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<intType, P> packSnorm(vecType<floatType, P> const & v);

	/// Convert each signed integer components of a vector to normalized floating-point values.
	/// 
	/// @see gtc_packing
	/// @see vecType<intType, P> packSnorm(vecType<floatType, P> const & v)
	template <typename intType, typename floatType, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<floatType, P> unpackSnorm(vecType<intType, P> const & v);

	/// Convert each component of the normalized floating-point vector into unsigned integer values.
	///
	/// @see gtc_packing
	/// @see vec2 unpackUnorm2x4(uint8 p)
	GLM_FUNC_DECL uint8 packUnorm2x4(vec2 const & v);

	/// Convert each unsigned integer components of a vector to normalized floating-point values.
	/// 
	/// @see gtc_packing
	/// @see uint8 packUnorm2x4(vec2 const & v)
	GLM_FUNC_DECL vec2 unpackUnorm2x4(uint8 p);

	/// Convert each component of the normalized floating-point vector into unsigned integer values.
	///
	/// @see gtc_packing
	/// @see vec4 unpackUnorm4x4(uint16 p)
	GLM_FUNC_DECL uint16 packUnorm4x4(vec4 const & v);

	/// Convert each unsigned integer components of a vector to normalized floating-point values.
	/// 
	/// @see gtc_packing
	/// @see uint16 packUnorm4x4(vec4 const & v)
	GLM_FUNC_DECL vec4 unpackUnorm4x4(uint16 p);

	/// Convert each component of the normalized floating-point vector into unsigned integer values.
	///
	/// @see gtc_packing
	/// @see vec3 unpackUnorm1x5_1x6_1x5(uint16 p)
	GLM_FUNC_DECL uint16 packUnorm1x5_1x6_1x5(vec3 const & v);

	/// Convert each unsigned integer components of a vector to normalized floating-point values.
	/// 
	/// @see gtc_packing
	/// @see uint16 packUnorm1x5_1x6_1x5(vec3 const & v)
	GLM_FUNC_DECL vec3 unpackUnorm1x5_1x6_1x5(uint16 p);

	/// Convert each component of the normalized floating-point vector into unsigned integer values.
	///
	/// @see gtc_packing
	/// @see vec4 unpackUnorm3x5_1x1(uint16 p)
	GLM_FUNC_DECL uint16 packUnorm3x5_1x1(vec4 const & v);

	/// Convert each unsigned integer components of a vector to normalized floating-point values.
	/// 
	/// @see gtc_packing
	/// @see uint16 packUnorm3x5_1x1(vec4 const & v)
	GLM_FUNC_DECL vec4 unpackUnorm3x5_1x1(uint16 p);

	/// Convert each component of the normalized floating-point vector into unsigned integer values.
	///
	/// @see gtc_packing
	/// @see vec3 unpackUnorm2x3_1x2(uint8 p)
	GLM_FUNC_DECL uint8 packUnorm2x3_1x2(vec3 const & v);

	/// Convert each unsigned integer components of a vector to normalized floating-point values.
	/// 
	/// @see gtc_packing
	/// @see uint8 packUnorm2x3_1x2(vec3 const & v)
	GLM_FUNC_DECL vec3 unpackUnorm2x3_1x2(uint8 p);
	/// @}
}// namespace glm

#include "packing.inl"
