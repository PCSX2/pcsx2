/// @ref gtx_fast_square_root
/// @file glm/gtx/fast_square_root.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_fast_square_root GLM_GTX_fast_square_root
/// @ingroup gtx
///
/// @brief Fast but less accurate implementations of square root based functions.
/// - Sqrt optimisation based on Newton's method, 
/// www.gamedev.net/community/forums/topic.asp?topic id=139956
///
/// <glm/gtx/fast_square_root.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../common.hpp"
#include "../exponential.hpp"
#include "../geometric.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_fast_square_root extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_fast_square_root
	/// @{

	/// Faster than the common sqrt function but less accurate.
	///
	/// @see gtx_fast_square_root extension.
	template <typename genType> 
	GLM_FUNC_DECL genType fastSqrt(genType x);

	/// Faster than the common sqrt function but less accurate.
	///
	/// @see gtx_fast_square_root extension.
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<T, P> fastSqrt(vecType<T, P> const & x);

	/// Faster than the common inversesqrt function but less accurate.
	///
	/// @see gtx_fast_square_root extension.
	template <typename genType> 
	GLM_FUNC_DECL genType fastInverseSqrt(genType x);

	/// Faster than the common inversesqrt function but less accurate.
	///
	/// @see gtx_fast_square_root extension.
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<T, P> fastInverseSqrt(vecType<T, P> const & x);

	/// Faster than the common length function but less accurate.
	///
	/// @see gtx_fast_square_root extension.
	template <typename genType>
	GLM_FUNC_DECL genType fastLength(genType x);

	/// Faster than the common length function but less accurate.
	///
	/// @see gtx_fast_square_root extension.
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL T fastLength(vecType<T, P> const & x);

	/// Faster than the common distance function but less accurate.
	///
	/// @see gtx_fast_square_root extension.
	template <typename genType>
	GLM_FUNC_DECL genType fastDistance(genType x, genType y);

	/// Faster than the common distance function but less accurate.
	///
	/// @see gtx_fast_square_root extension.
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL T fastDistance(vecType<T, P> const & x, vecType<T, P> const & y);

	/// Faster than the common normalize function but less accurate.
	///
	/// @see gtx_fast_square_root extension.
	template <typename genType> 
	GLM_FUNC_DECL genType fastNormalize(genType const & x);

	/// @}
}// namespace glm

#include "fast_square_root.inl"
