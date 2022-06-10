//#version 420 // Keep it for editor detection


#ifdef VERTEX_SHADER

layout(location = 0) in vec2 POSITION;
layout(location = 1) in vec2 TEXCOORD0;
layout(location = 7) in vec4 COLOR;

// FIXME set the interpolation (don't know what dx do)
// flat means that there is no interpolation. The value given to the fragment shader is based on the provoking vertex conventions.
//
// noperspective means that there will be linear interpolation in window-space. This is usually not what you want, but it can have its uses.
//
// smooth, the default, means to do perspective-correct interpolation.
//
// The centroid qualifier only matters when multisampling. If this qualifier is not present, then the value is interpolated to the pixel's center, anywhere in the pixel, or to one of the pixel's samples. This sample may lie outside of the actual primitive being rendered, since a primitive can cover only part of a pixel's area. The centroid qualifier is used to prevent this; the interpolation point must fall within both the pixel's area and the primitive's area.
out vec4 PSin_p;
out vec2 PSin_t;
out vec4 PSin_c;

void vs_main()
{
    PSin_p = vec4(POSITION, 0.5f, 1.0f);
    PSin_t = TEXCOORD0;
    PSin_c = COLOR;
    gl_Position = vec4(POSITION, 0.5f, 1.0f); // NOTE I don't know if it is possible to merge POSITION_OUT and gl_Position
}

#endif

#ifdef FRAGMENT_SHADER

uniform vec4 u_source_rect;
uniform vec4 u_target_rect;
uniform vec2 u_source_size;
uniform vec2 u_target_size;
uniform vec2 u_target_resolution;
uniform vec2 u_rcp_target_resolution; // 1 / u_target_resolution
uniform vec2 u_source_resolution;
uniform vec2 u_rcp_source_resolution; // 1 / u_source_resolution
uniform float u_time;
uniform vec3 cb0_pad0;

in vec4 PSin_p;
in vec2 PSin_t;
in vec4 PSin_c;

layout(location = 0) out vec4 SV_Target0;

vec4 sample_c()
{
    return texture(TextureSampler, PSin_t);
}

vec4 ps_crt(uint i)
{
    vec4 mask[4] = vec4[4]
        (
         vec4(1, 0, 0, 0),
         vec4(0, 1, 0, 0),
         vec4(0, 0, 1, 0),
         vec4(1, 1, 1, 0)
        );
    return sample_c() * clamp((mask[i] + 0.5f), 0.0f, 1.0f);
}

#ifdef ps_copy
void ps_copy()
{
    SV_Target0 = sample_c();
}
#endif

#ifdef ps_filter_scanlines
vec4 ps_scanlines(uint i)
{
    vec4 mask[2] =
    {
        vec4(1, 1, 1, 0),
        vec4(0, 0, 0, 0)
    };

    return sample_c() * clamp((mask[i] + 0.5f), 0.0f, 1.0f);
}

void ps_filter_scanlines() // scanlines
{
    highp uvec4 p = uvec4(gl_FragCoord);

    vec4 c = ps_scanlines(p.y % 2u);

    SV_Target0 = c;
}
#endif

#ifdef ps_filter_diagonal
void ps_filter_diagonal() // diagonal
{
    highp uvec4 p = uvec4(gl_FragCoord);

    vec4 c = ps_crt((p.x + (p.y % 3u)) % 3u);

    SV_Target0 = c;
}
#endif

#ifdef ps_filter_triangular
void ps_filter_triangular() // triangular
{
    highp uvec4 p = uvec4(gl_FragCoord);

    vec4 c = ps_crt(((p.x + ((p.y >> 1u) & 1u) * 3u) >> 1u) % 3u);

    SV_Target0 = c;
}
#endif

#ifdef ps_filter_complex
void ps_filter_complex()
{
    const float PI = 3.14159265359f;
    vec2 texdim = vec2(textureSize(TextureSampler, 0));
    float factor = (0.9f - 0.4f * cos(2.0f * PI * PSin_t.y * texdim.y));
    vec4 c =  factor * texture(TextureSampler, vec2(PSin_t.x, (floor(PSin_t.y * texdim.y) + 0.5f) / texdim.y));

    SV_Target0 = c;
}
#endif

#endif
