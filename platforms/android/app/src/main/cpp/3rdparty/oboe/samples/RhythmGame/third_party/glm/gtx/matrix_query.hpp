/// @ref gtx_matrix_query
/// @file glm/gtx/matrix_query.hpp
///
/// @see core (dependence)
/// @see gtx_vector_query (dependence)
///
/// @defgroup gtx_matrix_query GLM_GTX_matrix_query
/// @ingroup gtx
///
/// @brief Query to evaluate matrix properties
///
/// <glm/gtx/matrix_query.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtx/vector_query.hpp"
#include <limits>

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_matrix_query extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_matrix_query
	/// @{

	/// Return whether a matrix a null matrix.
	/// From GLM_GTX_matrix_query extension.
	template<typename T, precision P>
	GLM_FUNC_DECL bool isNull(tmat2x2<T, P> const & m, T const & epsilon);
		
	/// Return whether a matrix a null matrix.
	/// From GLM_GTX_matrix_query extension.
	template<typename T, precision P>
	GLM_FUNC_DECL bool isNull(tmat3x3<T, P> const & m, T const & epsilon);
		
	/// Return whether a matrix is a null matrix.
	/// From GLM_GTX_matrix_query extension.
	template<typename T, precision P>
	GLM_FUNC_DECL bool isNull(tmat4x4<T, P> const & m, T const & epsilon);
			
	/// Return whether a matrix is an identity matrix.
	/// From GLM_GTX_matrix_query extension.
	template<typename T, precision P, template <typename, precision> class matType>
	GLM_FUNC_DECL bool isIdentity(matType<T, P> const & m, T const & epsilon);

	/// Return whether a matrix is a normalized matrix.
	/// From GLM_GTX_matrix_query extension.
	template<typename T, precision P>
	GLM_FUNC_DECL bool isNormalized(tmat2x2<T, P> const & m, T const & epsilon);

	/// Return whether a matrix is a normalized matrix.
	/// From GLM_GTX_matrix_query extension.
	template<typename T, precision P>
	GLM_FUNC_DECL bool isNormalized(tmat3x3<T, P> const & m, T const & epsilon);

	/// Return whether a matrix is a normalized matrix.
	/// From GLM_GTX_matrix_query extension.
	template<typename T, precision P>
	GLM_FUNC_DECL bool isNormalized(tmat4x4<T, P> const & m, T const & epsilon);

	/// Return whether a matrix is an orthonormalized matrix.
	/// From GLM_GTX_matrix_query extension.
	template<typename T, precision P, template <typename, precision> class matType>
	GLM_FUNC_DECL bool isOrthogonal(matType<T, P> const & m, T const & epsilon);

	/// @}
}//namespace glm

#include "matrix_query.inl"
