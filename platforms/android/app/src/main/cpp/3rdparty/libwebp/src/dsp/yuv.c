// Copyright 2010 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// YUV->RGB conversion functions
//
// Author: Skal (pascal.massimino@gmail.com)

#include "src/dsp/yuv.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "src/dsp/cpu.h"
#include "src/dsp/dsp.h"
#include "src/webp/decode.h"
#include "src/webp/types.h"

// Uncomment to disable gamma-compression during RGB->U/V averaging
#define USE_GAMMA_COMPRESSION

// If defined, use table to compute x / alpha.
#define USE_INVERSE_ALPHA_TABLE

#ifdef USE_GAMMA_COMPRESSION
#include <math.h>
#endif

//-----------------------------------------------------------------------------
// Plain-C version

#define ROW_FUNC(FUNC_NAME, FUNC, XSTEP)                                     \
  static void FUNC_NAME(                                                     \
      const uint8_t* WEBP_RESTRICT y, const uint8_t* WEBP_RESTRICT u,        \
      const uint8_t* WEBP_RESTRICT v, uint8_t* WEBP_RESTRICT dst, int len) { \
    const uint8_t* const end = dst + (len & ~1) * (XSTEP);                   \
    while (dst != end) {                                                     \
      FUNC(y[0], u[0], v[0], dst);                                           \
      FUNC(y[1], u[0], v[0], dst + (XSTEP));                                 \
      y += 2;                                                                \
      ++u;                                                                   \
      ++v;                                                                   \
      dst += 2 * (XSTEP);                                                    \
    }                                                                        \
    if (len & 1) {                                                           \
      FUNC(y[0], u[0], v[0], dst);                                           \
    }                                                                        \
  }

// All variants implemented.
ROW_FUNC(YuvToRgbRow, VP8YuvToRgb, 3)
ROW_FUNC(YuvToBgrRow, VP8YuvToBgr, 3)
ROW_FUNC(YuvToRgbaRow, VP8YuvToRgba, 4)
ROW_FUNC(YuvToBgraRow, VP8YuvToBgra, 4)
ROW_FUNC(YuvToArgbRow, VP8YuvToArgb, 4)
ROW_FUNC(YuvToRgba4444Row, VP8YuvToRgba4444, 2)
ROW_FUNC(YuvToRgb565Row, VP8YuvToRgb565, 2)

#undef ROW_FUNC

// Main call for processing a plane with a WebPSamplerRowFunc function:
void WebPSamplerProcessPlane(const uint8_t* WEBP_RESTRICT y, int y_stride,
                             const uint8_t* WEBP_RESTRICT u,
                             const uint8_t* WEBP_RESTRICT v, int uv_stride,
                             uint8_t* WEBP_RESTRICT dst, int dst_stride,
                             int width, int height, WebPSamplerRowFunc func) {
  int j;
  for (j = 0; j < height; ++j) {
    func(y, u, v, dst, width);
    y += y_stride;
    if (j & 1) {
      u += uv_stride;
      v += uv_stride;
    }
    dst += dst_stride;
  }
}

//-----------------------------------------------------------------------------
// Main call

WebPSamplerRowFunc WebPSamplers[MODE_LAST];

extern VP8CPUInfo VP8GetCPUInfo;
extern void WebPInitSamplersSSE2(void);
extern void WebPInitSamplersSSE41(void);
extern void WebPInitSamplersMIPS32(void);
extern void WebPInitSamplersMIPSdspR2(void);

WEBP_DSP_INIT_FUNC(WebPInitSamplers) {
  WebPSamplers[MODE_RGB] = YuvToRgbRow;
  WebPSamplers[MODE_RGBA] = YuvToRgbaRow;
  WebPSamplers[MODE_BGR] = YuvToBgrRow;
  WebPSamplers[MODE_BGRA] = YuvToBgraRow;
  WebPSamplers[MODE_ARGB] = YuvToArgbRow;
  WebPSamplers[MODE_RGBA_4444] = YuvToRgba4444Row;
  WebPSamplers[MODE_RGB_565] = YuvToRgb565Row;
  WebPSamplers[MODE_rgbA] = YuvToRgbaRow;
  WebPSamplers[MODE_bgrA] = YuvToBgraRow;
  WebPSamplers[MODE_Argb] = YuvToArgbRow;
  WebPSamplers[MODE_rgbA_4444] = YuvToRgba4444Row;

  // If defined, use CPUInfo() to overwrite some pointers with faster versions.
  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPInitSamplersSSE2();
    }
