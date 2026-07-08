/// @ref gtc_type_ptr
/// @file glm/gtc/type_ptr.inl

#include <cstring>

namespace glm
{
	/// @addtogroup gtc_type_ptr
	/// @{

	/// Return the constant address to the data of the vector input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T const * value_ptr
	(
		tvec2<T, P> const & vec
	)
	{
		return &(vec.x);
	}

	//! Return the address to the data of the vector input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T * value_ptr
	(
		tvec2<T, P> & vec
	)
	{
		return &(vec.x);
	}

	/// Return the constant address to the data of the vector input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T const * value_ptr
	(
		tvec3<T, P> const & vec
	)
	{
		return &(vec.x);
	}

	//! Return the address to the data of the vector input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T * value_ptr
	(
		tvec3<T, P> & vec
	)
	{
		return &(vec.x);
	}
		
	/// Return the constant address to the data of the vector input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T const * value_ptr
	(	
		tvec4<T, P> const & vec
	)
	{
		return &(vec.x);
	}

	//! Return the address to the data of the vector input.
	//! From GLM_GTC_type_ptr extension.
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T * value_ptr
	(	
		tvec4<T, P> & vec
	)
	{
		return &(vec.x);
	}

	/// Return the constant address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T const * value_ptr
	(
		tmat2x2<T, P> const & mat
	)
	{
		return &(mat[0].x);
	}

	//! Return the address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T * value_ptr
	(
		tmat2x2<T, P> & mat
	)
	{
		return &(mat[0].x);
	}
		
	/// Return the constant address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T const * value_ptr
	(
		tmat3x3<T, P> const & mat
	)
	{
		return &(mat[0].x);
	}

	//! Return the address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T * value_ptr
	(
		tmat3x3<T, P> & mat
	)
	{
		return &(mat[0].x);
	}
		
	/// Return the constant address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T const * value_ptr
	(
		tmat4x4<T, P> const & mat
	)
	{
		return &(mat[0].x);
	}

	//! Return the address to the data of the matrix input.
	//! From GLM_GTC_type_ptr extension.
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T * value_ptr
	(
		tmat4x4<T, P> & mat
	)
	{
		return &(mat[0].x);
	}

	/// Return the constant address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T const * value_ptr
	(
		tmat2x3<T, P> const & mat
	)
	{
		return &(mat[0].x);
	}

	//! Return the address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T * value_ptr
	(
		tmat2x3<T, P> & mat
	)
	{
		return &(mat[0].x);
	}
		
	/// Return the constant address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T const * value_ptr
	(
		tmat3x2<T, P> const & mat
	)
	{
		return &(mat[0].x);
	}

	//! Return the address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T * value_ptr
	(
		tmat3x2<T, P> & mat
	)
	{
		return &(mat[0].x);
	}
		
	/// Return the constant address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T const * value_ptr
	(
		tmat2x4<T, P> const & mat
	)
	{
		return &(mat[0].x);
	}

	//! Return the address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T * value_ptr
	(
		tmat2x4<T, P> & mat
	)
	{
		return &(mat[0].x);
	}
		
	/// Return the constant address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T const * value_ptr
	(
		tmat4x2<T, P> const & mat
	)
	{
		return &(mat[0].x);
	}

	//! Return the address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T * value_ptr
	(	
		tmat4x2<T, P> & mat
	)
	{
		return &(mat[0].x);
	}
		
	/// Return the constant address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T const * value_ptr
	(
		tmat3x4<T, P> const & mat
	)
	{
		return &(mat[0].x);
	}

	//! Return the address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T * value_ptr
	(
		tmat3x4<T, P> & mat
	)
	{
		return &(mat[0].x);
	}
		
	/// Return the constant address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T const * value_ptr
	(
		tmat4x3<T, P> const & mat
	)
	{
		return &(mat[0].x);
	}

	/// Return the address to the data of the matrix input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T * value_ptr(tmat4x3<T, P> & mat)
	{
		return &(mat[0].x);
	}

	/// Return the constant address to the data of the input parameter.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T const * value_ptr
	(
		tquat<T, P> const & q
	)
	{
		return &(q[0]);
	}

