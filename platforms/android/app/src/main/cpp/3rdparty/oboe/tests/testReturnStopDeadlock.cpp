/*
 * Copyright 2024 The Android Open Source Project
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
#include <thread>
#include <future>

// Test returning DataCallbackResult::Stop from a callback.
using namespace oboe;

// Test whether there is a deadlock when stopping streams.
// See Issue #2059

class TestReturnStopDeadlock  : public ::testing::Test {
public:

    void start(bool useOpenSL);
    void stop();

    int32_t getCycleCount() {
        return mCycleCount.load();
    }

protected:
    void TearDown() override;
    
private:

    void cycleRapidly(bool useOpenSL);

    class MyDataCallback : public oboe::AudioStreamDataCallback {    public:

        MyDataCallback() {}

        oboe::DataCallbackResult onAudioReady(
                oboe::AudioStream *audioStream,
                void *audioData,
                int32_t numFrames) override;

        std::atomic<bool> returnStop = false;
        std::atomic<int32_t> callbackCount{0};
    };

    std::shared_ptr<oboe::AudioStream> mStream;
    std::shared_ptr<MyDataCallback> mDataCallback;
    std::atomic<int32_t> mCycleCount{0};
    std::atomic<bool> mThreadEnabled{false};
    std::thread mCycleThread;

    static constexpr int kChannelCount = 1;
    static constexpr int kMaxSleepMicros = 25000;
};

// start a thread to cycle through stream tests
void TestReturnStopDeadlock::start(bool useOpenSL) {
    mThreadEnabled = true;
    mCycleCount = 0;
    mCycleThread = std::thread([this, useOpenSL]() {
        cycleRapidly(useOpenSL);
    });
}

void TestReturnStopDeadlock::stop() {
    mThreadEnabled = false;
    // Terminate the thread with a timeout.
    const int timeout = 1;
    auto future = std::async(std::launch::async, &std::thread::join, &mCycleThread);
    ASSERT_NE(future.wait_for(std::chrono::seconds(timeout)), std::future_status::timeout)
        << " join() timed out! cycles = " << getCycleCount();
}

void TestReturnStopDeadlock::TearDown() {
    if (mStream) {
        mStream->close();
    }
}

void TestReturnStopDeadlock::cycleRapidly(bool useOpenSL) {
    while(mThreadEnabled) {
        mCycleCount++;
        mDataCallback = std::make_shared<MyDataCallback>();

        AudioStreamBuilder builder;
        oboe::Result result = builder.setFormat(oboe::AudioFormat::Float)
                ->setAudioApi(useOpenSL ? oboe::AudioApi::OpenSLES : oboe::AudioApi::AAudio)
                ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
                ->setChannelCount(kChannelCount)
                ->setDataCallback(mDataCallback)
                ->setUsage(oboe::Usage::Notification)
                ->openStream(mStream);
        ASSERT_EQ(result, oboe::Result::OK);

        mStream->setDelayBeforeCloseMillis(0);

        result = mStream->requestStart();
        ASSERT_EQ(result, oboe::Result::OK);

        // Sleep for some random time.
        int countdown = 100;
        while ((mDataCallback->callbackCount < 4) && (--countdown > 0)) {
            int32_t durationMicros = (int32_t)(drand48() * kMaxSleepMicros);
            usleep(durationMicros);
        }
        mDataCallback->returnStop = true;
        result = mStream->close();
        ASSERT_EQ(result, oboe::Result::OK);
        mStream = nullptr;
        ASSERT_GT(mDataCallback->callbackCount, 1) << " cycleCount = " << mCycleCount;
    }
}

// Callback that returns Continue or Stop
DataCallbackResult TestReturnStopDeadlock::MyDataCallback::onAudioReady(
        AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) {
    float *floatData = (float *) audioData;
    const int numSamples = numFrames * kChannelCount;
    callbackCount++;

    // Fill buffer with white noise.
    for (int i = 0; i < numSamples; i++) {
        floatData[i] = ((float) drand48() - 0.5f) * 2 * 0.1f;
    }
    usleep(500); // half a millisecond
    if (returnStop) {
        usleep(20 * 1000);
        return DataCallbackResult::Stop;
    } else {
        return DataCallbackResult::Continue;
    }
}

TEST_F(TestReturnStopDeadlock, RapidCycleAAudio){
    start(false);
    usleep(3000 * 1000);
    stop();
}

TEST_F(TestReturnStopDeadlock, RapidCycleOpenSL){
    start(true);
    usleep(3000 * 1000);
    stop();
}
