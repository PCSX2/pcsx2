// nlc_codec.cpp: near-lossless codec for Groovy_MiSTer (reference model)
// See nlc_codec.h for the pipeline and bit-order spec.
//
// This file is deliberately written as a clear, dependency-free reference so it
// can double as the RTL specification. Performance is secondary to clarity and
// to bit-exact reproducibility between host (C++) and FPGA (Verilog).

#include "nlc_codec.h"
#include <stdlib.h>
#include <string.h>
#include <thread>    // plane-parallel encode (host side; the HPS never calls nlc_encode)

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

static inline int iclamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// zigzag map signed<->unsigned (0,-1,1,-2,2,... -> 0,1,2,3,4,...)
static inline uint32_t zz(int v)   { return v >= 0 ? (uint32_t)(v << 1) : (uint32_t)(((-v) << 1) - 1); }
static inline int      unzz(uint32_t u) { return (u & 1) ? -(int)((u + 1) >> 1) : (int)(u >> 1); }

// number of bits to represent u (bitlen(0)=0, bitlen(1)=1, bitlen(2..3)=2, ...)
static inline int bitlen(uint32_t u) { int b = 0; while (u) { b++; u >>= 1; } return b; }

// JPEG-LS near-lossless residual quantiser / dequantiser
static inline int nl_quant(int e, int near_lvl) {
    // Constant-divisor branches. A runtime divisor here costs over 1M integer divides per
    // frame on the encode hot path, about 3 ms/frame whenever near > 0, whereas constants let
    // the compiler emit multiply-by-reciprocal. The rounding is unchanged and the results are
    // bit-identical.
    int a, q;
    switch (near_lvl) {
        case 0:  return e;
        case 1:  a = (e > 0) ? e : -e; q = (a + 1) / 3; break;
        case 2:  a = (e > 0) ? e : -e; q = (a + 2) / 5; break;
        case 3:  a = (e > 0) ? e : -e; q = (a + 3) / 7; break;
        default: a = (e > 0) ? e : -e; q = (a + near_lvl) / (2 * near_lvl + 1); break;
    }
    return (e > 0) ? q : -q;
}
static inline int nl_dequant(int qe, int near_lvl) { return qe * (2 * near_lvl + 1); }

// ---------------------------------------------------------------------------
// Reversible YCoCg-R  (t == floor((R+B)/2), so Y in [0,255], Co/Cg in [-255,255])
// ---------------------------------------------------------------------------

void nlc_rgb_to_ycocg(int R, int G, int B, int* Y, int* Co, int* Cg) {
    int co = R - B;
    int t  = B + (co >> 1);   // arithmetic (floor) shift, consistent both ways
    int cg = G - t;
    int y  = t + (cg >> 1);
    *Y = y; *Co = co; *Cg = cg;
}

void nlc_ycocg_to_rgb(int Y, int Co, int Cg, int* R, int* G, int* B) {
    int t = Y - (Cg >> 1);
    int g = Cg + t;
    int b = t - (Co >> 1);
    int r = b + Co;
    *R = r; *G = g; *B = b;
}

int nlc_med_predict(int a, int b, int c) {
    int mn = a < b ? a : b;
    int mx = a < b ? b : a;
    if (c >= mx) return mn;
    if (c <= mn) return mx;
    return a + b - c;
}

// ---------------------------------------------------------------------------
// LSB-first bit I/O (the wire/RTL bit-order spec)
// ---------------------------------------------------------------------------

struct BitW {
    uint8_t* buf; size_t cap; size_t n; uint64_t acc; int nbits; bool ovf;
    void init(uint8_t* b, size_t c) { buf = b; cap = c; n = 0; acc = 0; nbits = 0; ovf = false; }
    void put(uint32_t val, int bits) {            // bits in [0,24]
        if (bits <= 0) return;
        uint32_t mask = (bits < 32) ? ((1u << bits) - 1u) : 0xffffffffu;
        acc |= (uint64_t)(val & mask) << nbits;
        nbits += bits;
        while (nbits >= 8) { if (n < cap) buf[n] = (uint8_t)(acc & 0xff); else ovf = true; n++; acc >>= 8; nbits -= 8; }
    }
    size_t finish() { if (nbits > 0) { if (n < cap) buf[n] = (uint8_t)(acc & 0xff); else ovf = true; n++; acc = 0; nbits = 0; } return n; }
};

