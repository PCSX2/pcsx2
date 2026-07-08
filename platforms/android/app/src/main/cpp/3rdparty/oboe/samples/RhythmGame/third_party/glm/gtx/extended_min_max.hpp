/// @ref gtx_extended_min_max
/// @file glm/gtx/extended_min_max.hpp
///
/// @see core (dependence)
/// @see gtx_half_float (dependence)
///
/// @defgroup gtx_extented_min_max GLM_GTX_extented_min_max
/// @ingroup gtx
///
/// Min and max functions for 3 to 4 parameters.
///
/// <glm/gtx/extented_min_max.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_extented_min_max extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_extented_min_max
	/// @{

	/// Return the minimum component-wise values of 3 inputs 
	/// @see gtx_extented_min_max
	template <typename T>
	GLM_FUNC_DECL T min(
		T const & x, 
		T const & y, 
		T const & z);

	/// Return the minimum component-wise values of 3 inputs
	/// @see gtx_extented_min_max
	template <typename T, template <typename> class C>
	GLM_FUNC_DECL C<T> min(
		C<T> const & x, 
		typename C<T>::T const & y, 
		typename C<T>::T const & z);

	/// Return the minimum component-wise values of 3 inputs 
	/// @see gtx_extented_min_max
	template <typename T, template <typename> class C>
	GLM_FUNC_DECL C<T> min(
		C<T> const & x, 
		C<T> const & y, 
		C<T> const & z);

	/// Return the minimum component-wise values of 4 inputs 
	/// @see gtx_extented_min_max
	template <typename T>
	GLM_FUNC_DECL T min(
		T const & x, 
		T const & y, 
		T const & z, 
		T const & w);

	/// Return the minimum component-wise values of 4 inputs 
	/// @see gtx_extented_min_max
	template <typename T, template <typename> class C>
	GLM_FUNC_DECL C<T> min(
		C<T> const & x, 
		typename C<T>::T const & y, 
		typename C<T>::T const & z, 
		typename C<T>::T const & w);

	/// Return the minimum component-wise values of 4 inputs
	/// @see gtx_extented_min_max
	template <typename T, template <typename> class C>
	GLM_FUNC_DECL C<T> min(
		C<T> const & x, 
		C<T> const & y, 
		C<T> const & z,
		C<T> const & w);

	/// Return the maximum component-wise values of 3 inputs 
	/// @see gtx_extented_min_max
	template <typename T>
	GLM_FUNC_DECL T max(
		T const & x, 
		T const & y, 
		T const & z);

	/// Return the maximum component-wise values of 3 inputs
	/// @see gtx_extented_min_max
	template <typename T, template <typename> class C>
	GLM_FUNC_DECL C<T> max(
		C<T> const & x, 
		typename C<T>::T const & y, 
		typename C<T>::T const & z);

	/// Return the maximum component-wise values of 3 inputs 
	/// @see gtx_extented_min_max
	template <typename T, template <typename> class C>
	GLM_FUNC_DECL C<T> max(
		C<T> const & x, 
		C<T> const & y, 
		C<T> const & z);

	/// Return the maximum component-wise values of 4 inputs
	/// @see gtx_extented_min_max
	template <typename T>
	GLM_FUNC_DECL T max(
		T const & x, 
		T const & y, 
		T const & z, 
		T const & w);

	/// Return the maximum component-wise values of 4 inputs 
	/// @see gtx_extented_min_max
	template <typename T, template <typename> class C>
	GLM_FUNC_DECL C<T> max(
		C<T> const & x, 
		typename C<T>::T const & y, 
		typename C<T>::T const & z, 
		typename C<T>::T const & w);

	/// Return the maximum component-wise values of 4 inputs 
	/// @see gtx_extented_min_max
	template <typename T, template <typename> class C>
	GLM_FUNC_DECL C<T> max(
		C<T> const & x, 
		C<T> const & y, 
		C<T> const & z, 
		C<T> const & w);

	/// @}
}//namespace glm

#include "extended_min_max.inl"
