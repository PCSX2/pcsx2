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

#include "TriangleOscillator.h"

TriangleOscillator::TriangleOscillator()
        : OscillatorBase() {
}

int32_t TriangleOscillator::onProcess(int32_t numFrames) {
    const float *frequencies = frequency.getBuffer();
    const float *amplitudes = amplitude.getBuffer();
    float *buffer = output.getBuffer();

    // Use the phase directly as a non-band-limited "triangle".
    // WARNING: This will generate unpleasant aliasing artifacts at higher frequencies.
    for (int i = 0; i < numFrames; i++) {
        float phase = incrementPhase(frequencies[i]); // phase ranges from -1 to +1
        float triangle = 2.0f * ((phase < 0.0f) ? (0.5f + phase): (0.5f - phase));
        *buffer++ = triangle * amplitudes[i];
    }

    return numFrames;
}
