STATIC FLOAT4 ps_color(IN_PARAM(PSMainState, state))
{
	FLOAT2 st, st_int;
	if (PS_FST == FALSE)
	{
		st = state.psin.t.xy / state.psin.t.w;
		st_int = state.psin.ti.zw / state.psin.t.w;
	}
	else
	{
		// Note: xy are normalized coordinates
		st = state.psin.ti.xy;
		st_int = state.psin.ti.zw;
	}

	FLOAT4 T;
	if (PS_CHANNEL == 1)
		T = fetch_red(state);
	else if (PS_CHANNEL == 2)
		T = fetch_green(state);
	else if (PS_CHANNEL == 3)
		T = fetch_blue(state);
	else if (PS_CHANNEL == 4)
		T = fetch_alpha(state);
	else if (PS_CHANNEL == 5)
		T = fetch_rgb(state);
	else if (PS_CHANNEL == 6)
		T = fetch_gXbY(state);
	else if (PS_DEPTH_FMT != 0)
		T = sample_depth(state, st_int);
	else
		T = sample_color(state, st);

	if (PS_SHUFFLE != FALSE && PS_SHUFFLE_SAME == FALSE && PS_READ16_SRC == FALSE &&
		!(PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE))
	{
		UINT4 denorm_c_before = UINT4(T);
		if ((PS_PROCESS_BA & SHUFFLE_READ) != FALSE)
		{
			T.r = float((denorm_c_before.b << 3) & 0xF8u);
			T.g = float(((denorm_c_before.b >> 2) & 0x38u) | ((denorm_c_before.a << 6) & 0xC0u));
			T.b = float((denorm_c_before.a << 1) & 0xF8u);
			T.a = float(denorm_c_before.a & 0x80u);
		}
		else
		{
			T.r = float((denorm_c_before.r << 3) & 0xF8u);
			T.g = float(((denorm_c_before.r >> 2) & 0x38u) | ((denorm_c_before.g << 6) & 0xC0u));
			T.b = float((denorm_c_before.g << 1) & 0xF8u);
			T.a = float(denorm_c_before.g & 0x80u);
		}
		
		T.a = (T.a >= 127.5 ? state.cb.ta.y : PS_AEM == FALSE || any_nonzero(INT3(T.rgb) & int(0xF8)) ? state.cb.ta.x : 0.f) * 255.f;
	}

	FLOAT4 C = tfx(state, T, PS_IIP != FALSE ? state.psin.c : state.psin.fc);

	fog(state, C, state.psin.t.z);

	return C;
}
