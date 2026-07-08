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

/**
 * Tools for measuring latency and for detecting glitches.
 * These classes are pure math and can be used with any audio system.
 */

#ifndef ANALYZER_LATENCY_ANALYZER_H
#define ANALYZER_LATENCY_ANALYZER_H

#include <algorithm>
#include <assert.h>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <memory>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

#include "common/OboeDebug.h"

#include "PeakDetector.h"
#include "PseudoRandom.h"
#include "RandomPulseGenerator.h"

// This is used when the code is in not in Android.
#ifndef ALOGD
#define ALOGD LOGD
#define ALOGE LOGE
#define ALOGW LOGW
#endif

#define LOOPBACK_RESULT_TAG  "RESULT: "

// Enable or disable the optimized latency calculation.
#define USE_FAST_LATENCY_CALCULATION 1

static constexpr int32_t kDefaultSampleRate = 48000;
static constexpr int32_t kMillisPerSecond   = 1000;  // by definition
static constexpr int32_t kMaxLatencyMillis  = 1000;  // arbitrary and generous

struct LatencyReport {
    int32_t latencyInFrames = 0;
    double correlation = 0.0;

    void reset() {
        latencyInFrames = 0;
        correlation = 0.0;
    }
};

/**
 * Calculate a normalized cross correlation.
 * @return value between -1.0 and 1.0
 */

static float calculateNormalizedCorrelation(const float *a,
                                             const float *b,
                                             int windowSize,
                                             int stride) {
    float correlation = 0.0;
    float sumProducts = 0.0;
    float sumSquares = 0.0;

    // Correlate a against b.
    for (int i = 0; i < windowSize; i += stride) {
        float s1 = a[i];
        float s2 = b[i];
        // Use a normalized cross-correlation.
        sumProducts += s1 * s2;
        sumSquares += ((s1 * s1) + (s2 * s2));
    }

    if (sumSquares >= 1.0e-9) {
        correlation = 2.0 * sumProducts / sumSquares;
    }
    return correlation;
}

static double calculateRootMeanSquare(float *data, int32_t numSamples) {
    double sum = 0.0;
    for (int32_t i = 0; i < numSamples; i++) {
        double sample = data[i];
        sum += sample * sample;
    }
    return sqrt(sum / numSamples);
}

/**
 * Monophonic recording with processing.
 * Samples are stored as floats internally.
 */
class AudioRecording
{
public:

    void allocate(int maxFrames) {
        mData = std::make_unique<float[]>(maxFrames);
        mMaxFrames = maxFrames;
        mFrameCounter = 0;
    }

    // Write SHORT data from the first channel.
    int32_t write(const int16_t *inputData, int32_t inputChannelCount, int32_t numFrames) {
        // stop at end of buffer
        if ((mFrameCounter + numFrames) > mMaxFrames) {
            numFrames = mMaxFrames - mFrameCounter;
        }
        for (int i = 0; i < numFrames; i++) {
            mData[mFrameCounter++] = inputData[i * inputChannelCount] * (1.0f / 32768);
        }
        return numFrames;
    }

    // Write FLOAT data from the first channel.
    int32_t write(const float *inputData, int32_t inputChannelCount, int32_t numFrames) {
        // stop at end of buffer
        if ((mFrameCounter + numFrames) > mMaxFrames) {
            numFrames = mMaxFrames - mFrameCounter;
        }
        for (int i = 0; i < numFrames; i++) {
            mData[mFrameCounter++] = inputData[i * inputChannelCount];
        }
        return numFrames;
    }

    // Write single FLOAT value.
    int32_t write(float sample) {
        // stop at end of buffer
        if (mFrameCounter < mMaxFrames) {
            mData[mFrameCounter++] = sample;
            return 1;
        }
        return 0;
    }

    void clear() {
        mFrameCounter = 0;
    }

    int32_t size() const {
        return mFrameCounter;
    }

    bool isFull() const {
        return mFrameCounter >= mMaxFrames;
    }

    float *getData() const {
        return mData.get();
    }

    void setSampleRate(int32_t sampleRate) {
        mSampleRate = sampleRate;
    }

    int32_t getSampleRate() const {
        return mSampleRate;
    }

