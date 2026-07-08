/// @ref gtx_hash
/// @file glm/gtx/hash.inl
///
/// @see core (dependence)
///
/// @defgroup gtx_hash GLM_GTX_hash
/// @ingroup gtx
///
/// @brief Add std::hash support for glm types
///
/// <glm/gtx/hash.inl> need to be included to use these functionalities.

namespace glm {
namespace detail
{
	GLM_INLINE void hash_combine(size_t &seed, size_t hash)
	{
		hash += 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= hash;
	}
}}

namespace std
{
	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tvec1<T, P>>::operator()(glm::tvec1<T, P> const & v) const
	{
		hash<T> hasher;
		return hasher(v.x);
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tvec2<T, P>>::operator()(glm::tvec2<T, P> const & v) const
	{
		size_t seed = 0;
		hash<T> hasher;
		glm::detail::hash_combine(seed, hasher(v.x));
		glm::detail::hash_combine(seed, hasher(v.y));
		return seed;
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tvec3<T, P>>::operator()(glm::tvec3<T, P> const & v) const
	{
		size_t seed = 0;
		hash<T> hasher;
		glm::detail::hash_combine(seed, hasher(v.x));
		glm::detail::hash_combine(seed, hasher(v.y));
		glm::detail::hash_combine(seed, hasher(v.z));
		return seed;
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tvec4<T, P>>::operator()(glm::tvec4<T, P> const & v) const
	{
		size_t seed = 0;
		hash<T> hasher;
		glm::detail::hash_combine(seed, hasher(v.x));
		glm::detail::hash_combine(seed, hasher(v.y));
		glm::detail::hash_combine(seed, hasher(v.z));
		glm::detail::hash_combine(seed, hasher(v.w));
		return seed;
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tquat<T, P>>::operator()(glm::tquat<T,P> const & q) const
	{
		size_t seed = 0;
		hash<T> hasher;
		glm::detail::hash_combine(seed, hasher(q.x));
		glm::detail::hash_combine(seed, hasher(q.y));
		glm::detail::hash_combine(seed, hasher(q.z));
		glm::detail::hash_combine(seed, hasher(q.w));
		return seed;
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tdualquat<T, P>>::operator()(glm::tdualquat<T, P> const & q) const
	{
		size_t seed = 0;
		hash<glm::tquat<T, P>> hasher;
		glm::detail::hash_combine(seed, hasher(q.real));
		glm::detail::hash_combine(seed, hasher(q.dual));
		return seed;
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tmat2x2<T, P>>::operator()(glm::tmat2x2<T, P> const & m) const
	{
		size_t seed = 0;
		hash<glm::tvec2<T, P>> hasher;
		glm::detail::hash_combine(seed, hasher(m[0]));
		glm::detail::hash_combine(seed, hasher(m[1]));
		return seed;
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tmat2x3<T, P>>::operator()(glm::tmat2x3<T, P> const & m) const
	{
		size_t seed = 0;
		hash<glm::tvec3<T, P>> hasher;
		glm::detail::hash_combine(seed, hasher(m[0]));
		glm::detail::hash_combine(seed, hasher(m[1]));
		return seed;
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tmat2x4<T, P>>::operator()(glm::tmat2x4<T, P> const & m) const
	{
		size_t seed = 0;
		hash<glm::tvec4<T, P>> hasher;
		glm::detail::hash_combine(seed, hasher(m[0]));
		glm::detail::hash_combine(seed, hasher(m[1]));
		return seed;
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tmat3x2<T, P>>::operator()(glm::tmat3x2<T, P> const & m) const
	{
		size_t seed = 0;
		hash<glm::tvec2<T, P>> hasher;
		glm::detail::hash_combine(seed, hasher(m[0]));
		glm::detail::hash_combine(seed, hasher(m[1]));
		glm::detail::hash_combine(seed, hasher(m[2]));
		return seed;
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tmat3x3<T, P>>::operator()(glm::tmat3x3<T, P> const & m) const
	{
		size_t seed = 0;
		hash<glm::tvec3<T, P>> hasher;
		glm::detail::hash_combine(seed, hasher(m[0]));
		glm::detail::hash_combine(seed, hasher(m[1]));
		glm::detail::hash_combine(seed, hasher(m[2]));
		return seed;
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tmat3x4<T, P>>::operator()(glm::tmat3x4<T, P> const & m) const
	{
		size_t seed = 0;
		hash<glm::tvec4<T, P>> hasher;
		glm::detail::hash_combine(seed, hasher(m[0]));
		glm::detail::hash_combine(seed, hasher(m[1]));
		glm::detail::hash_combine(seed, hasher(m[2]));
		return seed;
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tmat4x2<T,P>>::operator()(glm::tmat4x2<T,P> const & m) const
	{
		size_t seed = 0;
		hash<glm::tvec2<T, P>> hasher;
		glm::detail::hash_combine(seed, hasher(m[0]));
		glm::detail::hash_combine(seed, hasher(m[1]));
		glm::detail::hash_combine(seed, hasher(m[2]));
		glm::detail::hash_combine(seed, hasher(m[3]));
		return seed;
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tmat4x3<T,P>>::operator()(glm::tmat4x3<T,P> const & m) const
	{
		size_t seed = 0;
		hash<glm::tvec3<T, P>> hasher;
		glm::detail::hash_combine(seed, hasher(m[0]));
		glm::detail::hash_combine(seed, hasher(m[1]));
		glm::detail::hash_combine(seed, hasher(m[2]));
		glm::detail::hash_combine(seed, hasher(m[3]));
		return seed;
	}

	template <typename T, glm::precision P>
	GLM_FUNC_QUALIFIER size_t hash<glm::tmat4x4<T,P>>::operator()(glm::tmat4x4<T, P> const & m) const
	{
		size_t seed = 0;
		hash<glm::tvec4<T, P>> hasher;
		glm::detail::hash_combine(seed, hasher(m[0]));
		glm::detail::hash_combine(seed, hasher(m[1]));
		glm::detail::hash_combine(seed, hasher(m[2]));
		glm::detail::hash_combine(seed, hasher(m[3]));
		return seed;
	}
}
