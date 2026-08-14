STATIC uint fetch_raw_depth(IN_PARAM(PSMainState, state))
{
	return uint(PS_READ_TEX_DEPTH(state, USHORT2(state.psin.p.xy + state.cb.channel_shuffle_offset), 0) * EXP2_POS_32);
}

STATIC FLOAT4 fetch_raw_color(IN_PARAM(PSMainState, state))
{
	if (PS_TEX_IS_FB != FALSE)
		return state.current_color;
	else
		return PS_READ_TEX(state, USHORT2(state.psin.p.xy + state.cb.channel_shuffle_offset), 0);
}

STATIC FLOAT4 fetch_c(IN_PARAM(PSMainState, state), USHORT2 uv)
{
	if (PS_TEX_IS_FB != FALSE)
		return state.current_color;
	else if (PS_TEX_IS_DEPTH)
		return FLOAT4(PS_READ_TEX_DEPTH(state, uv, 0), 0.0f, 0.0f, 0.0f);
	else
		return PS_READ_TEX(state, uv, 0);
}

STATIC FLOAT4 fetch_red(IN_PARAM(PSMainState, state))
{
	float rt = PS_TEX_IS_DEPTH ? float(fetch_raw_depth(state) & 0xFFu) / 255.f : fetch_raw_color(state).r;
	return sample_p_norm(state, rt) * 255.f;
}

STATIC FLOAT4 fetch_green(IN_PARAM(PSMainState, state))
{
	float rt = PS_TEX_IS_DEPTH ? float((fetch_raw_depth(state) >> 8) & 0xFFu) / 255.f : fetch_raw_color(state).g;
	return sample_p_norm(state, rt) * 255.f;
}

STATIC FLOAT4 fetch_blue(IN_PARAM(PSMainState, state))
{
	float rt = PS_TEX_IS_DEPTH ? float((fetch_raw_depth(state) >> 16) & 0xFFu) / 255.f : fetch_raw_color(state).b;
	return sample_p_norm(state, rt) * 255.f;
}

STATIC FLOAT4 fetch_alpha(IN_PARAM(PSMainState, state))
{
	return sample_p_norm(state, fetch_raw_color(state).a) * 255.f;
}

STATIC FLOAT4 fetch_rgb(IN_PARAM(PSMainState, state))
{
	FLOAT4 rt = fetch_raw_color(state);
	return FLOAT4(sample_p_norm(state, rt.r).r, sample_p_norm(state, rt.g).g, sample_p_norm(state, rt.b).b, 1) * 255.f;
}

STATIC FLOAT4 fetch_gXbY(IN_PARAM(PSMainState, state))
{
	if (PS_TEX_IS_DEPTH)
	{
		uint depth = fetch_raw_depth(state);
		uint bg = (depth >> (8 + state.cb.channel_shuffle_green_shift)) & 0xFFu;
		return float4_bcast(bg);
	}
	else
	{
		UINT4 rt = UINT4(fetch_raw_color(state) * 255.5f);
		uint green = (rt.g >> state.cb.channel_shuffle_green_shift) & state.cb.channel_shuffle_green_mask;
		uint blue  = (rt.b >> state.cb.channel_shuffle_blue_shift)  & state.cb.channel_shuffle_blue_mask;
		return float4_bcast(green | blue);
	}
}
