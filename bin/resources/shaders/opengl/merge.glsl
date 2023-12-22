// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

//#version 420 // Keep it for editor detection

#ifdef FRAGMENT_SHADER

in vec4 PSin_p;
in vec2 PSin_t;
in vec4 PSin_c;

uniform vec4 BGColor;

layout(binding = 0) uniform sampler2D TextureSampler;

layout(location = 0) out vec4 SV_Target0;

void ps_main0()
{
	vec4 c = texture(TextureSampler, PSin_t);
	// Note: clamping will be done by fixed unit
	c.a *= 2.0f;
	SV_Target0 = c;
}

void ps_main1()
{
	vec4 c = texture(TextureSampler, PSin_t);
	c.a = BGColor.a;
	SV_Target0 = c;
}

#endif
