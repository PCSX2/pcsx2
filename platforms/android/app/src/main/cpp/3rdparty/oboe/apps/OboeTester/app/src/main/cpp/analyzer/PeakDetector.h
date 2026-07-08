/*
 * Copyright 2015 The Android Open Source Project
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

#ifndef ANALYZER_PEAK_DETECTOR_H
#define ANALYZER_PEAK_DETECTOR_H

#include <math.h>

/**
 * Measure a peak envelope by rising with the peaks,
 * and decaying exponentially after each peak.
 * The absolute value of the input signal is used.
 */
class PeakDetector {
public:

    void reset() {
        mLevel = 0.0;
    }

    double process(double input) {
        mLevel *= mDecay; // exponential decay
        input = fabs(input);
        // never fall below the input signal
        if (input > mLevel) {
            mLevel = input;
        }
        return mLevel;
    }

    double getLevel() const {
        return mLevel;
    }

    double getDecay() const {
        return mDecay;
    }

    /**
     * Multiply the level by this amount on every iteration.
     * This provides an exponential decay curve.
     * A value just under 1.0 is best, for example, 0.99;
     * @param decay scale level for each input
     */
    void setDecay(double decay) {
        mDecay = decay;
    }

private:
    static constexpr double kDefaultDecay = 0.99f;

    double mLevel = 0.0;
    double mDecay = kDefaultDecay;
};
#endif //ANALYZER_PEAK_DETECTOR_H
