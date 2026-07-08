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
 *
 * This code was translated from the JSyn Java code.
 * JSyn is Copyright 2009 Phil Burk, Mobileer Inc
 * JSyn is licensed under the Apache License, Version 2.0
 */

#ifndef SYNTHMARK_BIQUAD_FILTER_H
#define SYNTHMARK_BIQUAD_FILTER_H

#include <cstdint>
#include <math.h>
#include "SynthTools.h"
#include "UnitGenerator.h"

namespace marksynth {

#define BIQUAD_MIN_FREQ      (0.00001f) // REVIEW
#define BIQUAD_MIN_Q         (0.00001f) // REVIEW

#define RECALCULATE_PER_SAMPLE   0

/**
 * Time varying lowpass resonant filter.
 */
class BiquadFilter : public UnitGenerator
{
public:
    BiquadFilter()
    : mQ(1.0)
    {
        xn1 = xn2 = yn1 = yn2 = (synth_float_t) 0;
        a0 = a1 = a2 = b1 = b2 = (synth_float_t) 0;
    }

    virtual ~BiquadFilter() = default;

    /**
     * Resonance, typically between 1.0 and 10.0.
     * Input will clipped at a BIQUAD_MIN_Q.
     */
    void setQ(synth_float_t q) {
        if( q < BIQUAD_MIN_Q ) {
            q = BIQUAD_MIN_Q;
        }
        mQ = q;
    }

    synth_float_t getQ() {
        return mQ;
    }

    void generate(synth_float_t *input,
                  synth_float_t *frequencies,
                  int32_t numSamples) {
        synth_float_t xn, yn;

#if RECALCULATE_PER_SAMPLE == 0
        calculateCoefficients(frequencies[0], mQ);
#endif
        for (int i = 0; i < numSamples; i++) {
#if RECALCULATE_PER_SAMPLE == 1
                calculateCoefficients(frequencies[i], mQ);
#endif
            // Generate outputs by filtering inputs.
            xn = input[i];
            synth_float_t finite = (a0 * xn) + (a1 * xn1) + (a2 * xn2);
            // Use double precision for recursive portion.
            yn = finite - (b1 * yn1) - (b2 * yn2);
            output[i] = (synth_float_t) yn;

            // Delay input and output values.
            xn2 = xn1;
            xn1 = xn;
            yn2 = yn1;
            yn1 = yn;
        }

        // Apply a small bipolar impulse to filter to prevent arithmetic underflow.
        yn1 += (synth_float_t) 1.0E-26;
        yn2 -= (synth_float_t) 1.0E-26;
    }


private:
    synth_float_t      mQ;

    synth_float_t      xn1;    // delay lines
    synth_float_t      xn2;
    double             yn1;
    double             yn2;

    synth_float_t      a0;    // coefficients
    synth_float_t      a1;
    synth_float_t      a2;

    synth_float_t      b1;
    synth_float_t      b2;

    synth_float_t      cos_omega;
    synth_float_t      sin_omega;
    synth_float_t      alpha;

    // Calculate coefficients common to many parametric biquad filters.
    void calcCommon( synth_float_t ratio, synth_float_t Q )
    {
        synth_float_t omega;

        /* Don't let frequency get too close to Nyquist or filter will blow up. */
        if( ratio >= 0.499f ) ratio = 0.499f;
        omega = 2.0f * (synth_float_t)M_PI * ratio;

#if 1
        // This is not significantly faster on Mac or Linux.
        cos_omega = SynthTools::fastCosine(omega);
        sin_omega = SynthTools::fastSine(omega );
#else
        {
            float fsin_omega;
            float fcos_omega;
            sincosf(omega, &fsin_omega, &fcos_omega);
            cos_omega = (synth_float_t) fcos_omega;
            sin_omega = (synth_float_t) fsin_omega;
        }
#endif
        alpha = sin_omega / (2.0f * Q);
    }

    // Lowpass coefficients
    void calculateCoefficients( synth_float_t frequency, synth_float_t Q )
    {
        synth_float_t    scalar, omc;

        if( frequency  < BIQUAD_MIN_FREQ )  frequency  = BIQUAD_MIN_FREQ;

        calcCommon( frequency * mSamplePeriod, Q );

        scalar = 1.0f / (1.0f + alpha);
        omc = (1.0f - cos_omega);

        a0 = omc * 0.5f * scalar;
        a1 = omc * scalar;
        a2 = a0;
        b1 = -2.0f * cos_omega * scalar;
        b2 = (1.0f - alpha) * scalar;
    }
};

};
#endif // SYNTHMARK_BIQUAD_FILTER_H
