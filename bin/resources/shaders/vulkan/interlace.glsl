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
layout(location = 0) out vec4 o_col0;

layout(push_constant) uniform cb0
{
	vec2 ZrH;
};

layout(set = 0, binding = 0) uniform sampler2D samp0;

#ifdef ps_main0
void ps_main0()
{
	o_col0 = texture(samp0, v_tex);
	if ((int(gl_FragCoord.y) & 1) == 0)
		discard;
}
#endif

#ifdef ps_main1
void ps_main1()
{
	o_col0 = texture(samp0, v_tex);
	if ((int(gl_FragCoord.y) & 1) != 0)
		discard;
}
#endif

#ifdef ps_main2
void ps_main2()
{
	vec4 c0 = texture(samp0, v_tex - ZrH);
	vec4 c1 = texture(samp0, v_tex);
	vec4 c2 = texture(samp0, v_tex + ZrH);

	o_col0 = (c0 + c1 * 2.0f + c2) / 4.0f;
}
#endif

#ifdef ps_main3
void ps_main3()
{
	o_col0 = texture(samp0, v_tex);
}
#endif

#endif
