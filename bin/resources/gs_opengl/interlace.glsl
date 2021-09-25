//#version 420 // Keep it for editor detection

in SHADER
{
    vec4 p;
    vec2 t;
    vec4 c;
} PSin;

#ifdef FRAGMENT_SHADER

layout(std140, binding = 11) uniform cb11
{
    vec2 ZrH;
    float hH;
};

layout(location = 0) out vec4 SV_Target0;

// TODO ensure that clip (discard) is < 0 and not <= 0 ???
void ps_main0()
{
    if (fract(PSin.t.y * hH) - 0.5 < 0.0)
        discard;
    // I'm not sure it impact us but be safe to lookup texture before conditional if
    // see: http://www.opengl.org/wiki/GLSL_Sampler#Non-uniform_flow_control
    vec4 c = texture(TextureSampler, PSin.t);

    SV_Target0 = c;
}

void ps_main1()
{
    if (0.5 - fract(PSin.t.y * hH) < 0.0)
        discard;
    // I'm not sure it impact us but be safe to lookup texture before conditional if
    // see: http://www.opengl.org/wiki/GLSL_Sampler#Non-uniform_flow_control
    vec4 c = texture(TextureSampler, PSin.t);

    SV_Target0 = c;
}

void ps_main2()
{
    vec4 c0 = texture(TextureSampler, PSin.t - ZrH);
    vec4 c1 = texture(TextureSampler, PSin.t);
    vec4 c2 = texture(TextureSampler, PSin.t + ZrH);

    SV_Target0 = (c0 + c1 * 2.0f + c2) / 4.0f;
}

void ps_main3()
{
    SV_Target0 = texture(TextureSampler, PSin.t);
}

#endif
