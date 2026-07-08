/// @ref gtx_log_base
/// @file glm/gtx/log_base.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_log_base GLM_GTX_log_base
/// @ingroup gtx
///
/// @brief Logarithm for any base. base can be a vector or a scalar.
///
/// <glm/gtx/log_base.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_log_base extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_log_base
	/// @{

	/// Logarithm for any base.
	/// From GLM_GTX_log_base.
	template <typename genType>
	GLM_FUNC_DECL genType log(
		genType const & x,
		genType const & base);

	/// Logarithm for any base.
	/// From GLM_GTX_log_base.
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<T, P> sign(
		vecType<T, P> const & x,
		vecType<T, P> const & base);

	/// @}
}//namespace glm

#include "log_base.inl"
