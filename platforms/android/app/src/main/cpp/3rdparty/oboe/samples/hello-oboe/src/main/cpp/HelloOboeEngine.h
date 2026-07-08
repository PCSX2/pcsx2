/*
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

#ifndef OBOE_HELLO_OBOE_ENGINE_H
#define OBOE_HELLO_OBOE_ENGINE_H

#include <oboe/Oboe.h>

#include "SoundGenerator.h"
#include "LatencyTuningCallback.h"
#include "IRestartable.h"
#include "DefaultErrorCallback.h"

constexpr int32_t kBufferSizeAutomatic = 0;

class HelloOboeEngine : public IRestartable {

public:
    HelloOboeEngine();

    virtual ~HelloOboeEngine() = default;

    void tap(bool isDown);

    /**
     * Open and start a stream.
     * @param deviceId the audio device id, can be obtained through an {@link AudioDeviceInfo} object
     * using Java/JNI.
     * @return error or OK
     */
    oboe::Result start(oboe::AudioApi audioApi, int deviceId, int channelCount);
    /* Start using current settings. */
    oboe::Result start();

    /**
     * Stop and close the stream.
     */
    oboe::Result stop();

    // From IRestartable
    void restart() override;

    void setBufferSizeInBursts(int32_t numBursts);

    /**
     * Calculate the current latency between writing a frame to the output stream and
     * the same frame being presented to the audio hardware.
     *
     * Here's how the calculation works:
     *
     * 1) Get the time a particular frame was presented to the audio hardware
     * @see AudioStream::getTimestamp
     * 2) From this extrapolate the time which the *next* audio frame written to the stream
     * will be presented
     * 3) Assume that the next audio frame is written at the current time
     * 4) currentLatency = nextFramePresentationTime - nextFrameWriteTime
     *
     * @return  Output Latency in Milliseconds
     */
    double getCurrentOutputLatencyMillis();

    bool isLatencyDetectionSupported();

    bool isAAudioRecommended();

private:
    oboe::Result reopenStream();
    oboe::Result openPlaybackStream();

    std::shared_ptr<oboe::AudioStream> mStream;
    std::shared_ptr<LatencyTuningCallback> mLatencyCallback;
    std::shared_ptr<DefaultErrorCallback> mErrorCallback;
    std::shared_ptr<SoundGenerator> mAudioSource;
    bool mIsLatencyDetectionSupported = false;

    int32_t        mDeviceId = oboe::Unspecified;
    int32_t        mChannelCount = oboe::Unspecified;
    oboe::AudioApi mAudioApi = oboe::AudioApi::Unspecified;
    std::mutex     mLock;
};

#endif //OBOE_HELLO_OBOE_ENGINE_H
