#ifdef SHADER_MODEL // make safe to include in resource file to enforce dependency

Texture2D Texture;
SamplerState Sampler;

cbuffer cb0
{
	float4 ZrH;
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
	float2 vstep = float2(0.0f, ZrH.y);
	float4 c0 = Texture.Sample(Sampler, input.t - vstep);
	float4 c1 = Texture.Sample(Sampler, input.t);
	float4 c2 = Texture.Sample(Sampler, input.t + vstep);

	return (c0 + c1 * 2 + c2) / 4;
}

float4 ps_main3(PS_INPUT input) : SV_Target0
{
	return Texture.Sample(Sampler, input.t);
}


float4 ps_main4(PS_INPUT input) : SV_Target0
{
    const int    vres     = int(round(ZrH.z));
	const int    idx      = int(round(ZrH.x));
	const int    bank     = idx >> 1;
	const int    field    = idx & 1;
	const int    vpos     = int(input.p.y) + (((((vres + 1) >> 1) << 1) - vres) & bank);
	const float2 bofs     = float2(0.0f, 0.5f * bank);
	const float2 vscale   = float2(1.0f, 2.0f);
	const float2 optr     = input.t - bofs;
	const float2 iptr     = optr * vscale;

    if ((optr.y >= 0.0f) && (optr.y < 0.5f) && ((vpos & 1) == field))
		return Texture.Sample(Sampler, iptr);
	else
		discard;

	return float4(0.0f, 0.0f, 0.0f, 0.0f);
	
}


float4 ps_main5(PS_INPUT input) : SV_Target0
{
	const float  sensitivity = ZrH.w;
	const float3 motion_thr = float3(1.0, 1.0, 1.0) * sensitivity;
	const float2 vofs = float2(0.0f, 0.5f);
	const float2 vscale = float2(1.0f, 0.5f);
	const int    idx = int(round(ZrH.x));
	const int    bank = idx >> 1;
	const int    field = idx & 1;
	const float2 line_ofs = float2(0.0f, ZrH.y);
	const float2 iptr = input.t * vscale;

	float2 p_new_cf;
	float2 p_old_cf;
	float2 p_new_af;
	float2 p_old_af;

	switch (idx)
	{
		case 0:
			p_new_cf = iptr;
			p_new_af = iptr + vofs;
			p_old_cf = iptr + vofs;
			p_old_af = iptr;
			break;
		case 1:
			p_new_cf = iptr;
			p_new_af = iptr;
			p_old_cf = iptr + vofs;
			p_old_af = iptr + vofs;
			break;
		case 2:
			p_new_cf = iptr + vofs;
			p_new_af = iptr;
			p_old_cf = iptr;
			p_old_af = iptr + vofs;
			break;
		case 3:
			p_new_cf = iptr + vofs;
			p_new_af = iptr + vofs;
			p_old_cf = iptr;
			p_old_af = iptr;
			break;
		default:
			break;
	}

	// calculating motion

	float4 hn = Texture.Sample(Sampler, p_new_cf - line_ofs); // high
	float4 cn = Texture.Sample(Sampler, p_new_af); // center
	float4 ln = Texture.Sample(Sampler, p_new_cf + line_ofs); // low

	float4 ho = Texture.Sample(Sampler, p_old_cf - line_ofs); // high
	float4 co = Texture.Sample(Sampler, p_old_af); // center
	float4 lo = Texture.Sample(Sampler, p_old_cf + line_ofs); // low

	float3 mh = hn.rgb - ho.rgb;
	float3 mc = cn.rgb - co.rgb;
	float3 ml = ln.rgb - lo.rgb;

	mh = max(mh, -mh) - motion_thr;
	mc = max(mc, -mc) - motion_thr;
	ml = max(ml, -ml) - motion_thr;


//    float mh_max = max(max(mh.x, mh.y), mh.z);
//    float mc_max = max(max(mc.x, mc.y), mc.z);
//    float ml_max = max(max(ml.x, ml.y), ml.z);

	float mh_max = mh.x + mh.y + mh.z;
	float mc_max = mc.x + mc.y + mc.z;
	float ml_max = ml.x + ml.y + ml.z;

	// selecting deinterlacing output

	if (((int(input.p.y) & 1) == field)) // output coordinate present on current field
	{
		return Texture.Sample(Sampler, p_new_cf);
	}
	else if ((iptr.y > 0.5f - line_ofs.y) || (iptr.y < 0.0 + line_ofs.y))
	{
		return Texture.Sample(Sampler, p_new_af);
	}
	else
	{
		if (((mh_max > 0.0f) || (ml_max > 0.0f)) || (mc_max > 0.0f))
		{
			return (hn + ln) / 2.0f;
		}
		else
		{
			return Texture.Sample(Sampler, p_new_af);
		}
	}

	return float4(0.0f, 0.0f, 0.0f, 0.0f);
}


#endif
