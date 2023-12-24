// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GSMTLShaderCommon.h"

using namespace metal;

fragment float4 ps_merge0(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	float4 c = res.sample(data.t);
	c.a *= 2.f;
	return c;
}

fragment float4 ps_merge1(ConvertShaderData data [[stage_in]], ConvertPSRes res,
	constant vector_float4& BGColor [[buffer(GSMTLBufferIndexUniforms)]])
{
	float4 c = res.sample(data.t);
	c.a = BGColor.a;
	return c;
}

