// SPDX-FileCopyrightText: 2007-2010 Christian Kothe, 2024 Connor McLaughlin <stenzek@gmail.com>
// SPDX-License-Identifier: GPL-2.0+

#include "FreeSurroundDecoder.h"

#include <algorithm>
#include <cmath>

static constexpr float pi = 3.141592654f;
static constexpr float epsilon = 0.000001f;

template<typename T>
static inline T sqr(T x)
{
  return x * x;
}

template<typename T>
static inline T clamp1(T x)
{
  return std::clamp(x, static_cast<T>(-1), static_cast<T>(1));
}

template<typename T>
static inline T sign(T x)
{
  return x < static_cast<T>(0) ? static_cast<T>(-1) : (x > static_cast<T>(0) ? static_cast<T>(1) : static_cast<T>(0));
}

static inline double amplitude(const std::complex<double>& x)
{
  return sqrt(sqr(x.real()) + sqr(x.imag()));
}
static inline double phase(const std::complex<double>& x)
{
  return atan2(x.imag(), x.real());
}
static inline std::complex<double> polar(double a, double p)
{
  return std::complex<double>(a * std::cos(p), a * std::sin(p));
}

// get the distance of the soundfield edge, along a given angle
static inline double edgedistance(double a)
{
  return std::min(std::sqrt(1 + sqr(std::tan(a))), std::sqrt(1 + sqr(1 / std::tan(a))));
}

static void transform_decode(double a, double p, double& x, double& y);

// apply a circular_wrap transformation to some position
static void transform_circular_wrap(double& x, double& y, double refangle);

// apply a focus transformation to some position
static void transform_focus(double& x, double& y, double focus);

// get the index (and fractional offset!) in a piecewise-linear channel allocation grid
int FreeSurroundDecoder::MapToGrid(double& x)
{
  const double gp = ((x + 1) * 0.5) * (grid_res - 1);
  const double i = std::min(static_cast<double>(grid_res - 2), std::floor(gp));
  x = gp - i;
  return static_cast<int>(i);
}

FreeSurroundDecoder::FreeSurroundDecoder(ChannelSetup setup, unsigned blocksize)
  : cmap(s_channel_maps[static_cast<size_t>(setup)])
{
  N = blocksize;
  C = static_cast<unsigned>(cmap.luts.size());
  wnd.resize(blocksize);
  lt.resize(blocksize);
  rt.resize(blocksize);
  dst.resize(blocksize);
  lf.resize(blocksize / 2 + 1);
  rf.resize(blocksize / 2 + 1);

  forward = kiss_fftr_alloc(blocksize, 0, 0, 0);
  inverse = kiss_fftr_alloc(blocksize, 1, 0, 0);

  // allocate per-channel buffers
  inbuf.resize(3 * N);
  outbuf.resize((N + N / 2) * C);
  signal.resize(C, std::vector<cplx>(N));

  // init the window function
  for (unsigned k = 0; k < N; k++)
    wnd[k] = sqrt(0.5 * (1 - cos(2 * pi * k / N)) / N);

  // set default parameters
  SetCircularWrap(90);
  SetShift(0);
  SetDepth(1);
  SetFocus(0);
  SetCenterImage(1);
  SetFrontSeparation(1);
  SetRearSeparation(1);
  SetLowCutoff(40.0f / 22050.0f);
  SetHighCutoff(90.0f / 22050.0f);
  SetBassRedirection(false);
}

FreeSurroundDecoder::~FreeSurroundDecoder()
{
  kiss_fftr_free(forward);
  kiss_fftr_free(inverse);
}

// decode a stereo chunk, produces a multichannel chunk of the same size (lagged)
float* FreeSurroundDecoder::Decode(float* input)
{
  // append incoming data to the end of the input buffer
  memcpy(&inbuf[N], &input[0], 8 * N);
  // process first and second half, overlapped
  BufferedDecode(&inbuf[0]);
  BufferedDecode(&inbuf[N]);
  // shift last half of the input to the beginning (for overlapping with a future block)
  memcpy(&inbuf[0], &inbuf[2 * N], 4 * N);
  buffer_empty = false;
  return &outbuf[0];
}

// flush the internal buffers
void FreeSurroundDecoder::Flush()
{
  memset(&outbuf[0], 0, outbuf.size() * 4);
  memset(&inbuf[0], 0, inbuf.size() * 4);
  buffer_empty = true;
}

