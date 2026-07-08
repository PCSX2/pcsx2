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

#include <algorithm>

#include "InterpolatingDelayLine.h"

InterpolatingDelayLine::InterpolatingDelayLine(int32_t delaySize) {
    mDelaySize = delaySize;
    mDelayLine = std::make_unique<float[]>(delaySize);
}

float InterpolatingDelayLine::process(float delay, float input) {
    float *writeAddress = mDelayLine.get() + mCursor;
    *writeAddress = input;
    mDelayLine.get()[mCursor] = input;
    int32_t delayInt = std::min(mDelaySize - 1, (int32_t) delay);
    int32_t readIndex = mCursor - delayInt;
    if (readIndex < 0) {
        readIndex += mDelaySize;
    }
    // TODO interpolate
    float *readAddress = mDelayLine.get() + readIndex;
    float output = *readAddress;
    mCursor++;
    if (mCursor >= mDelaySize) {
        mCursor = 0;
    }
    return output;
};