    /**
     * Square the samples so they are all positive and so the peaks are emphasized.
     */
    void square() {
        float *x = mData.get();
        for (int i = 0; i < mFrameCounter; i++) {
            x[i] *= x[i];
        }
    }

    // Envelope follower that rides over the peak values.
    void detectPeaks(float decay) {
        float level = 0.0f;
        float *x = mData.get();
        for (int i = 0; i < mFrameCounter; i++) {
            level *= decay; // exponential decay
            float input = fabs(x[i]);
            // never fall below the input signal
            if (input > level) {
                level = input;
            }
            x[i] = level; // write result back into the array
        }
    }

    /**
     * Amplify a signal so that the peak matches the specified target.
     *
     * @param target final max value
     * @return gain applied to signal
     */
    float normalize(float target) {
        float maxValue = 1.0e-9f;
        for (int i = 0; i < mFrameCounter; i++) {
            maxValue = std::max(maxValue, fabsf(mData[i]));
        }
        float gain = target / maxValue;
        for (int i = 0; i < mFrameCounter; i++) {
            mData[i] *= gain;
        }
        return gain;
    }

private:
    std::unique_ptr<float[]> mData;
    int32_t       mFrameCounter = 0;
    int32_t       mMaxFrames = 0;
    int32_t       mSampleRate = kDefaultSampleRate; // common default
};

/**
  * Find latency using cross correlation in window of the recorded audio.
  * The stride is used to skip over samples and reduce the CPU load.
  */
static int measureLatencyFromPulsePartial(AudioRecording &recorded,
                                          int32_t recordedOffset,
                                          int32_t recordedWindowSize,
                                          AudioRecording &pulse,
                                          LatencyReport *report,
                                          int32_t stride) {
    report->reset();

    if (recordedOffset + recordedWindowSize + pulse.size() > recorded.size()) {
        ALOGE("%s() tried to correlate past end of recording, recordedOffset = %d frames\n",
              __func__, recordedOffset);
        return -3;
    }

    int32_t numCorrelations = recordedWindowSize / stride;
    if (numCorrelations < 10) {
        ALOGE("%s() recording too small = %d frames, numCorrelations = %d\n",
              __func__, recorded.size(), numCorrelations);
        return -1;
    }
    std::unique_ptr<float[]> correlations= std::make_unique<float[]>(numCorrelations);

    // Correlate pulse against the recorded data.
    for (int32_t i = 0; i < numCorrelations; i++) {
        const int32_t index = (i * stride) + recordedOffset;
        float correlation = calculateNormalizedCorrelation(&recorded.getData()[index],
                                                           &pulse.getData()[0],
                                                           pulse.size(),
                                                           stride);
        correlations[i] = correlation;
    }
    // Find highest peak in correlation array.
    float peakCorrelation = 0.0;
    int32_t peakIndex = -1;
    for (int32_t i = 0; i < numCorrelations; i++) {
        float value = fabsf(correlations[i]);
        if (value > peakCorrelation) {
            peakCorrelation = value;
            peakIndex = i;
        }
    }
    if (peakIndex < 0) {
        ALOGE("%s() no signal for correlation\n", __func__);
        return -2;
    }
#if 0
    // Dump correlation data for charting.
    else {
        const int32_t margin = 50;
        int32_t startIndex = std::max(0, peakIndex - margin);
        int32_t endIndex = std::min(numCorrelations - 1, peakIndex + margin);
        for (int32_t index = startIndex; index < endIndex; index++) {
            ALOGD("Correlation, %d, %f", index, correlations[index]);
        }
    }
#endif

    report->latencyInFrames = recordedOffset + (peakIndex * stride);
    report->correlation = peakCorrelation;

    return 0;
}

