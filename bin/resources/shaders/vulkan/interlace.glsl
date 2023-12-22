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

layout(push_constant) uniform cb0
{
	vec4 ZrH;
};

layout(set = 0, binding = 0) uniform sampler2D samp0;


// Weave shader
#ifdef ps_main0
void ps_main0()
{
	const int idx   = int(ZrH.x);          // buffer index passed from CPU
	const int field = idx & 1;             // current field
	const int vpos  = int(gl_FragCoord.y); // vertical position of destination texture

	if ((vpos & 1) == field)
		o_col0 = textureLod(samp0, v_tex, 0);
	else
		discard;
}
#endif


// Bob shader
#ifdef ps_main1
void ps_main1()
{
	o_col0 = textureLod(samp0, v_tex, 0);
}
#endif


// Blend shader
#ifdef ps_main2
void ps_main2()
{
	vec2 vstep = vec2(0.0f, ZrH.y);
	vec4 c0 = textureLod(samp0, v_tex - vstep, 0);
	vec4 c1 = textureLod(samp0, v_tex, 0);
	vec4 c2 = textureLod(samp0, v_tex + vstep, 0);

	o_col0 = (c0 + c1 * 2.0f + c2) / 4.0f;
}
#endif


// MAD shader - buffering
#ifdef ps_main3
void ps_main3()
{
	// We take half the lines from the current frame and stores them in the MAD frame buffer.
	// the MAD frame buffer is split in 2 consecutive banks of 2 fields each, the fields in each bank
	// are interleaved (top field at even lines and bottom field at odd lines).
	// When the source texture has an odd vres, the first line of bank 1 would be an odd index
	// causing the wrong lines to be discarded, so a vertical offset (lofs) is added to the vertical
	// position of the destination texture to force the proper field alignment

	const int  idx    = int(ZrH.x);                               // buffer index passed from CPU
	const int  bank   = idx >> 1;                                 // current bank
	const int  field  = idx & 1;                                  // current field
	const int  vres   = int(ZrH.z) >> 1;                          // vertical resolution of source texture
	const int  lofs   = ((((vres + 1) >> 1) << 1) - vres) & bank; // line alignment offset for bank 1
	const int  vpos   = int(gl_FragCoord.y) + lofs;               // vertical position of destination texture

	// if the index of current destination line belongs to the current fiels we update it, otherwise
	// we leave the old line in the destination buffer
	if ((vpos & 1) == field)
		o_col0 = textureLod(samp0, v_tex, 0);
	else
		discard;
}
#endif


// MAD shader - reconstruction
#ifdef ps_main4
void ps_main4()
{
	// we use the contents of the MAD frame buffer to reconstruct the missing lines from the current
	// field.

	const int   idx          = int(ZrH.x);                         // buffer index passed from CPU
	const int   bank         = idx >> 1;                           // current bank
	const int   field        = idx & 1;                            // current field
	const int   vpos         = int(gl_FragCoord.y);                // vertical position of destination texture
	const float sensitivity  = ZrH.w;                              // passed from CPU, higher values mean more likely to use weave
	const vec3  motion_thr   = vec3(1.0, 1.0, 1.0) * sensitivity;  //
	const vec2  bofs         = vec2(0.0f, 0.5f);                   // position of the bank 1 relative to source texture size
	const vec2  vscale       = vec2(1.0f, 0.5f);                   // scaling factor from source to destination texture
	const vec2  lofs         = vec2(0.0f, ZrH.y) * vscale;         // distance between two adjacent lines relative to source texture size
	const vec2  iptr         = v_tex * vscale;                     // pointer to the current pixel in the source texture

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

	// calculating motion, only relevant for missing lines where the "center line" is pointed by p_t1

	vec4 hn = textureLod(samp0, p_t0 - lofs, 0); // new high pixel
	vec4 cn = textureLod(samp0, p_t1, 0);        // new center pixel
	vec4 ln = textureLod(samp0, p_t0 + lofs, 0); // new low pixel

	vec4 ho = textureLod(samp0, p_t2 - lofs, 0); // old high pixel
	vec4 co = textureLod(samp0, p_t3, 0);        // old center pixel
	vec4 lo = textureLod(samp0, p_t2 + lofs, 0); // old low pixel

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

	if ((vpos & 1) == field) // output coordinate present on current field
	{
		// output coordinate present on current field
		o_col0 = textureLod(samp0, p_t0, 0);
	}
	else if ((iptr.y > 0.5f - lofs.y) || (iptr.y < 0.0 + lofs.y))
	{
		// top and bottom lines are always weaved
		o_col0 = cn;
	}
	else
	{
		// missing line needs to be reconstructed
		if(((mh_max > 0.0f) || (ml_max > 0.0f)) || (mc_max > 0.0f))
			// high motion -> interpolate pixels above and below
			o_col0 = (hn + ln) / 2.0f;
		else
			// low motion -> weave
			o_col0 = cn;
	}
}
#endif

#endif
