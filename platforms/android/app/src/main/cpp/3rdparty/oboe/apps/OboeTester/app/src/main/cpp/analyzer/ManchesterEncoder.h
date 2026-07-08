/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANALYZER_MANCHESTER_ENCODER_H
#define ANALYZER_MANCHESTER_ENCODER_H

#include <cstdint>

/**
 * Encode bytes using Manchester Coding scheme.
 *
 * Manchester Code is self clocking.
 * There is a transition in the middle of every bit.
 * Zero is high then low.
 * One is low then high.
 *
 * This avoids having long DC sections that would droop when
 * passed though analog circuits with AC coupling.
 *
 * IEEE 802.3 compatible.
 */

class ManchesterEncoder {
public:
    ManchesterEncoder(int samplesPerPulse)
            : mSamplesPerPulse(samplesPerPulse)
            , mSamplesPerPulseHalf(samplesPerPulse / 2)
            , mCursor(samplesPerPulse) {
    }

    virtual ~ManchesterEncoder() = default;

    /**
     * This will be called when the next byte is needed.
     * @return next byte
     */
    virtual uint8_t onNextByte() = 0;

    /**
     * Generate the next floating point sample.
     * @return next float
     */
    virtual float nextFloat() {
        advanceSample();
        if (mCurrentBit) {
            return (mCursor < mSamplesPerPulseHalf) ? -1.0f : 1.0f; // one
        } else {
            return (mCursor < mSamplesPerPulseHalf) ? 1.0f : -1.0f; // zero
        }
    }

protected:
    /**
     * This will be called when a new bit is ready to be encoded.
     * It can be used to prepare the encoded samples.
     */
    virtual void onNextBit(bool /* current */) {};

    void advanceSample() {
        // Are we ready for a new bit?
        if (++mCursor >= mSamplesPerPulse) {
            mCursor = 0;
            if (mBitsLeft == 0) {
                mCurrentByte = onNextByte();
                mBitsLeft = 8;
            }
            --mBitsLeft;
            mCurrentBit = (mCurrentByte >> mBitsLeft) & 1;
            onNextBit(mCurrentBit);
        }
    }

    bool getCurrentBit() {
        return mCurrentBit;
    }

    const int mSamplesPerPulse;
    const int mSamplesPerPulseHalf;
    int       mCursor;
    int       mBitsLeft = 0;
    uint8_t   mCurrentByte = 0;
    bool      mCurrentBit = false;
};
#endif //ANALYZER_MANCHESTER_ENCODER_H
