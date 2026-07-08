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

#ifndef ANALYZER_BASE_SINE_ANALYZER_H
#define ANALYZER_BASE_SINE_ANALYZER_H

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>

#include "InfiniteRecording.h"
#include "LatencyAnalyzer.h"

/**
 * Output a steady sine wave and analyze the return signal.
 *
 * Use a cosine transform to measure the predicted magnitude and relative phase of the
 * looped back sine wave. Then generate a predicted signal and compare with the actual signal.
 */
class BaseSineAnalyzer : public LoopbackProcessor {
public:

    enum SignalType {
        Sine,
        Chirp,
        MultiTone
    };

    BaseSineAnalyzer()
            : LoopbackProcessor()
            , mInfiniteRecording(64 * 1024) {}

    virtual bool isOutputEnabled() { return true; }

    void setSignalType(int signalType) {
        mSignalType = static_cast<SignalType>(signalType);
        if (mSignalType < SignalType::Sine || mSignalType > SignalType::MultiTone) {
            ALOGD("%s(), invalid signal type %d\n", __func__, mSignalType);
        }
    }

    void setMagnitude(double magnitude) {
        mMagnitude = magnitude;
        mScaledTolerance = mMagnitude * getTolerance();
    }

    /**
     *
     * @return valid phase or kPhaseInvalid=-999
     */
    double getPhaseOffset() {
        ALOGD("%s(), mPhaseOffset = %f\n", __func__, mPhaseOffset);
        return mPhaseOffset;
    }

    double getMagnitude() const {
        return mMagnitude;
    }

    /**
     * Set the amplitude of some white noise that can be added to the output signal.
     * This can be used to try to defeat feedback suppression that may block pure sine waves.
     * The default is zero because it can add phaseJitter and result in flaky tests.
     * @param noiseAmplitude amplitude of white noise, default is 0.0
     */
    void setNoiseAmplitude(double noiseAmplitude) {
        mNoiseAmplitude = noiseAmplitude;
    }

    double getNoiseAmplitude() const {
        return mNoiseAmplitude;
    }

    double getTolerance() {
        return mTolerance;
    }

    void setTolerance(double tolerance) {
        mTolerance = tolerance;
    }

    // advance and wrap phase
    void incrementInputPhase() {
        mInputPhase += mPhaseIncrement;
        if (mInputPhase > M_PI) {
            mInputPhase -= (2.0 * M_PI);
        }
    }

    // advance and wrap phase
    void incrementOutputPhase() {
        mOutputPhase += mPhaseIncrement;
        if (mOutputPhase > M_PI) {
            mOutputPhase -= (2.0 * M_PI);
        }
    }

    void incrementMultiTonePhases() {
        for (size_t i = 0; i < mMultiTonePhases.size(); i++) {
            mMultiTonePhases[i] += mMultiTonePhaseIncrements[i];
            if (mMultiTonePhases[i] > M_PI) {
                mMultiTonePhases[i] -= (2.0 * M_PI);
            }
        }
    }


    /**
     * @param frameData upon return, contains the reference sine wave
     * @param channelCount
     */
    result_code processOutputFrame(float *frameData, int channelCount) override {
        float output = 0.0f;
        // Output sine wave so we can measure it.
        if (isOutputEnabled()) {
            switch (mSignalType) {
                case Chirp: {
                    if (mFrameCounter < getSampleRate() * kChirpDurationSeconds) {
                        double sinOut = sin(mOutputPhase);
                        // Simple linear chirp from kChirpStartFrequency to mChirpEndFrequencyActual
                        // in kChirpDurationSeconds seconds.
                        double freq = kChirpStartFrequency
                                      + (mChirpEndFrequencyActual - kChirpStartFrequency)
                                        * mFrameCounter
                                        / (getSampleRate() * kChirpDurationSeconds);
                        mPhaseIncrement = 2.0 * M_PI * freq / getSampleRate();
                        incrementOutputPhase();
                        output = sinOut * mOutputAmplitude;
                        mFrameCounter++;
                    } else {
                        output = 0.0f;
                    }
                    break;
                }
                case MultiTone: {
                    double sum = 0.0;
                    for (double phase : mMultiTonePhases) {
                        sum += sin(phase);
                    }
                    incrementMultiTonePhases();
                    output = (sum / mMultiTonePhases.size()) * mOutputAmplitude;
                    break;
                }
                case Sine:
                default: {
                    double sinOut = sin(mOutputPhase);
                    incrementOutputPhase();
                    output = (sinOut * mOutputAmplitude)
                            + (mWhiteNoise.nextRandomDouble() * getNoiseAmplitude());
                    // ALOGD("sin(%f) = %f, %f\n", mOutputPhase, sinOut,  kPhaseIncrement);
                    break;
                }
            }
        }
        for (int i = 0; i < channelCount; i++) {
            frameData[i] = (i == getOutputChannel()) ? output : 0.0f;
        }
        return RESULT_OK;
    }

    /**
     * Calculate the magnitude of the component of the input signal
     * that matches the analysis frequency.
     * Also calculate the phase that we can use to create a
     * signal that matches that component.
     * The phase will be between -PI and +PI.
     */
    double calculateMagnitudePhase(double *phasePtr = nullptr) {
        if (mFramesAccumulated == 0) {
            return 0.0;
        }
        double sinMean = mSinAccumulator / mFramesAccumulated;
        double cosMean = mCosAccumulator / mFramesAccumulated;
        double magnitude = 2.0 * sqrt((sinMean * sinMean) + (cosMean * cosMean));
        if (phasePtr != nullptr) {
            double phase;
            if (magnitude < kMinValidMagnitude) {
                phase = kPhaseInvalid;
                ALOGD("%s() mag very low! sinMean = %7.5f, cosMean = %7.5f",
                      __func__, sinMean, cosMean);
            } else {
                phase = atan2(cosMean, sinMean);
                if (phase == 0.0) {
                    ALOGD("%s() phase zero! sinMean = %7.5f, cosMean = %7.5f",
                          __func__, sinMean, cosMean);
                }
            }
            *phasePtr = phase;
        }
        return magnitude;
    }

