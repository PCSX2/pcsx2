/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef _PLAYER_SAMPLESOURCE_
#define _PLAYER_SAMPLESOURCE_

#include <cstdint>

#include "DataSource.h"

#include "SampleBuffer.h"

namespace iolib {

/**
 * Defines an interface for audio data provided to a player object.
 * Concrete examples include OneShotSampleBuffer. One could imagine a LoopingSampleBuffer.
 * Supports stereo position via mPan member.
 */
class SampleSource: public DataSource {
public:
    // Pan position of the audio in a stereo mix
    // [left:-1.0f] <- [center: 0.0f] -> -[right: 1.0f]
    static constexpr float PAN_HARDLEFT = -1.0f;
    static constexpr float PAN_HARDRIGHT = 1.0f;
    static constexpr float PAN_CENTER = 0.0f;

    SampleSource(SampleBuffer *sampleBuffer, float pan)
     : mSampleBuffer(sampleBuffer), mCurSampleIndex(0), mIsPlaying(false), mIsLoopMode(false), mGain(1.0f) {
        setPan(pan);
    }
    virtual ~SampleSource() {}

    void setPlayMode(bool fromPause = true) {
        mIsPlaying = true;
        if (fromPause) {
            mCurSampleIndex = 0;
        }
    }
    void setStopMode(bool isPause = false) {
        mIsPlaying = false;
        if (!isPause) {
            mCurSampleIndex = 0;
        }
    }

    void setLoopMode(bool isLoopMode) { mIsLoopMode = isLoopMode; }

    bool isPlaying() { return mIsPlaying; }

    int32_t getPlayHeadPosition() const { return mCurSampleIndex; }

    void setPlayHeadPosition(int32_t position) {
        if (mSampleBuffer != nullptr && position >= 0 && position < mSampleBuffer->getNumSamples()) {
            mCurSampleIndex = position;
        }
    }

    SampleBuffer* getSampleBuffer() { return mSampleBuffer; }

    void setPan(float pan) {
        if (pan < PAN_HARDLEFT) {
            mPan = PAN_HARDLEFT;
        } else if (pan > PAN_HARDRIGHT) {
            mPan = PAN_HARDRIGHT;
        } else {
            mPan = pan;
        }
        calcGainFactors();
    }

    float getPan() {
        return mPan;
    }

    void setGain(float gain) {
        mGain = gain;
        calcGainFactors();
    }

    float getGain() {
        return mGain;
    }

protected:
    SampleBuffer    *mSampleBuffer;

    int32_t mCurSampleIndex;

    bool mIsPlaying;
    std::atomic<bool> mIsLoopMode;

    // Logical pan value
    float mPan;

    // precomputed channel gains for pan
    float mLeftGain;
    float mRightGain;

    // Overall gain
    float mGain;

private:
    void calcGainFactors() {
        // useful panning information: http://www.cs.cmu.edu/~music/icm-online/readings/panlaws/
        float rightPan = (mPan * 0.5) + 0.5;
        mRightGain = rightPan * mGain;
        mLeftGain = (1.0 - rightPan) * mGain;    }
};

} // namespace wavlib

#endif //_PLAYER_SAMPLESOURCE_
