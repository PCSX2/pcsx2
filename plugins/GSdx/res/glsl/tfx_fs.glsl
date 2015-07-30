//#version 420 // Keep it for text editor detection

// Require for bit operation
//#extension GL_ARB_gpu_shader5 : enable

#define FMT_32 0
#define FMT_24 1
#define FMT_16 2
#define FMT_PAL 4 /* flag bit */

// APITRACE_DEBUG allows to force pixel output to easily detect
// the fragment computed by primitive
#define APITRACE_DEBUG 0
// TEX_COORD_DEBUG output the uv coordinate as color. It is useful
// to detect bad sampling due to upscaling
//#define TEX_COORD_DEBUG
// Just copy directly the texture coordinate
#ifdef TEX_COORD_DEBUG
#define PS_TFX 1
#define PS_TCC 1
#endif

// Not sure we have same issue on opengl. Doesn't work anyway on ATI card
// And I say this as an ATI user.
#define ATI_SUCKS 0

#define SW_BLEND (PS_BLEND_A || PS_BLEND_B || PS_BLEND_D)

#ifdef FRAGMENT_SHADER

in SHADER
{
	vec4 t;
	vec4 c;
	flat vec4 fc;
} PSin;

#define PSin_t (PSin.t)
#define PSin_c (PSin.c)
#define PSin_fc (PSin.fc)

// Same buffer but 2 colors for dual source blending
layout(location = 0, index = 0) out vec4 SV_Target0;
layout(location = 0, index = 1) out vec4 SV_Target1;

#ifdef ENABLE_BINDLESS_TEX
layout(bindless_sampler, location = 0) uniform sampler2D TextureSampler;
layout(bindless_sampler, location = 1) uniform sampler2D PaletteSampler;
#else
layout(binding = 0) uniform sampler2D TextureSampler;
layout(binding = 1) uniform sampler2D PaletteSampler;
layout(binding = 3) uniform sampler2D RtSampler; // note 2 already use by the image below
#endif

#ifndef DISABLE_GL42_image
#if PS_DATE > 0
// FIXME how to declare memory access
layout(r32i, binding = 2) coherent uniform iimage2D img_prim_min;
// Don't enable it. Discard fragment can still write in the depth buffer
// it breaks shadow in Shin Megami Tensei Nocturne
//layout(early_fragment_tests) in;

// I don't remember why I set this parameter but it is surely useless
//layout(pixel_center_integer) in vec4 gl_FragCoord;
#endif
#else
// use basic stencil
#endif


layout(std140, binding = 21) uniform cb21
{
	vec3 FogColor;
	float AREF;
	vec4 WH;
	vec2 MinF;
	vec2 TA;
	uvec4 MskFix;
	uvec4 FbMask;
	vec3 _not_yet_used;
	float Af;
	vec4 HalfTexel;
	vec4 MinMax;
	vec2 TC_OffsetHack;
};

vec4 sample_c(vec2 uv)
{
	// FIXME: check the issue on openGL
#if (ATI_SUCKS == 1) && (PS_POINT_SAMPLER == 1)
	// Weird issue with ATI cards (happens on at least HD 4xxx and 5xxx),
	// it looks like they add 127/128 of a texel to sampling coordinates
	// occasionally causing point sampling to erroneously round up.
	// I'm manually adjusting coordinates to the centre of texels here,
	// though the centre is just paranoia, the top left corner works fine.
	uv = (trunc(uv * WH.zw) + vec2(0.5, 0.5)) / WH.zw;
#endif

	return texture(TextureSampler, uv);
}

vec4 sample_p(uint idx)
{
	return texelFetch(PaletteSampler, ivec2(idx, 0u), 0);
}

