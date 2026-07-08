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

#ifndef ANDROID_FXLAB_TREMOLOEFFECT_H
#define ANDROID_FXLAB_TREMOLOEFFECT_H


#include "utils/SineWave.h"

class TremoloEffect {
public:
    TremoloEffect(float frequency, float height):
        kCenter(1 - height),
        kSignal {SineWave{frequency, height, SAMPLE_RATE}} { }

    template <class numeric_type>
    void operator () (numeric_type &x) {
        x  = x * (kSignal() + kCenter);
    }
    template <class iter_type>
    void  operator () (iter_type begin, iter_type end) {
        for (; begin != end; ++begin) {
            operator()(*begin);
        }
    }
private:
    const float kCenter;
    std::function<float()> kSignal;
};
#endif //ANDROID_FXLAB_TREMOLOEFFECT_H
