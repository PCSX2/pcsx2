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

#ifndef OBOETESTER_TEST_ROUTING_CRASH_H
#define OBOETESTER_TEST_ROUTING_CRASH_H

#include "oboe/Oboe.h"
#include <thread>

/**
 * Try to cause a crash by changing routing during a data callback.
 * We use Use::VoiceCommunication for the stream and
 * setSpeakerPhoneOn(b) to force a routing change.
 * This works best when connected to a BT headset.
 */
class TestRoutingCrash {
public:

    int32_t start(bool useInput);
    int32_t stop();

    int32_t getSleepTimeMicros() {
        return (int32_t) (averageSleepTimeMicros.load());
    }

protected:

    std::atomic<double> averageSleepTimeMicros{0};

private:

    class MyDataCallback : public oboe::AudioStreamDataCallback {    public:

        MyDataCallback(TestRoutingCrash *parent): mParent(parent) {}

        oboe::DataCallbackResult onAudioReady(
                oboe::AudioStream *audioStream,
                void *audioData,
                int32_t numFrames) override;
    private:
        TestRoutingCrash *mParent;
        // For sine generator.
        float mPhase = 0.0f;
        static constexpr float kPhaseIncrement = 2.0f * (float) M_PI * 440.0f / 48000.0f;
        float mInputSum = 0.0f; // For saving input data sum to prevent over-optimization.
    };

    std::shared_ptr<oboe::AudioStream> mStream;
    std::shared_ptr<MyDataCallback> mDataCallback;

    static constexpr int kChannelCount = 1;
};

#endif //OBOETESTER_TEST_ROUTING_CRASH_H
