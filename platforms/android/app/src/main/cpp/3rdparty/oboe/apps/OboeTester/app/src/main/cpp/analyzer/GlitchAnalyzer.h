/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ANALYZER_GLITCH_ANALYZER_H
#define ANALYZER_GLITCH_ANALYZER_H

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>

#include "InfiniteRecording.h"
#include "LatencyAnalyzer.h"
#include "BaseSineAnalyzer.h"
#include "PseudoRandom.h"

/**
 * Output a steady sine wave and analyze the return signal.
 *
 * Use a cosine transform to measure the predicted magnitude and relative phase of the
 * looped back sine wave. Then generate a predicted signal and compare with the actual signal.
 */
class GlitchAnalyzer : public BaseSineAnalyzer {
public:

    GlitchAnalyzer() : BaseSineAnalyzer() {}

    int32_t getState() const {
        return mState;
    }

    double getPeakAmplitude() const {
        return mPeakFollower.getLevel();
    }

    double getSineAmplitude() const {
        return mMagnitude;
    }

    int getSinePeriod() const {
        return mSinePeriod;
    }

    int32_t getGlitchCount() const {
        return mGlitchCount;
    }

    int32_t getGlitchLength() const {
        return mGlitchLength;
    }

    int32_t getStateFrameCount(int state) const {
        return mStateFrameCounters[state];
    }

    double getSignalToNoiseDB() {
        static const double threshold = 1.0e-14;
        if (mState != STATE_LOCKED
                || mMeanSquareSignal < threshold
                || mMeanSquareNoise < threshold) {
            return -999.0; // error indicator
        } else {
            double signalToNoise = mMeanSquareSignal / mMeanSquareNoise; // power ratio
            double signalToNoiseDB = 10.0 * log(signalToNoise);
            if (signalToNoiseDB < static_cast<float>(MIN_SNR_DB)) {
                setResult(ERROR_VOLUME_TOO_LOW);
            }
            return signalToNoiseDB;
        }
    }

    std::string analyze() override {
        std::stringstream report;
        report << "GlitchAnalyzer ------------------\n";
        report << LOOPBACK_RESULT_TAG "peak.amplitude     = " << std::setw(8)
               << getPeakAmplitude() << "\n";
        report << LOOPBACK_RESULT_TAG "sine.magnitude     = " << std::setw(8)
               << getSineAmplitude() << "\n";
        report << LOOPBACK_RESULT_TAG "rms.noise          = " << std::setw(8)
               << mMeanSquareNoise << "\n";
        report << LOOPBACK_RESULT_TAG "signal.to.noise.db = " << std::setw(8)
               << getSignalToNoiseDB() << "\n";
        report << LOOPBACK_RESULT_TAG "frames.accumulated = " << std::setw(8)
               << mFramesAccumulated << "\n";
        report << LOOPBACK_RESULT_TAG "sine.period        = " << std::setw(8)
               << mSinePeriod << "\n";
        report << LOOPBACK_RESULT_TAG "test.state         = " << std::setw(8)
               << mState << "\n";
        report << LOOPBACK_RESULT_TAG "frame.count        = " << std::setw(8)
               << mFrameCounter << "\n";
        // Did we ever get a lock?
        bool gotLock = (mState == STATE_LOCKED) || (mGlitchCount > 0);
        if (!gotLock) {
            report << "ERROR - failed to lock on reference sine tone.\n";
            setResult(ERROR_NO_LOCK);
        } else {
            // Only print if meaningful.
            report << LOOPBACK_RESULT_TAG "glitch.count       = " << std::setw(8)
                   << mGlitchCount << "\n";
            report << LOOPBACK_RESULT_TAG "max.glitch         = " << std::setw(8)
                   << mMaxGlitchDelta << "\n";
            if (mGlitchCount > 0) {
                report << "ERROR - number of glitches > 0\n";
                setResult(ERROR_GLITCHES);
            }
        }
        return report.str();
    }

    void printStatus() override {
        ALOGD("st = %d, #gl = %3d,", mState, mGlitchCount);
    }

