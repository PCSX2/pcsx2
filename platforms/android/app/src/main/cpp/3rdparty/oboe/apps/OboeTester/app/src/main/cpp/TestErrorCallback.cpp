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

#include <stdlib.h>

#include "common/OboeDebug.h"
#include "TestErrorCallback.h"

using namespace oboe;

oboe::Result TestErrorCallback::open() {
    mCallbackMagic = 0;
    mDataCallback = std::make_shared<MyDataCallback>();
    mErrorCallback = std::make_shared<MyErrorCallback>(this);
    AudioStreamBuilder builder;
    oboe::Result result = builder.setSharingMode(oboe::SharingMode::Exclusive)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setFormat(oboe::AudioFormat::Float)
            ->setChannelCount(kChannelCount)
#if 0
            ->setDataCallback(mDataCallback.get())
            ->setErrorCallback(mErrorCallback.get()) // This can lead to a crash or FAIL.
#else
            ->setDataCallback(mDataCallback)
            ->setErrorCallback(mErrorCallback) // shared_ptr avoids a crash
#endif
            ->openStream(mStream);
    return result;
}

oboe::Result TestErrorCallback::start() {
    return mStream->requestStart();
}

oboe::Result TestErrorCallback::stop() {
    return mStream->requestStop();
}

oboe::Result TestErrorCallback::close() {
    return mStream->close();
}

int TestErrorCallback::test() {
    oboe::Result result = open();
    if (result != oboe::Result::OK) {
        return (int) result;
    }
    return (int) start();
}

DataCallbackResult TestErrorCallback::MyDataCallback::onAudioReady(
        AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) {
    float *output = (float *) audioData;
    // Fill buffer with random numbers to create "white noise".
    int numSamples = numFrames * kChannelCount;
    for (int i = 0; i < numSamples; i++) {
        *output++ = (float)((drand48() - 0.5) * 0.2);
    }
    return oboe::DataCallbackResult::Continue;
}
