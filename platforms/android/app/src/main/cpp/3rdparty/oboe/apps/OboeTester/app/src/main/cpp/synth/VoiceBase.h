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

#ifndef SYNTHMARK_VOICE_BASE_H
#define SYNTHMARK_VOICE_BASE_H

#include <cstdint>
#include "SynthTools.h"
#include "UnitGenerator.h"

namespace marksynth {
/**
 * Base class for building synthesizers.
 */
class VoiceBase  : public UnitGenerator
{
public:
    VoiceBase()
    : mPitch(60.0) // MIDI Middle C is 60
    , mVelocity(1.0) // normalized
    {
    }

    virtual ~VoiceBase() = default;

    void setPitch(synth_float_t pitch) {
        mPitch = pitch;
    }

    void noteOn(synth_float_t pitch, synth_float_t velocity) {
        mVelocity = velocity;
        mPitch = pitch;
    }

    void noteOff() {
    }

    virtual void generate(int32_t numFrames) = 0;

protected:
    synth_float_t mPitch;
    synth_float_t mVelocity;
};
};
#endif // SYNTHMARK_VOICE_BASE_H
