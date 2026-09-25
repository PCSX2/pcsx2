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

layout(push_constant) uniform const_buffer
{
	uvec4 const0;
	uvec4 const1;
	ivec2 srcOffset;
};

layout(set=0, binding=0) uniform texture2D imgSrc;

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

layout(location = 0) out vec4 o_col0;

void main()
{
	// Pixel coordinate for the current fragment
	AU2 gxy = AU2(gl_FragCoord.xy);

#if CAS_SHARPEN_ONLY
	const bool sharpenOnly = true;
#else
	const bool sharpenOnly = false;
#endif

	AF3 c = AF3(0.0f);
	CasFilter(c.r, c.g, c.b, gxy, const0, const1, sharpenOnly);

	o_col0 = AF4(c, 1.0f);
}