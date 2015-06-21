//#version 420 // Keep it for text editor detection

// note lerp => mix

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

// Not sure we have same issue on opengl. Doesn't work anyway on ATI card
// And I say this as an ATI user.
#define ATI_SUCKS 0

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
layout(early_fragment_tests) in;
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

#ifdef SUBROUTINE_GL40
// Function pointer type + the functionn pointer variable
subroutine void AlphaTestType(vec4 c);
layout(location = 0) subroutine uniform AlphaTestType atst;

subroutine vec4 TfxType(vec4 t, vec4 c);
layout(location = 2) subroutine uniform TfxType tfx;

subroutine void ColClipType(inout vec4 c);
layout(location = 1) subroutine uniform ColClipType colclip;
#endif


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

	// FIXME investigate texture gather (filtering impact?)
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

	// FIXME investigate texture gather (filtering impact?)
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
	return i & 16u;
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
#if ((PS_FMT & ~FMT_PAL) == FMT_24)
		c[i].a = ( (PS_AEM == 0) || any(bvec3(c[i].rgb))  ) ? TA.x : 0.0f;
#elif ((PS_FMT & ~FMT_PAL) == FMT_16)
		c[i].a = c[i].a >= 0.5 ? TA.y : ( (PS_AEM == 0) || any(bvec3(c[i].rgb)) ) ? TA.x : 0.0f;
#endif
	}

#if(PS_LTF != 0)
	t = mix(mix(c[0], c[1], dd.x), mix(c[2], c[3], dd.x), dd.y);
#else
	t = c[0];
#endif

	return t;
}

#ifndef SUBROUTINE_GL40
vec4 tfx(vec4 t, vec4 c)
{
	vec4 c_out = c;
#if (PS_TFX == 0)
	if(PS_TCC != 0)
		c_out = c * t * 255.0f / 128.0f;
	else
		c_out.rgb = c.rgb * t.rgb * 255.0f / 128.0f;
#elif (PS_TFX == 1)
	if(PS_TCC != 0)
		c_out = t;
	else
		c_out.rgb = t.rgb;
#elif (PS_TFX == 2)
	c_out.rgb = c.rgb * t.rgb * 255.0f / 128.0f + c.a;

	if(PS_TCC != 0)
		c_out.a += t.a;
#elif (PS_TFX == 3)
	c_out.rgb = c.rgb * t.rgb * 255.0f / 128.0f + c.a;

	if(PS_TCC != 0)
		c_out.a = t.a;
#endif

	return c_out;
}
#endif

#ifndef SUBROUTINE_GL40
void atst(vec4 c)
{
	float a = trunc(c.a * 255.0 + 0.01);

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
#endif

#ifndef SUBROUTINE_GL40
void colclip(inout vec4 c)
{
#if (PS_COLCLIP == 2)
	c.rgb = 256.0f/255.0f - c.rgb;
#endif
#if (PS_COLCLIP > 0)
	bvec3 factor = lessThan(c.rgb, vec3(128.0f/255.0f));
	c.rgb *= vec3(factor);
#endif
}
#endif

void fog(inout vec4 c, float f)
{
#if PS_FOG != 0
	c.rgb = mix(FogColor, c.rgb, f);
#endif
}

vec4 ps_color()
{
	vec4 t = sample_color(PSin_t.xy, PSin_t.w);

	vec4 zero = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	vec4 one = vec4(1.0f, 1.0f, 1.0f, 1.0f);
#ifdef TEX_COORD_DEBUG
	vec4 c = clamp(t, zero, one);
#else
#if PS_IIP == 1
	vec4 c = clamp(tfx(t, PSin_c), zero, one);
#else
	vec4 c = clamp(tfx(t, PSin_fc), zero, one);
#endif
#endif

	atst(c);

	fog(c, PSin_t.z);

#if (PS_COLCLIP < 3)
	colclip(c);
#endif

#if (PS_CLR1 != 0) // needed for Cd * (As/Ad/F + 1) blending modes
	c.rgb = vec3(1.0f, 1.0f, 1.0f);
#endif

	return c;
}

void ps_fbmask(inout vec4 c)
{
	// FIXME do I need special case for 16 bits
#if PS_FBMASK
	vec4 rt = texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0);
	uvec4 denorm_rt = uvec4(rt * 255.0f + 0.5f);
	uvec4 denorm_c = uvec4(c * 255.0f + 0.5f);
	c = vec4((denorm_c & ~FbMask) | (denorm_rt & FbMask)) / 255.0f;
#endif
}

