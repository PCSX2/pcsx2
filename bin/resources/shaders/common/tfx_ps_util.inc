// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

PS_STATIC void discard_all()
{
	__IF__ (PS_ROV_COLOR != 0 || PS_ROV_DEPTH != PS_ROV_DEPTH_NONE)
		color_discarded = depth_discarded = true;
	__ELSE__
		discard;
	__ENDIF__
}

PS_STATIC void discard_color(IN_OUT_PARAM(float4, color))
{
	__IF__ (PS_ROV_COLOR != 0)
		color_discarded = true;
	__ELSE__
		color = current_color;
	__ENDIF__
}

PS_STATIC void discard_depth(IN_OUT_PARAM(float, depth))
{
	__IF__ (PS_ROV_DEPTH == PS_ROV_DEPTH_READ_WRITE)
		depth_discarded = true;
	__ELSE__
		depth = current_depth;
	__ENDIF__
}
