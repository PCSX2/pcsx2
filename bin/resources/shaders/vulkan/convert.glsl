#ifndef PS_SCALE_FACTOR
#define PS_SCALE_FACTOR 1
#endif

#ifdef VERTEX_SHADER

layout(location = 0) in vec4 a_pos;
layout(location = 1) in vec2 a_tex;

layout(location = 0) out vec2 v_tex;

void main()
{
	gl_Position = vec4(a_pos.x, -a_pos.y, a_pos.z, a_pos.w);
	v_tex = a_tex;
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) in vec2 v_tex;

#if defined(ps_convert_rgba8_16bits) || defined(ps_convert_float32_32bits)
layout(location = 0) out uint o_col0;
#else
layout(location = 0) out vec4 o_col0;
#endif

layout(set = 0, binding = 0) uniform sampler2D samp0;

vec4 sample_c(vec2 uv)
{
	return texture(samp0, uv);
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
		return sample_c(v_tex) * clamp((mask[i] + 0.5f), 0.0f, 1.0f);
}

vec4 ps_scanlines(uint i)
{
		vec4 mask[2] =
		{
				vec4(1, 1, 1, 0),
				vec4(0, 0, 0, 0)
		};

		return sample_c(v_tex) * clamp((mask[i] + 0.5f), 0.0f, 1.0f);
}

#ifdef ps_copy
void ps_copy()
{
	o_col0 = sample_c(v_tex);
}
#endif

#ifdef ps_depth_copy
void ps_depth_copy()
{
  gl_FragDepth = sample_c(v_tex).r;
}
#endif

#ifdef ps_filter_transparency
void ps_filter_transparency()
{
	vec4 c = sample_c(v_tex);
	
	c.a = dot(c.rgb, vec3(0.299, 0.587, 0.114));

	o_col0 = c;
}
#endif

#ifdef ps_convert_rgba8_16bits
// Need to be careful with precision here, it can break games like Spider-Man 3 and Dogs Life
void ps_convert_rgba8_16bits()
{
	uvec4 i = uvec4(sample_c(v_tex) * vec4(255.5f, 255.5f, 255.5f, 255.5f));

	o_col0 = ((i.x & 0x00F8u) >> 3) | ((i.y & 0x00F8u) << 2) | ((i.z & 0x00f8u) << 7) | ((i.w & 0x80u) << 8);
}
#endif

#ifdef ps_datm1
void ps_datm1()
{
	o_col0 = vec4(0, 0, 0, 0);

	if(sample_c(v_tex).a < (127.5f / 255.0f)) // >= 0x80 pass
		discard;

}
#endif

#ifdef ps_datm0
void ps_datm0()
{
	o_col0 = vec4(0, 0, 0, 0);

	if((127.5f / 255.0f) < sample_c(v_tex).a) // < 0x80 pass (== 0x80 should not pass)
		discard;
}
#endif

#ifdef ps_mod256
void ps_mod256()
{
	vec4 c = roundEven(sample_c(v_tex) * 255);
	// We use 2 fmod to avoid negative value.
	vec4 fmod1 = mod(c, 256) + 256;
	vec4 fmod2 = mod(fmod1, 256);

	o_col0 = fmod2 / 255.0f;
}
#endif

#ifdef ps_filter_scanlines
void ps_filter_scanlines() // scanlines
{
	uvec4 p = uvec4(gl_FragCoord);

	o_col0 = ps_scanlines(p.y % 2);
}
#endif

#ifdef ps_filter_diagonal
void ps_filter_diagonal() // diagonal
{
	uvec4 p = uvec4(gl_FragCoord);
	o_col0 = ps_crt((p.x + (p.y % 3)) % 3);
}
#endif

#ifdef ps_filter_triangular
void ps_filter_triangular() // triangular
{
	uvec4 p = uvec4(gl_FragCoord);

	// output.c = ps_crt(input, ((p.x + (p.y & 1) * 3) >> 1) % 3); 
	o_col0 = ps_crt(((p.x + ((p.y >> 1) & 1) * 3) >> 1) % 3);
}
#endif

