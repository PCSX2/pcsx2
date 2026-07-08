/*
 * Copyright 2019 The Android Open Source Project
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

class TestStreamWaitState : public ::testing::Test {

protected:

    void SetUp(){
        mBuilder.setPerformanceMode(PerformanceMode::None);
        mBuilder.setDirection(Direction::Output);
    }

    bool openStream(Direction direction, PerformanceMode perfMode) {
        mBuilder.setDirection(direction);
        mBuilder.setPerformanceMode(perfMode);
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

    bool closeStream() {
        Result r = mStream->close();
        EXPECT_TRUE(r == Result::OK || r == Result::ErrorClosed) <<
            "Failed to close stream. " << convertToText(r);
        return (r == Result::OK || r == Result::ErrorClosed);
    }

    // if zero then don't wait for a state change
    void checkWaitForStateChangeTimeout(int64_t timeout = kTimeoutInNanos) {
        StreamState next = StreamState::Unknown;
        Result result = mStream->waitForStateChange(mStream->getState(), &next, timeout);
        EXPECT_EQ(Result::ErrorTimeout, result);
    }

    void checkStopWhileWaiting() {
        StreamState next = StreamState::Unknown;
        auto r = mStream->requestStart();
        EXPECT_EQ(r, Result::OK);
        r = mStream->waitForStateChange(StreamState::Starting, &next, kTimeoutInNanos);
        EXPECT_EQ(r, Result::OK);
        EXPECT_EQ(next, StreamState::Started) << "next = " << convertToText(next);

        std::shared_ptr<AudioStream> str = mStream;

        std::thread stopper([str] {
            usleep(200 * 1000);
            str->requestStop();
        });

        r = mStream->waitForStateChange(StreamState::Started, &next, 1000 * kNanosPerMillisecond);
        stopper.join();
        EXPECT_EQ(r, Result::OK);
        // May have caught in stopping transition. Wait for full stop.
        if (next == StreamState::Stopping) {
            r = mStream->waitForStateChange(StreamState::Stopping, &next, 1000 * kNanosPerMillisecond);
            EXPECT_EQ(r, Result::OK);
        }
        ASSERT_EQ(next, StreamState::Stopped) << "next = " << convertToText(next);
    }

    void checkCloseWhileWaiting() {
        StreamState next = StreamState::Unknown;
        auto r = mStream->requestStart();
        EXPECT_EQ(r, Result::OK);
        r = mStream->waitForStateChange(StreamState::Starting, &next, kTimeoutInNanos);
        EXPECT_EQ(r, Result::OK);
        EXPECT_EQ(next, StreamState::Started) << "next = " << convertToText(next);

        std::shared_ptr<AudioStream> str = mStream;

        std::thread closer([str] {
            usleep(200 * 1000);
            str->close();
        });

        r = mStream->waitForStateChange(StreamState::Started, &next, 1000 * kNanosPerMillisecond);
        closer.join();
        // You might catch this at any point in stopping or closing.
        EXPECT_TRUE(r == Result::OK || r == Result::ErrorClosed) << "r = " << convertToText(r);
        ASSERT_TRUE(next == StreamState::Stopping
                    || next == StreamState::Stopped
                    || next == StreamState::Pausing
                    || next == StreamState::Paused
                    || next == StreamState::Closed) << "next = " << convertToText(next);
    }

    AudioStreamBuilder mBuilder;
    std::shared_ptr<AudioStream> mStream;
    static constexpr int kTimeoutInNanos = 100 * kNanosPerMillisecond;

};

// Test return of error timeout when zero passed as the timeoutNanos.
TEST_F(TestStreamWaitState, OutputLowWaitZero) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::LowLatency));
    checkWaitForStateChangeTimeout(0);
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamWaitState, OutputNoneWaitZero) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::None));
    checkWaitForStateChangeTimeout(0);
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamWaitState, OutputLowWaitZeroSLES) {
    AudioStreamBuilder builder;
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setAudioApi(AudioApi::OpenSLES);
    ASSERT_TRUE(openStream(builder));
    checkWaitForStateChangeTimeout(0);
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamWaitState, OutputNoneWaitZeroSLES) {
    AudioStreamBuilder builder;
    builder.setPerformanceMode(PerformanceMode::None);
    builder.setAudioApi(AudioApi::OpenSLES);
    ASSERT_TRUE(openStream(builder));
    checkWaitForStateChangeTimeout(0);
    ASSERT_TRUE(closeStream());
}

// Test actual timeout.
TEST_F(TestStreamWaitState, OutputLowWaitNonZero) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::LowLatency));
    checkWaitForStateChangeTimeout();
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamWaitState, OutputNoneWaitNonZero) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::None));
    checkWaitForStateChangeTimeout();
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamWaitState, OutputLowWaitNonZeroSLES) {
    AudioStreamBuilder builder;
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setAudioApi(AudioApi::OpenSLES);
    ASSERT_TRUE(openStream(builder));
    checkWaitForStateChangeTimeout();
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamWaitState, OutputNoneWaitNonZeroSLES) {
    AudioStreamBuilder builder;
    builder.setPerformanceMode(PerformanceMode::None);
    builder.setAudioApi(AudioApi::OpenSLES);
    ASSERT_TRUE(openStream(builder));
    checkWaitForStateChangeTimeout();
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamWaitState, OutputLowStopWhileWaiting) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::LowLatency));
    checkStopWhileWaiting();
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamWaitState, OutputNoneStopWhileWaiting) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::LowLatency));
    checkStopWhileWaiting();
    ASSERT_TRUE(closeStream());
}


TEST_F(TestStreamWaitState, OutputLowStopWhileWaitingSLES) {
    AudioStreamBuilder builder;
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setAudioApi(AudioApi::OpenSLES);
    ASSERT_TRUE(openStream(builder));
    checkStopWhileWaiting();
    ASSERT_TRUE(closeStream());
}


TEST_F(TestStreamWaitState, OutputLowCloseWhileWaiting) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::LowLatency));
    checkCloseWhileWaiting();
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamWaitState, OutputNoneCloseWhileWaiting) {
    ASSERT_TRUE(openStream(Direction::Output, PerformanceMode::None));
    checkCloseWhileWaiting();
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamWaitState, InputLowCloseWhileWaiting) {
    ASSERT_TRUE(openStream(Direction::Input, PerformanceMode::LowLatency));
    checkCloseWhileWaiting();
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamWaitState, InputNoneCloseWhileWaiting) {
    ASSERT_TRUE(openStream(Direction::Input, PerformanceMode::None));
    checkCloseWhileWaiting();
    ASSERT_TRUE(closeStream());
}

TEST_F(TestStreamWaitState, OutputNoneCloseWhileWaitingSLES) {
    AudioStreamBuilder builder;
    builder.setPerformanceMode(PerformanceMode::None);
    builder.setAudioApi(AudioApi::OpenSLES);
    ASSERT_TRUE(openStream(builder));
    checkCloseWhileWaiting();
    ASSERT_TRUE(closeStream());
}


TEST_F(TestStreamWaitState, OutputLowCloseWhileWaitingSLES) {
    AudioStreamBuilder builder;
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setAudioApi(AudioApi::OpenSLES);
    ASSERT_TRUE(openStream(builder));
    checkCloseWhileWaiting();
    ASSERT_TRUE(closeStream());
}
