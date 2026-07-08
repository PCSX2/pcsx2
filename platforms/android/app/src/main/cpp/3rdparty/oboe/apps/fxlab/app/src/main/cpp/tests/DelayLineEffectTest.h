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

#ifndef ANDROID_FXLAB_DELAYLINEEFFECTTEST_H
#define ANDROID_FXLAB_DELAYLINEEFFECTTEST_H


#include <gtest/gtest.h>

#include "../effects/DelayLineEffect.h"


namespace {
    TEST(DelayLineEffectTest, SingleFeedForwardTest) {
        DelayLineEffect<float*> e1 {0, 1, 0, 1, 0, [](){return 0;}};
        std::array<float, 5> inputData {1, 0, 0.5, 0.25, 0};
        for (int i = 0; i < 5; i++){
            e1(inputData[i]);
        }
        EXPECT_EQ(inputData[0], 0);
        EXPECT_EQ(inputData[1], 1);
        EXPECT_EQ(inputData[2], 0);
        EXPECT_EQ(inputData[3], 0.5);
        EXPECT_EQ(inputData[4], 0.25);
    }
    TEST(DelayLineEffectTest, FFandBlendTest) {
        DelayLineEffect<float*> e1 {1, 1, 0, 1, 0, [](){return 0;}};
        std::array<float, 5> inputData {1, 0, 0.5, 0.25, 0};
        for (int i = 0; i < 5; i++) {
            e1(inputData[i]);
        }
        EXPECT_EQ(inputData[0], 1);
        EXPECT_EQ(inputData[1], 1);
        EXPECT_EQ(inputData[2], 0.5);
        EXPECT_EQ(inputData[3], 0.75);
        EXPECT_EQ(inputData[4], 0.25);
    }
}
#endif //ANDROID_FXLAB_DELAYLINEEFFECTTEST_H
