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
#ifndef SHARED_SYNTH_SOUND_H
#define SHARED_SYNTH_SOUND_H
#include <cstdint>
#include <atomic>
#include <math.h>
#include <memory>
#include "IRenderableAudio.h"
constexpr float kDefaultFrequency = 440.0;
constexpr int32_t kDefaultSampleRate = 48000;
constexpr float kPi = M_PI;
constexpr float kTwoPi = kPi * 2;
constexpr int32_t kNumSineWaves = 5;
constexpr float kSustainMultiplier = 0.99999;
constexpr float kReleaseMultiplier = 0.999;
// Stop playing music below this cutoff
constexpr float kMasterAmplitudeCutOff = 0.01;

class SynthSound : public IRenderableAudio {

public:
    SynthSound() {

    }

    ~SynthSound() {

    };

    void noteOn() {
        mTrigger = true; // start a note envelope
        mAmplitudeScaler = kSustainMultiplier;
    }

    void noteOff() {
        mAmplitudeScaler = kReleaseMultiplier;
    }

    void setSampleRate(int32_t sampleRate) {
        mSampleRate = sampleRate;
        updatePhaseIncrement();
    };
    void setFrequency(float frequency) {
        mFrequency = frequency;
        updatePhaseIncrement();
    };
    // Amplitudes from https://epubs.siam.org/doi/pdf/10.1137/S00361445003822
    inline void setAmplitude(float amplitude) {
        mAmplitudes[0] = amplitude * .2f;
        mAmplitudes[1] = amplitude;
        mAmplitudes[2] = amplitude * .1f;
        mAmplitudes[3] = amplitude * .02f;
        mAmplitudes[4] = amplitude * .15f;
    };
    // From IRenderableAudio
    void renderAudio(float *audioData, int32_t numFrames) override {
        for (int i = 0; i < numFrames; ++i) {
            if (mTrigger.exchange(false)) {
                mMasterAmplitude = 1.0;
                mPhase = 0.0f;
            } else {
                mMasterAmplitude *= mAmplitudeScaler;
            }

            audioData[i] = 0;
            if (mMasterAmplitude < kMasterAmplitudeCutOff) {
                continue;
            }
            for (int j = 0; j < kNumSineWaves; ++j) {
                audioData[i] += sinf(mPhase * (j + 1)) * mAmplitudes[j] * mMasterAmplitude;
            }
            mPhase += mPhaseIncrement;
            if (mPhase > kTwoPi) {
                mPhase -=  kTwoPi;
            }
        }
    };

private:
    std::atomic<bool> mTrigger { false };
    float mMasterAmplitude = 0.0f;
    std::atomic<float> mAmplitudeScaler { 0.0f };
    std::array<std::atomic<float>, kNumSineWaves> mAmplitudes;
    float mPhase = 0.0f;
    std::atomic<float> mPhaseIncrement { 0 };
    std::atomic<float> mFrequency { kDefaultFrequency };
    std::atomic<int32_t> mSampleRate { kDefaultSampleRate };
    void updatePhaseIncrement(){
        // Note how there is a division here. If this file is changed so that updatePhaseIncrement
        // is called more frequently, please cache 1/mSampleRate. This allows this operation to not
        // need divisions.
        mPhaseIncrement = kTwoPi * mFrequency / static_cast<float>(mSampleRate);
    };
};
#endif //SHARED_SYNTH_SOUND_H
