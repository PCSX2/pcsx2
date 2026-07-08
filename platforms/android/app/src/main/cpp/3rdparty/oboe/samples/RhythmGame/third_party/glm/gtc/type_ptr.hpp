/// @ref gtc_type_ptr
/// @file glm/gtc/type_ptr.hpp
///
/// @see core (dependence)
/// @see gtc_half_float (dependence)
/// @see gtc_quaternion (dependence)
///
/// @defgroup gtc_type_ptr GLM_GTC_type_ptr
/// @ingroup gtc
///
/// @brief Handles the interaction between pointers and vector, matrix types.
///
/// This extension defines an overloaded function, glm::value_ptr, which
/// takes any of the \ref core_template "core template types". It returns
/// a pointer to the memory layout of the object. Matrix types store their values
/// in column-major order.
/// 
/// This is useful for uploading data to matrices or copying data to buffer objects.
///
/// Example:
/// @code
/// #include <glm/glm.hpp>
/// #include <glm/gtc/type_ptr.hpp>
///
/// glm::vec3 aVector(3);
/// glm::mat4 someMatrix(1.0);
///
/// glUniform3fv(uniformLoc, 1, glm::value_ptr(aVector));
/// glUniformMatrix4fv(uniformMatrixLoc, 1, GL_FALSE, glm::value_ptr(someMatrix));
/// @endcode
///
/// <glm/gtc/type_ptr.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../gtc/quaternion.hpp"
#include "../vec2.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"
#include "../mat2x2.hpp"
#include "../mat2x3.hpp"
#include "../mat2x4.hpp"
#include "../mat3x2.hpp"
#include "../mat3x3.hpp"
#include "../mat3x4.hpp"
#include "../mat4x2.hpp"
#include "../mat4x3.hpp"
#include "../mat4x4.hpp"
#include <cstring>

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_type_ptr extension included")
#endif

namespace glm
{
	/// @addtogroup gtc_type_ptr
	/// @{

	/// Return the constant address to the data of the input parameter.
	/// @see gtc_type_ptr
	template<typename genType>
	GLM_FUNC_DECL typename genType::value_type const * value_ptr(genType const & vec);

	/// Build a vector from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tvec2<T, defaultp> make_vec2(T const * const ptr);

	/// Build a vector from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tvec3<T, defaultp> make_vec3(T const * const ptr);

	/// Build a vector from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tvec4<T, defaultp> make_vec4(T const * const ptr);

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tmat2x2<T, defaultp> make_mat2x2(T const * const ptr);

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tmat2x3<T, defaultp> make_mat2x3(T const * const ptr);

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tmat2x4<T, defaultp> make_mat2x4(T const * const ptr);

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tmat3x2<T, defaultp> make_mat3x2(T const * const ptr);

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tmat3x3<T, defaultp> make_mat3x3(T const * const ptr);

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tmat3x4<T, defaultp> make_mat3x4(T const * const ptr);

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tmat4x2<T, defaultp> make_mat4x2(T const * const ptr);

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tmat4x3<T, defaultp> make_mat4x3(T const * const ptr);

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tmat4x4<T, defaultp> make_mat4x4(T const * const ptr);
	
	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tmat2x2<T, defaultp> make_mat2(T const * const ptr);

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tmat3x3<T, defaultp> make_mat3(T const * const ptr);
		
	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tmat4x4<T, defaultp> make_mat4(T const * const ptr);

	/// Build a quaternion from a pointer.
	/// @see gtc_type_ptr
	template<typename T>
	GLM_FUNC_DECL tquat<T, defaultp> make_quat(T const * const ptr);

	/// @}
}//namespace glm

#include "type_ptr.inl"
