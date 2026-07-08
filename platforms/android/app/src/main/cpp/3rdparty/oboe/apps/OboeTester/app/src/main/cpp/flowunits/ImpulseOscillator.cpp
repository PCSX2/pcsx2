/*
 * Copyright 2018 The Android Open Source Project
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

#include <math.h>
#include <unistd.h>

#include "ImpulseOscillator.h"

ImpulseOscillator::ImpulseOscillator()
        : OscillatorBase() {
}

int32_t ImpulseOscillator::onProcess(int32_t numFrames) {
    const float *frequencies = frequency.getBuffer();
    const float *amplitudes = amplitude.getBuffer();
    float *buffer = output.getBuffer();

    for (int i = 0; i < numFrames; i++) {
        float value = 0.0f;
        mPhase += mFrequencyToPhaseIncrement * frequencies[i];
        if (mPhase >= 1.0f) {
            value = amplitudes[i]; // spike
            mPhase -= 2.0f;
        }
        *buffer++ = value;
    }

    return numFrames;
}
