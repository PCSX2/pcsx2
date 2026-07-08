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
#ifndef ANDROID_FXLAB_OVERDRIVEDESCRIPTION_H
#define ANDROID_FXLAB_OVERDRIVEDESCRIPTION_H

#include <limits>
#include <cmath>
#include <iterator>

#include "EffectDescription.h"
#include "../SingleFunctionEffects.h"
#include "../DriveControl.h"

namespace  Effect {
class OverdriveDescription : public EffectDescription<OverdriveDescription, 1> {
public:
    static constexpr std::string_view getName() {
        return std::string_view("Overdrive");
    }

    static constexpr std::string_view getCategory() {
        return std::string_view("Nonlinear");
    }

    static constexpr std::array<ParamType, getNumParams()> getParams() {
        return std::array<ParamType, getNumParams()>{
            ParamType("Drive (db)", -10, 50, 0)
        };
    }

    template<class iter_type>
    static _ef<iter_type> buildEffect(std::array<float, getNumParams()> paramArr) {
        double scale = pow(2.0, paramArr[0] / 10);
        return DriveControl<iter_type> {SingleFunctionEffects::overdrive<iter_type>, scale};
    }
};
}
#endif //ANDROID_FXLAB_OVERDRIVEDESCRIPTION_H
