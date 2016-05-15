#ifdef SHADER_MODEL // make safe to include in resource file to enforce dependency
#if SHADER_MODEL >= 0x400

struct VS_INPUT
{
	float4 p : POSITION; 
	float2 t : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
};

Texture2D Texture;
SamplerState TextureSampler;

float4 sample_c(float2 uv)
{
	return Texture.Sample(TextureSampler, uv);
}

float4 sample_lod(float2 uv, float lod)
{
	return Texture.SampleLevel(TextureSampler, uv, lod);
}

struct PS_INPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
};

struct PS_OUTPUT
{
	float4 c : SV_Target0;
};

#elif SHADER_MODEL <= 0x300

struct VS_INPUT
{
	float4 p : POSITION; 
	float2 t : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 p : POSITION;
	float2 t : TEXCOORD0;
};

struct PS_INPUT
{
#if SHADER_MODEL < 0x300
	float4 p : TEXCOORD1;
#else
	float4 p : VPOS;
#endif
	float2 t : TEXCOORD0;
};

struct PS_OUTPUT
{
	float4 c : COLOR;
};

sampler Texture : register(s0);

float4 sample_c(float2 uv)
{
	return tex2D(Texture, uv);
}
#endif

VS_OUTPUT vs_main(VS_INPUT input)
{
	VS_OUTPUT output;

	output.p = input.p;
	output.t = input.t;

	return output;
}

PS_OUTPUT ps_main0(PS_INPUT input)
{
	PS_OUTPUT output;
	
	output.c = sample_c(input.t);

	return output;
}

PS_OUTPUT ps_main7(PS_INPUT input)
{
	PS_OUTPUT output;
	
	float4 c = sample_c(input.t);
	
	c.a = dot(c.rgb, float3(0.299, 0.587, 0.114));

	output.c = c;

	return output;
}

float4 ps_crt(PS_INPUT input, int i)
{
	float4 mask[4] = 
	{
		float4(1, 0, 0, 0), 
		float4(0, 1, 0, 0), 
		float4(0, 0, 1, 0), 
		float4(1, 1, 1, 0)
	};
	
	return sample_c(input.t) * saturate(mask[i] + 0.5f);
}

float4 ps_scanlines(PS_INPUT input, int i)
{
	float4 mask[2] =
	{
		float4(1, 1, 1, 0),
		float4(0, 0, 0, 0)
	};

	return sample_c(input.t) * saturate(mask[i] + 0.5f);
}

#if SHADER_MODEL >= 0x400

uint ps_main1(PS_INPUT input) : SV_Target0
{
	float4 c = sample_c(input.t);

	c.a *= 256.0f / 127; // hm, 0.5 won't give us 1.0 if we just multiply with 2

	uint4 i = c * float4(0x001f, 0x03e0, 0x7c00, 0x8000);

	return (i.x & 0x001f) | (i.y & 0x03e0) | (i.z & 0x7c00) | (i.w & 0x8000);	
}

PS_OUTPUT ps_main2(PS_INPUT input)
{
	PS_OUTPUT output;
	
	clip(sample_c(input.t).a - 127.5f / 255); // >= 0x80 pass
	
	output.c = 0;

	return output;
}

PS_OUTPUT ps_main3(PS_INPUT input)
{
	PS_OUTPUT output;
	
	clip(127.5f / 255 - sample_c(input.t).a); // < 0x80 pass (== 0x80 should not pass)
	
	output.c = 0;

	return output;
}

PS_OUTPUT ps_main4(PS_INPUT input)
{
	PS_OUTPUT output;
	
	output.c = fmod(sample_c(input.t) * 255 + 0.5f, 256) / 255;

	return output;
}

PS_OUTPUT ps_main5(PS_INPUT input) // scanlines
{
	PS_OUTPUT output;
	
	uint4 p = (uint4)input.p;

	output.c = ps_scanlines(input, p.y % 2);

	return output;
}

PS_OUTPUT ps_main6(PS_INPUT input) // diagonal
{
	PS_OUTPUT output;

	uint4 p = (uint4)input.p;

	output.c = ps_crt(input, (p.x + (p.y % 3)) % 3);

	return output;
}

