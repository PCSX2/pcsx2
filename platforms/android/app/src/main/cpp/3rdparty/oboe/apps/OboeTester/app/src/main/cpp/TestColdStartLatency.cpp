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
#include "TestColdStartLatency.h"
#include "OboeTools.h"

using namespace oboe;

int32_t TestColdStartLatency::open(bool useInput, bool useLowLatency, bool useMmap, bool
        useExclusive) {

    mDataCallback = std::make_shared<MyDataCallback>();

    // Enable MMAP if needed
    bool wasMMapEnabled = AAudioExtensions::getInstance().isMMapEnabled();
    AAudioExtensions::getInstance().setMMapEnabled(useMmap);

    int64_t beginOpenNanos = AudioClock::getNanoseconds();

    AudioStreamBuilder builder;
    Result result = builder.setFormat(AudioFormat::Float)
            ->setPerformanceMode(useLowLatency ? PerformanceMode::LowLatency :
                    PerformanceMode::None)
            ->setDirection(useInput ? Direction::Input : Direction::Output)
            ->setChannelCount(kChannelCount)
            ->setDataCallback(mDataCallback)
            ->setSharingMode(useExclusive ? SharingMode::Exclusive : SharingMode::Shared)
            ->openStream(mStream);

    int64_t endOpenNanos = AudioClock::getNanoseconds();
    int64_t actualDurationNanos = endOpenNanos - beginOpenNanos;
    mOpenTimeMicros = actualDurationNanos / NANOS_PER_MICROSECOND;

    // Revert MMAP back to its previous state
    AAudioExtensions::getInstance().setMMapEnabled(wasMMapEnabled);

    if (result == Result::OK) {
        mDeviceId = mStream->getDeviceId();
    }

    return (int32_t) result;
}

int32_t TestColdStartLatency::start() {
    mBeginStartNanos = AudioClock::getNanoseconds();
    Result result = mStream->requestStart();
    int64_t endStartNanos = AudioClock::getNanoseconds();
    int64_t actualDurationNanos = endStartNanos - mBeginStartNanos;
    mStartTimeMicros = actualDurationNanos / NANOS_PER_MICROSECOND;
    return (int32_t) result;
}

int32_t TestColdStartLatency::close() {
    if (!mStream) {
        return (int32_t)Result::OK;
    }
    Result result1 = mStream->requestStop();
    Result result2 = mStream->close();
    return (int32_t)((result1 != Result::OK) ? result1 : result2);
}

int32_t TestColdStartLatency::getColdStartTimeMicros() {
    int64_t position;
    int64_t timestampNanos;
    if (mStream->getDirection() == Direction::Output) {
        auto result = mStream->getTimestamp(CLOCK_MONOTONIC);
        if (!result) {
            return -1; // ERROR
        }
        auto frameTimestamp = result.value();
        // Calculate the time that frame[0] would have been played by the speaker.
        position = frameTimestamp.position;
        timestampNanos = frameTimestamp.timestamp;
    } else {
        position = mStream->getFramesRead();
        timestampNanos = AudioClock::getNanoseconds();
    }
    double sampleRate = (double) mStream->getSampleRate();

    int64_t elapsedNanos = NANOS_PER_SECOND * (position / sampleRate);
    int64_t timeOfFrameZero = timestampNanos - elapsedNanos;
    int64_t coldStartLatencyNanos = timeOfFrameZero - mBeginStartNanos;
    return coldStartLatencyNanos / NANOS_PER_MICROSECOND;
}

void TestColdStartLatency::waitForValidTimestamp() {
    while (!mStream->getTimestamp(CLOCK_MONOTONIC)) {
        usleep(kPollPeriodMillis * 1000);
    }
}

// Callback that sleeps then touches the audio buffer.
DataCallbackResult TestColdStartLatency::MyDataCallback::onAudioReady(
        AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) {
    float *floatData = (float *) audioData;
    const int numSamples = numFrames * kChannelCount;
    if (audioStream->getDirection() == Direction::Output) {
        // Fill mono buffer with a sine wave.
        for (int i = 0; i < numSamples; i++) {
            *floatData++ = sinf(mPhase) * 0.2f;
            if ((i % kChannelCount) == (kChannelCount - 1)) {
                mPhase += kPhaseIncrement;
                // Wrap the phase around in a circle.
                if (mPhase >= M_PI) mPhase -= 2 * M_PI;
            }
        }
    }
    return DataCallbackResult::Continue;
}
