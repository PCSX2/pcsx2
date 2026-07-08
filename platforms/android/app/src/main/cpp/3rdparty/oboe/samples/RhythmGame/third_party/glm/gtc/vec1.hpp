/// @ref gtc_vec1
/// @file glm/gtc/vec1.hpp
///
/// @see core (dependence)
///
/// @defgroup gtc_vec1 GLM_GTC_vec1
/// @ingroup gtc
/// 
/// @brief Add vec1, ivec1, uvec1 and bvec1 types.
/// <glm/gtc/vec1.hpp> need to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../detail/type_vec1.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_vec1 extension included")
#endif

namespace glm
{
	/// 1 component vector of high precision floating-point numbers. 
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef highp_vec1_t			highp_vec1;

	/// 1 component vector of medium precision floating-point numbers. 
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef mediump_vec1_t			mediump_vec1;

	/// 1 component vector of low precision floating-point numbers. 
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef lowp_vec1_t				lowp_vec1;

	/// 1 component vector of high precision floating-point numbers. 
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef highp_dvec1_t			highp_dvec1;

	/// 1 component vector of medium precision floating-point numbers. 
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef mediump_dvec1_t			mediump_dvec1;

	/// 1 component vector of low precision floating-point numbers. 
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef lowp_dvec1_t			lowp_dvec1;

	/// 1 component vector of high precision signed integer numbers. 
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef highp_ivec1_t			highp_ivec1;

	/// 1 component vector of medium precision signed integer numbers. 
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef mediump_ivec1_t			mediump_ivec1;

	/// 1 component vector of low precision signed integer numbers. 
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef lowp_ivec1_t			lowp_ivec1;

	/// 1 component vector of high precision unsigned integer numbers. 
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef highp_uvec1_t			highp_uvec1;

	/// 1 component vector of medium precision unsigned integer numbers. 
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef mediump_uvec1_t			mediump_uvec1;

	/// 1 component vector of low precision unsigned integer numbers. 
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef lowp_uvec1_t			lowp_uvec1;

	/// 1 component vector of high precision boolean. 
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef highp_bvec1_t			highp_bvec1;

	/// 1 component vector of medium precision boolean.
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef mediump_bvec1_t			mediump_bvec1;

	/// 1 component vector of low precision boolean.
	/// There is no guarantee on the actual precision.
	/// @see gtc_vec1 extension.
	typedef lowp_bvec1_t			lowp_bvec1;

	//////////////////////////
	// vec1 definition

#if(defined(GLM_PRECISION_HIGHP_BOOL))
	typedef highp_bvec1				bvec1;
#elif(defined(GLM_PRECISION_MEDIUMP_BOOL))
	typedef mediump_bvec1			bvec1;
#elif(defined(GLM_PRECISION_LOWP_BOOL))
	typedef lowp_bvec1				bvec1;
#else
	/// 1 component vector of boolean.
	/// @see gtc_vec1 extension.
	typedef highp_bvec1				bvec1;
#endif//GLM_PRECISION

#if(defined(GLM_PRECISION_HIGHP_FLOAT))
	typedef highp_vec1				vec1;
#elif(defined(GLM_PRECISION_MEDIUMP_FLOAT))
	typedef mediump_vec1			vec1;
#elif(defined(GLM_PRECISION_LOWP_FLOAT))
	typedef lowp_vec1				vec1;
#else
	/// 1 component vector of floating-point numbers.
	/// @see gtc_vec1 extension.
	typedef highp_vec1				vec1;
#endif//GLM_PRECISION

#if(defined(GLM_PRECISION_HIGHP_DOUBLE))
	typedef highp_dvec1				dvec1;
#elif(defined(GLM_PRECISION_MEDIUMP_DOUBLE))
	typedef mediump_dvec1			dvec1;
#elif(defined(GLM_PRECISION_LOWP_DOUBLE))
	typedef lowp_dvec1				dvec1;
#else
	/// 1 component vector of floating-point numbers.
	/// @see gtc_vec1 extension.
	typedef highp_dvec1				dvec1;
#endif//GLM_PRECISION

#if(defined(GLM_PRECISION_HIGHP_INT))
	typedef highp_ivec1			ivec1;
#elif(defined(GLM_PRECISION_MEDIUMP_INT))
	typedef mediump_ivec1		ivec1;
#elif(defined(GLM_PRECISION_LOWP_INT))
	typedef lowp_ivec1			ivec1;
#else
	/// 1 component vector of signed integer numbers. 
	/// @see gtc_vec1 extension.
	typedef highp_ivec1			ivec1;
#endif//GLM_PRECISION

#if(defined(GLM_PRECISION_HIGHP_UINT))
	typedef highp_uvec1			uvec1;
#elif(defined(GLM_PRECISION_MEDIUMP_UINT))
	typedef mediump_uvec1		uvec1;
#elif(defined(GLM_PRECISION_LOWP_UINT))
	typedef lowp_uvec1			uvec1;
#else
	/// 1 component vector of unsigned integer numbers. 
	/// @see gtc_vec1 extension.
	typedef highp_uvec1			uvec1;
#endif//GLM_PRECISION

}// namespace glm

#include "vec1.inl"