PS_OUTPUT ps_main8(PS_INPUT input) // triangular
{
	PS_OUTPUT output;

	uint4 p = (uint4)input.p;

	// output.c = ps_crt(input, ((p.x + (p.y & 1) * 3) >> 1) % 3); 
	output.c = ps_crt(input, ((p.x + ((p.y >> 1) & 1) * 3) >> 1) % 3);

	return output;
}

static const float PI = 3.14159265359f;
PS_OUTPUT ps_main9(PS_INPUT input) // triangular
{
	PS_OUTPUT output;

	float2 texdim, halfpixel; 
	Texture.GetDimensions(texdim.x, texdim.y); 
	if (ddy(input.t.y) * texdim.y > 0.5) 
		output.c = sample_c(input.t); 
	else
		output.c = (0.9 - 0.4 * cos(2 * PI * input.t.y * texdim.y)) * sample_c(float2(input.t.x, (floor(input.t.y * texdim.y) + 0.5) / texdim.y));

	return output;
}

PS_OUTPUT ps_main10(PS_INPUT input) // Bicubic Scaling
{
	PS_OUTPUT output;

	float2 _xyFrame;
	Texture.GetDimensions(_xyFrame.x, _xyFrame.y);
	float2 inputSize = float2(1.0 / _xyFrame.x, 1.0 / _xyFrame.y);

	float2 coord_hg = input.t * _xyFrame - 0.5;
	float2 index = floor(coord_hg);
	float2 f = coord_hg - index;

	float4x4 M = {
	-1.0, 3.0,-3.0, 1.0, 3.0,-6.0, 3.0, 0.0,
	-3.0, 0.0, 3.0, 0.0, 1.0, 4.0, 1.0, 0.0 };

	M /= 6.0;

	float4 wx = mul(float4(f.x*f.x*f.x, f.x*f.x, f.x, 1.0), M);
	float4 wy = mul(float4(f.y*f.y*f.y, f.y*f.y, f.y, 1.0), M);
	float2 w0 = float2(wx.x, wy.x);
	float2 w1 = float2(wx.y, wy.y);
	float2 w2 = float2(wx.z, wy.z);
	float2 w3 = float2(wx.w, wy.w);

	float2 g0 = w0 + w1;
	float2 g1 = w2 + w3;
	float2 h0 = w1 / g0 - 1.0;
	float2 h1 = w3 / g1 + 1.0;

	float2 coord00 = index + h0;
	float2 coord10 = index + float2(h1.x, h0.y);
	float2 coord01 = index + float2(h0.x, h1.y);
	float2 coord11 = index + h1;

	coord00 = (coord00 + 0.5) * inputSize;
	coord10 = (coord10 + 0.5) * inputSize;
	coord01 = (coord01 + 0.5) * inputSize;
	coord11 = (coord11 + 0.5) * inputSize;

	float4 tex00 = sample_lod(coord00, 0.0);
	float4 tex10 = sample_lod(coord10, 0.0);
	float4 tex01 = sample_lod(coord01, 0.0);
	float4 tex11 = sample_lod(coord11, 0.0);

	tex00 = lerp(tex01, tex00, float4(g0.y, g0.y, g0.y, g0.y));
	tex10 = lerp(tex11, tex10, float4(g0.y, g0.y, g0.y, g0.y));

	float4 res = lerp(tex10, tex00, float4(g0.x, g0.x, g0.x, g0.x));

	output.c = res;

	return output;
}

float3 PixelPos(float xpos, float ypos) // Lanczos Scaling
{
	return sample_c(float2(xpos, ypos)).rgb;
}

float4 WeightQuad(float x)
{
	#define FIX(c) max(abs(c), 1e-5);
	const float PI = 3.1415926535897932384626433832795;

	float4 weight = FIX(PI * float4(1.0 + x, x, 1.0 - x, 2.0 - x));
	float4 ret = sin(weight) * sin(weight / 2.0) / (weight * weight);

	return ret / dot(ret, float4(1.0, 1.0, 1.0, 1.0));
}

float3 LineRun(float ypos, float4 xpos, float4 linetaps)
{
	return mul(linetaps, float4x3(
		PixelPos(xpos.x, ypos),
		PixelPos(xpos.y, ypos),
		PixelPos(xpos.z, ypos),
		PixelPos(xpos.w, ypos)));
}

