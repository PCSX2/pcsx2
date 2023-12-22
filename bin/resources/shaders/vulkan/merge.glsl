// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#ifdef VERTEX_SHADER

layout(location = 0) in vec4 a_pos;
layout(location = 1) in vec2 a_tex;

layout(location = 0) out vec2 v_tex;

void main()
{
	gl_Position = vec4(a_pos.x, -a_pos.y, a_pos.z, a_pos.w);
	v_tex = a_tex;
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) in vec2 v_tex;
layout(location = 0) out vec4 o_col0;

layout(push_constant) uniform cb10
{
		vec4 BGColor;
};

layout(set = 0, binding = 0) uniform sampler2D samp0;

void ps_main0()
{
		vec4 c = texture(samp0, v_tex);
		// Note: clamping will be done by fixed unit
		c.a *= 2.0f;
		o_col0 = c;
}

void ps_main1()
{
		vec4 c = texture(samp0, v_tex);
		c.a = BGColor.a;
		o_col0 = c;
}

#endif
