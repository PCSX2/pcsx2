// SPDX-FileCopyrightText: 2007-2010 Christian Kothe, 2024 Connor McLaughlin <stenzek@gmail.com>
// SPDX-License-Identifier: GPL-2.0+

#pragma once

#include "kiss_fftr.h"

#include <array>
#include <cmath>
#include <complex>
#include <span>
#include <vector>

/**
 * The FreeSurround decoder.
 */
class FreeSurroundDecoder
{
public:
  /**
   * The supported output channel setups.
   * A channel setup is defined by the set of channels that are present. Here is a graphic
   * of the cs_5point1 setup: http://en.wikipedia.org/wiki/File:5_1_channels_(surround_sound)_label.svg
   */
  enum class ChannelSetup
  {
    Stereo,
    Surround41,
    Surround51,
    Surround71,
    Legacy, // same channels as cs_5point1 but different upmixing transform; does not support the focus control
    MaxCount
  };

  static constexpr int grid_res = 21; // resolution of the lookup grid
  using LUT = const float (*)[grid_res];

  /**
   * Create an instance of the decoder.
   * @param setup The output channel setup -- determines the number of output channels
   *			   and their place in the sound field.
   * @param blocksize Granularity at which data is processed by the decode() function.
   *				   Must be a power of two and should correspond to ca. 10ms worth of single-channel
   *				   samples (default is 4096 for 44.1Khz data). Do not make it shorter or longer
   *				   than 5ms to 20ms since the granularity at which locations are decoded
   *				   changes with this.
   */
  FreeSurroundDecoder(ChannelSetup setup = ChannelSetup::Surround51, unsigned blocksize = 4096);
  ~FreeSurroundDecoder();

  /**
   * Decode a chunk of stereo sound. The output is delayed by half of the blocksize.
   * This function is the only one needed for straightforward decoding.
   * @param input Contains exactly blocksize (multiplexed) stereo samples, i.e. 2*blocksize numbers.
   * @return A pointer to an internal buffer of exactly blocksize (multiplexed) multichannel samples.
   *		  The actual number of values depends on the number of output channels in the chosen
   *		  channel setup.
   */
  float* Decode(float* input);

  /**
   * Flush the internal buffer.
   */
  void Flush();

  // --- soundfield transformations
  // These functions allow to set up geometric transformations of the sound field after it has been decoded.
  // The sound field is best pictured as a 2-dimensional square with the listener in its
  // center which can be shifted or stretched in various ways before it is sent to the
  // speakers. The order in which these transformations are applied is as listed below.

  /**
   * Allows to wrap the soundfield around the listener in a circular manner.
   * Determines the angle of the frontal sound stage relative to the listener, in degrees.
   * A setting of 90° corresponds to standard surround decoding, 180° stretches the front stage from
   * ear to ear, 270° wraps it around most of the head. The side and rear content of the sound
   * field is compressed accordingly behind the listerer. (default: 90, range: [0°..360°])
   */
  void SetCircularWrap(float v);

  /**
   * Allows to shift the soundfield forward or backward.
   * Value range: [-1.0..+1.0]. 0 is no offset, positive values move the sound
   * forward, negative values move it backwards. (default: 0)
   */
  void SetShift(float v);

  /**
   * Allows to scale the soundfield backwards.
   * Value range: [0.0..+5.0] -- 0 is all compressed to the front, 1 is no change, 5 is scaled 5x backwards (default: 1)
   */
  void SetDepth(float v);

  /**
   * Allows to control the localization (i.e., focality) of sources.
   * Value range: [-1.0..+1.0] -- 0 means unchanged, positive means more localized, negative means more ambient
   * (default: 0)
   */
  void SetFocus(float v);

  // --- rendering parameters
  // These parameters control how the sound field is mapped onto speakers.

  /**
   * Set the presence of the front center channel(s).
   * Value range: [0.0..1.0] -- fully present at 1.0, fully replaced by left/right at 0.0 (default: 1).
   * The default of 1.0 results in spec-conformant decoding ("movie mode") while a value of 0.7 is
   * better suited for music reproduction (which is usually mixed without a center channel).
   */
  void SetCenterImage(float v);

  /**
   * Set the front stereo separation.
   * Value range: [0.0..inf] -- 1.0 is default, 0.0 is mono.
   */
  void SetFrontSeparation(float v);

  /**
   * Set the rear stereo separation.
   * Value range: [0.0..inf] -- 1.0 is default, 0.0 is mono.
   */
  void SetRearSeparation(float v);

  // --- bass redirection (to LFE)

  /**
   * Enable/disable LFE channel (default: false = disabled)
   */
  void SetBassRedirection(bool v);

  /**
   * Set the lower end of the transition band, in Hz/Nyquist (default: 40/22050).
   */
  void SetLowCutoff(float v);

  /**
   * Set the upper end of the transition band, in Hz/Nyquist (default: 90/22050).
   */
  void SetHighCutoff(float v);

  // --- info

  /**
   * Number of samples currently held in the buffer.
   */
  unsigned GetSamplesBuffered();

private:
  using cplx = std::complex<double>;

  struct ChannelMap
  {
    std::span<const LUT> luts;
    const float* xsf;
  };

  static const std::array<ChannelMap, static_cast<size_t>(ChannelSetup::MaxCount)> s_channel_maps;

  void BufferedDecode(float* input);

  // get the index (and fractional offset!) in a piecewise-linear channel allocation grid
  static int MapToGrid(double& x);

  // constants
  const ChannelMap& cmap; // the channel setup
  unsigned N, C;          // number of samples per input/output block, number of output channels

  // parameters
  float circular_wrap;    // angle of the front soundstage around the listener (90°=default)
  float shift;            // forward/backward offset of the soundstage
  float depth;            // backward extension of the soundstage
  float focus;            // localization of the sound events
  float center_image;     // presence of the center speaker
  float front_separation; // front stereo separation
  float rear_separation;  // rear stereo separation
  float lo_cut, hi_cut;   // LFE cutoff frequencies
  bool use_lfe;           // whether to use the LFE channel

  // FFT data structures
  std::vector<double> lt, rt, dst; // left total, right total (source arrays), time-domain destination buffer array
  std::vector<cplx> lf, rf;        // left total / right total in frequency domain
  kiss_fftr_cfg forward = nullptr;
  kiss_fftr_cfg inverse = nullptr; // FFT buffers

  // buffers
  bool buffer_empty = true;              // whether the buffer is currently empty or dirty
  std::vector<float> inbuf;              // stereo input buffer (multiplexed)
  std::vector<float> outbuf;             // multichannel output buffer (multiplexed)
  std::vector<double> wnd;               // the window function, precomputed
  std::vector<std::vector<cplx>> signal; // the signal to be constructed in every channel, in the frequency domain
};
