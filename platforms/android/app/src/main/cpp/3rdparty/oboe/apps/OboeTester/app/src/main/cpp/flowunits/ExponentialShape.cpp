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

#include <math.h>

#include "ExponentialShape.h"

ExponentialShape::ExponentialShape()
        : FlowGraphFilter(1) {
}

int32_t ExponentialShape::onProcess(int32_t numFrames) {
    float *inputs = input.getBuffer();
    float *outputs = output.getBuffer();

    for (int i = 0; i < numFrames; i++) {
        float normalizedPhase = (inputs[i] * 0.5) + 0.5;
        outputs[i] = mMinimum * powf(mRatio, normalizedPhase);
    }

    return numFrames;
}