#if PS_BLEND > 0
void ps_blend(inout vec4 c, in float As)
{
	vec4 rt = texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0);
#if PS_DFMT == FMT_24
	float Ad = 1.0f;
#else
	// FIXME FMT_16 case
	// FIXME Ad or Ad * 2?
	float Ad = rt.a * 255.0f / 128.0f;
#endif
	// Let the compiler do its jobs !
	vec3 Cd = rt.rgb;
	vec3 Cs = c.rgb;

#if PS_BLEND == 1
	//   1   =>  0000: (Cs - Cs)*As + Cs ==> Cs
	; // nop

#elif PS_BLEND == 2
	//  2   =>  0001: (Cs - Cs)*As + Cd ==> Cd
	c.rgb = Cd;

#elif PS_BLEND == 3
	//   3   =>  0002: (Cs - Cs)*As +  0 ==> 0
	c.rgb = vec3(0.0);

#elif PS_BLEND == 4
	//   4   => *0100: (Cs - Cd)*As + Cs ==> Cs*(As + 1) - Cd*As
	c.rgb = Cs * (As + 1.0f) - Cd * As;

#elif PS_BLEND == 5
	//   5   => *0110: (Cs - Cd)*Ad + Cs ==> Cs*(Ad + 1) - Cd*Ad
	c.rgb = Cs * (Ad + 1.0f) - Cd * Ad;

#elif PS_BLEND == 6
	//   6   => *0120: (Cs - Cd)*F  + Cs ==> Cs*(F + 1) - Cd*F
	c.rgb = Cs * (Af + 1.0f) - Cd * Af;

#elif PS_BLEND == 7
	//    7   => *0200: (Cs -  0)*As + Cs ==> Cs*(As + 1)
	c.rgb = Cs * (As + 1.0f);

#elif PS_BLEND == 8
	//   8   => *0210: (Cs -  0)*Ad + Cs ==> Cs*(Ad + 1)
	c.rgb = Cs * (Ad + 1.0f);

#elif PS_BLEND == 9
	//   9   => *0220: (Cs -  0)*F  + Cs ==> Cs*(F + 1)
	c.rgb = Cs * (Af + 1.0f);

#elif PS_BLEND == 10
	//   10   => *1001: (Cd - Cs)*As + Cd ==> Cd*(As + 1) - Cs*As
	c.rgb = Cd * (As + 1.0f) - Cs * As;

#elif PS_BLEND == 11
	//   11   => *1011: (Cd - Cs)*Ad + Cd ==> Cd*(Ad + 1) - Cs*Ad
	c.rgb = Cd * (Ad + 1.0f) - Cs * Ad;

#elif PS_BLEND == 12
	//   12   => *1021: (Cd - Cs)*F  + Cd ==> Cd*(F + 1) - Cs*F
	c.rgb = Cd * (Af + 1.0f) - Cs * Af;

#elif PS_BLEND == 13
	//  13   =>  0101: (Cs - Cd)*As + Cd ==> Cs*As + Cd*(1 - As)
	c.rgb = Cs * As + Cd * (1.0f - As);

#elif PS_BLEND == 14
	//  14   =>  0102: (Cs - Cd)*As +  0 ==> Cs*As - Cd*As
	c.rgb = Cs * As - Cd * As;

#elif PS_BLEND == 15
	//  15   =>  0111: (Cs - Cd)*Ad + Cd ==> Cs*Ad + Cd*(1 - Ad)
	c.rgb = Cs * Ad + Cd * (1.0f - Ad);

#elif PS_BLEND == 16
	//  16   =>  0112: (Cs - Cd)*Ad +  0 ==> Cs*Ad - Cd*Ad
	c.rgb = Cs * Ad - Cd * Ad;

