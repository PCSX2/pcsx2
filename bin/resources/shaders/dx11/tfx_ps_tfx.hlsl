// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifndef PS_TFX
#define PS_TFX 0
#define PS_TCC 0
#endif

export float4 tfx(float4 T, float4 C)
{
	float4 C_out;
	float4 FxT = trunc((C * T) / 128.0f);

#if (PS_TFX == 0)
	C_out = FxT;
#elif (PS_TFX == 1)
	C_out = T;
#elif (PS_TFX == 2)
	C_out.rgb = FxT.rgb + C.a;
	C_out.a = T.a + C.a;
#elif (PS_TFX == 3)
	C_out.rgb = FxT.rgb + C.a;
	C_out.a = T.a;
#else
	C_out = C;
#endif

#if (PS_TCC == 0)
	C_out.a = C.a;
#endif

#if (PS_TFX == 0) || (PS_TFX == 2) || (PS_TFX == 3)
	// Clamp only when it is useful
	C_out = min(C_out, 255.0f);
#endif

	return C_out;
}
