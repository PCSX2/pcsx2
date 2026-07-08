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

#ifndef ANALYZER_ROUNDED_MANCHESTER_ENCODER_H
#define ANALYZER_ROUNDED_MANCHESTER_ENCODER_H

#include <math.h>
#include <memory>
#include <stdlib.h>
#include "ManchesterEncoder.h"

/**
 * Encode bytes using Manchester Code.
 * Round the edges using a half cosine to reduce ringing caused by a hard edge.
 */

class RoundedManchesterEncoder : public ManchesterEncoder {
public:
    RoundedManchesterEncoder(int samplesPerPulse)
            : ManchesterEncoder(samplesPerPulse) {
        int rampSize = samplesPerPulse / 4;
        mZeroAfterZero = std::make_unique<float[]>(samplesPerPulse);
        mZeroAfterOne = std::make_unique<float[]>(samplesPerPulse);

        int sampleIndex = 0;
        for (int rampIndex = 0; rampIndex < rampSize; rampIndex++) {
            float phase = (rampIndex + 1) * M_PI / rampSize;
            float sample = -cosf(phase);
            mZeroAfterZero[sampleIndex] = sample;
            mZeroAfterOne[sampleIndex] = 1.0f;
            sampleIndex++;
        }
        for (int rampIndex = 0; rampIndex < rampSize; rampIndex++) {
            mZeroAfterZero[sampleIndex] = 1.0f;
            mZeroAfterOne[sampleIndex] = 1.0f;
            sampleIndex++;
        }
        for (int rampIndex = 0; rampIndex < rampSize; rampIndex++) {
            float phase = (rampIndex + 1) * M_PI / rampSize;
            float sample = cosf(phase);
            mZeroAfterZero[sampleIndex] = sample;
            mZeroAfterOne[sampleIndex] = sample;
            sampleIndex++;
        }
        for (int rampIndex = 0; rampIndex < rampSize; rampIndex++) {
            mZeroAfterZero[sampleIndex] = -1.0f;
            mZeroAfterOne[sampleIndex] = -1.0f;
            sampleIndex++;
        }
    }

    void onNextBit(bool current) override {
        // Do we need to use the rounded edge?
        mCurrentSamples = (current ^ mPreviousBit)
                          ? mZeroAfterOne.get()
                          : mZeroAfterZero.get();
        mPreviousBit = current;
    }

    float nextFloat() override {
        advanceSample();
        float output = mCurrentSamples[mCursor];
        if (getCurrentBit()) output = -output;
        return output;
    }

private:

    bool mPreviousBit = false;
    float *mCurrentSamples = nullptr;
    std::unique_ptr<float[]> mZeroAfterZero;
    std::unique_ptr<float[]> mZeroAfterOne;
};

#endif //ANALYZER_ROUNDED_MANCHESTER_ENCODER_H
