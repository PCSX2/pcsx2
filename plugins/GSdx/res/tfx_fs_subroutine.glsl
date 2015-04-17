//#version 420 // Keep it for text editor detection

// Subroutine of standard fs function (I don't know if it will be ever used one day)

// FIXME crash nvidia
#if 0
// Function pointer type
subroutine vec4 WrapType(vec4 uv);

// a function pointer variable
layout(location = 4) subroutine uniform WrapType wrapuv;

layout(index = 24) subroutine(WrapType)
vec4 wrapuv_wms_wmt_2(vec4 uv)
{
    vec4 uv_out = uv;
    uv_out = clamp(uv, MinMax.xyxy, MinMax.zwzw);
    return uv_out;
}

layout(index = 25) subroutine(WrapType)
vec4 wrapuv_wms_wmt3(vec4 uv)
{
    vec4 uv_out = uv;
    uv_out = vec4((ivec4(uv * WH.xyxy) & ivec4(MskFix.xyxy)) | ivec4(MskFix.zwzw)) / WH.xyxy;
    return uv_out;
}

layout(index = 26) subroutine(WrapType)
vec4 wrapuv_wms2_wmt3(vec4 uv)
{
    vec4 uv_out = uv;
    uv_out.xz = clamp(uv.xz, MinMax.xx, MinMax.zz);
    uv_out.yw = vec2((ivec2(uv.yw * WH.yy) & ivec2(MskFix.yy)) | ivec2(MskFix.ww)) / WH.yy;
    return uv_out;
}

layout(index = 27) subroutine(WrapType)
vec4 wrapuv_wms3_wmt2(vec4 uv)
{
    vec4 uv_out = uv;
    uv_out.xz = vec2((ivec2(uv.xz * WH.xx) & ivec2(MskFix.xx)) | ivec2(MskFix.zz)) / WH.xx;
    uv_out.yw = clamp(uv.yw, MinMax.yy, MinMax.ww);
    return uv_out;
}

layout(index = 28) subroutine(WrapType)
vec4 wrapuv_wms2_wmtx(vec4 uv)
{
    vec4 uv_out = uv;
    uv_out.xz = clamp(uv.xz, MinMax.xx, MinMax.zz);
    return uv_out;
}

layout(index = 29) subroutine(WrapType)
vec4 wrapuv_wmsx_wmt3(vec4 uv)
{
    vec4 uv_out = uv;
    uv_out.yw = vec2((ivec2(uv.yw * WH.yy) & ivec2(MskFix.yy)) | ivec2(MskFix.ww)) / WH.yy;
    return uv_out;
}

layout(index = 30) subroutine(WrapType)
vec4 wrapuv_wms3_wmtx(vec4 uv)
{
    vec4 uv_out = uv;
    uv_out.xz = vec2((ivec2(uv.xz * WH.xx) & ivec2(MskFix.xx)) | ivec2(MskFix.zz)) / WH.xx;
    return uv_out;
}

layout(index = 31) subroutine(WrapType)
vec4 wrapuv_wmsx_wmt2(vec4 uv)
{
    vec4 uv_out = uv;
    uv_out.yw = clamp(uv.yw, MinMax.yy, MinMax.ww);
    return uv_out;
}

layout(index = 32) subroutine(WrapType)
vec4 wrapuv_dummy(vec4 uv)
{
    return uv;
}
#endif

// FIXME crash nvidia
#if 0
// Function pointer type
subroutine vec2 ClampType(vec2 uv);

// a function pointer variable
layout(location = 3) subroutine uniform ClampType clampuv;

layout(index = 20) subroutine(ClampType)
vec2 clampuv_wms2_wmt2(vec2 uv)
{
    return clamp(uv, MinF, MinMax.zw);
}

layout(index = 21) subroutine(ClampType)
vec2 clampuv_wms2(vec2 uv)
{
    vec2 uv_out = uv;
    uv_out.x = clamp(uv.x, MinF.x, MinMax.z);
    return uv_out;
}

layout(index = 22) subroutine(ClampType)
vec2 clampuv_wmt2(vec2 uv)
{
    vec2 uv_out = uv;
    uv_out.y = clamp(uv.y, MinF.y, MinMax.w);
    return uv_out;
}

layout(index = 23) subroutine(ClampType)
vec2 clampuv_dummy(vec2 uv)
{
    return uv;
}
#endif

#ifdef SUBROUTINE_GL40
layout(index = 11) subroutine(TfxType)
vec4 tfx_0_tcc_0(vec4 t, vec4 c)
{
    vec4 c_out = c;
    c_out.rgb = c.rgb * t.rgb * 255.0f / 128.0f;
    return c_out;
}

