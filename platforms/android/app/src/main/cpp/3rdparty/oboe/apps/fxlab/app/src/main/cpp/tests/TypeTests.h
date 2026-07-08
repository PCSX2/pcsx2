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
#ifndef ANDROID_FXLAB_TYPETESTS_H
#define ANDROID_FXLAB_TYPETESTS_H


#include <gtest/gtest.h>
#include <iostream>
#include <unistd.h>

#include "../effects/Effects.h"
#include "../FunctionList.h"
namespace {
TEST(TypeTests, Templating) {
    TremoloEffect t {1, 2};
    VibratoEffect<float*> v {1, 2};
    float x = 5;
    t(x);
    auto pass = std::get<0>(EffectsTuple);
    auto descrip = std::get<1>(EffectsTuple);
    auto vibdes = std::get<2>(EffectsTuple);
    auto gaindes = std::get<3>(EffectsTuple);
    auto f = descrip.buildDefaultEffect<float*>();
    auto g = vibdes.buildDefaultEffect<float*>();
    auto h = vibdes.buildDefaultEffect<float*>();
    f(&x, &x + 1); g(&x, &x + 1); h(&x, &x + 1);
    auto j = gaindes.buildDefaultEffect<float*>();
    auto k = gaindes.modifyEffect<float*>(j, std::array<float, 1> {10});
    float floatArr[4] = {1,2,3, 4};
    for (int i =0; i < 4; i++) {
        k(floatArr + i, floatArr + i + 1);
    }
    auto arr = std::array<float, 1> {10};
    auto data = std::array<int, 5> {1, 2, 3, 4, 5};
    std::function<void(int*, int*)> fe = std::get<Effect::GainDescription>(EffectsTuple).buildEffect<int*>(arr);
    fe(data.begin(), data.end());
    EXPECT_EQ(data[0], 2);

}
}
#endif //ANDROID_FXLAB_TYPETESTS_H
