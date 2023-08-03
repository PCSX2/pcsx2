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

Texture2D Texture;
SamplerState Sampler;

cbuffer cb0
{
	float4 params;
};

/*
** Contrast, saturation, brightness
** Code of this function is from TGM's shader pack
** http://irrlicht.sourceforge.net/phpBB2/viewtopic.php?t=21057
*/

// For all settings: 1.0 = 100% 0.5=50% 1.5 = 150% 
float4 ContrastSaturationBrightness(float4 color) // Ported to HLSL
{
	float brt = params.x;
	float con = params.y;
	float sat = params.z;
	
	// Increase or decrease these values to adjust r, g and b color channels separately
	const float AvgLumR = 0.5;
	const float AvgLumG = 0.5;
	const float AvgLumB = 0.5;
	
	const float3 LumCoeff = float3(0.2125, 0.7154, 0.0721);
	
	float3 AvgLumin = float3(AvgLumR, AvgLumG, AvgLumB);
	float3 brtColor = color.rgb * brt;
	float3 intensity = dot(brtColor, LumCoeff);
	float3 satColor = lerp(intensity, brtColor, sat);
	float3 conColor = lerp(AvgLumin, satColor, con);

	color.rgb = conColor;	
	return color;
}

struct PS_INPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
};

float4 ps_main(PS_INPUT input) : SV_Target0
{
	float4 c = Texture.Sample(Sampler, input.t);
	return ContrastSaturationBrightness(c);
}
