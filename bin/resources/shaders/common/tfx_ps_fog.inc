PS_STATIC void fog(IN_OUT_PARAM(float4, C), float f)
{
	if (PS_FOG != 0)
		C.rgb = trunc(lerp(cb.fog_color, C.rgb, (f * 255.0f) / 256.0f));
}
