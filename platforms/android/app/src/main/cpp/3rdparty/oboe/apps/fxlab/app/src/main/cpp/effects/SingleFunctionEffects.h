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
#ifndef ANDROID_FXLAB_SINGLEFUNCTIONEFFECTS_H
#define ANDROID_FXLAB_SINGLEFUNCTIONEFFECTS_H

#include <list>

 namespace SingleFunctionEffects {

template<class floating>
void _overdrive (floating &x) {
    static constexpr double third = (1.0 / 3.0);
    auto abs = std::abs(x);
    if (abs <= third) {
        x *= 2;
    } else if (abs <= 2 * third) {
        x = std::copysign((3 - (2 - 3 * abs) * (2 - 3 * abs)) * third, x);
    } else {
        x = std::copysign(1, x);
    }
}

template <class iter_type>
void overdrive(iter_type beg, iter_type end) {
    for (; beg != end; ++beg){
        _overdrive(*beg);
    }
}

template <class floating>
void _distortion (floating &x) {
    x = std::copysign(-std::expm1(-std::abs(x)), x);
}

template <class iter_type>
void distortion(iter_type beg, iter_type end) {
    for (; beg != end; ++beg) {
        _distortion(*beg);
    }
}

}
#endif //ANDROID_FXLAB_SINGLEFUNCTIONEFFECTS_H
