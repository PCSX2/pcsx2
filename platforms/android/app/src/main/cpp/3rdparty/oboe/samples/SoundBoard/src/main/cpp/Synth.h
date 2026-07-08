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

#ifndef SOUNDBOARD_SYNTH_H
#define SOUNDBOARD_SYNTH_H

#include <array>
#include <TappableAudioSource.h>

#include <SynthSound.h>
#include <Mixer.h>
#include <MonoToStereo.h>

constexpr float kOscBaseFrequency = 196.00; // Start at G3
constexpr float kOscFrequencyMultiplier = 1.05946309436;
constexpr float kOscBaseAmplitude = 0.20;
constexpr float kOscAmplitudeMultiplier = 0.96;

class Synth : public IRenderableAudio, public ITappable {
public:
    static ::std::shared_ptr<Synth> create(const int32_t sampleRate, const int32_t channelCount, const int32_t numSignals) {
        return ::std::make_shared<Synth>(sampleRate, channelCount, numSignals);
    }

    Synth(const int32_t sampleRate, const int32_t channelCount, const int32_t numSignals) {
        mNumSignals = numSignals;
        mOscs = std::make_unique<SynthSound[]>(numSignals);
        float curFrequency = kOscBaseFrequency;
        float curAmplitude = kOscBaseAmplitude;
        for (int i = 0; i < numSignals; ++i) {
            mOscs[i].setSampleRate(sampleRate);
            mOscs[i].setFrequency(curFrequency);
            curFrequency *= kOscFrequencyMultiplier;
            mOscs[i].setAmplitude(curAmplitude);
            curAmplitude *= kOscAmplitudeMultiplier;
            mMixer.addTrack(&mOscs[i]);
        }

        if (channelCount == oboe::ChannelCount::Stereo) {
            mOutputStage =  &mConverter;
        } else {
            mOutputStage = &mMixer;
        }
    }

    void noteOff(int32_t noteIndex) {
        mOscs[noteIndex].noteOff();
    }

    void noteOn(int32_t noteIndex) {
        mOscs[noteIndex].noteOn();
    }

    void tap(bool isOn) override {
        for (int i = 0; i < mNumSignals; i++) {
            if (isOn) {
                mOscs[i].noteOn();
            } else {
                mOscs[i].noteOff();
            }
        }
    };

    // From IRenderableAudio
    void renderAudio(float *audioData, int32_t numFrames) override {
        mOutputStage->renderAudio(audioData, numFrames);
    };

    virtual ~Synth() {
    }
private:
    // Rendering objects
    int32_t mNumSignals;
    std::unique_ptr<SynthSound[]> mOscs;
    Mixer mMixer;
    MonoToStereo mConverter = MonoToStereo(&mMixer);
    IRenderableAudio *mOutputStage; // This will point to either the mixer or converter, so it needs to be raw
};


#endif //SOUNDBOARD_SYNTH_H
