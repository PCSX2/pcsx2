#ifdef SHADER_MODEL // make safe to include in resource file to enforce dependency

#ifndef PS_SCALE_FACTOR
#define PS_SCALE_FACTOR 1
#endif

struct VS_INPUT
{
	float4 p : POSITION;
	float2 t : TEXCOORD0;
	float4 c : COLOR;
};

struct VS_OUTPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
	float4 c : COLOR;
};

Texture2D Texture;
SamplerState TextureSampler;

float4 sample_c(float2 uv)
{
	return Texture.Sample(TextureSampler, uv);
}

struct PS_INPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
	float4 c : COLOR;
};

struct PS_OUTPUT
{
	float4 c : SV_Target0;
};

VS_OUTPUT vs_main(VS_INPUT input)
{
	VS_OUTPUT output;

	output.p = input.p;
	output.t = input.t;
	output.c = input.c;

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

	float4 c = round(sample_c(input.t) * 255);
	// We use 2 fmod to avoid negative value.
	float4 fmod1 = fmod(c, 256) + 256;
	float4 fmod2 = fmod(fmod1, 256);

	output.c = fmod2 / 255.0f;

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

uint ps_main10(PS_INPUT input) : SV_Target0
{
	// Convert a FLOAT32 depth texture into a 32 bits UINT texture
	return uint(exp2(32.0f) * sample_c(input.t).r);
}

PS_OUTPUT ps_main11(PS_INPUT input)
{
	PS_OUTPUT output;

	// Convert a FLOAT32 depth texture into a RGBA color texture
	const float4 bitSh = float4(exp2(24.0f), exp2(16.0f), exp2(8.0f), exp2(0.0f));
	const float4 bitMsk = float4(0.0, 1.0 / 256.0, 1.0 / 256.0, 1.0 / 256.0);

	float4 res = frac(float4(sample_c(input.t).rrrr) * bitSh);

	output.c = (res - res.xxyz * bitMsk) * 256.0f / 255.0f;

	return output;
}

PS_OUTPUT ps_main12(PS_INPUT input)
{
	PS_OUTPUT output;

	// Convert a FLOAT32 (only 16 lsb) depth into a RGB5A1 color texture
	const float4 bitSh = float4(exp2(32.0f), exp2(27.0f), exp2(22.0f), exp2(17.0f));
	const uint4 bitMsk = uint4(0x1F, 0x1F, 0x1F, 0x1);
	uint4 color = uint4(float4(sample_c(input.t).rrrr) * bitSh) & bitMsk;

	output.c = float4(color) / float4(32.0f, 32.0f, 32.0f, 1.0f);

	return output;
}
float ps_main13(PS_INPUT input) : SV_Depth
{
	// Convert a RRGBA texture into a float depth texture
	// FIXME: I'm afraid of the accuracy
	const float4 bitSh = float4(exp2(-32.0f), exp2(-24.0f), exp2(-16.0f), exp2(-8.0f)) * (float4)255.0;

	return dot(sample_c(input.t), bitSh);
}

float ps_main14(PS_INPUT input) : SV_Depth
{
	// Same as above but without the alpha channel (24 bits Z)

	// Convert a RRGBA texture into a float depth texture
	const float3 bitSh = float3(exp2(-32.0f), exp2(-24.0f), exp2(-16.0f)) * (float3)255.0;

	return dot(sample_c(input.t).rgb, bitSh);
}

float ps_main15(PS_INPUT input) : SV_Depth
{
	// Same as above but without the A/B channels (16 bits Z)

	// Convert a RRGBA texture into a float depth texture
	// FIXME: I'm afraid of the accuracy
	const float2 bitSh = float2(exp2(-32.0f), exp2(-24.0f)) * (float2)255.0;

	return dot(sample_c(input.t).rg, bitSh);
}

float ps_main16(PS_INPUT input) : SV_Depth
{
	// Convert a RGB5A1 (saved as RGBA8) color to a 16 bit Z
	// FIXME: I'm afraid of the accuracy
	const float4 bitSh = float4(exp2(-32.0f), exp2(-27.0f), exp2(-22.0f), exp2(-17.0f));
	// Trunc color to drop useless lsb
	float4 color = trunc(sample_c(input.t) * (float4)255.0 / float4(8.0f, 8.0f, 8.0f, 128.0f));

	return dot(float4(color), bitSh);
}

PS_OUTPUT ps_main17(PS_INPUT input)
{
	PS_OUTPUT output;

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

	uint2 sel = uint2(input.p.xy) % uint2(16u, 16u);
	int2  tb  = ((int2(input.p.xy) & ~int2(15, 3)) >> 1);

	int ty   = tb.y | (int(input.p.y) & 1);
	int txN  = tb.x | (int(input.p.x) & 7);
	int txH  = tb.x | ((int(input.p.x) + 4) & 7);

	txN *= PS_SCALE_FACTOR;
	txH *= PS_SCALE_FACTOR;
	ty  *= PS_SCALE_FACTOR;

	// TODO investigate texture gather
	float4 cN = Texture.Load(int3(txN, ty, 0));
	float4 cH = Texture.Load(int3(txH, ty, 0));


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

	output.c = (float4)(c); // Divide by something here?

	return output;
}

// DUMMY
PS_OUTPUT ps_main18(PS_INPUT input)
{
	PS_OUTPUT output;

	output.c = input.p;

	return output;
}

PS_OUTPUT ps_main19(PS_INPUT input)
{
	PS_OUTPUT output;

	output.c = input.c * float4(1.0, 1.0, 1.0, sample_c(input.t).r);

	return output;
}

#endif