#elif PS_BLEND == 17
	//  17   =>  0121: (Cs - Cd)*F  + Cd ==> Cs*F + Cd*(1 - F)
	c.rgb = Cs * Af + Cd * (1.0f - Af);

#elif PS_BLEND == 18
	//  18   =>  0122: (Cs - Cd)*F  +  0 ==> Cs*F - Cd*F
	c.rgb = Cs * Af - Cd * Af;

#elif PS_BLEND == 19
	//  19   =>  0201: (Cs -  0)*As + Cd ==> Cs*As + Cd
	c.rgb = Cs * As + Cd;

#elif PS_BLEND == 20
	//   20   =>  0202: (Cs -  0)*As +  0 ==> Cs*As
	c.rgb = Cs * As;

#elif PS_BLEND == 21
	//  21   =>  0211: (Cs -  0)*Ad + Cd ==> Cs*Ad + Cd
	c.rgb = Cs * Ad + Cd;

#elif PS_BLEND == 22
	//  22   =>  0212: (Cs -  0)*Ad +  0 ==> Cs*Ad
	c.rgb = Cs * Ad;

#elif PS_BLEND == 23
	//  23   =>  0221: (Cs -  0)*F  + Cd ==> Cs*F + Cd
	c.rgb = Cs * Af + Cd;

#elif PS_BLEND == 24
	//   24   =>  0222: (Cs -  0)*F  +  0 ==> Cs*F
	c.rgb = Cs * Af;

#elif PS_BLEND == 25
	//  25   =>  1000: (Cd - Cs)*As + Cs ==> Cd*As + Cs*(1 - As)
	c.rgb = Cd * As + Cs * (1.0f - As);

#elif PS_BLEND == 26
	//  26   =>  1002: (Cd - Cs)*As +  0 ==> Cd*As - Cs*As
	c.rgb = Cd * As - Cs * As;

#elif PS_BLEND == 27
	//  27   =>  1010: (Cd - Cs)*Ad + Cs ==> Cd*Ad + Cs*(1 - Ad)
	c.rgb = Cd * Ad + Cs * (1.0f - Ad);

#elif PS_BLEND == 28
	//  28   =>  1012: (Cd - Cs)*Ad +  0 ==> Cd*Ad - Cs*Ad
	c.rgb = Cd * Ad - Cs * Ad;

#elif PS_BLEND == 29
	//  29   =>  1020: (Cd - Cs)*F  + Cs ==> Cd*F + Cs*(1 - F)
	c.rgb = Cd * Af + Cs * (1.0f - Af);

#elif PS_BLEND == 30
	//  30   =>  1022: (Cd - Cs)*F  +  0 ==> Cd*F - Cs*F
	c.rgb = Cd * Af - Cs * Af;

#elif PS_BLEND == 31
	//  31   =>  1200: (Cd -  0)*As + Cs ==> Cs + Cd*As
	c.rgb = Cs + Cd * As;

#elif PS_BLEND == 55
	//  C_CLR | 55   => #1201: (Cd -  0)*As + Cd ==> Cd*(1 + As)
	c.rgb =  Cd * (1.0f + As);

#elif PS_BLEND == 32
	//  32   =>  1202: (Cd -  0)*As +  0 ==> Cd*As
	c.rgb = Cd * As;

#elif PS_BLEND == 33
	//  33   =>  1210: (Cd -  0)*Ad + Cs ==> Cs + Cd*Ad
	c.rgb = Cs + Cd * Ad;

#elif PS_BLEND == 56
	//  C_CLR | 56   => #1211: (Cd -  0)*Ad + Cd ==> Cd*(1 + Ad)
	c.rgb = Cd * (1.0f + Ad);

#elif PS_BLEND == 34
	//  34   =>  1212: (Cd -  0)*Ad +  0 ==> Cd*Ad
	c.rgb = Cd * Ad;

#elif PS_BLEND == 35
	//   35   =>  1220: (Cd -  0)*F  + Cs ==> Cs + Cd*F
	c.rgb = Cs + Cd * Af;

