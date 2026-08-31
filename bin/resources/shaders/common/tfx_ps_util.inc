STATIC void discard_all()
{
	if (PS_ROV_COLOR != FALSE || PS_ROV_DEPTH != PS_ROV_DEPTH_NONE)
		ps_color_discarded = ps_depth_discarded = true;
	else
		GPU_DISCARD;
}

STATIC void discard_color(IN_OUT_PARAM(FLOAT4, color))
{
	if (PS_ROV_COLOR != FALSE)
		ps_color_discarded = true;
	else
		color = ps_current_color;
}

STATIC void discard_depth(IN_OUT_PARAM(float, depth))
{
	if (PS_ROV_DEPTH == PS_ROV_DEPTH_READ_WRITE)
		ps_depth_discarded = true;
	else
		depth = ps_current_depth;
}

STATIC FLOAT4 sample_tex_lod(FLOAT2 uv, float lod)
{
	if (PS_TEX_IS_DEPTH)
		return FLOAT4(PS_SAMPLE_TEX_DEPTH_LOD(uv, lod), 0.0f, 0.0f, 0.0f);
	else
		return PS_SAMPLE_TEX_LOD(uv, lod);
}

STATIC FLOAT4 sample_tex(FLOAT2 uv)
{
	if (PS_TEX_IS_DEPTH)
		return FLOAT4(PS_SAMPLE_TEX_DEPTH(uv), 0.0f, 0.0f, 0.0);
	else
		return PS_SAMPLE_TEX(uv);
}

STATIC FLOAT4 read_tex(UINT2 pos, uint lod)
{
	if (PS_TEX_IS_DEPTH)
		return FLOAT4(PS_READ_TEX_DEPTH(pos, lod), 0.0f, 0.0f, 0.0f);
	else
		return PS_READ_TEX(pos, lod);
}

STATIC FLOAT4 read_tex(UINT2 pos)
{
	return read_tex(pos, 0);
}

STATIC UINT2 get_tex_dims()
{
	UINT2 dims = UINT2(0, 0);
	if (PS_TEX_IS_DEPTH)
		PS_GET_TEX_DEPTH_DIMS(dims);
	else
		PS_GET_TEX_DIMS(dims);
	return dims;
}