vec4 wrapuv(vec4 uv)
{
	vec4 uv_out = uv;

#if PS_WMS == PS_WMT

#if PS_WMS == 2
	uv_out = clamp(uv, MinMax.xyxy, MinMax.zwzw);
#elif PS_WMS == 3
	uv_out = vec4((ivec4(uv * WH.xyxy) & ivec4(MskFix.xyxy)) | ivec4(MskFix.zwzw)) / WH.xyxy;
#endif

#else // PS_WMS != PS_WMT

#if PS_WMS == 2
	uv_out.xz = clamp(uv.xz, MinMax.xx, MinMax.zz);

#elif PS_WMS == 3
	uv_out.xz = vec2((ivec2(uv.xz * WH.xx) & ivec2(MskFix.xx)) | ivec2(MskFix.zz)) / WH.xx;

#endif

#if PS_WMT == 2
	uv_out.yw = clamp(uv.yw, MinMax.yy, MinMax.ww);

#elif PS_WMT == 3

	uv_out.yw = vec2((ivec2(uv.yw * WH.yy) & ivec2(MskFix.yy)) | ivec2(MskFix.ww)) / WH.yy;
#endif

#endif

	return uv_out;
}

vec2 clampuv(vec2 uv)
{
	vec2 uv_out = uv;

#if (PS_WMS == 2) && (PS_WMT == 2)
	uv_out = clamp(uv, MinF, MinMax.zw);
#elif PS_WMS == 2
	uv_out.x = clamp(uv.x, MinF.x, MinMax.z);
#elif PS_WMT == 2
	uv_out.y = clamp(uv.y, MinF.y, MinMax.w);
#endif

	return uv_out;
}

mat4 sample_4c(vec4 uv)
{
	mat4 c;

    // Note: texture gather can't be used because of special clamping/wrapping
    // Also it doesn't support lod
	c[0] = sample_c(uv.xy);
	c[1] = sample_c(uv.zy);
	c[2] = sample_c(uv.xw);
	c[3] = sample_c(uv.zw);

	return c;
}

uvec4 sample_4_index(vec4 uv)
{
	vec4 c;

	// Either GSdx will send a texture that contains a single channel
	// in this case the red channel is remapped as alpha channel
	//
	// Or we have an old RT (ie RGBA8) that contains index (4/8) in the alpha channel

    // Note: texture gather can't be used because of special clamping/wrapping
    // Also it doesn't support lod
	c.x = sample_c(uv.xy).a;
	c.y = sample_c(uv.zy).a;
	c.z = sample_c(uv.xw).a;
	c.w = sample_c(uv.zw).a;

	uvec4 i = uvec4(c * 255.0f + 0.5f); // Denormalize value

#if PS_IFMT == 1
	// 4HH
	return i >> 4u;
#elif PS_IFMT == 2
	// 4HL
	return i & 0xFu;
#else
	// 8 bits
	return i;
#endif

}

mat4 sample_4p(uvec4 u)
{
	mat4 c;

	c[0] = sample_p(u.x);
	c[1] = sample_p(u.y);
	c[2] = sample_p(u.z);
	c[3] = sample_p(u.w);

	return c;
}

vec4 sample_color(vec2 st, float q)
{
	//FIXME: maybe we can set gl_Position.w = q in VS
#if (PS_FST == 0)
	st /= q;
#endif

#if (PS_TCOFFSETHACK == 1)
	st += TC_OffsetHack.xy;
#endif

	vec4 t;
	mat4 c;
	vec2 dd;

#if (PS_LTF == 0 && PS_FMT <= FMT_16 && PS_WMS < 3 && PS_WMT < 3)
	c[0] = sample_c(clampuv(st));
#ifdef TEX_COORD_DEBUG
	c[0].rg = clampuv(st).xy;
#endif

#else
	vec4 uv;

	if(PS_LTF != 0)
	{
		uv = st.xyxy + HalfTexel;
		dd = fract(uv.xy * WH.zw);
	}
	else
	{
		uv = st.xyxy;
	}

	uv = wrapuv(uv);

	if((PS_FMT & FMT_PAL) != 0)
	{
		c = sample_4p(sample_4_index(uv));
	}
	else
	{
		c = sample_4c(uv);
	}
#ifdef TEX_COORD_DEBUG
	c[0].rg = uv.xy;
	c[1].rg = uv.xy;
	c[2].rg = uv.xy;
	c[3].rg = uv.xy;
#endif

#endif

	// PERF: see the impact of the exansion before/after the interpolation
	for (int i = 0; i < 4; i++)
	{
        // PERF note: using dot produce reduces by 1 the number of instruction
        // but I'm not it is equivalent neither faster.
        //float sum = dot(c[i].rgb, vec3(1.0f));
#if ((PS_FMT & ~FMT_PAL) == FMT_24)
		c[i].a = ( (PS_AEM == 0) || any(bvec3(c[i].rgb))  ) ? TA.x : 0.0f;
		//c[i].a = ( (PS_AEM == 0) || (sum > 0.0f) ) ? TA.x : 0.0f;
#elif ((PS_FMT & ~FMT_PAL) == FMT_16)
		c[i].a = c[i].a >= 0.5 ? TA.y : ( (PS_AEM == 0) || any(bvec3(c[i].rgb)) ) ? TA.x : 0.0f;
		//c[i].a = c[i].a >= 0.5 ? TA.y : ( (PS_AEM == 0) || (sum > 0.0f) ) ? TA.x : 0.0f;
#endif
	}

#if(PS_LTF != 0)
	t = mix(mix(c[0], c[1], dd.x), mix(c[2], c[3], dd.x), dd.y);
#else
	t = c[0];
#endif

	// The 0.05f helps to fix the overbloom of sotc
	// I think the issue is related to the rounding of texture coodinate. The linear (from fixed unit)
	// interpolation could be slightly below the correct one.
	return trunc(t * 255.0f + 0.05f);
}