struct BitR {
    const uint8_t* buf; size_t cap; size_t n; uint64_t acc; int nbits;
    void init(const uint8_t* b, size_t c) { buf = b; cap = c; n = 0; acc = 0; nbits = 0; }
    uint32_t get(int bits) {                      // bits in [0,24]
        if (bits <= 0) return 0;
        while (nbits < bits) { uint64_t byte = (n < cap) ? buf[n] : 0; n++; acc |= byte << nbits; nbits += 8; }
        uint32_t mask = (bits < 32) ? ((1u << bits) - 1u) : 0xffffffffu;
        uint32_t v = (uint32_t)(acc & mask);
        acc >>= bits; nbits -= bits;
        return v;
    }
};

// ---------------------------------------------------------------------------
// Per-LINE closed-loop DPCM (MED predict + NEAR quantise, reconstruction-exact).
//
// The codec is LINE-INTERLEAVED: the bitstream stores, per scanline, the tiles
// of plane 0, then 1, then 2 (then alpha). A hardware decoder can therefore
// reconstruct one RGB line at a time from just per-plane line buffers, with no
// whole-plane buffering, so these operate on a single row.
//
// neighbour rule (must match RTL): `prev` = previous reconstructed row of this
// plane, `cur` = the row being reconstructed.
//   Ra(left)      = x>0      ? cur[x-1] : (y>0 ? prev[x]   : 0)
//   Rb(above)     = y>0      ? prev[x]  : Ra
//   Rc(aboveleft) = (x&&y)   ? prev[x-1]: Rb
// ---------------------------------------------------------------------------

static void line_encode(const int16_t* t, const int16_t* prev, int16_t* cur,
                        int W, int y, int near_lvl, int lo, int hi, uint32_t* u) {
    for (int x = 0; x < W; x++) {
        int Ra = x > 0 ? cur[x - 1] : (y > 0 ? prev[x] : 0);
        int Rb = y > 0 ? prev[x] : Ra;
        int Rc = (x > 0 && y > 0) ? prev[x - 1] : Rb;
        int pred = nlc_med_predict(Ra, Rb, Rc);
        int e  = t[x] - pred;
        int qe = nl_quant(e, near_lvl);
        int rv = iclamp(pred + nl_dequant(qe, near_lvl), lo, hi);
        cur[x] = (int16_t)rv;
        u[x] = zz(qe);
    }
}

static void line_decode(const uint32_t* u, const int16_t* prev, int16_t* cur,
                        int W, int y, int near_lvl, int lo, int hi) {
    for (int x = 0; x < W; x++) {
        int Ra = x > 0 ? cur[x - 1] : (y > 0 ? prev[x] : 0);
        int Rb = y > 0 ? prev[x] : Ra;
        int Rc = (x > 0 && y > 0) ? prev[x - 1] : Rb;
        int pred = nlc_med_predict(Ra, Rb, Rc);
        int qe = unzz(u[x]);
        int rv = iclamp(pred + nl_dequant(qe, near_lvl), lo, hi);
        cur[x] = (int16_t)rv;
    }
}

// ---------------------------------------------------------------------------
// Front-end packers, one SCANLINE of residuals at a time (W entries in u).
// GLOBAL = one width for the line; TILED = per-1D-tile width.
// ---------------------------------------------------------------------------

static void pack_line_global(BitW& bw, const uint32_t* u, int W, int wbits) {
    int w = 0; for (int i = 0; i < W; i++) { int b = bitlen(u[i]); if (b > w) w = b; }
    bw.put((uint32_t)w, wbits);
    for (int i = 0; i < W; i++) bw.put(u[i], w);
}
static void unpack_line_global(BitR& br, uint32_t* u, int W, int wbits) {
    int w = (int)br.get(wbits);
    for (int i = 0; i < W; i++) u[i] = br.get(w);
}

