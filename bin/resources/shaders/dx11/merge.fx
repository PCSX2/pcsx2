// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

Texture2D Texture;
SamplerState Sampler;

cbuffer cb0 : register(b0)
{
	float4 BGColor;
	int EMODA;
	int EMODC;
	int cb0_pad[2];
};

struct PS_INPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
};

float4 ps_main0(PS_INPUT input) : SV_Target0
{
	float4 c = Texture.Sample(Sampler, input.t);
	// Alpha 0x80 (128) would be interpreted as 1 (neutral) here, but after it's multiplied by 2 and clamped to 0xFF
	c.a = saturate(c.a * 2.0f); //TODO: why? Explain it. Shouldn't this be 255/128 (1.9921875)? Especially if the source texture was RTAd? VK too
	return c;
}

float4 ps_main1(PS_INPUT input) : SV_Target0
{
	float4 c = Texture.Sample(Sampler, input.t);
	c.a = BGColor.a;
	return c;
}
