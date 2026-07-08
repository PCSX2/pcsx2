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
#ifndef ANDROID_FXLAB_WHITECHORUSEFFECT_H
#define ANDROID_FXLAB_WHITECHORUSEFFECT_H

#include "DelayLineEffect.h"
#include "utils/WhiteNoise.h"

template <class iter_type>
class WhiteChorusEffect : public DelayLineEffect<iter_type> {
public:
    WhiteChorusEffect(float depth_ms, float delay_ms, float noise_pass):
        DelayLineEffect<iter_type> {0.7071, 1, -0.7071f,
            static_cast<int>(delay_ms * SAMPLE_RATE / 1000),
            static_cast<int>(depth_ms * SAMPLE_RATE / 1000),
            std::function<float()>{WhiteNoise{static_cast<int>(4800 * noise_pass)}}}
    {}
};
#endif //ANDROID_FXLAB_WHITECHORUSEFFECT_H
