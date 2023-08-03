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
