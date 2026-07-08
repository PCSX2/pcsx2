/*
 * Copyright 2021 The Android Open Source Project
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

#include <atomic>
#include <tuple>

#include <gtest/gtest.h>
#include <oboe/Oboe.h>


// Test returning DataCallbackResult::Stop from a callback.
using namespace oboe;

static constexpr int kTimeoutInNanos = 500 * kNanosPerMillisecond;

class ReturnStopCallback : public AudioStreamDataCallback {
public:
    DataCallbackResult onAudioReady(AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
        return (++callbackCount < kMaxCallbacks) ? DataCallbackResult::Continue : DataCallbackResult::Stop;
    }

    void reset() {
        callbackCount = 0;
    }

    int getMaxCallbacks() const { return kMaxCallbacks; }

    std::atomic<int> callbackCount{0};
    
private:
    // I get strange linker errors with GTest if I try to reference this directly.
    static constexpr int kMaxCallbacks = 40;
};

using StreamReturnStopParams = std::tuple<Direction, AudioApi, PerformanceMode, bool>;

class StreamReturnStop : public ::testing::Test,
                         public ::testing::WithParamInterface<StreamReturnStopParams> {

protected:
    void TearDown() override;

    AudioStreamBuilder mBuilder;
    std::shared_ptr<AudioStream> mStream;
};

void StreamReturnStop::TearDown() {
    if (mStream) {
        mStream->close();
    }
}

TEST_P(StreamReturnStop, VerifyStreamReturnStop) {
    const Direction direction = std::get<0>(GetParam());
    const AudioApi audioApi = std::get<1>(GetParam());
    const PerformanceMode performanceMode = std::get<2>(GetParam());
    const bool useRequestStart = std::get<3>(GetParam());

    ReturnStopCallback *callback = new ReturnStopCallback();
    mBuilder.setDirection(direction)
            ->setFormat(AudioFormat::I16)
            ->setPerformanceMode(performanceMode)
            ->setDataCallback(callback);
    if (mBuilder.isAAudioRecommended()) {
        mBuilder.setAudioApi(audioApi);
    }
    Result r = mBuilder.openStream(mStream);
    ASSERT_EQ(r, Result::OK) << "Failed to open stream. " << convertToText(r);

    // Start and stop several times.
    for (int i = 0; i < 3; i++) {
        callback->reset();
        // Oboe has two ways to start a stream.
        if (useRequestStart) {
            r = mStream->requestStart();
        } else {
            r = mStream->start();
        }
        ASSERT_EQ(r, Result::OK) << "Failed to start stream. " << convertToText(r);
    
        // Wait for callbacks to complete.
        const int kMaxCallbackPeriodMillis = 500;
        const int kPollPeriodMillis = 20;
        int timeout = 2 * callback->getMaxCallbacks() * kMaxCallbackPeriodMillis / kPollPeriodMillis;
        do {
            usleep(kPollPeriodMillis * 1000);
        } while (callback->callbackCount < callback->getMaxCallbacks() && timeout-- > 0);
        EXPECT_GT(timeout, 0) << "timed out waiting for enough callbacks";
        
        StreamState next = StreamState::Unknown;
        r = mStream->waitForStateChange(StreamState::Started, &next, kTimeoutInNanos);
        EXPECT_EQ(r, Result::OK) << "waitForStateChange(Started) timed out. " << convertToText(r);
        r = mStream->waitForStateChange(StreamState::Stopping, &next, kTimeoutInNanos);
        EXPECT_EQ(r, Result::OK) << "waitForStateChange(Stopping) timed out. " << convertToText(r);
        EXPECT_EQ(next, StreamState::Stopped) << "Stream not in state Stopped, was " << convertToText(next);
        
        EXPECT_EQ(callback->callbackCount, callback->getMaxCallbacks()) << "Too many callbacks = " << callback->callbackCount;

        const int kOboeStartStopSleepMSec = 10;
        usleep(kOboeStartStopSleepMSec * 1000); // avoid race condition in emulator
    }

    ASSERT_EQ(Result::OK, mStream->close());
}

INSTANTIATE_TEST_SUITE_P(
        StreamReturnStopTest,
        StreamReturnStop,
        ::testing::Values(
                // Last boolean is true if requestStart() should be called instead of start().
                StreamReturnStopParams({Direction::Output, AudioApi::AAudio, PerformanceMode::LowLatency, true}),
                StreamReturnStopParams({Direction::Output, AudioApi::AAudio, PerformanceMode::LowLatency, false}),
                StreamReturnStopParams({Direction::Output, AudioApi::AAudio, PerformanceMode::None, true}),
                StreamReturnStopParams({Direction::Output, AudioApi::AAudio, PerformanceMode::None, false}),
                StreamReturnStopParams({Direction::Output, AudioApi::OpenSLES, PerformanceMode::LowLatency, true}),
                StreamReturnStopParams({Direction::Output, AudioApi::OpenSLES, PerformanceMode::LowLatency, false}),
                StreamReturnStopParams({Direction::Input, AudioApi::AAudio, PerformanceMode::LowLatency, true}),
                StreamReturnStopParams({Direction::Input, AudioApi::AAudio, PerformanceMode::LowLatency, false})
                )
        );