	/// Return the address to the data of the quaternion input.
	/// @see gtc_type_ptr
	template<typename T, precision P>
	GLM_FUNC_QUALIFIER T * value_ptr
	(
		tquat<T, P> & q
	)
	{
		return &(q[0]);
	}

	/// Build a vector from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tvec2<T, defaultp> make_vec2(T const * const ptr)
	{
		tvec2<T, defaultp> Result;
		memcpy(value_ptr(Result), ptr, sizeof(tvec2<T, defaultp>));
		return Result;
	}

	/// Build a vector from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tvec3<T, defaultp> make_vec3(T const * const ptr)
	{
		tvec3<T, defaultp> Result;
		memcpy(value_ptr(Result), ptr, sizeof(tvec3<T, defaultp>));
		return Result;
	}

	/// Build a vector from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tvec4<T, defaultp> make_vec4(T const * const ptr)
	{
		tvec4<T, defaultp> Result;
		memcpy(value_ptr(Result), ptr, sizeof(tvec4<T, defaultp>));
		return Result;
	}

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tmat2x2<T, defaultp> make_mat2x2(T const * const ptr)
	{
		tmat2x2<T, defaultp> Result;
		memcpy(value_ptr(Result), ptr, sizeof(tmat2x2<T, defaultp>));
		return Result;
	}

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tmat2x3<T, defaultp> make_mat2x3(T const * const ptr)
	{
		tmat2x3<T, defaultp> Result;
		memcpy(value_ptr(Result), ptr, sizeof(tmat2x3<T, defaultp>));
		return Result;
	}

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tmat2x4<T, defaultp> make_mat2x4(T const * const ptr)
	{
		tmat2x4<T, defaultp> Result;
		memcpy(value_ptr(Result), ptr, sizeof(tmat2x4<T, defaultp>));
		return Result;
	}

	/// Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tmat3x2<T, defaultp> make_mat3x2(T const * const ptr)
	{
		tmat3x2<T, defaultp> Result;
		memcpy(value_ptr(Result), ptr, sizeof(tmat3x2<T, defaultp>));
		return Result;
	}

	//! Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tmat3x3<T, defaultp> make_mat3x3(T const * const ptr)
	{
		tmat3x3<T, defaultp> Result;
		memcpy(value_ptr(Result), ptr, sizeof(tmat3x3<T, defaultp>));
		return Result;
	}

	//! Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tmat3x4<T, defaultp> make_mat3x4(T const * const ptr)
	{
		tmat3x4<T, defaultp> Result;
		memcpy(value_ptr(Result), ptr, sizeof(tmat3x4<T, defaultp>));
		return Result;
	}

	//! Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tmat4x2<T, defaultp> make_mat4x2(T const * const ptr)
	{
		tmat4x2<T, defaultp> Result;
		memcpy(value_ptr(Result), ptr, sizeof(tmat4x2<T, defaultp>));
		return Result;
	}

	//! Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tmat4x3<T, defaultp> make_mat4x3(T const * const ptr)
	{
		tmat4x3<T, defaultp> Result;
		memcpy(value_ptr(Result), ptr, sizeof(tmat4x3<T, defaultp>));
		return Result;
	}

	//! Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tmat4x4<T, defaultp> make_mat4x4(T const * const ptr)
	{
		tmat4x4<T, defaultp> Result;
		memcpy(value_ptr(Result), ptr, sizeof(tmat4x4<T, defaultp>));
		return Result;
	}

	//! Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tmat2x2<T, defaultp> make_mat2(T const * const ptr)
	{
		return make_mat2x2(ptr);
	}

	//! Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tmat3x3<T, defaultp> make_mat3(T const * const ptr)
	{
		return make_mat3x3(ptr);
	}
		
	//! Build a matrix from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tmat4x4<T, defaultp> make_mat4(T const * const ptr)
	{
		return make_mat4x4(ptr);
	}

	//! Build a quaternion from a pointer.
	/// @see gtc_type_ptr
	template <typename T>
	GLM_FUNC_QUALIFIER tquat<T, defaultp> make_quat(T const * const ptr)
	{
		tquat<T, defaultp> Result;
		memcpy(value_ptr(Result), ptr, sizeof(tquat<T, defaultp>));
		return Result;
	}

	/// @}
}//namespace glm

