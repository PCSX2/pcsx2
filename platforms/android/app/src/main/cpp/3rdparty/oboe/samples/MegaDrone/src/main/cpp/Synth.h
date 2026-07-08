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

#ifndef MEGADRONE_SYNTH_H
#define MEGADRONE_SYNTH_H

#include <array>
#include <TappableAudioSource.h>

#include <Oscillator.h>
#include <Mixer.h>
#include <MonoToStereo.h>

constexpr int kNumOscillators = 100;
constexpr float kOscBaseFrequency = 116.0;
constexpr float kOscDivisor = 33;
constexpr float kOscAmplitude = 0.009;


class Synth : public TappableAudioSource {
public:

    Synth(int32_t sampleRate, int32_t channelCount) :
    TappableAudioSource(sampleRate, channelCount) {
        for (int i = 0; i < kNumOscillators; ++i) {
            mOscs[i].setSampleRate(mSampleRate);
            mOscs[i].setFrequency(kOscBaseFrequency + (static_cast<float>(i) / kOscDivisor));
            mOscs[i].setAmplitude(kOscAmplitude);
            mMixer.addTrack(&mOscs[i]);
        }
        if (mChannelCount == oboe::ChannelCount::Stereo) {
            mOutputStage =  &mConverter;
        } else {
            mOutputStage = &mMixer;
        }
    }

    void tap(bool isOn) override {
        for (auto &osc : mOscs) osc.setWaveOn(isOn);
    };

    // From IRenderableAudio
    void renderAudio(float *audioData, int32_t numFrames) override {
        mOutputStage->renderAudio(audioData, numFrames);
    };

    virtual ~Synth() {
    }
private:
    // Rendering objects
    std::array<Oscillator, kNumOscillators> mOscs;
    Mixer mMixer;
    MonoToStereo mConverter = MonoToStereo(&mMixer);
    IRenderableAudio *mOutputStage; // This will point to either the mixer or converter, so it needs to be raw
};


#endif //MEGADRONE_SYNTH_H
