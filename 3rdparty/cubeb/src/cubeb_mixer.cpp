/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 *
 * Adapted from code based on libswresample's rematrix.c
 */

#define NOMINMAX

#include "cubeb_mixer.h"
#include "cubeb-internal.h"
#include "cubeb_utils.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <type_traits>

#ifndef FF_ARRAY_ELEMS
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define CHANNELS_MAX 32
#define FRONT_LEFT 0
#define FRONT_RIGHT 1
#define FRONT_CENTER 2
#define LOW_FREQUENCY 3
#define BACK_LEFT 4
#define BACK_RIGHT 5
#define FRONT_LEFT_OF_CENTER 6
#define FRONT_RIGHT_OF_CENTER 7
#define BACK_CENTER 8
#define SIDE_LEFT 9
#define SIDE_RIGHT 10
#define TOP_CENTER 11
#define TOP_FRONT_LEFT 12
#define TOP_FRONT_CENTER 13
#define TOP_FRONT_RIGHT 14
#define TOP_BACK_LEFT 15
#define TOP_BACK_CENTER 16
#define TOP_BACK_RIGHT 17
#define NUM_NAMED_CHANNELS 18

#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440 /* 1/sqrt(2) */
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880 /* sqrt(2) */
#endif
#define SQRT3_2 1.22474487139158904909 /* sqrt(3/2) */

#define C30DB M_SQRT2
#define C15DB 1.189207115
#define C__0DB 1.0
#define C_15DB 0.840896415
#define C_30DB M_SQRT1_2
#define C_45DB 0.594603558
#define C_60DB 0.5

static cubeb_channel_layout
cubeb_channel_layout_check(cubeb_channel_layout l, uint32_t c)
{
  if (l == CUBEB_LAYOUT_UNDEFINED) {
    switch (c) {
    case 1:
      return CUBEB_LAYOUT_MONO;
    case 2:
      return CUBEB_LAYOUT_STEREO;
    }
  }
  return l;
}

unsigned int
cubeb_channel_layout_nb_channels(cubeb_channel_layout x)
{
#if __GNUC__ || __clang__
  return __builtin_popcount(x);
#else
  x -= (x >> 1) & 0x55555555;
  x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  x = (x + (x >> 4)) & 0x0F0F0F0F;
  x += x >> 8;
  return (x + (x >> 16)) & 0x3F;
#endif
}

struct MixerContext {
  MixerContext(cubeb_sample_format f, uint32_t in_channels,
               cubeb_channel_layout in, uint32_t out_channels,
               cubeb_channel_layout out)
      : _format(f), _in_ch_layout(cubeb_channel_layout_check(in, in_channels)),
        _out_ch_layout(cubeb_channel_layout_check(out, out_channels)),
        _in_ch_count(in_channels), _out_ch_count(out_channels)
  {
    if (in_channels != cubeb_channel_layout_nb_channels(in) ||
        out_channels != cubeb_channel_layout_nb_channels(out)) {
      // Mismatch between channels and layout, aborting.
      return;
    }
    _valid = init() >= 0;
  }

  static bool even(cubeb_channel_layout layout)
  {
    if (!layout) {
      return true;
    }
    if (layout & (layout - 1)) {
      return true;
    }
    return false;
  }

  // Ensure that the layout is sane (that is have symmetrical left/right
  // channels), if not, layout will be treated as mono.
  static cubeb_channel_layout clean_layout(cubeb_channel_layout layout)
  {
    if (layout && layout != CHANNEL_FRONT_LEFT && !(layout & (layout - 1))) {
      LOG("Treating layout as mono");
      return CHANNEL_FRONT_CENTER;
    }

    return layout;
  }

  static bool sane_layout(cubeb_channel_layout layout)
  {
    if (!(layout & CUBEB_LAYOUT_3F)) { // at least 1 front speaker
      return false;
    }
    if (!even(layout & (CHANNEL_FRONT_LEFT |
                        CHANNEL_FRONT_RIGHT))) { // no asymetric front
      return false;
    }
    if (!even(layout &
              (CHANNEL_SIDE_LEFT | CHANNEL_SIDE_RIGHT))) { // no asymetric side
      return false;
    }
    if (!even(layout & (CHANNEL_BACK_LEFT | CHANNEL_BACK_RIGHT))) {
      return false;
    }
    if (!even(layout &
              (CHANNEL_FRONT_LEFT_OF_CENTER | CHANNEL_FRONT_RIGHT_OF_CENTER))) {
      return false;
    }
    if (cubeb_channel_layout_nb_channels(layout) >= CHANNELS_MAX) {
      return false;
    }
    return true;
  }

