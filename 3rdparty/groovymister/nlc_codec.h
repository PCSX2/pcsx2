// nlc_codec.h: near-lossless codec for Groovy_MiSTer (reference model)
//
// This is the single source of truth for the new deterministic near-lossless
// video codec that runs alongside LZ4. The same arithmetic is used by:
//   * the host encoder (api/groovymister.cpp),
//   * the offline benchmark and reference model (tools/),
//   * the FPGA decoder (rtl/nlc_*.v), which must match it bit for bit.
//
// Pipeline (encode):  RGB -> YCoCg-R (reversible) -> per-plane MED predict ->
//                     NEAR quantize (closed-loop) -> pack residuals.
// Pipeline (decode):  unpack -> dequantize+reconstruct (MED) -> inverse YCoCg-R.
//
// WIRE FORMAT v2 (2026-07-02, for the PARALLEL-plane HW decoder): each scanline is
//   [8-byte header: u16 lenP0..lenP3 (padded byte lengths)]  P0-segment  P1-segment ...
// where each plane's line data is packed into its OWN 64-bit-WORD-ALIGNED segment and the
// little-endian u16 lengths give each padded segment's byte size, so np hardware bit-readers
// can start at their own offsets simultaneously (one line = one self-describing record).
//
// Front-ends (the only stage that differs):
//   NLC_PACK_GLOBAL  Stage 1a: one bit-width per plane (simplest; bring-up).
//   NLC_PACK_TILED   Stage 1b: block-adaptive per-tile bit-width (the codec).
//   NLC_PACK_RICE    Stage 2 : Golomb-Rice entropy (best ratio).
//
// Determinism: GLOBAL/TILED contain no per-symbol variable-length codes and no
// back-references, so a hardware decoder cannot wedge (no watchdog needed).
//
// Bit order spec (must be mirrored in RTL): bits are packed LSB-first into a
// byte stream; multi-bit fields are written low bit first. A 64-bit DDR word
// consumed by the FPGA is the little-endian view of 8 consecutive bytes.

#ifndef NLC_CODEC_H
#define NLC_CODEC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NLC_PACK_GLOBAL = 0,
    NLC_PACK_TILED  = 1,
    NLC_PACK_RICE   = 2,
} nlc_pack_t;

// Only NLC_RGB888 is usable over the Groovy wire. The FPGA decoder
// (rtl/nlc_decode_ddr.v) has three plane cores and packs 3 bytes per pixel, with no
// pixel-format input, and GroovyMister::CmdInit rejects the other two modes. They remain
// here because nlc_encode/nlc_decode are a self-consistent pair used for offline codec
// work (tools/nlc_bench).
typedef enum {
    NLC_RGB888 = 0,  // 3 bytes/pixel, R,G,B. The only mode the FPGA decoder can consume.
    NLC_RGBA   = 1,  // 4 bytes/pixel, R,G,B,A (A coded as a 4th plane). Software round-trip
                     // only: the FPGA reads three segments per line and never consumes the
                     // alpha one, so a wire session loses framing on line 1. sim/loopback
                     // decodes in software and will not reproduce that.
    NLC_RGB565 = 2,  // 2 bytes/pixel. Rejected by both nlc_encode and nlc_decode; there is
                     // no internal expansion to 888. Use NLC_RGB888 with near_lvl instead.
} nlc_rgb_t;

typedef enum {
    // RGB-direct: predict+quantise each R,G,B channel independently. NEAR=k then
    // guarantees |error| <= k on every channel exactly. No inter-channel decorrelation.
    NLC_COLOR_RGB   = 0,
    // YCoCg-R reversible transform first. Better ratio on photographic content, but
    // for NEAR=k the per-channel RGB error is bounded yet larger than k (the inverse
    // lifting mixes planes). At NEAR=0 both colour modes are bit-exact lossless.
    NLC_COLOR_YCOCG = 1,
} nlc_color_t;

typedef struct {
    int         width;       // pixels
    int         height;      // pixels (already field-halved by caller if interlaced)
    nlc_rgb_t   rgb;         // input pixel format
    nlc_color_t color;       // RGB-direct (exact +-k) or YCoCg-R (better ratio)
    int         near_lvl;    // NEAR quantization, 0 = lossless, k = +-k near-lossless (named to avoid MSVC `near` keyword)
    nlc_pack_t  pack;        // front-end
    int         tile;        // TILED: tile length in pixels along a scanline (e.g. 16/32)
    int         width_bits;  // TILED/GLOBAL: bits used to store a tile/plane width header (e.g. 4)
    int         rice_k;      // RICE: fixed k, or -1 for per-tile adaptive k
} nlc_params;

// Number of raw (uncompressed) bytes of one frame for these params.
size_t nlc_frame_bytes(const nlc_params* p);

// Safe upper bound on encoded size (for sizing dst). Always >= any real output.
size_t nlc_max_encoded_size(const nlc_params* p);

// Encode one interleaved RGB[A] frame `src` into `dst` (capacity dst_cap).
// Returns the encoded size in bytes, or -1 on error (bad params / dst too small).
int nlc_encode(const uint8_t* src, uint8_t* dst, size_t dst_cap, const nlc_params* p);

// Decode `csize` bytes at `in` back into interleaved RGB[A] `out`.
// Returns 0 on success, -1 on error. For NEAR=0 the result is bit-exact; for
// NEAR=k every channel is within +-k of the original.
int nlc_decode(const uint8_t* in, size_t csize, uint8_t* out, const nlc_params* p);

// --- Exposed primitives (for unit tests + RTL parity checks) -----------------

// Reversible YCoCg-R lifting on one pixel. Y in [0,255], Co/Cg in [-255,255].
void nlc_rgb_to_ycocg(int R, int G, int B, int* Y, int* Co, int* Cg);
void nlc_ycocg_to_rgb(int Y, int Co, int Cg, int* R, int* G, int* B);

// LOCO-I / JPEG-LS median edge predictor from neighbours a=left, b=above,
// c=above-left.
int  nlc_med_predict(int a, int b, int c);

#ifdef __cplusplus
}
#endif

#endif // NLC_CODEC_H
