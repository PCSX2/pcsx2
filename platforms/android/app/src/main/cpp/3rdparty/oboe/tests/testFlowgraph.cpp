/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Test FlowGraph
 */

#include "stdio.h"

#include <gtest/gtest.h>
#include <oboe/Oboe.h>

#include "flowgraph/ClipToRange.h"
#include "flowgraph/Limiter.h"
#include "flowgraph/MonoToMultiConverter.h"
#include "flowgraph/SourceFloat.h"
#include "flowgraph/RampLinear.h"
#include "flowgraph/SinkFloat.h"
#include "flowgraph/SinkI16.h"
#include "flowgraph/SinkI24.h"
#include "flowgraph/SinkI32.h"
#include "flowgraph/SourceI16.h"
#include "flowgraph/SourceI24.h"

using namespace oboe::flowgraph;

constexpr int kBytesPerI24Packed = 3;

TEST(test_flowgraph, module_sinki16) {
    static const float input[] = {1.0f, 0.5f, -0.25f, -1.0f, 0.0f, 53.9f, -87.2f};
    static const int16_t expected[] = {32767, 16384, -8192, -32768, 0, 32767, -32768};
    int16_t output[20];
    SourceFloat sourceFloat{1};
    SinkI16 sinkI16{1};

    int numInputFrames = sizeof(input) / sizeof(input[0]);
    sourceFloat.setData(input, numInputFrames);
    sourceFloat.output.connect(&sinkI16.input);

    int numOutputFrames = sizeof(output) / sizeof(int16_t);
    int32_t numRead = sinkI16.read(output, numOutputFrames);
    ASSERT_EQ(numInputFrames, numRead);
    for (int i = 0; i < numRead; i++) {
        EXPECT_EQ(expected[i], output[i]);
    }
}

TEST(test_flowgraph, module_mono_to_stereo) {
    static const float input[] = {1.0f, 2.0f, 3.0f};
    float output[100] = {};
    SourceFloat sourceFloat{1};
    MonoToMultiConverter monoToStereo{2};
    SinkFloat sinkFloat{2};

    sourceFloat.setData(input, 3);

    sourceFloat.output.connect(&monoToStereo.input);
    monoToStereo.output.connect(&sinkFloat.input);

    int32_t numRead = sinkFloat.read(output, 8);
    ASSERT_EQ(3, numRead);
    EXPECT_EQ(input[0], output[0]);
    EXPECT_EQ(input[0], output[1]);
    EXPECT_EQ(input[1], output[2]);
    EXPECT_EQ(input[1], output[3]);
}

TEST(test_flowgraph, module_ramp_linear) {
    constexpr int singleNumOutput = 1;
    constexpr int rampSize = 5;
    constexpr int numOutput = 100;
    constexpr float value = 1.0f;
    constexpr float initialTarget = 10.0f;
    constexpr float finalTarget = 100.0f;
    constexpr float tolerance = 0.0001f; // arbitrary
    float output[numOutput] = {};
    RampLinear rampLinear{1};
    SinkFloat sinkFloat{1};

    rampLinear.input.setValue(value);
    rampLinear.setLengthInFrames(rampSize);
    rampLinear.output.connect(&sinkFloat.input);

    // Check that the values go to the initial target instantly.
    rampLinear.setTarget(initialTarget);
    int32_t singleNumRead = sinkFloat.read(output, singleNumOutput);
    ASSERT_EQ(singleNumRead, singleNumOutput);
    EXPECT_NEAR(value * initialTarget, output[0], tolerance);

    // Now set target and check that the linear ramp works as expected.
    rampLinear.setTarget(finalTarget);
    int32_t numRead = sinkFloat.read(output, numOutput);
    const float incrementSize = (finalTarget - initialTarget) / rampSize;
    ASSERT_EQ(numOutput, numRead);

    int i = 0;
    for (; i < rampSize; i++) {
        float expected = value * (initialTarget + i * incrementSize);
        EXPECT_NEAR(expected, output[i], tolerance);
    }
    for (; i < numOutput; i++) {
        float expected = value * finalTarget;
        EXPECT_NEAR(expected, output[i], tolerance);
    }
}

// It is easiest to represent packed 24-bit data as a byte array.
// This test will read from input, convert to float, then write
// back to output as bytes.
TEST(test_flowgraph, module_packed_24) {
    static const uint8_t input[] = {0x01, 0x23, 0x45,
                                    0x67, 0x89, 0xAB,
                                    0xCD, 0xEF, 0x5A};
    uint8_t output[99] = {};
    SourceI24 sourceI24{1};
    SinkI24 sinkI24{1};

    int numInputFrames = sizeof(input) / kBytesPerI24Packed;
    sourceI24.setData(input, numInputFrames);
    sourceI24.output.connect(&sinkI24.input);

    int32_t numRead = sinkI24.read(output, sizeof(output) / kBytesPerI24Packed);
    ASSERT_EQ(numInputFrames, numRead);
    for (size_t i = 0; i < sizeof(input); i++) {
        EXPECT_EQ(input[i], output[i]);
    }
}