  int auto_matrix();
  int init();

  const cubeb_sample_format _format;
  const cubeb_channel_layout _in_ch_layout;  ///< input channel layout
  const cubeb_channel_layout _out_ch_layout; ///< output channel layout
  const uint32_t _in_ch_count;               ///< input channel count
  const uint32_t _out_ch_count;              ///< output channel count
  const float _surround_mix_level = C_30DB;  ///< surround mixing level
  const float _center_mix_level = C_30DB;    ///< center mixing level
  const float _lfe_mix_level = 1;            ///< LFE mixing level
  double _matrix[CHANNELS_MAX][CHANNELS_MAX] = {
      {0}}; ///< floating point rematrixing coefficients
  float _matrix_flt[CHANNELS_MAX][CHANNELS_MAX] = {
      {0}}; ///< single precision floating point rematrixing coefficients
  int32_t _matrix32[CHANNELS_MAX][CHANNELS_MAX] = {
      {0}}; ///< 17.15 fixed point rematrixing coefficients
  uint8_t _matrix_ch[CHANNELS_MAX][CHANNELS_MAX + 1] = {
      {0}}; ///< Lists of input channels per output channel that have non zero
            ///< rematrixing coefficients
  bool _clipping = false; ///< Set to true if clipping detection is required
  bool _valid = false;    ///< Set to true if context is valid.
};

