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

#ifndef ANDROID_FXLAB_DELAYLINEEFFECT_H
#define ANDROID_FXLAB_DELAYLINEEFFECT_H

#include <functional>

#include "utils/SineWave.h"
#include "utils/DelayLine.h"

// Abstract class for implementing delay line based effects
// This functor retains state (it must be used sequentially)
// Effects are float, mono, 48000 hz
template <class iter_type>
class DelayLineEffect {

public:
    // delay > 0, depth in samples, mod is control signal
    DelayLineEffect(float blend, float feedForward, float feedBack, int delay, int depth, std::function<float()> &&mod) :
        kBlend(blend),
        kFeedForward(feedForward),
        kFeedBack(feedBack),
        kDelay(delay),
        kDepth(depth),
        mMod(mod) { }


    void operator () (typename std::iterator_traits<iter_type>::reference x) {
        auto delayInput = x + kFeedBack * delayLine[kTap];
        auto variableDelay = mMod() * kDepth + kTap;
        int index = static_cast<int>(variableDelay);
        auto fracComp = 1 - (variableDelay - index);
        //linear
        // auto interpolated = fracComp * delayLine[index] + (1 - fracComp) * delayLine[index + 1];
        // all - pass
        float interpolated = fracComp * delayLine[index] + delayLine[index + 1]
                - fracComp * prevInterpolated;

        prevInterpolated = interpolated;
        delayLine.push(delayInput);
        x = interpolated * kFeedForward + kBlend * delayInput;
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
    const int kDelay;
    const int kDepth;
    const int kTap = kDelay + kDepth;

    // Control function
    const std::function<float()> mMod;

    // Memory
    float prevInterpolated = 0; // for all pass interp
    const size_t kDelayLineSize = kTap + kDepth + 1; // index one is immediate prev sample
    DelayLine<typename std::iterator_traits<iter_type>::value_type> delayLine {kDelayLineSize};
};
#endif //ANDROID_FXLAB_DELAYLINEEFFECT_H
