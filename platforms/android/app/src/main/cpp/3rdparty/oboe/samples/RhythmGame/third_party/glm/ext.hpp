/// @file glm/ext.hpp
///
/// @ref core (Dependence)
/// 
/// @defgroup gtc GTC Extensions (Stable)
///
/// @brief Functions and types that the GLSL specification doesn't define, but useful to have for a C++ program.
/// 
/// GTC extensions aim to be stable. 
/// 
/// Even if it's highly unrecommended, it's possible to include all the extensions at once by
/// including <glm/ext.hpp>. Otherwise, each extension needs to be included  a specific file.
/// 
/// @defgroup gtx GTX Extensions (Experimental)
/// 
/// @brief Functions and types that the GLSL specification doesn't define, but 
/// useful to have for a C++ program.
/// 
/// Experimental extensions are useful functions and types, but the development of
/// their API and functionality is not necessarily stable. They can change 
/// substantially between versions. Backwards compatibility is not much of an issue
/// for them.
/// 
/// Even if it's highly unrecommended, it's possible to include all the extensions 
/// at once by including <glm/ext.hpp>. Otherwise, each extension needs to be 
/// included  a specific file.

#pragma once

#include "glm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_MESSAGE_EXT_INCLUDED_DISPLAYED)
#	define GLM_MESSAGE_EXT_INCLUDED_DISPLAYED
#	pragma message("GLM: All extensions included (not recommanded)")
#endif//GLM_MESSAGES

#include "./gtc/bitfield.hpp"
#include "./gtc/color_space.hpp"
#include "./gtc/constants.hpp"
#include "./gtc/epsilon.hpp"
#include "./gtc/functions.hpp"
#include "./gtc/integer.hpp"
#include "./gtc/matrix_access.hpp"
#include "./gtc/matrix_integer.hpp"
#include "./gtc/matrix_inverse.hpp"
#include "./gtc/matrix_transform.hpp"
#include "./gtc/noise.hpp"
#include "./gtc/packing.hpp"
#include "./gtc/quaternion.hpp"
#include "./gtc/random.hpp"
#include "./gtc/reciprocal.hpp"
#include "./gtc/round.hpp"
//#include "./gtc/type_aligned.hpp"
#include "./gtc/type_precision.hpp"
#include "./gtc/type_ptr.hpp"
#include "./gtc/ulp.hpp"
#include "./gtc/vec1.hpp"
#if GLM_HAS_ALIGNED_TYPE
#	include "./gtc/type_aligned.hpp"
#endif

#include "./gtx/associated_min_max.hpp"
#include "./gtx/bit.hpp"
#include "./gtx/closest_point.hpp"
#include "./gtx/color_space.hpp"
#include "./gtx/color_space_YCoCg.hpp"
#include "./gtx/compatibility.hpp"
#include "./gtx/component_wise.hpp"
#include "./gtx/dual_quaternion.hpp"
#include "./gtx/euler_angles.hpp"
#include "./gtx/extend.hpp"
#include "./gtx/extended_min_max.hpp"
#include "./gtx/fast_exponential.hpp"
#include "./gtx/fast_square_root.hpp"
#include "./gtx/fast_trigonometry.hpp"
#include "./gtx/gradient_paint.hpp"
#include "./gtx/handed_coordinate_space.hpp"
#include "./gtx/integer.hpp"
#include "./gtx/intersect.hpp"
#include "./gtx/log_base.hpp"
#include "./gtx/matrix_cross_product.hpp"
#include "./gtx/matrix_interpolation.hpp"
#include "./gtx/matrix_major_storage.hpp"
#include "./gtx/matrix_operation.hpp"
#include "./gtx/matrix_query.hpp"
#include "./gtx/mixed_product.hpp"
#include "./gtx/norm.hpp"
#include "./gtx/normal.hpp"
#include "./gtx/normalize_dot.hpp"
#include "./gtx/number_precision.hpp"
#include "./gtx/optimum_pow.hpp"
#include "./gtx/orthonormalize.hpp"
#include "./gtx/perpendicular.hpp"
#include "./gtx/polar_coordinates.hpp"
#include "./gtx/projection.hpp"
#include "./gtx/quaternion.hpp"
#include "./gtx/raw_data.hpp"
#include "./gtx/rotate_vector.hpp"
#include "./gtx/spline.hpp"
#include "./gtx/std_based_type.hpp"
#if !(GLM_COMPILER & GLM_COMPILER_CUDA)
#	include "./gtx/string_cast.hpp"
#endif
#include "./gtx/transform.hpp"
#include "./gtx/transform2.hpp"
#include "./gtx/vector_angle.hpp"
#include "./gtx/vector_query.hpp"
#include "./gtx/wrap.hpp"

#if GLM_HAS_TEMPLATE_ALIASES
#	include "./gtx/scalar_multiplication.hpp"
#endif

#if GLM_HAS_RANGE_FOR
#	include "./gtx/range.hpp"
#endif
