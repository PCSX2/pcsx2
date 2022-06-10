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

layout(push_constant) uniform cb10
{
	vec4 u_source_rect;
	vec4 u_target_rect;
	vec2 u_source_size;
	vec2 u_target_size;
	vec2 u_target_resolution;
	vec2 u_rcp_target_resolution; // 1 / u_target_resolution
	vec2 u_source_resolution;
	vec2 u_rcp_source_resolution; // 1 / u_source_resolution
	float u_time;
	vec3 cb0_pad0;
};

layout(location = 0) in vec2 v_tex;

layout(location = 0) out vec4 o_col0;

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
	
	o_col0 = (0.9 - 0.4 * cos(2 * PI * v_tex.y * texdim.y)) * sample_c(vec2(v_tex.x, (floor(v_tex.y * texdim.y) + 0.5) / texdim.y));
}
#endif

#endif