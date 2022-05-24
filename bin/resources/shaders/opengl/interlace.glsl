//#version 420 // Keep it for editor detection

#ifdef FRAGMENT_SHADER

in vec4 PSin_p;
in vec2 PSin_t;
in vec4 PSin_c;

uniform vec2 ZrH;

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
    vec4 c0 = texture(TextureSampler, PSin_t - ZrH);
    vec4 c1 = texture(TextureSampler, PSin_t);
    vec4 c2 = texture(TextureSampler, PSin_t + ZrH);

    SV_Target0 = (c0 + c1 * 2.0f + c2) / 4.0f;
}

void ps_main3()
{
    SV_Target0 = texture(TextureSampler, PSin_t);
}

#endif
