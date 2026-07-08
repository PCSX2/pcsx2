/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SYNTHMARK_SYNTHTOOLS_H
#define SYNTHMARK_SYNTHTOOLS_H

#include <cmath>
#include <cstdint>

namespace marksynth {
typedef float synth_float_t;

// The number of frames that are synthesized at one time.
constexpr int kSynthmarkFramesPerRender  =  8;

constexpr int kSynthmarkSampleRate = 48000;
constexpr int kSynthmarkMaxVoices   = 1024;

/**
 * A fractional amplitude corresponding to exactly -96 dB.
 * amplitude = pow(10.0, db/20.0)
 */
constexpr double kAmplitudeDb96 = 1.0 / 63095.73444801943;

/** A fraction that is approximately -90.3 dB. Defined as 1 bit of an S16. */
constexpr double kAmplitudeDb90 = 1.0 / (1 << 15);

class SynthTools
{
public:

    static void fillBuffer(synth_float_t *output,
                                  int32_t numSamples,
                                  synth_float_t value) {
        for (int i = 0; i < numSamples; i++) {
            *output++ = value;
        }
    }

    static void scaleBuffer(const synth_float_t *input,
                                  synth_float_t *output,
                                  int32_t numSamples,
                                  synth_float_t multiplier) {
        for (int i = 0; i < numSamples; i++) {
            *output++ = *input++ * multiplier;
        }
    }

    static void scaleOffsetBuffer(const synth_float_t *input,
                                  synth_float_t *output,
                                  int32_t numSamples,
                                  synth_float_t multiplier,
                                  synth_float_t offset) {
        for (int i = 0; i < numSamples; i++) {
            *output++ = (*input++ * multiplier) + offset;
        }
    }

    static void mixBuffers(const synth_float_t *input1,
                           synth_float_t gain1,
                           const synth_float_t *input2,
                           synth_float_t gain2,
                           synth_float_t *output,
                           int32_t numSamples) {
        for (int i = 0; i < numSamples; i++) {
            *output++ = (*input1++ * gain1) + (*input2++ * gain2);
        }
    }

    static void multiplyBuffers(const synth_float_t *input1,
                                       const synth_float_t *input2,
                                       synth_float_t *output,
                                       int32_t numSamples) {
        for (int i = 0; i < numSamples; i++) {
            *output++ = *input1++ * *input2;
        }
    }

    static double convertTimeToExponentialScaler(synth_float_t duration, synth_float_t sampleRate) {
        // Calculate scaler so that scaler^frames = target/source
        synth_float_t numFrames = duration * sampleRate;
        return pow(kAmplitudeDb90, (1.0 / numFrames));
    }

    /**
     * Calculate sine using a Taylor expansion.
     * Code is based on SineOscillator from JSyn.
     *
     * @param phase between -PI and +PI
     */
    static synth_float_t fastSine(synth_float_t phase) {
        // Factorial coefficients.
        const synth_float_t IF3 = 1.0 / (2 * 3);
        const synth_float_t IF5 = IF3 / (4 * 5);
        const synth_float_t IF7 = IF5 / (6 * 7);
        const synth_float_t IF9 = IF7 / (8 * 9);
        const synth_float_t IF11 = IF9 / (10 * 11);

        /* Wrap phase back into region where results are more accurate. */
        synth_float_t x = (phase > M_PI_2) ? M_PI - phase
        : ((phase < -M_PI_2) ? -(M_PI + phase) : phase);

        synth_float_t x2 = (x * x);
        /* Taylor expansion out to x**11/11! factored into multiply-adds */
        return x * (x2 * (x2 * (x2 * (x2 * ((x2 * (-IF11)) + IF9) - IF7) + IF5) - IF3) + 1);
    }

    /**
     * Calculate cosine using a Taylor expansion.
     *
     * @param phase between -PI and +PI
     */
    static synth_float_t fastCosine(synth_float_t phase) {
        // Factorial coefficients.
        const synth_float_t IF2 = 1.0 / (2);
        const synth_float_t IF4 = IF2 / (3 * 4);
        const synth_float_t IF6 = IF4 / (5 * 6);
        const synth_float_t IF8 = IF6 / (7 * 8);
        const synth_float_t IF10 = IF8 / (9 * 10);

        /* Wrap phase back into region where results are more accurate. */
        synth_float_t x = phase;
        if (x < 0.0) {
             x = 0.0 - phase;
        }
        int negate = 1;
        if (x > M_PI_2) {
            x = M_PI_2 - x;
            negate = -1;
        }

        synth_float_t x2 = (x * x);
        /* Taylor expansion out to x**11/11! factored into multiply-adds */
        synth_float_t cosine =
                1 + (x2 * (x2 * (x2 * (x2 * ((x2 * (-IF10)) + IF8) - IF6) + IF4) - IF2));
        return cosine * negate;
    }


    /**
     * Calculate random 32 bit number using linear-congruential method.
     */
    static uint32_t nextRandomInteger() {
        static uint64_t seed = 99887766;
        // Use values for 64-bit sequence from MMIX by Donald Knuth.
        seed = (seed * 6364136223846793005L) + 1442695040888963407L;
        return (uint32_t) (seed >> 32); // The higher bits have a longer sequence.
    }

    /**
     * @return a random double between 0.0 and 1.0
     */
    static double nextRandomDouble() {
        const double scaler = 1.0 / (((uint64_t)1) << 32);
        return nextRandomInteger() * scaler;
    }

};
};
#endif // SYNTHMARK_SYNTHTOOLS_H
