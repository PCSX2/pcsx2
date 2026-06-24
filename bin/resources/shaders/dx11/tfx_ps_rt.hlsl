// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#if !PS_ROV_COLOR
Texture2D<float4> RtTexture : register(t2);
#endif

#if PS_ROV_COLOR
RasterizerOrderedTexture2D<unorm float4> RtTextureRov : register(u0);
static float4 rov_rt_value;
#endif

export void RtInit(int2 xy)
{
#if PS_ROV_COLOR
	rov_rt_value = RtTextureRov[xy];
#endif
}

export float4 RtLoad(int2 xy)
{
#if PS_ROV_COLOR
	return rov_rt_value;
#else
	return RtTexture.Load(int3(int2(xy), 0));
#endif
}

export void RtWrite(int2 xy, float4 c)
{
#if PS_ROV_COLOR
	RtTextureRov[xy] = c;
#endif
}
