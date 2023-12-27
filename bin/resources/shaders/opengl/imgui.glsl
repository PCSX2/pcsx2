// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#ifdef VERTEX_SHADER

layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 UV;
layout(location = 2) in vec4 Color;

uniform mat4 ProjMtx;

out vec2 Frag_UV;
out vec4 Frag_Color;

void vs_main()
{
	Frag_UV = UV;
	Frag_Color = Color;
	gl_Position = ProjMtx * vec4(Position.xy, 0.0, 1.0);
}

#endif

#ifdef FRAGMENT_SHADER

layout(binding = 0) uniform sampler2D Texture;

in vec2 Frag_UV;
in vec4 Frag_Color;

layout(location = 0) out vec4 Out_Color;

void ps_main()
{
	Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
}

#endif
