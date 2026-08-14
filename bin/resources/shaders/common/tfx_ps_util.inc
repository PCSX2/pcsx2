PS_STATIC void discard_all()
{
	if (PS_ROV_COLOR != FALSE || PS_ROV_DEPTH != PS_ROV_DEPTH_NONE)
		ps_color_discarded = ps_depth_discarded = true;
	else
		discard;
}

PS_STATIC void discard_color(IN_OUT_PARAM(float4, color))
{
	if (PS_ROV_COLOR != FALSE)
		ps_color_discarded = true;
	else
		color = ps_current_color;
}

PS_STATIC void discard_depth(IN_OUT_PARAM(float, depth))
{
	if (PS_ROV_DEPTH == PS_ROV_DEPTH_READ_WRITE)
		ps_depth_discarded = true;
	else
		depth = ps_current_depth;
}

PS_STATIC float4 sample_tex_lod(float2 uv, float lod)
{
	if (PS_TEX_IS_DEPTH)
		return float4(PS_SAMPLE_TEX_DEPTH_LOD(uv, lod), 0.0f, 0.0f, 0.0f);
	else
		return PS_SAMPLE_TEX_LOD(uv, lod);
}

PS_STATIC float4 sample_tex(float2 uv)
{
	if (PS_TEX_IS_DEPTH)
		return float4(PS_SAMPLE_TEX_DEPTH(uv), 0.0f, 0.0f, 0.0);
	else
		return PS_SAMPLE_TEX(uv);
}

PS_STATIC float4 read_tex(uint2 pos, uint lod)
{
	if (PS_TEX_IS_DEPTH)
		return float4(PS_READ_TEX_DEPTH(pos, lod), 0.0f, 0.0f, 0.0f);
	else
		return PS_READ_TEX(pos, lod);
}

PS_STATIC float4 read_tex(uint2 pos)
{
	return read_tex(pos, 0);
}

PS_STATIC uint2 get_tex_dims()
{
	uint2 dims = uint2(0, 0);
	if (PS_TEX_IS_DEPTH)
		PS_GET_TEX_DEPTH_DIMS(dims);
	else
		PS_GET_TEX_DIMS(dims);
	return dims;
}
