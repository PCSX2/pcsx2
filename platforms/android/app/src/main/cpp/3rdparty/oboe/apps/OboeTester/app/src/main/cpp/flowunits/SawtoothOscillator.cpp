/*
 * Copyright 2015 The Android Open Source Project
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

#include "SawtoothOscillator.h"

SawtoothOscillator::SawtoothOscillator()
        : OscillatorBase() {
}

int32_t SawtoothOscillator::onProcess(int32_t numFrames) {
    const float *frequencies = frequency.getBuffer();
    const float *amplitudes = amplitude.getBuffer();
    float *buffer = output.getBuffer();

    // Use the phase directly as a non-band-limited "sawtooth".
    // WARNING: This will generate unpleasant aliasing artifacts at higher frequencies.
    for (int i = 0; i < numFrames; i++) {
        float phase = incrementPhase(frequencies[i]); // phase ranges from -1 to +1
        *buffer++ = phase * amplitudes[i];
    }

    return numFrames;
}