static void pack_line_tiled(BitW& bw, const uint32_t* u, int W, int tile, int wbits) {
    for (int x0 = 0; x0 < W; x0 += tile) {
        int T = (x0 + tile <= W) ? tile : (W - x0);
        int w = 0; for (int i = 0; i < T; i++) { int b = bitlen(u[x0 + i]); if (b > w) w = b; }
        bw.put((uint32_t)w, wbits);
        for (int i = 0; i < T; i++) bw.put(u[x0 + i], w);
    }
}
static void unpack_line_tiled(BitR& br, uint32_t* u, int W, int tile, int wbits) {
    for (int x0 = 0; x0 < W; x0 += tile) {
        int T = (x0 + tile <= W) ? tile : (W - x0);
        int w = (int)br.get(wbits);
        for (int i = 0; i < T; i++) u[x0 + i] = br.get(w);
    }
}

// ---------------------------------------------------------------------------
// Golomb-Rice front-end. Per 1-D tile: store the Rice parameter k (wbits-wide),
// then code each zigzag residual u as unary(q=u>>k) + k remainder bits, where
// the unary part is bounded by a LIMIT escape (LIMIT zeros + stop, then the raw
// value in NLC_RICE_UBITS bits). The escape keeps the worst-case code length
// finite so a hardware decoder needs only a small bit window.
// ---------------------------------------------------------------------------

#define NLC_RICE_LIMIT 20   // max unary run before escape
#define NLC_RICE_UBITS 12   // escape payload width (covers u up to 4095; max u ~1020)

static int rice_k_for_tile(const uint32_t* u, int off, int T) {
    // JPEG-LS style: smallest k with (T << k) >= sum(u)
    uint64_t sum = 0; for (int i = 0; i < T; i++) sum += u[off + i];
    int k = 0; while (((uint64_t)T << k) < sum && k < 15) k++;
    return k;
}
static void rice_put(BitW& bw, uint32_t u, int k) {
    // The unary run "q zeros then a stop-1" is the value (1<<q) written LSB-first in q+1
    // bits, so it takes one put() instead of up to 21. The per-bit loop cost about 3 ms per
    // frame on the host encode path. Output is bit-identical, and q+1 <= 20 non-escape or 21
    // escape, both within the put() limit of 24 bits.
    uint32_t q = u >> k;
    if (q < NLC_RICE_LIMIT) {
        bw.put(1u << q, (int)q + 1);
        if (k > 0) bw.put(u & ((1u << k) - 1u), k);
    } else {
        bw.put(1u << NLC_RICE_LIMIT, NLC_RICE_LIMIT + 1);
        bw.put(u, NLC_RICE_UBITS);
    }
}
static uint32_t rice_get(BitR& br, int k) {
    int zeros = 0;
    while (zeros < NLC_RICE_LIMIT && br.get(1) == 0) zeros++;
    if (zeros < NLC_RICE_LIMIT) {
        uint32_t r = k > 0 ? br.get(k) : 0;
        return ((uint32_t)zeros << k) | r;
    }
    br.get(1);                       // consume the escape stop bit
    return br.get(NLC_RICE_UBITS);
}

static void pack_line_rice(BitW& bw, const uint32_t* u, int W, int tile, int wbits, int fixed_k) {
    for (int x0 = 0; x0 < W; x0 += tile) {
        int T = (x0 + tile <= W) ? tile : (W - x0);
        int k = (fixed_k >= 0) ? fixed_k : rice_k_for_tile(u, x0, T);
        bw.put((uint32_t)k, wbits);
        for (int i = 0; i < T; i++) rice_put(bw, u[x0 + i], k);
    }
}
static void unpack_line_rice(BitR& br, uint32_t* u, int W, int tile, int wbits) {
    for (int x0 = 0; x0 < W; x0 += tile) {
        int T = (x0 + tile <= W) ? tile : (W - x0);
        int k = (int)br.get(wbits);
        for (int i = 0; i < T; i++) u[x0 + i] = rice_get(br, k);
    }
}

