// Based on CAS_Shader.glsl
// Copyright(c) 2019 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

uniform uvec4 const0;
uniform uvec4 const1;
uniform ivec2 srcOffset;

layout(binding=0) uniform sampler2D imgSrc;
layout(binding=0, rgba8) uniform writeonly image2D imgDst;

#define A_GPU 1
#define A_GLSL 1

#include "ffx_a.h"

AF3 CasLoad(ASU2 p)
{
	return texelFetch(imgSrc, srcOffset + ivec2(p), 0).rgb;
}

// Lets you transform input from the load into a linear color space between 0 and 1. See ffx_cas.h
// In this case, our input is already linear and between 0 and 1
void CasInput(inout AF1 r, inout AF1 g, inout AF1 b) {}

#include "ffx_cas.h"

layout(local_size_x=64) in;
void main()
{
	// Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
	AU2 gxy = ARmp8x8(gl_LocalInvocationID.x)+AU2(gl_WorkGroupID.x<<4u,gl_WorkGroupID.y<<4u);

	// Filter.
	AF4 c = vec4(0.0f);
	CasFilter(c.r, c.g, c.b, gxy, const0, const1, CAS_SHARPEN_ONLY);
	imageStore(imgDst, ASU2(gxy), c);
	gxy.x += 8u;

	CasFilter(c.r, c.g, c.b, gxy, const0, const1, CAS_SHARPEN_ONLY);
	imageStore(imgDst, ASU2(gxy), c);
	gxy.y += 8u;

	CasFilter(c.r, c.g, c.b, gxy, const0, const1, CAS_SHARPEN_ONLY);
	imageStore(imgDst, ASU2(gxy), c);
	gxy.x -= 8u;

	CasFilter(c.r, c.g, c.b, gxy, const0, const1, CAS_SHARPEN_ONLY);
	imageStore(imgDst, ASU2(gxy), c);
}