#ifdef ps_filter_complex
void ps_filter_complex() // triangular
{
	const float PI = 3.14159265359f;
	vec2 texdim = vec2(textureSize(samp0, 0));
	if (dFdy(v_tex.y) * v_tex.y > 0.5) 
		o_col0 = sample_c(v_tex); 
	else
		o_col0 = (0.9 - 0.4 * cos(2 * PI * v_tex.y * texdim.y)) * sample_c(vec2(v_tex.x, (floor(v_tex.y * texdim.y) + 0.5) / texdim.y));
}
#endif

#ifdef ps_convert_float32_32bits
void ps_convert_float32_32bits()
{
	// Convert a vec32 depth texture into a 32 bits UINT texture
	o_col0 = uint(exp2(32.0f) * sample_c(v_tex).r);
}
#endif

#ifdef ps_convert_float32_rgba8
void ps_convert_float32_rgba8()
{
	// Convert a vec32 depth texture into a RGBA color texture
	uint d = uint(sample_c(v_tex).r * exp2(32.0f));
	o_col0 = vec4(uvec4((d & 0xFFu), ((d >> 8) & 0xFFu), ((d >> 16) & 0xFFu), (d >> 24))) / vec4(255.0);
}
#endif

#ifdef ps_convert_float16_rgb5a1
void ps_convert_float16_rgb5a1()
{
	// Convert a vec32 (only 16 lsb) depth into a RGB5A1 color texture
	uint d = uint(sample_c(v_tex).r * exp2(32.0f));
	o_col0 = vec4(uvec4((d & 0x1Fu), ((d >> 5) & 0x1Fu), ((d >> 10) & 0x1Fu), (d >> 15) & 0x01u)) / vec4(32.0f, 32.0f, 32.0f, 1.0f);
}
#endif

#ifdef ps_convert_rgba8_float32
void ps_convert_rgba8_float32()
{
	// Convert a RRGBA texture into a float depth texture
	uvec4 c = uvec4(sample_c(v_tex) * vec4(255.0f) + vec4(0.5f));
	gl_FragDepth = float(c.r | (c.g << 8) | (c.b << 16) | (c.a << 24)) * exp2(-32.0f);
}
#endif

#ifdef ps_convert_rgba8_float24
void ps_convert_rgba8_float24()
{
	// Same as above but without the alpha channel (24 bits Z)

	// Convert a RRGBA texture into a float depth texture
	uvec3 c = uvec3(sample_c(v_tex).rgb * vec3(255.0f) + vec3(0.5f));
	gl_FragDepth = float(c.r | (c.g << 8) | (c.b << 16)) * exp2(-32.0f);
}
#endif

#ifdef ps_convert_rgba8_float16
void ps_convert_rgba8_float16()
{
	// Same as above but without the A/B channels (16 bits Z)

	// Convert a RRGBA texture into a float depth texture
	uvec2 c = uvec2(sample_c(v_tex).rg * vec2(255.0f) + vec2(0.5f));
	gl_FragDepth = float(c.r | (c.g << 8)) * exp2(-32.0f);
}
#endif

#ifdef ps_convert_rgb5a1_float16
void ps_convert_rgb5a1_float16()
{
	// Convert a RGB5A1 (saved as RGBA8) color to a 16 bit Z
	uvec4 c = uvec4(sample_c(v_tex) * vec4(255.0f) + vec4(0.5f));
	gl_FragDepth = float(((c.r & 0xF8u) >> 3) | ((c.g & 0xF8u) << 2) | ((c.b & 0xF8u) << 7) | ((c.a & 0x80u) << 8)) * exp2(-32.0f);
}
#endif

