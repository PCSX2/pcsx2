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

uniform vec4 ZrH;

layout(binding = 0) uniform sampler2D TextureSampler;

layout(location = 0) out vec4 SV_Target0;


// Weave shader
void ps_main0()
{
	int idx   = int(ZrH.x);          // buffer index passed from CPU
	int field = idx & 1;             // current field
	int vpos  = int(gl_FragCoord.y); // vertical position of destination texture

	if ((vpos & 1) == field)
		SV_Target0 = textureLod(TextureSampler, PSin_t, 0);
	else
		discard;
}


// Bob shader
void ps_main1()
{
	SV_Target0 = textureLod(TextureSampler, PSin_t, 0);
}


// Blend shader
void ps_main2()
{
	vec2 vstep = vec2(0.0f, ZrH.y);
	vec4 c0 = textureLod(TextureSampler, PSin_t - vstep, 0);
	vec4 c1 = textureLod(TextureSampler, PSin_t, 0);
	vec4 c2 = textureLod(TextureSampler, PSin_t + vstep, 0);

	SV_Target0 = (c0 + c1 * 2.0f + c2) / 4.0f;
}


// MAD shader - buffering
void ps_main3()
{
	// We take half the lines from the current frame and stores them in the MAD frame buffer.
	// the MAD frame buffer is split in 2 consecutive banks of 2 fields each, the fields in each bank
	// are interleaved (top field at even lines and bottom field at odd lines).
	// When the source texture has an odd vres, the first line of bank 1 would be an odd index
	// causing the wrong lines to be discarded, so a vertical offset (lofs) is added to the vertical
	// position of the destination texture to force the proper field alignment

	int  idx    = int(ZrH.x);                                // buffer index passed from CPU
	int  bank   = idx >> 1;                                  // current bank
	int  field  = idx & 1;                                   // current field
	int  vres   = int(ZrH.z) >> 1;                           // vertical resolution of source texture
	int  lofs   = ((((vres + 1) >> 1) << 1) - vres) & bank;  // line alignment offset for bank 1
	int  vpos   = int(gl_FragCoord.y) + lofs;                // vertical position of destination texture

	// if the index of current destination line belongs to the current fiels we update it, otherwise
	// we leave the old line in the destination buffer
	if ((vpos & 1) == field)
		SV_Target0 = textureLod(TextureSampler, PSin_t, 0);
	else
		discard;
}


// MAD shader - reconstruction
void ps_main4()
{
	// we use the contents of the MAD frame buffer to reconstruct the missing lines from the current field.

	int   idx          = int(ZrH.x);                         // buffer index passed from CPU
	int   field        = idx & 1;                            // current field
	int   vpos         = int(gl_FragCoord.y);                // vertical position of destination texture
	float sensitivity  = ZrH.w;                              // passed from CPU, higher values mean more likely to use weave
	vec3  motion_thr   = vec3(1.0, 1.0, 1.0) * sensitivity;  //
	vec2  bofs         = vec2(0.0f, 0.5f);                   // position of the bank 1 relative to source texture size
	vec2  vscale       = vec2(1.0f, 0.5f);                   // scaling factor from source to destination texture
	vec2  lofs         = vec2(0.0f, ZrH.y) * vscale;         // distance between two adjacent lines relative to source texture size
	vec2  iptr         = PSin_t * vscale;                    // pointer to the current pixel in the source texture

	vec2 p_t0; // pointer to current pixel (missing or not) from most recent frame
	vec2 p_t1; // pointer to current pixel (missing or not) from one frame back
	vec2 p_t2; // pointer to current pixel (missing or not) from two frames back
	vec2 p_t3; // pointer to current pixel (missing or not) from three frames back

	switch (idx)
	{
		case 1:
			p_t0 = iptr;
			p_t1 = iptr;
			p_t2 = iptr + bofs;
			p_t3 = iptr + bofs;
			break;
		case 2:
			p_t0 = iptr + bofs;
			p_t1 = iptr;
			p_t2 = iptr;
			p_t3 = iptr + bofs;
			break;
		case 3:
			p_t0 = iptr + bofs;
			p_t1 = iptr + bofs;
			p_t2 = iptr;
			p_t3 = iptr;
			break;
		default:
			p_t0 = iptr;
			p_t1 = iptr + bofs;
			p_t2 = iptr + bofs;
			p_t3 = iptr;
			break;
	}


	// calculating motion, only relevant for missing lines where the "center line" is pointed
	// by p_t1

	vec4 hn = textureLod(TextureSampler, p_t0 - lofs, 0); // new high pixel
	vec4 cn = textureLod(TextureSampler, p_t1, 0);        // new center pixel
	vec4 ln = textureLod(TextureSampler, p_t0 + lofs, 0); // new low pixel

	vec4 ho = textureLod(TextureSampler, p_t2 - lofs, 0); // old high pixel
	vec4 co = textureLod(TextureSampler, p_t3, 0);        // old center pixel
	vec4 lo = textureLod(TextureSampler, p_t2 + lofs, 0); // old low pixel

	vec3 mh = hn.rgb - ho.rgb; // high pixel motion
	vec3 mc = cn.rgb - co.rgb; // center pixel motion
	vec3 ml = ln.rgb - lo.rgb; // low pixel motion

	mh = max(mh, -mh) - motion_thr;
	mc = max(mc, -mc) - motion_thr;
	ml = max(ml, -ml) - motion_thr;

	#if 1 // use this code to evaluate each color motion separately
		float mh_max = max(max(mh.x, mh.y), mh.z);
		float mc_max = max(max(mc.x, mc.y), mc.z);
		float ml_max = max(max(ml.x, ml.y), ml.z);
	#else // use this code to evaluate average color motion
		float mh_max = mh.x + mh.y + mh.z;
		float mc_max = mc.x + mc.y + mc.z;
		float ml_max = ml.x + ml.y + ml.z;
	#endif


	// selecting deinterlacing output

	if ((vpos & 1) == field)
	{
		// output coordinate present on current field
		SV_Target0 = textureLod(TextureSampler, p_t0, 0);
	}
	else if ((iptr.y > 0.5f - lofs.y) || (iptr.y < 0.0 + lofs.y))
	{
		// top and bottom lines are always weaved
		SV_Target0 = cn;
	}
	else
	{
		// missing line needs to be reconstructed
		if(((mh_max > 0.0f) || (ml_max > 0.0f)) || (mc_max > 0.0f))
			// high motion -> interpolate pixels above and below
			SV_Target0 = (hn + ln) / 2.0f;
		else
			// low motion -> weave
			SV_Target0 = cn;
	}
}

#endif
