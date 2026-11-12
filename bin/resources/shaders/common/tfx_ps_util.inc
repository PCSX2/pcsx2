PS_STATIC void discard_all()
{
	if (PS_ROV_COLOR != 0 || PS_ROV_DEPTH != PS_ROV_DEPTH_NONE)
		color_discarded = depth_discarded = true;
	else
		discard;
}

PS_STATIC void discard_color(IN_OUT_PARAM(float4, color))
{
	if (PS_ROV_COLOR != 0)
		color_discarded = true;
	else
		color = current_color;
}

PS_STATIC void discard_depth(IN_OUT_PARAM(float, depth))
{
	if (PS_ROV_DEPTH == PS_ROV_DEPTH_READ_WRITE)
		depth_discarded = true;
	else
		depth = current_depth;
}