#if USE_FAST_LATENCY_CALCULATION
static int measureLatencyFromPulse(AudioRecording &recorded,
                                   AudioRecording &pulse,
                                   LatencyReport *report) {
    const int32_t coarseStride = 16;
    const int32_t fineWindowSize = coarseStride * 8;
    const int32_t fineStride = 1;
    LatencyReport courseReport;
    courseReport.reset();
    // Do a rough search, skipping over most of the samples.
    int result = measureLatencyFromPulsePartial(recorded,
                                                0, // recordedOffset,
                                                recorded.size() - pulse.size(),
                                                pulse,
                                                &courseReport,
                                                coarseStride);
    if (result != 0) {
        return result;
    }
    // Now do a fine resolution search near the coarse latency result.
    int32_t recordedOffset = std::max(0, courseReport.latencyInFrames - (fineWindowSize / 2));
    result = measureLatencyFromPulsePartial(recorded,
                                            recordedOffset,
                                            fineWindowSize,
                                            pulse,
                                            report,
                                            fineStride );
    return result;
}
#else
// TODO - When we are confident of the new code we can remove this old code.
static int measureLatencyFromPulse(AudioRecording &recorded,
                                   AudioRecording &pulse,
                                   LatencyReport *report) {
    return measureLatencyFromPulsePartial(recorded,
                                          0,
                                          recorded.size() - pulse.size(),
                                          pulse,
                                          report,
                                          1 );
}
#endif

// ====================================================================================
class LoopbackProcessor {
public:
    virtual ~LoopbackProcessor() = default;

    enum result_code {
        RESULT_OK = 0,
        ERROR_NOISY = -99,
        ERROR_VOLUME_TOO_LOW,
        ERROR_VOLUME_TOO_HIGH,
        ERROR_CONFIDENCE,
        ERROR_INVALID_STATE,
        ERROR_GLITCHES,
        ERROR_NO_LOCK
    };

    virtual void prepareToTest() {
        reset();
    }

    virtual void reset() {
        mResult = 0;
        mResetCount++;
    }

    virtual result_code processInputFrame(const float *frameData, int channelCount) = 0;
    virtual result_code processOutputFrame(float *frameData, int channelCount) = 0;

    void process(const float *inputData, int inputChannelCount, int numInputFrames,
                 float *outputData, int outputChannelCount, int numOutputFrames) {
        int numBoth = std::min(numInputFrames, numOutputFrames);
        // Process one frame at a time.
        for (int i = 0; i < numBoth; i++) {
            processInputFrame(inputData, inputChannelCount);
            inputData += inputChannelCount;
            processOutputFrame(outputData, outputChannelCount);
            outputData += outputChannelCount;
        }
        // If there is more input than output.
        for (int i = numBoth; i < numInputFrames; i++) {
            processInputFrame(inputData, inputChannelCount);
            inputData += inputChannelCount;
        }
        // If there is more output than input.
        for (int i = numBoth; i < numOutputFrames; i++) {
            processOutputFrame(outputData, outputChannelCount);
            outputData += outputChannelCount;
        }
    }

    virtual std::string analyze() = 0;

    virtual void printStatus() {};

    int32_t getResult() {
        return mResult;
    }

    void setResult(int32_t result) {
        mResult = result;
    }

    virtual bool isDone() {
        return false;
    }

    virtual int save(const char *fileName) {
        (void) fileName;
        return -1;
    }

    virtual int load(const char *fileName) {
        (void) fileName;
        return -1;
    }

    virtual void setSampleRate(int32_t sampleRate) {
        mSampleRate = sampleRate;
    }

    int32_t getSampleRate() const {
        return mSampleRate;
    }

    int32_t getResetCount() const {
        return mResetCount;
    }

    /** Called when not enough input frames could be read after synchronization.
     */
    virtual void onInsufficientRead() {
        reset();
    }

    /**
     * Some analyzers may only look at one channel.
     * You can optionally specify that channel here.
     *
     * @param inputChannel
     */
    void setInputChannel(int inputChannel) {
        mInputChannel = inputChannel;
    }

    int getInputChannel() const {
        return mInputChannel;
    }

    /**
     * Some analyzers may only generate one channel.
     * You can optionally specify that channel here.
     *
     * @param outputChannel
     */
    void setOutputChannel(int outputChannel) {
        mOutputChannel = outputChannel;
    }

    int getOutputChannel() const {
        return mOutputChannel;
    }

protected:
    int32_t   mResetCount = 0;

private:

    int32_t mInputChannel = 0;
    int32_t mOutputChannel = 0;
    int32_t mSampleRate = kDefaultSampleRate;
    int32_t mResult = 0;
};

