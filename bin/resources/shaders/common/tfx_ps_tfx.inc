// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

PS_STATIC float4 tfx(float4 T, float4 C)
{
	float4 C_out;
	float4 FxT = trunc((C * T) / 128.f);
	__IF__ (PS_TFX == 0)
		C_out = FxT;
	__ELIF__ (PS_TFX == 1)
		C_out = T;
	__ELIF__ (PS_TFX == 2)
		C_out = float4(FxT.rgb, T.a) + C.a;
	__ELIF__ (PS_TFX == 3)
		C_out = float4(FxT.rgb + C.a, T.a);
	__ELSE__
		C_out = C;
	__ENDIF__

	__IF__ (PS_TCC == 0)
		C_out.a = C.a;
	__ENDIF__

	// Clamp only when it is useful
	__IF__ (PS_TFX == 0 || PS_TFX == 2 || PS_TFX == 3)
		C_out = min(C_out, 255.f);
	__ENDIF__

	return C_out;
}
