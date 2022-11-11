/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "GSMTLShaderCommon.h"

using namespace metal;

fragment float4 ps_interlace0(ConvertShaderData data [[stage_in]], ConvertPSRes res,
	constant GSMTLInterlacePSUniform& uniform [[buffer(GSMTLBufferIndexUniforms)]])
{
	if ((int(data.p.y) & 1) == 0)
		discard_fragment();
	return res.sample(data.t);
}

fragment float4 ps_interlace1(ConvertShaderData data [[stage_in]], ConvertPSRes res,
	constant GSMTLInterlacePSUniform& uniform [[buffer(GSMTLBufferIndexUniforms)]])
{
	if ((int(data.p.y) & 1) != 0)
		discard_fragment();
	return res.sample(data.t);
}

fragment float4 ps_interlace2(ConvertShaderData data [[stage_in]], ConvertPSRes res,
	constant GSMTLInterlacePSUniform& uniform [[buffer(GSMTLBufferIndexUniforms)]])
{
	float2 vstep = float2(0.0f, uniform.ZrH.y);
	float4 c0 = res.sample(data.t - vstep);
	float4 c1 = res.sample(data.t);
	float4 c2 = res.sample(data.t + vstep);
	return (c0 + c1 * 2.f + c2) / 4.f;
}

fragment float4 ps_interlace3(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	return res.sample(data.t);
}

fragment float4 ps_interlace4(ConvertShaderData data [[stage_in]], ConvertPSRes res,
	constant GSMTLInterlacePSUniform& uniform [[buffer(GSMTLBufferIndexUniforms)]])
{
    const int    vres     = int(round(uniform.ZrH.z));
	const int    idx      = int(round(uniform.ZrH.x));
	const int    bank     = idx >> 1;
	const int    field    = idx & 1;
	const int    vpos     = int(data.p.y) + (((((vres + 1) >> 1) << 1) - vres) & bank);
	const float2 bofs     = float2(0.0f, 0.5f * bank);
	const float2 vscale   = float2(1.0f, 2.0f);
	const float2 optr     = data.t - bofs;
	const float2 iptr     = optr * vscale;

    if ((optr.y >= 0.0f) && (optr.y < 0.5f) && ((vpos & 1) == field))
		return res.sample(iptr);
	else
		discard_fragment();

	return float4(0.0f, 0.0f, 0.0f, 0.0f);
	
}

fragment float4 ps_interlace5(ConvertShaderData data [[stage_in]], ConvertPSRes res,
	constant GSMTLInterlacePSUniform& uniform [[buffer(GSMTLBufferIndexUniforms)]])
{
	const float  sensitivity = uniform.ZrH.w;
	const float3 motion_thr  = float3(1.0, 1.0, 1.0) * sensitivity;
	const float2 vofs        = float2(0.0f, 0.5f);
	const float2 vscale      = float2(1.0f, 0.5f);
	const int    idx         = int(round(uniform.ZrH.x));
	const int    bank        = idx >> 1;
	const int    field       = idx & 1;
	const float2 line_ofs    = float2(0.0f, uniform.ZrH.y);
	const float2 iptr        = data.t * vscale;

	float2 p_new_cf;
	float2 p_old_cf;
	float2 p_new_af;
	float2 p_old_af;

	switch (idx)
	{
		case 0:
			p_new_cf = iptr;
			p_new_af = iptr + vofs;
			p_old_cf = iptr + vofs;
			p_old_af = iptr;
			break;
		case 1:
			p_new_cf = iptr;
			p_new_af = iptr;
			p_old_cf = iptr + vofs;
			p_old_af = iptr + vofs;
			break;
		case 2:
			p_new_cf = iptr + vofs;
			p_new_af = iptr;
			p_old_cf = iptr;
			p_old_af = iptr + vofs;
			break;
		case 3:
			p_new_cf = iptr + vofs;
			p_new_af = iptr + vofs;
			p_old_cf = iptr;
			p_old_af = iptr;
			break;
		default:
			break;
	}

	// calculating motion

	float4 hn = res.sample(p_new_cf - line_ofs); // high
	float4 cn = res.sample(p_new_af);            // center
	float4 ln = res.sample(p_new_cf + line_ofs); // low

	float4 ho = res.sample(p_old_cf - line_ofs); // high
	float4 co = res.sample(p_old_af);            // center
	float4 lo = res.sample(p_old_cf + line_ofs); // low

	float3 mh = hn.rgb - ho.rgb;
	float3 mc = cn.rgb - co.rgb;
	float3 ml = ln.rgb - lo.rgb;

	mh = max(mh, -mh) - motion_thr;
	mc = max(mc, -mc) - motion_thr;
	ml = max(ml, -ml) - motion_thr;


//    float mh_max = max(max(mh.x, mh.y), mh.z);
//    float mc_max = max(max(mc.x, mc.y), mc.z);
//    float ml_max = max(max(ml.x, ml.y), ml.z);

	float mh_max = mh.x + mh.y + mh.z;
	float mc_max = mc.x + mc.y + mc.z;
	float ml_max = ml.x + ml.y + ml.z;

	// selecting deinterlacing output

	if (((int(data.p.y) & 1) == field)) // output coordinate present on current field
	{
		return res.sample(p_new_cf);
	}
	else if ((iptr.y > 0.5f - line_ofs.y) || (iptr.y < 0.0 + line_ofs.y))
	{
		return res.sample(p_new_af);
	}
	else
	{
		if (((mh_max > 0.0f) || (ml_max > 0.0f)) || (mc_max > 0.0f))
		{
			return (hn + ln) / 2.0f;
		}
		else
		{
			return res.sample(p_new_af);
		}
	}

	return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
