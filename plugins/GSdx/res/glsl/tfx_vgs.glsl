//#version 420 // Keep it for text editor detection

#ifdef VERTEX_SHADER
layout(location = 0) in vec2  i_st;
layout(location = 2) in vec4  i_c;
layout(location = 3) in float i_q;
layout(location = 4) in uvec2 i_p;
layout(location = 5) in uint  i_z;
layout(location = 6) in uvec2 i_uv;
layout(location = 7) in vec4  i_f;

out SHADER
{
    vec4 t;
    vec4 c;
	flat vec4 fc;
} VSout;

#define VSout_t (VSout.t)
#define VSout_c (VSout.c)
#define VSout_fc (VSout.fc)

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
#if !pGL_ES
    float gl_ClipDistance[];
#endif
};

layout(std140, binding = 20) uniform cb20
{
    vec2 VertexScale;
    vec2 VertexOffset;
    vec2 TextureScale;
};

#ifdef ZERO_TO_ONE_DEPTH
const float exp_min32 = exp2(-32.0f);
#else
const float exp_min31 = exp2(-31.0f);
#endif

#ifdef SUBROUTINE_GL40
// Function pointer type
subroutine void TextureCoordType(void);

// a function pointer variable
layout(location = 0) subroutine uniform TextureCoordType texture_coord;

layout(index = 0) subroutine(TextureCoordType)
void tme_0()
{
    VSout_t.xy = vec2(0.0f, 0.0f);
    VSout_t.w = 1.0f;
}

layout(index = 1) subroutine(TextureCoordType)
void tme_1_fst_0()
{
    VSout_t.xy = i_st;
    VSout_t.w = i_q;
}

layout(index = 2) subroutine(TextureCoordType)
void tme_1_fst_1()
{
    VSout_t.xy = vec2(i_uv) * TextureScale;
    VSout_t.w = 1.0f;
}

#else

void texture_coord()
{
    if(VS_TME != 0)
    {
        if(VS_FST != 0)
        {
            if (VS_WILDHACK == 1) {
                VSout_t.xy = vec2(i_uv &  uvec2(0x3FEF, 0x3FEF)) * TextureScale;
            } else {
                VSout_t.xy = vec2(i_uv) * TextureScale;
            }
            VSout_t.w = 1.0f;
        }
        else
        {
            VSout_t.xy = i_st;
            VSout_t.w = i_q;
        }
    }
    else
    {
        VSout_t.xy = vec2(0.0f, 0.0f);
        VSout_t.w = 1.0f;
    }
}

#endif

void vs_main()
{
    highp uint z;
    if(VS_BPPZ == 1) // 24
        z = i_z & uint(0xffffff);
    else if(VS_BPPZ == 2) // 16
        z = i_z & uint(0xffff);
    else
        z = i_z;

    // pos -= 0.05 (1/320 pixel) helps avoiding rounding problems (integral part of pos is usually 5 digits, 0.05 is about as low as we can go)
    // example: ceil(afterseveralvertextransformations(y = 133)) => 134 => line 133 stays empty
    // input granularity is 1/16 pixel, anything smaller than that won't step drawing up/left by one pixel
    // example: 133.0625 (133 + 1/16) should start from line 134, ceil(133.0625 - 0.05) still above 133
    vec4 p;

    p.xy = vec2(i_p) - vec2(0.05f, 0.05f);
    p.xy = p.xy * VertexScale - VertexOffset;
    p.w = 1.0f;
#ifdef ZERO_TO_ONE_DEPTH
    if(VS_LOGZ == 1) {
        p.z = max(0.0f, log2(float(z))) / 32.0f;
    } else {
        p.z = float(z) * exp_min32;
    }
#else
    if(VS_LOGZ == 1) {
        p.z = max(0.0f, log2(float(z))) / 31.0f - 1.0f;
    } else {
        p.z = float(z) * exp_min31 - 1.0f;
    }
#endif

    gl_Position = p;

    texture_coord();

    VSout_c = i_c;
	VSout_fc = i_c;
    VSout_t.z = i_f.x;
}

#endif

#ifdef GEOMETRY_SHADER

in gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
#if !pGL_ES
    float gl_ClipDistance[];
#endif
} gl_in[];
//in int gl_PrimitiveIDIn;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
#if !pGL_ES
    float gl_ClipDistance[];
#endif
};
//out int gl_PrimitiveID;

in SHADER
{
    vec4 t;
    vec4 c;
    flat vec4 fc;
} GSin[];

out SHADER
{
    vec4 t;
    vec4 c;
    flat vec4 fc;
} GSout;

layout(std140, binding = 22) uniform cb22
{
    vec4 rt_size;
};


struct vertex
{
    vec4 t;
    vec4 c;
    vec2 a;
};

void out_vertex(in vertex v)
{
    GSout.t = v.t;
    GSout.c = v.c;
    gl_PrimitiveID = gl_PrimitiveIDIn;
    EmitVertex();
}

void out_flat(in vec2 dp)
{
    // Flat output
    GSout.fc = GSin[1].fc;
}

layout(lines) in;
layout(triangle_strip, max_vertices = 6) out;

void gs_main()
{
    // Rescale from -1 1 to 0:1 (require window size)
    vec2 dp = vec2(0.0f, 0.0f);

    // left top     => GSin[0];
    // right bottom => GSin[1];
    vertex rb = vertex(GSin[1].t, GSin[1].c, dp);
    vertex lt = vertex(GSin[0].t, GSin[0].c, vec2(0.0f, 0.0f));

    vec4 rb_p = gl_in[1].gl_Position;
    vec4 lb_p = gl_in[1].gl_Position;
    vec4 rt_p = gl_in[1].gl_Position;
    vec4 lt_p = gl_in[0].gl_Position;

    // flat depth
    lt_p.z = rb_p.z;
    // flat fog and texture perspective
    lt.t.zw = rb.t.zw;
    // flat color
    lt.c = rb.c;

	// Swap texture and position coordinate
    vertex lb = rb;
    lb_p.x = lt_p.x;
    lb.t.x = lt.t.x;
    lb.a.x = lt.a.x;

    vertex rt = rb;
    rt_p.y = lt_p.y;
    rt.t.y = lt.t.y;
    rt.a.y = lt.a.y;

    // Triangle 1
    gl_Position = lt_p;
    out_vertex(lt);

    gl_Position = lb_p;
    out_vertex(lb);

    gl_Position = rt_p;
    out_flat(dp);
    out_vertex(rt);
    EndPrimitive();

    // Triangle 2
    gl_Position = lb_p;
    out_vertex(lb);

    gl_Position = rt_p;
    out_vertex(rt);

    gl_Position = rb_p;
    out_flat(dp);
    out_vertex(rb);
    EndPrimitive();
}

#endif
