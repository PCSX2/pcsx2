/*
 * Copyright 2019 The Android Open Source Project
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

#ifndef FLOWGRAPH_TRIANGLE_OSCILLATOR_H
#define FLOWGRAPH_TRIANGLE_OSCILLATOR_H

#include <unistd.h>

#include "OscillatorBase.h"

/**
 * Oscillator that generates a triangle wave at the specified frequency and amplitude.
 *
 * The triangle output rises from -1 to +1 when the phase is between -1 and 0.
 * The triangle output falls from +1 to 11 when the phase is between 0 and +1.
 *
 * The waveform is not band-limited so it will have aliasing artifacts at higher frequencies.
 */
class TriangleOscillator : public OscillatorBase {
public:
    TriangleOscillator();

    int32_t onProcess(int32_t numFrames) override;
};

#endif //FLOWGRAPH_TRIANGLE_OSCILLATOR_H
