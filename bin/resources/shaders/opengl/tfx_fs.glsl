// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

//#version 420 // Keep it for text editor detection

#define FMT_32 0
#define FMT_24 1
#define FMT_16 2

// TEX_COORD_DEBUG output the uv coordinate as color. It is useful
// to detect bad sampling due to upscaling
//#define TEX_COORD_DEBUG
// Just copy directly the texture coordinate
#ifdef TEX_COORD_DEBUG
#define PS_TFX 1
#define PS_TCC 1
#endif

#define SW_BLEND (PS_BLEND_A || PS_BLEND_B || PS_BLEND_D)
#define SW_BLEND_NEEDS_RT (SW_BLEND && (PS_BLEND_A == 1 || PS_BLEND_B == 1 || PS_BLEND_C == 1 || PS_BLEND_D == 1))
#define SW_AD_TO_HW (PS_BLEND_C == 1 && PS_A_MASKED)
#define PS_PRIMID_INIT (PS_DATE == 1 || PS_DATE == 2)
#define NEEDS_RT_EARLY (PS_TEX_IS_FB == 1 || PS_DATE >= 5)
#define NEEDS_RT (NEEDS_RT_EARLY || (!PS_PRIMID_INIT && (PS_FBMASK || SW_BLEND_NEEDS_RT || SW_AD_TO_HW)))
#define NEEDS_TEX (PS_TFX != 4)

layout(std140, binding = 0) uniform cb21
{
	vec3 FogColor;
	float AREF;

	vec4 WH;

	vec2 TA;
	float MaxDepthPS;
	float Af;

	uvec4 FbMask;

	vec4 HalfTexel;

	vec4 MinMax;
	vec4 STRange;

	ivec4 ChannelShuffle;

	vec2 TC_OffsetHack;
	vec2 STScale;

	mat4 DitherMatrix;

	float ScaledScaleFactor;
	float RcpScaleFactor;
};

in SHADER
{
	vec4 t_float;
	vec4 t_int;

	#if PS_IIP != 0
		vec4 c;
	#else
		flat vec4 c;
	#endif
} PSin;

#define TARGET_0_QUALIFIER out

// Only enable framebuffer fetch when we actually need it.
#if HAS_FRAMEBUFFER_FETCH && NEEDS_RT
	// We need to force the colour to be defined here, to read from it.
	// Basically the only scenario where this'll happen is RGBA masked and DATE is active.
	#undef PS_NO_COLOR
	#define PS_NO_COLOR 0
	#if defined(GL_EXT_shader_framebuffer_fetch)
		#undef TARGET_0_QUALIFIER
		#define TARGET_0_QUALIFIER inout
		#define LAST_FRAG_COLOR SV_Target0
	#elif defined(GL_ARM_shader_framebuffer_fetch)
		#define LAST_FRAG_COLOR gl_LastFragColorARM
	#endif
#endif

#if !PS_NO_COLOR
#if !defined(DISABLE_DUAL_SOURCE) && !PS_NO_COLOR1
	// Same buffer but 2 colors for dual source blending
	layout(location = 0, index = 0) TARGET_0_QUALIFIER vec4 SV_Target0;
	layout(location = 0, index = 1) out vec4 SV_Target1;
#else
	layout(location = 0) TARGET_0_QUALIFIER vec4 SV_Target0;
#endif
#endif

#if NEEDS_TEX
layout(binding = 0) uniform sampler2D TextureSampler;
layout(binding = 1) uniform sampler2D PaletteSampler;
#endif

#if !HAS_FRAMEBUFFER_FETCH && NEEDS_RT
layout(binding = 2) uniform sampler2D RtSampler; // note 2 already use by the image below
#endif

#if PS_DATE == 3
layout(binding = 3) uniform sampler2D img_prim_min;

// I don't remember why I set this parameter but it is surely useless
//layout(pixel_center_integer) in vec4 gl_FragCoord;
#endif

vec4 fetch_rt()
{
#if !NEEDS_RT
	return vec4(0.0);
#elif HAS_FRAMEBUFFER_FETCH
	return LAST_FRAG_COLOR;
#else
	return texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0);
#endif
}

#if NEEDS_TEX