    /**
     * Perform sin/cos analysis on each sample.
     * Measure magnitude and phase on every period.
     * Updates mPhaseOffset
     * @param sample
     * @param referencePhase
     * @return true if magnitude and phase updated
     */
    bool transformSample(float sample) {
        // Compare incoming signal with the reference input sine wave.
        mSinAccumulator += static_cast<double>(sample) * sin(mInputPhase);
        mCosAccumulator += static_cast<double>(sample) * cos(mInputPhase);
        incrementInputPhase();

        mFramesAccumulated++;
        // Must be a multiple of the period or the calculation will not be accurate.
        if (mFramesAccumulated == mSinePeriod) {
            const double coefficient = 0.1;
            double magnitude = calculateMagnitudePhase(&mPhaseOffset);

            ALOGD("%s(), magnitude = %f, phaseOffset = %f\n", __func__,
                  magnitude, mPhaseOffset);
            if (mPhaseOffset != kPhaseInvalid) {
                // One pole averaging filter for magnitude.
                setMagnitude((mMagnitude * (1.0 - coefficient)) + (magnitude * coefficient));
            }
            resetAccumulator();
            return true;
        } else {
            return false;
        }
    }

    // reset the sine wave detector
    virtual void resetAccumulator() {
        mFramesAccumulated = 0;
        mSinAccumulator = 0.0;
        mCosAccumulator = 0.0;
    }

    void reset() override {
        LoopbackProcessor::reset();
        resetAccumulator();
        mMagnitude = 0.0;
        mFrameCounter = 0;
    }

    void prepareToTest() override {
        LoopbackProcessor::prepareToTest();
        mSinePeriod = getSampleRate() / kTargetGlitchFrequency;
        mInputPhase = 0.0f;
        mOutputPhase = 0.0f;
        mInverseSinePeriod = 1.0 / mSinePeriod;
        mPhaseIncrement = 2.0 * M_PI * mInverseSinePeriod;

        mMultiTonePhases.clear();
        mMultiTonePhaseIncrements.clear();
        for (double freq : sMultiToneFrequencies) {
            mMultiTonePhases.push_back(0.0);
            mMultiTonePhaseIncrements.push_back(2.0 * M_PI * freq / getSampleRate());
        }

        // Adjust chirp frequency to be no higher than Nyquist.
        mChirpEndFrequencyActual = std::min((double)kChirpEndFrequency, getSampleRate() / 2.0);
    }

protected:
    // Use a frequency that will not align with the common burst sizes.
    // If it aligns then buffer reordering bugs could be masked.
    static constexpr int32_t kTargetGlitchFrequency = 857; // Match CTS Verifier

    // Chirp constants
    static constexpr double kChirpStartFrequency = 20.0;
    static constexpr double kChirpEndFrequency = 15000.0;
    static constexpr double kChirpDurationSeconds = 4.0;

    // Multi-tone constants
    static constexpr double sMultiToneFrequencies[] = {401.0, 601.0, 1009.0, 1409.0, 2203.0};

    SignalType mSignalType = Sine;
    int32_t mFrameCounter = 0;

    int32_t mSinePeriod = 1; // this will be set before use
    double  mInverseSinePeriod = 1.0;
    double  mPhaseIncrement = 0.0;
    // Use two sine wave phases, input and output.
    // This is because the number of input and output samples may differ
    // in a callback and the output frame count may advance ahead of the input, or visa versa.
    double  mInputPhase = 0.0;
    double  mOutputPhase = 0.0;

    // For chirp
    double mChirpEndFrequencyActual = kChirpEndFrequency; // Nyquist adjusted

    // For multi-tone
    std::vector<double> mMultiTonePhases;
    std::vector<double> mMultiTonePhaseIncrements;


    double  mOutputAmplitude = 0.90;
    // This is the phase offset between the mInputPhase sine wave and the recorded
    // signal at the tuned frequency.
    // If this jumps around then we are probably just hearing noise.
    // Noise can cause the magnitude to be high but mPhaseOffset will be pretty random.
    // If we are tracking a sine wave then mPhaseOffset should be consistent.
    double  mPhaseOffset = 0.0;
    // kPhaseInvalid indicates that the phase measurement cannot be used.
    // We were seeing times when a magnitude of zero was causing atan2(s,c) to
    // return a phase of zero, which looked valid to Java. This is a way of passing
    // an error code back to Java as a single value to avoid race conditions.
    static constexpr double kPhaseInvalid = -999.0;
    double  mMagnitude = 0.0;
    static constexpr double kMinValidMagnitude = 2.0 / (1 << 16);
    int32_t mFramesAccumulated = 0;
    double  mSinAccumulator = 0.0;
    double  mCosAccumulator = 0.0;
    double  mScaledTolerance = 0.0;

    InfiniteRecording<float> mInfiniteRecording;

private:
    float   mTolerance = 0.10; // scaled from 0.0 to 1.0

    // Default to zero because some phones have a high pass filter that make it too loud.
    float mNoiseAmplitude = 0.0f;
    PseudoRandom  mWhiteNoise;
};

#endif //ANALYZER_BASE_SINE_ANALYZER_H
