/// @ref core
/// @file glm/glm.hpp
///
/// @defgroup core GLM Core
///
/// @brief The core of GLM, which implements exactly and only the GLSL specification to the degree possible.
///
/// The GLM core consists of @ref core_types "C++ types that mirror GLSL types" and
/// C++ functions that mirror the GLSL functions. It also includes 
/// @ref core_precision "a set of precision-based types" that can be used in the appropriate
/// functions. The C++ types are all based on a basic set of @ref core_template "template types".
///
/// The best documentation for GLM Core is the current GLSL specification,
/// <a href="http://www.opengl.org/registry/doc/GLSLangSpec.4.20.8.clean.pdf">version 4.2
/// (pdf file)</a>.
///
/// GLM core functionnalities require <glm/glm.hpp> to be included to be used.
///
/// @defgroup core_types Types
///
/// @brief The standard types defined by the specification.
///
/// These types are all typedefs of more generalized, template types. To see the definition
/// of these template types, go to @ref core_template.
///
/// @ingroup core
///
/// @defgroup core_precision Precision types
///
/// @brief Non-GLSL types that are used to define precision-based types.
///
/// The GLSL language allows the user to define the precision of a particular variable.
/// In OpenGL's GLSL, these precision qualifiers have no effect; they are there for compatibility
/// with OpenGL ES's precision qualifiers, where they @em do have an effect.
///
/// C++ has no language equivalent to precision qualifiers. So GLM provides the next-best thing:
/// a number of typedefs of the @ref core_template that use a particular precision.
///
/// None of these types make any guarantees about the actual precision used.
///
/// @ingroup core
///
/// @defgroup core_template Template types
///
/// @brief The generic template types used as the basis for the core types. 
///
/// These types are all templates used to define the actual @ref core_types.
/// These templetes are implementation details of GLM types and should not be used explicitly.
///
/// @ingroup core

#include "detail/_fixes.hpp"

#pragma once

#include <cmath>
#include <climits>
#include <cfloat>
#include <limits>
#include <cassert>
#include "fwd.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_MESSAGE_CORE_INCLUDED_DISPLAYED)
#	define GLM_MESSAGE_CORE_INCLUDED_DISPLAYED
#	pragma message("GLM: Core library included")
#endif//GLM_MESSAGES

#include "vec2.hpp"
#include "vec3.hpp"
#include "vec4.hpp"
#include "mat2x2.hpp"
#include "mat2x3.hpp"
#include "mat2x4.hpp"
#include "mat3x2.hpp"
#include "mat3x3.hpp"
#include "mat3x4.hpp"
#include "mat4x2.hpp"
#include "mat4x3.hpp"
#include "mat4x4.hpp"

#include "trigonometric.hpp"
#include "exponential.hpp"
#include "common.hpp"
#include "packing.hpp"
#include "geometric.hpp"
#include "matrix.hpp"
#include "vector_relational.hpp"
#include "integer.hpp"
