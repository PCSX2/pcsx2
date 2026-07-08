/*
 * Copyright  2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef ANDROID_FXLAB_COMBFILTER_H
#define ANDROID_FXLAB_COMBFILTER_H

#include "utils/DelayLine.h"

template <class iter_type>
class CombFilter {

public:
    // delay > 0 in samples
    CombFilter(float blend, float feedForward, float feedBack, int delay) :
        kBlend(blend),
        kFeedForward(feedForward),
        kFeedBack(feedBack),
        kDelay(static_cast<size_t>(delay)) {}


    void operator () (typename std::iterator_traits<iter_type>::reference x) {
        auto delayOutput = delayLine[kDelay];
        auto delayInput = x + kFeedBack * delayOutput;
        delayLine.push(delayInput);
        x = delayInput * kBlend + delayOutput * kFeedForward;
    }
    void operator () (iter_type begin, iter_type end) {
        for (; begin != end; ++begin) {
            operator()(*begin);
        }
    }
private:
    // Weights
    const float kBlend;
    const float kFeedForward;
    const float kFeedBack;
    const size_t kDelay;

    DelayLine<typename std::iterator_traits<iter_type>::value_type> delayLine {kDelay + 1};
};
#endif //ANDROID_FXLAB_COMBFILTER_H