int
MixerContext::auto_matrix()
{
  double matrix[NUM_NAMED_CHANNELS][NUM_NAMED_CHANNELS] = {{0}};
  double maxcoef = 0;
  float maxval;

  cubeb_channel_layout in_ch_layout = clean_layout(_in_ch_layout);
  cubeb_channel_layout out_ch_layout = clean_layout(_out_ch_layout);

  if (!sane_layout(in_ch_layout)) {
    // Channel Not Supported
    LOG("Input Layout %x is not supported", _in_ch_layout);
    return -1;
  }

  if (!sane_layout(out_ch_layout)) {
    LOG("Output Layout %x is not supported", _out_ch_layout);
    return -1;
  }

  for (uint32_t i = 0; i < FF_ARRAY_ELEMS(matrix); i++) {
    if (in_ch_layout & out_ch_layout & (1U << i)) {
      matrix[i][i] = 1.0;
    }
  }

  cubeb_channel_layout unaccounted = in_ch_layout & ~out_ch_layout;

  // Rematrixing is done via a matrix of coefficient that should be applied to
  // all channels. Channels are treated as pair and must be symmetrical (if a
  // left channel exists, the corresponding right should exist too) unless the
  // output layout has similar layout. Channels are then mixed toward the front
  // center or back center if they exist with a slight bias toward the front.

  if (unaccounted & CHANNEL_FRONT_CENTER) {
    if ((out_ch_layout & CUBEB_LAYOUT_STEREO) == CUBEB_LAYOUT_STEREO) {
      if (in_ch_layout & CUBEB_LAYOUT_STEREO) {
        matrix[FRONT_LEFT][FRONT_CENTER] += _center_mix_level;
        matrix[FRONT_RIGHT][FRONT_CENTER] += _center_mix_level;
      } else {
        matrix[FRONT_LEFT][FRONT_CENTER] += M_SQRT1_2;
        matrix[FRONT_RIGHT][FRONT_CENTER] += M_SQRT1_2;
      }
    }
  }
  if (unaccounted & CUBEB_LAYOUT_STEREO) {
    if (out_ch_layout & CHANNEL_FRONT_CENTER) {
      matrix[FRONT_CENTER][FRONT_LEFT] += M_SQRT1_2;
      matrix[FRONT_CENTER][FRONT_RIGHT] += M_SQRT1_2;
      if (in_ch_layout & CHANNEL_FRONT_CENTER)
        matrix[FRONT_CENTER][FRONT_CENTER] = _center_mix_level * M_SQRT2;
    }
  }

  if (unaccounted & CHANNEL_BACK_CENTER) {
    if (out_ch_layout & CHANNEL_BACK_LEFT) {
      matrix[BACK_LEFT][BACK_CENTER] += M_SQRT1_2;
      matrix[BACK_RIGHT][BACK_CENTER] += M_SQRT1_2;
    } else if (out_ch_layout & CHANNEL_SIDE_LEFT) {
      matrix[SIDE_LEFT][BACK_CENTER] += M_SQRT1_2;
      matrix[SIDE_RIGHT][BACK_CENTER] += M_SQRT1_2;
    } else if (out_ch_layout & CHANNEL_FRONT_LEFT) {
      matrix[FRONT_LEFT][BACK_CENTER] += _surround_mix_level * M_SQRT1_2;
      matrix[FRONT_RIGHT][BACK_CENTER] += _surround_mix_level * M_SQRT1_2;
    } else if (out_ch_layout & CHANNEL_FRONT_CENTER) {
      matrix[FRONT_CENTER][BACK_CENTER] += _surround_mix_level * M_SQRT1_2;
    }
  }
  if (unaccounted & CHANNEL_BACK_LEFT) {
    if (out_ch_layout & CHANNEL_BACK_CENTER) {
      matrix[BACK_CENTER][BACK_LEFT] += M_SQRT1_2;
      matrix[BACK_CENTER][BACK_RIGHT] += M_SQRT1_2;
    } else if (out_ch_layout & CHANNEL_SIDE_LEFT) {
      if (in_ch_layout & CHANNEL_SIDE_LEFT) {
        matrix[SIDE_LEFT][BACK_LEFT] += M_SQRT1_2;
        matrix[SIDE_RIGHT][BACK_RIGHT] += M_SQRT1_2;
      } else {
        matrix[SIDE_LEFT][BACK_LEFT] += 1.0;
        matrix[SIDE_RIGHT][BACK_RIGHT] += 1.0;
      }
    } else if (out_ch_layout & CHANNEL_FRONT_LEFT) {
      matrix[FRONT_LEFT][BACK_LEFT] += _surround_mix_level;
      matrix[FRONT_RIGHT][BACK_RIGHT] += _surround_mix_level;
    } else if (out_ch_layout & CHANNEL_FRONT_CENTER) {
      matrix[FRONT_CENTER][BACK_LEFT] += _surround_mix_level * M_SQRT1_2;
      matrix[FRONT_CENTER][BACK_RIGHT] += _surround_mix_level * M_SQRT1_2;
    }
  }

  if (unaccounted & CHANNEL_SIDE_LEFT) {
    if (out_ch_layout & CHANNEL_BACK_LEFT) {
      /* if back channels do not exist in the input, just copy side
         channels to back channels, otherwise mix side into back */
      if (in_ch_layout & CHANNEL_BACK_LEFT) {
        matrix[BACK_LEFT][SIDE_LEFT] += M_SQRT1_2;
        matrix[BACK_RIGHT][SIDE_RIGHT] += M_SQRT1_2;
      } else {
        matrix[BACK_LEFT][SIDE_LEFT] += 1.0;
        matrix[BACK_RIGHT][SIDE_RIGHT] += 1.0;
      }
    } else if (out_ch_layout & CHANNEL_BACK_CENTER) {
      matrix[BACK_CENTER][SIDE_LEFT] += M_SQRT1_2;
      matrix[BACK_CENTER][SIDE_RIGHT] += M_SQRT1_2;
    } else if (out_ch_layout & CHANNEL_FRONT_LEFT) {
      matrix[FRONT_LEFT][SIDE_LEFT] += _surround_mix_level;
      matrix[FRONT_RIGHT][SIDE_RIGHT] += _surround_mix_level;
    } else if (out_ch_layout & CHANNEL_FRONT_CENTER) {
      matrix[FRONT_CENTER][SIDE_LEFT] += _surround_mix_level * M_SQRT1_2;
      matrix[FRONT_CENTER][SIDE_RIGHT] += _surround_mix_level * M_SQRT1_2;
    }
  }

  if (unaccounted & CHANNEL_FRONT_LEFT_OF_CENTER) {
    if (out_ch_layout & CHANNEL_FRONT_LEFT) {
      matrix[FRONT_LEFT][FRONT_LEFT_OF_CENTER] += 1.0;
      matrix[FRONT_RIGHT][FRONT_RIGHT_OF_CENTER] += 1.0;
    } else if (out_ch_layout & CHANNEL_FRONT_CENTER) {
      matrix[FRONT_CENTER][FRONT_LEFT_OF_CENTER] += M_SQRT1_2;
      matrix[FRONT_CENTER][FRONT_RIGHT_OF_CENTER] += M_SQRT1_2;
    }
  }
  /* mix LFE into front left/right or center */
  if (unaccounted & CHANNEL_LOW_FREQUENCY) {
    if (out_ch_layout & CHANNEL_FRONT_CENTER) {
      matrix[FRONT_CENTER][LOW_FREQUENCY] += _lfe_mix_level;
    } else if (out_ch_layout & CHANNEL_FRONT_LEFT) {
      matrix[FRONT_LEFT][LOW_FREQUENCY] += _lfe_mix_level * M_SQRT1_2;
      matrix[FRONT_RIGHT][LOW_FREQUENCY] += _lfe_mix_level * M_SQRT1_2;
    }
  }

  // Normalize the conversion matrix.
  for (uint32_t out_i = 0, i = 0; i < CHANNELS_MAX; i++) {
    double sum = 0;
    int in_i = 0;
    if ((out_ch_layout & (1U << i)) == 0) {
      continue;
    }
    for (uint32_t j = 0; j < CHANNELS_MAX; j++) {
      if ((in_ch_layout & (1U << j)) == 0) {
        continue;
      }
      if (i < FF_ARRAY_ELEMS(matrix) && j < FF_ARRAY_ELEMS(matrix[0])) {
        _matrix[out_i][in_i] = matrix[i][j];
      } else {
        _matrix[out_i][in_i] =
            i == j && (in_ch_layout & out_ch_layout & (1U << i));
      }
      sum += fabs(_matrix[out_i][in_i]);
      in_i++;
    }
    maxcoef = std::max(maxcoef, sum);
    out_i++;
  }

  if (_format == CUBEB_SAMPLE_S16NE) {
    maxval = 1.0;
  } else {
    maxval = INT_MAX;
  }

  // Normalize matrix if needed.
  if (maxcoef > maxval) {
    maxcoef /= maxval;
    for (uint32_t i = 0; i < CHANNELS_MAX; i++)
      for (uint32_t j = 0; j < CHANNELS_MAX; j++) {
        _matrix[i][j] /= maxcoef;
      }
  }

  if (_format == CUBEB_SAMPLE_FLOAT32NE) {
    for (uint32_t i = 0; i < FF_ARRAY_ELEMS(_matrix); i++) {
      for (uint32_t j = 0; j < FF_ARRAY_ELEMS(_matrix[0]); j++) {
        _matrix_flt[i][j] = _matrix[i][j];
      }
    }
  }

  return 0;
}