#endif  // WEBP_HAVE_SSE2
#if defined(WEBP_HAVE_SSE41)
    if (VP8GetCPUInfo(kSSE4_1)) {
      WebPInitSamplersSSE41();
    }
#endif  // WEBP_HAVE_SSE41
#if defined(WEBP_USE_MIPS32)
    if (VP8GetCPUInfo(kMIPS32)) {
      WebPInitSamplersMIPS32();
    }
#endif  // WEBP_USE_MIPS32
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      WebPInitSamplersMIPSdspR2();
    }
#endif  // WEBP_USE_MIPS_DSP_R2
  }
}

//-----------------------------------------------------------------------------
// ARGB -> YUV converters

static void ConvertARGBToY_C(const uint32_t* WEBP_RESTRICT argb,
                             uint8_t* WEBP_RESTRICT y, int width) {
  int i;
  for (i = 0; i < width; ++i) {
    const uint32_t p = argb[i];
    y[i] =
        VP8RGBToY((p >> 16) & 0xff, (p >> 8) & 0xff, (p >> 0) & 0xff, YUV_HALF);
  }
}

void WebPConvertARGBToUV_C(const uint32_t* WEBP_RESTRICT argb,
                           uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                           int src_width, int do_store) {
  // No rounding. Last pixel is dealt with separately.
  const int uv_width = src_width >> 1;
  int i;
  for (i = 0; i < uv_width; ++i) {
    const uint32_t v0 = argb[2 * i + 0];
    const uint32_t v1 = argb[2 * i + 1];
    // VP8RGBToU/V expects four accumulated pixels. Hence we need to
    // scale r/g/b value by a factor 2. We just shift v0/v1 one bit less.
    const int r = ((v0 >> 15) & 0x1fe) + ((v1 >> 15) & 0x1fe);
    const int g = ((v0 >> 7) & 0x1fe) + ((v1 >> 7) & 0x1fe);
    const int b = ((v0 << 1) & 0x1fe) + ((v1 << 1) & 0x1fe);
    const int tmp_u = VP8RGBToU(r, g, b, YUV_HALF << 2);
    const int tmp_v = VP8RGBToV(r, g, b, YUV_HALF << 2);
    if (do_store) {
      u[i] = tmp_u;
      v[i] = tmp_v;
    } else {
      // Approximated average-of-four. But it's an acceptable diff.
      u[i] = (u[i] + tmp_u + 1) >> 1;
      v[i] = (v[i] + tmp_v + 1) >> 1;
    }
  }
  if (src_width & 1) {  // last pixel
    const uint32_t v0 = argb[2 * i + 0];
    const int r = (v0 >> 14) & 0x3fc;
    const int g = (v0 >> 6) & 0x3fc;
    const int b = (v0 << 2) & 0x3fc;
    const int tmp_u = VP8RGBToU(r, g, b, YUV_HALF << 2);
    const int tmp_v = VP8RGBToV(r, g, b, YUV_HALF << 2);
    if (do_store) {
      u[i] = tmp_u;
      v[i] = tmp_v;
    } else {
      u[i] = (u[i] + tmp_u + 1) >> 1;
      v[i] = (v[i] + tmp_v + 1) >> 1;
    }
  }
}

//-----------------------------------------------------------------------------

static void ConvertRGBToY_C(const uint8_t* WEBP_RESTRICT rgb,
                            uint8_t* WEBP_RESTRICT y, int width, int step) {
  int i;
  for (i = 0; i < width; ++i, rgb += step) {
    y[i] = VP8RGBToY(rgb[0], rgb[1], rgb[2], YUV_HALF);
  }
}

static void ConvertBGRToY_C(const uint8_t* WEBP_RESTRICT bgr,
                            uint8_t* WEBP_RESTRICT y, int width, int step) {
  int i;
  for (i = 0; i < width; ++i, bgr += step) {
    y[i] = VP8RGBToY(bgr[2], bgr[1], bgr[0], YUV_HALF);
  }
}

void WebPConvertRGBA32ToUV_C(const uint16_t* WEBP_RESTRICT rgb,
                             uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                             int width) {
  int i;
  for (i = 0; i < width; i += 1, rgb += 4) {
    const int r = rgb[0], g = rgb[1], b = rgb[2];
    u[i] = VP8RGBToU(r, g, b, YUV_HALF << 2);
    v[i] = VP8RGBToV(r, g, b, YUV_HALF << 2);
  }
}

