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

#ifndef SYNTHMARK_SAWTOOTH_OSCILLATOR_DPW_H
#define SYNTHMARK_SAWTOOTH_OSCILLATOR_DPW_H

#include <cstdint>
#include <math.h>
#include "SynthTools.h"
#include "DifferentiatedParabola.h"
#include "SawtoothOscillator.h"

namespace marksynth {
/**
 * Band limited sawtooth oscillator.
 * Suitable as a sound source.
 */

class SawtoothOscillatorDPW : public SawtoothOscillator
{
public:
    SawtoothOscillatorDPW()
    : SawtoothOscillator()
    , dpw() {}

    virtual ~SawtoothOscillatorDPW() = default;

    virtual inline synth_float_t translatePhase(synth_float_t phase, synth_float_t phaseIncrement) {
        return dpw.next(phase, phaseIncrement);
    }

private:
    DifferentiatedParabola dpw;
};

};
#endif // SYNTHMARK_SAWTOOTH_OSCILLATOR_DPW_H