class LatencyAnalyzer : public LoopbackProcessor {
public:

    LatencyAnalyzer() : LoopbackProcessor() {}
    virtual ~LatencyAnalyzer() = default;

    /**
     * Call this after the constructor because it calls other virtual methods.
     */
    virtual void setup() = 0;

    virtual int32_t getProgress() const = 0;

    virtual int getState() const = 0;

    // @return latency in frames
    virtual int32_t getMeasuredLatency() const = 0;

    /**
     * This is an overall confidence in the latency result based on correlation, SNR, etc.
     * @return probability value between 0.0 and 1.0
     */
    double getMeasuredConfidence() const {
        // Limit the ratio and prevent divide-by-zero.
        double noiseSignalRatio = getSignalRMS() <= getBackgroundRMS()
                                  ? 1.0 : getBackgroundRMS() / getSignalRMS();
        // Prevent high background noise and low signals from generating false matches.
        double adjustedConfidence = getMeasuredCorrelation() - noiseSignalRatio;
        return std::max(0.0, adjustedConfidence);
    }

    /**
     * Cross correlation value for the noise pulse against
     * the corresponding position in the normalized recording.
     *
     * @return value between -1.0 and 1.0
     */
    virtual double getMeasuredCorrelation() const = 0;

    virtual double getBackgroundRMS() const = 0;

    virtual double getSignalRMS() const = 0;

    virtual bool hasEnoughData() const = 0;
};

// ====================================================================================
/**
 * Measure latency given a loopback stream data.
 * Use an encoded bit train as the sound source because it
 * has an unambiguous correlation value.
 * Uses a state machine to cycle through various stages.
 *
 */
class PulseLatencyAnalyzer : public LatencyAnalyzer {
public:

    void setup() override {
        int32_t pulseLength = calculatePulseLength();
        int32_t maxLatencyFrames = getSampleRate() * kMaxLatencyMillis / kMillisPerSecond;
        mFramesToRecord = pulseLength + maxLatencyFrames;
        mAudioRecording.allocate(mFramesToRecord);
        mAudioRecording.setSampleRate(getSampleRate());
    }

    int getState() const override {
        return mState;
    }

    void setSampleRate(int32_t sampleRate) override {
        LoopbackProcessor::setSampleRate(sampleRate);
        mAudioRecording.setSampleRate(sampleRate);
    }

    void reset() override {
        LoopbackProcessor::reset();
        mState = STATE_MEASURE_BACKGROUND;
        mDownCounter = (int32_t) (getSampleRate() * kBackgroundMeasurementLengthSeconds);
        mLoopCounter = 0;

        mPulseCursor = 0;
        mBackgroundSumSquare = 0.0f;
        mBackgroundSumCount = 0;
        mBackgroundRMS = 0.0f;
        mSignalRMS = 0.0f;

        generatePulseRecording(calculatePulseLength());
        mAudioRecording.clear();
        mLatencyReport.reset();
    }

    bool hasEnoughData() const override {
        return mAudioRecording.isFull();
    }

    bool isDone() override {
        return mState == STATE_DONE;
    }

    int32_t getProgress() const override {
        return mAudioRecording.size();
    }