TEST(test_flowgraph, module_clip_to_range) {
    constexpr float myMin = -2.0f;
    constexpr float myMax = 1.5f;

    static const float input[] = {-9.7, 0.5f, -0.25, 1.0f, 12.3};
    static const float expected[] = {myMin, 0.5f, -0.25, 1.0f, myMax};
    float output[100];
    SourceFloat sourceFloat{1};
    ClipToRange clipper{1};
    SinkFloat sinkFloat{1};

    int numInputFrames = sizeof(input) / sizeof(input[0]);
    sourceFloat.setData(input, numInputFrames);

    clipper.setMinimum(myMin);
    clipper.setMaximum(myMax);

    sourceFloat.output.connect(&clipper.input);
    clipper.output.connect(&sinkFloat.input);

    int numOutputFrames = sizeof(output) / sizeof(output[0]);
    int32_t numRead = sinkFloat.read(output, numOutputFrames);
    ASSERT_EQ(numInputFrames, numRead);
    constexpr float tolerance = 0.000001f; // arbitrary
    for (int i = 0; i < numRead; i++) {
        EXPECT_NEAR(expected[i], output[i], tolerance);
    }
}

TEST(test_flowgraph, module_sinki32) {
    static constexpr int kNumSamples = 8;
    static const float input[] = {
        1.0f, 0.5f, -0.25f, -1.0f,
        0.0f, 53.9f, -87.2f, -1.02f};
    static const int32_t expected[] = {
        INT32_MAX, 1 << 30, INT32_MIN / 4, INT32_MIN,
        0, INT32_MAX, INT32_MIN, INT32_MIN};
    int32_t output[kNumSamples + 10]; // larger than input

    SourceFloat sourceFloat{1};
    SinkI32 sinkI32{1};

    sourceFloat.setData(input, kNumSamples);
    sourceFloat.output.connect(&sinkI32.input);

    int numOutputFrames = sizeof(output) / sizeof(int32_t);
    int32_t numRead = sinkI32.read(output, numOutputFrames);
    ASSERT_EQ(kNumSamples, numRead);
    for (int i = 0; i < numRead; i++) {
        EXPECT_EQ(expected[i], output[i]) << ", i = " << i;
    }
}

TEST(test_flowgraph, module_limiter) {
    constexpr int kNumSamples = 101;
    constexpr float kLastSample = 3.0f;
    constexpr float kFirstSample = -kLastSample;
    constexpr float kDeltaBetweenSamples = (kLastSample - kFirstSample) / (kNumSamples - 1);
    constexpr float kTolerance = 0.00001f;

    float input[kNumSamples];
    float output[kNumSamples];
    SourceFloat sourceFloat{1};
    Limiter limiter{1};
    SinkFloat sinkFloat{1};

    for (int i = 0; i < kNumSamples; i++) {
        input[i] = kFirstSample + i * kDeltaBetweenSamples;
    }

    const int numInputFrames = std::size(input);
    sourceFloat.setData(input, numInputFrames);

    sourceFloat.output.connect(&limiter.input);
    limiter.output.connect(&sinkFloat.input);

    const int numOutputFrames = std::size(output);
    int32_t numRead = sinkFloat.read(output, numOutputFrames);
    ASSERT_EQ(numInputFrames, numRead);

    for (int i = 0; i < numRead; i++) {
        // limiter must be symmetric wrt 0.
        EXPECT_NEAR(output[i], -output[kNumSamples - i - 1], kTolerance);
        if (i > 0) {
            EXPECT_GE(output[i], output[i - 1]); // limiter must be monotonic
        }
        if (input[i] == 0.f) {
            EXPECT_EQ(0.f, output[i]);
        } else if (input[i] > 0.0f) {
            EXPECT_GE(output[i], 0.0f);
            EXPECT_LE(output[i], M_SQRT2); // limiter actually limits
            EXPECT_LE(output[i], input[i]); // a limiter, gain <= 1
        } else {
            EXPECT_LE(output[i], 0.0f);
            EXPECT_GE(output[i], -M_SQRT2); // limiter actually limits
            EXPECT_GE(output[i], input[i]); // a limiter, gain <= 1
        }
        if (-1.f <= input[i] && input[i] <= 1.f) {
            EXPECT_EQ(input[i], output[i]);
        }
    }
}

TEST(test_flowgraph, module_limiter_nan) {
    constexpr int kArbitraryOutputSize = 100;
    constexpr float kFloatNan = NAN;
    static const float input[] = {kFloatNan, 0.5f, kFloatNan, kFloatNan, -10.0f, kFloatNan};
    static const float expected[] = {0.0f, 0.5f, 0.5f, 0.5f, -M_SQRT2, -M_SQRT2};
    constexpr float tolerance = 0.00001f;
    float output[kArbitraryOutputSize];
    SourceFloat sourceFloat{1};
    Limiter limiter{1};
    SinkFloat sinkFloat{1};

    const int numInputFrames = std::size(input);
    sourceFloat.setData(input, numInputFrames);

    sourceFloat.output.connect(&limiter.input);
    limiter.output.connect(&sinkFloat.input);

    const int numOutputFrames = std::size(output);
    int32_t numRead = sinkFloat.read(output, numOutputFrames);
    ASSERT_EQ(numInputFrames, numRead);

    for (int i = 0; i < numRead; i++) {
        EXPECT_NEAR(expected[i], output[i], tolerance);
    }
}