layout(index = 12) subroutine(TfxType)
vec4 tfx_1_tcc_0(vec4 t, vec4 c)
{
    vec4 c_out = c;
    c_out.rgb = t.rgb;
    return c_out;
}

layout(index = 13) subroutine(TfxType)
vec4 tfx_2_tcc_0(vec4 t, vec4 c)
{
    vec4 c_out = c;
    c_out.rgb = c.rgb * t.rgb * 255.0f / 128.0f + c.a;
    return c_out;
}

layout(index = 14) subroutine(TfxType)
vec4 tfx_3_tcc_0(vec4 t, vec4 c)
{
    vec4 c_out = c;
    c_out.rgb = c.rgb * t.rgb * 255.0f / 128.0f + c.a;
    return c_out;
}

layout(index = 15) subroutine(TfxType)
vec4 tfx_0_tcc_1(vec4 t, vec4 c)
{
    vec4 c_out = c;
    c_out = c * t * 255.0f / 128.0f;
    return c_out;
}

layout(index = 16) subroutine(TfxType)
vec4 tfx_1_tcc_1(vec4 t, vec4 c)
{
    vec4 c_out = c;
    c_out = t;
    return c_out;
}

layout(index = 17) subroutine(TfxType)
vec4 tfx_2_tcc_1(vec4 t, vec4 c)
{
    vec4 c_out = c;
    c_out.rgb = c.rgb * t.rgb * 255.0f / 128.0f + c.a;
    c_out.a += t.a;
    return c_out;
}

layout(index = 18) subroutine(TfxType)
vec4 tfx_3_tcc_1(vec4 t, vec4 c)
{
    vec4 c_out = c;
    c_out.rgb = c.rgb * t.rgb * 255.0f / 128.0f + c.a;
    c_out.a = t.a;
    return c_out;
}

layout(index = 19) subroutine(TfxType)
vec4 tfx_dummy(vec4 t, vec4 c)
{
    return c;
}
#endif

#ifdef SUBROUTINE_GL40
layout(index = 0) subroutine(AlphaTestType)
void atest_never(vec4 c)
{
    discard;
}

layout(index = 1) subroutine(AlphaTestType)
void atest_always(vec4 c)
{
    // Nothing to do
}

layout(index = 2) subroutine(AlphaTestType)
void atest_l(vec4 c)
{
    float a = trunc(c.a * 255.0 + 0.01);
    if (PS_SPRITEHACK == 0)
        if ((AREF - a - 0.5f) < 0.0f)
            discard;
}

layout(index = 3) subroutine(AlphaTestType)
void atest_le(vec4 c)
{
    float a = trunc(c.a * 255.0 + 0.01);
    if ((AREF - a + 0.5f) < 0.0f)
        discard;
}

layout(index = 4) subroutine(AlphaTestType)
void atest_e(vec4 c)
{
    float a = trunc(c.a * 255.0 + 0.01);
    if ((0.5f - abs(a - AREF)) < 0.0f)
        discard;
}

layout(index = 5) subroutine(AlphaTestType)
void atest_ge(vec4 c)
{
    float a = trunc(c.a * 255.0 + 0.01);
    if ((a-AREF + 0.5f) < 0.0f)
        discard;
}

layout(index = 6) subroutine(AlphaTestType)
void atest_g(vec4 c)
{
    float a = trunc(c.a * 255.0 + 0.01);
    if ((a-AREF - 0.5f) < 0.0f)
        discard;
}

layout(index = 7) subroutine(AlphaTestType)
void atest_ne(vec4 c)
{
    float a = trunc(c.a * 255.0 + 0.01);
    if ((abs(a - AREF) - 0.5f) < 0.0f)
        discard;
}
#endif

#ifdef SUBROUTINE_GL40
layout(index = 8) subroutine(ColClipType)
void colclip_0(inout vec4 c)
{
	// nothing to do
}

layout(index = 9) subroutine(ColClipType)
void colclip_1(inout vec4 c)
{
	// FIXME !!!!
	//c.rgb *= c.rgb < 128./255;
	bvec3 factor = bvec3(128.0f/255.0f, 128.0f/255.0f, 128.0f/255.0f);
	c.rgb *= vec3(factor);
}

layout(index = 10) subroutine(ColClipType)
void colclip_2(inout vec4 c)
{
	c.rgb = 256.0f/255.0f - c.rgb;
	// FIXME !!!!
	//c.rgb *= c.rgb < 128./255;
	bvec3 factor = bvec3(128.0f/255.0f, 128.0f/255.0f, 128.0f/255.0f);
	c.rgb *= vec3(factor);
}
#endif
