/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef SYNTHMARK_SYNTHESIZER_H
#define SYNTHMARK_SYNTHESIZER_H

#include <cstdint>
#include <math.h>
#include <memory>
#include <string.h>
#include <cassert>
#include "SynthTools.h"
#include "VoiceBase.h"
#include "SimpleVoice.h"

namespace marksynth {
#define SAMPLES_PER_FRAME   2

/**
 * Manage an array of voices.
 * Note that this is not a fully featured general purpose synthesizer.
 * It is designed simply to have a similar CPU load as a common synthesizer.
 */
class Synthesizer
{
public:
    Synthesizer()
    : mMaxVoices(0)
    , mActiveVoiceCount(0)
    , mVoices(NULL)
    {}

    virtual ~Synthesizer() {
        delete[] mVoices;
    };

    int32_t setup(int32_t sampleRate, int32_t maxVoices) {
        mMaxVoices = maxVoices;
        UnitGenerator::setSampleRate(sampleRate);
        mVoices = new SimpleVoice[mMaxVoices];
        return (mVoices == NULL) ? -1 : 0;
    }

    void allNotesOn() {
        notesOn(mMaxVoices);
    }

    int32_t notesOn(int32_t numVoices) {
        if (numVoices > mMaxVoices) {
            return -1;
        }
        mActiveVoiceCount = numVoices;
        // Leave some headroom so the resonant filter does not clip.
        mVoiceAmplitude = 0.5f / sqrt(mActiveVoiceCount);

        int pitchIndex = 0;
        synth_float_t pitches[] = {60.0, 64.0, 67.0, 69.0};
        for(int iv = 0; iv < mActiveVoiceCount; iv++ ) {
            SimpleVoice *voice = &mVoices[iv];
            // Randomize pitches by a few cents to smooth out the CPU load.
            float pitchOffset = 0.03f * (float) SynthTools::nextRandomDouble();
            synth_float_t pitch = pitches[pitchIndex++] + pitchOffset;
            if (pitchIndex > 3) pitchIndex = 0;
            voice->noteOn(pitch, 1.0);
        }
        return 0;
    }

    void allNotesOff() {
        for(int iv = 0; iv < mActiveVoiceCount; iv++ ) {
            SimpleVoice *voice = &mVoices[iv];
            voice->noteOff();
        }
    }

    void renderStereo(float *output, int32_t numFrames) {
        int32_t framesLeft = numFrames;
        float *renderBuffer = output;

        // Clear mixing buffer.
        memset(output, 0, numFrames * SAMPLES_PER_FRAME * sizeof(float));

        while (framesLeft > 0) {
            int framesThisTime = std::min(kSynthmarkFramesPerRender, framesLeft);
            for(int iv = 0; iv < mActiveVoiceCount; iv++ ) {
                SimpleVoice *voice = &mVoices[iv];
                voice->generate(framesThisTime);
                float *mix = renderBuffer;

                synth_float_t leftGain = mVoiceAmplitude;
                synth_float_t rightGain = mVoiceAmplitude;
                if (mActiveVoiceCount > 1) {
                    synth_float_t pan = iv / (mActiveVoiceCount - 1.0f);
                    leftGain *= pan;
                    rightGain *= 1.0 - pan;
                }
                for(int n = 0; n < kSynthmarkFramesPerRender; n++ ) {
                    synth_float_t sample = voice->output[n];
                    *mix++ += (float) (sample * leftGain);
                    *mix++ += (float) (sample * rightGain);
                }
            }
            framesLeft -= framesThisTime;
            mFrameCounter += framesThisTime;
            renderBuffer += framesThisTime * SAMPLES_PER_FRAME;
        }
        assert(framesLeft == 0);
    }

    int32_t getActiveVoiceCount() {
        return mActiveVoiceCount;
    }

private:
    int32_t mMaxVoices;
    int32_t mActiveVoiceCount;
    int64_t mFrameCounter;
    SimpleVoice *mVoices;
    synth_float_t mVoiceAmplitude = 1.0;
};
};
#endif // SYNTHMARK_SYNTHESIZER_H