// ---------------------------------------------------------------------------
// Plane layout: number of planes and per-plane clamp ranges
// ---------------------------------------------------------------------------

static int plane_count(nlc_rgb_t rgb) { return rgb == NLC_RGBA ? 4 : 3; }
static void plane_range(nlc_color_t color, int p, int* lo, int* hi) {
    // RGB-direct: planes 0..2 = R,G,B in [0,255]; plane 3 = A.
    // YCoCg-R:    plane 0 = Y [0,255]; planes 1,2 = Co,Cg [-255,255]; plane 3 = A.
    if (color == NLC_COLOR_RGB || p == 0 || p == 3) { *lo = 0; *hi = 255; }
    else                                            { *lo = -255; *hi = 255; }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

size_t nlc_frame_bytes(const nlc_params* p) {
    int bpp = (p->rgb == NLC_RGBA) ? 4 : (p->rgb == NLC_RGB565 ? 2 : 3);
    return (size_t)p->width * p->height * bpp;
}

size_t nlc_max_encoded_size(const nlc_params* p) {
    // Generous bound: residuals are <=~11 bits, plus tile headers. 2x raw + slack.
    // v2 adds a word-aligned 8B line header + up to 7B pad per plane per line.
    return 2 * nlc_frame_bytes(p) + (size_t)p->width * p->height + (size_t)p->height * 40 + 1024;
}

// split interleaved RGB[A] frame into int16 planes
static void to_planes(const uint8_t* src, const nlc_params* p, int16_t** planes) {
    int N = p->width * p->height;
    int bpp = (p->rgb == NLC_RGBA) ? 4 : 3;
    for (int i = 0; i < N; i++) {
        int R = src[i * bpp + 0], G = src[i * bpp + 1], B = src[i * bpp + 2];
        if (p->color == NLC_COLOR_YCOCG) {
            int Y, Co, Cg; nlc_rgb_to_ycocg(R, G, B, &Y, &Co, &Cg);
            planes[0][i] = (int16_t)Y; planes[1][i] = (int16_t)Co; planes[2][i] = (int16_t)Cg;
        } else {
            planes[0][i] = (int16_t)R; planes[1][i] = (int16_t)G; planes[2][i] = (int16_t)B;
        }
        if (p->rgb == NLC_RGBA) planes[3][i] = (int16_t)src[i * bpp + 3];
    }
}

// inverse colour transform of one reconstructed scanline -> interleaved RGB[A]
static void combine_line(int16_t** cur, const nlc_params* p, uint8_t* outrow, int W) {
    int bpp = (p->rgb == NLC_RGBA) ? 4 : 3;
    for (int x = 0; x < W; x++) {
        int R, G, B;
        if (p->color == NLC_COLOR_YCOCG) nlc_ycocg_to_rgb(cur[0][x], cur[1][x], cur[2][x], &R, &G, &B);
        else { R = cur[0][x]; G = cur[1][x]; B = cur[2][x]; }
        outrow[x*bpp+0] = (uint8_t)iclamp(R, 0, 255);
        outrow[x*bpp+1] = (uint8_t)iclamp(G, 0, 255);
        outrow[x*bpp+2] = (uint8_t)iclamp(B, 0, 255);
        if (p->rgb == NLC_RGBA) outrow[x*bpp+3] = (uint8_t)iclamp(cur[3][x], 0, 255);
    }
}

int nlc_encode(const uint8_t* src, uint8_t* dst, size_t dst_cap, const nlc_params* p) {
    if (!src || !dst || !p) return -1;
    // RGB565 is not supported and is not planned. Adding it would mean a new packing mode in
    // the FPGA decoder, which has three plane cores and a fixed 3 bytes/pixel output with no
    // pixel-format input (rtl/nlc_decode_ddr.v), plus a new bitstream and hardware
    // validation. It would also not pay for itself: on the 720x480 capture corpus, rice/ycc
    // on RGB888 runs 276 Mbps lossless and 199 Mbps at near=1, against 332 Mbps for
    // uncompressed 565. near_lvl is the better control anyway, since it quantises the
    // prediction residual rather than putting a fixed 5/6/5 grid across smooth gradients.
    if (p->rgb == NLC_RGB565) return -1;
    int W = p->width, H = p->height, N = W * H, np = plane_count(p->rgb);
    int wbits = p->width_bits > 0 ? p->width_bits : 4;
    int tile  = p->tile > 0 ? p->tile : 16;

    int16_t* planes[4] = {0,0,0,0};
    for (int k = 0; k < np; k++) planes[k] = (int16_t*)malloc(sizeof(int16_t) * N);
    to_planes(src, p, planes);

    int16_t* prevL[4] = {0,0,0,0};
    int16_t* curL[4]  = {0,0,0,0};
    for (int k = 0; k < np; k++) { prevL[k] = (int16_t*)calloc(W, sizeof(int16_t)); curL[k] = (int16_t*)calloc(W, sizeof(int16_t)); }
    uint32_t* uline = (uint32_t*)malloc(sizeof(uint32_t) * W);

    // FORMAT v2 (per-plane offsets, for the PARALLEL-plane HW decoder): each LINE is one
    // 64-bit-WORD-ALIGNED record:
    //   [8-byte header: u16 lenP0, u16 lenP1, u16 lenP2, u16 reserved(np=4: lenP3)]
    //   P0-segment  P1-segment  ...        (each segment PADDED to a multiple of 8 bytes)
    // The little-endian u16 lengths are the PADDED byte size of each plane's independently-
    // packed segment. Word alignment everywhere means the HW line loader routes whole 64-bit
    // words to per-plane buffers (no byte steering), and np bit-readers start simultaneously.
    // Plane-parallel encode. The per-sample MED, quantize and pack work dominated the host
    // frame budget at 12-18 ms serial, which held the sender to about 41 fps. Planes are
    // fully independent, each with its own MED recurrence and its own per-line segment, so
    // each encodes all of its lines into a private buffer on its own thread and a serial pass
    // then assembles the v2 line records in the same order as before. The split leaves every
    // plane's segment bytes untouched, so the output is bit-identical to the serial encoder,
    // verified against the capture corpus.
    uint8_t*  segbuf[4]  = {0,0,0,0};
    uint16_t* seglen[4]  = {0,0,0,0};
    bool      povf[4]    = {false,false,false,false};
    for (int k = 0; k < np; k++) {
        segbuf[k] = (uint8_t*)malloc(dst_cap);
        seglen[k] = (uint16_t*)malloc(sizeof(uint16_t) * (size_t)H);
    }

    auto encode_plane = [&](int k) {
        int lo, hi; plane_range(p->color, k, &lo, &hi);
        uint32_t* ul   = (uint32_t*)malloc(sizeof(uint32_t) * W);
        int16_t*  prev = (int16_t*)calloc(W, sizeof(int16_t));
        int16_t*  cur  = (int16_t*)calloc(W, sizeof(int16_t));
        size_t off = 0;
        for (int y = 0; y < H; y++) {
            line_encode(planes[k] + (size_t)y * W, prev, cur, W, y, p->near_lvl, lo, hi, ul);
            BitW bw; bw.init(segbuf[k] + off, dst_cap - off);
            if      (p->pack == NLC_PACK_GLOBAL) pack_line_global(bw, ul, W, wbits);
            else if (p->pack == NLC_PACK_TILED)  pack_line_tiled(bw, ul, W, tile, wbits);
            else                                 pack_line_rice(bw, ul, W, tile, wbits, p->rice_k);
            size_t seg = bw.finish();
            size_t pad = (8 - (seg & 7)) & 7;    // word-align the segment
            if (bw.ovf || seg + pad > 0xffff || off + seg + pad > dst_cap) { povf[k] = true; break; }
            memset(segbuf[k] + off + seg, 0, pad);
            seg += pad;
            seglen[k][y] = (uint16_t)seg;
            off += seg;
            int16_t* t = prev; prev = cur; cur = t;
        }
        free(ul); free(prev); free(cur);
    };

    {
        std::thread th[4];
        for (int k = 1; k < np; k++) th[k] = std::thread(encode_plane, k);
        encode_plane(0);
        for (int k = 1; k < np; k++) th[k].join();
    }

    bool ovf = povf[0] || povf[1] || povf[2] || povf[3];
    size_t csize = 0;
    if (!ovf) {
        size_t soff[4] = {0,0,0,0};
        for (int y = 0; y < H && !ovf; y++) {
            size_t hdr = csize;                      // 8-byte header slot
            csize += 8;
            if (csize > dst_cap) { ovf = true; break; }
            memset(dst + hdr, 0, 8);
            for (int k = 0; k < np; k++) {
                size_t seg = seglen[k][y];
                if (csize + seg > dst_cap) { ovf = true; break; }
                memcpy(dst + csize, segbuf[k] + soff[k], seg);
                soff[k] += seg;
                dst[hdr + 2*k]     = (uint8_t)(seg & 0xff);
                dst[hdr + 2*k + 1] = (uint8_t)(seg >> 8);
                csize += seg;
            }
        }
    }

    for (int k = 0; k < np; k++) { free(planes[k]); free(prevL[k]); free(curL[k]); free(segbuf[k]); free(seglen[k]); }
    free(uline);
    if (ovf) return -1;
    return (int)csize;
}

int nlc_decode(const uint8_t* in, size_t csize, uint8_t* out, const nlc_params* p) {
    if (!in || !out || !p) return -1;
    if (p->rgb == NLC_RGB565) return -1;
    int W = p->width, H = p->height, np = plane_count(p->rgb);
    int bpp = (p->rgb == NLC_RGBA) ? 4 : 3;
    int wbits = p->width_bits > 0 ? p->width_bits : 4;
    int tile  = p->tile > 0 ? p->tile : 16;

    int16_t* prevL[4] = {0,0,0,0};
    int16_t* curL[4]  = {0,0,0,0};
    for (int k = 0; k < np; k++) { prevL[k] = (int16_t*)calloc(W, sizeof(int16_t)); curL[k] = (int16_t*)calloc(W, sizeof(int16_t)); }
    uint32_t* uline = (uint32_t*)malloc(sizeof(uint32_t) * W);

    // FORMAT v2: per line, an 8-byte word-aligned header of u16 PADDED segment lengths, then each
    // plane's word-aligned segment (mirrors the parallel HW decoder's independent bit-readers).
    size_t pos = 0;
    for (int y = 0; y < H; y++) {
        size_t seglen[4] = {0,0,0,0};
        if (pos + 8 <= csize) {
            for (int k = 0; k < np && k < 4; k++) seglen[k] = (size_t)in[pos + 2*k] | ((size_t)in[pos + 2*k + 1] << 8);
            pos += 8;
        } else pos = csize;                                   // truncated input: decode zeros below
        for (int k = 0; k < np; k++) {
            int lo, hi; plane_range(p->color, k, &lo, &hi);
            size_t avail = (pos <= csize) ? (csize - pos) : 0;
            BitR br; br.init(in + pos, seglen[k] <= avail ? seglen[k] : avail);
            if      (p->pack == NLC_PACK_GLOBAL) unpack_line_global(br, uline, W, wbits);
            else if (p->pack == NLC_PACK_TILED)  unpack_line_tiled(br, uline, W, tile, wbits);
            else                                 unpack_line_rice(br, uline, W, tile, wbits);
            line_decode(uline, prevL[k], curL[k], W, y, p->near_lvl, lo, hi);
            pos += seglen[k];
        }
        combine_line(curL, p, out + (size_t)y * W * bpp, W);
        for (int k = 0; k < np; k++) { int16_t* t = prevL[k]; prevL[k] = curL[k]; curL[k] = t; }
    }

    for (int k = 0; k < np; k++) { free(prevL[k]); free(curL[k]); }
    free(uline);
    return 0;
}