vec4 sample_c(vec2 uv)
{
#if PS_TEX_IS_FB == 1
	return fetch_rt();
#elif PS_REGION_RECT
	return texelFetch(TextureSampler, ivec2(uv), 0);
#else

#if PS_POINT_SAMPLER
	// Weird issue with ATI/AMD cards,
	// it looks like they add 127/128 of a texel to sampling coordinates
	// occasionally causing point sampling to erroneously round up.
	// I'm manually adjusting coordinates to the centre of texels here,
	// though the centre is just paranoia, the top left corner works fine.
	// As of 2018 this issue is still present.
	uv = (trunc(uv * WH.zw) + vec2(0.5, 0.5)) / WH.zw;
#endif
#if !PS_ADJS && !PS_ADJT
	uv *= STScale;
#else
	#if PS_ADJS
		uv.x = (uv.x - STRange.x) * STRange.z;
	#else
		uv.x = uv.x * STScale.x;
	#endif
	#if PS_ADJT
		uv.y = (uv.y - STRange.y) * STRange.w;
	#else
		uv.y = uv.y * STScale.y;
	#endif
#endif

#if PS_AUTOMATIC_LOD == 1
	return texture(TextureSampler, uv);
#elif PS_MANUAL_LOD == 1
	// FIXME add LOD: K - ( LOG2(Q) * (1 << L))
	float K = MinMax.x;
	float L = MinMax.y;
	float bias = MinMax.z;
	float max_lod = MinMax.w;

	float gs_lod = K - log2(abs(PSin.t_float.w)) * L;
	// FIXME max useful ?
	//float lod = max(min(gs_lod, max_lod) - bias, 0.0f);
	float lod = min(gs_lod, max_lod) - bias;

	return textureLod(TextureSampler, uv, lod);
#else
	return textureLod(TextureSampler, uv, 0.0f); // No lod
#endif

#endif
}

vec4 sample_p(uint idx)
{
	return texelFetch(PaletteSampler, ivec2(int(idx), 0), 0);
}

vec4 sample_p_norm(float u)
{
	return sample_p(uint(u * 255.5f));
}