    std::string analyze() override {
        std::stringstream report;
        report << "PulseLatencyAnalyzer ---------------\n";
        report << LOOPBACK_RESULT_TAG "test.state             = "
                << std::setw(8) << mState << "\n";
        report << LOOPBACK_RESULT_TAG "test.state.name        = "
                << convertStateToText(mState) << "\n";
        report << LOOPBACK_RESULT_TAG "background.rms         = "
                << std::setw(8) << mBackgroundRMS << "\n";

        int32_t newResult = RESULT_OK;
        if (mState != STATE_GOT_DATA) {
            report << "WARNING - Bad state. Check volume on device.\n";
            // setResult(ERROR_INVALID_STATE);
        } else {
            float gain = mAudioRecording.normalize(1.0f);
            measureLatency();

            // Calculate signalRMS even if it is bogus.
            // Also it may be used in the confidence calculation below.
            mSignalRMS = calculateRootMeanSquare(
                    &mAudioRecording.getData()[mLatencyReport.latencyInFrames], mPulse.size())
                         / gain;
            if (getMeasuredConfidence() < getMinimumConfidence()) {
                report << "   ERROR - confidence too low!";
                newResult = ERROR_CONFIDENCE;
            }

            double latencyMillis = kMillisPerSecond * (double) mLatencyReport.latencyInFrames
                                   / getSampleRate();
            report << LOOPBACK_RESULT_TAG "latency.frames         = " << std::setw(8)
                   << mLatencyReport.latencyInFrames << "\n";
            report << LOOPBACK_RESULT_TAG "latency.msec           = " << std::setw(8)
                   << latencyMillis << "\n";
            report << LOOPBACK_RESULT_TAG "latency.confidence     = " << std::setw(8)
                   << getMeasuredConfidence() << "\n";
            report << LOOPBACK_RESULT_TAG "latency.correlation    = " << std::setw(8)
                   << getMeasuredCorrelation() << "\n";
        }
        mState = STATE_DONE;
        if (getResult() == RESULT_OK) {
            setResult(newResult);
        }

        return report.str();
    }

    int32_t getMeasuredLatency() const override {
        return mLatencyReport.latencyInFrames;
    }

    double getMeasuredCorrelation() const override {
        return mLatencyReport.correlation;
    }

    double getBackgroundRMS() const override {
        return mBackgroundRMS;
    }

    double getSignalRMS() const override {
        return mSignalRMS;
    }

    bool isRecordingComplete() {
        return mState == STATE_GOT_DATA;
    }

    void printStatus() override {
        ALOGD("latency: st = %d = %s", mState, convertStateToText(mState));
    }

    result_code processInputFrame(const float *frameData, int /* channelCount */) override {
        echo_state nextState = mState;
        mLoopCounter++;
        float input = frameData[0];

        switch (mState) {
            case STATE_MEASURE_BACKGROUND:
                // Measure background RMS on channel 0
                mBackgroundSumSquare += static_cast<double>(input) * input;
                mBackgroundSumCount++;
                mDownCounter--;
                if (mDownCounter <= 0) {
                    mBackgroundRMS = sqrtf(mBackgroundSumSquare / mBackgroundSumCount);
                    nextState = STATE_IN_PULSE;
                    mPulseCursor = 0;
                }
                break;

            case STATE_IN_PULSE:
                // Record input until the mAudioRecording is full.
                mAudioRecording.write(input);
                if (hasEnoughData()) {
                    nextState = STATE_GOT_DATA;
                }
                break;

            case STATE_GOT_DATA:
            case STATE_DONE:
            default:
                break;
        }

        mState = nextState;
        return RESULT_OK;
    }

    result_code processOutputFrame(float *frameData, int channelCount) override {
        switch (mState) {
            case STATE_IN_PULSE:
                if (mPulseCursor < mPulse.size()) {
                    float pulseSample = mPulse.getData()[mPulseCursor++];
                    for (int i = 0; i < channelCount; i++) {
                        frameData[i] = pulseSample;
                    }
                } else {
                    for (int i = 0; i < channelCount; i++) {
                        frameData[i] = 0;
                    }
                }
                break;

            case STATE_MEASURE_BACKGROUND:
            case STATE_GOT_DATA:
            case STATE_DONE:
            default:
                for (int i = 0; i < channelCount; i++) {
                    frameData[i] = 0.0f; // silence
                }
                break;
        }

        return RESULT_OK;
    }

protected:

    virtual int32_t calculatePulseLength() const = 0;

    virtual void generatePulseRecording(int32_t pulseLength) = 0;

    virtual void measureLatency() = 0;

    virtual double getMinimumConfidence() const {
        return 0.5;
    }

    AudioRecording     mPulse;
    AudioRecording     mAudioRecording; // contains only the input after starting the pulse
    LatencyReport      mLatencyReport;

    static constexpr int32_t kPulseLengthMillis = 500;
    float              mPulseAmplitude = 0.5f;
    double             mBackgroundRMS = 0.0;
    double             mSignalRMS = 0.0;

private:

    enum echo_state {
        STATE_MEASURE_BACKGROUND,
        STATE_IN_PULSE,
        STATE_GOT_DATA, // must match RoundTripLatencyActivity.java
        STATE_DONE,
    };

