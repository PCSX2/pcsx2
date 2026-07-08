/// @ref gtx_range
/// @file glm/gtx/range.hpp
/// @author Joshua Moerman
///
/// @defgroup gtx_range GLM_GTX_range
/// @ingroup gtx
///
/// @brief Defines begin and end for vectors and matrices. Useful for range-based for loop.
/// The range is defined over the elements, not over columns or rows (e.g. mat4 has 16 elements).
///
/// <glm/gtx/range.hpp> need to be included to use these functionalities.

#pragma once

// Dependencies
#include "../detail/setup.hpp"

#if !GLM_HAS_RANGE_FOR
#	error "GLM_GTX_range requires C++11 suppport or 'range for'"
#endif

#include "../gtc/type_ptr.hpp"
#include "../gtc/vec1.hpp"

namespace glm
{
	/// @addtogroup gtx_range
	/// @{

	template <typename T, precision P>
	inline length_t components(tvec1<T, P> const & v)
	{
		return v.length();
	}
	
	template <typename T, precision P>
	inline length_t components(tvec2<T, P> const & v)
	{
		return v.length();
	}
	
	template <typename T, precision P>
	inline length_t components(tvec3<T, P> const & v)
	{
		return v.length();
	}
	
	template <typename T, precision P>
	inline length_t components(tvec4<T, P> const & v)
	{
		return v.length();
	}
	
	template <typename genType>
	inline length_t components(genType const & m)
	{
		return m.length() * m[0].length();
	}
	
	template <typename genType>
	inline typename genType::value_type const * begin(genType const & v)
	{
		return value_ptr(v);
	}

	template <typename genType>
	inline typename genType::value_type const * end(genType const & v)
	{
		return begin(v) + components(v);
	}

	template <typename genType>
	inline typename genType::value_type * begin(genType& v)
	{
		return value_ptr(v);
	}

	template <typename genType>
	inline typename genType::value_type * end(genType& v)
	{
		return begin(v) + components(v);
	}

	/// @}
}//namespace glm
