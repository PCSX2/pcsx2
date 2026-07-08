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

#include "SineOscillator.h"

/*
 * This calls sinf() so it is not very efficient.
 * A more efficient implementation might use a wave-table or a polynomial.
 */
SineOscillator::SineOscillator()
        : OscillatorBase() {
}

int32_t SineOscillator::onProcess(int32_t numFrames) {
    const float *frequencies = frequency.getBuffer();
    const float *amplitudes = amplitude.getBuffer();
    float *buffer = output.getBuffer();

    // Generate sine wave.
    for (int i = 0; i < numFrames; i++) {
        float phase = incrementPhase(frequencies[i]); // phase ranges from -1 to +1
        *buffer++ = sinf(phase * M_PI) * amplitudes[i];
    }

    return numFrames;
}
