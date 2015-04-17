//#version 420 // Keep it for editor detection

struct vertex_basic
{
    vec4 p;
    vec2 t;
};

in SHADER
{
    vec4 p;
    vec2 t;
} PSin;

#define PSin_p (PSin.p)
#define PSin_t (PSin.t)

#ifdef FRAGMENT_SHADER

layout(location = 0) out vec4 SV_Target0;

layout(std140, binding = 10) uniform cb10
{
    vec4 BGColor;
};

#ifdef ENABLE_BINDLESS_TEX
layout(bindless_sampler, location = 0) uniform sampler2D TextureSampler;
#else
layout(binding = 0) uniform sampler2D TextureSampler;
#endif

void ps_main0()
{
    vec4 c = texture(TextureSampler, PSin_t);
    c.a = min(c.a * 2.0, 1.0);
    SV_Target0 = c;
}

void ps_main1()
{
    vec4 c = texture(TextureSampler, PSin_t);
    c.a = BGColor.a;
    SV_Target0 = c;
}

#endif
