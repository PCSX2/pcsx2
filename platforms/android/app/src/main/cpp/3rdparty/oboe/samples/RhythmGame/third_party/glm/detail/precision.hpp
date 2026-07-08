/// @ref core
/// @file glm/detail/precision.hpp

#pragma once

#include "setup.hpp"

namespace glm
{
	enum precision
	{
		packed_highp,
		packed_mediump,
		packed_lowp,

#		if GLM_HAS_ALIGNED_TYPE
			aligned_highp,
			aligned_mediump,
			aligned_lowp,
			aligned = aligned_highp,
#		endif

		highp = packed_highp,
		mediump = packed_mediump,
		lowp = packed_lowp,
		packed = packed_highp,

#		if GLM_HAS_ALIGNED_TYPE && defined(GLM_FORCE_ALIGNED)
			defaultp = aligned_highp
#		else
			defaultp = highp
#		endif
	};
	
namespace detail
{
	template <glm::precision P>
	struct is_aligned
	{
		static const bool value = false;
	};

#	if GLM_HAS_ALIGNED_TYPE
		template<>
		struct is_aligned<glm::aligned_lowp>
		{
			static const bool value = true;
		};

		template<>
		struct is_aligned<glm::aligned_mediump>
		{
			static const bool value = true;
		};

		template<>
		struct is_aligned<glm::aligned_highp>
		{
			static const bool value = true;
		};
#	endif
}//namespace detail
}//namespace glm
