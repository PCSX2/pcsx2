#ifndef __METAL_VERSION__

// Utils
static void discard_all();
static void discard_color(IN_OUT_PARAM(float4, color));
static void discard_depth(IN_OUT_PARAM(float, depth));
static float4 sample_tex_lod(float2 uv, float lod);
static float4 sample_tex(float2 uv);
static float4 read_tex(uint2 pos, uint lod);
static float4 read_tex(uint2 pos);
static uint2 get_tex_dims();

// Sample anisotropic filtering
static float4 sample_c_af(float2 uv, float uv_w);

// Fetch color/depth/single channels
static uint fetch_raw_depth();
static float4 fetch_raw_color();
static float4 fetch_c(ushort2 uv);
static float4 fetch_red();
static float4 fetch_green();
static float4 fetch_blue();
static float4 fetch_alpha();
static float4 fetch_rgb();
static float4 fetch_gXbY();

// Sampling
static float manual_lod(float uv_w);
static float4 sample_c(float2 uv);
static float4 sample_p(uint idx);
static float4 sample_p_norm(float u);
static float4 clamp_wrap_uv(float4 uv);
static float4x4 sample_4c(float4 uv);
static uint4 sample_4_index(float4 uv);
static float4x4 sample_4p(uint4 u);
static float4 sample_color(float2 st);
static ushort2 clamp_wrap_uv_depth(ushort2 uv);
static float4 sample_depth(float2 st);

// Texture function
static float4 tfx(float4 T, float4 C);

// Alpha test
static bool atst(float4 C);

// Fog
static void fog(IN_OUT_PARAM(float4, C), float f);

// Color
static float4 ps_color();

// Postprocessing
static void ps_fbmask(IN_OUT_PARAM(float4, C));
static void ps_dither(IN_OUT_PARAM(float4, C), float As);
static void ps_color_clamp_wrap(IN_OUT_PARAM(float4, C));
static void ps_alpha_correct(IN_OUT_PARAM(float4, C));

// Blend
static void ps_blend(IN_OUT_PARAM(float4, Color), IN_OUT_PARAM(float4, As_rgba));

// Main
static void ps_shuffle(IN_OUT_PARAM(float4, C));
static PSOutputGeneric ps_main_impl();

#endif // __METAL_VERSION__
