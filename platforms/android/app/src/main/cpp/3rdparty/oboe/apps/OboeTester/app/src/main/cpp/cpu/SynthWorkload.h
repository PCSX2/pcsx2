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

#ifndef SYNTH_WORKLOAD_H
#define SYNTH_WORKLOAD_H

#include "../synth/Synthesizer.h"

class SynthWorkload {
public:
    SynthWorkload() : SynthWorkload((int) (0.2 * marksynth::kSynthmarkSampleRate),
                                     (int) (0.3 * marksynth::kSynthmarkSampleRate)) {

    }

    SynthWorkload(int onFrames, int offFrames) {
        mSynth.setup(marksynth::kSynthmarkSampleRate, marksynth::kSynthmarkMaxVoices);
        mOnFrames = onFrames;
        mOffFrames = offFrames;
    }

    void onCallback(double workload) {
        // If workload changes then restart notes.
        if (workload != mPreviousWorkload) {
            mSynth.allNotesOff();
            mAreNotesOn = false;
            mCountdown = 0; // trigger notes on
            mPreviousWorkload = workload;
        }
        if (mCountdown <= 0) {
            if (mAreNotesOn) {
                mSynth.allNotesOff();
                mAreNotesOn = false;
                mCountdown = mOffFrames;
            } else {
                mSynth.notesOn((int)mPreviousWorkload);
                mAreNotesOn = true;
                mCountdown = mOnFrames;
            }
        }
    }

    /**
     * Render the notes into a stereo buffer.
     * Passing a nullptr will cause the calculated results to be discarded.
     * The workload should be the same.
     * @param buffer a real stereo buffer or nullptr
     * @param numFrames
     */
    void renderStereo(float *buffer, int numFrames) {
        if (buffer == nullptr) {
            int framesLeft = numFrames;
            while (framesLeft > 0) {
                int framesThisTime = std::min(kDummyBufferSizeInFrames, framesLeft);
                // Do the work then throw it away.
                mSynth.renderStereo(mDummyStereoBuffer, framesThisTime);
                framesLeft -= framesThisTime;
            }
        } else {
            mSynth.renderStereo(buffer, numFrames);
        }
        mCountdown -= numFrames;
    }

private:
    marksynth::Synthesizer   mSynth;
    static constexpr int     kDummyBufferSizeInFrames = 32;
    float                    mDummyStereoBuffer[kDummyBufferSizeInFrames * 2];
    double                   mPreviousWorkload = 1.0;
    bool                     mAreNotesOn = false;
    int                      mCountdown = 0;
    int                      mOnFrames = 0;
    int                      mOffFrames = 0;
};

#endif //SYNTH_WORKLOAD_H