//------------------------------------------------------------------------------
// Code for gamma correction

#if defined(USE_GAMMA_COMPRESSION)

// Gamma correction compensates loss of resolution during chroma subsampling.
#define GAMMA_FIX 12     // fixed-point precision for linear values
#define GAMMA_TAB_FIX 7  // fixed-point fractional bits precision
#define GAMMA_TAB_SIZE (1 << (GAMMA_FIX - GAMMA_TAB_FIX))
static const double kGamma = 0.80;
static const int kGammaScale = ((1 << GAMMA_FIX) - 1);
static const int kGammaTabScale = (1 << GAMMA_TAB_FIX);
static const int kGammaTabRounder = (1 << GAMMA_TAB_FIX >> 1);

static int kLinearToGammaTab[GAMMA_TAB_SIZE + 1];
static uint16_t kGammaToLinearTab[256];
static volatile int kGammaTablesOk = 0;
extern VP8CPUInfo VP8GetCPUInfo;

WEBP_DSP_INIT_FUNC(WebPInitGammaTables) {
  if (!kGammaTablesOk) {
    int v;
    const double scale = (double)(1 << GAMMA_TAB_FIX) / kGammaScale;
    const double norm = 1. / 255.;
    for (v = 0; v <= 255; ++v) {
      kGammaToLinearTab[v] =
          (uint16_t)(pow(norm * v, kGamma) * kGammaScale + .5);
    }
    for (v = 0; v <= GAMMA_TAB_SIZE; ++v) {
      kLinearToGammaTab[v] = (int)(255. * pow(scale * v, 1. / kGamma) + .5);
    }
    kGammaTablesOk = 1;
  }
}

static WEBP_INLINE uint32_t GammaToLinear(uint8_t v) {
  return kGammaToLinearTab[v];
}

static WEBP_INLINE int Interpolate(int v) {
  const int tab_pos = v >> (GAMMA_TAB_FIX + 2);   // integer part
  const int x = v & ((kGammaTabScale << 2) - 1);  // fractional part
  const int v0 = kLinearToGammaTab[tab_pos];
  const int v1 = kLinearToGammaTab[tab_pos + 1];
  const int y = v1 * x + v0 * ((kGammaTabScale << 2) - x);  // interpolate
  assert(tab_pos + 1 < GAMMA_TAB_SIZE + 1);
  return y;
}

// Convert a linear value 'v' to YUV_FIX+2 fixed-point precision
// U/V value, suitable for RGBToU/V calls.
static WEBP_INLINE int LinearToGamma(uint32_t base_value, int shift) {
  const int y = Interpolate(base_value << shift);  // final uplifted value
  return (y + kGammaTabRounder) >> GAMMA_TAB_FIX;  // descale
}

#else

void WebPInitGammaTables(void) {}
static WEBP_INLINE uint32_t GammaToLinear(uint8_t v) { return v; }
static WEBP_INLINE int LinearToGamma(uint32_t base_value, int shift) {
  return (int)(base_value << shift);
}

#endif  // USE_GAMMA_COMPRESSION

#define SUM4(ptr, step)                                                  \
  LinearToGamma(GammaToLinear((ptr)[0]) + GammaToLinear((ptr)[(step)]) + \
                    GammaToLinear((ptr)[rgb_stride]) +                   \
                    GammaToLinear((ptr)[rgb_stride + (step)]),           \
                0)

#define SUM2(ptr) \
  LinearToGamma(GammaToLinear((ptr)[0]) + GammaToLinear((ptr)[rgb_stride]), 1)

//------------------------------------------------------------------------------
// "Fast" regular RGB->YUV

#define SUM4(ptr, step)                                                  \
  LinearToGamma(GammaToLinear((ptr)[0]) + GammaToLinear((ptr)[(step)]) + \
                    GammaToLinear((ptr)[rgb_stride]) +                   \
                    GammaToLinear((ptr)[rgb_stride + (step)]),           \
                0)

#define SUM2(ptr) \
  LinearToGamma(GammaToLinear((ptr)[0]) + GammaToLinear((ptr)[rgb_stride]), 1)

#define SUM2ALPHA(ptr) ((ptr)[0] + (ptr)[rgb_stride])
#define SUM4ALPHA(ptr) (SUM2ALPHA(ptr) + SUM2ALPHA((ptr) + 4))

#if defined(USE_INVERSE_ALPHA_TABLE)

