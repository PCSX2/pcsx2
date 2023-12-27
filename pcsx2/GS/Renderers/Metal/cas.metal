// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#define A_GPU 1
#define A_MSL 1
#define A_HALF 1

#include "../../../../bin/resources/shaders/common/ffx_a.h"

struct CASTextureF
{
	const thread texture2d<float, access::read>& tex;
	uint2 offset;
};

struct CASTextureH
{
	const thread texture2d<half, access::read>& tex;
	ushort2 offset;
};

#define CAS_TEXTURE CASTextureF
#define CAS_TEXTUREH CASTextureH

A_STATIC AF3 CasLoad(CASTextureF tex, ASU2 coord)
{
	return tex.tex.read(AU2(coord) + tex.offset).rgb;
}
#define CasInput(r,g,b)

A_STATIC AH3 CasLoadH(CASTextureH tex, ASW2 coord)
{
	return tex.tex.read(AW2(coord) + tex.offset).rgb;
}

A_STATIC void CasInputH(inoutAH2 r, inoutAH2 g, inoutAH2 b){}

#include "../../../../bin/resources/shaders/common/ffx_cas.h"

#include "GSMTLShaderCommon.h"

constant bool CAS_SHARPEN_ONLY [[function_constant(GSMTLConstantIndex_CAS_SHARPEN_ONLY)]];

kernel void CASFloat(
	uint2 localID [[thread_position_in_threadgroup]],
	uint2 workgroupID [[threadgroup_position_in_grid]],
	texture2d<float, access::read> input [[texture(0)]],
	texture2d<float, access::write> output [[texture(1)]],
	constant GSMTLCASPSUniform& cb [[buffer(GSMTLBufferIndexUniforms)]])
{
	// Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
	AU2 gxy = ARmp8x8(localID.x) + (workgroupID << 4);
	const AU4 const0 = cb.const0;
	const AU4 const1 = cb.const1;
	const CASTextureF tex{input, AU2(cb.srcOffset)};

	// Filter.
	float r, g, b;

	CasFilter(tex, r, g, b, gxy, const0, const1, CAS_SHARPEN_ONLY);
	output.write(float4(r, g, b, 1), gxy);
	gxy.x += 8;

	CasFilter(tex, r, g, b, gxy, const0, const1, CAS_SHARPEN_ONLY);
	output.write(float4(r, g, b, 1), gxy);
	gxy.y += 8;

	CasFilter(tex, r, g, b, gxy, const0, const1, CAS_SHARPEN_ONLY);
	output.write(float4(r, g, b, 1), gxy);
	gxy.x -= 8;

	CasFilter(tex, r, g, b, gxy, const0, const1, CAS_SHARPEN_ONLY);
	output.write(float4(r, g, b, 1), gxy);
}

kernel void CASHalf(
	uint2 localID [[thread_position_in_threadgroup]],
	uint2 workgroupID [[threadgroup_position_in_grid]],
	texture2d<half, access::read> input [[texture(0)]],
	texture2d<half, access::write> output [[texture(1)]],
	constant GSMTLCASPSUniform& cb [[buffer(GSMTLBufferIndexUniforms)]])
{
	// Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
	AU2 gxy = ARmp8x8(localID.x) + (workgroupID << 4);
	const AU4 const0 = cb.const0;
	const AU4 const1 = cb.const1;
	const CASTextureH tex{input, AW2(cb.srcOffset)};

	// Filter.
	half2 r, g, b;

	#pragma unroll
	for (int i = 0; i < 2; i++)
	{
		CasFilterH(tex, r, g, b, gxy, const0, const1, CAS_SHARPEN_ONLY);
		output.write(half4(r.x, g.x, b.x, 1), gxy);
		output.write(half4(r.y, g.y, b.y, 1), gxy + AU2(8, 0));
		gxy.y += 8;
	}
}