vec4 tfx(vec4 T, vec4 C)
{
	vec4 C_out;
	vec4 FxT = trunc(trunc(C) * T / 128.0f);

#if (PS_TFX == 0)
	C_out = FxT;
#elif (PS_TFX == 1)
	C_out = T;
#elif (PS_TFX == 2)
	C_out.rgb = FxT.rgb + C.a;
	C_out.a = T.a + C.a;
#elif (PS_TFX == 3)
	C_out.rgb = FxT.rgb + C.a;
	C_out.a = T.a;
#else
	C_out = C;
#endif

#if (PS_TCC == 0)
    C_out.a = C.a;
#endif

#if (PS_TFX == 0) || (PS_TFX == 2) || (PS_TFX == 3)
	// Clamp only when it is useful
	C_out = min(C_out, 255.0f);
#endif

	return C_out;
}

void atst(vec4 C)
{
	// FIXME use integer cmp
	float a = C.a;

#if (PS_ATST == 0) // never
	discard;
#elif (PS_ATST == 1) // always
	// nothing to do
#elif (PS_ATST == 2) // l
	if ((AREF - a - 0.5f) < 0.0f)
		discard;
#elif (PS_ATST == 3 ) // le
	if ((AREF - a + 0.5f) < 0.0f)
		discard;
#elif (PS_ATST == 4) // e
	if ((0.5f - abs(a - AREF)) < 0.0f)
		discard;
#elif (PS_ATST == 5) // ge
	if ((a-AREF + 0.5f) < 0.0f)
		discard;
#elif (PS_ATST == 6) // g
	if ((a-AREF - 0.5f) < 0.0f)
		discard;
#elif (PS_ATST == 7) // ne
	if ((abs(a - AREF) - 0.5f) < 0.0f)
		discard;
#endif
}

void colclip(inout vec4 C)
{
#if (PS_COLCLIP == 2)
	C.rgb = 256.0f - C.rgb;
#endif
#if (PS_COLCLIP == 1 || PS_COLCLIP == 2)
	bvec3 factor = lessThan(C.rgb, vec3(128.0f));
	C.rgb *= vec3(factor);
#endif
}

void fog(inout vec4 C, float f)
{
#if PS_FOG != 0
	C.rgb = trunc(mix(FogColor, C.rgb, f));
#endif
}

vec4 ps_color()
{
	vec4 T = sample_color(PSin_t.xy, PSin_t.w);

#if PS_IIP == 1
	vec4 C = tfx(T, PSin_c);
#else
	vec4 C = tfx(T, PSin_fc);
#endif

	atst(C);

	fog(C, PSin_t.z);

	colclip(C);

#if (PS_CLR1 != 0) // needed for Cd * (As/Ad/F + 1) blending modes
	C.rgb = vec3(255.0f);
#endif

	return C;
}

