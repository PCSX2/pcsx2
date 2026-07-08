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

static const char *TAG = "SimpleNoiseMaker";

#include <android/log.h>

#include <SimpleNoiseMaker.h>

using namespace oboe;

oboe::Result SimpleNoiseMaker::open() {
    // Use shared_ptr to prevent use of a deleted callback.
    mDataCallback = std::make_shared<MyDataCallback>();
    mErrorCallback = std::make_shared<MyErrorCallback>(this);

    AudioStreamBuilder builder;
    oboe::Result result = builder.setSharingMode(oboe::SharingMode::Exclusive)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setFormat(oboe::AudioFormat::Float)
            ->setChannelCount(kChannelCount)
            ->setDataCallback(mDataCallback)
            ->setErrorCallback(mErrorCallback)
                    // Open using a shared_ptr.
            ->openStream(mStream);
    return result;
}

oboe::Result SimpleNoiseMaker::start() {
    return mStream->requestStart();
}

oboe::Result SimpleNoiseMaker::stop() {
    return mStream->requestStop();
}

oboe::Result SimpleNoiseMaker::close() {
    return mStream->close();
}

/**
 * This callback method will be called from a high priority audio thread.
 * It should only do math and not do any blocking operations like
 * reading or writing files, memory allocation, or networking.
 * @param audioStream
 * @param audioData pointer to an array of samples to be filled
 * @param numFrames number of frames needed
 * @return
 */
DataCallbackResult SimpleNoiseMaker::MyDataCallback::onAudioReady(
        AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) {
    // We requested float when we built the stream.
    float *output = (float *) audioData;
    // Fill buffer with random numbers to create "white noise".
    int numSamples = numFrames * kChannelCount;
    for (int i = 0; i < numSamples; i++) {
        // drand48() returns a random number between 0.0 and 1.0.
        // Center and scale it to a reasonable value.
        *output++ = (float) ((drand48() - 0.5) * 0.6);
    }
    return oboe::DataCallbackResult::Continue;
}

void SimpleNoiseMaker::MyErrorCallback::onErrorAfterClose(oboe::AudioStream *oboeStream,
                                                          oboe::Result error) {
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "%s() - error = %s",
                        __func__,
                        oboe::convertToText(error)
    );
    // Try to open and start a new stream after a disconnect.
    if (mParent->open() == Result::OK) {
        mParent->start();
    }
}
