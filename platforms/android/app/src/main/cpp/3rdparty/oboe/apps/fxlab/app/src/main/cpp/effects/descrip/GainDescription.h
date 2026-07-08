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
#ifndef ANDROID_FXLAB_GAINDESCRIPTION_H
#define ANDROID_FXLAB_GAINDESCRIPTION_H
#include "EffectDescription.h"
#include <cmath>
#include <iostream>

namespace Effect {
class GainDescription : public EffectDescription<GainDescription, 1> {
public:
    static constexpr std::string_view getName() {
        return std::string_view("Gain");
    }

    static constexpr std::string_view getCategory() {
        return std::string_view("None");
    }

    static constexpr std::array<ParamType, getNumParams()> getParams() {
        return std::array <ParamType, getNumParams()> {
                ParamType("Gain", -30, 20, 0)
        };
    }

    template<class iter_type>
    static _ef<iter_type> buildEffect(std::array<float, getNumParams()> paramArr) {
        float scale = paramArr[0] / 10;
        return _ef<iter_type> {
            [=](iter_type beg, iter_type end) {
                for (; beg != end; ++beg) *beg *= pow(2.0, scale);
            }
        };
    }
};
} // namespace Effect
#endif //ANDROID_FXLAB_GAINDESCRIPTION_H
