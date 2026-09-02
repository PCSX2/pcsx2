static float4 ps_color()
{
	float2 st, st_int;
	if (PS_FST == FALSE)
	{
		st = ps_in.t.xy / ps_in.t.w;
		st_int = ps_in.ti.zw / ps_in.t.w;
	}
	else
	{
		// Note: xy are normalized coordinates
		st = ps_in.ti.xy;
		st_int = ps_in.ti.zw;
	}

	float4 T;
	if (PS_CHANNEL == 1)
		T = fetch_red();
	else if (PS_CHANNEL == 2)
		T = fetch_green();
	else if (PS_CHANNEL == 3)
		T = fetch_blue();
	else if (PS_CHANNEL == 4)
		T = fetch_alpha();
	else if (PS_CHANNEL == 5)
		T = fetch_rgb();
	else if (PS_CHANNEL == 6)
		T = fetch_gXbY();
	else if (PS_DEPTH_FMT != 0)
		T = sample_depth(st_int);
	else
		T = sample_color(st);

	if (PS_SHUFFLE != FALSE && PS_SHUFFLE_SAME == FALSE && PS_READ16_SRC == FALSE &&
		!(PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE))
	{
		uint4 denorm_c_before = uint4(T);
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
		
		T.a = (T.a >= 127.5 ? ps_cb.ta.y : PS_AEM == FALSE || any_nonzero(int3(T.rgb) & int(0xF8)) ? ps_cb.ta.x : 0.f) * 255.f;
	}

	float4 C = tfx(T, PS_IIP != FALSE ? ps_in.c : ps_in.fc);

	fog(C, ps_in.t.z);

	return C;
}
