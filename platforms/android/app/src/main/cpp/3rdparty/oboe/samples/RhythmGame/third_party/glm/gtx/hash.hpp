/// @ref gtx_hash
/// @file glm/gtx/hash.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_hash GLM_GTX_hash
/// @ingroup gtx
/// 
/// @brief Add std::hash support for glm types
/// 
/// <glm/gtx/hash.hpp> need to be included to use these functionalities.

#pragma once

#include <functional>

#include "../vec2.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"
#include "../gtc/vec1.hpp"

#include "../gtc/quaternion.hpp"
#include "../gtx/dual_quaternion.hpp"

#include "../mat2x2.hpp"
#include "../mat2x3.hpp"
#include "../mat2x4.hpp"

#include "../mat3x2.hpp"
#include "../mat3x3.hpp"
#include "../mat3x4.hpp"

#include "../mat4x2.hpp"
#include "../mat4x3.hpp"
#include "../mat4x4.hpp"

#if !GLM_HAS_CXX11_STL
#	error "GLM_GTX_hash requires C++11 standard library support"
#endif

namespace std
{
	template <typename T, glm::precision P>
	struct hash<glm::tvec1<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tvec1<T, P> const & v) const;
	};

	template <typename T, glm::precision P>
	struct hash<glm::tvec2<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tvec2<T, P> const & v) const;
	};

	template <typename T, glm::precision P>
	struct hash<glm::tvec3<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tvec3<T, P> const & v) const;
	};

	template <typename T, glm::precision P>
	struct hash<glm::tvec4<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tvec4<T, P> const & v) const;
	};

	template <typename T, glm::precision P>
	struct hash<glm::tquat<T,P>>
	{
		GLM_FUNC_DECL size_t operator()(glm::tquat<T, P> const & q) const;
	};

	template <typename T, glm::precision P>
	struct hash<glm::tdualquat<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tdualquat<T,P> const & q) const;
	};

	template <typename T, glm::precision P>
	struct hash<glm::tmat2x2<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tmat2x2<T,P> const & m) const;
	};

	template <typename T, glm::precision P>
	struct hash<glm::tmat2x3<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tmat2x3<T,P> const & m) const;
	};

	template <typename T, glm::precision P>
	struct hash<glm::tmat2x4<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tmat2x4<T,P> const & m) const;
	};

	template <typename T, glm::precision P>
	struct hash<glm::tmat3x2<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tmat3x2<T,P> const & m) const;
	};

	template <typename T, glm::precision P>
	struct hash<glm::tmat3x3<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tmat3x3<T,P> const & m) const;
	};

	template <typename T, glm::precision P>
	struct hash<glm::tmat3x4<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tmat3x4<T,P> const & m) const;
	};

	template <typename T, glm::precision P>
	struct hash<glm::tmat4x2<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tmat4x2<T,P> const & m) const;
	};
	
	template <typename T, glm::precision P>
	struct hash<glm::tmat4x3<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tmat4x3<T,P> const & m) const;
	};

	template <typename T, glm::precision P>
	struct hash<glm::tmat4x4<T,P> >
	{
		GLM_FUNC_DECL size_t operator()(glm::tmat4x4<T,P> const & m) const;
	};
} // namespace std

#include "hash.inl"
