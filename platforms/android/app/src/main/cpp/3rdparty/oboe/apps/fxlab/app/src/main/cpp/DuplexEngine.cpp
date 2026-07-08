/*
 * Copyright  2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "DuplexEngine.h"
#include "effects/Effects.h"

DuplexEngine::DuplexEngine() {
    beginStreams();
}

void DuplexEngine::beginStreams() {
    // This ordering is extremely important
    openInStream();
    if (inStream->getFormat() == oboe::AudioFormat::Float) {
        functionList.emplace<FunctionList<float *>>();
        createCallback<float_t>();
    } else if (inStream->getFormat() == oboe::AudioFormat::I16) {
        createCallback<int16_t>();
    } else {
        stopStreams();
    }
    SAMPLE_RATE = inStream->getSampleRate();
    openOutStream();

    oboe::Result result = startStreams();
    if (result != oboe::Result::OK) stopStreams();
}

template<class numeric>
void DuplexEngine::createCallback() {
    mCallback = std::make_unique<DuplexCallback<numeric>>(
            *inStream, [&functionStack = this->functionList](numeric *beg, numeric *end) {
                std::get<FunctionList<numeric *>>(functionStack)(beg, end);
            },
            inStream->getBufferCapacityInFrames(),
            std::bind(&DuplexEngine::beginStreams, this));
}


oboe::AudioStreamBuilder DuplexEngine::defaultBuilder() {
    return *oboe::AudioStreamBuilder()
            .setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive);
}

void DuplexEngine::openInStream() {
    defaultBuilder().setDirection(oboe::Direction::Input)
            ->setFormat(oboe::AudioFormat::Float) // For now
            ->setChannelCount(1) // Mono in for effects processing
            ->openManagedStream(inStream);
}

void DuplexEngine::openOutStream() {
    defaultBuilder().setCallback(mCallback.get())
            ->setSampleRate(inStream->getSampleRate())
            ->setFormat(inStream->getFormat())
            ->setChannelCount(2) // Stereo out
            ->openManagedStream(outStream);
}

oboe::Result DuplexEngine::startStreams() {
    oboe::Result result = outStream->requestStart();
    int64_t timeoutNanos = 500 * 1000000; // arbitrary 1/2 second
    auto currentState = outStream->getState();
    auto nextState = oboe::StreamState::Unknown;
    while (result == oboe::Result::OK && currentState != oboe::StreamState::Started) {
        result = outStream->waitForStateChange(currentState, &nextState, timeoutNanos);
        currentState = nextState;
    }
    if (result != oboe::Result::OK) return result;
    return inStream->requestStart();
}

oboe::Result DuplexEngine::stopStreams() {
    oboe::Result outputResult = inStream->requestStop();
    oboe::Result inputResult = outStream->requestStop();
    if (outputResult != oboe::Result::OK) return outputResult;
    return inputResult;
}

