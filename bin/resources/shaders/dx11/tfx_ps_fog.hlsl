// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifndef PS_FOG
#define PS_FOG 0
#endif

#include "tfx_ps_resources.hlsl"

export float4 fog(float4 c, float f)
{
	if(PS_FOG)
	{
		c.rgb = trunc(lerp(FogColor, c.rgb, (f * 255.0f) / 256.0f));
	}

	return c;
}
