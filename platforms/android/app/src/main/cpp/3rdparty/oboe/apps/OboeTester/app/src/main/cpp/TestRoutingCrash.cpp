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
#include "TestRoutingCrash.h"

using namespace oboe;

// open start start an Oboe stream
int32_t TestRoutingCrash::start(bool useInput) {

    mDataCallback = std::make_shared<MyDataCallback>(this);

    // Disable MMAP because we are trying to crash a Legacy Stream.
    bool wasMMapEnabled = AAudioExtensions::getInstance().isMMapEnabled();
    AAudioExtensions::getInstance().setMMapEnabled(false);

    AudioStreamBuilder builder;
    oboe::Result result = builder.setFormat(oboe::AudioFormat::Float)
#if 1
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
#else
            ->setPerformanceMode(oboe::PerformanceMode::None)
#endif
            ->setDirection(useInput ? oboe::Direction::Input : oboe::Direction::Output)
            ->setChannelCount(kChannelCount)
            ->setDataCallback(mDataCallback)
            // Use VoiceCommunication so we can reroute it by setting SpeakerPhone ON/OFF.
            ->setUsage(oboe::Usage::VoiceCommunication)
            ->openStream(mStream);
    if (result != oboe::Result::OK) {
        return (int32_t) result;
    }

    AAudioExtensions::getInstance().setMMapEnabled(wasMMapEnabled);
    return (int32_t) mStream->requestStart();
}

int32_t TestRoutingCrash::stop() {
    oboe::Result result1 =  mStream->requestStop();
    oboe::Result result2 =   mStream->close();
    return (int32_t)((result1 != oboe::Result::OK) ? result1 : result2);
}

// Callback that sleeps then touches the audio buffer.
DataCallbackResult TestRoutingCrash::MyDataCallback::onAudioReady(
        AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) {
    float *floatData = (float *) audioData;

    // If I call getTimestamp() here it does NOT crash!

    // Simulate the timing of a heavy workload by sleeping.
    // Otherwise the window for the crash is very narrow.
    const double kDutyCycle = 0.7;
    const double bufferTimeNanos = 1.0e9 * numFrames / (double) audioStream->getSampleRate();
    const int64_t targetDurationNanos = (int64_t) (bufferTimeNanos * kDutyCycle);
    if (targetDurationNanos > 0) {
        AudioClock::sleepForNanos(targetDurationNanos);
    }
    const double kFilterCoefficient = 0.95; // low pass IIR filter
    const double sleepMicros = targetDurationNanos * 0.0001;
    mParent->averageSleepTimeMicros = ((1.0 - kFilterCoefficient) * sleepMicros)
            + (kFilterCoefficient * mParent->averageSleepTimeMicros);

    // If I call getTimestamp() here it crashes.
    audioStream->getTimestamp(CLOCK_MONOTONIC); // Trigger a restoreTrack_l() in framework.

    const int numSamples = numFrames * kChannelCount;
    if (audioStream->getDirection() == oboe::Direction::Input) {
        // Read buffer and write sum of samples to a member variable.
        // We just want to touch the memory and not get optimized away by the compiler.
        float sum = 0.0f;
        for (int i = 0; i < numSamples; i++) {
            sum += *floatData++;
        }
        mInputSum = sum;
    } else {
        // Fill mono buffer with a sine wave.
        // If the routing occurred then the buffer may be dead and
        // we may be writing into unallocated memory.
        for (int i = 0; i < numSamples; i++) {
            *floatData++ = sinf(mPhase) * 0.2f;
            mPhase += kPhaseIncrement;
            // Wrap the phase around in a circle.
            if (mPhase >= M_PI) mPhase -= 2 * M_PI;
        }
    }

    // If I call getTimestamp() here it does NOT crash!

    return oboe::DataCallbackResult::Continue;
}
