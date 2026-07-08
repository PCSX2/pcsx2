/*
 * Copyright (C) 2025 The Android Open Source Project
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

#include "DataPathAnalyzer.h"
#include <sstream>
#include <iomanip>
#include "fft.h"

double DataPathAnalyzer::calculatePhaseError(double p1, double p2) {
    double diff = p1 - p2;
    // Wrap around the circle.
    while (diff > M_PI) {
        diff -= (2 * M_PI);
    }
    while (diff < -M_PI) {
        diff += (2 * M_PI);
    }
    return diff;
}

BaseSineAnalyzer::result_code DataPathAnalyzer::processInputFrame(const float *frameData, int channelCount) {
    switch (mSignalType) {
        case Chirp: {
            if (mFftBuffer.size() != mFftBufferSize) {
                mFftBuffer.resize(mFftBufferSize);
            }
            if (mFftBufferIndex == 0) {
                mFftBufferStartFrame = mFrameCounter;
            }
            float sample = frameData[getInputChannel()];
            mFftBuffer[mFftBufferIndex++] = sample;
            if (mFftBufferIndex >= mFftBufferSize) {
                // Perform Spectrogram Analysis
                std::stringstream report;
                report << "Chirp analysis (peak frequency per window):\n";

                std::vector<double> peakFreqs;
                for (int i = 0; i + mSpectrogramWindowSize <= mFftBufferSize; i += mSpectrogramHopSize) {
                    CVector fftInput(mSpectrogramWindowSize);
                    for (int j = 0; j < mSpectrogramWindowSize; j++) {
                        fftInput[j] = Complex(mFftBuffer[i + j], 0);
                    }
                    fft(fftInput);

                    double maxMag = 0;
                    int peakBin = 0;
                    long frameInChirp = mFftBufferStartFrame + i + mSpectrogramWindowSize / 2;
                    double maxFreq = kChirpEndFrequency;
                    if (maxFreq > getSampleRate() / 2.0) {
                        maxFreq = getSampleRate() / 2.0;
                    }
                    double expectedFreq = kChirpStartFrequency + (maxFreq - kChirpStartFrequency) * frameInChirp / (getSampleRate() * kChirpDurationSeconds);
                    int expectedBin = (int)(expectedFreq * mSpectrogramWindowSize / getSampleRate());
                    int searchRadius = 50; // search in a window of 100 bins
                    int startBin = std::max(1, expectedBin - searchRadius);
                    int endBin = std::min(mSpectrogramWindowSize / 2, expectedBin + searchRadius);

                    for (int k = startBin; k < endBin; k++) {
                        double mag = std::abs(fftInput[k]);
                        if (mag > maxMag) {
                            maxMag = mag;
                            peakBin = k;
                        }
                    }
                    double peakFreq = (double)peakBin * getSampleRate() / mSpectrogramWindowSize;
                    peakFreqs.push_back(peakFreq);
                    report << std::fixed << std::setprecision(0) << peakFreq << " Hz\n";
                }

                // Check if frequencies are monotonically increasing
                bool passed = true;
                for (size_t i = 1; i < peakFreqs.size(); i++) {
                    if (peakFreqs[i] < peakFreqs[i-1]) {
                        passed = false;
                        break;
                    }
                }

                if (passed) {
                    mAnalysisResult = 0; // Pass
                    report << "PASS: Frequencies are monotonically increasing.\n";
                } else {
                    mAnalysisResult = 1; // Fail
                    report << "FAIL: Frequencies are not monotonically increasing.\n";
                }

                mFrequencyResponse = report.str();
                mFftBufferIndex = 0;
            }
            break;
        }
        case MultiTone: {
            if (mFftBuffer.size() != mFftBufferSize) {
                mFftBuffer.resize(mFftBufferSize);
            }
            float sample = frameData[getInputChannel()];
            mFftBuffer[mFftBufferIndex++] = sample;
            if (mFftBufferIndex >= mFftBufferSize) {
                // Perform FFT
                CVector fftInput(mFftBufferSize);
                for (int i = 0; i < mFftBufferSize; i++) {
                    fftInput[i] = Complex(mFftBuffer[i], 0);
                }
                fft(fftInput);

                // Analyze FFT output
                double signalPower = 0;
                double noisePower = 0;

                const int numTones = sizeof(sMultiToneFrequencies) / sizeof(sMultiToneFrequencies[0]);
                int bins[numTones];
                for (int i = 0; i < numTones; i++) {
                    bins[i] = (int)(sMultiToneFrequencies[i] * mFftBufferSize / getSampleRate());
                }

                for (int i = 1; i < mFftBufferSize / 2; i++) {
                    double power = std::norm(fftInput[i]);
                    bool isSignal = false;
                    for (int j = 0; j < numTones; j++) {
                        if (i >= bins[j] - 1 && i <= bins[j] + 1) {
                            isSignal = true;
                            break;
                        }
                    }
                    if (isSignal) {
                        signalPower += power;
                    } else {
                        noisePower += power;
                    }
                }

                std::stringstream report;
                report << "Multi-tone analysis:\n";
                if (noisePower > 0) {
                    double sinad = 10 * log10(signalPower / noisePower);
                    report << "SINAD = " << std::fixed << std::setprecision(2) << sinad << " dB\n";
                    if (sinad < mMinSinad) {
                        mAnalysisResult = 1; // Fail
                        report << "FAIL: SINAD is below threshold of " << mMinSinad << " dB\n";
                    } else {
                        mAnalysisResult = 0; // Pass
                    }
                } else {
                    report << "SINAD = inf\n";
                    mAnalysisResult = 0; // Pass
                }
                mDistortionReport = report.str();
                mFftBufferIndex = 0;
            }
            break;
        }
        case Sine:
        default: {
            float sample = frameData[getInputChannel()];
            mInfiniteRecording.write(sample);

            if (transformSample(sample)) {
                // Analyze magnitude and phase on every period.
                if (mPhaseOffset != kPhaseInvalid &&
                    mMagnitude >= kMinSmoothedMagnitude) {
                    double diff = fabs(
                        calculatePhaseError(mPhaseOffset, mPreviousPhaseOffset));
                    if (diff < mPhaseTolerance) {
                        mMaxMagnitude = std::max(mMagnitude, mMaxMagnitude);
                    }
                    constexpr int kMinPhaseCount = 4;
                    if (mPhaseCount >= kMinPhaseCount) {
                        mPhaseErrorSum += diff;
                        mPhaseErrorCount++;
                    }
                    mPreviousPhaseOffset = mPhaseOffset;
                    mPhaseCount++;
                }
            }
            break;
        }
    }
    return RESULT_OK;
}

std::string DataPathAnalyzer::analyze() {
    std::stringstream report;
    report << "DataPathAnalyzer ------------------\n";
    switch (mSignalType) {
        case Sine:
            report << "LOOPBACK_RESULT_TAG " << "sine.magnitude     = " << std::setw(8)
                   << mMagnitude << "\n";
            report << "LOOPBACK_RESULT_TAG " << "frames.accumulated = " << std::setw(8)
                   << mFramesAccumulated << "\n";
            report << "LOOPBACK_RESULT_TAG " << "sine.period        = " << std::setw(8)
                   << mSinePeriod << "\n";
            break;
        case Chirp:
            report << "Chirp analysis not implemented yet.\n";
            break;
        case MultiTone:
            report << "Multi-tone analysis not implemented yet.\n";
            break;
    }
    return report.str();
}

void DataPathAnalyzer::reset() {
    BaseSineAnalyzer::reset();
    mPhaseErrorSum = 0.0;
    mPhaseErrorCount = 0;
    mPhaseCount = 0;
    mPreviousPhaseOffset = 999.0; // Arbitrary high offset to prevent early lock.
    mMaxMagnitude = 0.0;
}

double DataPathAnalyzer::getMaxMagnitude() {
    return mMaxMagnitude;
}

std::string DataPathAnalyzer::getFrequencyResponse() {
    return mFrequencyResponse;
}

std::string DataPathAnalyzer::getDistortionReport() {
    return mDistortionReport;
}

int DataPathAnalyzer::getAnalysisResult() {
    return mAnalysisResult;
}

double DataPathAnalyzer::getAveragePhaseError() {
  return mPhaseErrorCount > 0 ? mPhaseErrorSum / mPhaseErrorCount : M_PI;
}

int DataPathAnalyzer::getPhaseCount() { return mPhaseCount; }

bool DataPathAnalyzer::isPhaseJitterValid() { 
    // Arbitrary number of measurements to be considered valid.
    constexpr int kMinPhaseErrorCount = 5;
    return mPhaseErrorCount >= kMinPhaseErrorCount;
 }
