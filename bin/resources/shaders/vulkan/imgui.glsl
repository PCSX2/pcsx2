// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

layout(push_constant) uniform PushConstants
{
	vec2 uScale;
	vec2 uTranslate;
	vec2 uBrigthness;
	vec2 uPad;
};

#ifdef VERTEX_SHADER

layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 UV;
layout(location = 2) in vec4 Color;

layout(location = 0) out vec2 Frag_UV;
layout(location = 1) out vec4 Frag_Color;

void vs_main()
{
	Frag_UV = UV;
	Frag_Color = Color;
	gl_Position = vec4(Position * uScale + uTranslate, 0.0f, 1.0f);
}

#endif

#ifdef FRAGMENT_SHADER

#ifndef PS_HDR
#define PS_HDR 0
#endif

layout(binding = 0) uniform sampler2D Texture;

layout(location = 0) in vec2 Frag_UV;
layout(location = 1) in vec4 Frag_Color;

layout(location = 0) out vec4 Out_Color;

void ps_main()
{
	Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
#if PS_HDR
	Out_Color.rgb = pow(Out_Color.rgb, vec3(2.2));
	//Out_Color.a = pow(Out_Color.a, 1.0 / 2.2); // Approximation to match gamma space blends
#endif
	Out_Color.rgb *= uBrigthness.x; // Always 1 in SDR
}

#endif