vec4 clamp_wrap_uv(vec4 uv)
{
	vec4 uv_out = uv;
	vec4 tex_size = WH.xyxy;

#if PS_WMS == PS_WMT

#if PS_REGION_RECT == 1 && PS_WMS == 0
	uv_out = fract(uv);
#elif PS_REGION_RECT == 1 && PS_WMS == 1
	uv_out = clamp(uv, vec4(0.0f), vec4(1.0f));
#elif PS_WMS == 2
	uv_out = clamp(uv, MinMax.xyxy, MinMax.zwzw);
#elif PS_WMS == 3
	#if PS_FST == 0
	// wrap negative uv coords to avoid an off by one error that shifted
	// textures. Fixes Xenosaga's hair issue.
	uv = fract(uv);
	#endif
	uv_out = vec4((uvec4(uv * tex_size) & floatBitsToUint(MinMax.xyxy)) | floatBitsToUint(MinMax.zwzw)) / tex_size;
#endif

#else // PS_WMS != PS_WMT

#if PS_REGION_RECT == 1 && PS_WMS == 0
	uv.xz = fract(uv.xz);

#elif PS_REGION_RECT == 1 && PS_WMS == 1
	uv.xz = clamp(uv.xz, vec2(0.0f), vec2(1.0f));

#elif PS_WMS == 2
	uv_out.xz = clamp(uv.xz, MinMax.xx, MinMax.zz);

#elif PS_WMS == 3
	#if PS_FST == 0
		uv.xz = fract(uv.xz);
	#endif
	uv_out.xz = vec2((uvec2(uv.xz * tex_size.xx) & floatBitsToUint(MinMax.xx)) | floatBitsToUint(MinMax.zz)) / tex_size.xx;

#endif

#if PS_REGION_RECT == 1 && PS_WMT == 0
	uv_out.yw = fract(uv.yw);

#elif PS_REGION_RECT == 1 && PS_WMT == 1
	uv_out.yw = clamp(uv.yw, vec2(0.0f), vec2(1.0f));

#elif PS_WMT == 2
	uv_out.yw = clamp(uv.yw, MinMax.yy, MinMax.ww);

#elif PS_WMT == 3
	#if PS_FST == 0
		uv.yw = fract(uv.yw);
	#endif
	uv_out.yw = vec2((uvec2(uv.yw * tex_size.yy) & floatBitsToUint(MinMax.yy)) | floatBitsToUint(MinMax.ww)) / tex_size.yy;
#endif

#endif

#if PS_REGION_RECT == 1
	// Normalized -> Integer Coordinates.
	uv_out = clamp(uv_out * WH.zwzw + STRange.xyxy, STRange.xyxy, STRange.zwzw);
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

	// Either GS will send a texture that contains a single channel
	// in this case the red channel is remapped as alpha channel
	//
	// Or we have an old RT (ie RGBA8) that contains index (4/8) in the alpha channel

	// Note: texture gather can't be used because of special clamping/wrapping
	// Also it doesn't support lod
	c.x = sample_c(uv.xy).a;
	c.y = sample_c(uv.zy).a;
	c.z = sample_c(uv.xw).a;
	c.w = sample_c(uv.zw).a;

	uvec4 i = uvec4(c * 255.5f); // Denormalize value

#if PS_PAL_FMT == 1
	// 4HL
	return i & 0xFu;
#elif PS_PAL_FMT == 2
	// 4HH
	return i >> 4u;
#else
	// 8
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

int fetch_raw_depth()
{
#if HAS_CLIP_CONTROL
	float multiplier = exp2(32.0f);
#else
	float multiplier = exp2(24.0f);
#endif

#if PS_TEX_IS_FB == 1
	return int(fetch_rt().r * multiplier);
#else
	return int(texelFetch(TextureSampler, ivec2(gl_FragCoord.xy), 0).r * multiplier);
#endif
}

vec4 fetch_raw_color()
{
#if PS_TEX_IS_FB == 1
	return fetch_rt();
#else
	return texelFetch(TextureSampler, ivec2(gl_FragCoord.xy), 0);
#endif
}

vec4 fetch_c(ivec2 uv)
{
	return texelFetch(TextureSampler, ivec2(uv), 0);
}

//////////////////////////////////////////////////////////////////////
// Depth sampling
//////////////////////////////////////////////////////////////////////
ivec2 clamp_wrap_uv_depth(ivec2 uv)
{
	ivec2 uv_out = uv;

	// Keep the full precision
	// It allow to multiply the ScalingFactor before the 1/16 coeff
	ivec4 mask = floatBitsToInt(MinMax) << 4;

#if PS_WMS == PS_WMT

#if PS_WMS == 2
	uv_out = clamp(uv, mask.xy, mask.zw);
#elif PS_WMS == 3
	uv_out = (uv & mask.xy) | mask.zw;
#endif

#else // PS_WMS != PS_WMT

#if PS_WMS == 2
	uv_out.x = clamp(uv.x, mask.x, mask.z);
#elif PS_WMS == 3
	uv_out.x = (uv.x & mask.x) | mask.z;
#endif

#if PS_WMT == 2
	uv_out.y = clamp(uv.y, mask.y, mask.w);
#elif PS_WMT == 3
	uv_out.y = (uv.y & mask.y) | mask.w;
#endif

#endif

	return uv_out;
}

vec4 sample_depth(vec2 st)
{
	vec2 uv_f = vec2(clamp_wrap_uv_depth(ivec2(st))) * vec2(ScaledScaleFactor);

	#if PS_REGION_RECT == 1
		uv_f = clamp(uv_f + STRange.xy, STRange.xy, STRange.zw);
	#endif

	ivec2 uv = ivec2(uv_f);
	vec4 t = vec4(0.0f);

#if PS_TALES_OF_ABYSS_HLE == 1
	// Warning: UV can't be used in channel effect
	int depth = fetch_raw_depth();

	// Convert msb based on the palette
	t = texelFetch(PaletteSampler, ivec2((depth >> 8) & 0xFF, 0), 0) * 255.0f;

#elif PS_URBAN_CHAOS_HLE == 1
	// Depth buffer is read as a RGB5A1 texture. The game try to extract the green channel.
	// So it will do a first channel trick to extract lsb, value is right-shifted.
	// Then a new channel trick to extract msb which will shifted to the left.
	// OpenGL uses a FLOAT32 format for the depth so it requires a couple of conversion.
	// To be faster both steps (msb&lsb) are done in a single pass.

	// Warning: UV can't be used in channel effect
	int depth = fetch_raw_depth();

	// Convert lsb based on the palette
	t = texelFetch(PaletteSampler, ivec2((depth & 0xFF), 0), 0) * 255.0f;

	// Msb is easier
	float green = float((depth >> 8) & 0xFF) * 36.0f;
	green = min(green, 255.0f);

	t.g += green;


#elif PS_DEPTH_FMT == 1
	// Based on ps_convert_float32_rgba8 of convert
	// Convert a GL_FLOAT32 depth texture into a RGBA color texture
	#if HAS_CLIP_CONTROL
		uint d = uint(fetch_c(uv).r * exp2(32.0f));
	#else
		uint d = uint(fetch_c(uv).r * exp2(24.0f));
	#endif
	t = vec4(uvec4((d & 0xFFu), ((d >> 8) & 0xFFu), ((d >> 16) & 0xFFu), (d >> 24)));

#elif PS_DEPTH_FMT == 2
	// Based on ps_convert_float16_rgb5a1 of convert
	// Convert a GL_FLOAT32 (only 16 lsb) depth into a RGB5A1 color texture
	#if HAS_CLIP_CONTROL
		uint d = uint(fetch_c(uv).r * exp2(32.0f));
	#else
		uint d = uint(fetch_c(uv).r * exp2(24.0f));
	#endif
	t = vec4(uvec4((d & 0x1Fu), ((d >> 5) & 0x1Fu), ((d >> 10) & 0x1Fu), (d >> 15) & 0x01u)) * vec4(8.0f, 8.0f, 8.0f, 128.0f);

#elif PS_DEPTH_FMT == 3
	// Convert a RGBA/RGB5A1 color texture into a RGBA/RGB5A1 color texture
	t = fetch_c(uv) * 255.0f;

#endif


	// warning t ranges from 0 to 255
#if (PS_AEM_FMT == FMT_24)
	t.a = ( (PS_AEM == 0) || any(bvec3(t.rgb))  ) ? 255.0f * TA.x : 0.0f;
#elif (PS_AEM_FMT == FMT_16)
	t.a = t.a >= 128.0f ? 255.0f * TA.y : ( (PS_AEM == 0) || any(bvec3(t.rgb)) ) ? 255.0f * TA.x : 0.0f;
#endif


	return t;
}

//////////////////////////////////////////////////////////////////////
// Fetch a Single Channel
//////////////////////////////////////////////////////////////////////
vec4 fetch_red()
{
#if PS_DEPTH_FMT == 1 || PS_DEPTH_FMT == 2
	int depth = (fetch_raw_depth()) & 0xFF;
	vec4 rt = vec4(depth) / 255.0f;
#else
	vec4 rt = fetch_raw_color();
#endif
	return sample_p_norm(rt.r) * 255.0f;
}

vec4 fetch_green()
{
#if PS_DEPTH_FMT == 1 || PS_DEPTH_FMT == 2
	int depth = (fetch_raw_depth() >> 8) & 0xFF;
	vec4 rt = vec4(depth) / 255.0f;
#else
	vec4 rt = fetch_raw_color();
#endif
	return sample_p_norm(rt.g) * 255.0f;
}

vec4 fetch_blue()
{
#if PS_DEPTH_FMT == 1 || PS_DEPTH_FMT == 2
	int depth = (fetch_raw_depth() >> 16) & 0xFF;
	vec4 rt = vec4(depth) / 255.0f;
#else
	vec4 rt = fetch_raw_color();
#endif
	return sample_p_norm(rt.b) * 255.0f;
}

vec4 fetch_alpha()
{
	vec4 rt = fetch_raw_color();
	return sample_p_norm(rt.a) * 255.0f;
}

vec4 fetch_rgb()
{
	vec4 rt = fetch_raw_color();
	vec4 c = vec4(sample_p_norm(rt.r).r, sample_p_norm(rt.g).g, sample_p_norm(rt.b).b, 1.0f);
	return c * 255.0f;
}

vec4 fetch_gXbY()
{
#if PS_DEPTH_FMT == 1 || PS_DEPTH_FMT == 2
	int depth = fetch_raw_depth();
	int bg = (depth >> (8 + ChannelShuffle.w)) & 0xFF;
	return vec4(bg);
#else
	ivec4 rt = ivec4(fetch_raw_color() * 255.0f);
	int green = (rt.g >> ChannelShuffle.w) & ChannelShuffle.z;
	int blue  = (rt.b << ChannelShuffle.y) & ChannelShuffle.x;
	return vec4(green | blue);
#endif
}

//////////////////////////////////////////////////////////////////////

vec4 sample_color(vec2 st)
{
#if (PS_TCOFFSETHACK == 1)
	st += TC_OffsetHack.xy;
#endif

	vec4 t;
	mat4 c;
	vec2 dd;

	// FIXME I'm not sure this condition is useful (I think code will be optimized)
#if (PS_LTF == 0 && PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_REGION_RECT == 0 && PS_WMS < 2 && PS_WMT < 2)
	// No software LTF and pure 32 bits RGBA texure without special texture wrapping
	c[0] = sample_c(st);
#ifdef TEX_COORD_DEBUG
	c[0].rg = st.xy;
#endif

#else
	vec4 uv;

	if(PS_LTF != 0)
	{
		uv = st.xyxy + HalfTexel;
		dd = fract(uv.xy * WH.zw);
#if (PS_FST == 0)
		// Background in Shin Megami Tensei Lucifers
		// I suspect that uv isn't a standard number, so fract is outside of the [0;1] range
		// Note: it is free on GPU but let's do it only for float coordinate
		dd = clamp(dd, vec2(0.0f), vec2(1.0f));
#endif
	}
	else
	{
		uv = st.xyxy;
	}

	uv = clamp_wrap_uv(uv);

#if PS_PAL_FMT != 0
	c = sample_4p(sample_4_index(uv));
#else
	c = sample_4c(uv);
#endif

#ifdef TEX_COORD_DEBUG
	c[0].rg = uv.xy;
	c[1].rg = uv.xy;
	c[2].rg = uv.xy;
	c[3].rg = uv.xy;
#endif

#endif

	// PERF note: using dot product reduces by 1 the number of instruction
	// but I'm not sure it is equivalent neither faster.
	for (int i = 0; i < 4; i++)
	{
		//float sum = dot(c[i].rgb, vec3(1.0f));
#if (PS_AEM_FMT == FMT_24)
		c[i].a = ( (PS_AEM == 0) || any(bvec3(c[i].rgb))  ) ? TA.x : 0.0f;
		//c[i].a = ( (PS_AEM == 0) || (sum > 0.0f) ) ? TA.x : 0.0f;
#elif (PS_AEM_FMT == FMT_16)
		c[i].a = c[i].a >= 0.5 ? TA.y : ( (PS_AEM == 0) || any(bvec3(ivec3(c[i].rgb * 255.0f) & ivec3(0xF8))) ) ? TA.x : 0.0f;
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

#endif // NEEDS_TEX

vec4 tfx(vec4 T, vec4 C)
{
	vec4 C_out;
	vec4 FxT = trunc((C * T) / 128.0f);

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
	float a = C.a;

#if (PS_ATST == 0)
	// nothing to do
#elif (PS_ATST == 1)
	if (a > AREF) discard;
#elif (PS_ATST == 2)
	if (a < AREF) discard;
#elif (PS_ATST == 3)
	if (abs(a - AREF) > 0.5f) discard;
#elif (PS_ATST == 4)
	if (abs(a - AREF) < 0.5f) discard;
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
	//FIXME: maybe we can set gl_Position.w = q in VS
#if (PS_FST == 0)
	vec2 st = PSin.t_float.xy / vec2(PSin.t_float.w);
	vec2 st_int = PSin.t_int.zw / vec2(PSin.t_float.w);
#else
	// Note xy are normalized coordinate
	vec2 st = PSin.t_int.xy;
	vec2 st_int = PSin.t_int.zw;
#endif

#if !NEEDS_TEX
	vec4 T = vec4(0.0);
#elif PS_CHANNEL_FETCH == 1
	vec4 T = fetch_red();
#elif PS_CHANNEL_FETCH == 2
	vec4 T = fetch_green();
#elif PS_CHANNEL_FETCH == 3
	vec4 T = fetch_blue();
#elif PS_CHANNEL_FETCH == 4
	vec4 T = fetch_alpha();
#elif PS_CHANNEL_FETCH == 5
	vec4 T = fetch_rgb();
#elif PS_CHANNEL_FETCH == 6
	vec4 T = fetch_gXbY();
#elif PS_DEPTH_FMT > 0
	// Integral coordinate
	vec4 T = sample_depth(st_int);
#else
	vec4 T = sample_color(st);
#endif

	#if PS_SHUFFLE && !PS_SHUFFLE_SAME && !PS_READ16_SRC
		uvec4 denorm_c_before = uvec4(T);
		#if PS_READ_BA
			T.r = float((denorm_c_before.b << 3) & 0xF8);
			T.g = float(((denorm_c_before.b >> 2) & 0x38) | ((denorm_c_before.a << 6) & 0xC0));
			T.b = float((denorm_c_before.a << 1) & 0xF8);
			T.a = float(denorm_c_before.a & 0x80);
		#else
			T.r = float((denorm_c_before.r << 3) & 0xF8);
			T.g = float(((denorm_c_before.r >> 2) & 0x38) | ((denorm_c_before.g << 6) & 0xC0));
			T.b = float((denorm_c_before.g << 1) & 0xF8);
			T.a = float(denorm_c_before.g & 0x80);
		#endif
	#endif
	
	vec4 C = tfx(T, PSin.c);

	atst(C);

	fog(C, PSin.t_float.z);

	return C;
}

void ps_fbmask(inout vec4 C)
{
	// FIXME do I need special case for 16 bits
#if PS_FBMASK
	vec4 RT = trunc(fetch_rt() * 255.0f + 0.1f);
	C = vec4((uvec4(C) & ~FbMask) | (uvec4(RT) & FbMask));
#endif
}

void ps_dither(inout vec3 C)
{
#if PS_DITHER
	#if PS_DITHER == 2
		ivec2 fpos = ivec2(gl_FragCoord.xy);
	#else
		ivec2 fpos = ivec2(gl_FragCoord.xy * RcpScaleFactor);
	#endif
		float value = DitherMatrix[fpos.y&3][fpos.x&3];
	#if PS_ROUND_INV
		C -= value;
	#else
		C += value;
	#endif
#endif
}

void ps_color_clamp_wrap(inout vec3 C)
{
	// When dithering the bottom 3 bits become meaningless and cause lines in the picture
	// so we need to limit the color depth on dithered items
#if SW_BLEND || PS_DITHER || PS_FBMASK

#if PS_DST_FMT == FMT_16 && PS_BLEND_MIX == 0 && PS_ROUND_INV
	C += 7.0f; // Need to round up, not down since the shader will invert
#endif

	// Correct the Color value based on the output format
#if PS_COLCLIP == 0 && PS_HDR == 0
	// Standard Clamp
	C = clamp(C, vec3(0.0f), vec3(255.0f));
#endif

	// FIXME rouding of negative float?
	// compiler uses trunc but it might need floor

	// Warning: normally blending equation is mult(A, B) = A * B >> 7. GPU have the full accuracy
	// GS: Color = 1, Alpha = 255 => output 1
	// GPU: Color = 1/255, Alpha = 255/255 * 255/128 => output 1.9921875
#if PS_DST_FMT == FMT_16 && PS_BLEND_MIX == 0
	// In 16 bits format, only 5 bits of colors are used. It impacts shadows computation of Castlevania
	C = vec3(ivec3(C) & ivec3(0xF8));
#elif PS_COLCLIP == 1 || PS_HDR == 1
	C = vec3(ivec3(C) & ivec3(0xFF));
#endif

#endif
}

void ps_blend(inout vec4 Color, inout vec4 As_rgba)
{
float As = As_rgba.a;

#if SW_BLEND

	// PABE
#if PS_PABE
	// No blending so early exit
	if (As < 1.0f)
		return;
#endif

#if SW_BLEND_NEEDS_RT
	vec4 RT = trunc(fetch_rt() * 255.0f + 0.1f);
#else
	// Not used, but we define it to make the selection below simpler.
	vec4 RT = vec4(0.0f);
#endif
	// FIXME FMT_16 case
	// FIXME Ad or Ad * 2?
	float Ad = RT.a / 128.0f;

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

	// As/Af clamp alpha for Blend mix
	// We shouldn't clamp blend mix with blend hw 1 as we want alpha higher
	float C_clamped = C;
#if PS_BLEND_MIX > 0 && PS_BLEND_HW != 1
	C_clamped = min(C_clamped, 1.0f);
#endif

#if PS_BLEND_A == PS_BLEND_B
	Color.rgb = D;
// In blend_mix, HW adds on some alpha factor * dst.
// Truncating here wouldn't quite get the right result because it prevents the <1 bit here from combining with a <1 bit in dst to form a ≥1 amount that pushes over the truncation.
// Instead, apply an offset to convert HW's round to a floor.
// Since alpha is in 1/128 increments, subtracting (0.5 - 0.5/128 == 127/256) would get us what we want if GPUs blended in full precision.
// But they don't.  Details here: https://github.com/PCSX2/pcsx2/pull/6809#issuecomment-1211473399
// Based on the scripts at the above link, the ideal choice for Intel GPUs is 126/256, AMD 120/256.  Nvidia is a lost cause.
// 124/256 seems like a reasonable compromise, providing the correct answer 99.3% of the time on Intel (vs 99.6% for 126/256), and 97% of the time on AMD (vs 97.4% for 120/256).
#elif PS_BLEND_MIX == 2
	Color.rgb = ((A - B) * C_clamped + D) + (124.0f/256.0f);
#elif PS_BLEND_MIX == 1
	Color.rgb = ((A - B) * C_clamped + D) - (124.0f/256.0f);
#else
	Color.rgb = trunc((A - B) * C + D);
#endif

#if PS_BLEND_HW == 1
	// As or Af
	As_rgba.rgb = vec3(C);
	// Subtract 1 for alpha to compensate for the changed equation,
	// if c.rgb > 255.0f then we further need to adjust alpha accordingly,
	// we pick the lowest overflow from all colors because it's the safest,
	// we divide by 255 the color because we don't know Cd value,
	// changed alpha should only be done for hw blend.
	vec3 alpha_compensate = max(vec3(1.0f), Color.rgb / vec3(255.0f));
	As_rgba.rgb -= alpha_compensate;
#elif PS_BLEND_HW == 2
	// Compensate slightly for Cd*(As + 1) - Cs*As.
	// The initial factor we chose is 1 (0.00392)
	// as that is the minimum color Cd can be,
	// then we multiply by alpha to get the minimum
	// blended value it can be.
	float color_compensate = 1.0f * (C + 1.0f);
	Color.rgb -= vec3(color_compensate);
#elif PS_BLEND_HW == 3
	// As, Ad or Af clamped.
	As_rgba.rgb = vec3(C_clamped);
	// Cs*(Alpha + 1) might overflow, if it does then adjust alpha value
	// that is sent on second output to compensate.
	vec3 overflow_check = (Color.rgb - vec3(255.0f)) / 255.0f;
	vec3 alpha_compensate = max(vec3(0.0f), overflow_check);
	As_rgba.rgb -= alpha_compensate;
#endif

#else
	// Needed for Cd * (As/Ad/F + 1) blending modes
#if PS_BLEND_HW == 1
	Color.rgb = vec3(255.0f);
#elif PS_BLEND_HW == 2
	// Cd*As,Cd*Ad or Cd*F

#if PS_BLEND_C == 2
	float Alpha = Af;
#else
	float Alpha = As;
#endif

	Color.rgb = max(vec3(0.0f), (Alpha - vec3(1.0f)));
	Color.rgb *= vec3(255.0f);
#elif PS_BLEND_HW == 3
	// Needed for Cs*Ad, Cs*Ad + Cd, Cd - Cs*Ad
	// Multiply Color.rgb by (255/128) to compensate for wrong Ad/255 value when rgb are below 128.
	// When any color channel is higher than 128 then adjust the compensation automatically
	// to give us more accurate colors, otherwise they will be wrong.
	// The higher the value (>128) the lower the compensation will be.
	float max_color = max(max(Color.r, Color.g), Color.b);
	float color_compensate = 255.0f / max(128.0f, max_color);
	Color.rgb *= vec3(color_compensate);
#endif

#endif
}

void ps_main()
{
#if PS_SCANMSK & 2
	// fail depth test on prohibited lines
	if ((int(gl_FragCoord.y) & 1) == (PS_SCANMSK & 1))
		discard;
#endif

#if PS_DATE >= 5

#if PS_WRITE_RG == 1
	// Pseudo 16 bits access.
	float rt_a = fetch_rt().g;
#else
	float rt_a = fetch_rt().a;
#endif

#if (PS_DATE & 3) == 1
	// DATM == 0: Pixel with alpha equal to 1 will failed
	bool bad = (127.5f / 255.0f) < rt_a;
#elif (PS_DATE & 3) == 2
	// DATM == 1: Pixel with alpha equal to 0 will failed
	bool bad = rt_a < (127.5f / 255.0f);
#endif

	if (bad) {
		discard;
	}

#endif

#if PS_DATE == 3
	int stencil_ceil = int(texelFetch(img_prim_min, ivec2(gl_FragCoord.xy), 0).r);
	// Note gl_PrimitiveID == stencil_ceil will be the primitive that will update
	// the bad alpha value so we must keep it.

	if (gl_PrimitiveID > stencil_ceil) {
		discard;
	}
#endif

	vec4 C = ps_color();

	// Must be done before alpha correction

	// AA (Fixed one) will output a coverage of 1.0 as alpha
#if PS_FIXED_ONE_A
	C.a = 128.0f;
#endif

#if SW_AD_TO_HW
	vec4 RT = trunc(fetch_rt() * 255.0f + 0.1f);
	vec4 alpha_blend = vec4(RT.a / 128.0f);
#else
	vec4 alpha_blend = vec4(C.a / 128.0f);
#endif

	// Correct the ALPHA value based on the output format
#if (PS_DST_FMT == FMT_16)
	float A_one = 128.0f; // alpha output will be 0x80
	C.a = (PS_FBA != 0) ? A_one : step(128.0f, C.a) * A_one;
#elif (PS_DST_FMT == FMT_32) && (PS_FBA != 0)
	if(C.a < 128.0f) C.a += 128.0f;
#endif

	// Get first primitive that will write a failling alpha value
#if PS_DATE == 1
	// DATM == 0
	// Pixel with alpha equal to 1 will failed (128-255)
	SV_Target0 = (C.a > 127.5f) ? vec4(gl_PrimitiveID) : vec4(0x7FFFFFFF);
	return;
#elif PS_DATE == 2
	// DATM == 1
	// Pixel with alpha equal to 0 will failed (0-127)
	SV_Target0 = (C.a < 127.5f) ? vec4(gl_PrimitiveID) : vec4(0x7FFFFFFF);
	return;
#endif

	ps_blend(C, alpha_blend);


#if PS_SHUFFLE
	#if !PS_SHUFFLE_SAME && !PS_READ16_SRC
		uvec4 denorm_c_after = uvec4(C);
		#if PS_READ_BA
			C.b = float(((denorm_c_after.r >> 3) & 0x1F) | ((denorm_c_after.g << 2) & 0xE0));
			C.a = float(((denorm_c_after.g >> 6) & 0x3) | ((denorm_c_after.b >> 1) & 0x7C) | (denorm_c_after.a & 0x80));
		#else
			C.r = float(((denorm_c_after.r >> 3) & 0x1F) | ((denorm_c_after.g << 2) & 0xE0));
			C.g = float(((denorm_c_after.g >> 6) & 0x3) | ((denorm_c_after.b >> 1) & 0x7C) | (denorm_c_after.a & 0x80));
		#endif
	#endif
	
	uvec4 denorm_c = uvec4(C);
	uvec2 denorm_TA = uvec2(vec2(TA.xy) * 255.0f + 0.5f);

// Special case for 32bit input and 16bit output, shuffle used by The Godfather
#if PS_SHUFFLE_SAME
#if (PS_READ_BA)
	C = vec4(float((denorm_c.b & 0x7Fu) | (denorm_c.a & 0x80u)));
#else
	C.ga = C.rg;
#endif
// Copy of a 16bit source in to this target
#elif PS_READ16_SRC
	C.rb = vec2(float((denorm_c.r >> 3) | (((denorm_c.g >> 3) & 0x7u) << 5)));
	if (bool(denorm_c.a & 0x80u))
		C.ga = vec2(float((denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.y & 0x80u)));
	else
		C.ga = vec2(float((denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.x & 0x80u)));
// Write RB part. Mask will take care of the correct destination
#elif PS_READ_BA
	C.rb = C.bb;
	// FIXME precompute my_TA & 0x80

	// Write GA part. Mask will take care of the correct destination
	// Note: GLSL 4.50/GL_EXT_shader_integer_mix support a mix instruction to select a component\n"
	// However Nvidia emulate it with an if (at least on kepler arch) ...\n"

	// bit field operation requires GL4 HW. Could be nice to merge it with step/mix below
	// uint my_ta = (bool(bitfieldExtract(denorm_c.a, 7, 1))) ? denorm_TA.y : denorm_TA.x;
	// denorm_c.a = bitfieldInsert(denorm_c.a, bitfieldExtract(my_ta, 7, 1), 7, 1);
	// c.ga = vec2(float(denorm_c.a));

	if (bool(denorm_c.a & 0x80u))
		C.ga = vec2(float((denorm_c.a & 0x7Fu) | (denorm_TA.y & 0x80u)));
	else
		C.ga = vec2(float((denorm_c.a & 0x7Fu) | (denorm_TA.x & 0x80u)));

#else
	C.rb = C.rr;
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

#endif // PS_SHUFFLE_SAME
#endif // PS_SHUFFLE

	ps_dither(C.rgb);

	// Color clamp/wrap needs to be done after sw blending and dithering
	ps_color_clamp_wrap(C.rgb);

	ps_fbmask(C);

#if !PS_NO_COLOR
	#if PS_HDR == 1
		SV_Target0 = vec4(C.rgb / 65535.0f, C.a / 255.0f);
	#else
		SV_Target0 = C / 255.0f;
	#endif
	#if !defined(DISABLE_DUAL_SOURCE) && !PS_NO_COLOR1
		SV_Target1 = alpha_blend;
	#endif

	#if PS_NO_ABLEND
		// write alpha blend factor into col0
		SV_Target0.a = alpha_blend.a;
	#endif
	#if PS_ONLY_ALPHA
		// rgb isn't used
		SV_Target0.rgb = vec3(0.0f);
	#endif
#endif

#if PS_ZCLAMP
	gl_FragDepth = min(gl_FragCoord.z, MaxDepthPS);
#endif
}