// number of samples currently held in the buffer
unsigned FreeSurroundDecoder::GetSamplesBuffered()
{
  return buffer_empty ? 0 : N / 2;
}

// set soundfield & rendering parameters
void FreeSurroundDecoder::SetCircularWrap(float v)
{
  circular_wrap = v;
}
void FreeSurroundDecoder::SetShift(float v)
{
  shift = v;
}
void FreeSurroundDecoder::SetDepth(float v)
{
  depth = v;
}
void FreeSurroundDecoder::SetFocus(float v)
{
  focus = v;
}
void FreeSurroundDecoder::SetCenterImage(float v)
{
  center_image = v;
}
void FreeSurroundDecoder::SetFrontSeparation(float v)
{
  front_separation = v;
}
void FreeSurroundDecoder::SetRearSeparation(float v)
{
  rear_separation = v;
}
void FreeSurroundDecoder::SetLowCutoff(float v)
{
  lo_cut = v * (N / 2);
}
void FreeSurroundDecoder::SetHighCutoff(float v)
{
  hi_cut = v * (N / 2);
}
void FreeSurroundDecoder::SetBassRedirection(bool v)
{
  use_lfe = v;
}

// decode a block of data and overlap-add it into outbuf
void FreeSurroundDecoder::BufferedDecode(float* input)
{
  // demultiplex and apply window function
  for (unsigned k = 0; k < N; k++)
  {
    lt[k] = wnd[k] * input[k * 2 + 0];
    rt[k] = wnd[k] * input[k * 2 + 1];
  }

  // map into spectral domain
  kiss_fftr(forward, &lt[0], (kiss_fft_cpx*)&lf[0]);
  kiss_fftr(forward, &rt[0], (kiss_fft_cpx*)&rf[0]);

  // compute multichannel output signal in the spectral domain
  for (unsigned f = 1; f < N / 2; f++)
  {
    // get Lt/Rt amplitudes & phases
    double ampL = amplitude(lf[f]), ampR = amplitude(rf[f]);
    double phaseL = phase(lf[f]), phaseR = phase(rf[f]);
    // calculate the amplitude & phase differences
    double ampDiff = clamp1((ampL + ampR < epsilon) ? 0 : (ampR - ampL) / (ampR + ampL));
    double phaseDiff = abs(phaseL - phaseR);
    if (phaseDiff > pi)
      phaseDiff = 2 * pi - phaseDiff;

    // decode into x/y soundfield position
    double x, y;
    transform_decode(ampDiff, phaseDiff, x, y);
    // add wrap control
    transform_circular_wrap(x, y, circular_wrap);
    // add shift control
    y = clamp1(y - shift);
    // add depth control
    y = clamp1(1 - (1 - y) * depth);
    // add focus control
    transform_focus(x, y, focus);
    // add crossfeed control
    x = clamp1(x * (front_separation * (1 + y) / 2 + rear_separation * (1 - y) / 2));

    // get total signal amplitude
    double amp_total = sqrt(ampL * ampL + ampR * ampR);
    // and total L/C/R signal phases
    double phase_of[] = {phaseL, atan2(lf[f].imag() + rf[f].imag(), lf[f].real() + rf[f].real()), phaseR};
    // compute 2d channel map indexes p/q and update x/y to fractional offsets in the map grid
    int p = MapToGrid(x), q = MapToGrid(y);
    // map position to channel volumes
    for (unsigned c = 0; c < C - 1; c++)
    {
      // look up channel map at respective position (with bilinear interpolation) and build the signal
      const auto& a = cmap.luts[c];
      signal[c][f] = polar(amp_total * ((1 - x) * (1 - y) * a[q][p] + x * (1 - y) * a[q][p + 1] +
                                        (1 - x) * y * a[q + 1][p] + x * y * a[q + 1][p + 1]),
                           phase_of[1 + (int)sign(cmap.xsf[c])]);
    }

    // optionally redirect bass
    if (use_lfe && f < hi_cut)
    {
      // level of LFE channel according to normalized frequency
      double lfe_level = f < lo_cut ? 1 : 0.5 * (1 + cos(pi * (f - lo_cut) / (hi_cut - lo_cut)));
      // assign LFE channel
      signal[C - 1][f] = lfe_level * polar(amp_total, phase_of[1]);
      // subtract the signal from the other channels
      for (unsigned c = 0; c < C - 1; c++)
        signal[c][f] *= (1 - lfe_level);
    }
  }

  // shift the last 2/3 to the first 2/3 of the output buffer
  memmove(&outbuf[0], &outbuf[C * N / 2], N * C * 4);
  // and clear the rest
  memset(&outbuf[C * N], 0, C * 4 * N / 2);
  // backtransform each channel and overlap-add
  for (unsigned c = 0; c < C; c++)
  {
    // back-transform into time domain
    kiss_fftri(inverse, (kiss_fft_cpx*)&signal[c][0], &dst[0]);
    // add the result to the last 2/3 of the output buffer, windowed (and remultiplex)
    for (unsigned k = 0; k < N; k++)
      outbuf[C * (k + N / 2) + c] = static_cast<float>(outbuf[C * (k + N / 2) + c] + (wnd[k] * dst[k]));
  }
}