#ifdef ps_convert_rgba_8i
void ps_convert_rgba_8i()
{
	// Potential speed optimization. There is a high probability that
	// game only want to extract a single channel (blue). It will allow
	// to remove most of the conditional operation and yield a +2/3 fps
	// boost on MGS3
	//
	// Hypothesis wrong in Prince of Persia ... Seriously WTF !
	//#define ONLY_BLUE;

	// Convert a RGBA texture into a 8 bits packed texture
	// Input column: 8x2 RGBA pixels
	// 0: 8 RGBA
	// 1: 8 RGBA
	// Output column: 16x4 Index pixels
	// 0: 8 R | 8 B
	// 1: 8 R | 8 B
	// 2: 8 G | 8 A
	// 3: 8 G | 8 A
	float c;

	uvec2 sel = uvec2(gl_FragCoord.xy) % uvec2(16u, 16u);
	ivec2 tb  = ((ivec2(gl_FragCoord.xy) & ~ivec2(15, 3)) >> 1);

	int ty   = tb.y | (int(gl_FragCoord.y) & 1);
	int txN  = tb.x | (int(gl_FragCoord.x) & 7);
	int txH  = tb.x | ((int(gl_FragCoord.x) + 4) & 7);

	txN *= PS_SCALE_FACTOR;
	txH *= PS_SCALE_FACTOR;
	ty  *= PS_SCALE_FACTOR;

	// TODO investigate texture gather
	vec4 cN = texelFetch(samp0, ivec2(txN, ty), 0);
	vec4 cH = texelFetch(samp0, ivec2(txH, ty), 0);


	if ((sel.y & 4u) == 0u)
	{
#ifdef ONLY_BLUE
		c = cN.b;
#else
		// Column 0 and 2
		if ((sel.y & 3u) < 2u)
		{
			// First 2 lines of the col
			if (sel.x < 8u)
				c = cN.r;
			else
				c = cN.b;
		}
		else
		{
			if (sel.x < 8u)
				c = cH.g;
			else
				c = cH.a;
		}
#endif
	}
	else
	{
#ifdef ONLY_BLUE
		c = cH.b;
#else
		// Column 1 and 3
		if ((sel.y & 3u) < 2u)
		{
			// First 2 lines of the col
			if (sel.x < 8u)
				c = cH.r;
			else
				c = cH.b;
		}
		else
		{
			if (sel.x < 8u)
				c = cN.g;
			else
				c = cN.a;
		}
#endif
	}

	o_col0 = vec4(c); // Divide by something here?
}
#endif

#ifdef ps_yuv
layout(push_constant) uniform cb10
{
	int EMODA;
	int EMODC;
};

void ps_yuv()
{
  vec4 i = sample_c(v_tex);
  vec4 o;

  mat3 rgb2yuv;
  rgb2yuv[0] = vec3(0.587, -0.311, -0.419);
  rgb2yuv[1] = vec3(0.114, 0.500, -0.081);
  rgb2yuv[2] = vec3(0.299, -0.169, 0.500);

  vec3 yuv = rgb2yuv * i.gbr;

  float Y = float(0xDB)/255.0f * yuv.x + float(0x10)/255.0f;
  float Cr = float(0xE0)/255.0f * yuv.y + float(0x80)/255.0f;
  float Cb = float(0xE0)/255.0f * yuv.z + float(0x80)/255.0f;

  switch(EMODA) {
		case 0:
			o.a = i.a;
			break;
		case 1:
			o.a = Y;
			break;
		case 2:
			o.a = Y/2.0f;
			break;
		case 3:
			o.a = 0.0f;
			break;
  }

  switch(EMODC) {
    case 0:
      o.rgb = i.rgb;
      break;
    case 1:
      o.rgb = vec3(Y);
      break;
    case 2:
      o.rgb = vec3(Y, Cb, Cr);
      break;
    case 3:
      o.rgb = vec3(i.a);
      break;
  }

	o_col0 = o;
}
#endif

#if defined(ps_stencil_image_init_0) || defined(ps_stencil_image_init_1)

void main()
{
	o_col0 = vec4(0x7FFFFFFF);
	
	#ifdef ps_stencil_image_init_0
		if((127.5f / 255.0f) < sample_c(v_tex).a) // < 0x80 pass (== 0x80 should not pass)
			o_col0 = vec4(-1);
	#endif
	#ifdef ps_stencil_image_init_1
		if(sample_c(v_tex).a < (127.5f / 255.0f)) // >= 0x80 pass
			o_col0 = vec4(-1);
	#endif
}
#endif

#endif