// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifndef PS_RTA_CORRECTION
#define PS_RTA_CORRECTION 0
#endif

export float rta_correction_factor()
{
#if PS_RTA_CORRECTION
	return 128.0f;
#else
	return 255.0f;
#endif
}

export float rta_correction_lim()
{
#if PS_RTA_CORRECTION
	return (254.5f / 255.0f);
#else
	return (127.5f / 255.0f);
#endif
}

export void rta_correction_blend_hw_3(inout float4 Color)
{
#if PS_RTA_CORRECTION == 0
	// Needed for Cs*Ad, Cs*Ad + Cd, Cd - Cs*Ad
	// Multiply Color.rgb by (255/128) to compensate for wrong Ad/255 value when rgb are below 128.
	// When any color channel is higher than 128 then adjust the compensation automatically
	// to give us more accurate colors, otherwise they will be wrong.
	// The higher the value (>128) the lower the compensation will be.
	float max_color = max(max(Color.r, Color.g), Color.b);
	float color_compensate = 255.0f / max(128.0f, max_color);
	Color.rgb *= (float3)color_compensate;
#endif
}
