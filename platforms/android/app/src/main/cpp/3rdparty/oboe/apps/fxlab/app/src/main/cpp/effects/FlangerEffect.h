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
#ifndef ANDROID_FXLAB_FLANGEREFFECT_H
#define ANDROID_FXLAB_FLANGEREFFECT_H

#include "DelayLineEffect.h"

template<class iter_type>
class FlangerEffect : public DelayLineEffect<iter_type> {
public:
    // feedback should be 0.7071
    FlangerEffect(float depth_ms, float frequency, float feedback):
        DelayLineEffect<iter_type>(feedback, feedback, feedback, 0, depth_ms * SAMPLE_RATE / 1000,
                SineWave {frequency, 1, SAMPLE_RATE})  { }
};
#endif //ANDROID_FXLAB_FLANGEREFFECT_H
