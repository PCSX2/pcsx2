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

#ifndef ANALYZER_DATA_PATH_ANALYZER_H
#define ANALYZER_DATA_PATH_ANALYZER_H

#include <string>
#include <vector>
#include "BaseSineAnalyzer.h"

class DataPathAnalyzer : public BaseSineAnalyzer {
public:

    result_code processInputFrame(const float *frameData, int channelCount) override;
    std::string analyze() override;
    void reset() override;

    double getMaxMagnitude();
    double getAveragePhaseError();
    int getPhaseCount();
    bool isPhaseJitterValid();

    std::string getFrequencyResponse();
    std::string getDistortionReport();
    int getAnalysisResult();

private:
    static constexpr double kMinSmoothedMagnitude = 0.001;

    double calculatePhaseError(double p1, double p2);

    double  mPreviousPhaseOffset = 0.0;
    double  mPhaseTolerance = 2 * M_PI / 48;
    double  mMaxMagnitude = 0.0;
    int     mPhaseCount = 0;
    double  mPhaseErrorSum = 0.0;
    int     mPhaseErrorCount = 0;

    // For multi-tone analysis
    std::vector<float> mFftBuffer;
    int mFftBufferSize = 4096;
    int mFftBufferIndex = 0;
    long mFftBufferStartFrame = 0;
    std::string mDistortionReport;
    std::string mFrequencyResponse;

    // For chirp analysis
    std::vector<float> mSpectrogramBuffer;
    int mSpectrogramWindowSize = 1024;
    int mSpectrogramHopSize = 512;

    // For analysis result
    int mAnalysisResult = 0; // 0 = pass, 1 = fail
    const double mMinSinad = 10.0; // dB
};

#endif // ANALYZER_DATA_PATH_ANALYZER_H
