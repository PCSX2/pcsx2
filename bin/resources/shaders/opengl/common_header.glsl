//#version 420 // Keep it for editor detection

//////////////////////////////////////////////////////////////////////
// Common Interface Definition
//////////////////////////////////////////////////////////////////////

#ifdef VERTEX_SHADER

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
#if !pGL_ES
    float gl_ClipDistance[1];
#endif
};

#endif



#ifdef GEOMETRY_SHADER

in gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
#if !pGL_ES
    float gl_ClipDistance[1];
#endif
} gl_in[];

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
#if !pGL_ES
    float gl_ClipDistance[1];
#endif
};

#endif

//////////////////////////////////////////////////////////////////////
// Constant Buffer Definition
//////////////////////////////////////////////////////////////////////
// Performance note, some drivers (nouveau) will validate all Constant Buffers
// even if only one was updated.

#ifdef FRAGMENT_SHADER
layout(std140, binding = 6) uniform cb15
{
    int EMODA;
    int EMODC;
    ivec2 pad_cb15;
};
#endif

#if defined(VERTEX_SHADER) || defined(GEOMETRY_SHADER)
layout(std140, binding = 1) uniform cb20
{
    vec2  VertexScale;
    vec2  VertexOffset;

    vec2  TextureScale;
    vec2  TextureOffset;

    vec2  PointSize;
    uint  MaxDepth;
    uint  pad_cb20;
};
#endif

#if defined(VERTEX_SHADER) || defined(FRAGMENT_SHADER)
layout(std140, binding = 2) uniform cb21
{
    vec3 FogColor;
    float AREF;

    vec4 WH;

    vec2 TA;
    float MaxDepthPS;
    float Af;

    uvec4 MskFix;

    uvec4 FbMask;

    vec4 HalfTexel;

    vec4 MinMax;

    ivec4 ChannelShuffle;

    vec2 TC_OffsetHack;
    vec2 pad_cb21;

    mat4 DitherMatrix;
};
#endif

//layout(std140, binding = 22) uniform cb22
//{
//    vec4 rt_size;
//};

//////////////////////////////////////////////////////////////////////
// Default Sampler
//////////////////////////////////////////////////////////////////////
#ifdef FRAGMENT_SHADER

layout(binding = 0) uniform sampler2D TextureSampler;

#endif
