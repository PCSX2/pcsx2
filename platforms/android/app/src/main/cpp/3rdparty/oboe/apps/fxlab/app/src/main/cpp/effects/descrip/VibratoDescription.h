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
#ifndef ANDROID_FXLAB_VIBRATODESCRIPTION_H
#define ANDROID_FXLAB_VIBRATODESCRIPTION_H

#include "EffectDescription.h"
#include "../VibratroEffect.h"

namespace Effect {
class VibratoDescription : public EffectDescription<VibratoDescription, 2> {
public:
    static constexpr std::string_view getName() {
        return std::string_view("Vibrato");
    }

    static constexpr std::string_view getCategory() {
        return std::string_view("Delay");
    }

    static constexpr std::array<ParamType, getNumParams()> getParams() {
        return std::array<ParamType, getNumParams()> {
            ParamType("Depth (ms)", 0, 3, 1),
            ParamType("Frequency", 0.1, 5, 1)
        };
    }
    template<class iter_type>
    static _ef<iter_type> buildEffect(std::array<float, getNumParams()> paramArr) {
        return _ef<iter_type> {
            VibratoEffect<iter_type>{paramArr[0], paramArr[1]}
        };
    }
};
} //namespace Effect
#endif //ANDROID_FXLAB_VIBRATODESCRIPTION_H
