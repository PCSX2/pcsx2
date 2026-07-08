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

#ifndef OBOETESTER_TEST_RAPID_CYCLE_H
#define OBOETESTER_TEST_RAPID_CYCLE_H

#include "oboe/Oboe.h"
#include <thread>


/**
 * Try to cause a crash by changing routing during a data callback.
 * We use Use::VoiceCommunication for the stream and
 * setSpeakerPhoneOn(b) to force a routing change.
 * This works best when connected to a BT headset.
 */
class TestRapidCycle {
public:

    int32_t start(bool useOpenSL);
    int32_t stop();

    int32_t getCycleCount() {
        return mCycleCount.load();
    }

private:

    void cycleRapidly(bool useOpenSL);
    int32_t oneCycle(bool useOpenSL);

    class MyDataCallback : public oboe::AudioStreamDataCallback {    public:

        MyDataCallback() {}

        oboe::DataCallbackResult onAudioReady(
                oboe::AudioStream *audioStream,
                void *audioData,
                int32_t numFrames) override;

        bool returnStop = false;
    };

    std::shared_ptr<oboe::AudioStream> mStream;
    std::shared_ptr<MyDataCallback> mDataCallback;
    std::atomic<int32_t> mCycleCount{0};
    std::atomic<bool> mThreadEnabled{false};
    std::thread mCycleThread;

    static constexpr int kChannelCount = 1;
    static constexpr int kMaxSleepMicros = 25000;
};

#endif //OBOETESTER_TEST_RAPID_CYCLE_H