void ps_fbmask(inout vec4 C)
{
	// FIXME do I need special case for 16 bits
#if PS_FBMASK
	vec4 RT = trunc(texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0) * 255.0f + 0.1f);
	C = vec4((uvec4(C) & ~FbMask) | (uvec4(RT) & FbMask));
#endif
}

void ps_blend(inout vec4 Color, float As)
{
#if SW_BLEND
	vec4 RT = trunc(texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0) * 255.0f + 0.1f);

#if PS_DFMT == FMT_24
	float Ad = 1.0f;
#else
	// FIXME FMT_16 case
	// FIXME Ad or Ad * 2?
	float Ad = RT.a / 128.0f;
#endif

	// Let the compiler do its jobs !
	vec3 Cd = RT.rgb;
	vec3 Cs = Color.rgb;

#if PS_BLEND_A == 0
    vec3 A = Cs;
#elif PS_BLEND_A == 1
    vec3 A = Cd;
#else
    vec3 A = vec3(0.0f);
#endif

#if PS_BLEND_B == 0
    vec3 B = Cs;
#elif PS_BLEND_B == 1
    vec3 B = Cd;
#else
    vec3 B = vec3(0.0f);
#endif

#if PS_BLEND_C == 0
    float C = As;
#elif PS_BLEND_C == 1
    float C = Ad;
#else
    float C = Af;
#endif

#if PS_BLEND_D == 0
    vec3 D = Cs;
#elif PS_BLEND_D == 1
    vec3 D = Cd;
#else
    vec3 D = vec3(0.0f);
#endif

#if PS_BLEND_A == PS_BLEND_B
    Color.rgb = D;
#else
    Color.rgb = trunc((A - B) * C + D);
#endif

	// FIXME dithering

	// Correct the Color value based on the output format
#if PS_COLCLIP != 3
	// Standard Clamp
	Color.rgb = clamp(Color.rgb, vec3(0.0f), vec3(255.0f));
#endif

	// FIXME rouding of negative float?
	// compiler uses trunc but it might need floor

    // Warning: normally blending equation is mult(A, B) = A * B >> 7. GPU have the full accuracy
    // GS: Color = 1, Alpha = 255 => output 1
    // GPU: Color = 1/255, Alpha = 255/255 * 255/128 => output 1.9921875
#if PS_DFMT == FMT_16
	// In 16 bits format, only 5 bits of colors are used. It impacts shadows computation of Castlevania

	Color.rgb = vec3(ivec3(Color.rgb) & ivec3(0xF8));
#elif PS_COLCLIP == 3
	Color.rgb = vec3(ivec3(Color.rgb) & ivec3(0xFF));
#endif

#endif
}

void ps_main()
{
#if (PS_DATE & 3) == 1 && !defined(DISABLE_GL42_image)
	// DATM == 0
	// Pixel with alpha equal to 1 will failed
	float rt_a = texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0).a;
	if ((127.5f / 255.0f) < rt_a) { // < 0x80 pass (== 0x80 should not pass)
#if PS_DATE >= 5
		discard;
#else
		imageStore(img_prim_min, ivec2(gl_FragCoord.xy), ivec4(-1));
		return;
#endif
	}
#elif (PS_DATE & 3) == 2 && !defined(DISABLE_GL42_image)
	// DATM == 1
	// Pixel with alpha equal to 0 will failed
	float rt_a = texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0).a;
	if(rt_a < (127.5f / 255.0f)) { // >= 0x80 pass
#if PS_DATE >= 5
		discard;
#else
		imageStore(img_prim_min, ivec2(gl_FragCoord.xy), ivec4(-1));
		return;
#endif
	}
#endif

#if PS_DATE == 3 && !defined(DISABLE_GL42_image)
	int stencil_ceil = imageLoad(img_prim_min, ivec2(gl_FragCoord.xy)).r;
	// Note gl_PrimitiveID == stencil_ceil will be the primitive that will update
	// the bad alpha value so we must keep it.

	if (gl_PrimitiveID > stencil_ceil) {
		discard;
	}
