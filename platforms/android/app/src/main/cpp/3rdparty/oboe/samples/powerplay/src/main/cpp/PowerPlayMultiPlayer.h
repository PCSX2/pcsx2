/*
 * Copyright 2025 The Android Open Source Project
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

#ifndef SAMPLES_POWERPLAYMULTIPLAYER_H
#define SAMPLES_POWERPLAYMULTIPLAYER_H

#include <player/SimpleMultiPlayer.h>

/**
 * A simple streaming player for multiple SampleBuffers.
 */
class PowerPlayMultiPlayer : public iolib::SimpleMultiPlayer {
public:
    PowerPlayMultiPlayer() = default;

    virtual ~PowerPlayMultiPlayer() = default;

    void setupAudioStream(int32_t channelCount, oboe::PerformanceMode performanceMode) override;

    bool openStream(oboe::PerformanceMode performanceMode) override;

    void triggerUp(int32_t index) override;

    void triggerDown(int32_t index, oboe::PerformanceMode performanceMode) override;

    void updatePerformanceMode(oboe::PerformanceMode performanceMode);

    static bool setMMapEnabled(bool enabled);

    static bool isMMapEnabled();

    static bool isMMapSupported();

    int32_t getCurrentlyPlayingIndex();

    int32_t setBufferSizeInFrames(int32_t bufferSizeInFrames);

    int32_t getBufferCapacityInFrames();

    bool isOffloaded();

    int64_t getPlaybackPositionMillis();

    void seekTo(int32_t positionMillis);

    int64_t getDurationMillis(int32_t index);

private:
    class MyPresentationCallback : public oboe::AudioStreamPresentationCallback {
    public:
        MyPresentationCallback(iolib::SimpleMultiPlayer *parent) : mParent(parent) {}

        virtual ~MyPresentationCallback() {
        }

        void onPresentationEnded(oboe::AudioStream *oboeStream) override;

    private:
        iolib::SimpleMultiPlayer *mParent;
    };

    std::shared_ptr<MyPresentationCallback> mPresentationCallback;

    oboe::PerformanceMode mLastPerformanceMode;

    bool mLastMMapEnabled;
};

#endif //SAMPLES_POWERPLAYMULTIPLAYER_H
