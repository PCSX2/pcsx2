STATIC void discard_all(IN_OUT_PARAM(PSMainState, state))
{
	if (PS_ROV_COLOR != FALSE || PS_ROV_DEPTH != PS_ROV_DEPTH_NONE)
		state.color_discarded = state.depth_discarded = true;
	else
		GPU_DISCARD;
}

STATIC void discard_color(IN_OUT_PARAM(PSMainState, state), IN_OUT_PARAM(FLOAT4, color))
{
	if (PS_ROV_COLOR != FALSE)
		state.color_discarded = true;
	else
		color = state.current_color;
}

STATIC void discard_depth(IN_OUT_PARAM(PSMainState, state), IN_OUT_PARAM(float, depth))
{
	if (PS_ROV_DEPTH == PS_ROV_DEPTH_READ_WRITE)
		state.depth_discarded = true;
	else
		depth = state.current_depth;
}

STATIC FLOAT4 sample_tex_lod(IN_PARAM(PSMainState, state), FLOAT2 uv, float lod)
{
	if (PS_TEX_IS_DEPTH)
		return FLOAT4(PS_SAMPLE_TEX_DEPTH_LOD(state, uv, lod), 0.0f, 0.0f, 0.0f);
	else
		return PS_SAMPLE_TEX_LOD(state, uv, lod);
}

STATIC FLOAT4 sample_tex(IN_PARAM(PSMainState, state), FLOAT2 uv)
{
	if (PS_TEX_IS_DEPTH)
		return FLOAT4(PS_SAMPLE_TEX_DEPTH(state, uv), 0.0f, 0.0f, 0.0);
	else
		return PS_SAMPLE_TEX(state, uv);
}

STATIC FLOAT4 read_tex(IN_PARAM(PSMainState, state), UINT2 pos, uint lod)
{
	if (PS_TEX_IS_DEPTH)
		return FLOAT4(PS_READ_TEX_DEPTH(state, pos, lod), 0.0f, 0.0f, 0.0f);
	else
		return PS_READ_TEX(state, pos, lod);
}

STATIC FLOAT4 read_tex(IN_PARAM(PSMainState, state), UINT2 pos)
{
	return read_tex(state, pos, 0);
}

STATIC UINT2 get_tex_dims(IN_PARAM(PSMainState, state))
{
	UINT2 dims = UINT2(0, 0);
	if (PS_TEX_IS_DEPTH)
		PS_GET_TEX_DEPTH_DIMS(state, dims);
	else
		PS_GET_TEX_DIMS(state, dims);
	return dims;
}
