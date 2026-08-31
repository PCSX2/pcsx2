STATIC uint fetch_raw_depth()
{
	return uint(PS_READ_TEX_DEPTH(USHORT2(ps_in.p.xy + ps_cb.channel_shuffle_offset), 0) * EXP2_POS_32);
}

STATIC FLOAT4 fetch_raw_color()
{
	if (PS_TEX_IS_FB != FALSE)
		return ps_current_color;
	else
		return PS_READ_TEX(USHORT2(ps_in.p.xy + ps_cb.channel_shuffle_offset), 0);
}

STATIC FLOAT4 fetch_c(USHORT2 uv)
{
	if (PS_TEX_IS_FB != FALSE)
		return ps_current_color;
	else if (PS_TEX_IS_DEPTH)
		return FLOAT4(PS_READ_TEX_DEPTH(uv, 0), 0.0f, 0.0f, 0.0f);
	else
		return PS_READ_TEX(uv, 0);
}

STATIC FLOAT4 fetch_red()
{
	float rt = PS_TEX_IS_DEPTH ? float(fetch_raw_depth() & 0xFFu) / 255.f : fetch_raw_color().r;
	return sample_p_norm(rt) * 255.f;
}

STATIC FLOAT4 fetch_green()
{
	float rt = PS_TEX_IS_DEPTH ? float((fetch_raw_depth() >> 8) & 0xFFu) / 255.f : fetch_raw_color().g;
	return sample_p_norm(rt) * 255.f;
}

STATIC FLOAT4 fetch_blue()
{
	float rt = PS_TEX_IS_DEPTH ? float((fetch_raw_depth() >> 16) & 0xFFu) / 255.f : fetch_raw_color().b;
	return sample_p_norm(rt) * 255.f;
}

STATIC FLOAT4 fetch_alpha()
{
	return sample_p_norm(fetch_raw_color().a) * 255.f;
}

STATIC FLOAT4 fetch_rgb()
{
	FLOAT4 rt = fetch_raw_color();
	return FLOAT4(sample_p_norm(rt.r).r, sample_p_norm(rt.g).g, sample_p_norm(rt.b).b, 1) * 255.f;
}

STATIC FLOAT4 fetch_gXbY()
{
	if (PS_TEX_IS_DEPTH)
	{
		uint depth = fetch_raw_depth();
		uint bg = (depth >> (8 + ps_cb.channel_shuffle_green_shift)) & 0xFFu;
		return float4_bcast(bg);
	}
	else
	{
		UINT4 rt = UINT4(fetch_raw_color() * 255.5f);
		uint green = (rt.g >> ps_cb.channel_shuffle_green_shift) & ps_cb.channel_shuffle_green_mask;
		uint blue  = (rt.b >> ps_cb.channel_shuffle_blue_shift)  & ps_cb.channel_shuffle_blue_mask;
		return float4_bcast(green | blue);
	}
}
