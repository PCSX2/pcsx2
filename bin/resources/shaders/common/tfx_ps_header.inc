#ifndef __METAL_VERSION__

// Utils
STATIC void discard_all();
STATIC void discard_color(IN_OUT_PARAM(FLOAT4, color));
STATIC void discard_depth(IN_OUT_PARAM(float, depth));
STATIC FLOAT4 sample_tex_lod(FLOAT2 uv, float lod);
STATIC FLOAT4 sample_tex(FLOAT2 uv);
STATIC FLOAT4 read_tex(UINT2 pos, uint lod);
STATIC FLOAT4 read_tex(UINT2 pos);
STATIC UINT2 get_tex_dims();

// Sample anisotropic filtering
STATIC FLOAT4 sample_c_af(FLOAT2 uv, float uv_w);

// Fetch color/depth/single channels
STATIC uint fetch_raw_depth();
STATIC FLOAT4 fetch_raw_color();
STATIC FLOAT4 fetch_c(USHORT2 uv);
STATIC FLOAT4 fetch_red();
STATIC FLOAT4 fetch_green();
STATIC FLOAT4 fetch_blue();
STATIC FLOAT4 fetch_alpha();
STATIC FLOAT4 fetch_rgb();
STATIC FLOAT4 fetch_gXbY();

// Sampling
STATIC float manual_lod(float uv_w);
STATIC FLOAT4 sample_c(FLOAT2 uv);
STATIC FLOAT4 sample_p(uint idx);
STATIC FLOAT4 sample_p_norm(float u);
STATIC FLOAT4 clamp_wrap_uv(FLOAT4 uv);
STATIC FLOAT4x4 sample_4c(FLOAT4 uv);
STATIC UINT4 sample_4_index(FLOAT4 uv);
STATIC FLOAT4x4 sample_4p(UINT4 u);
STATIC FLOAT4 sample_color(FLOAT2 st);
STATIC USHORT2 clamp_wrap_uv_depth(USHORT2 uv);
STATIC FLOAT4 sample_depth(FLOAT2 st);

// Texture function
STATIC FLOAT4 tfx(FLOAT4 T, FLOAT4 C);

// Alpha test
STATIC bool atst(FLOAT4 C);

// Fog
STATIC void fog(IN_OUT_PARAM(FLOAT4, C), float f);

// Color
STATIC FLOAT4 ps_color();

// Postprocessing
STATIC void ps_fbmask(IN_OUT_PARAM(FLOAT4, C));
STATIC void ps_dither(IN_OUT_PARAM(FLOAT4, C), float As);
STATIC void ps_color_clamp_wrap(IN_OUT_PARAM(FLOAT4, C));
STATIC void ps_alpha_correct(IN_OUT_PARAM(FLOAT4, C));

// Blend
STATIC void ps_blend(IN_OUT_PARAM(FLOAT4, Color), IN_OUT_PARAM(FLOAT4, As_rgba));

// Main
STATIC void ps_shuffle(IN_OUT_PARAM(float4, C));
STATIC PSOutputGeneric ps_main_impl();

#endif // __METAL_VERSION__
