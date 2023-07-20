/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef VERTEX_SHADER

layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 UV;
layout(location = 2) in vec4 Color;

layout(push_constant) uniform PushConstants
{
	vec2 uScale;
	vec2 uTranslate;
};

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

layout(binding = 0) uniform sampler2D Texture;

layout(location = 0) in vec2 Frag_UV;
layout(location = 1) in vec4 Frag_Color;

layout(location = 0) out vec4 Out_Color;

void ps_main()
{
	Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
}

#endif
