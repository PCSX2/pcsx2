/// @ref gtx_std_based_type
/// @file glm/gtx/std_based_type.hpp
///
/// @see core (dependence)
/// @see gtx_extented_min_max (dependence)
///
/// @defgroup gtx_std_based_type GLM_GTX_std_based_type
/// @ingroup gtx
///
/// @brief Adds vector types based on STL value types.
/// <glm/gtx/std_based_type.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"
#include <cstdlib>

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_std_based_type extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_std_based_type
	/// @{

	/// Vector type based of one std::size_t component.
	/// @see GLM_GTX_std_based_type
	typedef tvec1<std::size_t, defaultp>		size1;

	/// Vector type based of two std::size_t components.
	/// @see GLM_GTX_std_based_type
	typedef tvec2<std::size_t, defaultp>		size2;

	/// Vector type based of three std::size_t components.
	/// @see GLM_GTX_std_based_type
	typedef tvec3<std::size_t, defaultp>		size3;

	/// Vector type based of four std::size_t components.
	/// @see GLM_GTX_std_based_type
	typedef tvec4<std::size_t, defaultp>		size4;

	/// Vector type based of one std::size_t component.
	/// @see GLM_GTX_std_based_type
	typedef tvec1<std::size_t, defaultp>		size1_t;

	/// Vector type based of two std::size_t components.
	/// @see GLM_GTX_std_based_type
	typedef tvec2<std::size_t, defaultp>		size2_t;

	/// Vector type based of three std::size_t components.
	/// @see GLM_GTX_std_based_type
	typedef tvec3<std::size_t, defaultp>		size3_t;

	/// Vector type based of four std::size_t components.
	/// @see GLM_GTX_std_based_type
	typedef tvec4<std::size_t, defaultp>		size4_t;

	/// @}
}//namespace glm

#include "std_based_type.inl"
