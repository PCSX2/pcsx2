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

#ifndef ANDROID_FXLAB_DELAYLINETEST_H
#define ANDROID_FXLAB_DELAYLINETEST_H

#include "../effects/utils/DelayLine.h"
#include <gtest/gtest.h>

namespace {
TEST(DelayLineTest, CreateAndAdd) {
    DelayLine<float> d1{10};
    d1.push(1.0);
    EXPECT_EQ(d1[1], 1.0);
}
TEST(DelayLineTest, UnfilledAddSequence) {
    DelayLine<float> d1{10};
d1.push(1.0);
d1.push(2.0);
    d1.push(3.0);
    EXPECT_EQ(d1[1], 3.0);
    EXPECT_EQ(d1[2], 2.0);
    EXPECT_EQ(d1[3], 1.0);
}
TEST(DelayLineTest, FilledAddSequence) {
    DelayLine<float> d1{4};
    d1.push(1.0);
    d1.push(2.0);
    d1.push(3.0);
    d1.push(4.0);
    EXPECT_EQ(d1[1], 4.0);
    EXPECT_EQ(d1[2], 3.0);
    EXPECT_EQ(d1[3], 2.0);
    EXPECT_EQ(d1[4], 1.0);
    d1.push(5.0);
    EXPECT_EQ(d1[1], 5.0);
    EXPECT_EQ(d1[2], 4.0);
    EXPECT_EQ(d1[3], 3.0);
    EXPECT_EQ(d1[4], 2.0);
    d1.push(6.0);
    EXPECT_EQ(d1[1], 6.0);
    EXPECT_EQ(d1[2], 5.0);
    EXPECT_EQ(d1[3], 4.0);
    EXPECT_EQ(d1[4], 3.0);
}

} // namespace
#endif //ANDROID_FXLAB_DELAYLINETEST_H
