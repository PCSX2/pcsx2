/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <string.h>

#include "wav/WavStreamReader.h"

#include "OneShotSampleSource.h"

namespace iolib {

void OneShotSampleSource::mixAudio(float* outBuff, int numChannels, int32_t numFrames) {
    int32_t numSamples = mSampleBuffer->getNumSamples();
    int32_t sampleChannels = mSampleBuffer->getProperties().channelCount;
    int32_t totalSamplesNeeded = numFrames * numChannels; // Total samples to fill the output buffer
    int32_t samplesProcessed = 0;
    bool isLoopMode = mIsLoopMode;

    while (samplesProcessed < totalSamplesNeeded && mIsPlaying) {
        int32_t samplesLeft = numSamples - mCurSampleIndex;
        int32_t framesLeft = (totalSamplesNeeded - samplesProcessed) / numChannels;
        int32_t numWriteFrames = std::min(framesLeft, samplesLeft / sampleChannels);

        if (numWriteFrames > 0) {
            const float* data = mSampleBuffer->getSampleData();
            if ((sampleChannels == 1) && (numChannels == 1)) {
                // MONO output from MONO samples
                for (int32_t frameIndex = 0; frameIndex < numWriteFrames; frameIndex++) {
                    outBuff[samplesProcessed + frameIndex] += data[mCurSampleIndex++] * mGain;
                }
            } else if ((sampleChannels == 1) && (numChannels == 2)) {
                // STEREO output from MONO samples
                int dstSampleIndex = samplesProcessed;
                for (int32_t frameIndex = 0; frameIndex < numWriteFrames; frameIndex++) {
                    outBuff[dstSampleIndex++] += data[mCurSampleIndex] * mLeftGain;
                    outBuff[dstSampleIndex++] += data[mCurSampleIndex++] * mRightGain;
                }
            } else if ((sampleChannels == 2) && (numChannels == 1)) {
                // MONO output from STEREO samples
                int dstSampleIndex = samplesProcessed;
                for (int32_t frameIndex = 0; frameIndex < numWriteFrames; frameIndex++) {
                    outBuff[dstSampleIndex++] += data[mCurSampleIndex++] * mLeftGain +
                                                 data[mCurSampleIndex++] * mRightGain;
                }
            } else if ((sampleChannels == 2) && (numChannels == 2)) {
                // STEREO output from STEREO samples
                int dstSampleIndex = samplesProcessed;
                for (int32_t frameIndex = 0; frameIndex < numWriteFrames; frameIndex++) {
                    outBuff[dstSampleIndex++] += data[mCurSampleIndex++] * mLeftGain;
                    outBuff[dstSampleIndex++] += data[mCurSampleIndex++] * mRightGain;
                }
            }

            samplesProcessed += numWriteFrames * numChannels;

            if (mCurSampleIndex >= numSamples) {
                if (isLoopMode) {
                    mCurSampleIndex = 0;
                } else {
                    mIsPlaying = false;
                }
            }
        } else {
            break; // No more samples to write in the current chunk
        }
    }
}

} // namespace wavlib