int
MixerContext::init()
{
  int r = auto_matrix();
  if (r) {
    return r;
  }

  // Determine if matrix operation would overflow
  if (_format == CUBEB_SAMPLE_S16NE) {
    int maxsum = 0;
    for (uint32_t i = 0; i < _out_ch_count; i++) {
      double rem = 0;
      int sum = 0;

      for (uint32_t j = 0; j < _in_ch_count; j++) {
        double target = _matrix[i][j] * 32768 + rem;
        int value = lrintf(target);
        rem += target - value;
        sum += std::abs(value);
      }
      maxsum = std::max(maxsum, sum);
    }
    if (maxsum > 32768) {
      _clipping = true;
    }
  }

  // FIXME quantize for integers
  for (uint32_t i = 0; i < CHANNELS_MAX; i++) {
    int ch_in = 0;
    for (uint32_t j = 0; j < CHANNELS_MAX; j++) {
      _matrix32[i][j] = lrintf(_matrix[i][j] * 32768);
      if (_matrix[i][j]) {
        _matrix_ch[i][++ch_in] = j;
      }
    }
    _matrix_ch[i][0] = ch_in;
  }

  return 0;
}

template <typename TYPE_SAMPLE, typename TYPE_COEFF, typename F>
void
sum2(TYPE_SAMPLE * out, uint32_t stride_out, const TYPE_SAMPLE * in1,
     const TYPE_SAMPLE * in2, uint32_t stride_in, TYPE_COEFF coeff1,
     TYPE_COEFF coeff2, F && operand, uint32_t frames)
{
  static_assert(
      std::is_same<TYPE_COEFF, decltype(operand(coeff1))>::value,
      "function must return the same type as used by coeff1 and coeff2");
  for (uint32_t i = 0; i < frames; i++) {
    *out = operand(coeff1 * *in1 + coeff2 * *in2);
    out += stride_out;
    in1 += stride_in;
    in2 += stride_in;
  }
}

