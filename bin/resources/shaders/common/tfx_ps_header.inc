#ifndef __METAL_VERSION__

// Utils
PS_STATIC void discard_all();
PS_STATIC void discard_color(IN_OUT_PARAM(float4, color));
PS_STATIC void discard_depth(IN_OUT_PARAM(float, depth));
PS_STATIC float4 sample_tex_lod(float2 uv, float lod);
PS_STATIC float4 sample_tex(float2 uv);
PS_STATIC float4 read_tex(uint2 pos, uint lod);
PS_STATIC float4 read_tex(uint2 pos);
PS_STATIC uint2 get_tex_dims();

// Sample anisotropic filtering
PS_STATIC float4 sample_c_af(float2 uv, float uv_w);

// Fetch color/depth/single channels
PS_STATIC uint fetch_raw_depth();
PS_STATIC float4 fetch_raw_color();
PS_STATIC float4 fetch_c(ushort2 uv);
PS_STATIC float4 fetch_red();
PS_STATIC float4 fetch_green();
PS_STATIC float4 fetch_blue();
PS_STATIC float4 fetch_alpha();
PS_STATIC float4 fetch_rgb();
PS_STATIC float4 fetch_gXbY();

// Sampling
PS_STATIC float manual_lod(float uv_w);
PS_STATIC float4 sample_c(float2 uv);
PS_STATIC float4 sample_p(uint idx);
PS_STATIC float4 sample_p_norm(float u);
PS_STATIC float4 clamp_wrap_uv(float4 uv);
PS_STATIC float4x4 sample_4c(float4 uv);
PS_STATIC uint4 sample_4_index(float4 uv);
PS_STATIC float4x4 sample_4p(uint4 u);
PS_STATIC float4 sample_color(float2 st);
PS_STATIC ushort2 clamp_wrap_uv_depth(ushort2 uv);
PS_STATIC float4 sample_depth(float2 st);

// Texture function
PS_STATIC float4 tfx(float4 T, float4 C);

// Alpha test
PS_STATIC bool atst(float4 C);

// Fog
PS_STATIC void fog(IN_OUT_PARAM(float4, C), float f);

// Color
PS_STATIC float4 ps_color();

// Postprocessing
PS_STATIC void ps_fbmask(IN_OUT_PARAM(float4, C));
PS_STATIC void ps_dither(IN_OUT_PARAM(float4, C), float As);
PS_STATIC void ps_color_clamp_wrap(IN_OUT_PARAM(float4, C));
PS_STATIC void ps_alpha_correct(IN_OUT_PARAM(float4, C));

// Blend
PS_STATIC void ps_blend(IN_OUT_PARAM(float4, Color), IN_OUT_PARAM(float4, As_rgba));

// Main
PS_STATIC void ps_shuffle(IN_OUT_PARAM(float4, C));
PS_STATIC PSOutputGeneric ps_main_impl();

#endif // __METAL_VERSION__
