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

#ifndef ANDROID_FXLAB_IIRDESCRIPTION_H
#define ANDROID_FXLAB_IIRDESCRIPTION_H


#include "../CombFilter.h"

namespace Effect {
class IIRDescription: public EffectDescription<IIRDescription, 3> {
public:
    static constexpr std::string_view getName() {
        return std::string_view("IIR");
    }

    static constexpr std::string_view getCategory() {
        return std::string_view("Comb");
    }

    static constexpr std::array<ParamType, getNumParams()> getParams() {
        return std::array<ParamType, getNumParams()> {
            ParamType("Delay (samples)", 1, 500, 10),
            ParamType("Gain", 0, 0.99, 0.5),
            ParamType("Scale", 0.25, 0.99, 0.5)
        };
    }
    template<class iter_type>
    static _ef<iter_type> buildEffect(std::array<float, getNumParams()> paramArr) {
        return _ef<iter_type> {
            CombFilter<iter_type>{paramArr[2], 0, paramArr[1], static_cast<int>(paramArr[0])}
        };
    }
};
} //namespace Effect
#
#endif //ANDROID_FXLAB_IIRDESCRIPTION_H