static const int kAlphaFix = 19;
// Following table is (1 << kAlphaFix) / a. The (v * kInvAlpha[a]) >> kAlphaFix
// formula is then equal to v / a in most (99.6%) cases. Note that this table
// and constant are adjusted very tightly to fit 32b arithmetic.
// In particular, they use the fact that the operands for 'v / a' are actually
// derived as v = (a0.p0 + a1.p1 + a2.p2 + a3.p3) and a = a0 + a1 + a2 + a3
// with ai in [0..255] and pi in [0..1<<GAMMA_FIX). The constraint to avoid
// overflow is: GAMMA_FIX + kAlphaFix <= 31.
static const uint32_t kInvAlpha[4 * 0xff + 1] = {
    0, /* alpha = 0 */
    524288, 262144, 174762, 131072, 104857, 87381, 74898, 65536, 58254, 52428,
    47662,  43690,  40329,  37449,  34952,  32768, 30840, 29127, 27594, 26214,
    24966,  23831,  22795,  21845,  20971,  20164, 19418, 18724, 18078, 17476,
    16912,  16384,  15887,  15420,  14979,  14563, 14169, 13797, 13443, 13107,
    12787,  12483,  12192,  11915,  11650,  11397, 11155, 10922, 10699, 10485,
    10280,  10082,  9892,   9709,   9532,   9362,  9198,  9039,  8886,  8738,
    8594,   8456,   8322,   8192,   8065,   7943,  7825,  7710,  7598,  7489,
    7384,   7281,   7182,   7084,   6990,   6898,  6808,  6721,  6636,  6553,
    6472,   6393,   6316,   6241,   6168,   6096,  6026,  5957,  5890,  5825,
    5761,   5698,   5637,   5577,   5518,   5461,  5405,  5349,  5295,  5242,
    5190,   5140,   5090,   5041,   4993,   4946,  4899,  4854,  4809,  4766,
    4723,   4681,   4639,   4599,   4559,   4519,  4481,  4443,  4405,  4369,
    4332,   4297,   4262,   4228,   4194,   4161,  4128,  4096,  4064,  4032,
    4002,   3971,   3942,   3912,   3883,   3855,  3826,  3799,  3771,  3744,
    3718,   3692,   3666,   3640,   3615,   3591,  3566,  3542,  3518,  3495,
    3472,   3449,   3426,   3404,   3382,   3360,  3339,  3318,  3297,  3276,
    3256,   3236,   3216,   3196,   3177,   3158,  3139,  3120,  3102,  3084,
    3066,   3048,   3030,   3013,   2995,   2978,  2962,  2945,  2928,  2912,
    2896,   2880,   2864,   2849,   2833,   2818,  2803,  2788,  2774,  2759,
    2744,   2730,   2716,   2702,   2688,   2674,  2661,  2647,  2634,  2621,
    2608,   2595,   2582,   2570,   2557,   2545,  2532,  2520,  2508,  2496,
    2484,   2473,   2461,   2449,   2438,   2427,  2416,  2404,  2394,  2383,
    2372,   2361,   2351,   2340,   2330,   2319,  2309,  2299,  2289,  2279,
    2269,   2259,   2250,   2240,   2231,   2221,  2212,  2202,  2193,  2184,
    2175,   2166,   2157,   2148,   2139,   2131,  2122,  2114,  2105,  2097,
    2088,   2080,   2072,   2064,   2056,   2048,  2040,  2032,  2024,  2016,
    2008,   2001,   1993,   1985,   1978,   1971,  1963,  1956,  1949,  1941,
    1934,   1927,   1920,   1913,   1906,   1899,  1892,  1885,  1879,  1872,
    1865,   1859,   1852,   1846,   1839,   1833,  1826,  1820,  1814,  1807,
    1801,   1795,   1789,   1783,   1777,   1771,  1765,  1759,  1753,  1747,
    1741,   1736,   1730,   1724,   1718,   1713,  1707,  1702,  1696,  1691,
    1685,   1680,   1675,   1669,   1664,   1659,  1653,  1648,  1643,  1638,
    1633,   1628,   1623,   1618,   1613,   1608,  1603,  1598,  1593,  1588,
    1583,   1579,   1574,   1569,   1565,   1560,  1555,  1551,  1546,  1542,
    1537,   1533,   1528,   1524,   1519,   1515,  1510,  1506,  1502,  1497,
    1493,   1489,   1485,   1481,   1476,   1472,  1468,  1464,  1460,  1456,
    1452,   1448,   1444,   1440,   1436,   1432,  1428,  1424,  1420,  1416,
    1413,   1409,   1405,   1401,   1398,   1394,  1390,  1387,  1383,  1379,
    1376,   1372,   1368,   1365,   1361,   1358,  1354,  1351,  1347,  1344,
    1340,   1337,   1334,   1330,   1327,   1323,  1320,  1317,  1314,  1310,
    1307,   1304,   1300,   1297,   1294,   1291,  1288,  1285,  1281,  1278,
    1275,   1272,   1269,   1266,   1263,   1260,  1257,  1254,  1251,  1248,
    1245,   1242,   1239,   1236,   1233,   1230,  1227,  1224,  1222,  1219,
    1216,   1213,   1210,   1208,   1205,   1202,  1199,  1197,  1194,  1191,
    1188,   1186,   1183,   1180,   1178,   1175,  1172,  1170,  1167,  1165,
    1162,   1159,   1157,   1154,   1152,   1149,  1147,  1144,  1142,  1139,
    1137,   1134,   1132,   1129,   1127,   1125,  1122,  1120,  1117,  1115,
    1113,   1110,   1108,   1106,   1103,   1101,  1099,  1096,  1094,  1092,
    1089,   1087,   1085,   1083,   1081,   1078,  1076,  1074,  1072,  1069,
    1067,   1065,   1063,   1061,   1059,   1057,  1054,  1052,  1050,  1048,
    1046,   1044,   1042,   1040,   1038,   1036,  1034,  1032,  1030,  1028,
    1026,   1024,   1022,   1020,   1018,   1016,  1014,  1012,  1010,  1008,
    1006,   1004,   1002,   1000,   998,    996,   994,   992,   991,   989,
    987,    985,    983,    981,    979,    978,   976,   974,   972,   970,
    969,    967,    965,    963,    961,    960,   958,   956,   954,   953,
    951,    949,    948,    946,    944,    942,   941,   939,   937,   936,
    934,    932,    931,    929,    927,    926,   924,   923,   921,   919,
    918,    916,    914,    913,    911,    910,   908,   907,   905,   903,
    902,    900,    899,    897,    896,    894,   893,   891,   890,   888,
    887,    885,    884,    882,    881,    879,   878,   876,   875,   873,
    872,    870,    869,    868,    866,    865,   863,   862,   860,   859,
    858,    856,    855,    853,    852,    851,   849,   848,   846,   845,
    844,    842,    841,    840,    838,    837,   836,   834,   833,   832,
    830,    829,    828,    826,    825,    824,   823,   821,   820,   819,
    817,    816,    815,    814,    812,    811,   810,   809,   807,   806,
    805,    804,    802,    801,    800,    799,   798,   796,   795,   794,
    793,    791,    790,    789,    788,    787,   786,   784,   783,   782,
    781,    780,    779,    777,    776,    775,   774,   773,   772,   771,
    769,    768,    767,    766,    765,    764,   763,   762,   760,   759,
    758,    757,    756,    755,    754,    753,   752,   751,   750,   748,
    747,    746,    745,    744,    743,    742,   741,   740,   739,   738,
    737,    736,    735,    734,    733,    732,   731,   730,   729,   728,
    727,    726,    725,    724,    723,    722,   721,   720,   719,   718,
    717,    716,    715,    714,    713,    712,   711,   710,   709,   708,
    707,    706,    705,    704,    703,    702,   701,   700,   699,   699,
    698,    697,    696,    695,    694,    693,   692,   691,   690,   689,
    688,    688,    687,    686,    685,    684,   683,   682,   681,   680,
    680,    679,    678,    677,    676,    675,   674,   673,   673,   672,
    671,    670,    669,    668,    667,    667,   666,   665,   664,   663,
    662,    661,    661,    660,    659,    658,   657,   657,   656,   655,
    654,    653,    652,    652,    651,    650,   649,   648,   648,   647,
    646,    645,    644,    644,    643,    642,   641,   640,   640,   639,
    638,    637,    637,    636,    635,    634,   633,   633,   632,   631,
    630,    630,    629,    628,    627,    627,   626,   625,   624,   624,
    623,    622,    621,    621,    620,    619,   618,   618,   617,   616,
    616,    615,    614,    613,    613,    612,   611,   611,   610,   609,
    608,    608,    607,    606,    606,    605,   604,   604,   603,   602,
    601,    601,    600,    599,    599,    598,   597,   597,   596,   595,
    595,    594,    593,    593,    592,    591,   591,   590,   589,   589,
    588,    587,    587,    586,    585,    585,   584,   583,   583,   582,
    581,    581,    580,    579,    579,    578,   578,   577,   576,   576,
    575,    574,    574,    573,    572,    572,   571,   571,   570,   569,
    569,    568,    568,    567,    566,    566,   565,   564,   564,   563,
    563,    562,    561,    561,    560,    560,   559,   558,   558,   557,
    557,    556,    555,    555,    554,    554,   553,   553,   552,   551,
    551,    550,    550,    549,    548,    548,   547,   547,   546,   546,
    545,    544,    544,    543,    543,    542,   542,   541,   541,   540,
    539,    539,    538,    538,    537,    537,   536,   536,   535,   534,
    534,    533,    533,    532,    532,    531,   531,   530,   530,   529,
    529,    528,    527,    527,    526,    526,   525,   525,   524,   524,
    523,    523,    522,    522,    521,    521,   520,   520,   519,   519,
    518,    518,    517,    517,    516,    516,   515,   515,   514,   514};

