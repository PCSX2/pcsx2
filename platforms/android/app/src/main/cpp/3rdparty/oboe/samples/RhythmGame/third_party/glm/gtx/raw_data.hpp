/// @ref gtx_raw_data
/// @file glm/gtx/raw_data.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_raw_data GLM_GTX_raw_data
/// @ingroup gtx
///
/// @brief Projection of a vector to other one
///
/// <glm/gtx/raw_data.hpp> need to be included to use these functionalities.

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/type_int.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_raw_data extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_raw_data
	/// @{

	//! Type for byte numbers. 
	//! From GLM_GTX_raw_data extension.
	typedef detail::uint8		byte;

	//! Type for word numbers. 
	//! From GLM_GTX_raw_data extension.
	typedef detail::uint16		word;

	//! Type for dword numbers. 
	//! From GLM_GTX_raw_data extension.
	typedef detail::uint32		dword;

	//! Type for qword numbers. 
	//! From GLM_GTX_raw_data extension.
	typedef detail::uint64		qword;

	/// @}
}// namespace glm

#include "raw_data.inl"
