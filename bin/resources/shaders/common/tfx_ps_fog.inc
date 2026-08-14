STATIC void fog(IN_PARAM(PSMainState, state), IN_OUT_PARAM(FLOAT4, C), float f)
{
	if (PS_FOG != FALSE)
		C.rgb = trunc(MIX(state.cb.fog_color, C.rgb, (f * 255.0f) / 256.0f));
}