template <typename TYPE_SAMPLE, typename TYPE_COEFF, typename F>
void
copy(TYPE_SAMPLE * out, uint32_t stride_out, const TYPE_SAMPLE * in,
     uint32_t stride_in, TYPE_COEFF coeff, F && operand, uint32_t frames)
{
  static_assert(std::is_same<TYPE_COEFF, decltype(operand(coeff))>::value,
                "function must return the same type as used by coeff");
  for (uint32_t i = 0; i < frames; i++) {
    *out = operand(coeff * *in);
    out += stride_out;
    in += stride_in;
  }
}

template <typename TYPE, typename TYPE_COEFF, size_t COLS, typename F>
static int
rematrix(const MixerContext * s, TYPE * aOut, const TYPE * aIn,
         const TYPE_COEFF (&matrix_coeff)[COLS][COLS], F && aF, uint32_t frames)
{
  static_assert(
      std::is_same<TYPE_COEFF, decltype(aF(matrix_coeff[0][0]))>::value,
      "function must return the same type as used by matrix_coeff");

  for (uint32_t out_i = 0; out_i < s->_out_ch_count; out_i++) {
    TYPE * out = aOut + out_i;
    switch (s->_matrix_ch[out_i][0]) {
    case 0:
      for (uint32_t i = 0; i < frames; i++) {
        out[i * s->_out_ch_count] = 0;
      }
      break;
    case 1: {
      int in_i = s->_matrix_ch[out_i][1];
      copy(out, s->_out_ch_count, aIn + in_i, s->_in_ch_count,
           matrix_coeff[out_i][in_i], aF, frames);
    } break;
    case 2:
      sum2(out, s->_out_ch_count, aIn + s->_matrix_ch[out_i][1],
           aIn + s->_matrix_ch[out_i][2], s->_in_ch_count,
           matrix_coeff[out_i][s->_matrix_ch[out_i][1]],
           matrix_coeff[out_i][s->_matrix_ch[out_i][2]], aF, frames);
      break;
    default:
      for (uint32_t i = 0; i < frames; i++) {
        TYPE_COEFF v = 0;
        for (uint32_t j = 0; j < s->_matrix_ch[out_i][0]; j++) {
          uint32_t in_i = s->_matrix_ch[out_i][1 + j];
          v += *(aIn + in_i + i * s->_in_ch_count) * matrix_coeff[out_i][in_i];
        }
        out[i * s->_out_ch_count] = aF(v);
      }
      break;
    }
  }
  return 0;
}

struct cubeb_mixer {
  cubeb_mixer(cubeb_sample_format format, uint32_t in_channels,
              cubeb_channel_layout in_layout, uint32_t out_channels,
              cubeb_channel_layout out_layout)
      : _context(format, in_channels, in_layout, out_channels, out_layout)
  {
  }

