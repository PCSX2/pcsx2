/*
 * Copyright 2019 The Android Open Source Project
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

#include "common/OboeDebug.h"
#include "FullDuplexAnalyzer.h"

oboe::Result  FullDuplexAnalyzer::start() {
    getLoopbackProcessor()->setSampleRate(getOutputStream()->getSampleRate());
    getLoopbackProcessor()->prepareToTest();
    mWriteReadDeltaValid = false;
    return FullDuplexStreamWithConversion::start();
}

oboe::DataCallbackResult FullDuplexAnalyzer::onBothStreamsReadyFloat(
        const float *inputData,
        int   numInputFrames,
        float *outputData,
        int   numOutputFrames) {

    int32_t inputStride = getInputStream()->getChannelCount();
    int32_t outputStride = getOutputStream()->getChannelCount();
    auto *inputFloat = static_cast<const float *>(inputData);
    float *outputFloat = outputData;

    // Get atomic snapshot of the relative frame positions so they
    // can be used to calculate timestamp latency.
    int64_t framesRead = getInputStream()->getFramesRead();
    int64_t framesWritten = getOutputStream()->getFramesWritten();
    mWriteReadDelta = framesWritten - framesRead;
    mWriteReadDeltaValid = true;

    (void) getLoopbackProcessor()->process(inputFloat, inputStride, numInputFrames,
                                   outputFloat, outputStride, numOutputFrames);

    // Save data for later analysis or for writing to a WAVE file.
    if (mRecording != nullptr) {
        float buffer[2];
        int numBoth = std::min(numInputFrames, numOutputFrames);
        // Offset to the selected channels that we are analyzing.
        inputFloat += getLoopbackProcessor()->getInputChannel();
        outputFloat += getLoopbackProcessor()->getOutputChannel();
        for (int i = 0; i < numBoth; i++) {
            buffer[0] = *outputFloat;
            outputFloat += outputStride;
            buffer[1] = *inputFloat;
            inputFloat += inputStride;
            mRecording->write(buffer, 1);
        }
        // Handle mismatch in numFrames.
        const float gapMarker = -0.9f; // Recognizable value so we can tell underruns from DSP gaps.
        buffer[0] = gapMarker; // gap in output
        for (int i = numBoth; i < numInputFrames; i++) {
            buffer[1] = *inputFloat;
            inputFloat += inputStride;
            mRecording->write(buffer, 1);
        }
        buffer[1] = gapMarker; // gap in input
        for (int i = numBoth; i < numOutputFrames; i++) {
            buffer[0] = *outputFloat;
            outputFloat += outputStride;
            mRecording->write(buffer, 1);
        }
    }
    return oboe::DataCallbackResult::Continue;
};
