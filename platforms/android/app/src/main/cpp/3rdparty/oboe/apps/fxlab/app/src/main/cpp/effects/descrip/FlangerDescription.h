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
#ifndef ANDROID_FXLAB_FLANGERDESCRIPTION_H
#define ANDROID_FXLAB_FLANGERDESCRIPTION_H

#include "EffectDescription.h"
#include "../FlangerEffect.h"

namespace Effect {
class FlangerDescription : public EffectDescription<FlangerDescription, 3> {
public:
    static constexpr std::string_view getName() {
        return std::string_view("Flanger");
    }

    static constexpr std::string_view getCategory() {
        return std::string_view("Delay");
    }

    static constexpr std::array<ParamType, getNumParams()> getParams() {
        return std::array<ParamType, 3> {
            ParamType("Depth (ms)", 0.1, 2, 1),
            ParamType("Frequency", 0.1, 1, 0.2),
            ParamType("Feedback", 0, 0.95, 0.7071),
        };
    }

    template <class iter_type>
    static _ef<iter_type> buildEffect(std::array<float, getNumParams()> paramArr) {
        return std::function<void(iter_type, iter_type)> {
            FlangerEffect<iter_type>{paramArr[0], paramArr[1], paramArr[2]}
        };
    }

};
} // namespace Effect
#endif //ANDROID_FXLAB_FLANGERDESCRIPTION_H