// Note that LinearToGamma() expects the values to be premultiplied by 4,
// so we incorporate this factor 4 inside the DIVIDE_BY_ALPHA macro directly.
#define DIVIDE_BY_ALPHA(sum, a) (((sum) * kInvAlpha[(a)]) >> (kAlphaFix - 2))

#else

#define DIVIDE_BY_ALPHA(sum, a) (4 * (sum) / (a))

#endif  // USE_INVERSE_ALPHA_TABLE

static WEBP_INLINE int LinearToGammaWeighted(const uint8_t* src,
                                             const uint8_t* a_ptr,
                                             uint32_t total_a, int step,
                                             int rgb_stride) {
  const uint32_t sum =
      a_ptr[0] * GammaToLinear(src[0]) +
      a_ptr[step] * GammaToLinear(src[step]) +
      a_ptr[rgb_stride] * GammaToLinear(src[rgb_stride]) +
      a_ptr[rgb_stride + step] * GammaToLinear(src[rgb_stride + step]);
  assert(total_a > 0 && total_a <= 4 * 0xff);
#if defined(USE_INVERSE_ALPHA_TABLE)
  assert((uint64_t)sum * kInvAlpha[total_a] < ((uint64_t)1 << 32));
#endif
  return LinearToGamma(DIVIDE_BY_ALPHA(sum, total_a), 0);
}

