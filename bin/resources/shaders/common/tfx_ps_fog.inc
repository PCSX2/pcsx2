// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

PS_STATIC void fog(IN_OUT_PARAM(float4, C), float f)
{
	__IF__ (PS_FOG != 0)
		C.rgb = trunc(lerp(cb.fog_color, C.rgb, (f * 255.0f) / 256.0f));
	__ENDIF__
}
