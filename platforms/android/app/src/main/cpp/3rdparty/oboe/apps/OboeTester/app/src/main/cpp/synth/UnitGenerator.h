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

#ifndef SYNTHMARK_UNIT_GENERATOR_H
#define SYNTHMARK_UNIT_GENERATOR_H

#include <cstdint>
#include <assert.h>
#include <math.h>
#include "SynthTools.h"
//#include "DifferentiatedParabola.h"

namespace marksynth {
class UnitGenerator
{
public:
    UnitGenerator() {}

    virtual ~UnitGenerator() = default;

    static void setSampleRate(int32_t sampleRate) {
        assert(sampleRate > 0);
        mSampleRate = sampleRate;
        mSamplePeriod = 1.0f / sampleRate;
    }

    static int32_t getSampleRate() {
        return mSampleRate;
    }

    synth_float_t output[kSynthmarkFramesPerRender];

public:
    static int32_t mSampleRate;
    static synth_float_t mSamplePeriod;
};
}
#endif // SYNTHMARK_UNIT_GENERATOR_H