#elif PS_BLEND == 57
	//  C_CLR | 57   => #1221: (Cd -  0)*F  + Cd ==> Cd*(1 + F)
	c.rgb = Cd * (1.0f + Af);

#elif PS_BLEND == 36
	//  36   =>  1222: (Cd -  0)*F  +  0 ==> Cd*F
	c.rgb = Cd * Af;

#elif PS_BLEND == 37
	//   37   =>  2000: (0  - Cs)*As + Cs ==> Cs*(1 - As)
	c.rgb = Cs * (1.0f - As);

#elif PS_BLEND == 38
	//  38   =>  2001: (0  - Cs)*As + Cd ==> Cd - Cs*As
	c.rgb = Cd - Cs * As;

#elif PS_BLEND == 39
	//   39   =>  2002: (0  - Cs)*As +  0 ==> 0 - Cs*As
	c.rgb = - Cs * As;

#elif PS_BLEND == 40
	//  40   =>  2010: (0  - Cs)*Ad + Cs ==> Cs*(1 - Ad)
	c.rgb = Cs * (1.0f - Ad);

#elif PS_BLEND == 41
	//  41   =>  2011: (0  - Cs)*Ad + Cd ==> Cd - Cs*Ad
	c.rgb = Cd - Cs * Ad;

#elif PS_BLEND == 42
	//  42   =>  2012: (0  - Cs)*Ad +  0 ==> 0 - Cs*Ad
	c.rgb = - Cs * Ad;

#elif PS_BLEND == 43
	//   43   =>  2020: (0  - Cs)*F  + Cs ==> Cs*(1 - F)
	c.rgb = Cs * (1.0f - Af);

#elif PS_BLEND == 44
	//  44   =>  2021: (0  - Cs)*F  + Cd ==> Cd - Cs*F
	c.rgb = Cd - Cs * Af;

#elif PS_BLEND == 45
	//   45   =>  2022: (0  - Cs)*F  +  0 ==> 0 - Cs*F
	c.rgb = - Cs * Af;

#elif PS_BLEND == 46
	//  46   =>  2100: (0  - Cd)*As + Cs ==> Cs - Cd*As
	c.rgb = Cs - Cd * As;

#elif PS_BLEND == 47
	//  47   =>  2101: (0  - Cd)*As + Cd ==> Cd*(1 - As)
	c.rgb = Cd * (1.0f - As);

#elif PS_BLEND == 48
	//  48   =>  2102: (0  - Cd)*As +  0 ==> 0 - Cd*As
	c.rgb = - Cd * As;

#elif PS_BLEND == 49
	//  49   =>  2110: (0  - Cd)*Ad + Cs ==> Cs - Cd*Ad
	c.rgb = Cs - Cd * Ad;

#elif PS_BLEND == 50
	//  50   =>  2111: (0  - Cd)*Ad + Cd ==> Cd*(1 - Ad)
	c.rgb = Cd * (1.0f - Ad);

#elif PS_BLEND == 51
	//  51   =>  2112: (0  - Cd)*Ad +  0 ==> 0 - Cd*Ad
	c.rgb = - Cd * Ad;

#elif PS_BLEND == 52
	//  52   =>  2120: (0  - Cd)*F  + Cs ==> Cs - Cd*F
	c.rgb = Cs - Cd * Af;

#elif PS_BLEND == 53
	//  53   =>  2121: (0  - Cd)*F  + Cd ==> Cd*(1 - F)
	c.rgb = Cd * (1.0f - Af);

#elif PS_BLEND == 54
	//  54   =>  2122: (0  - Cd)*F  +  0 ==> 0 - Cd*F
	c.rgb = - Cd * Af;

#endif

	// FIXME dithering

	// Correct the Color value based on the output format
#if PS_COLCLIP != 3
	// Standard Clamp
	c.rgb = clamp(c.rgb, vec3(0.0f), vec3(1.0f));
#endif

#if PS_DFMT == FMT_16
	// In 16 bits format, only 5 bits of colors are used. It impacts shadows computation of Castlevania

	// Basically we want to do 'c.rgb &= 0xF8' in denormalized mode
	c.rgb = vec3(uvec3((c.rgb * 255.0f) + 256.5f) & uvec3(0xF8)) / 255.0f;