void WebPAccumulateRGBA(const uint8_t* const r_ptr, const uint8_t* const g_ptr,
                        const uint8_t* const b_ptr, const uint8_t* const a_ptr,
                        int rgb_stride, uint16_t* dst, int width) {
  int i, j;
  // we loop over 2x2 blocks and produce one R/G/B/A value for each.
  for (i = 0, j = 0; i < (width >> 1); i += 1, j += 2 * 4, dst += 4) {
    const uint32_t a = SUM4ALPHA(a_ptr + j);
    int r, g, b;
    if (a == 4 * 0xff || a == 0) {
      r = SUM4(r_ptr + j, 4);
      g = SUM4(g_ptr + j, 4);
      b = SUM4(b_ptr + j, 4);
    } else {
      r = LinearToGammaWeighted(r_ptr + j, a_ptr + j, a, 4, rgb_stride);
      g = LinearToGammaWeighted(g_ptr + j, a_ptr + j, a, 4, rgb_stride);
      b = LinearToGammaWeighted(b_ptr + j, a_ptr + j, a, 4, rgb_stride);
    }
    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
    dst[3] = a;
  }
  if (width & 1) {
    const uint32_t a = 2u * SUM2ALPHA(a_ptr + j);
    int r, g, b;
    if (a == 4 * 0xff || a == 0) {
      r = SUM2(r_ptr + j);
      g = SUM2(g_ptr + j);
      b = SUM2(b_ptr + j);
    } else {
      r = LinearToGammaWeighted(r_ptr + j, a_ptr + j, a, 0, rgb_stride);
      g = LinearToGammaWeighted(g_ptr + j, a_ptr + j, a, 0, rgb_stride);
      b = LinearToGammaWeighted(b_ptr + j, a_ptr + j, a, 0, rgb_stride);
    }
    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
    dst[3] = a;
  }
}

