PS_STATIC void fog(IN_OUT_PARAM(float4, C), float f)
{
	if (PS_FOG != FALSE)
		C.rgb = trunc(lerp(ps_cb.fog_color, C.rgb, (f * 255.0f) / 256.0f));
}