#elif PS_COLCLIP == 3
	// Basically we want to do 'c.rgb &= 0xFF' in denormalized mode
	c.rgb = vec3(uvec3((c.rgb * 255.0f) + 256.5f) & uvec3(0xFF)) / 255.0f;
#endif

	// Don't compile => unable to find compatible overloaded function "mod(vec3)"
	//c.rgb = mod((c.rgb * 255.0f) + 256.5f) / 255.0f;
}
#endif

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
	int stencil_ceil = imageLoad(img_prim_min, ivec2(gl_FragCoord.xy));
	// Note gl_PrimitiveID == stencil_ceil will be the primitive that will update
	// the bad alpha value so we must keep it.

	if (gl_PrimitiveID > stencil_ceil) {
		discard;
	}
#endif

	vec4 c = ps_color();
#if (APITRACE_DEBUG & 1) == 1
	c.r = 1.0f;
#endif
#if (APITRACE_DEBUG & 2) == 2
	c.g = 1.0f;
#endif
#if (APITRACE_DEBUG & 4) == 4
	c.b = 1.0f;
#endif
#if (APITRACE_DEBUG & 8) == 8
	c.a = 0.5f;
#endif

#if PS_SHUFFLE
	uvec4 denorm_c = uvec4(c * 255.0f + 0.5f);
	uvec2 denorm_TA = uvec2(vec2(TA.xy) * 255.0f + 0.5f);

	// Write RB part. Mask will take care of the correct destination
#if PS_READ_BA
	c.rb = c.bb;
#else
	c.rb = c.rr;
#endif

	// Write GA part. Mask will take care of the correct destination
#if PS_READ_BA
	if (bool(denorm_c.a & 0x80u))
		c.ga = vec2(float((denorm_c.a & 0x7Fu) | (denorm_TA.y & 0x80u)) / 255.0f);
	else
		c.ga = vec2(float((denorm_c.a & 0x7Fu) | (denorm_TA.x & 0x80u)) / 255.0f);
#else
	if (bool(denorm_c.g & 0x80u))
		c.ga = vec2(float((denorm_c.g & 0x7Fu) | (denorm_TA.y & 0x80u)) / 255.0f);
	else
		c.ga = vec2(float((denorm_c.g & 0x7Fu) | (denorm_TA.x & 0x80u)) / 255.0f);
#endif

#endif

	// Must be done before alpha correction
	float alpha = c.a * 255.0f / 128.0f;

	// Correct the ALPHA value based on the output format
	// FIXME add support of alpha mask to replace properly PS_AOUT
#if (PS_DFMT == FMT_16) || (PS_AOUT)
	float a = 128.0f / 255.0; // alpha output will be 0x80
	c.a = (PS_FBA != 0) ? a : step(0.5, c.a) * a;
#elif (PS_DFMT == FMT_32) && (PS_FBA != 0)
	if(c.a < 0.5) c.a += 128.0f/255.0f;
#endif

	// Get first primitive that will write a failling alpha value
#if PS_DATE == 1 && !defined(DISABLE_GL42_image)
	// DATM == 0
	// Pixel with alpha equal to 1 will failed (128-255)
	if (c.a > 127.5f / 255.0f) {
		imageAtomicMin(img_prim_min, ivec2(gl_FragCoord.xy), gl_PrimitiveID);
		return;
	}
#elif PS_DATE == 2 && !defined(DISABLE_GL42_image)
	// DATM == 1
	// Pixel with alpha equal to 0 will failed (0-127)
	if (c.a < 127.5f / 255.0f) {
		imageAtomicMin(img_prim_min, ivec2(gl_FragCoord.xy), gl_PrimitiveID);
		return;
	}
#endif

#if PS_BLEND > 0
	ps_blend(c, alpha);
#endif

	ps_fbmask(c);

	SV_Target0 = c;
	SV_Target1 = vec4(alpha, alpha, alpha, alpha);
}

#endif