    /**
     * @param frameData contains microphone data with sine signal feedback
     * @param channelCount
     */
    result_code processInputFrame(const float *frameData, int /* channelCount */) override {
        result_code result = RESULT_OK;

        float sample = frameData[getInputChannel()];

        // Force a periodic glitch to test the detector!
        if (mForceGlitchDurationFrames > 0) {
            if (mForceGlitchCounter == 0) {
                ALOGE("%s: finish a glitch!!", __func__);
                mForceGlitchCounter = kForceGlitchPeriod;
            } else if (mForceGlitchCounter <= mForceGlitchDurationFrames) {
                // Force an abrupt offset.
                sample += (sample > 0.0) ? -kForceGlitchOffset : kForceGlitchOffset;
            }
            --mForceGlitchCounter;
        }

        float peak = mPeakFollower.process(sample);
        mInfiniteRecording.write(sample);

        mStateFrameCounters[mState]++; // count how many frames we are in each state

        switch (mState) {
            case STATE_IDLE:
                mDownCounter--;
                if (mDownCounter <= 0) {
                    mState = STATE_IMMUNE;
                    mDownCounter = IMMUNE_FRAME_COUNT;
                    mInputPhase = 0.0; // prevent spike at start
                    mOutputPhase = 0.0;
                    resetAccumulator();
                }
                break;

            case STATE_IMMUNE:
                mDownCounter--;
                if (mDownCounter <= 0) {
                    mState = STATE_WAITING_FOR_SIGNAL;
                }
                break;

            case STATE_WAITING_FOR_SIGNAL:
                if (peak > mThreshold) {
                    mState = STATE_WAITING_FOR_LOCK;
                    //ALOGD("%5d: switch to STATE_WAITING_FOR_LOCK", mFrameCounter);
                    resetAccumulator();
                }
                break;

            case STATE_WAITING_FOR_LOCK:
                mSinAccumulator += static_cast<double>(sample) * sinf(mInputPhase);
                mCosAccumulator += static_cast<double>(sample) * cosf(mInputPhase);
                mFramesAccumulated++;
                // Must be a multiple of the period or the calculation will not be accurate.
                if (mFramesAccumulated == mSinePeriod * PERIODS_NEEDED_FOR_LOCK) {
                    double magnitude = calculateMagnitudePhase(&mPhaseOffset);
                    if (mPhaseOffset != kPhaseInvalid) {
                        setMagnitude(magnitude);
                        ALOGD("%s() mag = %f, mPhaseOffset = %f",
                              __func__, magnitude, mPhaseOffset);
                        if (mMagnitude > mThreshold) {
                            if (fabs(mPhaseOffset) < kMaxPhaseError) {
                                mState = STATE_LOCKED;
                                mConsecutiveBadFrames = 0;
//                            ALOGD("%5d: switch to STATE_LOCKED", mFrameCounter);
                            }
                            // Adjust mInputPhase to match measured phase
                            mInputPhase += mPhaseOffset;
                        }
                    }
                    resetAccumulator();
                }
                incrementInputPhase();
                break;

            case STATE_LOCKED: {
                // Predict next sine value
                double predicted = sinf(mInputPhase) * mMagnitude;
                double diff = predicted - sample;
                double absDiff = fabs(diff);
                mMaxGlitchDelta = std::max(mMaxGlitchDelta, absDiff);
                if (absDiff > mScaledTolerance) { // bad frame
                    mConsecutiveBadFrames++;
                    mConsecutiveGoodFrames = 0;
                    LOGI("diff glitch frame #%d detected, absDiff = %g > %g",
                         mConsecutiveBadFrames, absDiff, mScaledTolerance);
                    if (mConsecutiveBadFrames > 0) {
                        result = ERROR_GLITCHES;
                        onGlitchStart();
                    }
                    resetAccumulator();
                } else { // good frame
                    mConsecutiveBadFrames = 0;
                    mConsecutiveGoodFrames++;

                    mSumSquareSignal += predicted * predicted;
                    mSumSquareNoise += diff * diff;

                    // Track incoming signal and slowly adjust magnitude to account
                    // for drift in the DRC or AGC.
                    // Must be a multiple of the period or the calculation will not be accurate.
                    if (transformSample(sample)) {
                        // Adjust phase to account for sample rate drift.
                        mInputPhase += mPhaseOffset;

                        mMeanSquareNoise = mSumSquareNoise * mInverseSinePeriod;
                        mMeanSquareSignal = mSumSquareSignal * mInverseSinePeriod;
                        mSumSquareNoise = 0.0;
                        mSumSquareSignal = 0.0;

                        if (fabs(mPhaseOffset) > kMaxPhaseError) {
                            result = ERROR_GLITCHES;
                            onGlitchStart();
                            ALOGD("phase glitch detected, phaseOffset = %g", mPhaseOffset);
                        } else if (mMagnitude < mThreshold) {
                            result = ERROR_GLITCHES;
                            onGlitchStart();
                            ALOGD("magnitude glitch detected, mMagnitude = %g", mMagnitude);
                        }
                    }
                }
            } break;

            case STATE_GLITCHING: {
                // Predict next sine value
                double predicted = sinf(mInputPhase) * mMagnitude;
                double diff = predicted - sample;
                double absDiff = fabs(diff);
                mMaxGlitchDelta = std::max(mMaxGlitchDelta, absDiff);
                if (absDiff > mScaledTolerance) { // bad frame
                    mConsecutiveBadFrames++;
                    mConsecutiveGoodFrames = 0;
                    mGlitchLength++;
                    if (mGlitchLength > maxMeasurableGlitchLength()) {
                        onGlitchTerminated();
                    }
                } else { // good frame
                    mConsecutiveBadFrames = 0;
                    mConsecutiveGoodFrames++;
                    // If we get a full sine period of good samples in a row then consider the glitch over.
                    // We don't want to just consider a zero crossing the end of a glitch.
                    if (mConsecutiveGoodFrames > mSinePeriod) {
                        onGlitchEnd();
                    }
                }
                incrementInputPhase();
            } break;

            case NUM_STATES: // not a real state
                break;
        }

        mFrameCounter++;

        return result;
    }

