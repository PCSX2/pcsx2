/*
 * Copyright 2018 The Android Open Source Project
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


#include <memory>
#include "MegaDroneEngine.h"

/**
 * Main audio engine for the MegaDrone sample. It is responsible for:
 *
 * - Creating the callback object which will be supplied when constructing the audio stream
 * - Setting the CPU core IDs to which the callback thread should bind to
 * - Creating the playback stream, including setting the callback object
 * - Creating `Synth` which will render the audio inside the callback
 * - Starting the playback stream
 * - Restarting the playback stream when `restart()` is called by the callback object
 *
 * @param cpuIds
 */
MegaDroneEngine::MegaDroneEngine(std::vector<int> cpuIds) {
    createCallback(cpuIds);
}

MegaDroneEngine::~MegaDroneEngine() {
    if (mStream) {
        LOGE("MegaDroneEngine destructor was called without calling stop()."
             "Please call stop() to ensure stream resources are not leaked.");
        stop();
    }
}

void MegaDroneEngine::tap(bool isDown) {
    mAudioSource->tap(isDown);
}

void MegaDroneEngine::restart() {
    stop();
    start();
}
// Create the playback stream
oboe::Result MegaDroneEngine::createPlaybackStream() {
    oboe::AudioStreamBuilder builder;
    return builder.setSharingMode(oboe::SharingMode::Exclusive)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setFormat(oboe::AudioFormat::Float)
            ->setDataCallback(mDataCallback)
            ->setErrorCallback(mErrorCallback)
            ->openStream(mStream);
}

// Create the callback and set its thread affinity to the supplied CPU core IDs
void MegaDroneEngine::createCallback(std::vector<int> cpuIds){

    mDataCallback = std::make_shared<DefaultDataCallback>();

    // Create the error callback, we supply ourselves as the parent so that we can restart the stream
    // when it's disconnected
    mErrorCallback = std::make_shared<DefaultErrorCallback>(*this);

    // Bind the audio callback to specific CPU cores as this can help avoid underruns caused by
    // core migrations
    mDataCallback->setCpuIds(cpuIds);
    mDataCallback->setThreadAffinityEnabled(true);
}

bool MegaDroneEngine::start() {
    // It is possible for a stream's device to become disconnected during stream open or between
    // stream open and stream start.
    // If the stream fails to start, close the old stream and try again.
    bool didStart = false;
    int tryCount = 0;
    do {
        if (tryCount > 0) {
            usleep(20 * 1000); // Sleep between tries to give the system time to settle.
        }
        didStart = attemptStart();
    } while (!didStart && tryCount++ < 3);
    if (!didStart) {
        LOGE("Failed at starting the stream");
    }
    return didStart;
}

bool MegaDroneEngine::attemptStart() {
    auto result = createPlaybackStream();

    if (result == Result::OK) {
        // Create our synthesizer audio source using the properties of the stream
        mAudioSource = std::make_shared<Synth>(mStream->getSampleRate(), mStream->getChannelCount());
        mDataCallback->reset();
        mDataCallback->setSource(std::dynamic_pointer_cast<IRenderableAudio>(mAudioSource));
        result = mStream->start();
        if (result == Result::OK) {
            return true;
        } else {
            LOGW("Failed attempt at starting the playback stream. Error: %s", convertToText(result));
            return false;
        }
    } else {
        LOGW("Failed attempt at creating the playback stream. Error: %s", convertToText(result));
        return false;
    }
}

bool MegaDroneEngine::stop() {
    if(mStream && mStream->getState() != oboe::StreamState::Closed) {
        mStream->stop();
        mStream->close();
    }
    mStream.reset();
    return true;
}

