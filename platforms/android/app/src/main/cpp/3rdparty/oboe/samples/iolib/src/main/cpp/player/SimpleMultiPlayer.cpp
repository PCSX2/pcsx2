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

#include <android/log.h>

// parselib includes
#include <stream/MemInputStream.h>
#include <wav/WavStreamReader.h>

// local includes
#include "OneShotSampleSource.h"
#include "SimpleMultiPlayer.h"

static const char* TAG = "SimpleMultiPlayer";

using namespace oboe;
using namespace parselib;

namespace iolib {

constexpr int32_t kBufferSizeInBursts = 2; // Use 2 bursts as the buffer size (double buffer)

SimpleMultiPlayer::SimpleMultiPlayer()
  : mChannelCount(0), mOutputReset(false), mSampleRate(0), mNumSampleBuffers(0)
{}

DataCallbackResult SimpleMultiPlayer::MyDataCallback::onAudioReady(AudioStream *oboeStream,
                                                                   void *audioData,
                                                                   int32_t numFrames) {

    StreamState streamState = oboeStream->getState();
    if (streamState != StreamState::Open && streamState != StreamState::Started) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "  streamState:%d", streamState);
    }
    if (streamState == StreamState::Disconnected) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "  streamState::Disconnected");
    }

    memset(audioData, 0, static_cast<size_t>(numFrames) * static_cast<size_t>
            (mParent->mChannelCount) * sizeof(float));

    // OneShotSampleSource* sources = mSampleSources.get();
    for(int32_t index = 0; index < mParent->mNumSampleBuffers; index++) {
        if (mParent->mSampleSources[index]->isPlaying()) {
            mParent->mSampleSources[index]->mixAudio((float*)audioData, mParent->mChannelCount,
                                                     numFrames);
        }
    }

    return DataCallbackResult::Continue;
}

void SimpleMultiPlayer::MyErrorCallback::onErrorAfterClose(AudioStream *oboeStream, Result error) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "==== onErrorAfterClose() error:%d", error);

    mParent->resetAll();
    if (mParent->openStream() && mParent->startStream()) {
        mParent->mOutputReset = true;
    }
}

bool SimpleMultiPlayer::openStream(PerformanceMode performanceMode) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "openStream()");

    // Use shared_ptr to prevent use of a deleted callback.
    mDataCallback = std::make_shared<MyDataCallback>(this);
    mErrorCallback = std::make_shared<MyErrorCallback>(this);

    // Create an audio stream
    AudioStreamBuilder builder;
    builder.setChannelCount(mChannelCount);
    // we will resample source data to device rate, so take default sample rate
    builder.setDataCallback(mDataCallback);
    builder.setErrorCallback(mErrorCallback);
    builder.setPerformanceMode(performanceMode);
    builder.setSharingMode(SharingMode::Exclusive);
    builder.setSampleRateConversionQuality(SampleRateConversionQuality::Medium);

    Result result = builder.openStream(mAudioStream);
    if (result != Result::OK){
        __android_log_print(
                ANDROID_LOG_ERROR,
                TAG,
                "openStream failed. Error: %s", convertToText(result));
        return false;
    }

    // Reduce stream latency by setting the buffer size to a multiple of the burst size
    // Note: this will fail with ErrorUnimplemented if we are using a callback with OpenSL ES
    // See oboe::AudioStreamBuffered::setBufferSizeInFrames
    if (mAudioStream->getPerformanceMode() != oboe::PerformanceMode::PowerSavingOffloaded ||
            !OboeExtensions::isMMapUsed(mAudioStream.get())) {
        result = mAudioStream->setBufferSizeInFrames(mAudioStream->getFramesPerBurst() * kBufferSizeInBursts);
    }

    if (result != Result::OK) {
        __android_log_print(
                ANDROID_LOG_WARN,
                TAG,
                "setBufferSizeInFrames failed. Error: %s", convertToText(result));
    }

    mSampleRate = mAudioStream->getSampleRate();

    return true;
}

bool SimpleMultiPlayer::startStream(PerformanceMode performanceMode) {
    int tryCount = 0;
    while (tryCount < 3) {
        bool wasOpenSuccessful = true;
        // Assume that openStream() was called successfully before startStream() call.
        if (tryCount > 0) {
            usleep(20 * 1000); // Sleep between tries to give the system time to settle.
            wasOpenSuccessful = openStream(performanceMode); // Try to open the stream again after the first try.
        }
        if (wasOpenSuccessful) {
            Result result = mAudioStream->requestStart();
            if (result != Result::OK){
                __android_log_print(
                        ANDROID_LOG_ERROR,
                        TAG,
                        "requestStart failed. Error: %s", convertToText(result));
                mAudioStream->close();
                mAudioStream.reset();
            } else {
                return true;
            }
        }
        tryCount++;
    }

    return false;
}

void SimpleMultiPlayer::setupAudioStream(int32_t channelCount, oboe::PerformanceMode performanceMode) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "setupAudioStream()");
    mChannelCount = channelCount;

    openStream(performanceMode);
}

void SimpleMultiPlayer::teardownAudioStream() {
    __android_log_print(ANDROID_LOG_INFO, TAG, "teardownAudioStream()");
    // tear down the player
    if (mAudioStream) {
        mAudioStream->stop();
        mAudioStream->close();
        mAudioStream.reset();
    }
}

void SimpleMultiPlayer::addSampleSource(SampleSource* source, SampleBuffer* buffer) {
    buffer->resampleData(mSampleRate);

    mSampleBuffers.push_back(buffer);
    mSampleSources.push_back(source);
    mNumSampleBuffers++;
}

void SimpleMultiPlayer::unloadSampleData() {
    __android_log_print(ANDROID_LOG_INFO, TAG, "unloadSampleData()");
    resetAll();

    for (int32_t bufferIndex = 0; bufferIndex < mNumSampleBuffers; bufferIndex++) {
        delete mSampleBuffers[bufferIndex];
        delete mSampleSources[bufferIndex];
    }

    mSampleBuffers.clear();
    mSampleSources.clear();

    mNumSampleBuffers = 0;
}

void SimpleMultiPlayer::triggerDown(int32_t index, oboe::PerformanceMode /* performanceMode */) {
    if (index < mNumSampleBuffers) {
        mSampleSources[index]->setPlayMode();
    }
}

void SimpleMultiPlayer::triggerUp(int32_t index) {
    if (index < mNumSampleBuffers) {
        mSampleSources[index]->setStopMode();
    }
}

void SimpleMultiPlayer::resetAll() {
    for (int32_t bufferIndex = 0; bufferIndex < mNumSampleBuffers; bufferIndex++) {
        mSampleSources[bufferIndex]->setStopMode();
    }
}

void SimpleMultiPlayer::setPan(int index, float pan) {
    mSampleSources[index]->setPan(pan);
}

float SimpleMultiPlayer::getPan(int index) {
    return mSampleSources[index]->getPan();
}

void SimpleMultiPlayer::setGain(int index, float gain) {
    mSampleSources[index]->setGain(gain);
}

float SimpleMultiPlayer::getGain(int index) {
    return mSampleSources[index]->getGain();
}

void SimpleMultiPlayer::setLoopMode(int index, bool isLoopMode) {
    mSampleSources[index]->setLoopMode(isLoopMode);
}

}
