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

#ifndef OBOETESTER_INTERPOLATING_DELAY_LINE_H
#define OBOETESTER_INTERPOLATING_DELAY_LINE_H

#include <memory>
#include <unistd.h>
#include <sys/types.h>

/**
 * Monophonic delay line.
 */
class InterpolatingDelayLine  {
public:
    explicit InterpolatingDelayLine(int32_t delaySize);

    /**
     * @param input sample to be written to the delay line
     * @param delay number of samples to delay the output
     * @return delayed value
     */
    float process(float delay, float input);

private:
    std::unique_ptr<float[]> mDelayLine;
    int32_t mCursor = 0;
    int32_t mDelaySize = 0;
};


#endif //OBOETESTER_INTERPOLATING_DELAY_LINE_H
