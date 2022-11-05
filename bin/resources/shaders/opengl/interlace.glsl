//#version 420 // Keep it for editor detection

#ifdef FRAGMENT_SHADER

in vec4 PSin_p;
in vec2 PSin_t;
in vec4 PSin_c;

uniform vec4 ZrH;

layout(location = 0) out vec4 SV_Target0;

// TODO ensure that clip (discard) is < 0 and not <= 0 ???
void ps_main0()
{
    if ((int(gl_FragCoord.y) & 1) == 0)
        discard;
    // I'm not sure it impact us but be safe to lookup texture before conditional if
    // see: http://www.opengl.org/wiki/GLSL_Sampler#Non-uniform_flow_control
    vec4 c = texture(TextureSampler, PSin_t);

    SV_Target0 = c;
}

void ps_main1()
{
    if ((int(gl_FragCoord.y) & 1) != 0)
        discard;
    // I'm not sure it impact us but be safe to lookup texture before conditional if
    // see: http://www.opengl.org/wiki/GLSL_Sampler#Non-uniform_flow_control
    vec4 c = texture(TextureSampler, PSin_t);

    SV_Target0 = c;
}

void ps_main2()
{
    vec2 vstep = vec2(0.0f, ZrH.y);
    vec4 c0 = texture(TextureSampler, PSin_t - vstep);
    vec4 c1 = texture(TextureSampler, PSin_t);
    vec4 c2 = texture(TextureSampler, PSin_t + vstep);

    SV_Target0 = (c0 + c1 * 2.0f + c2) / 4.0f;
}

void ps_main3()
{
    SV_Target0 = texture(TextureSampler, PSin_t);
}


void ps_main4()
{
    const int  vres   = int(round(ZrH.z));
    const int  idx    = int(round(ZrH.x));
    const int  bank   = idx >> 1;
    const int  field  = idx & 1;
    const int  vpos   = int(gl_FragCoord.y) + (((((vres + 1) >> 1) << 1) - vres) & bank);
    const vec2 bofs   = vec2(0.0f, 0.5f * bank);
    const vec2 vscale = vec2(1.0f, 2.0f); 
    const vec2 optr   = PSin_t - bofs;
    const vec2 iptr   = optr * vscale;

    if ((optr.y >= 0.0f) && (optr.y < 0.5f) && ((vpos & 1) == field))
    //if ((optr.y >= 0.0f) && (optr.y < 0.5f) && (int(iptr.y * vres) & 1) == field)
        SV_Target0 = texture(TextureSampler, iptr);
    else
        discard;
}


void ps_main5()
{
    const float sensitivity  = ZrH.w;
    const vec3  motion_thr   = vec3(1.0, 1.0, 1.0) * sensitivity;
    const vec2  vofs         = vec2(0.0f, 0.5f);
    const vec2  vscale       = vec2(1.0f, 0.5f);
    const int   idx          = int(round(ZrH.x));
    const int   bank         = idx >> 1;
    const int   field        = idx & 1;
    const vec2  line_ofs     = vec2(0.0f, ZrH.y);
    const vec2  iptr         = PSin_t * vscale;

    vec2 p_new_cf;
    vec2 p_old_cf;
    vec2 p_new_af;
    vec2 p_old_af;

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

    vec4 hn = texture(TextureSampler, p_new_cf - line_ofs); // high
    vec4 cn = texture(TextureSampler, p_new_af);            // center
    vec4 ln = texture(TextureSampler, p_new_cf + line_ofs); // low

    vec4 ho = texture(TextureSampler, p_old_cf - line_ofs); // high
    vec4 co = texture(TextureSampler, p_old_af);            // center
    vec4 lo = texture(TextureSampler, p_old_cf + line_ofs); // low

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
        SV_Target0 = texture(TextureSampler, p_new_cf);
        
    }
    else if ((iptr.y > 0.5f - line_ofs.y) || (iptr.y < 0.0 + line_ofs.y))
    {
        SV_Target0 = texture(TextureSampler, p_new_af);
    }
    else
    {
        if(((mh_max > 0.0f) || (ml_max > 0.0f)) || (mc_max > 0.0f))
        {
            SV_Target0 = (hn + ln) / 2.0f;
        }
        else
        {
            SV_Target0 = texture(TextureSampler, p_new_af);
        }
    }
}

#endif
