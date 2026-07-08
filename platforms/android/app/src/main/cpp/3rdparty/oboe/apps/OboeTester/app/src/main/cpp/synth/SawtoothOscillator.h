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

#ifndef SYNTHMARK_SAWTOOTH_OSCILLATOR_H
#define SYNTHMARK_SAWTOOTH_OSCILLATOR_H

#include <cstdint>
#include <math.h>
#include "SynthTools.h"
#include "UnitGenerator.h"
#include "DifferentiatedParabola.h"

namespace marksynth {
/**
 * Simple phasor that can be used to implement other oscillators.
 * Note that this is NON-bandlimited and should not be used
 * directly as a sound source.
 */
class SawtoothOscillator : public UnitGenerator
{
public:
    SawtoothOscillator()
    : mPhase(0) {}

    virtual ~SawtoothOscillator() = default;

    void generate(synth_float_t frequency, int32_t numSamples) {
        synth_float_t phase = mPhase;
        synth_float_t phaseIncrement = 2.0 * frequency * mSamplePeriod;
        for (int i = 0; i < numSamples; i++) {
            output[i] = translatePhase(phase, phaseIncrement);
            phase += phaseIncrement;
            if (phase > 1.0) {
                phase -= 2.0;
            }
        }
        mPhase = phase;
    }

    void generate(synth_float_t *frequencies, int32_t numSamples) {
        synth_float_t phase = mPhase;
        for (int i = 0; i < numSamples; i++) {
            synth_float_t phaseIncrement = 2.0 * frequencies[i] * mSamplePeriod;
            output[i] = translatePhase(phase, phaseIncrement);
            phase += phaseIncrement;
            if (phase > 1.0) {
                phase -= 2.0;
            }
        }
        mPhase = phase;
    }

    virtual synth_float_t translatePhase(synth_float_t phase, synth_float_t phaseIncrement) {
        (void) phaseIncrement;
        return phase;
    }

private:
    synth_float_t mPhase; // between -1.0 and +1.0
};

};
#endif // SYNTHMARK_SAWTOOTH_OSCILLATOR_H
