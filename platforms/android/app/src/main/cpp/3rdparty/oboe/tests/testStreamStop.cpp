/*
 * Copyright 2022 The Android Open Source Project
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

#include <thread>

#include <gtest/gtest.h>

#include <oboe/Oboe.h>

using namespace oboe;

using TestStreamStopParams = std::tuple<Direction, AudioApi, PerformanceMode>;

class TestStreamStop : public ::testing::Test,
                         public ::testing::WithParamInterface<TestStreamStopParams> {

protected:

    void SetUp(){
        mBuilder.setPerformanceMode(PerformanceMode::None);
        mBuilder.setDirection(Direction::Output);
    }

    bool openStream(Direction direction, AudioApi audioApi, PerformanceMode perfMode) {
        mBuilder.setDirection(direction);
        if (mBuilder.isAAudioRecommended()) {
            mBuilder.setAudioApi(audioApi);
        }
        mBuilder.setPerformanceMode(perfMode);
        mBuilder.setChannelCount(1);
        mBuilder.setFormat(AudioFormat::I16);
        Result r = mBuilder.openStream(mStream);
        EXPECT_EQ(r, Result::OK) << "Failed to open stream " << convertToText(r);
        if (r != Result::OK)
            return false;

        Direction d = mStream->getDirection();
        EXPECT_EQ(d, direction) << convertToText(mStream->getDirection());
        return (d == direction);
    }

    bool openStream(AudioStreamBuilder &builder) {
        Result r = builder.openStream(mStream);
        EXPECT_EQ(r, Result::OK) << "Failed to open stream " << convertToText(r);
        return (r == Result::OK);
    }

    void stopWhileUsingLargeBuffer() {
        StreamState next = StreamState::Unknown;
        auto r = mStream->requestStart();
        EXPECT_EQ(r, Result::OK);
        r = mStream->waitForStateChange(StreamState::Starting, &next, kTimeoutInNanos);
        EXPECT_EQ(r, Result::OK);
        EXPECT_EQ(next, StreamState::Started) << "next = " << convertToText(next);

        std::shared_ptr<AudioStream> str = mStream;

        int16_t buffer[kFramesToWrite] = {};

        std::thread stopper([str] {
            int64_t estimatedCompletionTimeUs = kMicroSecondsPerSecond * kFramesToWrite / str->getSampleRate();
            usleep(estimatedCompletionTimeUs / 2); // Stop halfway during the read/write
            EXPECT_EQ(str->close(), Result::OK);
        });

        if (mBuilder.getDirection() == Direction::Output) {
            r = mStream->write(&buffer, kFramesToWrite, kTimeoutInNanos);
        } else {
            r = mStream->read(&buffer, kFramesToWrite, kTimeoutInNanos);
        }
        if (r != Result::OK) {
            FAIL() << "Could not read/write to audio stream: " << static_cast<int>(r);
        }

        stopper.join();
        r = mStream->waitForStateChange(StreamState::Started, &next,
                                        1000 * kNanosPerMillisecond);
        if ((r != Result::ErrorClosed) && (r != Result::OK)) {
            FAIL() << "Wrong closed result type: " << static_cast<int>(r);
        }
    }

    AudioStreamBuilder mBuilder;
    std::shared_ptr<AudioStream> mStream;
    static constexpr int kTimeoutInNanos = 1000 * kNanosPerMillisecond;
    static constexpr int64_t kMicroSecondsPerSecond = 1000000;
    static constexpr int kFramesToWrite = 10000;

};

TEST_P(TestStreamStop, VerifyTestStreamStop) {
    const Direction direction = std::get<0>(GetParam());
    const AudioApi audioApi = std::get<1>(GetParam());
    const PerformanceMode performanceMode = std::get<2>(GetParam());

    ASSERT_TRUE(openStream(direction, audioApi, performanceMode));
    stopWhileUsingLargeBuffer();
}

INSTANTIATE_TEST_SUITE_P(
        TestStreamStopTest,
        TestStreamStop,
        ::testing::Values(
                TestStreamStopParams({Direction::Output, AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestStreamStopParams({Direction::Output, AudioApi::AAudio, PerformanceMode::None}),
                TestStreamStopParams({Direction::Output, AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestStreamStopParams({Direction::Output, AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestStreamStopParams({Direction::Output, AudioApi::OpenSLES, PerformanceMode::None}),
                TestStreamStopParams({Direction::Output, AudioApi::OpenSLES, PerformanceMode::PowerSaving}),
                TestStreamStopParams({Direction::Input, AudioApi::AAudio, PerformanceMode::LowLatency}),
                TestStreamStopParams({Direction::Input, AudioApi::AAudio, PerformanceMode::None}),
                TestStreamStopParams({Direction::Input, AudioApi::AAudio, PerformanceMode::PowerSaving}),
                TestStreamStopParams({Direction::Input, AudioApi::OpenSLES, PerformanceMode::LowLatency}),
                TestStreamStopParams({Direction::Input, AudioApi::OpenSLES, PerformanceMode::None}),
                TestStreamStopParams({Direction::Input, AudioApi::OpenSLES, PerformanceMode::PowerSaving})
        )
);
