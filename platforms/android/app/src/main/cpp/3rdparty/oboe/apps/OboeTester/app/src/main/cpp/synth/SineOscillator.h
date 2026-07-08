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

#ifndef SYNTHMARK_SINE_OSCILLATOR_H
#define SYNTHMARK_SINE_OSCILLATOR_H

#include <cstdint>
#include <math.h>
#include "SawtoothOscillator.h"
#include "SynthTools.h"

namespace marksynth {
class SineOscillator  : public SawtoothOscillator
{
public:
    SineOscillator()
    : SawtoothOscillator() {}

    virtual ~SineOscillator() = default;

    virtual inline synth_float_t translatePhase(synth_float_t phase, synth_float_t phaseIncrement) {
        (void) phaseIncrement;
        return SynthTools::fastSine(phase * M_PI);
    }

};
};
#endif // SYNTHMARK_SINE_OSCILLATOR_H
