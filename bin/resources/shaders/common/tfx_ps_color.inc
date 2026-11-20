// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

PS_STATIC float4 ps_color()
{
	float2 st, st_int;
	__IF__ (PS_FST == 0)
	{
		st = ps_in.t.xy / ps_in.t.w;
		st_int = ps_in.ti.zw / ps_in.t.w;
	}
	__ELSE__
	{
		// Note: xy are normalized coordinates
		st = ps_in.ti.xy;
		st_int = ps_in.ti.zw;
	}
	__ENDIF__

	float4 T;
	__IF__ (PS_CHANNEL == 1)
		T = fetch_red();
	__ELIF__ (PS_CHANNEL == 2)
		T = fetch_green();
	__ELIF__ (PS_CHANNEL == 3)
		T = fetch_blue();
	__ELIF__ (PS_CHANNEL == 4)
		T = fetch_alpha();
	__ELIF__ (PS_CHANNEL == 5)
		T = fetch_rgb();
	__ELIF__ (PS_CHANNEL == 6)
		T = fetch_gXbY();
	__ELIF__ (PS_DEPTH_FMT != 0)
		T = sample_depth(st_int);
	__ELSE__
		T = sample_color(st);
	__ENDIF__

	__IF__ (PS_SHUFFLE != 0 && PS_SHUFFLE_SAME == 0 && PS_READ16_SRC == 0 && !(PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE))
	{
		uint4 denorm_c_before = uint4(T);
		__IF__ ((PS_PROCESS_BA & SHUFFLE_READ) != 0)
		{
			T.r = float((denorm_c_before.b << 3) & 0xF8u);
			T.g = float(((denorm_c_before.b >> 2) & 0x38u) | ((denorm_c_before.a << 6) & 0xC0u));
			T.b = float((denorm_c_before.a << 1) & 0xF8u);
			T.a = float(denorm_c_before.a & 0x80u);
		}
		__ELSE__
		{
			T.r = float((denorm_c_before.r << 3) & 0xF8u);
			T.g = float(((denorm_c_before.r >> 2) & 0x38u) | ((denorm_c_before.g << 6) & 0xC0u));
			T.b = float((denorm_c_before.g << 1) & 0xF8u);
			T.a = float(denorm_c_before.g & 0x80u);
		}
		__ENDIF__
		
		T.a = (T.a >= 127.5 ? cb.ta.y : PS_AEM == 0 || any_nonzero(int3(T.rgb) & int(0xF8)) ? cb.ta.x : 0.f) * 255.f;
	}
	__ENDIF__

	float4 C = tfx(T, ps_in.c);

	fog(C, ps_in.t.z);

	return C;
}
