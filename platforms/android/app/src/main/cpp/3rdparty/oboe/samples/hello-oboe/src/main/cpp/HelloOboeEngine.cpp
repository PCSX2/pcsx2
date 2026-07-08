/**
 * Copyright 2017 The Android Open Source Project
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


#include <inttypes.h>
#include <memory>

#include <Oscillator.h>

#include "HelloOboeEngine.h"
#include "SoundGenerator.h"


/**
 * Main audio engine for the HelloOboe sample. It is responsible for:
 *
 * - Creating a callback object which is supplied when constructing the audio stream, and will be
 * called when the stream starts
 * - Restarting the stream when user-controllable properties (Audio API, channel count etc) are
 * changed, and when the stream is disconnected (e.g. when headphones are attached)
 * - Calculating the audio latency of the stream
 *
 */
HelloOboeEngine::HelloOboeEngine()
        : mLatencyCallback(std::make_shared<LatencyTuningCallback>()),
        mErrorCallback(std::make_shared<DefaultErrorCallback>(*this)) {
}

double HelloOboeEngine::getCurrentOutputLatencyMillis() {
    if (!mIsLatencyDetectionSupported) return -1.0;

    std::lock_guard<std::mutex> lock(mLock);
    if (!mStream) return -1.0;

    oboe::ResultWithValue<double> latencyResult = mStream->calculateLatencyMillis();
    if (latencyResult) {
        return latencyResult.value();
    } else {
        LOGE("Error calculating latency: %s", oboe::convertToText(latencyResult.error()));
        return -1.0;
    }
}

void HelloOboeEngine::setBufferSizeInBursts(int32_t numBursts) {
    std::lock_guard<std::mutex> lock(mLock);
    if (!mStream) return;

    mLatencyCallback->setBufferTuneEnabled(numBursts == kBufferSizeAutomatic);
    auto result = mStream->setBufferSizeInFrames(
            numBursts * mStream->getFramesPerBurst());
    if (result) {
        LOGD("Buffer size successfully changed to %d", result.value());
    } else {
        LOGW("Buffer size could not be changed, %d", result.error());
    }
}

bool HelloOboeEngine::isLatencyDetectionSupported() {
    return mIsLatencyDetectionSupported;
}

bool HelloOboeEngine::isAAudioRecommended() {
    return oboe::AudioStreamBuilder::isAAudioRecommended();
}

void HelloOboeEngine::tap(bool isDown) {
    if (mAudioSource) {
        mAudioSource->tap(isDown);
    }
}

oboe::Result HelloOboeEngine::openPlaybackStream() {
    oboe::AudioStreamBuilder builder;
    oboe::Result result = builder.setSharingMode(oboe::SharingMode::Exclusive)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setFormat(oboe::AudioFormat::Float)
        ->setFormatConversionAllowed(true)
        ->setDataCallback(mLatencyCallback)
        ->setErrorCallback(mErrorCallback)
        ->setAudioApi(mAudioApi)
        ->setChannelCount(mChannelCount)
        ->setDeviceId(mDeviceId)
        ->openStream(mStream);
    if (result == oboe::Result::OK) {
        mChannelCount = mStream->getChannelCount();
    }
    return result;
}

void HelloOboeEngine::restart() {
    // The stream will have already been closed by the error callback.
    mLatencyCallback->reset();
    start();
}

oboe::Result HelloOboeEngine::start(oboe::AudioApi audioApi, int deviceId, int channelCount) {
    mAudioApi = audioApi;
    mDeviceId = deviceId;
    mChannelCount = channelCount;
    return start();
}

oboe::Result HelloOboeEngine::start() {
    std::lock_guard<std::mutex> lock(mLock);
    oboe::Result result = oboe::Result::OK;
    // It is possible for a stream's device to become disconnected during the open or between
    // the Open and the Start.
    // So if it fails to start, close the old stream and try again.
    int tryCount = 0;
    do {
        if (tryCount > 0) {
            usleep(20 * 1000); // Sleep between tries to give the system time to settle.
        }
        mIsLatencyDetectionSupported = false;
        result = openPlaybackStream();
        if (result == oboe::Result::OK) {
            mAudioSource = std::make_shared<SoundGenerator>(mStream->getSampleRate(),
                                                            mStream->getChannelCount());
            mLatencyCallback->setSource(
                    std::dynamic_pointer_cast<IRenderableAudio>(mAudioSource));

            LOGD("Stream opened: AudioAPI = %d, channelCount = %d, deviceID = %d",
                 mStream->getAudioApi(),
                 mStream->getChannelCount(),
                 mStream->getDeviceId());

            result = mStream->requestStart();
            if (result != oboe::Result::OK) {
                LOGE("Error starting playback stream. Error: %s", oboe::convertToText(result));
                mStream->close();
                mStream.reset();
            } else {
                mIsLatencyDetectionSupported = (mStream->getTimestamp((CLOCK_MONOTONIC)) !=
                                                oboe::Result::ErrorUnimplemented);
            }
        } else {
            LOGE("Error creating playback stream. Error: %s", oboe::convertToText(result));
        }
    } while (result != oboe::Result::OK && tryCount++ < 3);
    return result;
}

oboe::Result HelloOboeEngine::stop() {
    oboe::Result result = oboe::Result::OK;
    // Stop, close and delete in case not already closed.
    std::lock_guard<std::mutex> lock(mLock);
    if (mStream) {
        result = mStream->stop();
        mStream->close();
        mStream.reset();
    }
    return result;
}

oboe::Result HelloOboeEngine::reopenStream() {
    if (mStream) {
        stop();
        return start();
    } else {
        return oboe::Result::OK;
    }
}