void WebPAccumulateRGB(const uint8_t* const r_ptr, const uint8_t* const g_ptr,
                       const uint8_t* const b_ptr, int step, int rgb_stride,
                       uint16_t* dst, int width) {
  int i, j;
  for (i = 0, j = 0; i < (width >> 1); i += 1, j += 2 * step, dst += 4) {
    dst[0] = SUM4(r_ptr + j, step);
    dst[1] = SUM4(g_ptr + j, step);
    dst[2] = SUM4(b_ptr + j, step);
    // MemorySanitizer may raise false positives with data that passes through
    // RGBA32PackedToPlanar_16b_SSE41() due to incorrect modeling of shuffles.
    // See https://crbug.com/webp/573.
#ifdef WEBP_MSAN
    dst[3] = 0;
#endif
  }
  if (width & 1) {
    dst[0] = SUM2(r_ptr + j);
    dst[1] = SUM2(g_ptr + j);
    dst[2] = SUM2(b_ptr + j);
#ifdef WEBP_MSAN
    dst[3] = 0;
#endif
  }
}

static void ImportYUVAFromRGBA_C(const uint8_t* r_ptr, const uint8_t* g_ptr,
                                 const uint8_t* b_ptr, const uint8_t* a_ptr,
                                 int step,        // bytes per pixel
                                 int rgb_stride,  // bytes per scanline
                                 int has_alpha, int width, int height,
                                 uint16_t* tmp_rgb, int y_stride, int uv_stride,
                                 int a_stride, uint8_t* dst_y, uint8_t* dst_u,
                                 uint8_t* dst_v, uint8_t* dst_a) {
  int y;
  const int is_rgb = (r_ptr < b_ptr);  // otherwise it's bgr
  const int uv_width = (width + 1) >> 1;

  has_alpha &= dst_a != NULL;
  if (has_alpha) {
#if defined(USE_GAMMA_COMPRESSION) && defined(USE_INVERSE_ALPHA_TABLE)
    assert(kAlphaFix + GAMMA_FIX <= 31);
#endif
  }

  WebPInitGammaTables();

  // Downsample Y/U/V planes, two rows at a time
  for (y = 0; y < (height >> 1); ++y) {
    int rows_have_alpha = has_alpha;
    if (is_rgb) {
      WebPConvertRGBToY(r_ptr, dst_y, width, step);
      WebPConvertRGBToY(r_ptr + rgb_stride, dst_y + y_stride, width, step);
    } else {
      WebPConvertBGRToY(b_ptr, dst_y, width, step);
      WebPConvertBGRToY(b_ptr + rgb_stride, dst_y + y_stride, width, step);
    }
    dst_y += 2 * y_stride;
    if (has_alpha) {
      rows_have_alpha &=
          !WebPExtractAlpha(a_ptr, rgb_stride, width, 2, dst_a, a_stride);
      dst_a += 2 * a_stride;
    } else if (dst_a != NULL) {
      int i;
      for (i = 0; i < 2; ++i, dst_a += a_stride) {
        memset(dst_a, 0xff, width);
      }
    }

    // Collect averaged R/G/B(/A)
    if (!rows_have_alpha) {
      WebPAccumulateRGB(r_ptr, g_ptr, b_ptr, step, rgb_stride, tmp_rgb, width);
    } else {
      WebPAccumulateRGBA(r_ptr, g_ptr, b_ptr, a_ptr, rgb_stride, tmp_rgb,
                         width);
    }
    // Convert to U/V
    WebPConvertRGBA32ToUV(tmp_rgb, dst_u, dst_v, uv_width);
    dst_u += uv_stride;
    dst_v += uv_stride;
    r_ptr += 2 * rgb_stride;
    b_ptr += 2 * rgb_stride;
    g_ptr += 2 * rgb_stride;
    if (has_alpha) a_ptr += 2 * rgb_stride;
  }
}

