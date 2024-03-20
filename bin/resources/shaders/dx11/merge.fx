// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

Texture2D Texture;
SamplerState Sampler;

cbuffer cb0 : register(b0)
{
	float4 BGColor;
	int EMODA;
	int EMODC;
	uint cb0_pad1;
	float cb0_pad2;
	uint cb0_pad3[4];
};

struct PS_INPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
};

float4 ps_main0(PS_INPUT input) : SV_Target0
{
	float4 c = Texture.Sample(Sampler, input.t);
	c.a *= 2.0f;
	return c;
}

float4 ps_main1(PS_INPUT input) : SV_Target0
{
	float4 c = Texture.Sample(Sampler, input.t);
	c.a = BGColor.a;
	return c;
}
