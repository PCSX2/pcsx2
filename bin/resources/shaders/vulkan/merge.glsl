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
