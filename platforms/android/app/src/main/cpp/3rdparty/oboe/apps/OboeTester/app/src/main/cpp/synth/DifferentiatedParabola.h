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

#ifndef SYNTHMARK_DIFFERENTIATED_PARABOLA_H
#define SYNTHMARK_DIFFERENTIATED_PARABOLA_H

#include <cstdint>
#include <math.h>
#include "SynthTools.h"

namespace marksynth {

constexpr double kDPWVeryLowFrequency  = 2.0 * 0.1 / kSynthmarkSampleRate;

/**
 * DPW is a tool for generating band-limited waveforms
 * based on a paper by Antti Huovilainen and Vesa Valimaki:
 *  "New Approaches to Digital Subtractive Synthesis"
 */
class DifferentiatedParabola
{
public:
    DifferentiatedParabola()
    : mZ1(0)
    , mZ2(0) {}

    virtual ~DifferentiatedParabola() = default;

    synth_float_t next(synth_float_t phase, synth_float_t phaseIncrement) {
        synth_float_t dpw;
        synth_float_t positivePhaseIncrement = (phaseIncrement < 0.0)
                ? phaseIncrement
                : 0.0 - phaseIncrement;

        // If the frequency is very low then just use the raw sawtooth.
        // This avoids divide by zero problems and scaling problems.
        if (positivePhaseIncrement < kDPWVeryLowFrequency) {
            dpw = phase;
        } else {
            // Calculate the parabola.
            synth_float_t squared = phase * phase;
            // Differentiate using a delayed value.
            synth_float_t diffed = squared - mZ2;
            // Delay line.
            // TODO - Why Z2. Vesa's paper says use Z1?
            mZ2 = mZ1;
            mZ1 = squared;

            // Calculate scaling
            dpw = diffed * 0.25f / positivePhaseIncrement; // TODO extract and optimize
        }
        return dpw;
    }

private:
    synth_float_t mZ1;
    synth_float_t mZ2;
};

};
#endif // SYNTHMARK_DIFFERENTIATED_PARABOLA_H
