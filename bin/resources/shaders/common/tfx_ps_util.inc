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
