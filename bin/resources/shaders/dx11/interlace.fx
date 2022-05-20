#ifdef SHADER_MODEL // make safe to include in resource file to enforce dependency

Texture2D Texture;
SamplerState Sampler;

cbuffer cb0
{
	float2 ZrH;
};

struct PS_INPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
};

float4 ps_main0(PS_INPUT input) : SV_Target0
{
	if ((int(input.p.y) & 1) == 0)
		discard;

	return Texture.Sample(Sampler, input.t);
}

float4 ps_main1(PS_INPUT input) : SV_Target0
{
	if ((int(input.p.y) & 1) != 0)
		discard;

	return Texture.Sample(Sampler, input.t);
}

float4 ps_main2(PS_INPUT input) : SV_Target0
{
	float4 c0 = Texture.Sample(Sampler, input.t - ZrH);
	float4 c1 = Texture.Sample(Sampler, input.t);
	float4 c2 = Texture.Sample(Sampler, input.t + ZrH);

	return (c0 + c1 * 2 + c2) / 4;
}

float4 ps_main3(PS_INPUT input) : SV_Target0
{
	return Texture.Sample(Sampler, input.t);
}
#endif