static void ImportYUVAFromRGBALastLine_C(
    const uint8_t* r_ptr, const uint8_t* g_ptr, const uint8_t* b_ptr,
    const uint8_t* a_ptr,
    int step,  // bytes per pixel
    int has_alpha, int width, uint16_t* tmp_rgb, uint8_t* dst_y, uint8_t* dst_u,
    uint8_t* dst_v, uint8_t* dst_a) {
  const int is_rgb = (r_ptr < b_ptr);  // otherwise it's bgr
  const int uv_width = (width + 1) >> 1;
  int row_has_alpha = has_alpha && dst_a != NULL;

  if (is_rgb) {
    WebPConvertRGBToY(r_ptr, dst_y, width, step);
  } else {
    WebPConvertBGRToY(b_ptr, dst_y, width, step);
  }
  if (row_has_alpha) {
    row_has_alpha &= !WebPExtractAlpha(a_ptr, 0, width, 1, dst_a, 0);
  } else if (dst_a != NULL) {
    memset(dst_a, 0xff, width);
  }

  // Collect averaged R/G/B(/A)
  if (!row_has_alpha) {
    // Collect averaged R/G/B
    WebPAccumulateRGB(r_ptr, g_ptr, b_ptr, step, /*rgb_stride=*/0, tmp_rgb,
                      width);
  } else {
    WebPAccumulateRGBA(r_ptr, g_ptr, b_ptr, a_ptr, /*rgb_stride=*/0, tmp_rgb,
                       width);
  }
  WebPConvertRGBA32ToUV(tmp_rgb, dst_u, dst_v, uv_width);
}

//-----------------------------------------------------------------------------

void (*WebPConvertRGBToY)(const uint8_t* WEBP_RESTRICT rgb,
                          uint8_t* WEBP_RESTRICT y, int width, int step);
void (*WebPConvertBGRToY)(const uint8_t* WEBP_RESTRICT bgr,
                          uint8_t* WEBP_RESTRICT y, int width, int step);
void (*WebPConvertRGBA32ToUV)(const uint16_t* WEBP_RESTRICT rgb,
                              uint8_t* WEBP_RESTRICT u,
                              uint8_t* WEBP_RESTRICT v, int width);

void (*WebPImportYUVAFromRGBA)(const uint8_t* r_ptr, const uint8_t* g_ptr,
                               const uint8_t* b_ptr, const uint8_t* a_ptr,
                               int step,        // bytes per pixel
                               int rgb_stride,  // bytes per scanline
                               int has_alpha, int width, int height,
                               uint16_t* tmp_rgb, int y_stride, int uv_stride,
                               int a_stride, uint8_t* dst_y, uint8_t* dst_u,
                               uint8_t* dst_v, uint8_t* dst_a);
void (*WebPImportYUVAFromRGBALastLine)(
    const uint8_t* r_ptr, const uint8_t* g_ptr, const uint8_t* b_ptr,
    const uint8_t* a_ptr,
    int step,  // bytes per pixel
    int has_alpha, int width, uint16_t* tmp_rgb, uint8_t* dst_y, uint8_t* dst_u,
    uint8_t* dst_v, uint8_t* dst_a);

void (*WebPConvertARGBToY)(const uint32_t* WEBP_RESTRICT argb,
                           uint8_t* WEBP_RESTRICT y, int width);
void (*WebPConvertARGBToUV)(const uint32_t* WEBP_RESTRICT argb,
                            uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                            int src_width, int do_store);

extern void WebPInitConvertARGBToYUVSSE2(void);
extern void WebPInitConvertARGBToYUVSSE41(void);
extern void WebPInitConvertARGBToYUVNEON(void);

WEBP_DSP_INIT_FUNC(WebPInitConvertARGBToYUV) {
  WebPConvertARGBToY = ConvertARGBToY_C;
  WebPConvertARGBToUV = WebPConvertARGBToUV_C;

  WebPConvertRGBToY = ConvertRGBToY_C;
  WebPConvertBGRToY = ConvertBGRToY_C;

  WebPConvertRGBA32ToUV = WebPConvertRGBA32ToUV_C;

  WebPImportYUVAFromRGBA = ImportYUVAFromRGBA_C;
  WebPImportYUVAFromRGBALastLine = ImportYUVAFromRGBALastLine_C;

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPInitConvertARGBToYUVSSE2();
    }
#endif  // WEBP_HAVE_SSE2
#if defined(WEBP_HAVE_SSE41)
    if (VP8GetCPUInfo(kSSE4_1)) {
      WebPInitConvertARGBToYUVSSE41();
    }
#endif  // WEBP_HAVE_SSE41
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    WebPInitConvertARGBToYUVNEON();
  }
#endif  // WEBP_HAVE_NEON

  assert(WebPConvertARGBToY != NULL);
  assert(WebPConvertARGBToUV != NULL);
  assert(WebPConvertRGBToY != NULL);
  assert(WebPConvertBGRToY != NULL);
  assert(WebPConvertRGBA32ToUV != NULL);
}
