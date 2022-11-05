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

#ifdef ps_main0
void ps_main0()
{
	o_col0 = texture(samp0, v_tex);
	if ((int(gl_FragCoord.y) & 1) == 0)
		discard;
}
#endif

#ifdef ps_main1
void ps_main1()
{
	o_col0 = texture(samp0, v_tex);
	if ((int(gl_FragCoord.y) & 1) != 0)
		discard;
}
#endif

#ifdef ps_main2
void ps_main2()
{
	vec2 vstep = vec2(0.0f, ZrH.y);
	vec4 c0 = texture(samp0, v_tex - vstep);
	vec4 c1 = texture(samp0, v_tex);
	vec4 c2 = texture(samp0, v_tex + vstep);

	o_col0 = (c0 + c1 * 2.0f + c2) / 4.0f;
}
#endif

#ifdef ps_main3
void ps_main3()
{
	o_col0 = texture(samp0, v_tex);
}
#endif


#ifdef ps_main4
void ps_main4()
{
    const int  vres   = int(round(ZrH.z));
    const int  idx    = int(round(ZrH.x));
    const int  bank   = idx >> 1;
    const int  field  = idx & 1;
	const int  vpos   = int(gl_FragCoord.y) + (((((vres + 1) >> 1) << 1) - vres) & bank);
    const vec2 bofs   = vec2(0.0f, 0.5f * bank);
    const vec2 vscale = vec2(1.0f, 2.0f); 
    const vec2 optr   = v_tex - bofs;
    const vec2 iptr   = optr * vscale;

    if ((optr.y >= 0.0f) && (optr.y < 0.5f) && ((vpos & 1) == field))
        o_col0 = texture(samp0, iptr);
    else
        discard;
}
#endif


#ifdef ps_main5
void ps_main5()
{
    const float sensitivity  = ZrH.w;
    const vec3  motion_thr   = vec3(1.0, 1.0, 1.0) * sensitivity;
    const vec2  vofs         = vec2(0.0f, 0.5f);
    const vec2  vscale       = vec2(1.0f, 0.5f);

    int  idx      = int(round(ZrH.x));
    int  bank     = idx >> 1;
    int  field    = idx & 1;
    vec2 line_ofs = vec2(0.0f, ZrH.y);

    vec2 iptr = v_tex * vscale;
    vec2 p_new_cf = vec2(0.0f, 0.0f);
    vec2 p_old_cf = vec2(0.0f, 0.0f);
    vec2 p_new_af = vec2(0.0f, 0.0f);
    vec2 p_old_af = vec2(0.0f, 0.0f);

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

    vec4 hn = texture(samp0, p_new_cf - line_ofs); // high
    vec4 cn = texture(samp0, p_new_af);            // center
    vec4 ln = texture(samp0, p_new_cf + line_ofs); // low

    vec4 ho = texture(samp0, p_old_cf - line_ofs); // high
    vec4 co = texture(samp0, p_old_af);            // center
    vec4 lo = texture(samp0, p_old_cf + line_ofs); // low

    vec3 mh = hn.rgb - ho.rgb;
    vec3 mc = cn.rgb - co.rgb;
    vec3 ml = ln.rgb - lo.rgb;

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

    if (((int(gl_FragCoord.y) & 1) == field)) // output coordinate present on current field
    {
        o_col0 = texture(samp0, p_new_cf);
        
    }
    else if ((iptr.y > 0.5f - line_ofs.y) || (iptr.y < 0.0 + line_ofs.y))
    {
        o_col0 = texture(samp0, p_new_af);
    }
    else
    {
        if(((mh_max > 0.0f) || (ml_max > 0.0f)) || (mc_max > 0.0f))
        {
            o_col0 = (hn + ln) / 2.0f;
        }
        else
        {
            o_col0 = texture(samp0, p_new_af);
        }
    }
}
#endif

#endif
