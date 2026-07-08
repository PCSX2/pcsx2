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

#include <stdlib.h>
#include <aaudio/AAudioExtensions.h>

#include "common/OboeDebug.h"
#include "oboe/AudioClock.h"
#include "TestRapidCycle.h"

using namespace oboe;

// start a thread to cycle through stream tests
int32_t TestRapidCycle::start(bool useOpenSL) {
    mThreadEnabled = true;
    mCycleCount = 0;
    mCycleThread = std::thread([this, useOpenSL]() {
        cycleRapidly(useOpenSL);
    });
    return 0;
}
int32_t TestRapidCycle::stop() {
    mThreadEnabled = false;
    mCycleThread.join();
    return 0;
}

void TestRapidCycle::cycleRapidly(bool useOpenSL) {
    while(mThreadEnabled && (oneCycle(useOpenSL) == 0));
}

int32_t TestRapidCycle::oneCycle(bool useOpenSL) {
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
    if (result != oboe::Result::OK) {
        return (int32_t) result;
    }

    mStream->setDelayBeforeCloseMillis(0);

    result = mStream->requestStart();
    if (result != oboe::Result::OK) {
        mStream->close();
        return (int32_t) result;
    }
// Sleep for some random time.
    int32_t durationMicros = (int32_t)(drand48() * kMaxSleepMicros);
    LOGD("TestRapidCycle::oneCycle() - Sleep for %d micros", durationMicros);
    usleep(durationMicros);
    LOGD("TestRapidCycle::oneCycle() - Woke up, close stream");
    mDataCallback->returnStop = true;
    result = mStream->close();
    return (int32_t) result;
}

// Callback that sleeps then touches the audio buffer.
DataCallbackResult TestRapidCycle::MyDataCallback::onAudioReady(
        AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) {
    float *floatData = (float *) audioData;
    const int numSamples = numFrames * kChannelCount;

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
