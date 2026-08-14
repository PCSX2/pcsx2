// Utils
STATIC void discard_all(IN_OUT_PARAM(PSMainState, state));
STATIC void discard_color(IN_OUT_PARAM(PSMainState, state), IN_OUT_PARAM(FLOAT4, color));
STATIC void discard_depth(IN_OUT_PARAM(PSMainState, state), IN_OUT_PARAM(float, depth));
STATIC FLOAT4 sample_tex_lod(IN_PARAM(PSMainState, state), FLOAT2 uv, float lod);
STATIC FLOAT4 sample_tex(IN_PARAM(PSMainState, state), FLOAT2 uv);
STATIC FLOAT4 read_tex(IN_PARAM(PSMainState, state), UINT2 pos, uint lod);
STATIC FLOAT4 read_tex(IN_PARAM(PSMainState, state), UINT2 pos);
STATIC UINT2 get_tex_dims(IN_PARAM(PSMainState, state));

// Sample anisotropic filtering
STATIC FLOAT4 sample_c_af(IN_PARAM(PSMainState, state), FLOAT2 uv, float uv_w);

// Fetch color/depth/single channels
STATIC uint fetch_raw_depth(IN_PARAM(PSMainState, state));
STATIC FLOAT4 fetch_raw_color(IN_PARAM(PSMainState, state));
STATIC FLOAT4 fetch_c(IN_PARAM(PSMainState, state), USHORT2 uv);
STATIC FLOAT4 fetch_red(IN_PARAM(PSMainState, state));
STATIC FLOAT4 fetch_green(IN_PARAM(PSMainState, state));
STATIC FLOAT4 fetch_blue(IN_PARAM(PSMainState, state));
STATIC FLOAT4 fetch_alpha(IN_PARAM(PSMainState, state));
STATIC FLOAT4 fetch_rgb(IN_PARAM(PSMainState, state));
STATIC FLOAT4 fetch_gXbY(IN_PARAM(PSMainState, state));

// Sampling
STATIC float manual_lod(IN_PARAM(PSMainState, state), float uv_w);
STATIC FLOAT4 sample_c(IN_PARAM(PSMainState, state), FLOAT2 uv);
STATIC FLOAT4 sample_p(IN_PARAM(PSMainState, state), uint idx);
STATIC FLOAT4 sample_p_norm(IN_PARAM(PSMainState, state), float u);
STATIC FLOAT4 clamp_wrap_uv(IN_PARAM(PSMainState, state), FLOAT4 uv);
STATIC FLOAT4x4 sample_4c(IN_PARAM(PSMainState, state), FLOAT4 uv);
STATIC UINT4 sample_4_index(IN_PARAM(PSMainState, state), FLOAT4 uv);
STATIC FLOAT4x4 sample_4p(IN_PARAM(PSMainState, state), UINT4 u);
STATIC FLOAT4 sample_color(IN_PARAM(PSMainState, state), FLOAT2 st);
STATIC USHORT2 clamp_wrap_uv_depth(IN_PARAM(PSMainState, state), USHORT2 uv);
STATIC FLOAT4 sample_depth(IN_PARAM(PSMainState, state), FLOAT2 st);

// Texture function
STATIC FLOAT4 tfx(IN_PARAM(PSMainState, state), FLOAT4 T, FLOAT4 C);

// Alpha test
STATIC bool atst(IN_PARAM(PSMainState, state), FLOAT4 C);

// Fog
STATIC void fog(IN_PARAM(PSMainState, state), IN_OUT_PARAM(FLOAT4, C), float f);

// Color
STATIC FLOAT4 ps_color(IN_PARAM(PSMainState, state));

// Postprocessing
STATIC void ps_fbmask(IN_PARAM(PSMainState, state), IN_OUT_PARAM(FLOAT4, C));
STATIC void ps_dither(IN_PARAM(PSMainState, state), IN_OUT_PARAM(FLOAT4, C), float As);
STATIC void ps_color_clamp_wrap(IN_PARAM(PSMainState, state), IN_OUT_PARAM(FLOAT4, C));
STATIC void ps_alpha_correct(IN_PARAM(PSMainState, state), IN_OUT_PARAM(FLOAT4, C));

// Blend
STATIC void ps_blend(IN_OUT_PARAM(PSMainState, state), IN_OUT_PARAM(FLOAT4, Color), IN_OUT_PARAM(FLOAT4, As_rgba));

// Main
STATIC void ps_shuffle(IN_PARAM(PSMainState, state), IN_OUT_PARAM(FLOAT4, C));
STATIC PSOutputGeneric ps_main_impl(IN_OUT_PARAM(PSMainState, state));
