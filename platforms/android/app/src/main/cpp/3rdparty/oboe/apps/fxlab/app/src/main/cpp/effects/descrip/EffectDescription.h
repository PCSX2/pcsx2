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

#ifndef ANDROID_FXLAB_EFFECTDESCRIPTION_H
#define ANDROID_FXLAB_EFFECTDESCRIPTION_H

#include <functional>
#include <array>
#include <vector>
#include <string_view>

template<class iter_type>
using _ef = std::function<void(iter_type, iter_type)>;

// Only Effect Descriptions should use this namespace
namespace Effect {

    class ParamType {
    public:
        constexpr ParamType(std::string_view name, float minVal, float maxVal, float defVal) :
                kName(name),
                kMinVal(minVal),
                kMaxVal(maxVal),
                kDefVal(defVal) {}

        constexpr ParamType(const ParamType &other) = delete;

        ParamType &operator=(const ParamType &other) = delete;

        constexpr ParamType(ParamType &&other) = default;

        constexpr ParamType &operator=(ParamType &&other) = delete;

        const std::string_view kName;
        const float kMinVal, kMaxVal, kDefVal;
    };


// EffectType is the description subclass, N is num of params
// Function implementations in this class contain shared behavior
// Which can be shadowed.
    template<class EffectType, size_t N>
    class EffectDescription {
    public:
        // These methods will be shadowed by subclasses

        static constexpr size_t getNumParams() {
            return N;
        }

        static constexpr std::array<float, N> getEmptyParams() {
            return std::array<float, EffectType::getNumParams()>();
        }

        static constexpr std::string_view getName();

        static constexpr std::string_view getCategory();

        static constexpr std::array<ParamType, N> getParams();

        template<class iter_type>
        static _ef<iter_type> buildEffect(std::array<float, N> paramArr);


        template<class iter_type>
        static _ef<iter_type> buildDefaultEffect() {
            auto params = EffectType::getEmptyParams();
            int i = 0;
            for (ParamType &mParam: EffectType::getParams()) {
                params[i++] = mParam.kDefVal;
            }
            return EffectType::template buildEffect<iter_type>(params);
        }

        // The default behavior is new effect, can be shadowed
        template<class iter_type>
        static _ef<iter_type> modifyEffect(
                _ef<iter_type> /* effect */, std::array<float, N> paramArr) {
            return EffectType::template buildEffect<iter_type>(std::move(paramArr));
        }
        template <class iter_type>
        static _ef<iter_type> modifyEffectVec(
                _ef<iter_type> effect, std::vector<float> paramVec) {
            std::array<float, N> arr;
            std::copy_n(paramVec.begin(), N, arr.begin());
            return EffectType::template modifyEffect<iter_type>(
                    std::move(effect), std::move(arr));
        }
    };

} // namespace effect


#endif //ANDROID_FXLAB_EFFECTDESCRIPTION_H