  template <typename T>
  void copy_and_trunc(size_t frames, const T * input_buffer,
                      T * output_buffer) const
  {
    if (_context._in_ch_count <= _context._out_ch_count) {
      // Not enough channels to copy, fill the gaps with silence.
      if (_context._in_ch_count == 1 && _context._out_ch_count >= 2) {
        // Special case for upmixing mono input to stereo and more. We will
        // duplicate the mono channel to the first two channels. On most system,
        // the first two channels are for left and right. It is commonly
        // expected that mono will on both left+right channels
        for (uint32_t i = 0; i < frames; i++) {
          output_buffer[0] = output_buffer[1] = *input_buffer;
          PodZero(output_buffer + 2, _context._out_ch_count - 2);
          output_buffer += _context._out_ch_count;
          input_buffer++;
        }
        return;
      }
      for (uint32_t i = 0; i < frames; i++) {
        PodCopy(output_buffer, input_buffer, _context._in_ch_count);
        output_buffer += _context._in_ch_count;
        input_buffer += _context._in_ch_count;
        PodZero(output_buffer, _context._out_ch_count - _context._in_ch_count);
        output_buffer += _context._out_ch_count - _context._in_ch_count;
      }
    } else {
      for (uint32_t i = 0; i < frames; i++) {
        PodCopy(output_buffer, input_buffer, _context._out_ch_count);
        output_buffer += _context._out_ch_count;
        input_buffer += _context._in_ch_count;
      }
    }
  }

  int mix(size_t frames, const void * input_buffer, size_t input_buffer_size,
          void * output_buffer, size_t output_buffer_size) const
  {
    if (frames <= 0 || _context._out_ch_count == 0) {
      return 0;
    }

    // Check if output buffer is of sufficient size.
    size_t size_read_needed =
        frames * _context._in_ch_count * cubeb_sample_size(_context._format);
    if (input_buffer_size < size_read_needed) {
      // We don't have enough data to read!
      return -1;
    }
    if (output_buffer_size * _context._in_ch_count <
        size_read_needed * _context._out_ch_count) {
      return -1;
    }

    if (!valid()) {
      // The channel layouts were invalid or unsupported, instead we will simply
      // either drop the extra channels, or fill with silence the missing ones
      if (_context._format == CUBEB_SAMPLE_FLOAT32NE) {
        copy_and_trunc(frames, static_cast<const float *>(input_buffer),
                       static_cast<float *>(output_buffer));
      } else {
        assert(_context._format == CUBEB_SAMPLE_S16NE);
        copy_and_trunc(frames, static_cast<const int16_t *>(input_buffer),
                       reinterpret_cast<int16_t *>(output_buffer));
      }
      return 0;
    }

    switch (_context._format) {
    case CUBEB_SAMPLE_FLOAT32NE: {
      auto f = [](float x) { return x; };
      return rematrix(&_context, static_cast<float *>(output_buffer),
                      static_cast<const float *>(input_buffer),
                      _context._matrix_flt, f, frames);
    }
    case CUBEB_SAMPLE_S16NE:
      if (_context._clipping) {
        auto f = [](int x) {
          int y = (x + 16384) >> 15;
          // clip the signed integer value into the -32768,32767 range.
          if ((y + 0x8000U) & ~0xFFFF) {
            return (y >> 31) ^ 0x7FFF;
          }
          return y;
        };
        return rematrix(&_context, static_cast<int16_t *>(output_buffer),
                        static_cast<const int16_t *>(input_buffer),
                        _context._matrix32, f, frames);
      } else {
        auto f = [](int x) { return (x + 16384) >> 15; };
        return rematrix(&_context, static_cast<int16_t *>(output_buffer),
                        static_cast<const int16_t *>(input_buffer),
                        _context._matrix32, f, frames);
      }
      break;
    default:
      assert(false);
      break;
    }

    return -1;
  }

  // Return false if any of the input or ouput layout were invalid.
  bool valid() const { return _context._valid; }

  virtual ~cubeb_mixer(){};

  MixerContext _context;
};

cubeb_mixer *
cubeb_mixer_create(cubeb_sample_format format, uint32_t in_channels,
                   cubeb_channel_layout in_layout, uint32_t out_channels,
                   cubeb_channel_layout out_layout)
{
  return new cubeb_mixer(format, in_channels, in_layout, out_channels,
                         out_layout);
}

void
cubeb_mixer_destroy(cubeb_mixer * mixer)
{
  delete mixer;
}

int
cubeb_mixer_mix(cubeb_mixer * mixer, size_t frames, const void * input_buffer,
                size_t input_buffer_size, void * output_buffer,
                size_t output_buffer_size)
{
  return mixer->mix(frames, input_buffer, input_buffer_size, output_buffer,
                    output_buffer_size);
}