PS_OUTPUT ps_main11(PS_INPUT input)
{
	PS_OUTPUT output;

	float2 _xyFrame;
	Texture.GetDimensions(_xyFrame.x, _xyFrame.y);
	float2 stepxy = float2(1.0 / _xyFrame.x, 1.0 / _xyFrame.y);

	float2 pos = input.t + stepxy;
	float2 f = frac(pos / stepxy);

	float2 xystart = (-2.0 - f) * stepxy + pos;
	float4 xpos = float4(xystart.x,
	xystart.x + stepxy.x,
	xystart.x + stepxy.x * 2.0,
	xystart.x + stepxy.x * 3.0);

	float4 linetaps = WeightQuad(f.x);
	float4 columntaps = WeightQuad(f.y);

	// final sum and weight normalization
	output.c = float4(mul(columntaps, float4x3(
	LineRun(xystart.y, xpos, linetaps),
	LineRun(xystart.y + stepxy.y, xpos, linetaps),
	LineRun(xystart.y + stepxy.y * 2.0, xpos, linetaps),
	LineRun(xystart.y + stepxy.y * 3.0, xpos, linetaps))), 1.0);

	return output;
}

#elif SHADER_MODEL <= 0x300

PS_OUTPUT ps_main1(PS_INPUT input)
{
	PS_OUTPUT output;
	
	float4 c = sample_c(input.t);
	
	c.a *= 128.0f / 255; // *= 0.5f is no good here, need to do this in order to get 0x80 for 1.0f (instead of 0x7f)
	
	output.c = c;

	return output;
}

PS_OUTPUT ps_main2(PS_INPUT input)
{
	PS_OUTPUT output;
	
	clip(sample_c(input.t).a - 255.0f / 255); // >= 0x80 pass
	
	output.c = 0;

	return output;
}

PS_OUTPUT ps_main3(PS_INPUT input)
{
	PS_OUTPUT output;
	
	clip(254.95f / 255 - sample_c(input.t).a); // < 0x80 pass (== 0x80 should not pass)
	
	output.c = 0;

	return output;
}

PS_OUTPUT ps_main4(PS_INPUT input)
{
	PS_OUTPUT output;
	
	output.c = 1;
	
	return output;
}

PS_OUTPUT ps_main5(PS_INPUT input) // scanlines
{
	PS_OUTPUT output;
	
	int4 p = (int4)input.p;

	output.c = ps_scanlines(input, p.y % 2);

	return output;
}

PS_OUTPUT ps_main6(PS_INPUT input) // diagonal
{
	PS_OUTPUT output;

	int4 p = (int4)input.p;

	output.c = ps_crt(input, (p.x + (p.y % 3)) % 3);

	return output;
}

PS_OUTPUT ps_main8(PS_INPUT input) // triangular
{
	PS_OUTPUT output;

	int4 p = (int4)input.p;

	// output.c = ps_crt(input, ((p.x + (p.y % 2) * 3) / 2) % 3);
	output.c = ps_crt(input, ((p.x + ((p.y / 2) % 2) * 3) / 2) % 3);

	return output;
}

static const float PI = 3.14159265359f;
PS_OUTPUT ps_main9(PS_INPUT input) // triangular
{
	PS_OUTPUT output;

	// Needs DX9 conversion
	/*float2 texdim, halfpixel; 
	Texture.GetDimensions(texdim.x, texdim.y); 
	if (ddy(input.t.y) * texdim.y > 0.5) 
		output.c = sample_c(input.t); 
	else
		output.c = (0.5 - 0.5 * cos(2 * PI * input.t.y * texdim.y)) * sample_c(float2(input.t.x, (floor(input.t.y * texdim.y) + 0.5) / texdim.y));
*/

	// replacement shader
	int4 p = (int4)input.p;
	output.c = ps_crt(input, ((p.x + ((p.y / 2) % 2) * 3) / 2) % 3);

	return output;
}

// Dummy SM 3.0 Scaling
PS_OUTPUT ps_main10(PS_INPUT input)
{
	PS_OUTPUT output;

	output.c = sample_c(input.t);

	return output;
}

// Dummy SM 3.0 Scaling
PS_OUTPUT ps_main11(PS_INPUT input)
{
	PS_OUTPUT output;

	output.c = sample_c(input.t);

	return output;
}


#endif
#endif