// transform amp/phase difference space into x/y soundfield space
void transform_decode(double a, double p, double& x, double& y)
{
  x = clamp1(1.0047 * a + 0.46804 * a * p * p * p - 0.2042 * a * p * p * p * p +
             0.0080586 * a * p * p * p * p * p * p * p - 0.0001526 * a * p * p * p * p * p * p * p * p * p * p -
             0.073512 * a * a * a * p - 0.2499 * a * a * a * p * p * p * p +
             0.016932 * a * a * a * p * p * p * p * p * p * p -
             0.00027707 * a * a * a * p * p * p * p * p * p * p * p * p * p +
             0.048105 * a * a * a * a * a * p * p * p * p * p * p * p -
             0.0065947 * a * a * a * a * a * p * p * p * p * p * p * p * p * p * p +
             0.0016006 * a * a * a * a * a * p * p * p * p * p * p * p * p * p * p * p -
             0.0071132 * a * a * a * a * a * a * a * p * p * p * p * p * p * p * p * p +
             0.0022336 * a * a * a * a * a * a * a * p * p * p * p * p * p * p * p * p * p * p -
             0.0004804 * a * a * a * a * a * a * a * p * p * p * p * p * p * p * p * p * p * p * p);
  y = clamp1(0.98592 - 0.62237 * p + 0.077875 * p * p - 0.0026929 * p * p * p * p * p + 0.4971 * a * a * p -
             0.00032124 * a * a * p * p * p * p * p * p +
             9.2491e-006 * a * a * a * a * p * p * p * p * p * p * p * p * p * p +
             0.051549 * a * a * a * a * a * a * a * a + 1.0727e-014 * a * a * a * a * a * a * a * a * a * a);
}

// apply a circular_wrap transformation to some position
void transform_circular_wrap(double& x, double& y, double refangle)
{
  if (refangle == 90)
    return;
  refangle = refangle * pi / 180;
  double baseangle = 90 * pi / 180;
  // translate into edge-normalized polar coordinates
  double ang = atan2(x, y), len = sqrt(x * x + y * y);
  len = len / edgedistance(ang);
  // apply circular_wrap transform
  if (abs(ang) < baseangle / 2)
    // angle falls within the front region (to be enlarged)
    ang *= refangle / baseangle;
  else
    // angle falls within the rear region (to be shrunken)
    ang = pi - (-(((refangle - 2 * pi) * (pi - abs(ang)) * sign(ang)) / (2 * pi - baseangle)));
  // translate back into soundfield position
  len = len * edgedistance(ang);
  x = clamp1(sin(ang) * len);
  y = clamp1(cos(ang) * len);
}

// apply a focus transformation to some position
void transform_focus(double& x, double& y, double focus)
{
  if (focus == 0)
    return;
  // translate into edge-normalized polar coordinates
  double ang = atan2(x, y), len = clamp1(sqrt(x * x + y * y) / edgedistance(ang));
  // apply focus
  len = focus > 0 ? 1 - pow(1 - len, 1 + focus * 20) : pow(len, 1 - focus * 20);
  // back-transform into euclidian soundfield position
  len = len * edgedistance(ang);
  x = clamp1(sin(ang) * len);
  y = clamp1(cos(ang) * len);
}