    int maxMeasurableGlitchLength() const { return 2 * mSinePeriod; }

    bool isOutputEnabled() override { return mState != STATE_IDLE; }

    void onGlitchStart() {
        mState = STATE_GLITCHING;
        mGlitchLength = 1;
        mLastGlitchPosition = mInfiniteRecording.getTotalWritten();
        ALOGD("%5d: STARTED a glitch # %d, pos = %5d",
              mFrameCounter, mGlitchCount, (int)mLastGlitchPosition);
        ALOGD("glitch mSinePeriod = %d", mSinePeriod);
    }

    /**
     * Give up waiting for a glitch to end and try to resync.
     */
    void onGlitchTerminated() {
        mGlitchCount++;
        ALOGD("%5d: TERMINATED a glitch # %d, length = %d", mFrameCounter, mGlitchCount, mGlitchLength);
        // We don't know how long the glitch really is so set the length to -1.
        mGlitchLength = -1;
        mState = STATE_WAITING_FOR_LOCK;
        resetAccumulator();
    }

    void onGlitchEnd() {
        mGlitchCount++;
        ALOGD("%5d: ENDED a glitch # %d, length = %d", mFrameCounter, mGlitchCount, mGlitchLength);
        mState = STATE_LOCKED;
        resetAccumulator();
    }

    // reset the sine wave detector
    void resetAccumulator() override {
        BaseSineAnalyzer::resetAccumulator();
    }

    void reset() override {
        BaseSineAnalyzer::reset();
        mState = STATE_IDLE;
        mDownCounter = IDLE_FRAME_COUNT;
    }

    void prepareToTest() override {
        BaseSineAnalyzer::prepareToTest();
        mGlitchCount = 0;
        mGlitchLength = 0;
        mMaxGlitchDelta = 0.0;
        for (int i = 0; i < NUM_STATES; i++) {
            mStateFrameCounters[i] = 0;
        }
    }

    int32_t getLastGlitch(float *buffer, int32_t length) {
        const int margin = mSinePeriod;
        int32_t numSamples = mInfiniteRecording.readFrom(buffer,
                                                         mLastGlitchPosition - margin,
                                                         length);
        ALOGD("%s: glitch at %d, edge = %7.4f, %7.4f, %7.4f",
              __func__, (int)mLastGlitchPosition,
            buffer[margin - 1], buffer[margin], buffer[margin+1]);
        return numSamples;
    }

    int32_t getRecentSamples(float *buffer, int32_t length) {
        int firstSample = mInfiniteRecording.getTotalWritten() - length;
        int32_t numSamples = mInfiniteRecording.readFrom(buffer,
                                                         firstSample,
                                                         length);
        return numSamples;
    }

    void setForcedGlitchDuration(int frames) {
        mForceGlitchDurationFrames = frames;
    }

private:

    // These must match the values in GlitchActivity.java
    enum sine_state_t {
        STATE_IDLE,               // beginning
        STATE_IMMUNE,             // ignoring input, waiting for HW to settle
        STATE_WAITING_FOR_SIGNAL, // looking for a loud signal
        STATE_WAITING_FOR_LOCK,   // trying to lock onto the phase of the sine
        STATE_LOCKED,             // locked on the sine wave, looking for glitches
        STATE_GLITCHING,          // locked on the sine wave but glitching
        NUM_STATES
    };

    enum constants {
        // Arbitrary durations, assuming 48000 Hz
        IDLE_FRAME_COUNT = 48 * 100,
        IMMUNE_FRAME_COUNT = 48 * 100,
        PERIODS_NEEDED_FOR_LOCK = 8,
        MIN_SNR_DB = 65
    };

    static constexpr double kMaxPhaseError = M_PI * 0.05;

    double  mThreshold = 0.005;

    int32_t mStateFrameCounters[NUM_STATES];
    sine_state_t  mState = STATE_IDLE;
    int64_t       mLastGlitchPosition;

    double  mMaxGlitchDelta = 0.0;
    int32_t mGlitchCount = 0;
    int32_t mConsecutiveBadFrames = 0;
    int32_t mConsecutiveGoodFrames = 0;
    int32_t mGlitchLength = 0;
    int     mDownCounter = IDLE_FRAME_COUNT;
    int32_t mFrameCounter = 0;

    int32_t mForceGlitchDurationFrames = 0; // if > 0 then force a glitch for debugging
    static constexpr int32_t kForceGlitchPeriod = 2 * 48000; // How often we glitch
    static constexpr float   kForceGlitchOffset = 0.20f;
    int32_t mForceGlitchCounter = kForceGlitchPeriod; // count down and trigger at zero

    // measure background noise continuously as a deviation from the expected signal
    double  mSumSquareSignal = 0.0;
    double  mSumSquareNoise = 0.0;
    double  mMeanSquareSignal = 0.0;
    double  mMeanSquareNoise = 0.0;

    PeakDetector  mPeakFollower;
};


#endif //ANALYZER_GLITCH_ANALYZER_H
