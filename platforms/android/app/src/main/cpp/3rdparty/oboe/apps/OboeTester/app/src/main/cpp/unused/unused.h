/*
 * Copyright 2017 The Android Open Source Project
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


#ifndef OBOETESTER_UNUSED_H
#define OBOETESTER_UNUSED_H

// Store this code for later use.
#if 0

/*

FIR filter designed with
http://t-filter.appspot.com

sampling frequency: 48000 Hz

* 0 Hz - 8000 Hz
  gain = 1.2
  desired ripple = 5 dB
  actual ripple = 5.595266169703693 dB

* 12000 Hz - 20000 Hz
  gain = 0
  desired attenuation = -40 dB
  actual attenuation = -37.58691566571914 dB

*/

#define FILTER_TAP_NUM 11

static const float sFilterTaps8000[FILTER_TAP_NUM] = {
        -0.05944219353343189f,
        -0.07303434839503208f,
        -0.037690487672689066f,
         0.1870480506596512f,
         0.3910337357836833f,
         0.5333672385425637f,
         0.3910337357836833f,
         0.1870480506596512f,
        -0.037690487672689066f,
        -0.07303434839503208f,
        -0.05944219353343189f
};

class LowPassFilter {
public:

    /*
     * Filter one input sample.
     * @return filtered output
     */
    float filter(float input) {
        float output = 0.0f;
        mX[mCursor] = input;
        // Index backwards over x.
        int xIndex = mCursor + FILTER_TAP_NUM;
        // Write twice so we avoid having to wrap in the middle of the convolution.
        mX[xIndex] = input;
        for (int i = 0; i < FILTER_TAP_NUM; i++) {
            output += sFilterTaps8000[i] * mX[xIndex--];
        }
        if (++mCursor >= FILTER_TAP_NUM) {
            mCursor = 0;
        }
        return output;
    }

    /**
     * @return true if PASSED
     */
    bool test() {
        // Measure the impulse of the filter at different phases so we exercise
        // all the wraparound cases in the FIR.
        for (int offset = 0; offset < (FILTER_TAP_NUM * 2); offset++ ) {
            // LOGD("LowPassFilter: cursor = %d\n", mCursor);
            // Offset by one each time.
            if (filter(0.0f) != 0.0f) {
                LOGD("ERROR: filter should return 0.0 before impulse response\n");
                return false;
            }
            for (int i = 0; i < FILTER_TAP_NUM; i++) {
                float output = filter((i == 0) ? 1.0f : 0.0f); // impulse
                if (output != sFilterTaps8000[i]) {
                    LOGD("ERROR: filter should return impulse response\n");
                    return false;
                }
            }
            for (int i = 0; i < FILTER_TAP_NUM; i++) {
                if (filter(0.0f) != 0.0f) {
                    LOGD("ERROR: filter should return 0.0 after impulse response\n");
                    return false;
                }
            }
        }
        return true;
    }

private:
    float   mX[FILTER_TAP_NUM * 2]{}; // twice as big as needed to avoid wrapping
    int32_t mCursor = 0;
};

/**
 * Low pass filter the recording using a simple FIR filter.
 * Note that the lowpass filter cutoff tracks the sample rate.
 * That is OK because the impulse width is a fixed number of samples.
 */
void lowPassFilter() {
    for (int i = 0; i < mFrameCounter; i++) {
        mData[i] = mLowPassFilter.filter(mData[i]);
    }
}

/**
 * Remove DC offset using a one-pole one-zero IIR filter.
 */
void dcBlocker() {
    const float R = 0.996; // narrow notch at zero Hz
    float x1 = 0.0;
    float y1 = 0.0;
    for (int i = 0; i < mFrameCounter; i++) {
        const float x = mData[i];
        const float y = x - x1 + (R * y1);
        mData[i] = y;
        y1 = y;
        x1 = x;
    }
}
#endif

#endif //OBOETESTER_UNUSED_H
