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
#include "SoundBoardEngine.h"

/**
 * Main audio engine for the SoundBoard sample. It is responsible for:
 *
 * - Creating the callback object which will be supplied when constructing the audio stream
 * - Creating the playback stream, including setting the callback object
 * - Creating `Synth` which will render the audio inside the callback
 * - Starting the playback stream
 * - Restarting the playback stream when `restart()` is called by the callback object
 *
 * @param numSignals
 */
SoundBoardEngine::SoundBoardEngine(int32_t numSignals) {
    createCallback(numSignals);
}

SoundBoardEngine::~SoundBoardEngine() {
    if (mStream) {
        LOGE("SoundBoardEngine destructor was called without calling stop()."
             "Please call stop() to ensure stream resources are not leaked.");
        stop();
    }
}

void SoundBoardEngine::noteOff(int32_t noteIndex) {
    mSynth->noteOff(noteIndex);
}

void SoundBoardEngine::noteOn(int32_t noteIndex) {
    mSynth->noteOn(noteIndex);
}

void SoundBoardEngine::tap(bool isDown) {
    mSynth->tap(isDown);
}

void SoundBoardEngine::restart() {
    stop();
    start();
}
// Create the playback stream
oboe::Result SoundBoardEngine::createPlaybackStream() {
    oboe::AudioStreamBuilder builder;
    return builder.setSharingMode(oboe::SharingMode::Exclusive)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setFormat(oboe::AudioFormat::Float)
            ->setDataCallback(mDataCallback)
            ->setErrorCallback(mErrorCallback)
            ->openStream(mStream);
}

// Create the callback and set its thread affinity to the supplied CPU core IDs
void SoundBoardEngine::createCallback(int32_t numSignals){

    mDataCallback = std::make_shared<DefaultDataCallback>();

    // Create the error callback, we supply ourselves as the parent so that we can restart the stream
    // when it's disconnected
    mErrorCallback = std::make_shared<DefaultErrorCallback>(*this);

    mNumSignals = numSignals;
}

bool SoundBoardEngine::start() {
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

bool SoundBoardEngine::attemptStart() {
    auto result = createPlaybackStream();

    if (result == Result::OK) {
        // Create our synthesizer audio source using the properties of the stream
        mSynth = Synth::create(mStream->getSampleRate(), mStream->getChannelCount(), mNumSignals);
        mDataCallback->reset();
        mDataCallback->setSource(std::dynamic_pointer_cast<IRenderableAudio>(mSynth));
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

bool SoundBoardEngine::stop() {
    if(mStream && mStream->getState() != oboe::StreamState::Closed) {
        mStream->stop();
        mStream->close();
    }
    mStream.reset();
    return true;
}