#endif

	vec4 C = ps_color();
#if (APITRACE_DEBUG & 1) == 1
	C.r = 255f;
#endif
#if (APITRACE_DEBUG & 2) == 2
	C.g = 255f;
#endif
#if (APITRACE_DEBUG & 4) == 4
	C.b = 255f;
#endif
#if (APITRACE_DEBUG & 8) == 8
	C.a = 128f;
#endif

#if PS_SHUFFLE
	uvec4 denorm_c = uvec4(C);
	uvec2 denorm_TA = uvec2(vec2(TA.xy) * 255.0f + 0.5f);

	// Write RB part. Mask will take care of the correct destination
#if PS_READ_BA
	C.rb = C.bb;
#else
	C.rb = C.rr;
#endif

	// FIXME precompute my_TA & 0x80

	// Write GA part. Mask will take care of the correct destination
	// Note: GLSL 4.50/GL_EXT_shader_integer_mix support a mix instruction to select a component\n"
	// However Nvidia emulate it with an if (at least on kepler arch) ...\n"
#if PS_READ_BA
	// bit field operation requires GL4 HW. Could be nice to merge it with step/mix below
	// uint my_ta = (bool(bitfieldExtract(denorm_c.a, 7, 1))) ? denorm_TA.y : denorm_TA.x;
	// denorm_c.a = bitfieldInsert(denorm_c.a, bitfieldExtract(my_ta, 7, 1), 7, 1);
	// c.ga = vec2(float(denorm_c.a));

	if (bool(denorm_c.a & 0x80u))
		C.ga = vec2(float((denorm_c.a & 0x7Fu) | (denorm_TA.y & 0x80u)));
	else
		C.ga = vec2(float((denorm_c.a & 0x7Fu) | (denorm_TA.x & 0x80u)));

#else
	if (bool(denorm_c.g & 0x80u))
		C.ga = vec2(float((denorm_c.g & 0x7Fu) | (denorm_TA.y & 0x80u)));
	else
		C.ga = vec2(float((denorm_c.g & 0x7Fu) | (denorm_TA.x & 0x80u)));

	// Nice idea but step/mix requires 4 instructions
	// set / trunc / I2F / Mad
	//
	// float sel = step(128.0f, c.g);
	// vec2 c_shuffle = vec2((denorm_c.gg & 0x7Fu) | (denorm_TA & 0x80u));
	// c.ga = mix(c_shuffle.xx, c_shuffle.yy, sel);
#endif

#endif

	// Must be done before alpha correction
	float alpha_blend = C.a / 128.0f;

	// Correct the ALPHA value based on the output format
#if (PS_DFMT == FMT_16)
	float A_one = 128.0f; // alpha output will be 0x80
	C.a = (PS_FBA != 0) ? A_one : step(128.0f, C.a) * A_one;
#elif (PS_DFMT == FMT_32) && (PS_FBA != 0)
	if(C.a < 128.0f) C.a += 128.0f;
#endif

	// Get first primitive that will write a failling alpha value
#if PS_DATE == 1 && !defined(DISABLE_GL42_image)
	// DATM == 0
	// Pixel with alpha equal to 1 will failed (128-255)
	if (C.a > 127.5f) {
		imageAtomicMin(img_prim_min, ivec2(gl_FragCoord.xy), gl_PrimitiveID);
		return;
	}
#elif PS_DATE == 2 && !defined(DISABLE_GL42_image)
	// DATM == 1
	// Pixel with alpha equal to 0 will failed (0-127)
	if (C.a < 127.5f) {
		imageAtomicMin(img_prim_min, ivec2(gl_FragCoord.xy), gl_PrimitiveID);
		return;
	}
#endif

	ps_blend(C, alpha_blend);

	ps_fbmask(C);

#if PS_HDR == 1
	// Use negative value to avoid overflow of the texture (in accumulation mode)
	if (any(greaterThan(C.rgb, vec3(128.0f)))) {
		C.rgb = (C.rgb - 256.0f);
	}
#endif
	SV_Target0 = C / 255.0f;
	SV_Target1 = vec4(alpha_blend);
}

#endif
