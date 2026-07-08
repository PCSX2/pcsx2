/// @ref gtx_associated_min_max
/// @file glm/gtx/associated_min_max.hpp
///
/// @see core (dependence)
/// @see gtx_extented_min_max (dependence)
///
/// @defgroup gtx_associated_min_max GLM_GTX_associated_min_max
/// @ingroup gtx
/// 
/// @brief Min and max functions that return associated values not the compared onces.
/// <glm/gtx/associated_min_max.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_associated_min_max extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_associated_min_max
	/// @{

	/// Minimum comparison between 2 variables and returns 2 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P>
	GLM_FUNC_DECL U associatedMin(T x, U a, T y, U b);

	/// Minimum comparison between 2 variables and returns 2 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL tvec2<U, P> associatedMin(
		vecType<T, P> const & x, vecType<U, P> const & a,
		vecType<T, P> const & y, vecType<U, P> const & b);

	/// Minimum comparison between 2 variables and returns 2 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<U, P> associatedMin(
		T x, const vecType<U, P>& a,
		T y, const vecType<U, P>& b);

	/// Minimum comparison between 2 variables and returns 2 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<U, P> associatedMin(
		vecType<T, P> const & x, U a,
		vecType<T, P> const & y, U b);

	/// Minimum comparison between 3 variables and returns 3 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U>
	GLM_FUNC_DECL U associatedMin(
		T x, U a,
		T y, U b,
		T z, U c);

	/// Minimum comparison between 3 variables and returns 3 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<U, P> associatedMin(
		vecType<T, P> const & x, vecType<U, P> const & a,
		vecType<T, P> const & y, vecType<U, P> const & b,
		vecType<T, P> const & z, vecType<U, P> const & c);

	/// Minimum comparison between 4 variables and returns 4 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U>
	GLM_FUNC_DECL U associatedMin(
		T x, U a,
		T y, U b,
		T z, U c,
		T w, U d);

	/// Minimum comparison between 4 variables and returns 4 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<U, P> associatedMin(
		vecType<T, P> const & x, vecType<U, P> const & a,
		vecType<T, P> const & y, vecType<U, P> const & b,
		vecType<T, P> const & z, vecType<U, P> const & c,
		vecType<T, P> const & w, vecType<U, P> const & d);

	/// Minimum comparison between 4 variables and returns 4 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<U, P> associatedMin(
		T x, vecType<U, P> const & a,
		T y, vecType<U, P> const & b,
		T z, vecType<U, P> const & c,
		T w, vecType<U, P> const & d);

	/// Minimum comparison between 4 variables and returns 4 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<U, P> associatedMin(
		vecType<T, P> const & x, U a,
		vecType<T, P> const & y, U b,
		vecType<T, P> const & z, U c,
		vecType<T, P> const & w, U d);

	/// Maximum comparison between 2 variables and returns 2 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U>
	GLM_FUNC_DECL U associatedMax(T x, U a, T y, U b);

	/// Maximum comparison between 2 variables and returns 2 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL tvec2<U, P> associatedMax(
		vecType<T, P> const & x, vecType<U, P> const & a,
		vecType<T, P> const & y, vecType<U, P> const & b);

	/// Maximum comparison between 2 variables and returns 2 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<T, P> associatedMax(
		T x, vecType<U, P> const & a,
		T y, vecType<U, P> const & b);

	/// Maximum comparison between 2 variables and returns 2 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<U, P> associatedMax(
		vecType<T, P> const & x, U a,
		vecType<T, P> const & y, U b);

	/// Maximum comparison between 3 variables and returns 3 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U>
	GLM_FUNC_DECL U associatedMax(
		T x, U a,
		T y, U b,
		T z, U c);

	/// Maximum comparison between 3 variables and returns 3 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<U, P> associatedMax(
		vecType<T, P> const & x, vecType<U, P> const & a,
		vecType<T, P> const & y, vecType<U, P> const & b,
		vecType<T, P> const & z, vecType<U, P> const & c);

	/// Maximum comparison between 3 variables and returns 3 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<T, P> associatedMax(
		T x, vecType<U, P> const & a,
		T y, vecType<U, P> const & b,
		T z, vecType<U, P> const & c);

	/// Maximum comparison between 3 variables and returns 3 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<U, P> associatedMax(
		vecType<T, P> const & x, U a,
		vecType<T, P> const & y, U b,
		vecType<T, P> const & z, U c);

	/// Maximum comparison between 4 variables and returns 4 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U>
	GLM_FUNC_DECL U associatedMax(
		T x, U a,
		T y, U b,
		T z, U c,
		T w, U d);

	/// Maximum comparison between 4 variables and returns 4 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<U, P> associatedMax(
		vecType<T, P> const & x, vecType<U, P> const & a,
		vecType<T, P> const & y, vecType<U, P> const & b,
		vecType<T, P> const & z, vecType<U, P> const & c,
		vecType<T, P> const & w, vecType<U, P> const & d);

	/// Maximum comparison between 4 variables and returns 4 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<U, P> associatedMax(
		T x, vecType<U, P> const & a,
		T y, vecType<U, P> const & b,
		T z, vecType<U, P> const & c,
		T w, vecType<U, P> const & d);

	/// Maximum comparison between 4 variables and returns 4 associated variable values
	/// @see gtx_associated_min_max
	template<typename T, typename U, precision P, template <typename, precision> class vecType>
	GLM_FUNC_DECL vecType<U, P> associatedMax(
		vecType<T, P> const & x, U a,
		vecType<T, P> const & y, U b,
		vecType<T, P> const & z, U c,
		vecType<T, P> const & w, U d);

	/// @}
} //namespace glm

#include "associated_min_max.inl"
