/*
 * Copyright 2023 The Android Open Source Project
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

#ifndef OBOETESTER_TEST_COLD_START_LATENCY_H
#define OBOETESTER_TEST_COLD_START_LATENCY_H

#include "oboe/Oboe.h"
#include <thread>

/**
 * Test for getting the cold start latency
 */
class TestColdStartLatency {
public:

    int32_t open(bool useInput, bool useLowLatency, bool useMmap, bool useExclusive);
    int32_t start();
    int32_t close();

    void waitForValidTimestamp();

    int32_t getColdStartTimeMicros();

    int32_t getOpenTimeMicros() {
        return (int32_t) (mOpenTimeMicros.load());
    }

    int32_t getStartTimeMicros() {
        return (int32_t) (mStartTimeMicros.load());
    }

    int32_t getDeviceId() {
        return mDeviceId;
    }

protected:
    std::atomic<int64_t> mBeginStartNanos{0};
    std::atomic<double> mOpenTimeMicros{0};
    std::atomic<double> mStartTimeMicros{0};
    std::atomic<double> mColdStartTimeMicros{0};
    std::atomic<int32_t> mDeviceId{0};

private:

    class MyDataCallback : public oboe::AudioStreamDataCallback {    public:

        MyDataCallback() {}

        oboe::DataCallbackResult onAudioReady(
                oboe::AudioStream *audioStream,
                void *audioData,
                int32_t numFrames) override;
    private:
        // For sine generator.
        float mPhase = 0.0f;
        static constexpr float kPhaseIncrement = 2.0f * (float) M_PI * 440.0f / 48000.0f;
    };

    std::shared_ptr<oboe::AudioStream> mStream;
    std::shared_ptr<MyDataCallback> mDataCallback;

    static constexpr int kChannelCount = 1;
    static constexpr int kPollPeriodMillis = 1;
};

#endif //OBOETESTER_TEST_COLD_START_LATENCY_H