    const char *convertStateToText(echo_state state) {
        switch (state) {
            case STATE_MEASURE_BACKGROUND:
                return "INIT";
            case STATE_IN_PULSE:
                return "PULSE";
            case STATE_GOT_DATA:
                return "GOT_DATA";
            case STATE_DONE:
                return "DONE";
        }
        return "UNKNOWN";
    }

    int32_t         mDownCounter = 500;
    int32_t         mLoopCounter = 0;
    echo_state      mState = STATE_MEASURE_BACKGROUND;

    static constexpr double  kBackgroundMeasurementLengthSeconds = 0.5;

    int32_t            mPulseCursor = 0;

    double             mBackgroundSumSquare = 0.0;
    int32_t            mBackgroundSumCount = 0;
    int32_t            mFramesToRecord = 0;

};

/**
 * This algorithm uses a series of random bits encoded using the
 * Manchester encoder. It works well for wired loopback but not very well for
 * through the air loopback.
 */
class EncodedRandomLatencyAnalyzer : public PulseLatencyAnalyzer {

protected:

    int32_t calculatePulseLength() const override {
        // Calculate integer number of bits.
        int32_t numPulseBits = getSampleRate() * kPulseLengthMillis
                               / (kFramesPerEncodedBit * kMillisPerSecond);
        return numPulseBits * kFramesPerEncodedBit;
    }

    void generatePulseRecording(int32_t pulseLength) override {
        mPulse.allocate(pulseLength);
        RandomPulseGenerator pulser(kFramesPerEncodedBit);
        for (int i = 0; i < pulseLength; i++) {
            mPulse.write(pulser.nextFloat() * mPulseAmplitude);
        }
    }

    double getMinimumConfidence() const override {
        return 0.2;
    }

    void measureLatency() override {
        measureLatencyFromPulse(mAudioRecording,
                                mPulse,
                                &mLatencyReport);
    }

private:
    static constexpr int32_t kFramesPerEncodedBit = 8; // multiple of 2
};

/**
 * This algorithm uses White Noise sent in a short burst pattern.
 * The original signal and the recorded signal are then run through
 * an envelope follower to convert the fine detail into more of
 * a rectangular block before the correlation phase.
 */
class WhiteNoiseLatencyAnalyzer : public PulseLatencyAnalyzer {

protected:

    int32_t calculatePulseLength() const override {
        return getSampleRate() * kPulseLengthMillis / kMillisPerSecond;
    }

    void generatePulseRecording(int32_t pulseLength) override {
        mPulse.allocate(pulseLength);
        // Turn the noise on and off to sharpen the correlation peak.
        // Use more zeros than ones so that the correlation will be less than 0.5 even when there
        // is a strong background noise.
        int8_t pattern[] = {1, 0, 0,
                            1, 1, 0, 0, 0,
                            1, 1, 1, 0, 0, 0, 0,
                            1, 1, 1, 1, 0, 0, 0, 0, 0
                            };
        PseudoRandom random;
        const int32_t numSections = sizeof(pattern);
        const int32_t framesPerSection = pulseLength / numSections;
        for (int section = 0; section < numSections; section++) {
            if (pattern[section]) {
                for (int i = 0; i < framesPerSection; i++) {
                    mPulse.write((float) (random.nextRandomDouble() * mPulseAmplitude));
                }
            } else {
                for (int i = 0; i < framesPerSection; i++) {
                    mPulse.write(0.0f);
                }
            }
        }
        // Write any remaining frames.
        int32_t framesWritten = framesPerSection * numSections;
        for (int i = framesWritten; i < pulseLength; i++) {
            mPulse.write(0.0f);
        }
    }

    void measureLatency() override {
        // Smooth out the noise so we see rectangular blocks.
        // This improves immunity against phase cancellation and distortion.
        static constexpr float decay = 0.99f; // just under 1.0, lower numbers decay faster
        mAudioRecording.detectPeaks(decay);
        mPulse.detectPeaks(decay);
        measureLatencyFromPulse(mAudioRecording,
                                mPulse,
                                &mLatencyReport);
    }

};

#endif // ANALYZER_LATENCY_ANALYZER_H
